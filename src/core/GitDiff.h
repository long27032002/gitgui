#pragma once

#include <QByteArray>
#include <QSet>
#include <QVector>

namespace gitcore {

enum class DiffLineKind { Context, Add, Remove, NoNewline };

// Dữ liệu diff được giữ ở dạng byte thô (không qua QString) để việc stage từng dòng
// không bao giờ làm hỏng nội dung file không phải UTF-8.
struct DiffLine {
    DiffLineKind kind = DiffLineKind::Context;
    QByteArray raw;  // nguyên dòng gồm ký tự đầu (' ', '+', '-', '\\'), không có '\n'

    [[nodiscard]] bool isChange() const
    {
        return kind == DiffLineKind::Add || kind == DiffLineKind::Remove;
    }
};

struct DiffHunk {
    QByteArray header;   // dòng "@@ -a,b +c,d @@ ..." gốc
    QByteArray heading;  // phần sau "@@" thứ hai (tên hàm...), giữ nguyên
    int oldStart = 0, oldCount = 0, newStart = 0, newCount = 0;
    QVector<DiffLine> lines;

    [[nodiscard]] bool hasChanges() const
    {
        for (const DiffLine &l : lines)
            if (l.isChange())
                return true;
        return false;
    }
};

struct FileDiff {
    QVector<QByteArray> headerLines;  // các dòng trước hunk đầu tiên
    QVector<DiffHunk> hunks;
    bool supported = true;  // false nếu gặp định dạng không hiểu (vd: diff gộp khi conflict)
};

// Một dòng được chọn được định danh bằng (chỉ số hunk, chỉ số dòng trong hunk).
using LineSelection = QSet<quint64>;
[[nodiscard]] inline quint64 lineKey(int hunk, int line)
{
    return (static_cast<quint64>(static_cast<quint32>(hunk)) << 32) | static_cast<quint32>(line);
}

enum class PatchMode {
    Stage,     // patch áp vào index: đưa các dòng đã chọn từ working tree vào index
    Unstage,   // patch áp ngược vào index: bỏ các dòng đã chọn khỏi index
    StageNew,  // như Stage nhưng cho file chưa có trong index: giữ nguyên header "new file"
};

class GitDiff {
public:
    // Parse đầu ra `git diff` của MỘT file.
    [[nodiscard]] static FileDiff parse(const QByteArray &text);

    // Diff tổng hợp cho một file mới (chưa theo dõi): toàn bộ nội dung là các dòng thêm vào.
    // File nhị phân (có byte NUL) trả về diff không có hunk.
    [[nodiscard]] static FileDiff forNewFile(const QString &path, const QByteArray &content,
                                             bool executable = false);

    // Khoá định danh bền vững của từng dòng thay đổi, theo nội dung chứ không theo vị trí:
    // [hunk][dòng] -> "<dòng thô>\x1f<lần xuất hiện thứ n của dòng giống hệt>"; dòng không đổi -> rỗng.
    // Nhờ vậy lựa chọn "loại dòng này" vẫn đúng khi file được sửa tiếp và các dòng dịch chỗ.
    [[nodiscard]] static QVector<QVector<QByteArray>> lineKeys(const FileDiff &diff);
    // Mọi dòng thay đổi trừ những dòng nằm trong `excluded`.
    [[nodiscard]] static LineSelection includedLines(const FileDiff &diff, const QSet<QByteArray> &excluded);
    // Ngược lại của includedLines: khoá của các dòng thay đổi KHÔNG nằm trong `included`.
    [[nodiscard]] static QSet<QByteArray> excludedKeys(const FileDiff &diff, const LineSelection &included);
    [[nodiscard]] static int changeLineCount(const FileDiff &diff);

    // Sinh patch chỉ chứa các dòng được chọn (giống `git add -p` khi sửa hunk).
    // Với Unstage, patch phải được áp bằng `git apply --cached --reverse`.
    // Trả về rỗng nếu không có dòng thay đổi nào được chọn.
    [[nodiscard]] static QByteArray buildPatch(const FileDiff &diff, const LineSelection &selection,
                                               PatchMode mode);
};

} // namespace gitcore
