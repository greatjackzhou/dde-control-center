#include "environmentmodule.h"
#include "envvarstore.h"
#include "envvarmodel.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QDebug>
#include <pwd.h>
#include <unistd.h>
#include <QRegularExpression>

#include "dccfactory.h"

EnvironmentModule::EnvironmentModule(QObject *parent)
    : QObject(parent)
    , m_saving(false)
{
    // 系统级环境变量文件
    QString systemPath = QStringLiteral("/etc/environment");
    m_systemStore = new EnvVarStore(systemPath, true, this);
    m_systemModel = new EnvVarModel(m_systemStore, this);

    // 用户级环境变量文件 ~/.pam_environment
    QString homeDir = QDir::homePath();
    QString userPath = homeDir + "/.pam_environment";
    m_userStore = new EnvVarStore(userPath, false, this);
    m_userModel = new EnvVarModel(m_userStore, this);

    // 获取当前用户名
    struct passwd *pw = getpwuid(getuid());
    if (pw) {
        m_userName = QString::fromLocal8Bit(pw->pw_name);
    } else {
        m_userName = qEnvironmentVariable("USER", "user");
    }

    // 确保用户文件存在
    if (!QFile::exists(userPath)) {
        QFile f(userPath);
        f.open(QIODevice::WriteOnly | QIODevice::Truncate);
        f.close();
    }
}

EnvironmentModule::~EnvironmentModule()
{
}

QObject *EnvironmentModule::userModel() const
{
    return m_userModel;
}

QObject *EnvironmentModule::systemModel() const
{
    return m_systemModel;
}

QString EnvironmentModule::userName() const
{
    return m_userName;
}

bool EnvironmentModule::saving() const
{
    return m_saving;
}

void EnvironmentModule::setSaving(bool saving)
{
    if (m_saving != saving) {
        m_saving = saving;
        Q_EMIT savingChanged();
    }
}

bool EnvironmentModule::addUserVar(const QString &key, const QString &value)
{
    setSaving(true);
    bool ok = m_userStore->addVariable(key, value);
    setSaving(false);
    if (ok) {
        applyRuntimeVar(key, value);
    }
    Q_EMIT operationResult(ok, ok ? tr("Added successfully") : tr("Failed to add: variable already exists"));
    return ok;
}

bool EnvironmentModule::editUserVar(const QString &oldKey, const QString &key, const QString &value)
{
    setSaving(true);
    bool ok = m_userStore->editVariable(oldKey, key, value);
    setSaving(false);
    if (ok) {
        if (oldKey != key) {
            revokeRuntimeVar(oldKey);
        }
        applyRuntimeVar(key, value);
    }
    Q_EMIT operationResult(ok, ok ? tr("Modified successfully") : tr("Failed to modify"));
    return ok;
}

bool EnvironmentModule::removeUserVar(const QString &key)
{
    setSaving(true);
    bool ok = m_userStore->removeVariable(key);
    setSaving(false);
    if (ok) {
        revokeRuntimeVar(key);
    }
    Q_EMIT operationResult(ok, ok ? tr("Deleted successfully") : tr("Failed to delete"));
    return ok;
}

bool EnvironmentModule::addSystemVar(const QString &key, const QString &value)
{
    setSaving(true);
    bool ok = m_systemStore->addVariable(key, value);
    setSaving(false);
    if (ok) {
        // logind 全局环境已由 helper 刷新，这里同步当前用户会话环境
        applyRuntimeVar(key, value);
    }
    Q_EMIT operationResult(ok, ok ? tr("Added successfully") : tr("Failed to add: variable already exists or permission denied"));
    return ok;
}

bool EnvironmentModule::editSystemVar(const QString &oldKey, const QString &key, const QString &value)
{
    setSaving(true);
    bool ok = m_systemStore->editVariable(oldKey, key, value);
    setSaving(false);
    if (ok) {
        if (oldKey != key) {
            revokeRuntimeVar(oldKey);
        }
        applyRuntimeVar(key, value);
    }
    Q_EMIT operationResult(ok, ok ? tr("Modified successfully") : tr("Failed to modify or permission denied"));
    return ok;
}

