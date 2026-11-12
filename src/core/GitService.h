#pragma once

#include "GitDiff.h"
#include "GitTypes.h"

#include <QDateTime>
#include <QString>
#include <QStringList>

#include <QVector>
#include <atomic>

#include <optional>

namespace gitcore {

// API cấp cao cho một repo. Mọi hàm đều chặn (blocking) — gọi từ luồng worker.
class GitService {
public:
    explicit GitService(QString repoPath);

    // Thư mục gốc của repo (rev-parse --show-toplevel); nullopt nếu không phải repo git.
    [[nodiscard]] static std::optional<QString> findRoot(const QString &path);

    [[nodiscard]] const QString &repoPath() const { return m_repoPath; }

    // `git config user.name`; rỗng nếu chưa cấu hình.
    [[nodiscard]] QString userName() const;
    // Thời điểm fetch/pull gần nhất (theo FETCH_HEAD); QDateTime không hợp lệ nếu chưa có.
    [[nodiscard]] QDateTime lastFetchTime() const;

    [[nodiscard]] std::optional<RepoStatus> status(QString *error = nullptr) const;
    [[nodiscard]] GitResult stage(const QStringList &paths) const;
    [[nodiscard]] GitResult unstage(const QStringList &paths) const;
    // Diff của một file; `staged` = true so sánh index với HEAD.
    [[nodiscard]] GitResult diff(const QString &path, bool staged) const;
    // Áp patch vào index (stage một phần); `reverse` = true để bỏ các dòng đó khỏi index.
    [[nodiscard]] GitResult applyPatch(const QByteArray &patch, bool reverse) const;
    [[nodiscard]] GitResult commit(const QString &message) const;
    // Diff của một file giữa working tree và HEAD (hoặc cây rỗng nếu repo chưa có commit):
    // đúng thứ sẽ được commit nếu chọn toàn bộ. File chưa theo dõi không có diff ở đây.
    [[nodiscard]] GitResult diffForCommit(const QString &path) const;
    // Commit theo kiểu GitHub Desktop: đặt lại index về HEAD, stage đúng những file/dòng đã chọn
    // rồi commit. Mọi patch được dựng xong trước khi đụng vào index; thay đổi không được chọn
    // vẫn nằm nguyên ở working tree.
    // `amend` = true: sửa commit gần nhất (chỉ khi nó chưa push và không phải merge commit) bằng phần
    // đã chọn và message mới; với amend được phép không chọn gì để chỉ đổi message.
    [[nodiscard]] GitResult commitSelected(const QVector<CommitEntry> &entries, const QString &message,
                                           bool amend = false) const;

    // Commit ở đầu nhánh hiện tại; nullopt nếu repo chưa có commit nào.
    [[nodiscard]] std::optional<HeadInfo> headCommit() const;
    // Bỏ commit gần nhất nhưng giữ nguyên thay đổi trong working tree (`reset --mixed HEAD~1`).
    // Chỉ cho commit chưa push, không phải merge và không phải commit gốc.
    [[nodiscard]] GitResult undoLastCommit() const;

    // --- Bỏ thay đổi (discard) kèm sao lưu để hoàn tác ---
    // Khôi phục các file về HEAD (file chưa theo dõi thì bị xoá). Nội dung hiện tại được sao lưu
    // trước; `backupId` (nếu cho) nhận mã để undoDiscard().
    [[nodiscard]] GitResult discardFiles(const QVector<FileChange> &files, QString *backupId = nullptr) const;
    // Bỏ các dòng được chọn của một file bằng cách áp ngược patch lên working tree.
    [[nodiscard]] GitResult discardLines(const QString &path, const FileDiff &diff,
                                         const LineSelection &lines, QString *backupId = nullptr) const;
    [[nodiscard]] GitResult undoDiscard(const QString &backupId) const;

    // --- .gitignore ---
    // Thêm một dòng vào .gitignore ở thư mục gốc (tạo file nếu chưa có; bỏ qua nếu đã có dòng đó).
    [[nodiscard]] GitResult addIgnorePattern(const QString &pattern) const;

    // --- Tạo repository mới ---
    // Tạo thư mục `parentDir/name`, `git init -b branch`, tuỳ chọn README.md và .gitignore (tên mẫu
    // trong GitIgnoreTemplates), rồi nếu có file được tạo thì commit "Initial commit".
    [[nodiscard]] static InitResult initRepository(const QString &parentDir, const QString &name,
                                                   const QString &branch, bool withReadme,
                                                   const QString &gitignoreTemplate);

    // --- Đồng bộ với remote (cần mạng; timeout dài hơn lệnh thường) ---
    [[nodiscard]] GitResult fetch(const std::atomic_bool *cancel = nullptr) const;
    // Hợp nhất bằng merge (không rebase); không mở trình soạn thảo cho merge commit.
    [[nodiscard]] GitResult pull(const std::atomic_bool *cancel = nullptr) const;
    // `publish` = true khi nhánh chưa có upstream: đẩy lên remote "origin" (hoặc remote đầu
    // tiên) và đặt upstream.
    [[nodiscard]] GitResult push(bool publish, const std::atomic_bool *cancel = nullptr) const;
    [[nodiscard]] GitResult deleteRemoteBranch(const QString &remote, const QString &branch,
                                               const std::atomic_bool *cancel = nullptr) const;
    [[nodiscard]] QStringList remotes() const;

