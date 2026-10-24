#include "envvarstore.h"

#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <QTemporaryFile>
#include <QDir>
#include <QDebug>

EnvVarStore::EnvVarStore(const QString &filePath, bool needsPolkit, QObject *parent)
    : QObject(parent)
    , m_filePath(filePath)
    , m_needsPolkit(needsPolkit)
{
}

QMap<QString, QString> EnvVarStore::load() const
{
    QMap<QString, QString> vars;
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return vars;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        // 跳过空行和注释
        if (line.isEmpty() || line.startsWith('#')) {
            continue;
        }
        int eqPos = line.indexOf('=');
        if (eqPos <= 0) {
            continue;
        }
        QString key = line.left(eqPos).trimmed();
        QString value = line.mid(eqPos + 1).trimmed();
        // 去除外围引号
        if (value.length() >= 2) {
            if ((value.startsWith('"') && value.endsWith('"')) ||
                (value.startsWith('\'') && value.endsWith('\''))) {
                value = value.mid(1, value.length() - 2);
            }
        }
        vars[key] = value;
    }
    file.close();
    return vars;
}

bool EnvVarStore::save(const QMap<QString, QString> &variables)
{
    return writeToFile(variables);
}

bool EnvVarStore::addVariable(const QString &key, const QString &value)
{
    QMap<QString, QString> vars = load();
    if (vars.contains(key)) {
        return false; // 已存在
    }
    vars[key] = value;
    bool ok = save(vars);
    if (ok) {
        Q_EMIT variablesChanged();
    }
    return ok;
}

bool EnvVarStore::editVariable(const QString &oldKey, const QString &key, const QString &value)
{
    QMap<QString, QString> vars = load();
    if (!vars.contains(oldKey)) {
        return false;
    }
    // 如果改了 key 名称，需要删除旧 key
    if (oldKey != key) {
        vars.remove(oldKey);
    }
    vars[key] = value;
    bool ok = save(vars);
    if (ok) {
        Q_EMIT variablesChanged();
    }
    return ok;
}

bool EnvVarStore::removeVariable(const QString &key)
{
    QMap<QString, QString> vars = load();
    if (!vars.contains(key)) {
        return false;
    }
    vars.remove(key);
    bool ok = save(vars);
    if (ok) {
        Q_EMIT variablesChanged();
    }
    return ok;
}

bool EnvVarStore::contains(const QString &key) const
{
    return load().contains(key);
}

QString EnvVarStore::value(const QString &key) const
{
    return load().value(key);
}

bool EnvVarStore::writeToFile(const QMap<QString, QString> &variables)
{
    QByteArray content;
    QTextStream out(&content);
    for (auto it = variables.constBegin(); it != variables.constEnd(); ++it) {
        QString value = it.value();
        if (value.contains(' ') || value.contains(':') || value.contains(';')) {
            value = '"' + value + '"';
        }
        out << it.key() << "=" << value << "\n";
    }
    out.flush();

    if (m_needsPolkit) {
        return writeViaPolkit(content);
    } else {
        return writeDirect(content);
    }
}

bool EnvVarStore::writeViaPolkit(const QByteArray &content)
{
    // 写入临时文件，再经 polkit 授权的 helper 复制到目标位置。
    // helper 由 com.deepin.controlcenter.environment 动作授权（见 misc/*.policy），
    // 目标固定为 /etc/environment，故此处无需再传目标路径。
    QTemporaryFile tmpFile(QDir::tempPath() + "/dde-env-XXXXXX");
    tmpFile.setAutoRemove(false);
    if (!tmpFile.open()) {
        qWarning() << "Failed to create temp file:" << tmpFile.errorString();
        return false;
    }
    tmpFile.write(content);
    // 必须 flush 到磁盘，root 侧的 helper 进程才能读到完整内容
    tmpFile.flush();
    tmpFile.close();
    QString tmpPath = tmpFile.fileName();

    // helper 绝对路径由 CMake 单点定义（DDE_ENV_HELPER_PATH）传入，与 policy 的
    // exec.path 注解、install 目标保持同源；未定义时退回到 Debian 多架构安装位。
#ifndef DDE_ENV_HELPER_PATH
#define DDE_ENV_HELPER_PATH "/usr/lib/x86_64-linux-gnu/dde-control-center/dde-env-write"
#endif
    static const QString kHelper = QStringLiteral(DDE_ENV_HELPER_PATH);

    QProcess process;
    process.start("pkexec", {kHelper, tmpPath});
    if (!process.waitForStarted(30000)) {
        qWarning() << "pkexec failed to start (helper missing or polkit unavailable?)";
        QFile::remove(tmpPath);
        return false;
    }
    if (!process.waitForFinished(30000)) {
        qWarning() << "pkexec timed out";
        process.kill();
        process.waitForFinished(1000);
        QFile::remove(tmpPath);
        return false;
    }

    QFile::remove(tmpPath);

    // pkexec 约定：126 = 用户取消/未授权；127 = helper 无法执行；
    // 其余非 0 为 helper 自身返回码。0 表示写入成功。
    const int exitCode = process.exitCode();
    if (exitCode != 0) {
        const QString err = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
        if (exitCode == 126) {
            qWarning() << "polkit authorization cancelled or denied (code 126)";
        } else if (exitCode == 127) {
            qWarning() << "helper not executable (code 127):" << kHelper;
        } else {
            qWarning() << "dde-env-write helper failed (code" << exitCode << "):"
                       << err;
        }
        return false;
    }
    return true;
}

bool EnvVarStore::writeDirect(const QByteArray &content)
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "Failed to open file for writing:" << m_filePath << file.errorString();
        return false;
    }
    file.write(content);
    file.close();
    return true;
}
