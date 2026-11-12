#include "CloneController.h"

#include <GitRunner.h>

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTimer>

#ifdef Q_OS_UNIX
#include <csignal>
#include <sys/types.h>
#endif

namespace gitapp {

CloneController::CloneController(QObject *parent) : QObject(parent) {}

QString CloneController::suggestName(const QString &url) const
{
    QString u = url.trimmed();
    while (u.endsWith(QLatin1Char('/')) || u.endsWith(QLatin1Char('\\')))
        u.chop(1);
    if (u.endsWith(QStringLiteral(".git")))
        u.chop(4);
    const qsizetype cut = std::max({u.lastIndexOf(QLatin1Char('/')), u.lastIndexOf(QLatin1Char('\\')),
                                    u.lastIndexOf(QLatin1Char(':'))});
    return u.mid(cut + 1);
}

void CloneController::setError(const QString &message)
{
    if (m_error == message)
        return;
    m_error = message;
    emit errorChanged();
}

void CloneController::clearError()
{
    setError({});
}

void CloneController::start(const QString &url, const QString &parentDir, const QString &name)
{
    if (m_proc)
        return;
    const QString u = url.trimmed();
    const QString n = name.trimmed();
    if (u.isEmpty()) {
        setError(tr("Hãy nhập URL của repository cần clone."));
        return;
    }
    if (n.isEmpty() || n == QLatin1String(".") || n == QLatin1String("..") || n.contains(QLatin1Char('/'))
        || n.contains(QLatin1Char('\\'))) {
        setError(tr("Tên thư mục đích không hợp lệ."));
        return;
    }
    if (!QFileInfo(parentDir).isDir()) {
        setError(tr("Thư mục chứa \"%1\" không tồn tại.").arg(parentDir));
        return;
    }
    m_target = QDir(parentDir).filePath(n);
    if (QFileInfo::exists(m_target) && !QDir(m_target).isEmpty()) {
        setError(tr("Thư mục \"%1\" đã tồn tại và không trống.").arg(m_target));
        return;
    }
    m_createdTarget = !QFileInfo::exists(m_target);

    setError({});
    m_cancelled = false;
    m_percent = -1;
    m_stage = tr("Đang kết nối…");
    m_pending.clear();
    m_errorLines.clear();
    emit progressChanged();

    m_proc = new QProcess(this);
    gitcore::GitRunner::configureProcess(*m_proc, parentDir);
    connect(m_proc, &QProcess::readyReadStandardError, this, &CloneController::onStderr);
    connect(m_proc, &QProcess::finished, this, &CloneController::onFinished);
    connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            setError(tr("Không chạy được git. Hãy kiểm tra git đã được cài đặt."));
            m_proc->deleteLater();
            m_proc = nullptr;
            emit runningChanged();
        }
    });
    m_proc->start(m_program, {QStringLiteral("clone"), QStringLiteral("--progress"), QStringLiteral("--"), u,
                              m_target});
    emit runningChanged();
}

void CloneController::cancel()
{
    if (!m_proc)
        return;
    m_cancelled = true;
    if (m_proc->state() == QProcess::Starting) {  // chưa có pid: đợi chạy xong rồi dừng
        connect(m_proc, &QProcess::started, this, [this] { stopProcess(); });
        return;
    }
    stopProcess();
}

void CloneController::stopProcess()
{
    if (!m_proc)
        return;
#ifdef Q_OS_UNIX
    // Tiến trình con là trưởng nhóm tiến trình riêng (setsid): gửi tín hiệu cho cả nhóm để dừng luôn
    // các tiến trình phụ như git-remote-https hay ssh.
    if (m_proc->processId() > 0)
        ::kill(-static_cast<pid_t>(m_proc->processId()), SIGTERM);
    else
        m_proc->terminate();
#else
    m_proc->terminate();
#endif
    QProcess *p = m_proc;
    QTimer::singleShot(3000, this, [p] {
        if (p->state() != QProcess::NotRunning)
            p->kill();
    });
}

void CloneController::onStderr()
{
    // Git cập nhật dòng tiến độ bằng '\r', nên cắt theo cả '\r' lẫn '\n'.
    m_pending += m_proc->readAllStandardError();
    static const QRegularExpression pct(QStringLiteral(R"((\d+)%)"));
    qsizetype start = 0;
    for (qsizetype i = 0; i < m_pending.size(); ++i) {
        if (m_pending.at(i) != '\r' && m_pending.at(i) != '\n')
            continue;
        const QString line = QString::fromUtf8(m_pending.mid(start, i - start)).trimmed();
        start = i + 1;
        if (line.isEmpty())
            continue;
        const auto m = pct.match(line);
        if (!m.hasMatch()) {
            m_errorLines << line;
            if (m_errorLines.size() > 20)
                m_errorLines.removeFirst();
            continue;
        }
        m_percent = m.captured(1).toInt();
        if (line.startsWith(QStringLiteral("Receiving objects")))
            m_stage = tr("Đang tải về");
        else if (line.startsWith(QStringLiteral("Resolving deltas")))
            m_stage = tr("Đang giải quyết delta");
        else if (line.contains(QStringLiteral("Counting objects")))
            m_stage = tr("Máy chủ đang đếm đối tượng");
        else if (line.contains(QStringLiteral("Compressing objects")))
            m_stage = tr("Máy chủ đang nén đối tượng");
        else if (line.startsWith(QStringLiteral("Updating files")) || line.startsWith(QStringLiteral("Checking out")))
            m_stage = tr("Đang tạo file");
        else
            m_stage = line;
        emit progressChanged();
    }
    m_pending = m_pending.mid(start);
}

void CloneController::cleanupPartial()
{
    // Chỉ dọn thư mục do chính lần clone này tạo; không đụng vào thư mục có sẵn của người dùng.
    if (m_createdTarget && QFileInfo::exists(m_target))
        QDir(m_target).removeRecursively();
}

void CloneController::onFinished(int exitCode, QProcess::ExitStatus status)
{
    onStderr();
    m_proc->deleteLater();
    m_proc = nullptr;
    emit runningChanged();

    if (m_cancelled) {
        cleanupPartial();
        m_percent = -1;
        m_stage.clear();
        emit progressChanged();
        emit cancelled();
        return;
    }
    if (status == QProcess::NormalExit && exitCode == 0) {
        m_percent = 100;
        emit progressChanged();
        emit finished(m_target);
        return;
    }
    cleanupPartial();
    setError(m_errorLines.isEmpty() ? tr("git clone thất bại (mã %1).").arg(exitCode)
                                    : m_errorLines.join(QLatin1Char('\n')));
}

} // namespace gitapp
