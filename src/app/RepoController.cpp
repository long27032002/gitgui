#include "RepoController.h"

#include "AsyncTask.h"
#include "DiffResult.h"

#include <GitIgnoreTemplates.h>

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QRegularExpression>
#include <QUrl>

#include <algorithm>
#include <memory>

using namespace gitcore;

namespace gitapp {

namespace {

constexpr qint64 kMaxUntrackedBytes = 512 * 1024;

// File chưa được theo dõi không có diff trong git: tạo diff tổng hợp (toàn bộ nội dung là dòng thêm)
// để vẫn chọn được từng dòng đưa vào commit.
DiffResult untrackedResult(const QString &repoPath, const QString &relPath)
{
    DiffResult res;
    QFile f(repoPath + QLatin1Char('/') + relPath);
    if (!f.open(QIODevice::ReadOnly)) {
        res.plain = {QObject::tr("Không đọc được tệp: %1").arg(f.errorString())};
        return res;
    }
    const QByteArray data = f.read(kMaxUntrackedBytes + 1);
    if (data.size() > kMaxUntrackedBytes) {
        res.plain = {QObject::tr("Tệp mới quá lớn để hiển thị (> %1 KiB); sẽ được commit nguyên file.")
                         .arg(kMaxUntrackedBytes / 1024)};
        return res;
    }
    if (data.left(8000).contains('\0')) {
        res.plain = {QObject::tr("Tệp nhị phân — không hiển thị nội dung; sẽ được commit nguyên file.")};
        return res;
    }
    if (data.isEmpty()) {
        res.plain = {QObject::tr("(tệp trống)")};
        return res;
    }
    res.diff = GitDiff::forNewFile(relPath, data, QFileInfo(f).permissions().testFlag(QFileDevice::ExeOwner));
    res.structured = true;
    res.selectable = true;
    return res;
}

DiffResult computeDiff(const GitService &svc, const FileChange &fc)
{
    if (fc.kind == ChangeKind::Untracked)
        return untrackedResult(svc.repoPath(), fc.path);
    // Chọn từng dòng chỉ hợp lý với file thường; đổi tên/conflict chỉ để xem.
    return buildDiffResult(svc.diffForCommit(fc.path), fc.kind == ChangeKind::Ordinary);
}

} // namespace

RepoController::RepoController(QObject *parent)
    : QObject(parent), m_changes(this), m_diff(this), m_branches(this), m_history(this), m_conflicts(this), m_recent(this)
{
    connect(&m_diff, &DiffModel::selectionChanged, this, [this] {
        if (!m_applyingDiffSelection)
            syncSelectionFromDiff();
        emit selectedLineCountChanged();
    });

    connect(&m_history, &HistoryController::errorOccurred, this, &RepoController::setError);

    connect(&m_conflicts, &ConflictController::errorOccurred, this, &RepoController::setError);
    connect(&m_conflicts, &ConflictController::resolveContentRequested, this,
            [this](const QString &path, const QByteArray &content) {
                if (!m_service || busy())
                    return;
                const GitService svc = *m_service;
                mutate([svc, path, content] { return svc.resolveWithContent(path, content); },
                       tr("Đang lưu kết quả…"),
                       [this, path](const GitResult &) { setNotice(tr("Đã giải quyết xung đột trong %1.").arg(path)); });
            });
    connect(&m_conflicts, &ConflictController::resolveSideRequested, this,
            [this](const QString &path, bool useOurs) {
                if (!m_service || busy())
                    return;
                const GitService svc = *m_service;
                mutate([svc, path, useOurs] { return svc.resolveWithSide(path, useOurs); },
                       tr("Đang áp dụng…"),
                       [this, path](const GitResult &) { setNotice(tr("Đã giải quyết xung đột trong %1.").arg(path)); });
            });
    connect(&m_conflicts, &ConflictController::markResolvedRequested, this,
            [this](const QString &path) {
                if (!m_service || busy())
                    return;
                const GitService svc = *m_service;
                mutate([svc, path] { return svc.stage({path}); }, tr("Đang đánh dấu…"),
                       [this, path](const GitResult &) { setNotice(tr("Đã đánh dấu %1 là đã giải quyết.").arg(path)); });
            });

    m_noticeTimer.setSingleShot(true);
    m_noticeTimer.setInterval(6000);
    connect(&m_noticeTimer, &QTimer::timeout, this, [this] { setNotice({}); });

    m_lastFetchedTimer.setInterval(30 * 1000);
    connect(&m_lastFetchedTimer, &QTimer::timeout, this, [this] {
        const QString now = lastFetchedText();
        if (now != m_lastFetchedTextShown) {
            m_lastFetchedTextShown = now;
            emit lastFetchedTextChanged();
        }
    });
    m_lastFetchedTimer.start();

    m_autoFetchTimer.setInterval(5 * 60 * 1000);
    connect(&m_autoFetchTimer, &QTimer::timeout, this, &RepoController::backgroundFetch);
}

void RepoController::setAutoFetchIntervalMs(int ms)
{
    m_autoFetchTimer.setInterval(ms);
    if (m_service)
        m_autoFetchTimer.start();
}

std::optional<FileChange> RepoController::currentChange() const
{
    const int row = m_changes.rowOfPath(m_selectedPath);
    if (row < 0)
        return std::nullopt;
    return m_changes.at(row);
}

QString RepoController::selectedStatus() const
{
    const auto fc = currentChange();
    return fc ? ChangesModel::statusLabel(*fc) : QString();
}

QString RepoController::repoPath() const
{
    return m_service ? m_service->repoPath() : QString();
}

QString RepoController::repoName() const
{
    return m_service ? QFileInfo(m_service->repoPath()).fileName() : QString();
}

void RepoController::openRepo(const QString &path)
{
    if (path.isEmpty())
        return;
    beginTask();
    runAsync<std::optional<QString>>(
        this, [path] { return GitService::findRoot(path); },
        [this, path](const std::optional<QString> &root) {
            if (!root) {
                m_recent.remove(path);  // thư mục đã bị xoá/không còn là repo
                setError(tr("Thư mục này không phải repository Git:\n%1").arg(path));
                endTask();
                return;
            }
            m_service.emplace(*root);
            m_recent.touch(*root);
            m_lastFetch = {};
            m_history.setService(*m_service);
            m_conflicts.setState(std::nullopt, OperationKind::None, {});
            m_operation = OperationKind::None;
            m_conflictCount = 0;
            m_lastOid.clear();
            m_hasSaved = false;
            m_changes.setChanges({});
            m_diff.clear();
            setSelectedPath({});
            emit diffContextChanged();
            emit repoPathChanged();
            clearError();
            if (!m_pendingWarning.isEmpty()) {
                setError(m_pendingWarning);
                m_pendingWarning.clear();
            }
            m_autoFetchTimer.start();
            refresh();
            endTask();
        });
}

void RepoController::openRepoUrl(const QUrl &url)
{
    openRepo(url.toLocalFile());
}

struct RepoController::RefreshData {
    std::optional<RepoStatus> status;
    QString error;
    QVector<BranchInfo> branches;
    bool hasSaved = false;
    OperationKind op = OperationKind::None;
    QString userName;
    QDateTime lastFetch;
    std::optional<HeadInfo> head;
};

void RepoController::refresh()
{
    if (!m_service)
        return;
    beginTask();
    const GitService svc = *m_service;
    runAsync<RefreshData>(
        this,
        [svc] {
            RefreshData d;
            d.status = svc.status(&d.error);
            if (d.status) {
                d.op = svc.operationInProgress();
                d.userName = svc.userName();
                d.head = svc.headCommit();
                d.lastFetch = svc.lastFetchTime();
                d.branches = svc.branches().value_or(QVector<BranchInfo>{});
                if (d.status->branch != QLatin1String("(detached)"))
                    d.hasSaved = svc.hasSavedChanges(d.status->branch);
            }
            return d;
        },
        [this](const RefreshData &d) {
            if (d.status)
                applyStatus(d);
            else
                setError(tr("Không đọc được trạng thái repo:\n%1").arg(d.error.trimmed()));
            endTask();
        });
}

void RepoController::applyStatus(const RefreshData &data)
{
    const RepoStatus &status = *data.status;
    m_branches.setBranches(data.branches);
    m_hasSaved = data.hasSaved;
    m_operation = data.op;
    m_userName = data.userName;
    m_head = data.head;
    if (data.lastFetch.isValid() && (!m_lastFetch.isValid() || data.lastFetch > m_lastFetch)) {
        m_lastFetch = data.lastFetch;
        m_lastFetchedTextShown = lastFetchedText();
        emit lastFetchedTextChanged();
    }
    QVector<FileChange> unmerged;
    for (const FileChange &c : status.changes)
        if (c.kind == ChangeKind::Unmerged)
            unmerged.append(c);
    m_conflictCount = static_cast<int>(unmerged.size());
    m_conflicts.setState(m_service, m_operation, unmerged);
    if (status.oid != m_lastOid) {  // HEAD đã di chuyển: commit, pull, merge, đổi nhánh...
        m_lastOid = status.oid;
        m_history.reload();
    }
    m_branch = status.branch;
    m_upstream = status.upstream;
    m_ahead = status.ahead;
    m_behind = status.behind;

    QVector<FileChange> sorted = status.changes;
    std::sort(sorted.begin(), sorted.end(),
              [](const FileChange &a, const FileChange &b) { return a.path < b.path; });
    m_changes.setChanges(std::move(sorted));

    // Dọn lựa chọn của các file không còn thay đổi (đã commit/hoàn tác): lần sau xuất hiện lại sẽ mặc định chọn hết.
    for (auto it = m_selection.begin(); it != m_selection.end();) {
        if (m_changes.rowOfPath(it.key()) < 0) {
            m_changes.setCheckState(it.key(), 2);
            it = m_selection.erase(it);
        } else {
            ++it;
        }
    }
    emit statusChanged();

    int row = m_changes.rowOfPath(m_selectedPath);
    if (row < 0 && m_changes.rowCount() > 0)
        row = 0;  // tự chọn file đầu tiên cho tiện
    if (row >= 0) {
        const FileChange fc = m_changes.at(row);
        setSelectedPath(fc.path);
        emit diffContextChanged();
        loadDiff(fc);
    } else {
        setSelectedPath({});
        ++m_diffRequest;
        m_diff.clear();
        emit diffContextChanged();
    }
}

void RepoController::selectRow(int row)
{
    if (!m_service || row < 0 || row >= m_changes.rowCount())
        return;
    const FileChange fc = m_changes.at(row);
    setSelectedPath(fc.path);
    emit diffContextChanged();
    loadDiff(fc);
}

void RepoController::loadDiff(const FileChange &change)
{
    if (!m_service)
        return;
    const quint64 id = ++m_diffRequest;
    const GitService svc = *m_service;
    const QString path = change.path;
    runAsync<DiffResult>(
        this, [svc, change] { return computeDiff(svc, change); },
        [this, id, path](const DiffResult &res) {
            if (id != m_diffRequest)
                return;
            // Áp lựa chọn đã lưu của file này lên diff vừa tải (mặc định: chọn mọi dòng).
            const FileSelection sel = m_selection.value(path);
            m_applyingDiffSelection = true;
            showDiffResult(m_diff, res, path, sel.mode == FileSelection::Partial ? sel.excluded : QSet<QByteArray>{});
            if (sel.mode == FileSelection::None)
                m_diff.selectAll(false);
            m_applyingDiffSelection = false;
            syncSelectionFromDiff();  // chuẩn hoá: lựa chọn cũ không còn khớp dòng nào -> trở về "chọn hết"
        });
}

void RepoController::toggleDiffRow(int row)
{
    m_diff.toggleRow(row);
}

int RepoController::selectionStateOf(const QString &path) const
{
    switch (m_selection.value(path).mode) {
    case FileSelection::None:
        return 0;
    case FileSelection::Partial:
        return 1;
    case FileSelection::All:
        break;
    }
    return 2;
}

void RepoController::setFileSelection(const QString &path, const FileSelection &sel)
{
    if (sel.mode == FileSelection::All)
        m_selection.remove(path);
    else
        m_selection.insert(path, sel);
    m_changes.setCheckState(path, selectionStateOf(path));
    emit statusChanged();
}

void RepoController::syncSelectionFromDiff()
{
    if (m_selectedPath.isEmpty() || !m_diff.selectable())
        return;
    const int total = GitDiff::changeLineCount(m_diff.fileDiff());
    const int chosen = m_diff.selectedCount();
    FileSelection sel;
    if (chosen == total) {
        sel.mode = FileSelection::All;
    } else if (chosen == 0) {
        sel.mode = FileSelection::None;
    } else {
        sel.mode = FileSelection::Partial;
        sel.excluded = GitDiff::excludedKeys(m_diff.fileDiff(), m_diff.selection());
    }
    const FileSelection old = m_selection.value(m_selectedPath);
    if (old.mode == sel.mode && old.excluded == sel.excluded)
        return;
    setFileSelection(m_selectedPath, sel);
}

void RepoController::toggleChecked(int row)
{
    if (!m_service || busy() || row < 0 || row >= m_changes.rowCount())
        return;
    const FileChange fc = m_changes.at(row);
    if (fc.kind == ChangeKind::Unmerged) {
        // File còn dấu xung đột chưa được phép đưa vào commit.
        setError(tr("File \"%1\" đang xung đột. Hãy giải quyết trong tab Xung đột trước khi chọn nó vào commit.").arg(fc.path));
        return;
    }
    const bool include = selectionStateOf(fc.path) == 0;  // đang không chọn -> chọn hết; còn lại -> bỏ chọn
    FileSelection sel;
    sel.mode = include ? FileSelection::All : FileSelection::None;
    setFileSelection(fc.path, sel);
    if (fc.path == m_selectedPath && m_diff.selectable()) {
        m_applyingDiffSelection = true;
        m_diff.selectAll(include);
        m_applyingDiffSelection = false;
    }
}

void RepoController::commit(const QString &summary, const QString &description, const QString &coAuthors,
                            bool amend)
{
    if (!m_service || busy())
        return;
    const QString title = summary.trimmed();
    if (title.isEmpty()) {
        setError(tr("Cần nhập tiêu đề cho commit."));
        return;
    }
    if (m_operation != OperationKind::None || m_conflictCount > 0) {
        setError(tr("Đang có merge/rebase hoặc xung đột chưa xong. Hãy hoàn tất ở tab Xung đột trước khi commit."));
        return;
    }
    if (amend && !canAmend()) {
        setError(tr("Không thể sửa commit gần nhất: nó đã được push, là merge commit hoặc repo chưa có commit."));
        return;
    }

    // Co-author: "Tên <email>", phân tách bởi dấu phẩy hoặc xuống dòng.
    static const QRegularExpression separator(QStringLiteral("[,\\n]"));
    static const QRegularExpression valid(QStringLiteral(R"(^[^<>\n]+<[^<>@\s]+@[^<>@\s]+>$)"));
    QStringList trailers;
    for (const QString &raw : coAuthors.split(separator, Qt::SkipEmptyParts)) {
        const QString entry = raw.trimmed();
        if (entry.isEmpty())
            continue;
        if (!valid.match(entry).hasMatch()) {
            setError(tr("Co-author \"%1\" chưa đúng định dạng. Hãy nhập dạng: Tên <email@example.com>").arg(entry));
            return;
        }
        trailers << QStringLiteral("Co-authored-by: ") + entry;
    }

    // Gom các file/dòng đang được chọn.
    QVector<CommitEntry> entries;
    for (int i = 0; i < m_changes.rowCount(); ++i) {
        const FileChange &c = m_changes.at(i);
        if (c.kind == ChangeKind::Unmerged)
            continue;
        const FileSelection sel = m_selection.value(c.path);
        if (sel.mode == FileSelection::None)
            continue;
        CommitEntry e;
        e.path = c.path;
        e.origPath = c.origPath;
        e.partial = sel.mode == FileSelection::Partial && c.kind != ChangeKind::Renamed;
        e.excluded = sel.excluded;
        entries.append(e);
    }
    if (entries.isEmpty() && !amend) {
        setError(tr("Chưa chọn thay đổi nào để commit. Hãy tích vào ô bên trái tên file."));
        return;
    }

    QString message = title;
    if (!description.trimmed().isEmpty())
        message += QStringLiteral("\n\n") + description.trimmed();
    if (!trailers.isEmpty())
        message += QStringLiteral("\n\n") + trailers.join(QLatin1Char('\n'));

    const GitService svc = *m_service;
    mutate([svc, entries, message, amend] { return svc.commitSelected(entries, message, amend); },
           amend ? tr("Đang sửa commit…") : tr("Đang commit…"), [this](const GitResult &) { emit committed(); });
}

void RepoController::mutate(std::function<GitResult()> work, const QString &activity,
                            std::function<void(const GitResult &)> onSuccess,
                            std::shared_ptr<std::atomic_bool> cancel)
{
    beginTask();
    if (m_activity != activity) {
        m_activity = activity;
        emit activityChanged();
    }
    if (m_activeCancel != cancel) {
        m_activeCancel = std::move(cancel);
        emit canCancelChanged();
    }
    runAsync<GitResult>(
        this, std::move(work),
        [this, onSuccess = std::move(onSuccess)](const GitResult &r) {
            if (r.ok()) {
                clearError();
                if (onSuccess)
                    onSuccess(r);
            } else if (r.cancelled) {
                clearError();
                setNotice(tr("Đã huỷ lệnh mạng. Hãy kiểm tra trạng thái repository và remote."));
            } else if (r.timedOut) {
                setError(tr("Lệnh git chạy quá lâu và đã bị huỷ (kiểm tra kết nối mạng)."));
            } else {
                // Một số lỗi (vd conflict khi pull) git ghi ở stdout chứ không phải stderr.
                const QString msg = r.err.trimmed().isEmpty()
                                        ? QString::fromUtf8(r.out).trimmed() : r.err.trimmed();
                setError(msg.isEmpty() ? tr("Lệnh git thất bại (mã %1).").arg(r.exitCode) : msg);
            }
            refresh();  // gọi trước endTask để busy không nhấp nháy
            if (m_activeCancel) {
                m_activeCancel.reset();
                emit canCancelChanged();
            }
            endTask();
        });
}

void RepoController::fetch()
{
    if (!m_service || busy())
        return;
    const GitService svc = *m_service;
    auto cancel = std::make_shared<std::atomic_bool>(false);
    mutate([svc, cancel] { return svc.fetch(cancel.get()); }, tr("Đang fetch…"), [this](const GitResult &) {
        markFetched();
        setNotice(tr("Đã fetch xong."));
    }, cancel);
}

void RepoController::pull()
{
    if (!m_service || busy())
        return;
    if (!hasUpstream()) {
        setError(tr("Nhánh này chưa có upstream để pull. Hãy push (publish) nhánh trước."));
        return;
    }
    const GitService svc = *m_service;
    auto cancel = std::make_shared<std::atomic_bool>(false);
    mutate([svc, cancel] { return svc.pull(cancel.get()); }, tr("Đang pull…"), [this](const GitResult &r) {
        markFetched();
        setNotice(QString::fromUtf8(r.out).contains("Already up to date")
                      ? tr("Đã cập nhật — không có gì mới để pull.")
                      : tr("Đã pull xong."));
    }, cancel);
}

void RepoController::push()
{
    if (!m_service || busy())
        return;
    if (!canSync()) {
        setError(tr("Đang ở trạng thái detached HEAD — hãy chuyển sang một nhánh trước khi push."));
        return;
    }
    const GitService svc = *m_service;
    const bool publish = !hasUpstream();
    auto cancel = std::make_shared<std::atomic_bool>(false);
    mutate([svc, publish, cancel] { return svc.push(publish, cancel.get()); },
           publish ? tr("Đang publish nhánh…") : tr("Đang push…"),
           [this, publish](const GitResult &) {
               setNotice(publish ? tr("Đã publish nhánh lên remote.") : tr("Đã push xong."));
           }, cancel);
}

void RepoController::cancelNetworkOperation()
{
    if (!m_activeCancel)
        return;
    m_activeCancel->store(true);
    const QString stopping = tr("Đang huỷ lệnh mạng…");
    if (m_activity != stopping) {
        m_activity = stopping;
        emit activityChanged();
    }
}

void RepoController::deleteRemoteBranch(const QString &remoteBranch)
{
    if (!m_service || busy() || remoteBranch.isEmpty())
        return;
    const int slash = remoteBranch.indexOf(QLatin1Char('/'));
    if (slash <= 0 || slash == remoteBranch.size() - 1) {
        setError(tr("Tên nhánh remote không hợp lệ."));
        return;
    }
    const QString remote = remoteBranch.left(slash);
    const QString branch = remoteBranch.mid(slash + 1);
    const GitService svc = *m_service;
    auto cancel = std::make_shared<std::atomic_bool>(false);
    mutate([svc, remote, branch, cancel] { return svc.deleteRemoteBranch(remote, branch, cancel.get()); },
           tr("Đang xoá nhánh trên remote…"),
           [this, remoteBranch](const GitResult &) { setNotice(tr("Đã xoá nhánh remote %1.").arg(remoteBranch)); }, cancel);
}

void RepoController::backgroundFetch()
{
    // Lặng lẽ: không chiếm trạng thái busy, không báo lỗi (có thể chỉ là mất mạng).
    if (!m_service || !hasUpstream() || m_pending > 0 || m_bgFetching)
        return;
    m_bgFetching = true;
    const GitService svc = *m_service;
    runAsync<GitResult>(
        this, [svc] { return svc.fetch(); },
        [this](const GitResult &r) {
            m_bgFetching = false;
            if (r.ok())
                markFetched();
            if (r.ok() && m_pending == 0)
                refresh();
        });
}

void RepoController::markFetched()
{
    m_lastFetch = QDateTime::currentDateTime();
    m_lastFetchedTextShown = lastFetchedText();
    emit lastFetchedTextChanged();
}

QString RepoController::relativeTimeText(const QDateTime &then, const QDateTime &now)
{
    if (!then.isValid())
        return tr("Chưa từng fetch");
    const qint64 secs = then.secsTo(now);
    if (secs < 60)
        return tr("Vừa fetch xong");
    const qint64 mins = secs / 60;
    if (mins < 60)
        return tr("Fetch lần cuối %1 phút trước").arg(mins);
    const qint64 hours = mins / 60;
    if (hours < 24)
        return tr("Fetch lần cuối %1 giờ trước").arg(hours);
    return tr("Fetch lần cuối %1 ngày trước").arg(hours / 24);
}

int RepoController::includedCount() const
{
    int n = 0;
    for (int i = 0; i < m_changes.rowCount(); ++i) {
        const FileChange &c = m_changes.at(i);
        if (c.kind != ChangeKind::Unmerged && selectionStateOf(c.path) != 0)
            ++n;
    }
    return n;
}

int RepoController::selectAllState() const
{
    int total = 0, full = 0, any = 0;
    for (int i = 0; i < m_changes.rowCount(); ++i) {
        const FileChange &c = m_changes.at(i);
        if (c.kind == ChangeKind::Unmerged)
            continue;
        ++total;
        const int st = selectionStateOf(c.path);
        full += st == 2 ? 1 : 0;
        any += st >= 1 ? 1 : 0;
    }
    if (total == 0 || any == 0)
        return 0;
    return full == total ? 2 : 1;
}

void RepoController::toggleAll()
{
    if (!m_service || busy())
        return;
    const bool include = selectAllState() != 2;  // đã chọn hết -> bỏ hết; còn lại -> chọn hết
    for (int i = 0; i < m_changes.rowCount(); ++i) {
        const FileChange &c = m_changes.at(i);
        if (c.kind == ChangeKind::Unmerged)
            continue;  // file xung đột không bao giờ được tự đưa vào commit
        FileSelection sel;
        sel.mode = include ? FileSelection::All : FileSelection::None;
        setFileSelection(c.path, sel);
    }
    if (!m_selectedPath.isEmpty() && m_diff.selectable()) {
        m_applyingDiffSelection = true;
        m_diff.selectAll(include);
        m_applyingDiffSelection = false;
    }
}

void RepoController::setTheme(const Theme *theme)
{
    m_diff.setTheme(theme);
    m_history.setTheme(theme);
}

void RepoController::removeRecent(const QString &path)
{
    m_recent.remove(path);
}

void RepoController::revealRepoFolder()
{
    if (m_service)
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_service->repoPath()));
}

