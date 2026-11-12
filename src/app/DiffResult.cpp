#include "DiffResult.h"

#include <GitDiff.h>

#include <QObject>

namespace gitapp {

namespace {
constexpr int kMaxDiffLines = 5000;
constexpr qsizetype kMaxDiffBytes = 8 * 1024 * 1024;
} // namespace

QStringList splitDiffLines(const QByteArray &text)
{
    QStringList lines = QString::fromUtf8(text).split(QLatin1Char('\n'));
    if (!lines.isEmpty() && lines.last().isEmpty())
        lines.removeLast();
    if (lines.size() > kMaxDiffLines) {
        lines = lines.mid(0, kMaxDiffLines);
        lines << QObject::tr("… (diff quá dài, đã cắt bớt sau %1 dòng)").arg(kMaxDiffLines);
    }
    return lines;
}

DiffResult buildDiffResult(const gitcore::GitResult &r, bool selectable)
{
    DiffResult res;
    if (!r.ok()) {
        res.plain = {QObject::tr("Không đọc được diff: %1").arg(r.err.trimmed())};
        return res;
    }
    if (r.out.trimmed().isEmpty()) {
        res.plain = {QObject::tr("(không có thay đổi nội dung — có thể chỉ đổi quyền hoặc đổi tên)")};
        return res;
    }
    if (r.out.size() > kMaxDiffBytes) {
        res.plain = {QObject::tr("Diff quá lớn để hiển thị (%1 MiB).").arg(r.out.size() / (1024 * 1024))};
        return res;
    }

    res.diff = gitcore::GitDiff::parse(r.out);
    if (res.diff.supported && !res.diff.hunks.isEmpty()) {
        res.structured = true;
        res.selectable = selectable;
        return res;
    }
    res.plain = splitDiffLines(r.out);  // vd: "Binary files ... differ"
    return res;
}

void showDiffResult(DiffModel &model, const DiffResult &result, const QString &path,
                    const QSet<QByteArray> &excluded)
{
    model.setPath(path);
    if (result.structured)
        model.setFileDiff(result.diff, result.selectable, excluded);
    else
        model.setPlainLines(result.plain);
}

} // namespace gitapp
