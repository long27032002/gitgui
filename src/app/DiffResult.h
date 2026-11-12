#pragma once

#include "DiffModel.h"

#include <GitTypes.h>

#include <QStringList>

namespace gitapp {

// Kết quả đã xử lý của một lệnh diff, sẵn sàng nạp vào DiffModel (tính ở luồng worker).
struct DiffResult {
    gitcore::FileDiff diff;
    QStringList plain;  // dùng khi không có diff có cấu trúc (lỗi, rỗng, nhị phân...)
    bool structured = false;
    bool selectable = false;
};

[[nodiscard]] QStringList splitDiffLines(const QByteArray &text);

// `selectable` chỉ có hiệu lực khi diff parse được và có hunk.
[[nodiscard]] DiffResult buildDiffResult(const gitcore::GitResult &r, bool selectable);

// `path` chọn ngôn ngữ tô màu; `excluded` là các dòng bị loại khỏi commit (chỉ có nghĩa khi chọn được).
void showDiffResult(DiffModel &model, const DiffResult &result, const QString &path = {},
                    const QSet<QByteArray> &excluded = {});

} // namespace gitapp
