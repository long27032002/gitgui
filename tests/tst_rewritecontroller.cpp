#include <GitRunner.h>
#include <RepoController.h>

#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

using namespace gitapp;
using namespace gitcore;

class TstRewriteController : public QObject {
    Q_OBJECT
private slots:
    void init();

    void historyRange_selectExtendCollapse();
    void merge_modes_throughController();
    void merge_ffOnlyOnDivergedBranch_reportsError();
    void rebase_preparesCountsAndRewritesBranch();
    void rebase_withUncommittedChanges_isRefusedByGit();
    void cherryPickAndRevert_fromHistory();
    void cherryPickSelected_appliesRangeOldestFirst();
    void createBranchFromCommit_switchesToCommit();
    void cancelNetworkOperation_stopsFetchAndRefreshes();
    void squash_fromHistoryRange_rewritesBranch();
    void squash_plan_rejectsSingleCommit_andMergeCommits();
    void squash_middleWithDirtyTree_isFlaggedInPlan();
    void forcePush_afterSquash_flow();
    void divergedFlag_reflectsAheadAndBehind();

private:
    QByteArray git(const QStringList &args, bool *ok = nullptr);
    void write(const QString &rel, const QByteArray &data);
    void commitFile(const QString &rel, const QByteArray &data, const QString &msg);
    void makeLinear(int n);
    QString sha(const QString &rev);
    void openAndWait(RepoController &c);
    QStringList subjects();
    QString path() const { return m_dir->path(); }

    std::unique_ptr<QTemporaryDir> m_dir;
};

QByteArray TstRewriteController::git(const QStringList &args, bool *ok)
{
    const GitResult r = GitRunner::run(path(), args);
    if (ok)
        *ok = r.ok();
    else
        Q_ASSERT_X(r.ok(), "git", qPrintable(args.join(' ') + ": " + r.err));
    return r.out;
}

void TstRewriteController::write(const QString &rel, const QByteArray &data)
{
    QFile f(path() + '/' + rel);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(data);
}

void TstRewriteController::commitFile(const QString &rel, const QByteArray &data, const QString &msg)
{
    write(rel, data);
    git({"add", "-A"});
    git({"commit", "-q", "-m", msg});
}

void TstRewriteController::makeLinear(int n)
{
    for (int i = 1; i <= n; ++i)
        commitFile(QString("f%1.txt").arg(i), QByteArray::number(i) + "\n", QString("c%1").arg(i));
}

QString TstRewriteController::sha(const QString &rev)
{
    return QString::fromUtf8(git({"rev-parse", rev})).trimmed();
}

QStringList TstRewriteController::subjects()
{
    return QString::fromUtf8(git({"log", "--format=%s", "--reverse"})).split('\n', Qt::SkipEmptyParts);
}

void TstRewriteController::openAndWait(RepoController &c)
{
    c.openRepo(path());
    QTRY_VERIFY(c.hasRepo());
    QTRY_VERIFY(!c.busy());
}

void TstRewriteController::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    git({"init", "-q", "-b", "main"});
    git({"config", "user.name", "Test"});
    git({"config", "user.email", "test@example.com"});
    git({"config", "commit.gpgsign", "false"});
}

void TstRewriteController::historyRange_selectExtendCollapse()
{
    makeLinear(5);
    RepoController c;
    openAndWait(c);
    HistoryController *h = c.historyController();
    QTRY_COMPARE(h->commits()->rowCount(), 5);
    QTRY_COMPARE(h->rangeSize(), 1);  // tự chọn commit mới nhất

    h->selectCommit(2);
    QCOMPARE(h->rangeStart(), 2);
    QCOMPARE(h->rangeEnd(), 2);
    h->extendSelection(4);
    QCOMPARE(h->rangeStart(), 2);
    QCOMPARE(h->rangeEnd(), 4);
    QCOMPARE(h->rangeSize(), 3);
    h->extendSelection(0);  // kéo ngược lên: vẫn chuẩn hoá start <= end
    QCOMPARE(h->rangeStart(), 0);
    QCOMPARE(h->rangeEnd(), 2);
    h->selectCommit(2);  // bấm lại commit gốc của vùng chọn -> thu gọn
    QCOMPARE(h->rangeSize(), 1);
}

