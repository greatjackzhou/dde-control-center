#include "envvarmodel.h"
#include "envvarstore.h"

EnvVarModel::EnvVarModel(EnvVarStore *store, QObject *parent)
    : QAbstractListModel(parent)
    , m_store(store)
{
    connect(m_store, &EnvVarStore::variablesChanged, this, &EnvVarModel::reload);
    reload();
}

int EnvVarModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_keys.size();
}

QVariant EnvVarModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_keys.size()) {
        return QVariant();
    }
    switch (role) {
    case KeyRole:
        return m_keys.at(index.row());
    case ValueRole:
        return m_values.at(index.row());
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> EnvVarModel::roleNames() const
{
    return {
        {KeyRole, "key"},
        {ValueRole, "value"}
    };
}

void EnvVarModel::refresh()
{
    reload();
}

QString EnvVarModel::keyAt(int row) const
{
    if (row >= 0 && row < m_keys.size()) {
        return m_keys.at(row);
    }
    return QString();
}

QString EnvVarModel::valueAt(int row) const
{
    if (row >= 0 && row < m_values.size()) {
        return m_values.at(row);
    }
    return QString();
}

void EnvVarModel::reload()
{
    beginResetModel();
    QMap<QString, QString> vars = m_store->load();
    m_keys.clear();
    m_values.clear();
    for (auto it = vars.constBegin(); it != vars.constEnd(); ++it) {
        m_keys.append(it.key());
        m_values.append(it.value());
    }
    endResetModel();
    Q_EMIT countChanged();
}
