#ifndef ENNVARMODEL_H
#define ENNVARMODEL_H

#include <QAbstractListModel>
#include <QMap>
#include <QStringList>

class EnvVarStore;

class EnvVarModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        KeyRole = Qt::UserRole + 1,
        ValueRole
    };

    explicit EnvVarModel(EnvVarStore *store, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // 刷新模型数据（从 store 重新加载）
    Q_INVOKABLE void refresh();

    // 按行号获取 key
    Q_INVOKABLE QString keyAt(int row) const;
    // 按行号获取 value
    Q_INVOKABLE QString valueAt(int row) const;

Q_SIGNALS:
    void countChanged();

private:
    EnvVarStore *m_store;
    QStringList m_keys;
    QStringList m_values;

    void reload();
};

#endif // ENNVARMODEL_H
