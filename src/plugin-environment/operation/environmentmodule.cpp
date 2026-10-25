#include "environmentmodule.h"
#include "envvarstore.h"
#include "envvarmodel.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryFile>
#include <QDebug>
#include <pwd.h>
#include <unistd.h>
#include <QRegularExpression>
#include <QSet>

#include "dccfactory.h"

// 用户级 PATH 采用非破坏性存储：持久化值尾部带一个 "$PATH" 哨兵段，
// 表示"保留登录期标准路径"。读回展示给编辑器前需把哨兵剥离，
// 以免用户把它当成一个可编辑的路径条目。
static QStringList stripPathSentinel(const QString &raw)
{
    const QStringList all = raw.split(':', Qt::SkipEmptyParts);
    QStringList concrete;
    for (const QString &entry : all) {
        if (entry == QLatin1String("$PATH") || entry == QLatin1String("${PATH}")) {
            continue;
        }
        concrete.append(entry);
    }
    return concrete;
}

// 系统级 PATH 同样采用非破坏性存储：写入 /etc/environment 的值末尾追加一段
// “标准基准路径”，使不经过 /etc/profile、直接读 pam_env 的图形会话也不会
// 丢失 /usr/bin 等系统目录。展示给编辑器前需把这些基准目录剥离，只显示
// 用户真正添加的部分。
static QStringList standardPathBaseDirs()
{
    return { QStringLiteral("/usr/local/sbin"), QStringLiteral("/usr/local/bin"),
             QStringLiteral("/usr/sbin"), QStringLiteral("/usr/bin"),
             QStringLiteral("/sbin"), QStringLiteral("/bin") };
}

static QStringList stripStandardBase(const QString &raw)
{
    const QStringList baseList = standardPathBaseDirs();
    const QSet<QString> base(baseList.cbegin(), baseList.cend());
    const QStringList all = raw.split(':', Qt::SkipEmptyParts);
    QStringList out;
    for (const QString &entry : all) {
        if (base.contains(entry)) {
            continue;
        }
        out.append(entry);
    }
    return out;
}

// 提权 helper 绝对路径：由 CMake 以 DDE_ENV_HELPER_PATH 宏单点传入，与 policy
// 的 exec.path 注解、install 目标同源；未定义时退回 DDE libexec 安装位。
#ifndef DDE_ENV_HELPER_PATH
#define DDE_ENV_HELPER_PATH "/usr/libexec/dde-control-center/dde-env-write"
#endif
static const QString kEnvHelper = QStringLiteral(DDE_ENV_HELPER_PATH);

// 单引号安全包裹一个目录：生成脚本中以字面量使用，不会触发命令替换。
static QString shellSingleQuote(const QString &s)
{
    QString escaped = s;
    escaped.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
    return QLatin1Char('\'') + escaped + QLatin1Char('\'');
}

// 生成 /etc/profile.d/dde-control-center.sh 的完整内容（追加式、去重、不覆盖基准 PATH）。
// 无可安全嵌入的目录时返回空串（调用方据此删除脚本）。
static QString buildProfileDContent(const QStringList &dirs)
{
    QStringList quoted;
    for (const QString &d : dirs) {
        // 换行无法安全嵌入 for 列表，跳过（其余字符均在单引号内字面）
        if (d.contains(QLatin1Char('\n')) || d.contains(QLatin1Char('\r')))
            continue;
        quoted << shellSingleQuote(d);
    }
    if (quoted.isEmpty())
        return QString();
    return QStringLiteral(
               "#!/bin/sh\n"
               "# >>> dde-control-center managed PATH profile.d >>>\n"
               "# Managed by DDE Control Center environment plugin.\n"
               "# Manage system PATH in Control Center; manual edits here are overwritten on save.\n"
               "for d in ")
        + quoted.join(QLatin1Char(' '))
        + QStringLiteral(
               "; do\n"
               "    case \":${PATH}:\" in\n"
               "        *\":${d}:\"*) ;;\n"
               "        *) PATH=\"${d}:${PATH}\" ;;\n"
               "    esac\n"
               "done\n"
               "export PATH\n"
               "# <<< dde-control-center managed PATH profile.d <<<\n");
}

