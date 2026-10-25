#ifndef ENVIRONMENTMODULE_H
#define ENVIRONMENTMODULE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>

class EnvVarStore;
class EnvVarModel;

class EnvironmentModule : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject *userModel READ userModel NOTIFY userModelChanged)
    Q_PROPERTY(QObject *systemModel READ systemModel NOTIFY systemModelChanged)
    Q_PROPERTY(QString userName READ userName NOTIFY userNameChanged)
    Q_PROPERTY(bool saving READ saving NOTIFY savingChanged)

public:
    explicit EnvironmentModule(QObject *parent = nullptr);
    ~EnvironmentModule() override;

    QObject *userModel() const;
    QObject *systemModel() const;
    QString userName() const;
    bool saving() const;

    // 用户变量 CRUD（异步）。发起即返回，结果经 operationResult 信号回传；
    // 期间 saving==true，QML 应据此禁用按钮，且新操作在 saving 时被忽略（单飞）。
    Q_INVOKABLE void addUserVar(const QString &key, const QString &value);
    Q_INVOKABLE void editUserVar(const QString &oldKey, const QString &key, const QString &value);
    Q_INVOKABLE void removeUserVar(const QString &key);

    // 系统变量 CRUD（异步）。涉及 pkexec 提权时全程非阻塞，不冻结界面。
    Q_INVOKABLE void addSystemVar(const QString &key, const QString &value);
    Q_INVOKABLE void editSystemVar(const QString &oldKey, const QString &key, const QString &value);
    Q_INVOKABLE void removeSystemVar(const QString &key);

    // PATH 专用操作
    Q_INVOKABLE QStringList getPathEntries(bool isSystem);
    Q_INVOKABLE void setPathEntries(const QStringList &entries, bool isSystem);

    // 验证变量名是否合法
    Q_INVOKABLE bool isValidVarName(const QString &name);

Q_SIGNALS:
    void userModelChanged();
    void systemModelChanged();
    void userNameChanged();
    void savingChanged();
    void operationResult(bool success, const QString &message);

private:
    EnvVarStore *m_userStore;
    EnvVarStore *m_systemStore;
    EnvVarModel *m_userModel;
    EnvVarModel *m_systemModel;
    QString m_userName;
    bool m_saving;

    void setSaving(bool saving);

    // 每个异步操作的统一收尾：清除 saving 状态并发射 operationResult。
    void finishOperation(bool ok, const QString &message);

    // 变量变更后同步到运行时环境（当前进程 + systemd 用户环境），
    // 使新启动的程序无需注销即可看到新值；系统变量的 logind 全局
    // 环境由提权 helper（dde-env-write）负责刷新。均为 best-effort（startDetached）。
    // 注意：systemctl --user set-environment 只影响 systemd 用户服务，
    // 交互式终端/桌面会话的 PATH 由登录期 profile 脚本决定，需配合
    // syncUserPathToProfile 写入 ~/.profile 托管块才能在重登后对这些生效。
    void applyRuntimeVar(const QString &key, const QString &value);
    void revokeRuntimeVar(const QString &key);

    // 用户级 environment.d 变更后刷新 systemd 用户环境 generator（非阻塞），
    // 确保下次登录会话能从 ~/.config/environment.d/ 加载最新变量。
    void reloadUserEnvironment();

    // 依据用户级 PATH 变量，在 ~/.profile 与 ~/.bashrc 中各维护一段带起止
    // 标记、可幂等增删的 PATH 托管块（QSaveFile 原子写），使新开终端
    // （无论登录/非登录 shell）无需重登即可拿到自定义 PATH；已运行的 GUI
    // 会话仍需重启或重登。PATH 为空时删除两处块；不触碰标记以外的用户内容。
    void syncUserPathToProfile();

    // 向单个 shell 启动文件（~/.profile / ~/.bashrc）以原子方式写入或清除带
    // 守卫的 PATH 托管块。dirs 为空时仅剔除旧块（文件原本不存在则不创建）。
    void writePathBlockToFile(const QString &filePath, const QStringList &dirs);

    // 依据系统级 PATH（以 /etc/environment 为列表存储源），经 polkit 提权异步
    // 安装/删除 /etc/profile.d/dde-control-center.sh 追加式托管脚本，完成后回调。
    // 脚本不存在且无需写入时跳过删除，避免多余授权。/etc/environment 的 PATH
    // 会被 /etc/profile 覆盖，真正的系统登录 shell 生效依赖本脚本。
    void syncSystemPathToProfileDAsync(std::function<void(bool)> onDone);
};

#endif // ENVIRONMENTMODULE_H