void RepoController::setNotice(const QString &text)
{
    if (m_notice == text)
        return;
    m_notice = text;
    emit noticeChanged();
    if (text.isEmpty())
        m_noticeTimer.stop();
    else
        m_noticeTimer.start();
}

void RepoController::runSwitch(const QString &target, SwitchKind kind, bool stashChanges)
{
    if (stashChanges && !canSync()) {
        setError(tr("Đang ở trạng thái detached HEAD nên không thể cất tạm theo nhánh. "
                    "Hãy commit hoặc chọn mang thay đổi theo."));
        return;
    }
    const GitService svc = *m_service;
    const QString stashBranch = stashChanges ? m_branch : QString();
    mutate([svc, target, kind, stashBranch] { return svc.switchTo(target, kind, stashBranch); },
           kind == SwitchKind::CreateNew ? tr("Đang tạo nhánh…") : tr("Đang chuyển nhánh…"),
           [this, target, kind](const GitResult &) {
               setNotice(kind == SwitchKind::CreateNew ? tr("Đã tạo và chuyển sang nhánh %1.").arg(target)
                                                       : tr("Đã chuyển sang nhánh %1.").arg(target));
           });
}

void RepoController::switchBranch(const QString &name, bool stashChanges)
{
    if (!m_service || busy() || name.isEmpty() || name == m_branch)
        return;
    if (const auto remote = m_branches.find(name, true)) {
        // Nhánh remote: nếu đã có nhánh local cùng tên thì chuyển sang nó, không thì tạo mới theo dõi.
        const QString local = name.section(QLatin1Char('/'), 1);
        if (!local.isEmpty() && m_branches.find(local, false)) {
            if (local == m_branch)
                return;
            runSwitch(local, SwitchKind::Existing, stashChanges);
        } else {
            runSwitch(name, SwitchKind::TrackRemote, stashChanges);
        }
        return;
    }
    runSwitch(name, SwitchKind::Existing, stashChanges);
}

