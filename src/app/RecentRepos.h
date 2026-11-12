#pragma once

#include <QAbstractListModel>
#include <QStringList>

namespace gitapp {

// Các repository đã mở gần đây (mới nhất ở đầu), có thể lưu bền vững.
class RecentRepos : public QAbstractListModel {
    Q_OBJECT
public:
    enum Role { PathRole = Qt::UserRole + 1, NameRole, ParentRole };
    static constexpr int kMax = 20;

    explicit RecentRepos(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    // Bật lưu bền vững và nạp danh sách đã lưu. `iniFile` rỗng = nơi lưu mặc định của ứng dụng.
    void enablePersistence(const QString &iniFile = {});
    // Đưa `path` lên đầu danh sách (thêm nếu chưa có).
    void touch(const QString &path);
    void remove(const QString &path);
    [[nodiscard]] QStringList paths() const { return m_paths; }

private:
    void save() const;

    QStringList m_paths;
    bool m_persistent = false;
    QString m_iniFile;
};

} // namespace gitapp
