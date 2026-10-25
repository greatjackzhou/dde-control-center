#ifndef ENVIRONMENTMODULE_H
#define ENVIRONMENTMODULE_H

#include <QObject>
#include <QString>
#include <QStringList>

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

    // 用户变量 CRUD
    Q_INVOKABLE bool addUserVar(const QString &key, const QString &value);
    Q_INVOKABLE bool editUserVar(const QString &oldKey, const QString &key, const QString &value);
    Q_INVOKABLE bool removeUserVar(const QString &key);

    // 系统变量 CRUD
    Q_INVOKABLE bool addSystemVar(const QString &key, const QString &value);
    Q_INVOKABLE bool editSystemVar(const QString &oldKey, const QString &key, const QString &value);
    Q_INVOKABLE bool removeSystemVar(const QString &key);

    // PATH 专用操作
    Q_INVOKABLE QStringList getPathEntries(bool isSystem);
    Q_INVOKABLE bool setPathEntries(const QStringList &entries, bool isSystem);

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

    // 变量变更后同步到运行时环境（当前进程 + systemd 用户环境），
    // 使新启动的程序无需注销即可看到新值；系统变量的 logind 全局
    // 环境由提权 helper（dde-env-write）负责刷新。
    // 注意：systemctl --user set-environment 只影响 systemd 用户服务，
    // 交互式终端/桌面会话的 PATH 由登录期 profile 脚本决定，需配合
    // syncUserPathToProfile 写入 ~/.profile 托管块才能在重登后对这些生效。
    void applyRuntimeVar(const QString &key, const QString &value);
    void revokeRuntimeVar(const QString &key);

    // 用户级 environment.d 变更后刷新 systemd 用户环境 generator，
    // 确保下次登录会话能从 ~/.config/environment.d/ 加载最新变量。
    void reloadUserEnvironment();

    // 依据用户级 PATH 变量，在 ~/.profile 与 ~/.bashrc 中各维护一段带起止
    // 标记、可幂等增删的 PATH 托管块，使新开终端（无论登录/非登录 shell）
    // 无需重登即可拿到自定义 PATH；已运行的 GUI 会话仍需重启或重登。
    // PATH 为空时删除两处块；不触碰标记以外的用户内容。best-effort。
    void syncUserPathToProfile();

    // 向单个 shell 启动文件（~/.profile / ~/.bashrc）写入或清除带守卫的
    // PATH 托管块。dirs 为空时仅剔除旧块（文件原本不存在则不创建）。
    void writePathBlockToFile(const QString &filePath, const QStringList &dirs);

    // 依据系统级 PATH（以 /etc/environment 为列表存储源），经 polkit 提权在
    // /etc/profile.d/dde-control-center.sh 安装一个“追加式”托管脚本，使系统
    // PATH 对全体用户的登录 shell/GUI 会话重登后真正生效（/etc/environment 的
    // PATH 会被 /etc/profile 覆盖，单写它无效）。PATH 为空时删除该脚本。
    // best-effort，失败仅记日志，不影响 /etc/environment 的写入结果。
    void syncSystemPathToProfileD();
};

#endif // ENVIRONMENTMODULE_H
