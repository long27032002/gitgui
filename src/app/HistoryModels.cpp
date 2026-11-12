#include "HistoryModels.h"

#include <QDateTime>
#include <QVariantMap>

using namespace gitcore;

namespace gitapp {

CommitsModel::CommitsModel(QObject *parent) : QAbstractListModel(parent) {}

int CommitsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_items.size());
}

QHash<int, QByteArray> CommitsModel::roleNames() const
{
    return {{HashRole, "hash"}, {ShortHashRole, "shortHash"}, {AuthorRole, "author"},
            {DateRole, "dateText"}, {SubjectRole, "subject"}, {RefsRole, "refs"},
            {IsMergeRole, "isMerge"}, {ParentHashesRole, "parentHashes"},
            {OnCurrentBranchRole, "onCurrentBranch"}, {GraphNodeLaneRole, "graphNodeLane"},
            {GraphLaneCountRole, "graphLaneCount"}, {GraphEdgesRole, "graphEdges"},
            {GraphVerticalLanesRole, "graphVerticalLanes"}};
}

QVariant CommitsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return {};
    const CommitInfo &c = m_items.at(index.row());
    switch (role) {
    case HashRole:
        return c.hash;
    case ShortHashRole:
        return c.shortHash;
    case AuthorRole:
        return c.author;
    case DateRole:
        return QDateTime::fromSecsSinceEpoch(c.timestamp).toString(QStringLiteral("dd/MM/yyyy HH:mm"));
    case SubjectRole:
    case Qt::DisplayRole:
        return c.subject;
    case RefsRole:
        return c.refs;
    case IsMergeRole:
        return c.parentCount > 1;
    case ParentHashesRole:
        return c.parents;
    case OnCurrentBranchRole:
        return c.onCurrentBranch;
    case GraphNodeLaneRole:
        return index.row() < m_graph.size() ? m_graph.at(index.row()).nodeLane : 0;
    case GraphLaneCountRole:
        return index.row() < m_graph.size() ? m_graph.at(index.row()).laneCount : 1;
    case GraphEdgesRole:
        return index.row() < m_graph.size() ? m_graph.at(index.row()).edges : QVariantList{};
    case GraphVerticalLanesRole:
        return index.row() < m_graph.size() ? m_graph.at(index.row()).verticalLanes : QVariantList{};
    default:
        return {};
    }
}

void CommitsModel::setCommits(QVector<CommitInfo> commits)
{
    beginResetModel();
    m_items = std::move(commits);
    rebuildGraph();
    endResetModel();
}

void CommitsModel::append(const QVector<CommitInfo> &commits)
{
    if (commits.isEmpty())
        return;
    const int first = static_cast<int>(m_items.size());
    beginInsertRows({}, first, first + static_cast<int>(commits.size()) - 1);
    m_items += commits;
    rebuildGraph();
    endInsertRows();
}

void CommitsModel::rebuildGraph()
{
    m_graph.clear();
    m_graph.reserve(m_items.size());
    QVector<QString> active;
    const auto freeLane = [&active](int preferred) {
        for (int lane = qMax(0, preferred); lane < active.size(); ++lane)
            if (active.at(lane).isEmpty())
                return lane;
        for (int lane = 0; lane < qMin(preferred, active.size()); ++lane)
            if (active.at(lane).isEmpty())
                return lane;
        active.push_back({});
        return static_cast<int>(active.size() - 1);
    };

    for (const CommitInfo &commit : m_items) {
        int lane = active.indexOf(commit.hash);
        if (lane < 0) {
            lane = freeLane(0);
            active[lane] = commit.hash;
        }
        const int beforeCount = active.size();
        GraphRow row;
        row.nodeLane = lane;
        for (int i = 0; i < beforeCount; ++i)
            if (i != lane && !active.at(i).isEmpty())
                row.verticalLanes.push_back(i);

        active[lane].clear();
        for (qsizetype p = 0; p < commit.parents.size(); ++p) {
            const QString &parent = commit.parents.at(p);
            int target = active.indexOf(parent);
            if (target < 0) {
                if (p == 0 && active.at(lane).isEmpty())
                    target = lane;
                else
                    target = freeLane(lane + 1);
                active[target] = parent;
            }
            row.edges.push_back(QVariantMap{{QStringLiteral("toLane"), target},
                                            {QStringLiteral("branchLane"), lane}});
        }
        row.laneCount = qMax(qMax(beforeCount, active.size()), lane + 1);
        m_graph.push_back(std::move(row));
    }
}

int CommitsModel::rowOfHash(const QString &hash) const
{
    for (qsizetype i = 0; i < m_items.size(); ++i)
        if (m_items.at(i).hash == hash)
            return static_cast<int>(i);
    return -1;
}

CommitFilesModel::CommitFilesModel(QObject *parent) : QAbstractListModel(parent) {}

int CommitFilesModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_items.size());
}

QHash<int, QByteArray> CommitFilesModel::roleNames() const
{
    return {{StatusRole, "status"}, {PathRole, "path"}, {OrigPathRole, "origPath"}};
}

QVariant CommitFilesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return {};
    const CommitFile &f = m_items.at(index.row());
    switch (role) {
    case StatusRole:
        return QString(f.status);
    case PathRole:
    case Qt::DisplayRole:
        return f.path;
    case OrigPathRole:
        return f.origPath;
    default:
        return {};
    }
}

void CommitFilesModel::setFiles(QVector<CommitFile> files)
{
    beginResetModel();
    m_items = std::move(files);
    endResetModel();
}

} // namespace gitapp
