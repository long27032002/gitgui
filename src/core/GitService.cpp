#include "GitService.h"

#include "GitDiff.h"
#include "GitParser.h"
#include "GitIgnoreTemplates.h"
#include "GitRunner.h"

#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <algorithm>
#include <QFileDevice>
#include <QFileInfo>

namespace gitcore {

GitService::GitService(QString repoPath) : m_repoPath(std::move(repoPath)) {}

QString GitService::userName() const
{
    const GitResult r = git({QStringLiteral("config"), QStringLiteral("user.name")});
    return r.ok() ? QString::fromUtf8(r.out).trimmed() : QString();
}

QDateTime GitService::lastFetchTime() const
{
    const GitResult r = git({QStringLiteral("rev-parse"), QStringLiteral("--absolute-git-dir")});
    if (!r.ok())
        return {};
    const QFileInfo info(QString::fromUtf8(r.out).trimmed() + QStringLiteral("/FETCH_HEAD"));
    return info.exists() ? info.lastModified() : QDateTime();
}

std::optional<QString> GitService::findRoot(const QString &path)
{
    const GitResult r = GitRunner::run(path, {QStringLiteral("rev-parse"),
                                              QStringLiteral("--show-toplevel")});
    if (!r.ok())
        return std::nullopt;
    return QString::fromUtf8(r.out).trimmed();
}

namespace {
constexpr int kNetworkTimeoutMs = 120'000;

GitResult success()
{
    GitResult r;
    r.started = true;
    r.exitCode = 0;
    return r;
}

GitResult failure(const QString &message)
{
    GitResult r;
    r.started = true;
    r.exitCode = 1;
    r.err = message;
    return r;
}
} // namespace

GitResult GitService::git(const QStringList &args, const QByteArray &in, int timeoutMs) const
{
    return GitRunner::run(m_repoPath, args, in, timeoutMs);
}

std::optional<RepoStatus> GitService::status(QString *error) const
{
    const GitResult r = git({QStringLiteral("status"), QStringLiteral("--porcelain=v2"),
                             QStringLiteral("--branch"), QStringLiteral("-z"),
                             QStringLiteral("--untracked-files=all")});
    if (!r.ok()) {
        if (error)
            *error = r.err;
        return std::nullopt;
    }
    return GitParser::parseStatusV2(r.out);
}

namespace {
// Danh sách đường dẫn đưa qua stdin (phân cách NUL) để không vượt giới hạn độ dài dòng lệnh.
QByteArray pathspecInput(const QStringList &paths)
{
    QByteArray out;
    for (const QString &p : paths)
        out += p.toUtf8() + '\0';
    return out;
}
} // namespace

GitResult GitService::stage(const QStringList &paths) const
{
    if (paths.isEmpty())
        return failure(QStringLiteral("Không có file nào để stage."));
    // `add -A` xử lý cả file bị xoá.
    return git({QStringLiteral("add"), QStringLiteral("-A"), QStringLiteral("--pathspec-from-file=-"),
                QStringLiteral("--pathspec-file-nul")},
               pathspecInput(paths));
}

GitResult GitService::unstage(const QStringList &paths) const
{
    if (paths.isEmpty())
        return failure(QStringLiteral("Không có file nào để unstage."));
    // `reset` hoạt động cả khi repo chưa có commit nào (restore --staged thì không).
    return git({QStringLiteral("reset"), QStringLiteral("-q"), QStringLiteral("--pathspec-from-file=-"),
                QStringLiteral("--pathspec-file-nul")},
               pathspecInput(paths));
}

GitResult GitService::diff(const QString &path, bool staged) const
{
    QStringList args{QStringLiteral("diff"), QStringLiteral("--no-color")};
    if (staged)
        args << QStringLiteral("--cached");
    args << QStringLiteral("--") << path;
    return git(args);
}

GitResult GitService::applyPatch(const QByteArray &patch, bool reverse) const
{
    QStringList args{QStringLiteral("apply"), QStringLiteral("--cached"), QStringLiteral("--recount")};
    if (reverse)
        args << QStringLiteral("--reverse");
    args << QStringLiteral("-");
    return git(args, patch);
}

namespace {
constexpr const char *kEmptyTree = "4b825dc642cb6eb9a060e54bf8d69288fbee4904";
} // namespace

GitResult GitService::diffForCommit(const QString &path) const
{
    const bool hasHead = git({QStringLiteral("rev-parse"), QStringLiteral("--verify"), QStringLiteral("-q"),
                              QStringLiteral("HEAD")}).ok();
    return git({QStringLiteral("diff"), QStringLiteral("--no-color"),
                hasHead ? QStringLiteral("HEAD") : QString::fromLatin1(kEmptyTree), QStringLiteral("--"), path});
}

GitResult GitService::commitSelected(const QVector<CommitEntry> &entries, const QString &message,
                                     bool amend) const
{
    if (amend) {
        const auto head = headCommit();
        if (!head)
            return failure(QStringLiteral("Chưa có commit nào để sửa."));
        if (head->parentCount > 1)
            return failure(QStringLiteral("Không thể sửa một merge commit."));
        if (head->pushed)
            return failure(QStringLiteral("Commit gần nhất đã được push nên không sửa được ở đây."));
    }
    if (entries.isEmpty() && !amend)
        return failure(QStringLiteral("Chưa chọn thay đổi nào để commit."));

    // 1. Dựng mọi thứ cần thiết trước khi đụng vào index.
    QStringList whole;
    struct PartialPatch {
        QString path;
        QByteArray patch;
    };
    QVector<PartialPatch> patches;

    for (const CommitEntry &e : entries) {
        if (!e.partial) {
            whole << e.path;
            if (!e.origPath.isEmpty())
                whole << e.origPath;
            continue;
        }
        FileDiff fd;
        bool synthesized = false;
        const GitResult d = diffForCommit(e.path);
        if (d.ok())
            fd = GitDiff::parse(d.out);
        if (d.ok() && d.out.trimmed().isEmpty()) {  // file chưa theo dõi: diff tổng hợp từ nội dung
            QFile f(m_repoPath + QLatin1Char('/') + e.path);
            if (f.open(QIODevice::ReadOnly)) {
                fd = GitDiff::forNewFile(e.path, f.readAll(),
                                         QFileInfo(f).permissions().testFlag(QFileDevice::ExeOwner));
                synthesized = true;
            }
        }
        if (!fd.supported || fd.hunks.isEmpty()) {  // nhị phân/chỉ đổi quyền...: không chọn dòng được
            whole << e.path;
            continue;
        }
        const LineSelection included = GitDiff::includedLines(fd, e.excluded);
        if (included.isEmpty())
            continue;  // mọi dòng đã bị loại
        if (included.size() == GitDiff::changeLineCount(fd)) {
            whole << e.path;  // thực chất là chọn đủ: `git add` giữ nguyên quyền/chế độ file
            continue;
        }
        bool newFile = synthesized;
        for (const QByteArray &h : fd.headerLines)
            newFile = newFile || h.startsWith("new file mode");
        patches.append({e.path, GitDiff::buildPatch(fd, included, newFile ? PatchMode::StageNew : PatchMode::Stage)});
    }
    if (whole.isEmpty() && patches.isEmpty() && !amend)
        return failure(QStringLiteral("Chưa chọn thay đổi nào để commit."));

    // 2. Index về đúng HEAD, rồi stage đúng phần đã chọn.
    GitResult r = git({QStringLiteral("reset"), QStringLiteral("-q")});
    if (!r.ok())
        return r;
    if (!whole.isEmpty()) {
        r = stage(whole);
        if (!r.ok())
            return r;
    }
    for (const PartialPatch &p : std::as_const(patches)) {
        r = applyPatch(p.patch, false);
        if (!r.ok()) {
            r.err = QStringLiteral("Không stage được các dòng đã chọn của %1:\n%2").arg(p.path, r.err);
            return r;
        }
    }
    return commitInternal(message, amend);
}

GitResult GitService::commit(const QString &message) const
{
    return commitInternal(message, false);
}

GitResult GitService::commitInternal(const QString &message, bool amend) const
{
    // Message qua stdin: không bị giới hạn độ dài/ký tự đặc biệt của dòng lệnh.
    QStringList args{QStringLiteral("commit")};
    if (amend)
        args << QStringLiteral("--amend");
    args << QStringLiteral("-F") << QStringLiteral("-");
    return git(args, message.toUtf8());
}

std::optional<HeadInfo> GitService::headCommit() const
{
    const GitResult r = git({QStringLiteral("log"), QStringLiteral("-1"),
                             QStringLiteral("--format=%H%x1f%P%x1f%s%x1f%B")});
    if (!r.ok())
        return std::nullopt;  // repo chưa có commit nào
    const QStringList f = QString::fromUtf8(r.out).split(QChar(0x1f));
    if (f.size() < 4)
        return std::nullopt;
    HeadInfo h;
    h.hash = f[0].trimmed();
    h.parentCount = static_cast<int>(f[1].split(QLatin1Char(' '), Qt::SkipEmptyParts).size());
    h.subject = f[2];
    h.message = f.mid(3).join(QChar(0x1f)).trimmed();
    h.pushed = isPushed(h.hash);
    return h;
}

GitResult GitService::undoLastCommit() const
{
    const auto head = headCommit();
    if (!head)
        return failure(QStringLiteral("Chưa có commit nào để hoàn tác."));
    if (head->parentCount == 0)
        return failure(QStringLiteral("Không thể hoàn tác commit gốc của repo."));
    if (head->parentCount > 1)
        return failure(QStringLiteral("Không thể hoàn tác một merge commit."));
    if (head->pushed)
        return failure(QStringLiteral("Commit này đã được push nên không thể hoàn tác ở đây."));
    return git({QStringLiteral("reset"), QStringLiteral("-q"), QStringLiteral("HEAD~1")});
}

GitResult GitService::fetch(const std::atomic_bool *cancel) const
{
    return GitRunner::run(m_repoPath, {QStringLiteral("fetch"), QStringLiteral("--prune")}, {}, kNetworkTimeoutMs, cancel);
}

GitResult GitService::pull(const std::atomic_bool *cancel) const
{
	return GitRunner::run(m_repoPath, {QStringLiteral("pull"), QStringLiteral("--no-rebase"), QStringLiteral("--no-edit")},
	                      {}, kNetworkTimeoutMs, cancel);
}

QStringList GitService::remotes() const
{
    const GitResult r = git({QStringLiteral("remote")});
    if (!r.ok())
        return {};
    return QString::fromUtf8(r.out).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
}

GitResult GitService::push(bool publish, const std::atomic_bool *cancel) const
{
    if (!publish)
        return GitRunner::run(m_repoPath, {QStringLiteral("push")}, {}, kNetworkTimeoutMs, cancel);

    const QStringList names = remotes();
    if (names.isEmpty())
        return failure(QStringLiteral("Repo chưa có remote nào. Hãy thêm remote trước khi push "
                                      "(ví dụ: git remote add origin <url>)."));
    const QString remote = names.contains(QStringLiteral("origin")) ? QStringLiteral("origin")
                                                                    : names.first();
    return GitRunner::run(m_repoPath, {QStringLiteral("push"), QStringLiteral("-u"), remote, QStringLiteral("HEAD")}, {},
                          kNetworkTimeoutMs, cancel);
}

GitResult GitService::deleteRemoteBranch(const QString &remote, const QString &branch,
                                        const std::atomic_bool *cancel) const
{
    if (remote.isEmpty() || branch.isEmpty() || branch.startsWith(QLatin1Char('-')))
        return failure(QStringLiteral("Tên remote hoặc nhánh không hợp lệ."));
    GitResult r = GitRunner::run(m_repoPath,
                                 {QStringLiteral("push"), QStringLiteral("--delete"), remote, branch},
                                 {}, kNetworkTimeoutMs, cancel);
    if (r.ok())
        (void)git({QStringLiteral("update-ref"), QStringLiteral("-d"),
                   QStringLiteral("refs/remotes/%1/%2").arg(remote, branch)});
    return r;
}

namespace {
QString stashTag(const QString &branch)
{
    return QStringLiteral("gitgui:") + branch;
}
} // namespace

std::optional<QVector<BranchInfo>> GitService::branches(QString *error) const
{
    const GitResult r = git({QStringLiteral("for-each-ref"),
                             QStringLiteral("--format=%(refname)%09%(refname:short)%09%(upstream:short)%09%(HEAD)"),
                             QStringLiteral("refs/heads"), QStringLiteral("refs/remotes")});
    if (!r.ok()) {
        if (error)
            *error = r.err;
        return std::nullopt;
    }
    return GitParser::parseBranches(r.out);
}

GitResult GitService::stashPush(const QString &branch, bool *created) const
{
    const GitResult r = git({QStringLiteral("stash"), QStringLiteral("push"),
                             QStringLiteral("--include-untracked"), QStringLiteral("-m"),
                             stashTag(branch)});
    if (created)
        *created = r.ok() && !QString::fromUtf8(r.out).contains(QStringLiteral("No local changes to save"));
    return r;
}

std::optional<QString> GitService::findSavedStash(const QString &branch) const
{
    const GitResult r = git({QStringLiteral("stash"), QStringLiteral("list"),
                             QStringLiteral("--format=%gd%x09%gs")});
    if (!r.ok())
        return std::nullopt;
    const QString suffix = QStringLiteral(": ") + stashTag(branch);
    const QStringList lines = QString::fromUtf8(r.out).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {  // mới nhất ở đầu
        const QStringList f = line.split(QLatin1Char('\t'));
        if (f.size() == 2 && f[1].endsWith(suffix))
            return f[0];
    }
    return std::nullopt;
}

GitResult GitService::switchTo(const QString &name, SwitchKind kind, const QString &stashBranch,
                               const QString &startPoint) const
{
    if (kind == SwitchKind::CreateNew) {
        const GitResult v = git({QStringLiteral("check-ref-format"), QStringLiteral("--branch"), name});
        if (!v.ok())
            return failure(QStringLiteral("Tên nhánh không hợp lệ: \"%1\"").arg(name));
    }

    bool stashed = false;
    if (!stashBranch.isEmpty()) {
        const GitResult s = stashPush(stashBranch, &stashed);
        if (!s.ok())
            return s;
    }

    QStringList args{QStringLiteral("switch")};
    if (kind == SwitchKind::CreateNew)
        args << QStringLiteral("-c");
    else if (kind == SwitchKind::TrackRemote)
        args << QStringLiteral("--track");
    args << name;
    if (!startPoint.isEmpty())
        args << startPoint;
    const GitResult r = git(args);

    if (!r.ok() && stashed) {  // không để thay đổi của người dùng "biến mất" trong stash
        if (const auto ref = findSavedStash(stashBranch))
            (void)git({QStringLiteral("stash"), QStringLiteral("pop"), *ref});
    }
    return r;
}

GitResult GitService::deleteBranch(const QString &name, bool force) const
{
    return git({QStringLiteral("branch"), force ? QStringLiteral("-D") : QStringLiteral("-d"),
                QStringLiteral("--"), name});
}

GitResult GitService::renameBranch(const QString &oldName, const QString &newName) const
{
    const GitResult v = git({QStringLiteral("check-ref-format"), QStringLiteral("--branch"), newName});
    if (!v.ok())
        return failure(QStringLiteral("Tên nhánh không hợp lệ: \"%1\"").arg(newName));
    return git({QStringLiteral("branch"), QStringLiteral("-m"), oldName, newName});
}

bool GitService::hasSavedChanges(const QString &branch) const
{
    return findSavedStash(branch).has_value();
}

GitResult GitService::restoreSavedChanges(const QString &branch) const
{
    const auto ref = findSavedStash(branch);
    if (!ref)
        return failure(QStringLiteral("Không còn thay đổi nào được cất tạm cho nhánh này."));
    return git({QStringLiteral("stash"), QStringLiteral("pop"), *ref});
}

GitResult GitService::discardSavedChanges(const QString &branch) const
{
    const auto ref = findSavedStash(branch);
    if (!ref)
        return failure(QStringLiteral("Không còn thay đổi nào được cất tạm cho nhánh này."));
    return git({QStringLiteral("stash"), QStringLiteral("drop"), *ref});
}

std::optional<QVector<CommitInfo>> GitService::log(int skip, int count, QString *error) const
{
    const GitResult r = git({QStringLiteral("log"), QStringLiteral("--skip=%1").arg(skip),
                             QStringLiteral("-n"), QString::number(count),
                             QStringLiteral("--format=%H%x1f%h%x1f%an%x1f%at%x1f%P%x1f%s%x1f%D%x1e")});
    if (!r.ok()) {
        if (r.err.contains(QStringLiteral("does not have any commits yet")))
            return QVector<CommitInfo>{};
        if (error)
            *error = r.err;
        return std::nullopt;
    }
    return GitParser::parseLog(r.out);
}

std::optional<QVector<CommitInfo>> GitService::logAll(int skip, int count, QString *error) const
{
    const GitResult r = git({QStringLiteral("log"), QStringLiteral("--all"),
                             QStringLiteral("--topo-order"), QStringLiteral("--date-order"),
                             QStringLiteral("--skip=%1").arg(skip), QStringLiteral("-n"),
                             QString::number(count),
                             QStringLiteral("--format=%H%x1f%h%x1f%an%x1f%at%x1f%P%x1f%s%x1f%D%x1e")});
    if (!r.ok()) {
        if (r.err.contains(QStringLiteral("does not have any commits yet")))
            return QVector<CommitInfo>{};
        if (error)
            *error = r.err;
        return std::nullopt;
    }

    return GitParser::parseLog(r.out);
}

QSet<QString> GitService::currentBranchHashes() const
{
    const GitResult r = git({QStringLiteral("rev-list"), QStringLiteral("HEAD")});
    QSet<QString> hashes;
    if (r.ok())
        for (const QByteArray &line : r.out.split('\n'))
            if (!line.trimmed().isEmpty())
                hashes.insert(QString::fromLatin1(line.trimmed()));
    return hashes;
}

QString GitService::commitMessage(const QString &hash) const
{
    const GitResult r = git({QStringLiteral("show"), QStringLiteral("-s"),
                             QStringLiteral("--format=%B"), hash});
    return r.ok() ? QString::fromUtf8(r.out).trimmed() : QString();
}

std::optional<QVector<CommitFile>> GitService::commitFiles(const QString &hash) const
{
    const GitResult r = git({QStringLiteral("show"), QStringLiteral("--format="),
                             QStringLiteral("--name-status"), QStringLiteral("-z"),
                             QStringLiteral("-m"), QStringLiteral("--first-parent"), hash});
    if (!r.ok())
        return std::nullopt;
    return GitParser::parseNameStatus(r.out);
}

GitResult GitService::commitFileDiff(const QString &hash, const QString &path,
                                     const QString &origPath) const
{
    QStringList args{QStringLiteral("show"), QStringLiteral("--format="), QStringLiteral("--no-color"),
                     QStringLiteral("-m"), QStringLiteral("--first-parent"), hash,
                     QStringLiteral("--"), path};
    if (!origPath.isEmpty())
        args << origPath;
    return git(args);
}

OperationKind GitService::operationInProgress() const
{
    const GitResult r = git({QStringLiteral("rev-parse"), QStringLiteral("--absolute-git-dir")});
    if (!r.ok())
        return OperationKind::None;
    const QString gitDir = QString::fromUtf8(r.out).trimmed();
    const auto exists = [&gitDir](const char *name) { return QFileInfo::exists(gitDir + QLatin1Char('/') + QLatin1String(name)); };
    if (exists("rebase-merge") || exists("rebase-apply"))
        return OperationKind::Rebase;
    if (exists("MERGE_HEAD"))
        return OperationKind::Merge;
    if (exists("CHERRY_PICK_HEAD"))
        return OperationKind::CherryPick;
    if (exists("REVERT_HEAD"))
        return OperationKind::Revert;
    return OperationKind::None;
}

GitResult GitService::continueOperation(OperationKind op) const
{
    // core.editor=true: không mở trình soạn thảo để sửa message.
    const QStringList noEditor{QStringLiteral("-c"), QStringLiteral("core.editor=true")};
    switch (op) {
    case OperationKind::Merge:
        return git({QStringLiteral("commit"), QStringLiteral("--no-edit")});
    case OperationKind::Rebase:
        return git(noEditor + QStringList{QStringLiteral("rebase"), QStringLiteral("--continue")});
    case OperationKind::CherryPick:
        return git(noEditor + QStringList{QStringLiteral("cherry-pick"), QStringLiteral("--continue")});
    case OperationKind::Revert:
        return git(noEditor + QStringList{QStringLiteral("revert"), QStringLiteral("--continue")});
    case OperationKind::None:
        break;
    }
    return failure(QStringLiteral("Không có thao tác nào đang dở dang."));
}

GitResult GitService::abortOperation(OperationKind op) const
{
    switch (op) {
    case OperationKind::Merge:
        return git({QStringLiteral("merge"), QStringLiteral("--abort")});
    case OperationKind::Rebase:
        return git({QStringLiteral("rebase"), QStringLiteral("--abort")});
    case OperationKind::CherryPick:
        return git({QStringLiteral("cherry-pick"), QStringLiteral("--abort")});
    case OperationKind::Revert:
        return git({QStringLiteral("revert"), QStringLiteral("--abort")});
    case OperationKind::None:
        break;
    }
    return failure(QStringLiteral("Không có thao tác nào đang dở dang."));
}

GitResult GitService::resolveWithContent(const QString &path, const QByteArray &content) const
{
    QFile f(m_repoPath + QLatin1Char('/') + path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return failure(QStringLiteral("Không ghi được tệp %1: %2").arg(path, f.errorString()));
    if (f.write(content) != content.size())
        return failure(QStringLiteral("Ghi tệp %1 bị lỗi: %2").arg(path, f.errorString()));
    f.close();
    return git({QStringLiteral("add"), QStringLiteral("--"), path});
}

GitResult GitService::resolveWithSide(const QString &path, bool useOurs) const
{
    // Stage 2 = ours, stage 3 = theirs; thiếu stage nghĩa là bên đó đã xoá file.
    const GitResult ls = git({QStringLiteral("ls-files"), QStringLiteral("-u"), QStringLiteral("-z"),
                              QStringLiteral("--"), path});
    if (!ls.ok())
        return ls;
    const QByteArray wanted = useOurs ? "2\t" : "3\t";
    bool present = false;
    for (const QByteArray &entry : ls.out.split('\0')) {
        const qsizetype tab = entry.indexOf('\t');
        if (tab >= 1 && entry.mid(tab - 1, 2) == wanted)
            present = true;
    }
    if (!present)
        return git({QStringLiteral("rm"), QStringLiteral("-q"), QStringLiteral("--"), path});

    const GitResult co = git({QStringLiteral("checkout"),
                              useOurs ? QStringLiteral("--ours") : QStringLiteral("--theirs"),
                              QStringLiteral("--"), path});
    if (!co.ok())
        return co;
    return git({QStringLiteral("add"), QStringLiteral("--"), path});
}

int GitService::commitCount(const QString &base, const QString &tip) const
{
    const GitResult r = git({QStringLiteral("rev-list"), QStringLiteral("--count"),
                             base + QStringLiteral("..") + tip});
    return r.ok() ? QString::fromUtf8(r.out).trimmed().toInt() : -1;
}

int GitService::rewrittenPushedCount(const QString &base) const
{
    const GitResult mb = git({QStringLiteral("merge-base"), QStringLiteral("HEAD"), QStringLiteral("@{u}")});
    if (!mb.ok())
        return 0;  // chưa có upstream hoặc không có gốc chung
    return std::max(0, commitCount(base, QString::fromUtf8(mb.out).trimmed()));
}

bool GitService::isPushed(const QString &commit) const
{
    return git({QStringLiteral("merge-base"), QStringLiteral("--is-ancestor"), commit,
                QStringLiteral("@{u}")}).ok();
}

GitResult GitService::mergeBranch(const QString &name, MergeMode mode) const
{
    QStringList args{QStringLiteral("merge")};
    switch (mode) {
    case MergeMode::MergeCommit:
        args << QStringLiteral("--no-ff") << QStringLiteral("--no-edit");
        break;
    case MergeMode::Squash:
        args << QStringLiteral("--squash");
        break;
    case MergeMode::FastForwardOnly:
        args << QStringLiteral("--ff-only");
        break;
    }
    args << name;
    return git(args);
}

GitResult GitService::rebaseOnto(const QString &base) const
{
    return git({QStringLiteral("-c"), QStringLiteral("core.editor=true"), QStringLiteral("rebase"), base});
}

GitResult GitService::cherryPick(const QString &hash, bool isMerge) const
{
    return cherryPick(QStringList{hash}, isMerge);
}

GitResult GitService::cherryPick(const QStringList &hashes, bool isMerge) const
{
    if (hashes.isEmpty())
        return failure(QStringLiteral("Chưa chọn commit nào để cherry-pick."));
    QStringList args{QStringLiteral("cherry-pick")};
    if (isMerge)
        args << QStringLiteral("-m") << QStringLiteral("1");
    args << hashes;
    return git(args);
}

GitResult GitService::revert(const QString &hash, bool isMerge) const
{
    QStringList args{QStringLiteral("revert"), QStringLiteral("--no-edit")};
    if (isMerge)
        args << QStringLiteral("-m") << QStringLiteral("1");
    args << hash;
    return git(args);
}

GitResult GitService::squashCommits(const QString &newest, const QString &oldest,
                                    const QString &message) const
{
    if (message.trimmed().isEmpty())
        return failure(QStringLiteral("Cần nhập message cho commit sau khi gom."));

    const GitResult base = git({QStringLiteral("rev-parse"), QStringLiteral("--verify"), QStringLiteral("-q"),
                                oldest + QStringLiteral("^")});
    if (!base.ok())
        return failure(QStringLiteral("Không thể gom commit gốc của repo (nó không có commit cha)."));
    const QString baseSha = QString::fromUtf8(base.out).trimmed();

    // Cả đoạn từ cha của `oldest` tới HEAD phải là một chuỗi thẳng: gom/áp lại merge commit sẽ làm mất lịch sử.
    const GitResult chain = git({QStringLiteral("rev-list"), QStringLiteral("--parents"),
                                 baseSha + QStringLiteral("..HEAD")});
    if (!chain.ok())
        return chain;
    for (const QString &line : QString::fromUtf8(chain.out).split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        if (line.split(QLatin1Char(' '), Qt::SkipEmptyParts).size() != 2)
            return failure(QStringLiteral("Vùng chọn hoặc các commit phía trên nó có merge commit nên không thể gom."));
    }
    if (!git({QStringLiteral("merge-base"), QStringLiteral("--is-ancestor"), newest, QStringLiteral("HEAD")}).ok()
        || !git({QStringLiteral("merge-base"), QStringLiteral("--is-ancestor"), oldest, newest}).ok())
        return failure(QStringLiteral("Các commit được chọn không nằm liên tiếp trong nhánh hiện tại."));

    const GitResult tree = git({QStringLiteral("rev-parse"), newest + QStringLiteral("^{tree}")});
    const GitResult head = git({QStringLiteral("rev-parse"), QStringLiteral("HEAD")});
    const GitResult newestSha = git({QStringLiteral("rev-parse"), newest});
    if (!tree.ok() || !head.ok() || !newestSha.ok())
        return failure(QStringLiteral("Không đọc được thông tin commit."));
    const bool atTip = head.out.trimmed() == newestSha.out.trimmed();

    // Rebase từ chối khi có thay đổi chưa commit trên file đã theo dõi; kiểm tra trước khi tạo gì.
    if (!atTip) {
        const GitResult dirty = git({QStringLiteral("status"), QStringLiteral("--porcelain"),
                                     QStringLiteral("--untracked-files=no")});
        if (!dirty.ok() || !dirty.out.trimmed().isEmpty())
            return failure(QStringLiteral("Cần commit hoặc cất tạm các thay đổi chưa commit trước khi gom commit "
                                          "nằm dưới những commit khác."));
    }

    const GitResult created = git({QStringLiteral("commit-tree"), QString::fromUtf8(tree.out).trimmed(),
                                   QStringLiteral("-p"), baseSha, QStringLiteral("-F"), QStringLiteral("-")},
                                  message.trimmed().toUtf8() + '\n');
    if (!created.ok())
        return created;
    const QString squashed = QString::fromUtf8(created.out).trimmed();

    if (atTip)  // cây và index vẫn đúng như cũ nên giữ nguyên cả thay đổi chưa commit
        return git({QStringLiteral("reset"), QStringLiteral("--soft"), squashed});
    return git({QStringLiteral("-c"), QStringLiteral("core.editor=true"), QStringLiteral("rebase"),
                QStringLiteral("--onto"), squashed, QString::fromUtf8(newestSha.out).trimmed()});
}

std::optional<ForcePushInfo> GitService::forcePushInfo(QString *error) const
{
    const GitResult name = git({QStringLiteral("rev-parse"), QStringLiteral("--abbrev-ref"), QStringLiteral("@{u}")});
    const GitResult sha = git({QStringLiteral("rev-parse"), QStringLiteral("@{u}")});
    if (!name.ok() || !sha.ok()) {
        if (error)
            *error = QStringLiteral("Nhánh hiện tại chưa có upstream.");
        return std::nullopt;
    }
    ForcePushInfo info;
    info.upstream = QString::fromUtf8(name.out).trimmed();
    info.expectedSha = QString::fromUtf8(sha.out).trimmed();
    const GitResult lost = git({QStringLiteral("log"), QStringLiteral("-n"), QStringLiteral("50"),
                                QStringLiteral("--format=%H%x1f%h%x1f%an%x1f%at%x1f%P%x1f%s%x1f%D%x1e"),
                                QStringLiteral("HEAD..@{u}")});
    if (lost.ok())
        info.lost = GitParser::parseLog(lost.out);
    return info;
}

GitResult GitService::forcePush(const QString &expectedSha) const
{
    const GitResult branch = git({QStringLiteral("rev-parse"), QStringLiteral("--abbrev-ref"), QStringLiteral("HEAD")});
    if (!branch.ok())
        return branch;
    const QString b = QString::fromUtf8(branch.out).trimmed();
    const GitResult remote = git({QStringLiteral("config"), QStringLiteral("branch.%1.remote").arg(b)});
    const GitResult merge = git({QStringLiteral("config"), QStringLiteral("branch.%1.merge").arg(b)});
    if (!remote.ok() || !merge.ok())
        return failure(QStringLiteral("Nhánh hiện tại chưa có upstream nên không thể force push."));
    const QString remoteName = QString::fromUtf8(remote.out).trimmed();
    const QString ref = QString::fromUtf8(merge.out).trimmed();
    return git({QStringLiteral("push"), remoteName, QStringLiteral("HEAD:") + ref,
                QStringLiteral("--force-with-lease=%1:%2").arg(ref, expectedSha)},
               {}, kNetworkTimeoutMs);
}

namespace {

// Thư mục lưu bản sao lưu trước khi bỏ thay đổi: <git-dir>/gitgui-backups/<id>/
QString backupRoot(const GitResult &gitDirResult)
{
    return QString::fromUtf8(gitDirResult.out).trimmed() + QStringLiteral("/gitgui-backups");
}

} // namespace

GitResult GitService::discardFiles(const QVector<FileChange> &files, QString *backupId) const
{
    if (files.isEmpty())
        return failure(QStringLiteral("Không có file nào để bỏ thay đổi."));

    QStringList affected;       // mọi đường dẫn bị đụng tới (để sao lưu)
    QStringList restore;        // khôi phục từ HEAD
    QStringList removeTracked;  // có trong index nhưng không có trong HEAD: gỡ khỏi index và xoá file
    QStringList removeFiles;    // chưa theo dõi: xoá file
    for (const FileChange &f : files) {
        if (f.kind == ChangeKind::Unmerged)
            return failure(QStringLiteral("File \"%1\" đang xung đột; hãy giải quyết xung đột thay vì bỏ thay đổi.").arg(f.path));
        affected << f.path;
        switch (f.kind) {
        case ChangeKind::Untracked:
            removeFiles << f.path;
            break;
        case ChangeKind::Renamed:
            affected << f.origPath;
            restore << f.origPath;
            removeTracked << f.path;
            break;
        default:
            (f.indexStatus == QLatin1Char('A') ? removeTracked : restore) << f.path;
            break;
        }
    }

    // 1. Sao lưu nội dung hiện tại. Không sao lưu được thì không bỏ gì cả.
    const GitResult gitDir = git({QStringLiteral("rev-parse"), QStringLiteral("--absolute-git-dir")});
    if (!gitDir.ok())
        return gitDir;
    const QString id = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"));
    const QString dir = backupRoot(gitDir) + QLatin1Char('/') + id;
    QJsonArray manifest;
    for (const QString &p : std::as_const(affected)) {
        const QString src = m_repoPath + QLatin1Char('/') + p;
        const bool exists = QFileInfo(src).isFile();
        if (exists) {
            const QString dst = dir + QStringLiteral("/files/") + p;
            if (!QDir().mkpath(QFileInfo(dst).absolutePath()) || !QFile::copy(src, dst))
                return failure(QStringLiteral("Không sao lưu được \"%1\" nên không bỏ thay đổi.").arg(p));
        }
        manifest.append(QJsonObject{{QStringLiteral("path"), p}, {QStringLiteral("existed"), exists}});
    }
    QFile mf(dir + QStringLiteral("/manifest.json"));
    if (!QDir().mkpath(dir) || !mf.open(QIODevice::WriteOnly)
        || mf.write(QJsonDocument(manifest).toJson()) < 0)
        return failure(QStringLiteral("Không ghi được bản sao lưu nên không bỏ thay đổi."));
    mf.close();

    // Chỉ giữ 10 bản sao lưu mới nhất.
    QDir root(backupRoot(gitDir));
    const QStringList all = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (int i = 0; i < all.size() - 10; ++i)
        QDir(root.filePath(all.at(i))).removeRecursively();

    // 2. Bỏ thay đổi.
    GitResult r = success();
    if (!restore.isEmpty()) {
        r = git({QStringLiteral("checkout"), QStringLiteral("HEAD"), QStringLiteral("--pathspec-from-file=-"),
                 QStringLiteral("--pathspec-file-nul")},
                pathspecInput(restore));
        if (!r.ok())
            return r;
    }
    if (!removeTracked.isEmpty()) {
        r = git({QStringLiteral("rm"), QStringLiteral("-f"), QStringLiteral("-q"),
                 QStringLiteral("--pathspec-from-file=-"), QStringLiteral("--pathspec-file-nul")},
                pathspecInput(removeTracked));
        if (!r.ok())
            return r;
    }
    for (const QString &p : std::as_const(removeFiles)) {
        if (!QFile::remove(m_repoPath + QLatin1Char('/') + p))
            return failure(QStringLiteral("Không xoá được \"%1\".").arg(p));
    }
    if (backupId)
        *backupId = id;
    return r;
}

GitResult GitService::discardLines(const QString &path, const FileDiff &diff, const LineSelection &lines,
                                   QString *backupId) const
{
    if (lines.isEmpty())
        return failure(QStringLiteral("Chưa chọn dòng nào để bỏ."));
    const QByteArray patch = GitDiff::buildPatch(diff, lines, PatchMode::Unstage);
    if (patch.isEmpty())
        return failure(QStringLiteral("Các dòng được chọn không có thay đổi nào để bỏ."));

    const GitResult gitDir = git({QStringLiteral("rev-parse"), QStringLiteral("--absolute-git-dir")});
    if (!gitDir.ok())
        return gitDir;
    const QString id = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"));
    const QString dir = backupRoot(gitDir) + QLatin1Char('/') + id;
    const QString src = m_repoPath + QLatin1Char('/') + path;
    const bool exists = QFileInfo(src).isFile();
    if (exists && (!QDir().mkpath(dir + QStringLiteral("/files/") + QFileInfo(path).path())
                   || !QFile::copy(src, dir + QStringLiteral("/files/") + path)))
        return failure(QStringLiteral("Không sao lưu được \"%1\" nên không bỏ dòng nào.").arg(path));
    QJsonArray manifest;
    manifest.append(QJsonObject{{QStringLiteral("path"), path}, {QStringLiteral("existed"), exists}});
    QFile mf(dir + QStringLiteral("/manifest.json"));
    if (!QDir().mkpath(dir) || !mf.open(QIODevice::WriteOnly) || mf.write(QJsonDocument(manifest).toJson()) < 0)
        return failure(QStringLiteral("Không ghi được bản sao lưu nên không bỏ dòng nào."));
    mf.close();

    // Áp NGƯỢC patch (các dòng được chọn) lên working tree, không đụng index.
    const GitResult r = git({QStringLiteral("apply"), QStringLiteral("--reverse"), QStringLiteral("--recount"),
                             QStringLiteral("-")}, patch);
    if (r.ok() && backupId)
        *backupId = id;
    if (!r.ok())
        QDir(dir).removeRecursively();  // không có gì thay đổi nên bỏ bản sao lưu thừa
    return r;
}

GitResult GitService::undoDiscard(const QString &backupId) const
{
    const GitResult gitDir = git({QStringLiteral("rev-parse"), QStringLiteral("--absolute-git-dir")});
    if (!gitDir.ok())
        return gitDir;
    const QString dir = backupRoot(gitDir) + QLatin1Char('/') + backupId;
    QFile mf(dir + QStringLiteral("/manifest.json"));
    if (backupId.isEmpty() || !mf.open(QIODevice::ReadOnly))
        return failure(QStringLiteral("Không tìm thấy bản sao lưu để hoàn tác."));
    const QJsonArray manifest = QJsonDocument::fromJson(mf.readAll()).array();
    mf.close();

    for (const QJsonValue &v : manifest) {
        const QString p = v.toObject().value(QStringLiteral("path")).toString();
        const bool existed = v.toObject().value(QStringLiteral("existed")).toBool();
        const QString target = m_repoPath + QLatin1Char('/') + p;
        if (existed) {
            QDir().mkpath(QFileInfo(target).absolutePath());
            QFile::remove(target);
            if (!QFile::copy(dir + QStringLiteral("/files/") + p, target))
                return failure(QStringLiteral("Không khôi phục được \"%1\".").arg(p));
        } else {
            QFile::remove(target);  // trước đó file không tồn tại ở working tree
        }
    }
    QDir(dir).removeRecursively();
    return success();
}

GitResult GitService::addIgnorePattern(const QString &pattern) const
{
    const QString p = pattern.trimmed();
    if (p.isEmpty())
        return failure(QStringLiteral("Mẫu .gitignore trống."));
    const QString file = m_repoPath + QStringLiteral("/.gitignore");
    QByteArray existing;
    {
        QFile f(file);
        if (f.open(QIODevice::ReadOnly))
            existing = f.readAll();
    }
    for (const QByteArray &line : existing.split('\n'))
        if (QString::fromUtf8(line).trimmed() == p)
            return success();  // đã có dòng này: không cần làm gì
    QFile f(file);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Append))
        return failure(QStringLiteral("Không ghi được .gitignore: %1").arg(f.errorString()));
    if (!existing.isEmpty() && !existing.endsWith('\n'))
        f.write("\n");
    f.write(p.toUtf8() + '\n');
    return success();
}