// 统一调用 pkexec helper 并按退出码判定（126=取消/未授权；127=helper 不可执行）。
static bool runEnvHelper(const QStringList &argsAfterHelper)
{
    QProcess process;
    QStringList args;
    args << kEnvHelper << argsAfterHelper;
    process.start(QStringLiteral("pkexec"), args);
    if (!process.waitForStarted(30000)) {
        qWarning() << "pkexec failed to start env helper";
        return false;
    }
    if (!process.waitForFinished(30000)) {
        qWarning() << "pkexec env helper timed out";
        process.kill();
        process.waitForFinished(1000);
        return false;
    }
    const int code = process.exitCode();
    if (code != 0) {
        const QString err = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
        if (code == 126)
            qWarning() << "polkit authorization cancelled or denied (code 126)";
        else if (code == 127)
            qWarning() << "env helper not executable (code 127):" << kEnvHelper;
        else
            qWarning() << "env helper failed (code" << code << "):" << err;
        return false;
    }
    return true;
}

// 将内容经临时文件 + pkexec 安装到 /etc/profile.d/dde-control-center.sh。
static bool installProfileD(const QString &content)
{
    QTemporaryFile tmpFile(QDir::tempPath() + QStringLiteral("/dde-env-XXXXXX"));
    tmpFile.setAutoRemove(false);
    if (!tmpFile.open()) {
        qWarning() << "Failed to create temp file:" << tmpFile.errorString();
        return false;
    }
    tmpFile.write(content.toUtf8());
    tmpFile.flush();
    tmpFile.close();
    const QString tmpPath = tmpFile.fileName();

    const bool ok = runEnvHelper({QStringLiteral("profile-d"), tmpPath});
    QFile::remove(tmpPath);
    return ok;
}

// 删除 /etc/profile.d/dde-control-center.sh（不存在时 helper rm -f 也返回 0）。
static bool removeProfileD()
{
    return runEnvHelper({QStringLiteral("profile-d-remove")});
}

EnvironmentModule::EnvironmentModule(QObject *parent)
    : QObject(parent)
    , m_saving(false)
{
    // 系统级环境变量文件
    QString systemPath = QStringLiteral("/etc/environment");
    m_systemStore = new EnvVarStore(systemPath, true, this);
    m_systemModel = new EnvVarModel(m_systemStore, this);

    // 用户级环境变量文件 ~/.config/environment.d/*.conf（systemd 用户环境标准机制）。
    // 不使用 ~/.pam_environment：因 CVE-2010-4708 等安全风险，pam_env 已被多数发行版弃用。
    // 变量集中在插件专用文件 dde-control-center.conf 中，避免与其他软件的 conf 片段互相干扰。
    QString userEnvDir = QDir::homePath() + "/.config/environment.d";
    QString userPath = userEnvDir + "/dde-control-center.conf";
    m_userStore = new EnvVarStore(userPath, false, this);
    m_userModel = new EnvVarModel(m_userStore, this);

    // 获取当前用户名
    struct passwd *pw = getpwuid(getuid());
    if (pw) {
        m_userName = QString::fromLocal8Bit(pw->pw_name);
    } else {
        m_userName = qEnvironmentVariable("USER", "user");
    }

    // 确保 environment.d 目录与插件专用文件存在
    QDir().mkpath(userEnvDir);
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
        reloadUserEnvironment();
        if (key == QLatin1String("PATH")) {
            syncUserPathToProfile();
        }
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
        reloadUserEnvironment();
        if (key == QLatin1String("PATH") || oldKey == QLatin1String("PATH")) {
            syncUserPathToProfile();
        }
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
        reloadUserEnvironment();
        if (key == QLatin1String("PATH")) {
            syncUserPathToProfile();
        }
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
        if (key == QLatin1String("PATH")) {
            syncSystemPathToProfileD();
        }
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
        if (key == QLatin1String("PATH") || oldKey == QLatin1String("PATH")) {
            syncSystemPathToProfileD();
        }
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
        if (key == QLatin1String("PATH")) {
            syncSystemPathToProfileD();
        }
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
    // 用户级尾部可能带 "$PATH" 哨兵、系统级末尾带标准基准目录，均为非破坏性
    // 存储标记，展示给编辑器前需剥离，只显示用户真正添加的目录。
    if (!isSystem) {
        return stripPathSentinel(pathValue);
    }
    return stripStandardBase(pathValue);
}

