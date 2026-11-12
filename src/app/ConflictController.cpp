#include "ConflictController.h"

#include "AsyncTask.h"

#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QUrl>

#include <algorithm>

using namespace gitcore;

namespace gitapp {

namespace {

constexpr qint64 kMaxConflictBytes = 5 * 1024 * 1024;

struct LoadResult {
    QByteArray content;
    bool readOk = false;
    QString error;
    ConflictFile parsed;
    bool binary = false;
    bool tooLarge = false;
    QDateTime mtime;
    qint64 size = -1;
};

LoadResult readConflictFile(const QString &repoPath, const QString &path)
{
    LoadResult r;
    const QString full = repoPath + QLatin1Char('/') + path;
    QFile f(full);
    if (!f.exists()) {
        r.readOk = true;  // file đã bị xoá ở working tree (vd conflict kiểu xoá/sửa)
        return r;
    }
    if (!f.open(QIODevice::ReadOnly)) {
        r.error = f.errorString();
        return r;
    }
    const QFileInfo info(full);
    r.mtime = info.lastModified();
    r.size = info.size();
    if (r.size > kMaxConflictBytes) {
        r.tooLarge = true;
        r.readOk = true;
        return r;
    }
    r.content = f.readAll();
    r.readOk = true;
    r.binary = r.content.left(8000).contains('\0');
    if (!r.binary)
        r.parsed = GitConflict::parse(r.content);
    return r;
}

// Vài dòng cuối của đoạn văn bản đứng trước / vài dòng đầu của đoạn đứng sau khối xung đột.
QString lastLines(const QByteArray &text, int n)
{
    QList<QByteArray> lines = text.split('\n');
    if (!lines.isEmpty() && lines.last().isEmpty())
        lines.removeLast();
    if (lines.size() > n)
        lines = lines.mid(lines.size() - n);
    return QString::fromUtf8(lines.join('\n'));
}

QString firstLines(const QByteArray &text, int n)
{
    QList<QByteArray> lines = text.split('\n');
    if (!lines.isEmpty() && lines.last().isEmpty())
        lines.removeLast();
    return QString::fromUtf8(lines.mid(0, n).join('\n'));
}

QString trimTrailingNewline(QString s)
{
    while (s.endsWith(QLatin1Char('\n')) || s.endsWith(QLatin1Char('\r')))
        s.chop(1);
    return s;
}

} // namespace

ConflictController::ConflictController(QObject *parent)
    : QObject(parent), m_files(this), m_blocks(this)
{
}

void ConflictController::setState(std::optional<GitService> service, OperationKind op,
                                  QVector<FileChange> unmerged)
{
    const bool sameRepo = m_service && service && m_service->repoPath() == service->repoPath();
    m_service = std::move(service);
    m_op = op;
    m_unmerged = std::move(unmerged);
    std::sort(m_unmerged.begin(), m_unmerged.end(),
              [](const FileChange &a, const FileChange &b) { return a.path < b.path; });

    QVector<ConflictFilesModel::Row> rows;
    for (const FileChange &fc : std::as_const(m_unmerged)) {
        const bool oursHas = fc.indexStatus != QLatin1Char('D') && !(fc.indexStatus == QLatin1Char('U') && fc.worktreeStatus == QLatin1Char('A'));
        const bool theirsHas = fc.worktreeStatus != QLatin1Char('D') && !(fc.indexStatus == QLatin1Char('A') && fc.worktreeStatus == QLatin1Char('U'));
        const bool mineHas = mineIsOurs() ? oursHas : theirsHas;
        const bool incomingHas = mineIsOurs() ? theirsHas : oursHas;
        QString desc;
        if (mineHas && incomingHas)
            desc = fc.indexStatus == QLatin1Char('A') ? tr("Cả hai bên cùng thêm file này")
                                                       : tr("Cả hai bên cùng sửa file này");
        else if (!mineHas && incomingHas)
            desc = tr("Bạn đã xoá file này nhưng bên kia đã sửa nó");
        else if (mineHas && !incomingHas)
            desc = tr("Bạn đã sửa file này nhưng bên kia đã xoá nó");
        else
            desc = tr("Cả hai bên đều đã xoá file này");
        rows.append({fc.path, desc});
    }
    m_files.setRows(std::move(rows));

    if (m_files.rowCount() == 0) {
        clearFile();
        return;
    }
    const int keep = m_files.rowOfPath(m_selectedPath);
    if (keep >= 0 && sameRepo)
        return;  // giữ nguyên file đang xem và các lựa chọn chưa lưu
    loadFile(m_files.at(keep >= 0 ? keep : 0).path);
}

void ConflictController::clearFile()
{
    ++m_loadGen;
    const bool had = !m_selectedPath.isEmpty();
    m_selectedPath.clear();
    m_description.clear();
    m_mineAction.clear();
    m_incomingAction.clear();
    m_file = {};
    m_binary = m_hasMarkers = m_loading = false;
    m_blocks.setBlocks({});
    if (had) {
        emit fileChanged();
        emit choicesChanged();
    }
}

void ConflictController::select(int row)
{
    if (row < 0 || row >= m_files.rowCount())
        return;
    const QString path = m_files.at(row).path;
    if (path == m_selectedPath)
        return;
    loadFile(path);
}

void ConflictController::reload()
{
    if (!m_selectedPath.isEmpty())
        loadFile(m_selectedPath);
}

void ConflictController::reloadIfChanged()
{
    if (!m_service || m_selectedPath.isEmpty() || m_loading)
        return;
    const QFileInfo info(m_service->repoPath() + QLatin1Char('/') + m_selectedPath);
    if (info.lastModified() != m_loadedMtime || info.size() != m_loadedSize)
        loadFile(m_selectedPath);
}

void ConflictController::loadFile(const QString &path)
{
    if (!m_service)
        return;
    const FileChange *fc = nullptr;
    for (const FileChange &c : std::as_const(m_unmerged))
        if (c.path == path)
            fc = &c;
    if (!fc)
        return;

    const bool oursHas = fc->indexStatus != QLatin1Char('D') && !(fc->indexStatus == QLatin1Char('U') && fc->worktreeStatus == QLatin1Char('A'));
    const bool theirsHas = fc->worktreeStatus != QLatin1Char('D') && !(fc->indexStatus == QLatin1Char('A') && fc->worktreeStatus == QLatin1Char('U'));
    m_mineHasFile = mineIsOurs() ? oursHas : theirsHas;
    m_incomingHasFile = mineIsOurs() ? theirsHas : oursHas;

    m_selectedPath = path;
    m_description = m_files.at(std::max(0, m_files.rowOfPath(path))).description;
    m_mineAction = m_mineHasFile ? tr("Dùng toàn bộ bản của tôi") : tr("Xoá file (theo bản của tôi)");
    m_incomingAction = m_incomingHasFile ? tr("Dùng toàn bộ bản của họ") : tr("Xoá file (theo bản của họ)");
    m_loading = true;
    m_blocks.setBlocks({});
    m_binary = m_hasMarkers = false;
    emit fileChanged();
    emit choicesChanged();

    const quint64 gen = ++m_loadGen;
    const QString repoPath = m_service->repoPath();
    runAsync<LoadResult>(
        this, [repoPath, path] { return readConflictFile(repoPath, path); },
        [this, gen](const LoadResult &r) {
            if (gen != m_loadGen)
                return;
            m_loading = false;
            m_loadedMtime = r.mtime;
            m_loadedSize = r.size;
            m_binary = r.binary;
            m_file = r.parsed;
            if (!r.readOk)
                emit errorOccurred(tr("Không đọc được tệp %1: %2").arg(m_selectedPath, r.error));

            QVector<ConflictBlocksModel::Row> rows;
            const bool blockMode = r.readOk && !r.binary && !r.tooLarge && m_mineHasFile
                                   && m_incomingHasFile && r.parsed.supported;
            if (blockMode) {
                int number = 0;
                const auto &segs = r.parsed.segments;
                for (qsizetype i = 0; i < segs.size(); ++i) {
                    if (!segs.at(i).isConflict)
                        continue;
                    const ConflictBlock &b = segs.at(i).block;
                    ConflictBlocksModel::Row row;
                    row.number = ++number;
                    const QByteArray &mine = mineIsOurs() ? b.ours : b.theirs;
                    const QByteArray &incoming = mineIsOurs() ? b.theirs : b.ours;
                    row.mine = trimTrailingNewline(QString::fromUtf8(mine));
                    row.incoming = trimTrailingNewline(QString::fromUtf8(incoming));
                    const QString startLabel = GitConflict::markerLabel(b.startMarker);
                    const QString endLabel = GitConflict::markerLabel(b.endMarker);
                    row.mineLabel = mineIsOurs() ? startLabel : endLabel;
                    row.incomingLabel = mineIsOurs() ? endLabel : startLabel;
                    if (i > 0 && !segs.at(i - 1).isConflict)
                        row.before = lastLines(segs.at(i - 1).text, 3);
                    if (i + 1 < segs.size() && !segs.at(i + 1).isConflict)
                        row.after = firstLines(segs.at(i + 1).text, 3);
                    rows.append(row);
                }
            }
            m_hasMarkers = r.parsed.supported ? r.parsed.conflictCount() > 0
                                              : r.content.contains("<<<<<<< ");
            m_blocks.setBlocks(std::move(rows));
            emit fileChanged();
            emit choicesChanged();
        });
}

void ConflictController::choose(int block, int choice)
{
    if (block < 0 || block >= m_blocks.rowCount() || choice < 0 || choice > 3)
        return;
    m_blocks.setChoice(block, choice);
    emit choicesChanged();
}

void ConflictController::applyChoices()
{
    if (!m_service || m_selectedPath.isEmpty() || !allChosen())
        return;
    QVector<ConflictResolution> res;
    for (int i = 0; i < m_blocks.rowCount(); ++i) {
        switch (m_blocks.choiceAt(i)) {
        case 1:
            res.append(mineIsOurs() ? ConflictResolution::Ours : ConflictResolution::Theirs);
            break;
        case 2:
            res.append(mineIsOurs() ? ConflictResolution::Theirs : ConflictResolution::Ours);
            break;
        default:  // 3: giữ cả hai, của tôi trước
            res.append(mineIsOurs() ? ConflictResolution::Both : ConflictResolution::BothReversed);
            break;
        }
    }
    emit resolveContentRequested(m_selectedPath, GitConflict::resolve(m_file, res));
}

void ConflictController::resolveWholeFile(bool mine)
{
    if (m_selectedPath.isEmpty())
        return;
    emit resolveSideRequested(m_selectedPath, mine ? mineIsOurs() : !mineIsOurs());
}

void ConflictController::markResolved()
{
    if (!m_selectedPath.isEmpty())
        emit markResolvedRequested(m_selectedPath);
}

void ConflictController::openInEditor()
{
    if (!m_service || m_selectedPath.isEmpty())
        return;
    const QString full = m_service->repoPath() + QLatin1Char('/') + m_selectedPath;
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(full)))
        emit errorOccurred(tr("Không mở được tệp bằng ứng dụng mặc định. Hãy mở thủ công:\n%1").arg(full));
}

} // namespace gitapp
