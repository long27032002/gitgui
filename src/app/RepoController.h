#pragma once

#include "ChangesModel.h"
#include "BranchesModel.h"
#include "ConflictController.h"
#include "DiffModel.h"
#include "HistoryController.h"
#include "RecentRepos.h"

#include <GitService.h>

#include <QDateTime>
#include <QObject>
#include <QTimer>
#include <QVariantMap>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <QUrl>

#include <optional>
#include <atomic>
#include <memory>

namespace gitapp {

class Theme;

// Cầu nối giữa lõi git (blocking) và QML. Mọi lệnh git chạy ở luồng worker;
// luồng giao diện chỉ nhận kết quả qua signal.
class RepoController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *changes READ changes CONSTANT)
    Q_PROPERTY(bool hasRepo READ hasRepo NOTIFY repoPathChanged)
    Q_PROPERTY(QString repoPath READ repoPath NOTIFY repoPathChanged)
    Q_PROPERTY(QString repoName READ repoName NOTIFY repoPathChanged)
    Q_PROPERTY(QString branch READ branch NOTIFY statusChanged)
    Q_PROPERTY(QString upstream READ upstream NOTIFY statusChanged)
    Q_PROPERTY(int ahead READ ahead NOTIFY statusChanged)
    Q_PROPERTY(int behind READ behind NOTIFY statusChanged)
    // Số file đang được chọn đưa vào commit tiếp theo (mặc định mọi file được chọn).
    Q_PROPERTY(int includedCount READ includedCount NOTIFY statusChanged)
    // Commit gần nhất (HEAD) có thể hoàn tác / sửa không (chỉ khi chưa push, không phải merge...).
    Q_PROPERTY(bool canUndoCommit READ canUndoCommit NOTIFY statusChanged)
    Q_PROPERTY(bool canAmend READ canAmend NOTIFY statusChanged)
    Q_PROPERTY(QString lastCommitSubject READ lastCommitSubject NOTIFY statusChanged)
    // Có bản sao lưu của lần "bỏ thay đổi" gần nhất để hoàn tác.
    Q_PROPERTY(bool canUndoDiscard READ canUndoDiscard NOTIFY discardStateChanged)
    Q_PROPERTY(QString lastDiscardText READ lastDiscardText NOTIFY discardStateChanged)
    Q_PROPERTY(bool creating READ creating NOTIFY creatingChanged)
    Q_PROPERTY(QStringList gitignoreTemplates READ gitignoreTemplates CONSTANT)
    Q_PROPERTY(QString homeDir READ homeDir CONSTANT)
    Q_PROPERTY(bool hasUpstream READ hasUpstream NOTIFY statusChanged)
    Q_PROPERTY(QString userName READ userName NOTIFY statusChanged)
    // "Fetch lần cuối 8 phút trước"; tự cập nhật theo thời gian.
    Q_PROPERTY(QString lastFetchedText READ lastFetchedText NOTIFY lastFetchedTextChanged)
    Q_PROPERTY(QAbstractItemModel *recentRepos READ recentRepos CONSTANT)
    // Trạng thái ô chọn tất cả: 0 không file nào được chọn, 1 một phần, 2 tất cả (không tính file xung đột).
    Q_PROPERTY(int selectAllState READ selectAllState NOTIFY statusChanged)
    Q_PROPERTY(int changeCount READ changeCount NOTIFY statusChanged)
    // Thao tác dở dang: "", "merge", "rebase", "cherry-pick", "revert".
    Q_PROPERTY(QString operation READ operation NOTIFY statusChanged)
    Q_PROPERTY(int conflictCount READ conflictCount NOTIFY statusChanged)
    Q_PROPERTY(bool canContinueOperation READ canContinueOperation NOTIFY statusChanged)
    Q_PROPERTY(QObject *conflicts READ conflicts CONSTANT)
    // Nhánh local đã lệch khỏi upstream (cả ahead và behind) — thường do rebase/squash commit đã push.
    Q_PROPERTY(bool diverged READ diverged NOTIFY statusChanged)
    // Kết quả kiểm tra trước khi merge/rebase: {name, loaded, incoming, local, pushed}
    Q_PROPERTY(QVariantMap branchCompare READ branchCompare NOTIFY branchCompareChanged)
    // Kế hoạch squash: {ok, error, count, newest, oldest, message, pushed, dirty}
    Q_PROPERTY(QVariantMap squashPlan READ squashPlan NOTIFY squashPlanChanged)
    // Thông tin xác nhận force push: {loaded, error, upstream, expectedSha, count, commits}
    Q_PROPERTY(QVariantMap forcePushInfo READ forcePushInfo NOTIFY forcePushInfoChanged)
    Q_PROPERTY(QAbstractItemModel *branches READ branches CONSTANT)
    Q_PROPERTY(QObject *history READ history CONSTANT)
    // Có thay đổi đã được cất tạm khi rời nhánh hiện tại (có thể khôi phục).
    Q_PROPERTY(bool hasSavedChanges READ hasSavedChanges NOTIFY statusChanged)
    // Tên nhánh đang chờ người dùng xác nhận xoá bắt buộc (nhánh chưa merge); rỗng nếu không có.
    Q_PROPERTY(QString pendingForceDelete READ pendingForceDelete NOTIFY pendingForceDeleteChanged)
    Q_PROPERTY(bool canSync READ canSync NOTIFY statusChanged)  // false khi detached HEAD
    Q_PROPERTY(QString activity READ activity NOTIFY activityChanged)  // vd "Đang pull…"
    Q_PROPERTY(QString notice READ notice NOTIFY noticeChanged)        // thông báo thành công, tự tắt
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool canCancel READ canCancel NOTIFY canCancelChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(QString selectedPath READ selectedPath NOTIFY selectedPathChanged)
    // Ký hiệu trạng thái (M A D R U ?) của file đang chọn; rỗng nếu chưa chọn.
    Q_PROPERTY(QString selectedStatus READ selectedStatus NOTIFY diffContextChanged)
    Q_PROPERTY(QAbstractItemModel *diff READ diff CONSTANT)
    Q_PROPERTY(bool diffSelectable READ diffSelectable NOTIFY selectedLineCountChanged)