    // --- Nhánh ---
    [[nodiscard]] std::optional<QVector<BranchInfo>> branches(QString *error = nullptr) const;
    // Chuyển/tạo nhánh. Nếu `stashBranch` không rỗng, thay đổi chưa commit (kể cả file mới)
    // được cất tạm trước, gắn thẻ theo tên nhánh đó; nếu chuyển thất bại thì tự khôi phục.
    [[nodiscard]] GitResult switchTo(const QString &name, SwitchKind kind,
                                     const QString &stashBranch = {}, const QString &startPoint = {}) const;
    // Xoá nhánh local; `force` = true bỏ qua kiểm tra "đã merge".
    [[nodiscard]] GitResult deleteBranch(const QString &name, bool force) const;
    [[nodiscard]] GitResult renameBranch(const QString &oldName, const QString &newName) const;

    // --- Thay đổi đã cất tạm bởi app (thẻ "gitgui:<nhánh>") ---
    [[nodiscard]] bool hasSavedChanges(const QString &branch) const;
    [[nodiscard]] GitResult restoreSavedChanges(const QString &branch) const;
    [[nodiscard]] GitResult discardSavedChanges(const QString &branch) const;

    // --- Conflict và thao tác dở dang (merge/rebase/cherry-pick/revert) ---
    [[nodiscard]] OperationKind operationInProgress() const;
    // Merge: `git commit --no-edit`; các thao tác khác: `--continue` không mở trình soạn thảo.
    [[nodiscard]] GitResult continueOperation(OperationKind op) const;
    [[nodiscard]] GitResult abortOperation(OperationKind op) const;
    // Ghi nội dung đã giải quyết vào working tree rồi `git add` (đánh dấu đã giải quyết).
    [[nodiscard]] GitResult resolveWithContent(const QString &path, const QByteArray &content) const;
    // Lấy nguyên một bên của git (`ours`/`theirs`). Nếu bên đó đã xoá file thì xoá file.
    [[nodiscard]] GitResult resolveWithSide(const QString &path, bool useOurs) const;

    // --- Merge / rebase / cherry-pick / revert / squash ---
    // Số commit trong `tip` mà `base` chưa có (rev-list base..tip); -1 nếu lỗi.
    [[nodiscard]] int commitCount(const QString &base, const QString &tip) const;
    // Số commit sẽ bị viết lại mà đã được push, nếu rebase HEAD lên `base`; 0 nếu chưa có upstream.
    [[nodiscard]] int rewrittenPushedCount(const QString &base) const;
    // `commit` đã nằm trong upstream chưa (viết lại nó sẽ cần force push)?
    [[nodiscard]] bool isPushed(const QString &commit) const;
    [[nodiscard]] GitResult mergeBranch(const QString &name, MergeMode mode) const;
    [[nodiscard]] GitResult rebaseOnto(const QString &base) const;
    // Với merge commit dùng `-m 1` (coi cha đầu tiên là dòng chính).
    [[nodiscard]] GitResult cherryPick(const QString &hash, bool isMerge) const;
    [[nodiscard]] GitResult cherryPick(const QStringList &hashes, bool isMerge) const;
    [[nodiscard]] GitResult revert(const QString &hash, bool isMerge) const;
    // Gom các commit liên tiếp từ `oldest` đến `newest` (đều thuộc nhánh hiện tại, không có merge
    // commit trong đoạn từ oldest tới HEAD) thành một commit với `message`. Cây làm việc cuối
    // cùng giữ nguyên; các commit phía trên được áp lại bằng rebase.
    [[nodiscard]] GitResult squashCommits(const QString &newest, const QString &oldest,
                                          const QString &message) const;
    [[nodiscard]] std::optional<ForcePushInfo> forcePushInfo(QString *error = nullptr) const;
    // `--force-with-lease` gắn với đúng commit upstream mà người dùng đã xác nhận
    // (không dựa vào remote-tracking ref vì fetch nền có thể đã làm nó mới hơn).
    [[nodiscard]] GitResult forcePush(const QString &expectedSha) const;

    // --- Lịch sử ---
    // Trả về nullopt khi lỗi; repo chưa có commit nào cho danh sách rỗng.
    [[nodiscard]] std::optional<QVector<CommitInfo>> log(int skip, int count,
                                                         QString *error = nullptr) const;
    [[nodiscard]] std::optional<QVector<CommitInfo>> logAll(int skip, int count,
                                                            QString *error = nullptr) const;
    [[nodiscard]] QSet<QString> currentBranchHashes() const;
    [[nodiscard]] QString commitMessage(const QString &hash) const;
    [[nodiscard]] std::optional<QVector<CommitFile>> commitFiles(const QString &hash) const;
    // Diff của một file trong commit (so với cha đầu tiên; commit gốc so với cây rỗng).
    // `origPath` (nếu có) giúp git nhận ra đổi tên.
    [[nodiscard]] GitResult commitFileDiff(const QString &hash, const QString &path,
                                           const QString &origPath = {}) const;

private:
    [[nodiscard]] GitResult commitInternal(const QString &message, bool amend) const;
    [[nodiscard]] GitResult stashPush(const QString &branch, bool *created) const;
    [[nodiscard]] std::optional<QString> findSavedStash(const QString &branch) const;
    [[nodiscard]] GitResult git(const QStringList &args, const QByteArray &in = {},
                                int timeoutMs = 30'000) const;

    QString m_repoPath;
};

} // namespace gitcore