bool EnvironmentModule::removeSystemVar(const QString &key)
{
    setSaving(true);
    bool ok = m_systemStore->removeVariable(key);
    setSaving(false);
    if (ok) {
        revokeRuntimeVar(key);
    }
    Q_EMIT operationResult(ok, ok ? tr("Deleted successfully") : tr("Failed to delete or permission denied"));
    return ok;
}

QStringList EnvironmentModule::getPathEntries(bool isSystem)
{
    EnvVarStore *store = isSystem ? m_systemStore : m_userStore;
    QString pathValue = store->value("PATH");
    if (pathValue.isEmpty()) {
        return {};
    }
    return pathValue.split(':', Qt::SkipEmptyParts);
}

bool EnvironmentModule::setPathEntries(const QStringList &entries, bool isSystem)
{
    EnvVarStore *store = isSystem ? m_systemStore : m_userStore;
    QString newPath = entries.join(':');
    setSaving(true);
    bool ok;
    if (store->contains("PATH")) {
        ok = store->editVariable("PATH", "PATH", newPath);
    } else {
        ok = store->addVariable("PATH", newPath);
    }
    setSaving(false);
    if (ok) {
        applyRuntimeVar(QStringLiteral("PATH"), newPath);
    }
    Q_EMIT operationResult(ok, ok ? tr("PATH updated") : tr("Failed to update PATH"));
    return ok;
}

// 将变量写入运行时环境：
// 1) qputenv 更新当前 DCC 进程（其直接拉起的子进程立即可见）；
// 2) systemctl --user set-environment 更新 systemd 用户环境，
//    会话内新启动的组件无需注销即可继承新值。best-effort，失败仅记日志。
void EnvironmentModule::applyRuntimeVar(const QString &key, const QString &value)
{
    QString effective = value;
    if (key == QLatin1String("PATH")) {
        // 与当前会话 PATH 合并，避免覆盖登录期注入的标准路径
        QStringList merged = value.split(':', Qt::SkipEmptyParts);
        const QStringList current =
            qEnvironmentVariable("PATH").split(':', Qt::SkipEmptyParts);
        for (const QString &entry : current) {
            if (!merged.contains(entry)) {
                merged.append(entry);
            }
        }
        effective = merged.join(':');
    }

    qputenv(key.toLatin1().constData(), effective.toUtf8());

    const QString assignment = QStringLiteral("%1=%2").arg(key, effective);
    if (!QProcess::startDetached("systemctl",
            {QStringLiteral("--user"), QStringLiteral("set-environment"), assignment})) {
        qWarning() << "Failed to sync runtime env via systemctl --user:" << key;
    }
}

// 从运行时环境撤销变量，与 applyRuntimeVar 对称。
void EnvironmentModule::revokeRuntimeVar(const QString &key)
{
    // PATH 删除时不撤销运行时值，回退到登录期继承的 PATH，避免会话失去标准路径
    if (key == QLatin1String("PATH")) {
        return;
    }

    unsetenv(key.toLatin1().constData());

    if (!QProcess::startDetached("systemctl",
            {QStringLiteral("--user"), QStringLiteral("unset-environment"), key})) {
        qWarning() << "Failed to unset runtime env via systemctl --user:" << key;
    }
}

bool EnvironmentModule::isValidVarName(const QString &name)
{
    if (name.isEmpty()) return false;
    // 合法变量名：以字母或下划线开头，后跟字母、数字、下划线
    static QRegularExpression re("^[A-Za-z_][A-Za-z0-9_]*$");
    return re.match(name).hasMatch();
}

DCC_FACTORY_CLASS(EnvironmentModule)
#include "environmentmodule.moc"
