#pragma once

#include <QAbstractListModel>
#include <QVector>

namespace gitapp {

// Danh sách file đang xung đột.
class ConflictFilesModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Role { PathRole = Qt::UserRole + 1, DescriptionRole };
    struct Row {
        QString path;
        QString description;
        bool operator==(const Row &) const = default;
    };

    explicit ConflictFilesModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void setRows(QVector<Row> rows);
    [[nodiscard]] const Row &at(int row) const { return m_rows.at(row); }
    [[nodiscard]] int rowOfPath(const QString &path) const;

private:
    QVector<Row> m_rows;
};

// Các khối xung đột của file đang chọn, kèm lựa chọn của người dùng cho từng khối.
class ConflictBlocksModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Role { NumberRole = Qt::UserRole + 1, MineRole, IncomingRole, BeforeRole, AfterRole,
                MineLabelRole, IncomingLabelRole, ChoiceRole };
    // ChoiceRole: 0 chưa chọn, 1 bản của tôi, 2 bản của họ, 3 giữ cả hai (của tôi trước)
    struct Row {
        int number = 0;
        QString mine, incoming, before, after, mineLabel, incomingLabel;
        int choice = 0;
    };

    explicit ConflictBlocksModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void setBlocks(QVector<Row> rows);
    void setChoice(int row, int choice);
    [[nodiscard]] int choiceAt(int row) const { return m_rows.at(row).choice; }
    [[nodiscard]] int chosenCount() const;

private:
    QVector<Row> m_rows;
};

} // namespace gitapp
