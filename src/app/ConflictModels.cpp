#include "ConflictModels.h"

namespace gitapp {

ConflictFilesModel::ConflictFilesModel(QObject *parent) : QAbstractListModel(parent) {}

int ConflictFilesModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QHash<int, QByteArray> ConflictFilesModel::roleNames() const
{
    return {{PathRole, "path"}, {DescriptionRole, "description"}};
}

QVariant ConflictFilesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const Row &r = m_rows.at(index.row());
    switch (role) {
    case PathRole:
    case Qt::DisplayRole:
        return r.path;
    case DescriptionRole:
        return r.description;
    default:
        return {};
    }
}

void ConflictFilesModel::setRows(QVector<Row> rows)
{
    if (rows == m_rows)
        return;
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
}

int ConflictFilesModel::rowOfPath(const QString &path) const
{
    for (qsizetype i = 0; i < m_rows.size(); ++i)
        if (m_rows.at(i).path == path)
            return static_cast<int>(i);
    return -1;
}

ConflictBlocksModel::ConflictBlocksModel(QObject *parent) : QAbstractListModel(parent) {}

int ConflictBlocksModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QHash<int, QByteArray> ConflictBlocksModel::roleNames() const
{
    return {{NumberRole, "number"}, {MineRole, "mine"}, {IncomingRole, "incoming"},
            {BeforeRole, "before"}, {AfterRole, "after"}, {MineLabelRole, "mineLabel"},
            {IncomingLabelRole, "incomingLabel"}, {ChoiceRole, "choice"}};
}

QVariant ConflictBlocksModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const Row &r = m_rows.at(index.row());
    switch (role) {
    case NumberRole:
        return r.number;
    case MineRole:
        return r.mine;
    case IncomingRole:
        return r.incoming;
    case BeforeRole:
        return r.before;
    case AfterRole:
        return r.after;
    case MineLabelRole:
        return r.mineLabel;
    case IncomingLabelRole:
        return r.incomingLabel;
    case ChoiceRole:
        return r.choice;
    default:
        return {};
    }
}

void ConflictBlocksModel::setBlocks(QVector<Row> rows)
{
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
}

void ConflictBlocksModel::setChoice(int row, int choice)
{
    if (row < 0 || row >= m_rows.size() || m_rows.at(row).choice == choice)
        return;
    m_rows[row].choice = choice;
    emit dataChanged(index(row), index(row), {ChoiceRole});
}

int ConflictBlocksModel::chosenCount() const
{
    int n = 0;
    for (const Row &r : m_rows)
        n += r.choice != 0 ? 1 : 0;
    return n;
}

} // namespace gitapp