void TstRewriteController::merge_modes_throughController()
{
    makeLinear(1);
    git({"switch", "-q", "-c", "feat"});
    commitFile("x.txt", "x\n", "feat x");
    commitFile("y.txt", "y\n", "feat y");
    git({"switch", "-q", "main"});

    RepoController c;
    openAndWait(c);
    c.prepareBranchAction("feat");
    QTRY_VERIFY(c.branchCompare().value("loaded").toBool());
    QCOMPARE(c.branchCompare().value("incoming").toInt(), 2);
    QCOMPARE(c.branchCompare().value("local").toInt(), 0);

    // squash: không commit, chuyển sang vùng stage
    QSignalSpy staged(&c, &RepoController::squashMergeStaged);
    const QByteArray head = git({"rev-parse", "HEAD"});
    c.mergeBranch("feat", 1);
    QTRY_COMPARE(staged.count(), 1);
    QTRY_VERIFY(!c.busy());
    QCOMPARE(git({"rev-parse", "HEAD"}), head);
    QCOMPARE(c.changeCount(), 2);
    QCOMPARE(c.includedCount(), 2);  // thay đổi gom về mặc định được chọn sẵn để commit
    git({"reset", "-q", "--hard"});

    // merge commit
    c.refresh();
    QTRY_VERIFY(!c.busy());
    c.mergeBranch("feat", 0);
    QTRY_VERIFY(c.branchCompare().value("incoming").toInt() == 2);
    QTRY_COMPARE(git({"rev-list", "--merges", "--count", "HEAD"}).trimmed(), QByteArray("1"));
    QTRY_VERIFY(!c.busy());
    QVERIFY(c.errorMessage().isEmpty());
}

void TstRewriteController::merge_ffOnlyOnDivergedBranch_reportsError()
{
    makeLinear(1);
    git({"switch", "-q", "-c", "feat"});
    commitFile("x.txt", "x\n", "feat x");
    git({"switch", "-q", "main"});
    commitFile("m.txt", "m\n", "main m");

    RepoController c;
    openAndWait(c);
    c.mergeBranch("feat", 2);
    QTRY_VERIFY(!c.errorMessage().isEmpty());
    QTRY_VERIFY(!c.busy());
    QVERIFY(!QFile::exists(path() + "/x.txt"));
}

void TstRewriteController::rebase_preparesCountsAndRewritesBranch()
{
    makeLinear(1);
    git({"switch", "-q", "-c", "feat"});
    commitFile("a.txt", "a\n", "feat a");
    git({"switch", "-q", "main"});
    commitFile("m.txt", "m\n", "main m");
    git({"switch", "-q", "feat"});

    RepoController c;
    openAndWait(c);
    c.prepareBranchAction("main");
    QTRY_VERIFY(c.branchCompare().value("loaded").toBool());
    QCOMPARE(c.branchCompare().value("local").toInt(), 1);
    QCOMPARE(c.branchCompare().value("pushed").toInt(), 0);

    c.rebaseOnto("main");
    QTRY_COMPARE(git({"rev-list", "--count", "main..feat"}).trimmed(), QByteArray("1"));
    QTRY_VERIFY(!c.busy());
    QVERIFY(c.errorMessage().isEmpty());
    QCOMPARE(c.operation(), "");
    QVERIFY(QFile::exists(path() + "/m.txt"));
}

void TstRewriteController::rebase_withUncommittedChanges_isRefusedByGit()
{
    makeLinear(1);
    git({"switch", "-q", "-c", "feat"});
    commitFile("a.txt", "a\n", "feat a");
    git({"switch", "-q", "main"});
    commitFile("m.txt", "m\n", "main m");
    git({"switch", "-q", "feat"});
    write("a.txt", "dirty\n");

    RepoController c;
    openAndWait(c);
    c.rebaseOnto("main");
    QTRY_VERIFY(!c.errorMessage().isEmpty());
    QTRY_VERIFY(!c.busy());
    QCOMPARE(git({"rev-list", "--count", "main..feat"}).trimmed(), QByteArray("1"));
    QCOMPARE(c.operation(), "");
}

void TstRewriteController::cherryPickAndRevert_fromHistory()
{
    makeLinear(1);
    git({"switch", "-q", "-c", "feat"});
    commitFile("x.txt", "x\n", "feat x");
    const QString pick = sha("HEAD");
    git({"switch", "-q", "main"});

    RepoController c;
    openAndWait(c);
    c.cherryPick(pick);
    QTRY_VERIFY(QFile::exists(path() + "/x.txt"));
    QTRY_VERIFY(!c.busy());
    QVERIFY(c.errorMessage().isEmpty());

    c.revertCommit(sha("HEAD"));
    QTRY_VERIFY(!QFile::exists(path() + "/x.txt"));
    QTRY_VERIFY(!c.busy());
    QVERIFY(subjects().last().startsWith("Revert"));
}

