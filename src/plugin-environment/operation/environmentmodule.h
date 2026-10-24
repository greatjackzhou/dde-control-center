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
    void applyRuntimeVar(const QString &key, const QString &value);
    void revokeRuntimeVar(const QString &key);
};

#endif // ENVIRONMENTMODULE_H
