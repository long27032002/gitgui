#pragma once

#include <QByteArray>
#include <QSet>
#include <QString>
#include <QVector>
#include <atomic>

namespace gitcore {

// Kết quả thô của một lần chạy `git`.
struct GitResult {
    bool started = false;    // tiến trình khởi động được
    bool timedOut = false;   // bị huỷ do quá thời gian
    bool cancelled = false;  // người dùng yêu cầu dừng tiến trình
    int exitCode = -1;
    QByteArray out;
    QString err;

    [[nodiscard]] bool ok() const { return started && !timedOut && !cancelled && exitCode == 0; }
};

enum class ChangeKind { Ordinary, Renamed, Unmerged, Untracked };

// Một mục trong `git status --porcelain=v2`.
// indexStatus/worktreeStatus dùng ký tự của git: 'M','A','D','R','C','U','.'
struct FileChange {
    QString path;
    QString origPath;  // chỉ có khi Renamed
    ChangeKind kind = ChangeKind::Ordinary;
    QChar indexStatus = QLatin1Char('.');
    QChar worktreeStatus = QLatin1Char('.');

    [[nodiscard]] bool isStaged() const {
        return kind != ChangeKind::Untracked && kind != ChangeKind::Unmerged
               && indexStatus != QLatin1Char('.');
    }
    [[nodiscard]] bool isUnstaged() const {
        return kind == ChangeKind::Untracked
               || (kind != ChangeKind::Unmerged && worktreeStatus != QLatin1Char('.'));
    }
    bool operator==(const FileChange &) const = default;
};

struct RepoStatus {
    QString branch;       // tên nhánh, "(detached)" nếu detached HEAD
    QString oid;          // commit hiện tại, rỗng nếu chưa có commit nào
    QString upstream;     // rỗng nếu chưa theo dõi nhánh từ xa
    int ahead = 0;
    int behind = 0;
    QVector<FileChange> changes;
};

struct BranchInfo {
    QString name;        // "main" hoặc "origin/feature" với nhánh remote
    bool isRemote = false;
    bool isCurrent = false;
    QString upstream;    // rỗng nếu không có
    bool operator==(const BranchInfo &) const = default;
};

struct CommitInfo {
    QString hash;
    QString shortHash;
    QString author;
    QString subject;
    QString firstParent;    // rỗng với commit gốc
    QStringList parents;    // tất cả cha theo đúng thứ tự Git
    QString refs;           // chuỗi decoration của git (tên nhánh/tag trỏ vào commit)
    qint64 timestamp = 0;   // giây kể từ epoch (author date)
    int parentCount = 0;
    bool onCurrentBranch = false;
    bool operator==(const CommitInfo &) const = default;
};

struct CommitFile {
    QChar status = QLatin1Char('M');  // M A D R C T
    QString path;
    QString origPath;                 // chỉ có khi đổi tên/sao chép
    bool operator==(const CommitFile &) const = default;
};

// Cách chuyển nhánh.
enum class SwitchKind {
    Existing,     // nhánh local đã có
    TrackRemote,  // tạo nhánh local theo dõi một nhánh remote (name = "origin/foo")
    CreateNew,    // tạo nhánh mới từ HEAD hiện tại
};

enum class MergeMode {
    MergeCommit,      // luôn tạo merge commit (--no-ff)
    Squash,           // gom thay đổi vào vùng stage, không tự commit
    FastForwardOnly,  // chỉ cho phép khi có thể fast-forward
};

// Thông tin để người dùng xác nhận trước khi force push.
struct ForcePushInfo {
    QString upstream;                 // vd "origin/main"
    QString expectedSha;              // commit của upstream mà người dùng đã thấy
    QVector<CommitInfo> lost;         // commit trên remote sẽ bị ghi đè
};

// Một file được đưa vào commit theo kiểu GitHub Desktop: cả file, hoặc chỉ một số dòng.
struct CommitEntry {
    QString path;
    QString origPath;           // đổi tên: cả hai phía cùng được stage
    bool partial = false;       // true: chỉ commit các dòng không nằm trong `excluded`
    QSet<QByteArray> excluded;  // khoá dòng bị loại (xem GitDiff::lineKeys)
};

// Thông tin về commit ở đầu nhánh hiện tại (HEAD).
struct HeadInfo {
    QString hash;
    QString subject;
    QString message;       // toàn bộ message, đã bỏ khoảng trắng thừa ở hai đầu
    int parentCount = 0;   // 0: commit gốc; >1: merge commit
    bool pushed = false;   // đã nằm trong upstream (sửa/hoàn tác sẽ cần force push)
};

struct InitResult {
    GitResult git;
    QString path;
    bool committed = false;  // đã tạo commit "Initial commit"
    QString warning;         // vd: không commit được vì chưa cấu hình user.name/user.email
};

// Thao tác nhiều bước của git đang dở dang (thường vì có conflict).
enum class OperationKind { None, Merge, Rebase, CherryPick, Revert };

} // namespace gitcore
