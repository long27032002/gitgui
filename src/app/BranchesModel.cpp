#include "BranchesModel.h"

#include <algorithm>

using gitcore::BranchInfo;

namespace gitapp {

BranchesModel::BranchesModel(QObject *parent) : QAbstractListModel(parent) {}

int BranchesModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_items.size());
}

QHash<int, QByteArray> BranchesModel::roleNames() const
{
    return {{NameRole, "name"}, {IsRemoteRole, "isRemote"},
            {IsCurrentRole, "isCurrent"}, {UpstreamRole, "upstream"}};
}

QVariant BranchesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return {};
    const BranchInfo &b = m_items.at(index.row());
    switch (role) {
    case NameRole:
    case Qt::DisplayRole:
        return b.name;
    case IsRemoteRole:
        return b.isRemote;
    case IsCurrentRole:
        return b.isCurrent;
    case UpstreamRole:
        return b.upstream;
    default:
        return {};
    }
}

void BranchesModel::setBranches(QVector<BranchInfo> branches)
{
    std::sort(branches.begin(), branches.end(), [](const BranchInfo &a, const BranchInfo &b) {
        if (a.isRemote != b.isRemote)
            return !a.isRemote;
        if (a.isCurrent != b.isCurrent)
            return a.isCurrent;
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
    if (branches == m_items)
        return;
    beginResetModel();
    m_items = std::move(branches);
    endResetModel();
}

std::optional<BranchInfo> BranchesModel::find(const QString &name, bool remote) const
{
    for (const BranchInfo &b : m_items)
        if (b.name == name && b.isRemote == remote)
            return b;
    return std::nullopt;
}

} // namespace gitapp