void TstRewriteController::cherryPickSelected_appliesRangeOldestFirst()
{
    makeLinear(1);
    git({"switch", "-q", "-c", "feature"});
    commitFile("x.txt", "x\n", "feature x");
    commitFile("y.txt", "y\n", "feature y");

    RepoController c;
    openAndWait(c);
    HistoryController *h = c.historyController();
    QTRY_COMPARE(h->commits()->rowCount(), 3);
    h->extendSelection(1); // hai commit mới nhất; model mới nhất trước
    git({"switch", "-q", "main"});
    c.cherryPickSelected();
    QTRY_VERIFY(!c.busy());
    QVERIFY2(c.errorMessage().isEmpty(), qPrintable(c.errorMessage()));
    QCOMPARE(subjects(), (QStringList{"c1", "feature x", "feature y"}));
    QVERIFY(QFile::exists(path() + "/x.txt"));
    QVERIFY(QFile::exists(path() + "/y.txt"));
}

void TstRewriteController::createBranchFromCommit_switchesToCommit()
{
    makeLinear(2);
    const QString base = sha("HEAD~1");
    RepoController c;
    openAndWait(c);
    c.createBranchFromCommit("older-point", base, false);
    QTRY_VERIFY(!c.busy());
    QVERIFY2(c.errorMessage().isEmpty(), qPrintable(c.errorMessage()));
    QCOMPARE(git({"rev-parse", "HEAD"}).trimmed(), base.toUtf8());
    QCOMPARE(git({"branch", "--show-current"}).trimmed(), QByteArray("older-point"));
}

void TstRewriteController::cancelNetworkOperation_stopsFetchAndRefreshes()
{
    makeLinear(1);
    RepoController c;
    openAndWait(c);

    const QByteArray oldPath = qgetenv("PATH");
    const QString realGit = QStandardPaths::findExecutable(QStringLiteral("git"));
    QVERIFY(!realGit.isEmpty());
    const QString fakeBin = path() + QStringLiteral("/fakebin");
    QVERIFY(QDir().mkpath(fakeBin));
    QFile wrapper(fakeBin + QStringLiteral("/git"));
    QVERIFY(wrapper.open(QIODevice::WriteOnly));
    wrapper.write(QString("#!/bin/sh\nif [ \"$1\" = fetch ]; then sleep 10; else exec %1 \"$@\"; fi\n")
                      .arg(realGit).toLocal8Bit());
    wrapper.close();
    QVERIFY(wrapper.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                   | QFileDevice::ExeOwner | QFileDevice::ReadGroup
                                   | QFileDevice::ExeGroup | QFileDevice::ReadOther
                                   | QFileDevice::ExeOther));
    qputenv("PATH", fakeBin.toLocal8Bit() + ':' + oldPath);

    c.fetch();
    QTRY_VERIFY_WITH_TIMEOUT(c.canCancel(), 3000);
    c.cancelNetworkOperation();
    QTRY_VERIFY_WITH_TIMEOUT(!c.busy(), 5000);
    qputenv("PATH", oldPath);

    QVERIFY(!c.canCancel());
    QVERIFY(c.notice().contains(QStringLiteral("huỷ")));
    QCOMPARE(c.branch(), QStringLiteral("main"));
}

void TstRewriteController::squash_fromHistoryRange_rewritesBranch()
{
    makeLinear(5);
    RepoController c;
    openAndWait(c);
    HistoryController *h = c.historyController();
    QTRY_COMPARE(h->commits()->rowCount(), 5);
    const QString treeBefore = sha("HEAD^{tree}");

    h->selectCommit(1);      // c4
    h->extendSelection(3);   // ... c2
    c.prepareSquash();
    QTRY_VERIFY(c.squashPlan().value("loaded").toBool());
    QVERIFY(c.squashPlan().value("ok").toBool());
    QCOMPARE(c.squashPlan().value("count").toInt(), 3);
    const QString msg = c.squashPlan().value("message").toString();
    QVERIFY(msg.startsWith("c2\n\n- c2\n- c3\n- c4"));
    QVERIFY(!c.squashPlan().value("pushed").toBool());

    c.squashSelected("gom c2-c4");
    QTRY_VERIFY(!c.busy());
    QTRY_COMPARE(h->commits()->rowCount(), 3);
    QCOMPARE(subjects(), (QStringList{"c1", "gom c2-c4", "c5"}));
    QCOMPARE(sha("HEAD^{tree}"), treeBefore);
    QVERIFY(c.errorMessage().isEmpty());
}

