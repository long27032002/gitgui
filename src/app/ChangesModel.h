#pragma once

#include <GitTypes.h>

#include <QAbstractListModel>

namespace gitapp {

// Danh sách file thay đổi, hiển thị dạng một danh sách có checkbox (giống GitHub Desktop):
// checkbox bật = file (hoặc các dòng của nó) sẽ nằm trong commit tiếp theo. Mặc định mọi file được chọn.
class ChangesModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Role {
        PathRole = Qt::UserRole + 1,
        OrigPathRole,
        StatusRole,      // ký tự trạng thái: M A D R U ?
        CheckStateRole,  // 0 = không chọn, 1 = chọn một phần, 2 = chọn hết (khớp Qt::CheckState)
    };

    explicit ChangesModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    // Nếu danh sách đường dẫn không đổi thì chỉ phát dataChanged (giữ nguyên vị trí cuộn).
    void setChanges(QVector<gitcore::FileChange> items);
    [[nodiscard]] const gitcore::FileChange &at(int row) const { return m_items.at(row); }
    [[nodiscard]] int rowOfPath(const QString &path) const;

    // Trạng thái chọn của một file (mặc định 2 = chọn hết); do RepoController cập nhật.
    void setCheckState(const QString &path, int state);
    [[nodiscard]] int checkStateOfPath(const QString &path) const { return m_check.value(path, 2); }
    [[nodiscard]] static QString statusLabel(const gitcore::FileChange &fc);

private:
    QVector<gitcore::FileChange> m_items;
    QHash<QString, int> m_check;  // chỉ lưu các trạng thái khác mặc định
};

} // namespace gitapp
