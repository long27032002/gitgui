#include "GitDiff.h"

#include <QHash>
#include <QRegularExpression>

namespace gitcore {

namespace {

bool parseHunkHeader(const QByteArray &line, DiffHunk &h)
{
    static const QRegularExpression re(
        QStringLiteral(R"(^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@(.*)$)"));
    // Latin-1 là phép ánh xạ 1-1 từ byte, nên không mất dữ liệu.
    const QRegularExpressionMatch m = re.match(QString::fromLatin1(line));
    if (!m.hasMatch())
        return false;
    h.header = line;
    h.oldStart = m.captured(1).toInt();
    h.oldCount = m.captured(2).isEmpty() ? 1 : m.captured(2).toInt();
    h.newStart = m.captured(3).toInt();
    h.newCount = m.captured(4).isEmpty() ? 1 : m.captured(4).toInt();
    h.heading = m.captured(5).toLatin1();
    return true;
}

// "b/foo" hoặc "\"b/foo\"" -> cùng tên với tiền tố `to` ('a' hoặc 'b').
QByteArray swapPrefix(const QByteArray &name, char from, char to)
{
    QByteArray out = name;
    const qsizetype pos = out.startsWith('"') ? 1 : 0;
    if (out.size() > pos + 1 && out.at(pos) == from && out.at(pos + 1) == '/')
        out[pos] = to;
    return out;
}

// Khi chỉ stage/unstage một phần của file mới thêm hoặc bị xoá, git apply sẽ coi patch
// "new file"/"deleted file" là toàn bộ file. Ta chuyển nó thành patch sửa file thường.
QVector<QByteArray> normalizeHeader(const QVector<QByteArray> &header)
{
    QByteArray minus, plus;
    for (const QByteArray &l : header) {
        if (l.startsWith("--- "))
            minus = l.mid(4);
        else if (l.startsWith("+++ "))
            plus = l.mid(4);
    }

    QVector<QByteArray> out;
    for (const QByteArray &l : header) {
        static const char *const kDrop[] = {"new file mode ", "deleted file mode ", "index ",
                                            "old mode ", "new mode ", "similarity ",
                                            "rename ", "copy ", "dissimilarity "};
        bool drop = false;
        for (const char *p : kDrop)
            drop = drop || l.startsWith(p);
        if (drop)
            continue;
        if (l == "--- /dev/null")
            out << "--- " + swapPrefix(plus, 'b', 'a');
        else if (l == "+++ /dev/null")
            out << "+++ " + swapPrefix(minus, 'a', 'b');
        else
            out << l;
    }
    return out;
}

} // namespace

FileDiff GitDiff::parse(const QByteArray &text)
{
    FileDiff fd;
    const QList<QByteArray> lines = text.split('\n');
    qsizetype n = lines.size();
    if (n > 0 && lines.last().isEmpty())
        --n;  // dấu xuống dòng cuối cùng

    DiffHunk *cur = nullptr;
    for (qsizetype i = 0; i < n; ++i) {
        const QByteArray &l = lines.at(i);
        if (l.startsWith("@@")) {
            DiffHunk h;
            if (!parseHunkHeader(l, h)) {  // vd "@@@" của diff gộp
                fd.supported = false;
                fd.hunks.clear();
                return fd;
            }
            fd.hunks.append(h);
            cur = &fd.hunks.last();
            continue;
        }
        if (!cur) {
            fd.headerLines.append(l);
            continue;
        }
        DiffLine dl;
        dl.raw = l;
        const char c = l.isEmpty() ? ' ' : l.at(0);
        dl.kind = c == '+'    ? DiffLineKind::Add
                  : c == '-'  ? DiffLineKind::Remove
                  : c == '\\' ? DiffLineKind::NoNewline
                              : DiffLineKind::Context;
        cur->lines.append(dl);
    }
    return fd;
}

FileDiff GitDiff::forNewFile(const QString &path, const QByteArray &content, bool executable)
{
    FileDiff fd;
    const QByteArray p = path.toUtf8();
    fd.headerLines << "diff --git a/" + p + " b/" + p
                   << (executable ? QByteArray("new file mode 100755") : QByteArray("new file mode 100644"))
                   << QByteArray("--- /dev/null") << "+++ b/" + p;
    if (content.isEmpty() || content.left(8000).contains('\0'))
        return fd;  // file rỗng hoặc nhị phân: không có dòng nào để chọn

    QList<QByteArray> lines = content.split('\n');
    const bool endsWithNewline = content.endsWith('\n');
    if (endsWithNewline)
        lines.removeLast();
    DiffHunk h;
    h.oldStart = 0;
    h.oldCount = 0;
    h.newStart = 1;
    h.newCount = static_cast<int>(lines.size());
    h.header = "@@ -0,0 +1," + QByteArray::number(h.newCount) + " @@";
    for (const QByteArray &l : lines) {
        DiffLine dl;
        dl.kind = DiffLineKind::Add;
        dl.raw = '+' + l;
        h.lines.append(dl);
    }
    if (!endsWithNewline) {
        DiffLine marker;
        marker.kind = DiffLineKind::NoNewline;
        marker.raw = "\\ No newline at end of file";
        h.lines.append(marker);
    }
    fd.hunks.append(h);
    return fd;
}

QVector<QVector<QByteArray>> GitDiff::lineKeys(const FileDiff &diff)
{
    QVector<QVector<QByteArray>> out;
    QHash<QByteArray, int> seen;  // số lần đã gặp mỗi dòng thô giống hệt (trên toàn file)
    for (const DiffHunk &h : diff.hunks) {
        QVector<QByteArray> keys;
        for (const DiffLine &l : h.lines) {
            if (!l.isChange()) {
                keys.append(QByteArray());
                continue;
            }
            const int n = seen[l.raw]++;
            keys.append(l.raw + '\x1f' + QByteArray::number(n));
        }
        out.append(keys);
    }
    return out;
}

LineSelection GitDiff::includedLines(const FileDiff &diff, const QSet<QByteArray> &excluded)
{
    LineSelection sel;
    const auto keys = lineKeys(diff);
    for (int h = 0; h < keys.size(); ++h)
        for (int l = 0; l < keys[h].size(); ++l)
            if (!keys[h][l].isEmpty() && !excluded.contains(keys[h][l]))
                sel.insert(lineKey(h, l));
    return sel;
}

QSet<QByteArray> GitDiff::excludedKeys(const FileDiff &diff, const LineSelection &included)
{
    QSet<QByteArray> out;
    const auto keys = lineKeys(diff);
    for (int h = 0; h < keys.size(); ++h)
        for (int l = 0; l < keys[h].size(); ++l)
            if (!keys[h][l].isEmpty() && !included.contains(lineKey(h, l)))
                out.insert(keys[h][l]);
    return out;
}

int GitDiff::changeLineCount(const FileDiff &diff)
{
    int n = 0;
    for (const DiffHunk &h : diff.hunks)
        for (const DiffLine &l : h.lines)
            n += l.isChange() ? 1 : 0;
    return n;
}

QByteArray GitDiff::buildPatch(const FileDiff &diff, const LineSelection &selection, PatchMode mode)
{
    const bool stage = mode != PatchMode::Unstage;
    QByteArray hunks;
    int delta = 0;  // tổng (newCount - oldCount) của các hunk đã phát ra

    for (int h = 0; h < diff.hunks.size(); ++h) {
        const DiffHunk &hunk = diff.hunks.at(h);
        QByteArray body;
        int oldCount = 0, newCount = 0;
        bool anySelected = false;
        bool prevKept = false;

        for (int l = 0; l < hunk.lines.size(); ++l) {
            const DiffLine &dl = hunk.lines.at(l);
            const bool selected = selection.contains(lineKey(h, l));
            switch (dl.kind) {
            case DiffLineKind::Context:
                body += dl.raw + '\n';
                ++oldCount;
                ++newCount;
                prevKept = true;
                break;
            case DiffLineKind::Add:
                if (selected) {
                    body += dl.raw + '\n';
                    ++newCount;
                    anySelected = prevKept = true;
                } else if (!stage) {  // đang nằm trong index và vẫn ở lại -> ngữ cảnh
                    body += ' ' + dl.raw.mid(1) + '\n';
                    ++oldCount;
                    ++newCount;
                    prevKept = true;
                } else {
                    prevKept = false;
                }
                break;
            case DiffLineKind::Remove:
                if (selected) {
                    body += dl.raw + '\n';
                    ++oldCount;
                    anySelected = prevKept = true;
                } else if (stage) {  // vẫn còn trong index -> ngữ cảnh
                    body += ' ' + dl.raw.mid(1) + '\n';
                    ++oldCount;
                    ++newCount;
                    prevKept = true;
                } else {
                    prevKept = false;
                }
                break;
            case DiffLineKind::NoNewline:
                if (prevKept)
                    body += dl.raw + '\n';
                break;
            }
        }
        if (!anySelected)
            continue;

        int oldStart = hunk.oldStart;
        int newStart = hunk.newStart;
        if (stage) {
            newStart = hunk.oldStart + delta + (oldCount == 0 ? 1 : 0);
            if (newCount == 0)
                --newStart;
        } else {
            oldStart = hunk.newStart - delta + (newCount == 0 ? 1 : 0);
            if (oldCount == 0)
                --oldStart;
        }
        hunks += "@@ -" + QByteArray::number(oldStart) + ',' + QByteArray::number(oldCount) + " +"
                 + QByteArray::number(newStart) + ',' + QByteArray::number(newCount) + " @@"
                 + hunk.heading + '\n' + body;
        delta += newCount - oldCount;
    }

    if (hunks.isEmpty())
        return {};

    QByteArray patch;
    const QVector<QByteArray> header = mode == PatchMode::StageNew ? diff.headerLines
                                                                   : normalizeHeader(diff.headerLines);
    for (const QByteArray &l : header)
        patch += l + '\n';
    return patch + hunks;
}

} // namespace gitcore