void RepoController::createBranch(const QString &name, bool stashChanges)
{
    const QString trimmed = name.trimmed();
    if (!m_service || busy())
        return;
    if (trimmed.isEmpty()) {
        setError(tr("Cần nhập tên cho nhánh mới."));
        return;
    }
    runSwitch(trimmed, SwitchKind::CreateNew, stashChanges);
}

void RepoController::createBranchFromCommit(const QString &name, const QString &hash, bool stashChanges)
{
    const QString trimmed = name.trimmed();
    if (!m_service || busy() || trimmed.isEmpty() || hash.isEmpty())
        return;
    const GitService svc = *m_service;
    const QString stashBranch = stashChanges ? m_branch : QString();
    mutate([svc, trimmed, hash, stashBranch] {
        return svc.switchTo(trimmed, SwitchKind::CreateNew, stashBranch, hash);
    }, tr("Đang tạo nhánh từ commit…"),
           [this, trimmed](const GitResult &) { setNotice(tr("Đã tạo và chuyển sang nhánh %1.").arg(trimmed)); });
}

void RepoController::renameBranch(const QString &oldName, const QString &newName)
{
    const QString trimmed = newName.trimmed();
    if (!m_service || busy() || trimmed.isEmpty() || trimmed == oldName)
        return;
    const GitService svc = *m_service;
    mutate([svc, oldName, trimmed] { return svc.renameBranch(oldName, trimmed); },
           tr("Đang đổi tên nhánh…"),
           [this, trimmed](const GitResult &) { setNotice(tr("Đã đổi tên nhánh thành %1.").arg(trimmed)); });
}

