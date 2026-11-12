#pragma once

#include <QFutureWatcher>
#include <QObject>
#include <QtConcurrent>

namespace gitapp {

// Chạy `work` ở luồng worker, gọi `done` trên luồng của `ctx` khi xong.
// Nếu `ctx` bị huỷ trước đó thì `done` không còn được gọi.
template <typename R, typename Work, typename Done>
void runAsync(QObject *ctx, Work work, Done done)
{
    auto *watcher = new QFutureWatcher<R>(ctx);
    QObject::connect(watcher, &QFutureWatcherBase::finished, ctx,
                     [watcher, done = std::move(done)] {
                         done(watcher->result());
                         watcher->deleteLater();
                     });
    watcher->setFuture(QtConcurrent::run(std::move(work)));
}

} // namespace gitapp