InitResult GitService::initRepository(const QString &parentDir, const QString &name, const QString &branch,
                                      bool withReadme, const QString &gitignoreTemplate)
{
    InitResult res;
    const QString n = name.trimmed();
    if (n.isEmpty() || n == QLatin1String(".") || n == QLatin1String("..") || n.contains(QLatin1Char('/'))
        || n.contains(QLatin1Char('\\'))) {
        res.git = failure(QStringLiteral("Tên repository không hợp lệ."));
        return res;
    }
    if (!QFileInfo(parentDir).isDir()) {
        res.git = failure(QStringLiteral("Thư mục chứa \"%1\" không tồn tại.").arg(parentDir));
        return res;
    }
    const QString dir = QDir(parentDir).filePath(n);
    if (QFileInfo::exists(dir) && !QDir(dir).isEmpty()) {
        res.git = failure(QStringLiteral("Thư mục \"%1\" đã tồn tại và không trống.").arg(dir));
        return res;
    }
    const QString br = branch.trimmed().isEmpty() ? QStringLiteral("main") : branch.trimmed();
    if (!QDir().mkpath(dir)) {
        res.git = failure(QStringLiteral("Không tạo được thư mục \"%1\".").arg(dir));
        return res;
    }
    const GitResult chk = GitRunner::run(dir, {QStringLiteral("check-ref-format"), QStringLiteral("--branch"), br});
    if (!chk.ok()) {
        res.git = failure(QStringLiteral("Tên nhánh \"%1\" không hợp lệ.").arg(br));
        return res;
    }
    res.git = GitRunner::run(dir, {QStringLiteral("init"), QStringLiteral("-q"), QStringLiteral("-b"), br});
    if (!res.git.ok())
        return res;
    res.path = QDir(dir).absolutePath();

    bool wrote = false;
    if (withReadme) {
        QFile f(dir + QStringLiteral("/README.md"));
        if (f.open(QIODevice::WriteOnly)) {
            f.write("# " + n.toUtf8() + "\n");
            wrote = true;
        }
    }
    const QString ignore = GitIgnoreTemplates::content(gitignoreTemplate);
    if (!ignore.isEmpty()) {
        QFile f(dir + QStringLiteral("/.gitignore"));
        if (f.open(QIODevice::WriteOnly)) {
            f.write(ignore.toUtf8());
            wrote = true;
        }
    }
    if (wrote) {
        const GitResult add = GitRunner::run(dir, {QStringLiteral("add"), QStringLiteral("-A")});
        const GitResult commit = add.ok()
            ? GitRunner::run(dir, {QStringLiteral("commit"), QStringLiteral("-q"), QStringLiteral("-m"),
                                   QStringLiteral("Initial commit")})
            : add;
        res.committed = commit.ok();
        if (!commit.ok())
            res.warning = commit.err.trimmed();  // vd: chưa cấu hình user.name/user.email
    }
    return res;
}

} // namespace gitcore
