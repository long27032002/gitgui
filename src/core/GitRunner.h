#pragma once

#include "GitTypes.h"

#include <QStringList>
#include <atomic>

class QProcess;

namespace gitcore {

// Chạy `git` đồng bộ. Được thiết kế để gọi từ luồng worker
// (QtConcurrent) — không bao giờ gọi trực tiếp trên luồng giao diện.
class GitRunner {
public:
    static constexpr int kDefaultTimeoutMs = 30'000;

    // `stdinData` (nếu có) được ghi vào stdin rồi đóng lại.
    [[nodiscard]] static GitResult run(const QString &workDir,
                                       const QStringList &args,
                                       const QByteArray &stdinData = {},
                                       int timeoutMs = kDefaultTimeoutMs,
                                       const std::atomic_bool *cancel = nullptr);

    // Đặt môi trường chuẩn cho một tiến trình git bất kỳ (kể cả chạy bất đồng bộ như clone):
    // output ổn định, không hỏi mật khẩu qua terminal, tách khỏi terminal điều khiển.
    static void configureProcess(QProcess &proc, const QString &workDir);
};

} // namespace gitcore