public:
    explicit RepoController(QObject *parent = nullptr);

    [[nodiscard]] QAbstractItemModel *changes() { return &m_changes; }
    [[nodiscard]] bool hasRepo() const { return m_service.has_value(); }
    [[nodiscard]] QString repoPath() const;
    [[nodiscard]] QString repoName() const;
    [[nodiscard]] const QString &branch() const { return m_branch; }
    [[nodiscard]] const QString &upstream() const { return m_upstream; }
    [[nodiscard]] int ahead() const { return m_ahead; }
    [[nodiscard]] int behind() const { return m_behind; }
    [[nodiscard]] int includedCount() const;
    [[nodiscard]] bool canUndoCommit() const { return m_head && m_head->parentCount == 1 && !m_head->pushed; }
    [[nodiscard]] bool canAmend() const { return m_head && m_head->parentCount <= 1 && !m_head->pushed; }
    [[nodiscard]] QString lastCommitSubject() const { return m_head ? m_head->subject : QString(); }
    [[nodiscard]] bool canUndoDiscard() const { return !m_lastDiscardId.isEmpty(); }
    [[nodiscard]] const QString &lastDiscardText() const { return m_lastDiscardText; }
    [[nodiscard]] bool creating() const { return m_creating; }
    [[nodiscard]] QStringList gitignoreTemplates() const;
    [[nodiscard]] QString homeDir() const;
    // Tách message thành {subject, body, coAuthors}; các dòng "Co-authored-by:" cuối message được
    // gom vào coAuthors (cách nhau bằng ", ") để điền lại vào ô đồng tác giả.
    [[nodiscard]] static QVariantMap splitMessage(const QString &message);
    // Các phần của message commit gần nhất (rỗng nếu chưa có commit).
    Q_INVOKABLE QVariantMap lastCommitParts() const;
    [[nodiscard]] bool hasUpstream() const { return !m_upstream.isEmpty(); }
    [[nodiscard]] const QString &userName() const { return m_userName; }
    [[nodiscard]] QString lastFetchedText() const { return relativeTimeText(m_lastFetch, QDateTime::currentDateTime()); }
    [[nodiscard]] QAbstractItemModel *recentRepos() { return &m_recent; }
    [[nodiscard]] RecentRepos *recent() { return &m_recent; }
    [[nodiscard]] int selectAllState() const;
    // Gắn theme để diff tô màu cú pháp theo bảng màu hiện tại.
    void setTheme(const Theme *theme);
    // "Vừa fetch xong", "Fetch lần cuối 8 phút trước", "Chưa từng fetch"...
    [[nodiscard]] static QString relativeTimeText(const QDateTime &then, const QDateTime &now);
    [[nodiscard]] int changeCount() const { return m_changes.rowCount(); }
    [[nodiscard]] QString operation() const;
    [[nodiscard]] int conflictCount() const { return m_conflictCount; }
    [[nodiscard]] bool canContinueOperation() const
    {
        return m_operation != gitcore::OperationKind::None && m_conflictCount == 0;
    }
    [[nodiscard]] bool diverged() const { return hasUpstream() && m_ahead > 0 && m_behind > 0; }
    [[nodiscard]] const QVariantMap &branchCompare() const { return m_branchCompare; }
    [[nodiscard]] const QVariantMap &squashPlan() const { return m_squashPlan; }
    [[nodiscard]] const QVariantMap &forcePushInfo() const { return m_forcePushInfo; }
    [[nodiscard]] QObject *conflicts() { return &m_conflicts; }
    [[nodiscard]] ConflictController *conflictController() { return &m_conflicts; }
    [[nodiscard]] QAbstractItemModel *branches() { return &m_branches; }
    [[nodiscard]] HistoryController *historyController() { return &m_history; }
    [[nodiscard]] QObject *history() { return &m_history; }
    [[nodiscard]] bool hasSavedChanges() const { return m_hasSaved; }
    [[nodiscard]] const QString &pendingForceDelete() const { return m_pendingForceDelete; }
    [[nodiscard]] bool canSync() const { return m_service && !m_branch.isEmpty() && m_branch != QLatin1String("(detached)"); }
    [[nodiscard]] const QString &activity() const { return m_activity; }
    [[nodiscard]] const QString &notice() const { return m_notice; }
    [[nodiscard]] bool busy() const { return m_pending > 0; }
    [[nodiscard]] bool canCancel() const { return static_cast<bool>(m_activeCancel); }
    [[nodiscard]] const QString &errorMessage() const { return m_error; }
    [[nodiscard]] const QString &selectedPath() const { return m_selectedPath; }
    [[nodiscard]] QString selectedStatus() const;
    [[nodiscard]] QAbstractItemModel *diff() { return &m_diff; }
    [[nodiscard]] bool diffSelectable() const { return m_diff.selectable(); }

    Q_INVOKABLE void openRepo(const QString &path);
    Q_INVOKABLE void openRepoUrl(const QUrl &url);
    // Đường dẫn cục bộ của một URL file:// (kết quả của hộp thoại chọn thư mục).
    Q_INVOKABLE QString localPath(const QUrl &url) const { return url.toLocalFile(); }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void toggleChecked(int row);
    // Chọn hoặc bỏ chọn mọi file (trừ file đang xung đột), như ô "N changed files" của GitHub Desktop.
    Q_INVOKABLE void toggleAll();
    Q_INVOKABLE void removeRecent(const QString &path);
    Q_INVOKABLE void revealRepoFolder();
    Q_INVOKABLE void selectRow(int row);
    Q_INVOKABLE void toggleDiffRow(int row);
    Q_INVOKABLE void fetch();
    Q_INVOKABLE void pull();
    Q_INVOKABLE void push();
    Q_INVOKABLE void cancelNetworkOperation();
    Q_INVOKABLE void deleteRemoteBranch(const QString &remoteBranch);
    // `stashChanges` = true: cất tạm thay đổi chưa commit ở nhánh cũ (khôi phục được khi quay lại);
    // false: mang thay đổi theo sang nhánh mới. `name` có thể là nhánh remote (vd "origin/foo").
    Q_INVOKABLE void switchBranch(const QString &name, bool stashChanges);
    Q_INVOKABLE void createBranch(const QString &name, bool stashChanges);
    Q_INVOKABLE void createBranchFromCommit(const QString &name, const QString &hash, bool stashChanges);
    Q_INVOKABLE void renameBranch(const QString &oldName, const QString &newName);
    // Xoá nhánh local; nếu chưa merge sẽ đặt pendingForceDelete để giao diện hỏi lại.
    Q_INVOKABLE void deleteBranch(const QString &name);
    Q_INVOKABLE void confirmForceDelete();
    Q_INVOKABLE void cancelForceDelete();
    Q_INVOKABLE void restoreSavedChanges();
    Q_INVOKABLE void discardSavedChanges();
    // Hoàn tất (sau khi đã giải quyết hết conflict) hoặc huỷ thao tác merge/rebase/... đang dở.
    // --- Merge / rebase (nhánh `name` có thể là nhánh remote) ---
    Q_INVOKABLE void prepareBranchAction(const QString &name);  // tính trước số commit để hiển thị
    // mode: 0 merge commit, 1 squash (chỉ gom vào vùng stage), 2 chỉ fast-forward
    Q_INVOKABLE void mergeBranch(const QString &name, int mode);
    Q_INVOKABLE void rebaseOnto(const QString &name);
    // --- Từ tab Lịch sử ---
    Q_INVOKABLE void cherryPick(const QString &hash);
    Q_INVOKABLE void cherryPickSelected();
    Q_INVOKABLE void revertCommit(const QString &hash);
    Q_INVOKABLE void prepareSquash();                     // dựa trên đoạn commit đang chọn
    Q_INVOKABLE void squashSelected(const QString &message);
    // --- Force push (--force-with-lease) ---
    Q_INVOKABLE void prepareForcePush();
    Q_INVOKABLE void confirmForcePush();
    Q_INVOKABLE void cancelForcePush();
    Q_INVOKABLE void continueOperation();
    Q_INVOKABLE void abortOperation();
    // `coAuthors`: các mục "Tên <email>" cách nhau bởi dấu phẩy hoặc xuống dòng; được thêm vào message
    // dưới dạng dòng "Co-authored-by:".
    // `amend` = true: sửa commit gần nhất (cần canAmend) bằng phần đã chọn; có thể không chọn gì.
    Q_INVOKABLE void commit(const QString &summary, const QString &description,
                            const QString &coAuthors = {}, bool amend = false);
    // Hoàn tác commit gần nhất (giữ thay đổi); phát commitUndone để giao diện điền lại message.
    Q_INVOKABLE void undoLastCommit();

    // --- Bỏ thay đổi (luôn sao lưu để hoàn tác) ---
    Q_INVOKABLE void discardFiles(const QStringList &paths);
    Q_INVOKABLE void discardAll();
    // Bỏ dòng (hàng +/−) hoặc cả khối (hàng @@) trong diff của file đang xem.
    Q_INVOKABLE void discardDiffRow(int row);
    Q_INVOKABLE void undoDiscard();

    // --- .gitignore (chỉ nên dùng cho file chưa theo dõi) ---
    Q_INVOKABLE void ignoreFile(const QString &path);
    Q_INVOKABLE void ignoreExtension(const QString &path);
    Q_INVOKABLE void ignoreFolder(const QString &path);
    Q_INVOKABLE void openGitignore();

    // Tạo repository mới rồi mở nó. `gitignoreTemplate` rỗng = không tạo .gitignore.
    Q_INVOKABLE void createRepository(const QString &parentDir, const QString &name, const QString &branch,
                                      bool withReadme, const QString &gitignoreTemplate);
    Q_INVOKABLE void clearError();

    // Khoảng thời gian giữa hai lần tự fetch nền (mặc định 5 phút). Dùng cho test.
    void setAutoFetchIntervalMs(int ms);

