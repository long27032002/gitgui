#include "ChangesModel.h"

using gitcore::ChangeKind;
using gitcore::FileChange;

namespace gitapp {

ChangesModel::ChangesModel(QObject *parent) : QAbstractListModel(parent) {}

int ChangesModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_items.size());
}

QVariant ChangesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return {};
    const FileChange &fc = m_items.at(index.row());
    switch (role) {
    case PathRole:
    case Qt::DisplayRole:
        return fc.path;
    case OrigPathRole:
        return fc.origPath;
    case StatusRole:
        return statusLabel(fc);
    case CheckStateRole:
        return checkStateOfPath(fc.path);
    default:
        return {};
    }
}

QHash<int, QByteArray> ChangesModel::roleNames() const
{
    return {
        {PathRole, "path"},
        {OrigPathRole, "origPath"},
        {StatusRole, "status"},
        {CheckStateRole, "checkState"},
    };
}

void ChangesModel::setChanges(QVector<FileChange> items)
{
    bool samePaths = items.size() == m_items.size();
    for (qsizetype i = 0; samePaths && i < items.size(); ++i)
        samePaths = items.at(i).path == m_items.at(i).path;

    if (samePaths) {
        m_items = std::move(items);
        if (!m_items.isEmpty())
            emit dataChanged(index(0), index(static_cast<int>(m_items.size()) - 1));
        return;
    }
    beginResetModel();
    m_items = std::move(items);
    endResetModel();
}

int ChangesModel::rowOfPath(const QString &path) const
{
    for (qsizetype i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).path == path)
            return static_cast<int>(i);
    }
    return -1;
}

void ChangesModel::setCheckState(const QString &path, int state)
{
    if (checkStateOfPath(path) == state)
        return;
    if (state == 2)
        m_check.remove(path);
    else
        m_check.insert(path, state);
    const int row = rowOfPath(path);
    if (row >= 0)
        emit dataChanged(index(row), index(row), {CheckStateRole});
}

QString ChangesModel::statusLabel(const FileChange &fc)
{
    switch (fc.kind) {
    case ChangeKind::Untracked:
        return QStringLiteral("?");
    case ChangeKind::Unmerged:
        return QStringLiteral("U");
    case ChangeKind::Renamed:
        return QStringLiteral("R");
    case ChangeKind::Ordinary:
        break;
    }
    const QChar c = fc.worktreeStatus != QLatin1Char('.') ? fc.worktreeStatus : fc.indexStatus;
    return QString(c);
}

} // namespace gitapp