bool EnvironmentModule::setPathEntries(const QStringList &entries, bool isSystem)
{
    EnvVarStore *store = isSystem ? m_systemStore : m_userStore;

    // 过滤掉空段与用户可能误粘的 $PATH 哨兵，得到具体目录列表
    QStringList concrete;
    for (const QString &entry : entries) {
        if (entry.isEmpty()
            || entry == QLatin1String("$PATH")
            || entry == QLatin1String("${PATH}")) {
            continue;
        }
        concrete.append(entry);
    }

    setSaving(true);
    bool ok;
    if (concrete.isEmpty()) {
        // 清空 PATH：删除该变量（仅当存在时），交由登录期 profile 决定最终值
        ok = store->contains("PATH") ? store->removeVariable("PATH") : true;
    } else {
        // 系统级：写入 "自定义目录:标准基准"，使直读 pam_env 的 GUI 会话既
        // 不丢 /usr/bin 等系统目录，又能拿到自定义目录（编辑器展示时剥离基准）
        // 用户级：追加 :$PATH 哨兵做非破坏性写入，重登后 systemd 服务仍保留标准路径
        QString newPath = isSystem
            ? concrete.join(QLatin1Char(':')) + QLatin1Char(':') + standardPathBaseDirs().join(QLatin1Char(':'))
            : concrete.join(QLatin1Char(':')) + QStringLiteral(":$PATH");
        if (store->contains("PATH")) {
            ok = store->editVariable("PATH", "PATH", newPath);
        } else {
            ok = store->addVariable("PATH", newPath);
        }
    }
    setSaving(false);
    if (ok) {
        if (!isSystem) {
            // 用户级：先同步运行时（当前进程 + systemd 用户环境），再刷新
            // generator，并把自定义目录落到 ~/.profile 托管块供终端/桌面继承
            if (!concrete.isEmpty()) {
                applyRuntimeVar(QStringLiteral("PATH"), concrete.join(':'));
            }
            reloadUserEnvironment();
            syncUserPathToProfile();
        } else {
            if (!concrete.isEmpty()) {
                applyRuntimeVar(QStringLiteral("PATH"), concrete.join(':'));
            }
            // 系统级：同步维护 /etc/profile.d 追加脚本（dirs 为空时删除）
            syncSystemPathToProfileD();
        }
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

// 用户级持久化文件位于 ~/.config/environment.d/，由
// systemd-user-environment-generator 在用户管理器启动时解析。
// 仅靠 set-environment 同步当前会话不够：不刷新 generator，
// 下次登录会话仍会读到旧的环境快照。daemon-reload 会重新执行
// generator 并刷新用户管理器的环境缓存。best-effort，失败仅记日志。
void EnvironmentModule::reloadUserEnvironment()
{
    QProcess process;
    process.start("systemctl", {QStringLiteral("--user"), QStringLiteral("daemon-reload")});
    if (!process.waitForFinished(10000)) {
        qWarning() << "Failed to reload systemd user environment generator";
        process.kill();
        process.waitForFinished(1000);
    } else if (process.exitCode() != 0) {
        qWarning() << "systemctl --user daemon-reload failed:"
                   << QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
    }
}

// 在 ~/.profile 与 ~/.bashrc 中各维护一段带起止标记的 PATH 托管块，使新开
// 终端（无论登录还是非登录交互式 shell）无需重登就能拿到用户级自定义 PATH：
// - ~/.profile 覆盖登录 shell；
// - ~/.bashrc 覆盖桌面终端默认拉起的非登录交互式 shell。
// 块内用 DDE_ENV_PATH_APPLIED 环境变量做幂等守卫，避免同一 shell 链中
// 两文件都被 source 时重复 prepend。PATH 为空时删除两处块。best-effort。
void EnvironmentModule::syncUserPathToProfile()
{
    const QStringList dirs = stripPathSentinel(m_userStore->value("PATH"));
    const QString home = QDir::homePath();
    // 两个启动文件分别管理；dirs 为空时 writePathBlockToFile 仅剔除旧块
    writePathBlockToFile(home + QStringLiteral("/.profile"), dirs);
    writePathBlockToFile(home + QStringLiteral("/.bashrc"), dirs);
}

// 向单个 shell 启动文件写入或清除带守卫的 PATH 托管块。重写时只替换
// 标记之间内容，不触碰用户其余配置；文件不存在且无内容可写时不创建。
void EnvironmentModule::writePathBlockToFile(const QString &filePath, const QStringList &dirs)
{
    static const QString kBegin = QStringLiteral("# >>> dde-control-center managed PATH >>>");
    static const QString kEnd = QStringLiteral("# <<< dde-control-center managed PATH <<<");

    // 读取现有内容（不存在时不预先创建）
    QString content;
    QFile rf(filePath);
    const bool existed = rf.exists();
    if (existed) {
        if (!rf.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "Failed to read" << filePath << rf.errorString();
            return;
        }
        content = QString::fromUtf8(rf.readAll());
        rf.close();
    }

    // 剔除旧的托管块（含起止标记行），保留其余一切内容
    QString stripped;
    bool inBlock = false;
    const QStringList lines = content.split('\n');
    for (const QString &line : lines) {
        const QString t = line.trimmed();
        if (t == kBegin) {
            inBlock = true;
            continue;
        }
        if (t == kEnd) {
            inBlock = false;
            continue;
        }
        if (!inBlock) {
            stripped += line + '\n';
        }
    }
    // 收敛尾部多余空行，保证重写稳定
    while (stripped.endsWith(QStringLiteral("\n\n"))) {
        stripped.chop(1);
    }

    // 无自定义目录：若原本就没有文件，则无需创建也不需剔除；
    // 否则写回已剔除块的内容。
    if (dirs.isEmpty()) {
        if (!existed) {
            return;
        }
        QFile wf(filePath);
        if (!wf.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            qWarning() << "Failed to write" << filePath << wf.errorString();
            return;
        }
        wf.write(stripped.toUtf8());
        wf.close();
        return;
    }

    // 追加新的托管块：守卫确保同一会话链只应用一次；目录逐个以单引号字面量
    // 注入（不触发命令替换），去重后前插到基准 PATH，非破坏性。
    QStringList quotedDirs;
    for (const QString &d : dirs) {
        if (d.contains(QLatin1Char('\n')) || d.contains(QLatin1Char('\r'))) {
            continue;
        }
        quotedDirs << shellSingleQuote(d);
    }

    QString out = stripped;
    if (!out.isEmpty() && !out.endsWith('\n')) {
        out += '\n';
    }
    QString pathApply;
    if (!quotedDirs.isEmpty()) {
        pathApply =
            QStringLiteral("    for d in ") + quotedDirs.join(QLatin1Char(' '))
            + QStringLiteral(" ; do\n")
            + QStringLiteral("        case \":${PATH}:\" in\n")
            + QStringLiteral("            *\":${d}:\"*) ;;\n")
            + QStringLiteral("            *) PATH=\"${d}:${PATH}\" ;;\n")
            + QStringLiteral("        esac\n")
            + QStringLiteral("    done\n")
            + QStringLiteral("    export PATH\n");
    }
    const QString block =
        QStringLiteral("\n") + kBegin + QStringLiteral("\n")
        + QStringLiteral("# 由 DDE 控制中心自动维护，请在“环境变量”页面中编辑；\n")
        + QStringLiteral("# 手动修改本块内容会在下次保存时被覆盖。\n")
        + QStringLiteral("if [ -z \"$DDE_ENV_PATH_APPLIED\" ]; then\n")
        + QStringLiteral("    export DDE_ENV_PATH_APPLIED=1\n")
        + pathApply
        + QStringLiteral("fi\n")
        + kEnd + QStringLiteral("\n");
    out += block;

    QFile wf(filePath);
    if (!wf.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        qWarning() << "Failed to write" << filePath << wf.errorString();
        return;
    }
    wf.write(out.toUtf8());
    wf.close();
}

// 根据系统级 PATH 安装/删除 /etc/profile.d/dde-control-center.sh（经 polkit 提权）。
// /etc/environment 仍作为系统变量列表的存储源（供 UI 展示与回读），但它的 PATH
// 会被 login.defs 与 /etc/profile 覆盖，对登录 shell 无效；真正的生效靠本脚本。
void EnvironmentModule::syncSystemPathToProfileD()
{
    // 取自定义目录（剥离尾部标准基准）；为空则删除脚本。
    const QStringList dirs = stripStandardBase(m_systemStore->value(QStringLiteral("PATH")));
    if (dirs.isEmpty()) {
        removeProfileD();
        return;
    }
    const QString content = buildProfileDContent(dirs);
    if (content.isEmpty()) {
        // 无可安全写入的目录，删除脚本以免残留旧值
        removeProfileD();
        return;
    }
    installProfileD(content);
}

DCC_FACTORY_CLASS(EnvironmentModule)
#include "environmentmodule.moc"
