#pragma once

#include <GitTypes.h>

#include <QAbstractListModel>
#include <QVariantList>

namespace gitapp {

class CommitsModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Role { HashRole = Qt::UserRole + 1, ShortHashRole, AuthorRole, DateRole, SubjectRole,
                RefsRole, IsMergeRole, ParentHashesRole, OnCurrentBranchRole,
                GraphNodeLaneRole, GraphLaneCountRole, GraphEdgesRole, GraphVerticalLanesRole };

    explicit CommitsModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void setCommits(QVector<gitcore::CommitInfo> commits);
    void append(const QVector<gitcore::CommitInfo> &commits);
    [[nodiscard]] const gitcore::CommitInfo &at(int row) const { return m_items.at(row); }
    [[nodiscard]] int rowOfHash(const QString &hash) const;

private:
    struct GraphRow {
        int nodeLane = 0;
        int laneCount = 1;
        QVariantList edges;
        QVariantList verticalLanes;
    };
    void rebuildGraph();
    QVector<gitcore::CommitInfo> m_items;
    QVector<GraphRow> m_graph;
};

class CommitFilesModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Role { StatusRole = Qt::UserRole + 1, PathRole, OrigPathRole };

    explicit CommitFilesModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void setFiles(QVector<gitcore::CommitFile> files);
    [[nodiscard]] const gitcore::CommitFile &at(int row) const { return m_items.at(row); }

private:
    QVector<gitcore::CommitFile> m_items;
};

} // namespace gitapp
