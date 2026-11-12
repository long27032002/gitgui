#pragma once

#include "DiffModel.h"
#include "HistoryModels.h"

#include <GitService.h>

#include <QObject>
#include <QSet>

#include <algorithm>
#include <optional>

namespace gitapp {

class Theme;

// Tab "Lịch sử": danh sách commit, file đã đổi của commit đang chọn và diff của file đó.
class HistoryController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *commits READ commits CONSTANT)
    Q_PROPERTY(QAbstractItemModel *files READ files CONSTANT)
    Q_PROPERTY(QAbstractItemModel *diff READ diff CONSTANT)
    Q_PROPERTY(QString selectedHash READ selectedHash NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedFile READ selectedFile NOTIFY selectionChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    // Đoạn commit đang chọn (hàng của danh sách, rangeStart <= rangeEnd; mới nhất ở hàng nhỏ).
    Q_PROPERTY(int rangeStart READ rangeStart NOTIFY selectionChanged)
    Q_PROPERTY(int rangeEnd READ rangeEnd NOTIFY selectionChanged)
    Q_PROPERTY(int rangeSize READ rangeSize NOTIFY selectionChanged)
    Q_PROPERTY(bool canSquashSelection READ canSquashSelection NOTIFY selectionChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool canLoadMore READ canLoadMore NOTIFY loadingChanged)

public:
    explicit HistoryController(QObject *parent = nullptr);

    [[nodiscard]] QAbstractItemModel *commits() { return &m_commits; }
    [[nodiscard]] QAbstractItemModel *files() { return &m_files; }
    [[nodiscard]] QAbstractItemModel *diff() { return &m_diff; }
    [[nodiscard]] const QString &selectedHash() const { return m_selectedHash; }
    [[nodiscard]] const QString &selectedFile() const { return m_selectedFile; }
    [[nodiscard]] const QString &message() const { return m_message; }
    [[nodiscard]] int rangeStart() const { return m_anchorRow < 0 ? -1 : std::min(m_anchorRow, m_extentRow); }
    [[nodiscard]] int rangeEnd() const { return m_anchorRow < 0 ? -1 : std::max(m_anchorRow, m_extentRow); }
    [[nodiscard]] int rangeSize() const { return m_anchorRow < 0 ? 0 : rangeEnd() - rangeStart() + 1; }
    [[nodiscard]] bool canSquashSelection() const;
    [[nodiscard]] const CommitsModel &commitsModel() const { return m_commits; }
    [[nodiscard]] bool loading() const { return m_loading; }
    [[nodiscard]] bool canLoadMore() const { return !m_allLoaded && !m_loading; }

    // Gọi khi mở/đóng repo; xoá toàn bộ dữ liệu cũ.
    void setService(std::optional<gitcore::GitService> service);
    void setPageSize(int n) { m_pageSize = n; }
    void setTheme(const Theme *theme) { m_diff.setTheme(theme); }

    Q_INVOKABLE void reload();
    Q_INVOKABLE void loadMore();
    Q_INVOKABLE void selectCommit(int row);
    Q_INVOKABLE void selectFile(int row);
    // Shift+bấm: mở rộng vùng chọn từ commit đang chọn tới `row` (diff vẫn hiển thị commit gốc).
    Q_INVOKABLE void extendSelection(int row);

signals:
    void selectionChanged();
    void messageChanged();
    void loadingChanged();
    void errorOccurred(const QString &message);

private:
    void clearAll();
    void setLoading(bool on);

    std::optional<gitcore::GitService> m_service;
    CommitsModel m_commits;
    CommitFilesModel m_files;
    DiffModel m_diff;

    int m_anchorRow = -1;
    int m_extentRow = -1;
    QString m_selectedHash;
    QString m_selectedFile;
    QString m_message;
    bool m_loading = false;
    bool m_allLoaded = false;
    int m_pageSize = 200;

    // Mỗi yêu cầu mới làm kết quả của yêu cầu cũ trở nên lỗi thời.
    quint64 m_listGen = 0;
    quint64 m_detailGen = 0;
    quint64 m_diffGen = 0;
    QSet<QString> m_currentBranchHashes;
};

} // namespace gitapp