void RepoController::deleteBranch(const QString &name)
{
    if (!m_service || busy() || name.isEmpty())
        return;
    if (name == m_branch) {
        setError(tr("Không thể xoá nhánh đang làm việc. Hãy chuyển sang nhánh khác trước."));
        return;
    }
    const GitService svc = *m_service;
    beginTask();
    runAsync<GitResult>(
        this, [svc, name] { return svc.deleteBranch(name, false); },
        [this, name](const GitResult &r) {
            if (r.ok()) {
                clearError();
                setNotice(tr("Đã xoá nhánh %1.").arg(name));
            } else if (r.err.contains(QLatin1String("not fully merged"))) {
                m_pendingForceDelete = name;  // giao diện sẽ hỏi có xoá bắt buộc không
                emit pendingForceDeleteChanged();
            } else {
                setError(r.err.trimmed());
            }
            refresh();
            endTask();
        });
}

void RepoController::confirmForceDelete()
{
    if (!m_service || busy() || m_pendingForceDelete.isEmpty())
        return;
    const QString name = m_pendingForceDelete;
    m_pendingForceDelete.clear();
    emit pendingForceDeleteChanged();
    const GitService svc = *m_service;
    mutate([svc, name] { return svc.deleteBranch(name, true); }, tr("Đang xoá nhánh…"),
           [this, name](const GitResult &) { setNotice(tr("Đã xoá nhánh %1.").arg(name)); });
}

