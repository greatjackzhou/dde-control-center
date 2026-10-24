#ifndef ENNVARSOTRE_H
#define ENNVARSOTRE_H

#include <QObject>
#include <QString>
#include <QMap>

class EnvVarStore : public QObject
{
    Q_OBJECT
public:
    explicit EnvVarStore(const QString &filePath, bool needsPolkit, QObject *parent = nullptr);

    // 读取文件，返回有序的 key-value 对列表
    QMap<QString, QString> load() const;

    // 写入整个变量集合到文件
    bool save(const QMap<QString, QString> &variables);

    // 单条操作
    bool addVariable(const QString &key, const QString &value);
    bool editVariable(const QString &oldKey, const QString &key, const QString &value);
    bool removeVariable(const QString &key);

    // 检查 key 是否已存在
    bool contains(const QString &key) const;

    // 获取指定变量值
    QString value(const QString &key) const;

    // 文件路径
    QString filePath() const { return m_filePath; }

    // 是否需要提权写入
    bool needsPolkit() const { return m_needsPolkit; }

Q_SIGNALS:
    void variablesChanged();

private:
    QMap<QString, QString> m_variables;
    QString m_filePath;
    bool m_needsPolkit;

    bool writeToFile(const QMap<QString, QString> &variables);
    bool writeViaPolkit(const QByteArray &content);
    bool writeDirect(const QByteArray &content);
};

#endif // ENNVARSOTRE_H
