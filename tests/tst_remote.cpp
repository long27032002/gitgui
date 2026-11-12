// Kiểm thử fetch/pull/push với remote cục bộ (bare repo) — không cần mạng hay SSH.
#include <GitRunner.h>
#include <GitService.h>

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace gitcore;

class TstRemote : public QObject {
    Q_OBJECT
private slots:
    void init();
    void push_withoutRemote_failsWithClearMessage();
    void push_publish_setsUpstreamAndUploadsCommits();
    void fetch_updatesBehindCount();
    void pull_fastForwards();
    void pull_divergedCreatesMergeCommitWithoutEditor();
    void pull_conflict_failsAndLeavesUnmergedFile();
    void push_rejectedWhenBehind();
    void fetch_pruneRemovesDeletedRemoteBranch();
    void deleteRemoteBranch_removesServerBranchAndTrackingRef();

private:
    // Tạo repo làm việc `name` trong thư mục tạm.
    QString makeRepo(const QString &name);
    QString clone(const QString &name);
    QByteArray git(const QString &dir, const QStringList &args, bool *ok = nullptr);
    void write(const QString &dir, const QString &rel, const QByteArray &data);
    void commitFile(const QString &dir, const QString &rel, const QByteArray &data,
                    const QString &msg);
    QString dir(const QString &name) const { return m_tmp->path() + '/' + name; }

    std::unique_ptr<QTemporaryDir> m_tmp;
};

QByteArray TstRemote::git(const QString &d, const QStringList &args, bool *ok)
{
    const GitResult r = GitRunner::run(d, args);
    if (ok)
        *ok = r.ok();
    else
        Q_ASSERT_X(r.ok(), "git", qPrintable(args.join(' ') + ": " + r.err));
    return r.out;
}

void TstRemote::write(const QString &d, const QString &rel, const QByteArray &data)
{
    QFile f(d + '/' + rel);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(data);
}

void TstRemote::commitFile(const QString &d, const QString &rel, const QByteArray &data,
                           const QString &msg)
{
    write(d, rel, data);
    git(d, {"add", "-A"});
    git(d, {"commit", "-q", "-m", msg});
}

QString TstRemote::makeRepo(const QString &name)
{
    const QString d = dir(name);
    QDir().mkpath(d);
    git(d, {"init", "-q", "-b", "main"});
    git(d, {"config", "user.name", "Test"});
    git(d, {"config", "user.email", "test@example.com"});
    git(d, {"config", "commit.gpgsign", "false"});
    return d;
}

QString TstRemote::clone(const QString &name)
{
    git(m_tmp->path(), {"clone", "-q", dir("remote.git"), name});
    const QString d = dir(name);
    git(d, {"config", "user.name", "Test2"});
    git(d, {"config", "user.email", "test2@example.com"});
    git(d, {"config", "commit.gpgsign", "false"});
    return d;
}

void TstRemote::init()
{
    m_tmp = std::make_unique<QTemporaryDir>();
    QVERIFY(m_tmp->isValid());
    QDir().mkpath(dir("remote.git"));
    git(dir("remote.git"), {"init", "--bare", "-q", "-b", "main"});
}

void TstRemote::push_withoutRemote_failsWithClearMessage()
{
    const QString a = makeRepo("a");
    commitFile(a, "f.txt", "1\n", "c1");
    const GitResult r = GitService(a).push(true);
    QVERIFY(!r.ok());
    QVERIFY(r.err.contains("remote"));
}

void TstRemote::push_publish_setsUpstreamAndUploadsCommits()
{
    const QString a = makeRepo("a");
    git(a, {"remote", "add", "origin", dir("remote.git")});
    commitFile(a, "f.txt", "1\n", "c1");

    const GitResult r = GitService(a).push(true);
    QVERIFY2(r.ok(), qPrintable(r.err));
    QCOMPARE(git(a, {"rev-parse", "--abbrev-ref", "@{u}"}).trimmed(), QByteArray("origin/main"));
    QCOMPARE(git(dir("remote.git"), {"rev-list", "--count", "main"}).trimmed(), QByteArray("1"));

    // Lần sau đã có upstream: push thường.
    commitFile(a, "f.txt", "2\n", "c2");
    QVERIFY(GitService(a).push(false).ok());
    QCOMPARE(git(dir("remote.git"), {"rev-list", "--count", "main"}).trimmed(), QByteArray("2"));
}

void TstRemote::fetch_updatesBehindCount()
{
    const QString a = makeRepo("a");
    git(a, {"remote", "add", "origin", dir("remote.git")});
    commitFile(a, "f.txt", "1\n", "c1");
    QVERIFY(GitService(a).push(true).ok());
    const QString b = clone("b");

    commitFile(a, "f.txt", "2\n", "c2");
    QVERIFY(GitService(a).push(false).ok());

    GitService svcB(b);
    QCOMPARE(svcB.status()->behind, 0);  // chưa fetch thì chưa biết
    QVERIFY(svcB.fetch().ok());
    QCOMPARE(svcB.status()->behind, 1);
    QCOMPARE(svcB.status()->ahead, 0);
}