void RepoController::cancelForceDelete()
{
    if (m_pendingForceDelete.isEmpty())
        return;
    m_pendingForceDelete.clear();
    emit pendingForceDeleteChanged();
}

void RepoController::restoreSavedChanges()
{
    if (!m_service || busy() || !m_hasSaved)
        return;
    const GitService svc = *m_service;
    const QString branch = m_branch;
    mutate([svc, branch] { return svc.restoreSavedChanges(branch); }, tr("Đang khôi phục thay đổi…"),
           [this](const GitResult &) { setNotice(tr("Đã khôi phục thay đổi đã cất tạm.")); });
}

void RepoController::discardSavedChanges()
{
    if (!m_service || busy() || !m_hasSaved)
        return;
    const GitService svc = *m_service;
    const QString branch = m_branch;
    mutate([svc, branch] { return svc.discardSavedChanges(branch); }, tr("Đang xoá thay đổi đã cất…"),
           [this](const GitResult &) { setNotice(tr("Đã xoá thay đổi đã cất tạm.")); });
}

QString RepoController::operation() const
{
    switch (m_operation) {
    case OperationKind::Merge:
        return QStringLiteral("merge");
    case OperationKind::Rebase:
        return QStringLiteral("rebase");
    case OperationKind::CherryPick:
        return QStringLiteral("cherry-pick");
    case OperationKind::Revert:
        return QStringLiteral("revert");
    case OperationKind::None:
        break;
    }
    return {};
}

void RepoController::prepareBranchAction(const QString &name)
{
    const quint64 gen = ++m_prepareGen;
    m_branchCompare = {{QStringLiteral("name"), name}, {QStringLiteral("loaded"), false}};
    emit branchCompareChanged();
    if (!m_service)
        return;
    const GitService svc = *m_service;
    struct Counts {
        int incoming = 0, local = 0, pushed = 0;
    };
    runAsync<Counts>(
        this,
        [svc, name] {
            Counts c;
            c.incoming = svc.commitCount(QStringLiteral("HEAD"), name);
            c.local = svc.commitCount(name, QStringLiteral("HEAD"));
            c.pushed = svc.rewrittenPushedCount(name);
            return c;
        },
        [this, gen, name](const Counts &c) {
            if (gen != m_prepareGen)
                return;
            m_branchCompare = {{QStringLiteral("name"), name},
                               {QStringLiteral("loaded"), true},
                               {QStringLiteral("incoming"), c.incoming},  // commit của nhánh kia chưa có ở đây
                               {QStringLiteral("local"), c.local},        // commit của nhánh này chưa có ở nhánh kia
                               {QStringLiteral("pushed"), c.pushed}};     // commit đã push sẽ bị viết lại nếu rebase
            emit branchCompareChanged();
        });
}

