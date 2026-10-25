#include "envvarstore.h"
#include "envhelper.h"

#include <QFile>
#include <QSaveFile>
#include <QTextStream>
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

QList<EnvVarStore::Line> EnvVarStore::loadRaw() const
{
    QList<Line> lines;
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return lines;
    }
    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString raw = in.readLine();
        const QString t = raw.trimmed();
        Line l;
        l.raw = raw;
        l.isEntry = false;
        if (!t.isEmpty() && !t.startsWith('#')) {
            const int eqPos = t.indexOf('=');
            if (eqPos > 0) {
                l.key = t.left(eqPos).trimmed();
                l.isEntry = true;
            }
        }
        lines.append(l);
    }
    file.close();
    return lines;
}

QString EnvVarStore::formatEntryLine(const QString &key, const QString &value)
{
    QString v = value;
    if (v.contains(' ') || v.contains(':') || v.contains(';')) {
        v = '"' + v + '"';
    }
    return key + QLatin1Char('=') + v;
}

QByteArray EnvVarStore::joinLines(const QList<Line> &lines)
{
    QByteArray content;
    for (const Line &l : lines) {
        content += l.raw.toUtf8();
        content += '\n';
    }
    return content;
}

void EnvVarStore::addVariableAsync(const QString &key, const QString &value,
                                   std::function<void(bool)> onDone)
{
    QList<Line> lines = loadRaw();
    for (const Line &l : lines) {
        if (l.isEntry && l.key == key) {
            if (onDone) onDone(false); // 已存在
            return;
        }
    }
    Line nl;
    nl.key = key;
    nl.isEntry = true;
    nl.raw = formatEntryLine(key, value);
    lines.append(nl);
    writeContentAsync(joinLines(lines), [this, onDone](bool ok) {
        if (ok) {
            Q_EMIT variablesChanged();
        }
        if (onDone) onDone(ok);
    });
}

void EnvVarStore::editVariableAsync(const QString &oldKey, const QString &key,
                                    const QString &value, std::function<void(bool)> onDone)
{
    QList<Line> lines = loadRaw();
    int idx = -1;
    for (int i = 0; i < lines.size(); ++i) {
        if (lines[i].isEntry && lines[i].key == oldKey) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        if (onDone) onDone(false); // 原键不存在
        return;
    }
    // 改名时避免与既有键冲突（生成重复键）
    if (oldKey != key) {
        for (int i = 0; i < lines.size(); ++i) {
            if (i != idx && lines[i].isEntry && lines[i].key == key) {
                if (onDone) onDone(false);
                return;
            }
        }
    }
    lines[idx].raw = formatEntryLine(key, value);
    lines[idx].key = key;
    writeContentAsync(joinLines(lines), [this, onDone](bool ok) {
        if (ok) {
            Q_EMIT variablesChanged();
        }
        if (onDone) onDone(ok);
    });
}

void EnvVarStore::removeVariableAsync(const QString &key, std::function<void(bool)> onDone)
{
    QList<Line> lines = loadRaw();
    int idx = -1;
    for (int i = 0; i < lines.size(); ++i) {
        if (lines[i].isEntry && lines[i].key == key) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        if (onDone) onDone(false); // 不存在
        return;
    }
    lines.removeAt(idx);
    writeContentAsync(joinLines(lines), [this, onDone](bool ok) {
        if (ok) {
            Q_EMIT variablesChanged();
        }
        if (onDone) onDone(ok);
    });
}

bool EnvVarStore::contains(const QString &key) const
{
    return load().contains(key);
}

QString EnvVarStore::value(const QString &key) const
{
    return load().value(key);
}

void EnvVarStore::writeContentAsync(const QByteArray &content, std::function<void(bool)> onDone)
{
    if (m_needsPolkit) {
        // 系统级：经 polkit helper 原子安装到 /etc/environment（environment 模式）
        DdeEnvHelper::runWithContentAsync(content, {QStringLiteral("environment")},
                                          std::move(onDone));
        return;
    }

    // 用户级：QSaveFile 原子替换，避免中途失败清空原文件
    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open file for writing:" << m_filePath << file.errorString();
        if (onDone) onDone(false);
        return;
    }
    file.write(content);
    const bool ok = file.commit();
    if (!ok) {
        qWarning() << "Failed to commit" << m_filePath << file.errorString();
    }
    if (onDone) onDone(ok);
}
