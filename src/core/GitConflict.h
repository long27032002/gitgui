#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

namespace gitcore {

// Cách xử lý một khối xung đột.
enum class ConflictResolution {
    Unresolved,    // giữ nguyên dấu xung đột
    Ours,          // phần giữa "<<<<<<<" và "======="
    Theirs,        // phần giữa "=======" và ">>>>>>>"
    Both,          // Ours rồi đến Theirs
    BothReversed,  // Theirs rồi đến Ours
};

struct ConflictBlock {
    // Các dòng dấu, gồm cả ký tự xuống dòng gốc (giữ nguyên CRLF).
    QByteArray startMarker, baseMarker, midMarker, endMarker;
    QByteArray ours, base, theirs;
    bool hasBase = false;  // kiểu diff3 (có phần "|||||||")
};

struct ConflictSegment {
    bool isConflict = false;
    QByteArray text;       // khi !isConflict
    ConflictBlock block;   // khi isConflict
};

struct ConflictFile {
    QVector<ConflictSegment> segments;
    bool supported = true;  // false nếu dấu xung đột lồng nhau/thiếu cặp — không dám tự sửa

    [[nodiscard]] int conflictCount() const
    {
        int n = 0;
        for (const ConflictSegment &s : segments)
            n += s.isConflict ? 1 : 0;
        return n;
    }
};

// Làm việc hoàn toàn trên byte để không làm hỏng file không phải UTF-8 hay CRLF.
class GitConflict {
public:
    [[nodiscard]] static ConflictFile parse(const QByteArray &content);

    // `resolutions` có đúng conflictCount() phần tử, theo thứ tự xuất hiện trong file.
    [[nodiscard]] static QByteArray resolve(const ConflictFile &file,
                                            const QVector<ConflictResolution> &resolutions);

    // Tên sau dấu "<<<<<<< " / ">>>>>>> " (vd "HEAD", "origin/main"), rỗng nếu không có.
    [[nodiscard]] static QString markerLabel(const QByteArray &markerLine);
};

} // namespace gitcore