void RepoController::mergeBranch(const QString &name, int mode)
{
    if (!m_service || busy() || name.isEmpty())
        return;
    const MergeMode m = mode == 1 ? MergeMode::Squash
                        : mode == 2 ? MergeMode::FastForwardOnly : MergeMode::MergeCommit;
    const GitService svc = *m_service;
    mutate([svc, name, m] { return svc.mergeBranch(name, m); },
           m == MergeMode::Squash ? tr("Đang gom thay đổi…") : tr("Đang merge…"),
           [this, name, m](const GitResult &) {
               if (m == MergeMode::Squash) {
                   setNotice(tr("Đã gom thay đổi của %1 vào vùng stage — hãy nhập message và commit.").arg(name));
                   emit squashMergeStaged();
               } else {
                   setNotice(tr("Đã merge %1 vào nhánh hiện tại.").arg(name));
               }
           });
}

void RepoController::rebaseOnto(const QString &name)
{
    if (!m_service || busy() || name.isEmpty())
        return;
    const GitService svc = *m_service;
    mutate([svc, name] { return svc.rebaseOnto(name); }, tr("Đang rebase…"),
           [this, name](const GitResult &) { setNotice(tr("Đã rebase nhánh hiện tại lên %1.").arg(name)); });
}

void RepoController::cherryPick(const QString &hash)
{
    if (!m_service || busy() || hash.isEmpty())
        return;
    const int row = m_history.commitsModel().rowOfHash(hash);
    const bool isMerge = row >= 0 && m_history.commitsModel().at(row).parentCount > 1;
    const GitService svc = *m_service;
    mutate([svc, hash, isMerge] { return svc.cherryPick(hash, isMerge); }, tr("Đang cherry-pick…"),
           [this](const GitResult &) { setNotice(tr("Đã cherry-pick commit vào nhánh hiện tại.")); });
}

void RepoController::cherryPickSelected()
{
    if (!m_service || busy())
        return;
    const int first = m_history.rangeStart();
    const int last = m_history.rangeEnd();
    if (first < 0 || last < first)
        return;
    QStringList hashes;
    bool isMerge = false;
    const CommitsModel &model = m_history.commitsModel();
    for (int row = last; row >= first; --row) {
        hashes.push_back(model.at(row).hash);
        isMerge = isMerge || model.at(row).parentCount > 1;
    }
    const GitService svc = *m_service;
    mutate([svc, hashes, isMerge] { return svc.cherryPick(hashes, isMerge); },
           tr("Đang cherry-pick %1 commit…").arg(hashes.size()),
           [this, count = hashes.size()](const GitResult &) {
               setNotice(tr("Đã cherry-pick %1 commit vào nhánh hiện tại.").arg(count));
           });
}

void RepoController::revertCommit(const QString &hash)
{
    if (!m_service || busy() || hash.isEmpty())
        return;
    const int row = m_history.commitsModel().rowOfHash(hash);
    const bool isMerge = row >= 0 && m_history.commitsModel().at(row).parentCount > 1;
    const GitService svc = *m_service;
    mutate([svc, hash, isMerge] { return svc.revert(hash, isMerge); }, tr("Đang revert…"),
           [this](const GitResult &) { setNotice(tr("Đã tạo commit revert.")); });
}

void RepoController::prepareSquash()
{
    const quint64 gen = ++m_prepareGen;
    const CommitsModel &commits = m_history.commitsModel();
    const int from = m_history.rangeStart();  // commit mới nhất của đoạn
    const int to = m_history.rangeEnd();      // commit cũ nhất của đoạn

    const auto fail = [this](const QString &why) {
        m_squashPlan = {{QStringLiteral("ok"), false}, {QStringLiteral("loaded"), true},
                        {QStringLiteral("error"), why}};
        emit squashPlanChanged();
    };
    if (!m_service || from < 0 || to >= commits.rowCount())
        return fail(tr("Chưa chọn commit nào."));
    if (to - from + 1 < 2)
        return fail(tr("Hãy chọn ít nhất 2 commit liên tiếp (bấm một commit rồi Shift+bấm commit khác)."));
    for (int k = from; k <= to; ++k) {
        if (!commits.at(k).onCurrentBranch)
            return fail(tr("Chỉ có thể gom các commit thuộc nhánh hiện tại."));
        if (commits.at(k).parentCount != 1)
            return fail(commits.at(k).parentCount > 1
                            ? tr("Vùng chọn có merge commit nên không thể gom.")
                            : tr("Không thể gom commit gốc của repo (nó không có commit cha)."));
        if (k < to && commits.at(k).firstParent != commits.at(k + 1).hash)
            return fail(tr("Các commit được chọn không liên tiếp trên một nhánh thẳng."));
    }

    const QString newest = commits.at(from).hash;
    const QString oldest = commits.at(to).hash;
    QStringList lines;
    for (int k = to; k >= from; --k)  // cũ -> mới
        lines << QStringLiteral("- ") + commits.at(k).subject;
    const QString message = commits.at(to).subject + QStringLiteral("\n\n") + lines.join(QLatin1Char('\n'));
    const int count = to - from + 1;
    const bool dirty = from > 0 && m_changes.rowCount() > 0;  // commit nằm dưới đỉnh -> cần rebase -> cần cây sạch

    m_squashPlan = {{QStringLiteral("loaded"), false}};
    emit squashPlanChanged();
    const GitService svc = *m_service;
    runAsync<bool>(
        this, [svc, oldest] { return svc.isPushed(oldest); },
        [this, gen, newest, oldest, message, count, dirty](bool pushed) {
            if (gen != m_prepareGen)
                return;
            m_squashPlan = {{QStringLiteral("ok"), true},     {QStringLiteral("loaded"), true},
                            {QStringLiteral("count"), count}, {QStringLiteral("newest"), newest},
                            {QStringLiteral("oldest"), oldest}, {QStringLiteral("message"), message},
                            {QStringLiteral("pushed"), pushed}, {QStringLiteral("dirty"), dirty}};
            emit squashPlanChanged();
        });
}

