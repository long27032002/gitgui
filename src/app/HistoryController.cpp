#include "HistoryController.h"

#include "AsyncTask.h"
#include "DiffResult.h"

using namespace gitcore;

namespace gitapp {

namespace {
struct ListResult {
    std::optional<QVector<CommitInfo>> commits;
    QSet<QString> currentBranchHashes;
    QString error;
};
struct DetailResult {
    QString message;
    std::optional<QVector<CommitFile>> files;
};
} // namespace

HistoryController::HistoryController(QObject *parent)
    : QObject(parent), m_commits(this), m_files(this), m_diff(this)
{
}

void HistoryController::setService(std::optional<GitService> service)
{
    m_service = std::move(service);
    clearAll();
}

void HistoryController::clearAll()
{
    ++m_listGen;
    ++m_detailGen;
    ++m_diffGen;
    m_commits.setCommits({});
    m_files.setFiles({});
    m_diff.clear();
    m_selectedHash.clear();
    m_selectedFile.clear();
    m_message.clear();
    m_anchorRow = m_extentRow = -1;
    m_allLoaded = false;
    m_currentBranchHashes.clear();
    setLoading(false);
    emit selectionChanged();
    emit messageChanged();
}

void HistoryController::setLoading(bool on)
{
    if (m_loading == on)
        return;
    m_loading = on;
    emit loadingChanged();
}

void HistoryController::reload()
{
    if (!m_service) {
        clearAll();
        return;
    }
    const quint64 gen = ++m_listGen;
    setLoading(true);
    const GitService svc = *m_service;
    const int page = m_pageSize;
    runAsync<ListResult>(
        this,
        [svc, page] {
            ListResult r;
            r.commits = svc.logAll(0, page, &r.error);
            r.currentBranchHashes = svc.currentBranchHashes();
            return r;
        },
        [this, gen, page](const ListResult &r) {
            if (gen != m_listGen)
                return;
            setLoading(false);
            if (!r.commits) {
                emit errorOccurred(tr("Không đọc được lịch sử:\n%1").arg(r.error.trimmed()));
                return;
            }
            m_currentBranchHashes = r.currentBranchHashes;
            QVector<CommitInfo> commits = *r.commits;
            for (CommitInfo &commit : commits)
                commit.onCurrentBranch = m_currentBranchHashes.contains(commit.hash);
            m_commits.setCommits(std::move(commits));
            m_allLoaded = r.commits->size() < page;
            emit loadingChanged();

            // Giữ commit đang chọn nếu vẫn còn; nếu không chọn commit mới nhất.
            const int keep = m_commits.rowOfHash(m_selectedHash);
            if (keep >= 0) {
                m_anchorRow = m_extentRow = keep;  // hàng có thể đã dịch chỗ; bỏ vùng chọn mở rộng
                emit selectionChanged();
                return;
            }
            if (m_commits.rowCount() > 0) {
                m_selectedHash.clear();
                selectCommit(0);
            } else {
                m_selectedHash.clear();
                m_selectedFile.clear();
                m_anchorRow = m_extentRow = -1;
                m_files.setFiles({});
                m_diff.clear();
                m_message.clear();
                emit selectionChanged();
                emit messageChanged();
            }
        });
}

void HistoryController::loadMore()
{
    if (!m_service || m_loading || m_allLoaded)
        return;
    const quint64 gen = m_listGen;
    setLoading(true);
    const GitService svc = *m_service;
    const int page = m_pageSize;
    const int skip = m_commits.rowCount();
    runAsync<ListResult>(
        this,
        [svc, page, skip] {
            ListResult r;
            r.commits = svc.logAll(skip, page, &r.error);
            return r;
        },
        [this, gen, page](const ListResult &r) {
            if (gen != m_listGen)
                return;
            setLoading(false);
            if (!r.commits) {
                emit errorOccurred(tr("Không đọc được lịch sử:\n%1").arg(r.error.trimmed()));
                return;
            }
            QVector<CommitInfo> commits = *r.commits;
            for (CommitInfo &commit : commits)
                commit.onCurrentBranch = m_currentBranchHashes.contains(commit.hash);
            m_commits.append(commits);
            m_allLoaded = r.commits->size() < page;
            emit loadingChanged();
        });
}

void HistoryController::selectCommit(int row)
{
    if (!m_service || row < 0 || row >= m_commits.rowCount())
        return;
    const QString hash = m_commits.at(row).hash;
    if (hash == m_selectedHash) {
        if (m_anchorRow != row || m_extentRow != row) {  // bấm lại: thu gọn vùng chọn mở rộng
            m_anchorRow = m_extentRow = row;
            emit selectionChanged();
        }
        return;
    }
    m_selectedHash = hash;
    m_anchorRow = m_extentRow = row;
    m_selectedFile.clear();
    ++m_diffGen;
    m_diff.clear();
    emit selectionChanged();

    const quint64 id = ++m_detailGen;
    const GitService svc = *m_service;
    runAsync<DetailResult>(
        this,
        [svc, hash] {
            DetailResult d;
            d.message = svc.commitMessage(hash);
            d.files = svc.commitFiles(hash);
            return d;
        },
        [this, id](const DetailResult &d) {
            if (id != m_detailGen)
                return;
            m_message = d.message;
            emit messageChanged();
            m_files.setFiles(d.files.value_or(QVector<CommitFile>{}));
            if (m_files.rowCount() > 0)
                selectFile(0);  // tự xem file đầu tiên
        });
}

void HistoryController::extendSelection(int row)
{
    if (m_anchorRow < 0 || row < 0 || row >= m_commits.rowCount() || row == m_extentRow)
        return;
    m_extentRow = row;
    emit selectionChanged();
}

bool HistoryController::canSquashSelection() const
{
    const int first = rangeStart();
    const int last = rangeEnd();
    if (rangeSize() < 2 || first < 0 || last >= m_commits.rowCount())
        return false;
    for (int row = first; row <= last; ++row) {
        const CommitInfo &commit = m_commits.at(row);
        if (!commit.onCurrentBranch || commit.parentCount != 1)
            return false;
        if (row < last && commit.firstParent != m_commits.at(row + 1).hash)
            return false;
    }
    return true;
}

void HistoryController::selectFile(int row)
{
    if (!m_service || row < 0 || row >= m_files.rowCount() || m_selectedHash.isEmpty())
        return;
    const CommitFile file = m_files.at(row);
    m_selectedFile = file.path;
    emit selectionChanged();

    const quint64 id = ++m_diffGen;
    const GitService svc = *m_service;
    const QString hash = m_selectedHash;
    runAsync<DiffResult>(
        this,
        [svc, hash, file] {
            return buildDiffResult(svc.commitFileDiff(hash, file.path, file.origPath), false);
        },
        [this, id, file](const DiffResult &res) {
            if (id != m_diffGen)
                return;
            showDiffResult(m_diff, res, file.path);
        });
}

} // namespace gitapp