signals:
    void repoPathChanged();
    void statusChanged();
    void busyChanged();
    void errorMessageChanged();
    void selectedPathChanged();
    void diffContextChanged();
    void selectedLineCountChanged();
    void activityChanged();
    void canCancelChanged();
    void lastFetchedTextChanged();
    void pendingForceDeleteChanged();
    void branchCompareChanged();
    void squashPlanChanged();
    void forcePushInfoChanged();
    void squashMergeStaged();  // merge --squash xong, thay đổi đang nằm ở vùng stage chờ commit
    void noticeChanged();
    void committed();
    // Đã hoàn tác commit: giao diện điền lại các ô tóm tắt/mô tả/đồng tác giả.
    void commitUndone(const QString &subject, const QString &body, const QString &coAuthors);
    void discardStateChanged();
    void creatingChanged();  // commit thành công — QML dùng để xoá ô nhập

private:
    // Lựa chọn file/dòng đưa vào commit, kiểu GitHub Desktop. Chỉ lưu các file khác mặc định (chọn hết).
    struct FileSelection {
        enum Mode { All, None, Partial } mode = All;
        QSet<QByteArray> excluded;  // khoá dòng bị loại khi Partial
    };
    void beginTask();
    void endTask();
    void setError(const QString &message);
    struct RefreshData;
    void applyStatus(const RefreshData &data);
    void runSwitch(const QString &target, gitcore::SwitchKind kind, bool stashChanges);
    void loadDiff(const gitcore::FileChange &change);
    // Ghi lại lựa chọn dòng hiện trong DiffModel vào kho lựa chọn của file đang xem.
    void syncSelectionFromDiff();
    void setFileSelection(const QString &path, const FileSelection &sel);
    [[nodiscard]] int selectionStateOf(const QString &path) const;  // 0 / 1 / 2
    void setSelectedPath(const QString &path);
    [[nodiscard]] std::optional<gitcore::FileChange> currentChange() const;
    // Chạy thao tác ghi (stage/unstage/commit), xong thì tự refresh.
    void mutate(std::function<gitcore::GitResult()> work, const QString &activity = {},
                std::function<void(const gitcore::GitResult &)> onSuccess = {},
                std::shared_ptr<std::atomic_bool> cancel = {});
    void setNotice(const QString &text);
    void markFetched();
    void backgroundFetch();

    std::optional<gitcore::GitService> m_service;
    ChangesModel m_changes;

    QString m_branch;
    QString m_upstream;
    int m_ahead = 0;
    int m_behind = 0;

    int m_pending = 0;
    QString m_activity;
    std::shared_ptr<std::atomic_bool> m_activeCancel;
    QString m_notice;
    QTimer m_noticeTimer;
    QTimer m_autoFetchTimer;
    bool m_bgFetching = false;
    QString m_error;
    QString m_selectedPath;
    DiffModel m_diff;
    BranchesModel m_branches;
    HistoryController m_history;
    ConflictController m_conflicts;
    gitcore::OperationKind m_operation = gitcore::OperationKind::None;
    int m_conflictCount = 0;
    QString m_userName;
    QDateTime m_lastFetch;
    QString m_lastFetchedTextShown;
    QTimer m_lastFetchedTimer;
    RecentRepos m_recent;
    std::optional<gitcore::HeadInfo> m_head;
    QString m_lastDiscardId;
    QString m_pendingWarning;  // cảnh báo hiện sau khi repo vừa tạo được mở xong
    QString m_lastDiscardText;
    bool m_creating = false;
    QVariantMap m_branchCompare;
    QVariantMap m_squashPlan;
    QVariantMap m_forcePushInfo;
    quint64 m_prepareGen = 0;  // bỏ kết quả kiểm tra của lần mở hộp thoại trước
    QString m_lastOid;           // để biết khi nào HEAD đổi và cần nạp lại lịch sử
    bool m_hasSaved = false;
    QString m_pendingForceDelete;
    QHash<QString, FileSelection> m_selection;
    bool m_applyingDiffSelection = false;  // đang đặt lại lựa chọn của DiffModel: bỏ qua tín hiệu của nó
    quint64 m_diffRequest = 0;  // bỏ qua kết quả diff của yêu cầu cũ
};

} // namespace gitapp