void RepoController::squashSelected(const QString &message)
{
    if (!m_service || busy() || !m_squashPlan.value(QStringLiteral("ok")).toBool())
        return;
    const QString newest = m_squashPlan.value(QStringLiteral("newest")).toString();
    const QString oldest = m_squashPlan.value(QStringLiteral("oldest")).toString();
    const int count = m_squashPlan.value(QStringLiteral("count")).toInt();
    const GitService svc = *m_service;
    mutate([svc, newest, oldest, message] { return svc.squashCommits(newest, oldest, message); },
           tr("Đang gom commit…"),
           [this, count](const GitResult &) { setNotice(tr("Đã gom %1 commit thành một.").arg(count)); });
}

void RepoController::prepareForcePush()
{
    const quint64 gen = ++m_prepareGen;
    m_forcePushInfo = {{QStringLiteral("loaded"), false}};
    emit forcePushInfoChanged();
    if (!m_service)
        return;
    const GitService svc = *m_service;
    struct Out {
        std::optional<ForcePushInfo> info;
        QString error;
    };
    runAsync<Out>(
        this,
        [svc] {
            Out o;
            o.info = svc.forcePushInfo(&o.error);
            return o;
        },
        [this, gen](const Out &o) {
            if (gen != m_prepareGen)
                return;
            if (!o.info) {
                m_forcePushInfo = {{QStringLiteral("loaded"), true}, {QStringLiteral("error"), o.error}};
            } else {
                QStringList commits;
                for (const CommitInfo &c : o.info->lost)
                    commits << c.shortHash + QStringLiteral("  ") + c.subject;
                m_forcePushInfo = {{QStringLiteral("loaded"), true},
                                   {QStringLiteral("upstream"), o.info->upstream},
                                   {QStringLiteral("expectedSha"), o.info->expectedSha},
                                   {QStringLiteral("count"), static_cast<int>(o.info->lost.size())},
                                   {QStringLiteral("commits"), commits}};
            }
            emit forcePushInfoChanged();
        });
}

void RepoController::confirmForcePush()
{
    const QString sha = m_forcePushInfo.value(QStringLiteral("expectedSha")).toString();
    if (!m_service || busy() || sha.isEmpty())
        return;
    cancelForcePush();  // đóng trạng thái xác nhận trước khi chạy
    const GitService svc = *m_service;
    mutate([svc, sha] { return svc.forcePush(sha); }, tr("Đang force push…"),
           [this](const GitResult &) { setNotice(tr("Đã force push.")); });
}

void RepoController::cancelForcePush()
{
    ++m_prepareGen;
    m_forcePushInfo.clear();
    emit forcePushInfoChanged();
}

void RepoController::continueOperation()
{
    if (!m_service || busy() || !canContinueOperation())
        return;
    const GitService svc = *m_service;
    const OperationKind op = m_operation;
    mutate([svc, op] { return svc.continueOperation(op); }, tr("Đang hoàn tất…"),
           [this](const GitResult &) { setNotice(tr("Đã hoàn tất thao tác.")); });
}

void RepoController::abortOperation()
{
    if (!m_service || busy() || m_operation == OperationKind::None)
        return;
    const GitService svc = *m_service;
    const OperationKind op = m_operation;
    mutate([svc, op] { return svc.abortOperation(op); }, tr("Đang huỷ…"),
           [this](const GitResult &) { setNotice(tr("Đã huỷ thao tác, repo trở về trạng thái trước đó.")); });
}

QStringList RepoController::gitignoreTemplates() const
{
    return GitIgnoreTemplates::names();
}

QString RepoController::homeDir() const
{
    return QDir::homePath();
}

QVariantMap RepoController::splitMessage(const QString &message)
{
    QStringList lines = message.trimmed().split(QLatin1Char('\n'));
    QStringList trailers;
    while (!lines.isEmpty()) {  // các dòng Co-authored-by ở cuối message
        const QString l = lines.last().trimmed();
        if (l.isEmpty()) {
            lines.removeLast();
        } else if (l.startsWith(QStringLiteral("Co-authored-by:"), Qt::CaseInsensitive)) {
            trailers.prepend(l.mid(static_cast<int>(QStringLiteral("Co-authored-by:").size())).trimmed());
            lines.removeLast();
        } else {
            break;
        }
    }
    return {{QStringLiteral("subject"), lines.value(0).trimmed()},
            {QStringLiteral("body"), lines.mid(1).join(QLatin1Char('\n')).trimmed()},
            {QStringLiteral("coAuthors"), trailers.join(QStringLiteral(", "))}};
}

QVariantMap RepoController::lastCommitParts() const
{
    return m_head ? splitMessage(m_head->message) : QVariantMap{};
}

void RepoController::undoLastCommit()
{
    if (!m_service || busy() || !canUndoCommit()) {
        if (m_service && !busy())
            setError(tr("Không thể hoàn tác commit gần nhất: nó đã được push, là merge commit hoặc là commit gốc."));
        return;
    }
    const QVariantMap parts = lastCommitParts();
    const GitService svc = *m_service;
    mutate([svc] { return svc.undoLastCommit(); }, tr("Đang hoàn tác commit…"), [this, parts](const GitResult &) {
        emit commitUndone(parts.value(QStringLiteral("subject")).toString(),
                          parts.value(QStringLiteral("body")).toString(),
                          parts.value(QStringLiteral("coAuthors")).toString());
        setNotice(tr("Đã hoàn tác commit — thay đổi quay lại danh sách."));
    });
}

void RepoController::discardFiles(const QStringList &paths)
{
    if (!m_service || busy() || paths.isEmpty())
        return;
    QVector<FileChange> files;
    for (const QString &p : paths) {
        const int row = m_changes.rowOfPath(p);
        if (row >= 0)
            files.append(m_changes.at(row));
    }
    if (files.isEmpty())
        return;
    const GitService svc = *m_service;
    const auto id = std::make_shared<QString>();
    const int n = static_cast<int>(files.size());
    mutate([svc, files, id] { return svc.discardFiles(files, id.get()); }, tr("Đang bỏ thay đổi…"),
           [this, id, n](const GitResult &) {
               const QString text = n == 1 ? tr("Đã bỏ thay đổi của 1 file") : tr("Đã bỏ thay đổi của %1 file").arg(n);
               m_lastDiscardId = *id;
               m_lastDiscardText = text;
               emit discardStateChanged();
               setNotice(text);
           });
}

