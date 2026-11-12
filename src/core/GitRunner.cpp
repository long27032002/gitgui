#include "GitRunner.h"

#include <QProcess>
#include <QProcessEnvironment>
#include <QElapsedTimer>

#ifdef Q_OS_UNIX
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#endif

namespace gitcore {

void GitRunner::configureProcess(QProcess &proc, const QString &workDir)
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));            // output ổn định
    env.insert(QStringLiteral("GIT_TERMINAL_PROMPT"), QStringLiteral("0")); // không chờ nhập tay
    env.insert(QStringLiteral("GIT_OPTIONAL_LOCKS"), QStringLiteral("0"));  // status không tranh khoá
    proc.setProcessEnvironment(env);
    proc.setWorkingDirectory(workDir);
#ifdef Q_OS_UNIX
    // Tách khỏi terminal điều khiển: ssh/gpg không thể hỏi passphrase hay xác nhận host key
    // qua /dev/tty (sẽ treo), mà báo lỗi ngay. Khoá nạp sẵn trong ssh-agent vẫn dùng được.
    proc.setChildProcessModifier([] { ::setsid(); });
#endif
}

GitResult GitRunner::run(const QString &workDir, const QStringList &args,
                         const QByteArray &stdinData, int timeoutMs, const std::atomic_bool *cancel)
{
    GitResult result;

    QProcess proc;
    configureProcess(proc, workDir);
    proc.start(QStringLiteral("git"), args);

    if (!proc.waitForStarted(5000)) {
        result.err = proc.errorString();
        return result;
    }
    result.started = true;

    if (!stdinData.isEmpty())
        proc.write(stdinData);
    proc.closeWriteChannel();

    QElapsedTimer timer;
    timer.start();
    while (proc.state() != QProcess::NotRunning && timer.elapsed() < timeoutMs) {
        if (cancel && cancel->load()) {
            result.cancelled = true;
#ifdef Q_OS_UNIX
            const pid_t pgid = static_cast<pid_t>(proc.processId());
            if (pgid > 0)
                ::kill(-pgid, SIGTERM);
#else
            proc.terminate();
#endif
            if (!proc.waitForFinished(1500)) {
#ifdef Q_OS_UNIX
                if (pgid > 0)
                    ::kill(-pgid, SIGKILL);
#else
                proc.kill();
#endif
                proc.waitForFinished(1500);
            }
            break;
        }
        proc.waitForFinished(100);
    }
    if (proc.state() != QProcess::NotRunning && !result.cancelled) {
        result.timedOut = true;
        proc.kill();
        proc.waitForFinished(2000);
    }

    result.exitCode = proc.exitCode();
    result.out = proc.readAllStandardOutput();
    result.err = QString::fromUtf8(proc.readAllStandardError());
    return result;
}

} // namespace gitcore
