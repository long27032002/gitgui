#pragma once

#include <QObject>
#include <QProcess>
#include <QString>

namespace gitapp {

// Clone repository từ URL bằng `git clone --progress`, chạy bất đồng bộ để hiển thị tiến độ và cho
// phép huỷ. Khi huỷ hoặc lỗi, thư mục dang dở do chính lần clone này tạo ra sẽ bị dọn.
class CloneController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(int percent READ percent NOTIFY progressChanged)      // -1: chưa biết
    Q_PROPERTY(QString stage READ stage NOTIFY progressChanged)      // vd "Đang tải về"
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorChanged)

public:
    explicit CloneController(QObject *parent = nullptr);

    [[nodiscard]] bool running() const { return m_proc != nullptr; }
    [[nodiscard]] int percent() const { return m_percent; }
    [[nodiscard]] const QString &stage() const { return m_stage; }
    [[nodiscard]] const QString &errorMessage() const { return m_error; }

    // Chương trình git sẽ chạy (mặc định "git"); đổi để kiểm thử.
    void setGitProgram(const QString &program) { m_program = program; }

    // "git@github.com:user/repo.git" -> "repo"
    Q_INVOKABLE QString suggestName(const QString &url) const;
    Q_INVOKABLE void start(const QString &url, const QString &parentDir, const QString &name);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void clearError();

signals:
    void runningChanged();
    void progressChanged();
    void errorChanged();
    void finished(const QString &path);  // clone thành công; `path` là thư mục repo
    void cancelled();

private:
    void setError(const QString &message);
    void onStderr();
    void onFinished(int exitCode, QProcess::ExitStatus status);
    void cleanupPartial();
    void stopProcess();

    QProcess *m_proc = nullptr;
    QString m_program = QStringLiteral("git");
    QString m_target;
    bool m_createdTarget = false;  // thư mục đích chưa tồn tại trước khi clone
    bool m_cancelled = false;
    int m_percent = -1;
    QString m_stage;
    QString m_error;
    QByteArray m_pending;   // phần dòng stderr chưa hoàn chỉnh
    QStringList m_errorLines;
};

} // namespace gitapp
