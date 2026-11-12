#include <GitRunner.h>
#include <GitService.h>

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>
#include <atomic>
#include <chrono>
#include <thread>

using namespace gitcore;

class TstGitService : public QObject {
    Q_OBJECT
private slots:
    void init();
    void findRoot_rejectsNonRepo();
    void status_onEmptyRepo_listsUntracked();
    void stage_unstage_roundTrip_beforeFirstCommit();
    void commit_clearsChanges_andRejectsEmptyCommit();
    void commit_supportsMultilineUtf8Message();
    void diff_showsUnstagedAndStaged();
    void status_trackedModification_andDeletion();
    void stageAndUnstage_handleManyAndUnusualPaths();
    void userName_andLastFetchTime();
    void gitRunner_stopsCancellableProcess();

private:
    void write(const QString &rel, const QByteArray &data);
    void git(const QStringList &args);
    QString path() const { return m_dir->path(); }

    std::unique_ptr<QTemporaryDir> m_dir;
};

void TstGitService::write(const QString &rel, const QByteArray &data)
{
    QFile f(path() + QLatin1Char('/') + rel);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

void TstGitService::git(const QStringList &args)
{
    const GitResult r = GitRunner::run(path(), args);
    QVERIFY2(r.ok(), qPrintable(r.err));
}

void TstGitService::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    git({"init", "-q"});
    git({"config", "user.name", "Test"});
    git({"config", "user.email", "test@example.com"});
    git({"config", "commit.gpgsign", "false"});
}

void TstGitService::findRoot_rejectsNonRepo()
{
    QTemporaryDir plain;
    // Thư mục tạm nằm ngoài mọi repo -> không phải repo git.
    QVERIFY(!GitService::findRoot(plain.path()).has_value());
    QVERIFY(GitService::findRoot(path()).has_value());
}

void TstGitService::status_onEmptyRepo_listsUntracked()
{
    write("a.txt", "hello\n");
    GitService svc(path());
    const auto st = svc.status();
    QVERIFY(st.has_value());
    QVERIFY(st->oid.isEmpty());
    QCOMPARE(st->changes.size(), 1);
    QCOMPARE(st->changes[0].kind, ChangeKind::Untracked);
}

void TstGitService::stage_unstage_roundTrip_beforeFirstCommit()
{
    write("a.txt", "hello\n");
    GitService svc(path());

    QVERIFY(svc.stage({"a.txt"}).ok());
    auto st = svc.status();
    QVERIFY(st && st->changes.size() == 1 && st->changes[0].isStaged());

    const GitResult un = svc.unstage({"a.txt"});
    QVERIFY2(un.ok(), qPrintable(un.err));
    st = svc.status();
    QVERIFY(st && st->changes.size() == 1);
    QCOMPARE(st->changes[0].kind, ChangeKind::Untracked);
}

void TstGitService::commit_clearsChanges_andRejectsEmptyCommit()
{
    write("a.txt", "hello\n");
    GitService svc(path());
    QVERIFY(svc.stage({"a.txt"}).ok());
    QVERIFY(svc.commit("first").ok());

    const auto st = svc.status();
    QVERIFY(st.has_value());
    QVERIFY(!st->oid.isEmpty());
    QVERIFY(st->changes.isEmpty());

    QVERIFY(!svc.commit("nothing staged").ok());
}

void TstGitService::commit_supportsMultilineUtf8Message()
{
    write("a.txt", "x\n");
    GitService svc(path());
    QVERIFY(svc.stage({"a.txt"}).ok());
    QVERIFY(svc.commit(QStringLiteral("Tiêu đề\n\nNội dung dòng 2")).ok());

    const GitResult log = GitRunner::run(path(), {"log", "-1", "--format=%B"});
    QVERIFY(log.ok());
    QVERIFY(QString::fromUtf8(log.out).contains(QStringLiteral("Tiêu đề")));
    QVERIFY(QString::fromUtf8(log.out).contains(QStringLiteral("Nội dung dòng 2")));
}

void TstGitService::diff_showsUnstagedAndStaged()
{
    write("a.txt", "one\n");
    GitService svc(path());
    QVERIFY(svc.stage({"a.txt"}).ok());
    QVERIFY(svc.commit("init").ok());

    write("a.txt", "two\n");
    GitResult d = svc.diff("a.txt", false);
    QVERIFY(d.ok());
    QVERIFY(d.out.contains("+two"));
    QVERIFY(svc.diff("a.txt", true).out.isEmpty());

    QVERIFY(svc.stage({"a.txt"}).ok());
    QVERIFY(svc.diff("a.txt", true).out.contains("+two"));
}