void TstRewriteController::squash_plan_rejectsSingleCommit_andMergeCommits()
{
    makeLinear(2);
    git({"switch", "-q", "-c", "side", "HEAD~1"});
    commitFile("s.txt", "s\n", "side s");
    git({"switch", "-q", "main"});
    git({"merge", "-q", "--no-ff", "--no-edit", "side"});
    commitFile("after.txt", "a\n", "after merge");

    RepoController c;
    openAndWait(c);
    HistoryController *h = c.historyController();
    QTRY_VERIFY(h->commits()->rowCount() >= 5);

    h->selectCommit(0);
    c.prepareSquash();
    QTRY_VERIFY(c.squashPlan().value("loaded").toBool());
    QVERIFY(!c.squashPlan().value("ok").toBool());  // chỉ một commit

    // Vùng chọn chứa merge commit (hàng 1).
    h->selectCommit(0);
    h->extendSelection(1);
    c.prepareSquash();
    QTRY_VERIFY(c.squashPlan().value("loaded").toBool());
    QVERIFY(!c.squashPlan().value("ok").toBool());
    QVERIFY(c.squashPlan().value("error").toString().contains("merge"));

    c.squashSelected("không được chạy");  // kế hoạch không hợp lệ -> bỏ qua
    QTest::qWait(100);
    QVERIFY(c.errorMessage().isEmpty());
}

void TstRewriteController::squash_middleWithDirtyTree_isFlaggedInPlan()
{
    makeLinear(4);
    write("f1.txt", "dirty\n");
    RepoController c;
    openAndWait(c);
    HistoryController *h = c.historyController();
    QTRY_COMPARE(h->commits()->rowCount(), 4);
    h->selectCommit(1);
    h->extendSelection(2);
    c.prepareSquash();
    QTRY_VERIFY(c.squashPlan().value("loaded").toBool());
    QVERIFY(c.squashPlan().value("ok").toBool());
    QVERIFY(c.squashPlan().value("dirty").toBool());  // có commit nằm trên -> cần rebase -> cây phải sạch

    h->selectCommit(0);
    h->extendSelection(1);  // đỉnh nhánh: không cần rebase, giữ nguyên thay đổi chưa commit
    c.prepareSquash();
    QTRY_VERIFY(c.squashPlan().value("loaded").toBool());
    QVERIFY(!c.squashPlan().value("dirty").toBool());
}

void TstRewriteController::forcePush_afterSquash_flow()
{
    QTemporaryDir remote;
    QVERIFY(GitRunner::run(remote.path(), {"init", "--bare", "-q", "-b", "main"}).ok());
    makeLinear(4);
    git({"remote", "add", "origin", remote.path()});
    git({"push", "-q", "-u", "origin", "main"});

    RepoController c;
    openAndWait(c);
    HistoryController *h = c.historyController();
    QTRY_COMPARE(h->commits()->rowCount(), 4);
    QVERIFY(!c.diverged());

    h->selectCommit(0);
    h->extendSelection(1);
    c.prepareSquash();
    QTRY_VERIFY(c.squashPlan().value("loaded").toBool());
    QVERIFY(c.squashPlan().value("pushed").toBool());  // sẽ cần force push
    c.squashSelected("gom c3+c4");
    QTRY_VERIFY(c.diverged());
    QTRY_VERIFY(!c.busy());

    c.prepareForcePush();
    QTRY_VERIFY(c.forcePushInfo().value("loaded").toBool());
    QCOMPARE(c.forcePushInfo().value("count").toInt(), 2);
    QCOMPARE(c.forcePushInfo().value("upstream").toString(), "origin/main");
    QVERIFY(c.forcePushInfo().value("commits").toStringList().first().contains("c4"));

    c.confirmForcePush();
    QTRY_VERIFY(!c.diverged());
    QTRY_VERIFY(!c.busy());
    QVERIFY(c.errorMessage().isEmpty());
    QCOMPARE(QString::fromUtf8(GitRunner::run(remote.path(), {"rev-parse", "main"}).out).trimmed(),
             sha("HEAD"));

    // Huỷ không làm gì cả.
    c.prepareForcePush();
    QTRY_VERIFY(c.forcePushInfo().value("loaded").toBool());
    c.cancelForcePush();
    QVERIFY(c.forcePushInfo().isEmpty());
}

void TstRewriteController::divergedFlag_reflectsAheadAndBehind()
{
    QTemporaryDir remote;
    QVERIFY(GitRunner::run(remote.path(), {"init", "--bare", "-q", "-b", "main"}).ok());
    makeLinear(2);
    git({"remote", "add", "origin", remote.path()});
    git({"push", "-q", "-u", "origin", "main"});
    commitFile("l.txt", "l\n", "local only");

    RepoController c;
    openAndWait(c);
    QCOMPARE(c.ahead(), 1);
    QVERIFY(!c.diverged());  // chỉ ahead, chưa phân kỳ
}

QTEST_GUILESS_MAIN(TstRewriteController)
#include "tst_rewritecontroller.moc"
