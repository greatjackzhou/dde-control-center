#ifndef ENNVARSOTRE_H
#define ENNVARSOTRE_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QList>
#include <functional>

class EnvVarStore : public QObject
{
    Q_OBJECT
public:
    explicit EnvVarStore(const QString &filePath, bool needsPolkit, QObject *parent = nullptr);

    // 读取文件，返回 key-value 映射（供 EnvVarModel 展示；跳过注释/空行）
    QMap<QString, QString> load() const;

    // 单条操作（异步）。direct 目标立即回调；polkit 目标经 pkexec helper 后回调。
    // 前置条件不满足（新增重名 / 编辑改名为已存在的键 / 删除不存在的键）时回调
    // false 且不写文件。写入成功后发射 variablesChanged()。
    void addVariableAsync(const QString &key, const QString &value, std::function<void(bool)> onDone);
    void editVariableAsync(const QString &oldKey, const QString &key, const QString &value, std::function<void(bool)> onDone);
    void removeVariableAsync(const QString &key, std::function<void(bool)> onDone);

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
    // 逐行保留原始文本（含注释/空行），以支持增量写、保序保注释。
    struct Line {
        QString raw;    // 原始整行文本（不含换行符）
        QString key;    // 条目行的键；非条目行为空
        bool isEntry;   // 是否为合法 KEY=VALUE 条目行
    };

    QList<Line> loadRaw() const;
    // 依 direct/polkit 分派异步写整份内容
    void writeContentAsync(const QByteArray &content, std::function<void(bool)> onDone);
    // 生成一条 KEY=VALUE 文本（值含空白/冒号/分号时加双引号）
    static QString formatEntryLine(const QString &key, const QString &value);
    // 把行模型拼回文件内容
    static QByteArray joinLines(const QList<Line> &lines);

    QString m_filePath;
    bool m_needsPolkit;
};

#endif // ENNVARSOTRE_H