void TstRemote::pull_fastForwards()
{
    const QString a = makeRepo("a");
    git(a, {"remote", "add", "origin", dir("remote.git")});
    commitFile(a, "f.txt", "1\n", "c1");
    QVERIFY(GitService(a).push(true).ok());
    const QString b = clone("b");
    commitFile(a, "g.txt", "new\n", "c2");
    QVERIFY(GitService(a).push(false).ok());

    GitService svcB(b);
    const GitResult r = svcB.pull();
    QVERIFY2(r.ok(), qPrintable(r.err));
    QVERIFY(QFile::exists(b + "/g.txt"));
    QCOMPARE(svcB.status()->behind, 0);
}

void TstRemote::pull_divergedCreatesMergeCommitWithoutEditor()
{
    const QString a = makeRepo("a");
    git(a, {"remote", "add", "origin", dir("remote.git")});
    commitFile(a, "f.txt", "1\n", "c1");
    QVERIFY(GitService(a).push(true).ok());
    const QString b = clone("b");

    commitFile(a, "a.txt", "from a\n", "a2");
    QVERIFY(GitService(a).push(false).ok());
    commitFile(b, "b.txt", "from b\n", "b2");

    GitService svcB(b);
    QCOMPARE(svcB.status()->ahead, 1);
    const GitResult r = svcB.pull();  // không được treo chờ trình soạn thảo
    QVERIFY2(r.ok(), qPrintable(r.err));
    QCOMPARE(git(b, {"rev-list", "--merges", "--count", "HEAD"}).trimmed(), QByteArray("1"));
    QVERIFY(QFile::exists(b + "/a.txt") && QFile::exists(b + "/b.txt"));
    QVERIFY(svcB.push(false).ok());  // sau merge đẩy lên được
}

void TstRemote::pull_conflict_failsAndLeavesUnmergedFile()
{
    const QString a = makeRepo("a");
    git(a, {"remote", "add", "origin", dir("remote.git")});
    commitFile(a, "f.txt", "base\n", "c1");
    QVERIFY(GitService(a).push(true).ok());
    const QString b = clone("b");

    commitFile(a, "f.txt", "from a\n", "a2");
    QVERIFY(GitService(a).push(false).ok());
    commitFile(b, "f.txt", "from b\n", "b2");

    GitService svcB(b);
    const GitResult r = svcB.pull();
    QVERIFY(!r.ok());
    QVERIFY(!(r.err + QString::fromUtf8(r.out)).trimmed().isEmpty());
    const auto st = svcB.status();
    QVERIFY(st && st->changes.size() == 1);
    QCOMPARE(st->changes[0].kind, ChangeKind::Unmerged);
}

void TstRemote::push_rejectedWhenBehind()
{
    const QString a = makeRepo("a");
    git(a, {"remote", "add", "origin", dir("remote.git")});
    commitFile(a, "f.txt", "1\n", "c1");
    QVERIFY(GitService(a).push(true).ok());
    const QString b = clone("b");
    commitFile(a, "f.txt", "2\n", "c2");
    QVERIFY(GitService(a).push(false).ok());

    commitFile(b, "g.txt", "x\n", "b2");
    const GitResult r = GitService(b).push(false);
    QVERIFY(!r.ok());  // non-fast-forward: không được ghi đè lịch sử trên remote
    QCOMPARE(git(dir("remote.git"), {"rev-list", "--count", "main"}).trimmed(), QByteArray("2"));
}

void TstRemote::fetch_pruneRemovesDeletedRemoteBranch()
{
    const QString a = makeRepo("a");
    git(a, {"remote", "add", "origin", dir("remote.git")});
    commitFile(a, "f.txt", "1\n", "c1");
    QVERIFY(GitService(a).push(true).ok());
    git(a, {"push", "origin", "main:feature"});
    const QString b = clone("b");
    GitService svcB(b);
    QVERIFY(svcB.fetch().ok());
    QVERIFY(git(b, {"branch", "-r"}).contains("origin/feature"));

    git(a, {"push", "origin", "--delete", "feature"});
    QVERIFY(svcB.fetch().ok());
    QVERIFY(!git(b, {"branch", "-r"}).contains("origin/feature"));
}

void TstRemote::deleteRemoteBranch_removesServerBranchAndTrackingRef()
{
    const QString a = makeRepo("a");
    git(a, {"remote", "add", "origin", dir("remote.git")});
    commitFile(a, "f.txt", "1\n", "c1");
    QVERIFY(GitService(a).push(true).ok());
    git(a, {"push", "origin", "main:feature/topic"});
    QVERIFY(GitService(a).fetch().ok());
    QVERIFY(git(a, {"show-ref", "--verify", "refs/remotes/origin/feature/topic"}).size() > 0);

    const GitResult r = GitService(a).deleteRemoteBranch("origin", "feature/topic");
    QVERIFY2(r.ok(), qPrintable(r.err));
    bool exists = true;
    git(dir("remote.git"), {"show-ref", "--verify", "refs/heads/feature/topic"}, &exists);
    QVERIFY(!exists);
    git(a, {"show-ref", "--verify", "refs/remotes/origin/feature/topic"}, &exists);
    QVERIFY(!exists);
}

QTEST_APPLESS_MAIN(TstRemote)
#include "tst_remote.moc"
