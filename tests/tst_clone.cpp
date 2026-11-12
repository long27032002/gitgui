#include <CloneController.h>
#include <GitRunner.h>

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <csignal>

using namespace gitapp;
using namespace gitcore;

class TstClone : public QObject {
    Q_OBJECT
private slots:
    void init();

    void suggestName_fromCommonUrlForms();
    void validation_rejectsBadInputWithoutStarting();
    void clone_localRepository_finishesAndCreatesWorkingCopy();
    void clone_failure_reportsGitErrorAndCleansUp();
    void cancel_stopsProcessGroup_andRemovesPartialDirectory();
    void cancel_neverRemovesAPreExistingEmptyDirectory();

private:
    void makeRemote(QString &remoteOut);  // bare repo có một commit
    QString path() const { return m_dir->path(); }
    void fakeGit(const QString &body, QString &scriptOut);

    std::unique_ptr<QTemporaryDir> m_dir;
};

void TstClone::init()
{
    qputenv("GIT_AUTHOR_NAME", "T");
    qputenv("GIT_AUTHOR_EMAIL", "t@example.com");
    qputenv("GIT_COMMITTER_NAME", "T");
    qputenv("GIT_COMMITTER_EMAIL", "t@example.com");
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
}

void TstClone::makeRemote(QString &remoteOut)
{
    const QString remote = path() + "/remote.git";
    remoteOut = remote;
    const QString work = path() + "/work";
    QDir().mkpath(remote);
    QDir().mkpath(work);
    QVERIFY(GitRunner::run(remote, {"init", "--bare", "-q", "-b", "main"}).ok());
    QVERIFY(GitRunner::run(work, {"init", "-q", "-b", "main"}).ok());
    QFile f(work + "/hello.txt");
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("xin chào\n");
    f.close();
    QVERIFY(GitRunner::run(work, {"add", "-A"}).ok());
    QVERIFY(GitRunner::run(work, {"commit", "-q", "-m", "init"}).ok());
    QVERIFY(GitRunner::run(work, {"remote", "add", "origin", remote}).ok());
    QVERIFY(GitRunner::run(work, {"push", "-q", "-u", "origin", "main"}).ok());
}

// Tạo một "git giả" (script) để kiểm thử huỷ clone mà không cần mạng chậm.
void TstClone::fakeGit(const QString &body, QString &scriptOut)
{
    scriptOut = path() + "/fake-git.sh";
    QFile f(scriptOut);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(("#!/bin/sh\n" + body + "\n").toUtf8());
    f.close();
    QVERIFY(f.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
}

void TstClone::suggestName_fromCommonUrlForms()
{
    CloneController c;
    QCOMPARE(c.suggestName("https://github.com/user/repo.git"), "repo");
    QCOMPARE(c.suggestName("git@github.com:user/my-project.git"), "my-project");
    QCOMPARE(c.suggestName("https://example.com/a/b/c/"), "c");
    QCOMPARE(c.suggestName("/srv/git/local.git"), "local");
    QCOMPARE(c.suggestName("  ssh://host/team/app  "), "app");
    QCOMPARE(c.suggestName(""), "");
}

void TstClone::validation_rejectsBadInputWithoutStarting()
{
    CloneController c;
    c.start("", path(), "x");
    QVERIFY(!c.errorMessage().isEmpty());
    QVERIFY(!c.running());
    c.start("u", path(), "a/b");
    QVERIFY(c.errorMessage().contains("hợp lệ"));
    c.start("u", path() + "/khong-co", "x");
    QVERIFY(c.errorMessage().contains("không tồn tại"));
    QDir().mkpath(path() + "/dang-co");
    QFile f(path() + "/dang-co/f.txt");
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.close();
    c.start("u", path(), "dang-co");
    QVERIFY(c.errorMessage().contains("không trống"));
    QVERIFY(!c.running());
    c.clearError();
    QVERIFY(c.errorMessage().isEmpty());
}

void TstClone::clone_localRepository_finishesAndCreatesWorkingCopy()
{
    QString remote;
    makeRemote(remote);
    CloneController c;
    QSignalSpy finished(&c, &CloneController::finished);
    QSignalSpy running(&c, &CloneController::runningChanged);
    c.start(remote, path(), "ban-sao");
    QVERIFY(c.running());
    QTRY_COMPARE(finished.count(), 1);
    QVERIFY(!c.running());
    QVERIFY(c.errorMessage().isEmpty());
    QCOMPARE(finished.first().first().toString(), QDir(path()).filePath("ban-sao"));
    QVERIFY(QFile::exists(path() + "/ban-sao/.git"));
    QVERIFY(QFile::exists(path() + "/ban-sao/hello.txt"));
    QCOMPARE(c.percent(), 100);
    QVERIFY(running.count() >= 2);  // bắt đầu rồi kết thúc
}

void TstClone::clone_failure_reportsGitErrorAndCleansUp()
{
    CloneController c;
    QSignalSpy finished(&c, &CloneController::finished);
    c.start(path() + "/khong-co-repo-nao", path(), "that-bai");
    QTRY_VERIFY(!c.running());
    QCOMPARE(finished.count(), 0);
    QVERIFY(!c.errorMessage().isEmpty());
    QVERIFY(!QFile::exists(path() + "/that-bai"));  // thư mục dang dở được dọn
}

void TstClone::cancel_stopsProcessGroup_andRemovesPartialDirectory()
{
    // Git giả: tạo thư mục đích, in tiến độ, rồi "treo" bằng tiến trình sleep; ghi pid ra file.
    const QString pidFile = path() + "/pid.txt";
    CloneController c;
    QString script;
    fakeGit(QString("mkdir -p \"$5\"\necho $$ > %1\n"
                    "printf 'Receiving objects:  10%% (1/10)\\r' >&2\nexec sleep 60").arg(pidFile), script);
    c.setGitProgram(script);
    QSignalSpy cancelled(&c, &CloneController::cancelled);
    QSignalSpy finished(&c, &CloneController::finished);
    c.start("https://example.invalid/x.git", path(), "se-bi-huy");
    QTRY_COMPARE(c.percent(), 10);
    QCOMPARE(c.stage(), "Đang tải về");
    QVERIFY(QFile::exists(path() + "/se-bi-huy"));

    QFile pf(pidFile);
    QVERIFY(pf.open(QIODevice::ReadOnly));
    const int pid = pf.readAll().trimmed().toInt();
    QVERIFY(pid > 0);

    c.cancel();
    QTRY_COMPARE(cancelled.count(), 1);
    QVERIFY(!c.running());
    QCOMPARE(finished.count(), 0);
    QVERIFY(c.errorMessage().isEmpty());              // huỷ không phải là lỗi
    QVERIFY(!QFile::exists(path() + "/se-bi-huy"));  // thư mục dang dở đã bị dọn
    QTRY_VERIFY(::kill(pid, 0) != 0);                 // tiến trình đã thực sự dừng
}

void TstClone::cancel_neverRemovesAPreExistingEmptyDirectory()
{
    QDir().mkpath(path() + "/co-san");
    CloneController c;
    QString script;
    fakeGit("exec sleep 60", script);
    c.setGitProgram(script);
    QSignalSpy cancelled(&c, &CloneController::cancelled);
    c.start("https://example.invalid/x.git", path(), "co-san");
    QVERIFY(c.running());
    c.cancel();
    QTRY_COMPARE(cancelled.count(), 1);
    QVERIFY(QFile::exists(path() + "/co-san"));  // thư mục có sẵn của người dùng được giữ nguyên
}

QTEST_GUILESS_MAIN(TstClone)
#include "tst_clone.moc"
