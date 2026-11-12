#pragma once

#include <GitTypes.h>

#include <QAbstractListModel>

#include <optional>

namespace gitapp {

class BranchesModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Role { NameRole = Qt::UserRole + 1, IsRemoteRole, IsCurrentRole, UpstreamRole };

    explicit BranchesModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    // Sắp xếp: nhánh hiện tại, các nhánh local còn lại (A→Z), rồi nhánh remote (A→Z).
    void setBranches(QVector<gitcore::BranchInfo> branches);
    [[nodiscard]] std::optional<gitcore::BranchInfo> find(const QString &name, bool remote) const;

private:
    QVector<gitcore::BranchInfo> m_items;
};

} // namespace gitapp