void TstGitService::status_trackedModification_andDeletion()
{
    write("a.txt", "one\n");
    write("b.txt", "keep\n");
    GitService svc(path());
    QVERIFY(svc.stage({"a.txt", "b.txt"}).ok());
    QVERIFY(svc.commit("init").ok());

    write("a.txt", "changed\n");
    QVERIFY(QFile::remove(path() + "/b.txt"));
    // Xoá file rồi stage: `add -A --` phải ghi nhận việc xoá.
    QVERIFY(svc.stage({"b.txt"}).ok());

    const auto st = svc.status();
    QVERIFY(st.has_value());
    QCOMPARE(st->changes.size(), 2);
    for (const FileChange &c : st->changes) {
        if (c.path == "a.txt") {
            QVERIFY(c.isUnstaged() && !c.isStaged());
        } else {
            QCOMPARE(c.path, "b.txt");
            QCOMPARE(c.indexStatus, QLatin1Char('D'));
        }
    }
}

void TstGitService::stageAndUnstage_handleManyAndUnusualPaths()
{
    QStringList paths;
    for (int i = 0; i < 400; ++i)
        paths << QString("dir with space/tệp %1.txt").arg(i);
    QDir().mkpath(path() + "/dir with space");
    for (const QString &p : paths)
        write(p, "x\n");
    write("-leading-dash.txt", "y\n");
    paths << "-leading-dash.txt";

    GitService svc(path());
    QVERIFY2(svc.stage(paths).ok(), "stage nhiều file");
    auto st = svc.status();
    QVERIFY(st);
    QCOMPARE(st->changes.size(), paths.size());
    for (const FileChange &c : st->changes)
        QVERIFY(c.isStaged());

    QVERIFY(svc.unstage(paths).ok());  // cả khi chưa có commit nào
    st = svc.status();
    QVERIFY(st);
    for (const FileChange &c : st->changes)
        QVERIFY(!c.isStaged());

    QVERIFY(!svc.stage({}).ok());   // danh sách rỗng: báo lỗi rõ ràng, không stage cả repo
    QVERIFY(!svc.unstage({}).ok());
}

void TstGitService::userName_andLastFetchTime()
{
    GitService svc(path());
    QCOMPARE(svc.userName(), "Test");
    QVERIFY(!svc.lastFetchTime().isValid());  // chưa fetch lần nào

    QTemporaryDir remote;
    QVERIFY(GitRunner::run(remote.path(), {"init", "--bare", "-q", "-b", "main"}).ok());
    write("a.txt", "a\n");
    git({"add", "a.txt"});
    git({"commit", "-q", "-m", "c1"});
    git({"remote", "add", "origin", remote.path()});
    git({"push", "-q", "-u", "origin", "HEAD"});
    const QDateTime before = QDateTime::currentDateTime().addSecs(-5);
    QVERIFY(svc.fetch().ok());
    QVERIFY(svc.lastFetchTime().isValid());
    QVERIFY(svc.lastFetchTime() >= before);
}

void TstGitService::gitRunner_stopsCancellableProcess()
{
    const QByteArray oldPath = qgetenv("PATH");
    const QString fakeBin = path() + QStringLiteral("/fakebin");
    QVERIFY(QDir().mkpath(fakeBin));
    QFile wrapper(fakeBin + QStringLiteral("/git"));
    QVERIFY(wrapper.open(QIODevice::WriteOnly));
    wrapper.write("#!/bin/sh\nsleep 10\n");
    wrapper.close();
    QVERIFY(wrapper.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                   | QFileDevice::ExeOwner | QFileDevice::ReadGroup
                                   | QFileDevice::ExeGroup | QFileDevice::ReadOther
                                   | QFileDevice::ExeOther));
    qputenv("PATH", fakeBin.toLocal8Bit() + ':' + oldPath);

    std::atomic_bool cancel{false};
    std::thread requestCancel([&cancel] {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        cancel.store(true);
    });
    QElapsedTimer timer;
    timer.start();
    const GitResult r = GitRunner::run(path(), {QStringLiteral("fetch")}, {}, 30'000, &cancel);
    requestCancel.join();
    qputenv("PATH", oldPath);

    QVERIFY(r.cancelled);
    QVERIFY(!r.ok());
    QVERIFY(timer.elapsed() < 4000);
}

QTEST_APPLESS_MAIN(TstGitService)
#include "tst_gitservice.moc"