void RepoController::discardAll()
{
    QStringList paths;
    for (int i = 0; i < m_changes.rowCount(); ++i)
        if (m_changes.at(i).kind != ChangeKind::Unmerged)
            paths << m_changes.at(i).path;
    discardFiles(paths);
}

void RepoController::discardDiffRow(int row)
{
    if (!m_service || busy() || m_selectedPath.isEmpty() || !m_diff.selectable())
        return;
    const LineSelection sel = m_diff.linesForRow(row);
    if (sel.isEmpty())
        return;
    const GitService svc = *m_service;
    const QString path = m_selectedPath;
    const FileDiff diff = m_diff.fileDiff();
    const auto id = std::make_shared<QString>();
    mutate([svc, path, diff, sel, id] { return svc.discardLines(path, diff, sel, id.get()); },
           tr("Đang bỏ dòng…"), [this, id](const GitResult &) {
               m_lastDiscardId = *id;
               m_lastDiscardText = tr("Đã bỏ các dòng được chọn");
               emit discardStateChanged();
               setNotice(m_lastDiscardText);
           });
}

void RepoController::undoDiscard()
{
    if (!m_service || busy() || m_lastDiscardId.isEmpty())
        return;
    const GitService svc = *m_service;
    const QString id = m_lastDiscardId;
    mutate([svc, id] { return svc.undoDiscard(id); }, tr("Đang hoàn tác…"), [this](const GitResult &) {
        m_lastDiscardId.clear();
        m_lastDiscardText.clear();
        emit discardStateChanged();
        setNotice(tr("Đã hoàn tác việc bỏ thay đổi."));
    });
}

namespace {
// Escape ký tự đặc biệt của mẫu .gitignore trong tên file.
QString escapeIgnore(const QString &name)
{
    QString out;
    for (int i = 0; i < name.size(); ++i) {
        const QChar c = name.at(i);
        if (QStringLiteral("*?[]\\").contains(c) || (i == 0 && (c == QLatin1Char('#') || c == QLatin1Char('!'))))
            out += QLatin1Char('\\');
        out += c;
    }
    return out;
}
} // namespace

void RepoController::ignoreFile(const QString &path)
{
    if (!m_service || busy() || path.isEmpty())
        return;
    const QString pattern = QLatin1Char('/') + escapeIgnore(path);
    const GitService svc = *m_service;
    mutate([svc, pattern] { return svc.addIgnorePattern(pattern); }, tr("Đang cập nhật .gitignore…"),
           [this, pattern](const GitResult &) { setNotice(tr("Đã thêm \"%1\" vào .gitignore.").arg(pattern)); });
}

void RepoController::ignoreExtension(const QString &path)
{
    if (!m_service || busy())
        return;
    const QString suffix = QFileInfo(path).suffix();
    if (suffix.isEmpty()) {
        setError(tr("File này không có phần mở rộng để bỏ qua theo đuôi."));
        return;
    }
    const QString pattern = QStringLiteral("*.") + escapeIgnore(suffix);
    const GitService svc = *m_service;
    mutate([svc, pattern] { return svc.addIgnorePattern(pattern); }, tr("Đang cập nhật .gitignore…"),
           [this, pattern](const GitResult &) { setNotice(tr("Đã thêm \"%1\" vào .gitignore.").arg(pattern)); });
}

void RepoController::ignoreFolder(const QString &path)
{
    if (!m_service || busy())
        return;
    const QString dir = QFileInfo(path).path();
    if (dir.isEmpty() || dir == QLatin1String(".")) {
        setError(tr("File nằm ở thư mục gốc nên không có thư mục nào để bỏ qua."));
        return;
    }
    const QString pattern = QLatin1Char('/') + escapeIgnore(dir) + QLatin1Char('/');
    const GitService svc = *m_service;
    mutate([svc, pattern] { return svc.addIgnorePattern(pattern); }, tr("Đang cập nhật .gitignore…"),
           [this, pattern](const GitResult &) { setNotice(tr("Đã thêm \"%1\" vào .gitignore.").arg(pattern)); });
}

void RepoController::openGitignore()
{
    if (!m_service)
        return;
    const QString file = m_service->repoPath() + QStringLiteral("/.gitignore");
    if (!QFileInfo::exists(file)) {  // tạo file rỗng để editor mở được
        QFile f(file);
        if (!f.open(QIODevice::WriteOnly)) {
            setError(tr("Không tạo được .gitignore: %1").arg(f.errorString()));
            return;
        }
    }
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(file)))
        setError(tr("Không mở được .gitignore bằng ứng dụng mặc định. Hãy mở thủ công:\n%1").arg(file));
}

void RepoController::createRepository(const QString &parentDir, const QString &name, const QString &branch,
                                      bool withReadme, const QString &gitignoreTemplate)
{
    if (m_creating)
        return;
    m_creating = true;
    emit creatingChanged();
    runAsync<InitResult>(
        this,
        [parentDir, name, branch, withReadme, gitignoreTemplate] {
            return GitService::initRepository(parentDir, name, branch, withReadme, gitignoreTemplate);
        },
        [this](const InitResult &r) {
            m_creating = false;
            emit creatingChanged();
            if (!r.git.ok()) {
                setError(r.git.err.trimmed());
                return;
            }
            if (!r.committed && !r.warning.isEmpty())
                m_pendingWarning = tr("Đã tạo repository nhưng chưa tạo được commit đầu tiên:\n%1").arg(r.warning);
            openRepo(r.path);
        });
}

void RepoController::clearError()
{
    setError({});
}

void RepoController::setError(const QString &message)
{
    if (m_error == message)
        return;
    m_error = message;
    emit errorMessageChanged();
}

void RepoController::setSelectedPath(const QString &path)
{
    if (m_selectedPath == path)
        return;
    m_selectedPath = path;
    emit selectedPathChanged();
}

void RepoController::beginTask()
{
    if (m_pending++ == 0)
        emit busyChanged();
}

void RepoController::endTask()
{
    if (--m_pending == 0) {
        if (!m_activity.isEmpty()) {
            m_activity.clear();
            emit activityChanged();
        }
        emit busyChanged();
    }
}

} // namespace gitapp
