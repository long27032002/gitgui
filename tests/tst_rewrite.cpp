// Merge, rebase, cherry-pick, revert, squash và force push — với git thật.
#include <GitRunner.h>
#include <GitService.h>

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace gitcore;

class TstRewrite : public QObject {
    Q_OBJECT
private slots:
    void init();

    void commitCount_countsCommitsOnlyInTip();
    void merge_commitMode_createsMergeCommitEvenWhenFastForwardPossible();
    void merge_squashMode_stagesChangesWithoutCommitting();
    void merge_fastForwardOnly_failsWhenDiverged_succeedsWhenPossible();
    void merge_conflict_isReportedAsMergeOperation();
    void rebase_replaysCommitsOntoBase_andConflictIsReported();
    void cherryPick_appliesCommit_andHandlesMergeCommits();
    void cherryPick_appliesMultipleCommits_oldestFirst();
    void revert_createsInverseCommit_andHandlesMergeCommits();
    void squash_tipRange_keepsTreeAndDirtyChanges();
    void squash_middleRange_replaysNewerCommitsAndKeepsTree();
    void squash_rejectsRootDirtyTreeAndMergeCommits();
    void squash_replayOfNewerCommitsOnSquashedTreeIsAlwaysClean();
    void pushedDetection_countsRewrittenPushedCommits();
    void forcePush_overwritesOnlyWhatUserConfirmed();

private:
    QByteArray git(const QStringList &args, bool *ok = nullptr);
    void write(const QString &rel, const QByteArray &data);
    QByteArray read(const QString &rel);
    void commitFile(const QString &rel, const QByteArray &data, const QString &msg);
    QStringList subjects(const QString &range = "HEAD");
    QString tree(const QString &rev = "HEAD");
    // c1..cN, mỗi commit ghi một file riêng "fK.txt".
    void makeLinear(int n);
    QString path() const { return m_dir->path(); }

    std::unique_ptr<QTemporaryDir> m_dir;
};

QByteArray TstRewrite::git(const QStringList &args, bool *ok)
{
    const GitResult r = GitRunner::run(path(), args);
    if (ok)
        *ok = r.ok();
    else
        Q_ASSERT_X(r.ok(), "git", qPrintable(args.join(' ') + ": " + r.err));
    return r.out;
}

void TstRewrite::write(const QString &rel, const QByteArray &data)
{
    QFile f(path() + '/' + rel);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(data);
}

QByteArray TstRewrite::read(const QString &rel)
{
    QFile f(path() + '/' + rel);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

void TstRewrite::commitFile(const QString &rel, const QByteArray &data, const QString &msg)
{
    write(rel, data);
    git({"add", "-A"});
    git({"commit", "-q", "-m", msg});
}

QStringList TstRewrite::subjects(const QString &range)
{
    return QString::fromUtf8(git({"log", "--format=%s", "--reverse", range})).split('\n', Qt::SkipEmptyParts);
}

QString TstRewrite::tree(const QString &rev)
{
    return QString::fromUtf8(git({"rev-parse", rev + "^{tree}"})).trimmed();
}

void TstRewrite::makeLinear(int n)
{
    for (int i = 1; i <= n; ++i)
        commitFile(QString("f%1.txt").arg(i), QByteArray::number(i) + "\n", QString("c%1").arg(i));
}

void TstRewrite::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    git({"init", "-q", "-b", "main"});
    git({"config", "user.name", "Test"});
    git({"config", "user.email", "test@example.com"});
    git({"config", "commit.gpgsign", "false"});
}

void TstRewrite::commitCount_countsCommitsOnlyInTip()
{
    makeLinear(2);
    git({"switch", "-q", "-c", "side"});
    commitFile("s1.txt", "s\n", "s1");
    commitFile("s2.txt", "s\n", "s2");
    commitFile("s3.txt", "s\n", "s3");
    git({"switch", "-q", "main"});
    GitService svc(path());
    QCOMPARE(svc.commitCount("main", "side"), 3);
    QCOMPARE(svc.commitCount("side", "main"), 0);
    QCOMPARE(svc.commitCount("main", "khong-co"), -1);
}

void TstRewrite::merge_commitMode_createsMergeCommitEvenWhenFastForwardPossible()
{
    makeLinear(1);
    git({"switch", "-q", "-c", "feat"});
    commitFile("x.txt", "x\n", "feat 1");
    git({"switch", "-q", "main"});

    GitService svc(path());
    const GitResult r = svc.mergeBranch("feat", MergeMode::MergeCommit);
    QVERIFY2(r.ok(), qPrintable(r.err));
    QCOMPARE(git({"rev-list", "--merges", "--count", "HEAD"}).trimmed(), QByteArray("1"));
    QVERIFY(QFile::exists(path() + "/x.txt"));
}

void TstRewrite::merge_squashMode_stagesChangesWithoutCommitting()
{
    makeLinear(1);
    git({"switch", "-q", "-c", "feat"});
    commitFile("x.txt", "x\n", "feat 1");
    commitFile("y.txt", "y\n", "feat 2");
    git({"switch", "-q", "main"});
    const QByteArray before = git({"rev-parse", "HEAD"});

    GitService svc(path());
    QVERIFY(svc.mergeBranch("feat", MergeMode::Squash).ok());
    QCOMPARE(git({"rev-parse", "HEAD"}), before);           // chưa commit
    QCOMPARE(svc.operationInProgress(), OperationKind::None);  // không có MERGE_HEAD
    const auto st = svc.status();
    QVERIFY(st);
    QCOMPARE(st->changes.size(), 2);
    for (const FileChange &c : st->changes)
        QVERIFY(c.isStaged());
}

void TstRewrite::merge_fastForwardOnly_failsWhenDiverged_succeedsWhenPossible()
{
    makeLinear(1);
    git({"switch", "-q", "-c", "feat"});
    commitFile("x.txt", "x\n", "feat 1");
    git({"switch", "-q", "main"});

    GitService svc(path());
    QVERIFY(svc.mergeBranch("feat", MergeMode::FastForwardOnly).ok());  // ff được
    QCOMPARE(git({"rev-list", "--merges", "--count", "HEAD"}).trimmed(), QByteArray("0"));

    git({"switch", "-q", "-c", "feat2", "HEAD~1"});
    commitFile("z.txt", "z\n", "feat2 1");
    git({"switch", "-q", "main"});
    commitFile("m.txt", "m\n", "main 2");
    const GitResult r = svc.mergeBranch("feat2", MergeMode::FastForwardOnly);  // đã phân kỳ
    QVERIFY(!r.ok());
    QVERIFY(!QFile::exists(path() + "/z.txt"));
}

void TstRewrite::merge_conflict_isReportedAsMergeOperation()
{
    commitFile("f.txt", "base\n", "base");
    git({"switch", "-q", "-c", "feat"});
    commitFile("f.txt", "feat\n", "feat");
    git({"switch", "-q", "main"});
    commitFile("f.txt", "main\n", "main");
    GitService svc(path());
    QVERIFY(!svc.mergeBranch("feat", MergeMode::MergeCommit).ok());
    QCOMPARE(svc.operationInProgress(), OperationKind::Merge);
}

void TstRewrite::rebase_replaysCommitsOntoBase_andConflictIsReported()
{
    makeLinear(1);
    git({"switch", "-q", "-c", "feat"});
    commitFile("a.txt", "a\n", "feat a");
    commitFile("b.txt", "b\n", "feat b");
    git({"switch", "-q", "main"});
    commitFile("m.txt", "m\n", "main m");
    git({"switch", "-q", "feat"});

    GitService svc(path());
    QCOMPARE(svc.commitCount("main", "feat"), 2);
    QVERIFY(svc.rebaseOnto("main").ok());
    QCOMPARE(subjects("main..HEAD"), (QStringList{"feat a", "feat b"}));
    QCOMPARE(svc.commitCount("feat", "main"), 0);  // main đã nằm trong feat

    // conflict
    git({"switch", "-q", "main"});
    commitFile("c.txt", "main\n", "main c");
    git({"switch", "-q", "-c", "feat2", "main~1"});
    commitFile("c.txt", "feat2\n", "feat2 c");
    QVERIFY(!svc.rebaseOnto("main").ok());
    QCOMPARE(svc.operationInProgress(), OperationKind::Rebase);
    QVERIFY(svc.abortOperation(OperationKind::Rebase).ok());
}

void TstRewrite::cherryPick_appliesCommit_andHandlesMergeCommits()
{
    makeLinear(1);
    git({"switch", "-q", "-c", "feat"});
    commitFile("x.txt", "x\n", "feat x");
    const QString pick = QString::fromUtf8(git({"rev-parse", "HEAD"})).trimmed();
    git({"switch", "-q", "main"});

    GitService svc(path());
    QVERIFY(svc.cherryPick(pick, false).ok());
    QVERIFY(QFile::exists(path() + "/x.txt"));
    QCOMPARE(subjects("HEAD").last(), QString("feat x"));

    // merge commit: cần -m 1
    git({"switch", "-q", "-c", "side", "HEAD~1"});
    commitFile("s.txt", "s\n", "side s");
    git({"switch", "-q", "-c", "target", "main~1"});
    git({"switch", "-q", "main"});
    git({"merge", "-q", "--no-ff", "--no-edit", "side"});
    const QString mergeSha = QString::fromUtf8(git({"rev-parse", "HEAD"})).trimmed();
    git({"switch", "-q", "target"});
    QVERIFY(!svc.cherryPick(mergeSha, false).ok());  // git đòi -m với merge commit
    QCOMPARE(svc.operationInProgress(), OperationKind::None);
    QVERIFY(svc.cherryPick(mergeSha, true).ok());
    QVERIFY(QFile::exists(path() + "/s.txt"));
}

void TstRewrite::cherryPick_appliesMultipleCommits_oldestFirst()
{
    makeLinear(1);
    git({"switch", "-q", "-c", "feature"});
    commitFile("x.txt", "x\n", "feature x");
    const QString first = QString::fromUtf8(git({"rev-parse", "HEAD"})).trimmed();
    commitFile("y.txt", "y\n", "feature y");
    const QString second = QString::fromUtf8(git({"rev-parse", "HEAD"})).trimmed();
    git({"switch", "-q", "main"});

    const GitResult r = GitService(path()).cherryPick({first, second}, false);
    QVERIFY2(r.ok(), qPrintable(r.err));
    QVERIFY(QFile::exists(path() + "/x.txt"));
    QVERIFY(QFile::exists(path() + "/y.txt"));
    QCOMPARE(subjects().mid(1), (QStringList{"feature x", "feature y"}));
}

void TstRewrite::revert_createsInverseCommit_andHandlesMergeCommits()
{
    makeLinear(2);
    GitService svc(path());
    const QString c2 = QString::fromUtf8(git({"rev-parse", "HEAD"})).trimmed();
    QVERIFY(svc.revert(c2, false).ok());
    QVERIFY(!QFile::exists(path() + "/f2.txt"));
    QVERIFY(subjects().last().startsWith("Revert"));

    git({"switch", "-q", "-c", "side", "HEAD~2"});
    commitFile("s.txt", "s\n", "side s");
    git({"switch", "-q", "main"});
    git({"merge", "-q", "--no-ff", "--no-edit", "side"});
    const QString mergeSha = QString::fromUtf8(git({"rev-parse", "HEAD"})).trimmed();
    QVERIFY(svc.revert(mergeSha, true).ok());
    QVERIFY(!QFile::exists(path() + "/s.txt"));
}

void TstRewrite::squash_tipRange_keepsTreeAndDirtyChanges()
{
    makeLinear(5);
    const QString treeBefore = tree();
    const QString c3 = QString::fromUtf8(git({"rev-parse", "HEAD~2"})).trimmed();
    const QString c5 = QString::fromUtf8(git({"rev-parse", "HEAD"})).trimmed();
    write("f1.txt", "dirty edit\n");  // thay đổi chưa commit phải được giữ nguyên

    GitService svc(path());
    const GitResult r = svc.squashCommits(c5, c3, "gom c3-c5\n\nchi tiết");
    QVERIFY2(r.ok(), qPrintable(r.err));
    QCOMPARE(subjects(), (QStringList{"c1", "c2", "gom c3-c5"}));
    QCOMPARE(tree(), treeBefore);
    QCOMPARE(read("f1.txt"), QByteArray("dirty edit\n"));
    QVERIFY(QString::fromUtf8(git({"log", "-1", "--format=%B"})).contains("chi tiết"));
    QCOMPARE(svc.operationInProgress(), OperationKind::None);
}

void TstRewrite::squash_middleRange_replaysNewerCommitsAndKeepsTree()
{
    makeLinear(6);
    const QString treeBefore = tree();
    const QString c3 = QString::fromUtf8(git({"rev-parse", "HEAD~3"})).trimmed();
    const QString c4 = QString::fromUtf8(git({"rev-parse", "HEAD~2"})).trimmed();

    GitService svc(path());
    const GitResult r = svc.squashCommits(c4, c3, "gom c3+c4");
    QVERIFY2(r.ok(), qPrintable(r.err));
    QCOMPARE(subjects(), (QStringList{"c1", "c2", "gom c3+c4", "c5", "c6"}));
    QCOMPARE(tree(), treeBefore);  // không mất/đổi nội dung nào
    QCOMPARE(svc.operationInProgress(), OperationKind::None);
    QCOMPARE(git({"rev-parse", "--abbrev-ref", "HEAD"}).trimmed(), QByteArray("main"));
}

void TstRewrite::squash_rejectsRootDirtyTreeAndMergeCommits()
{
    makeLinear(4);
    GitService svc(path());
    const QString root = QString::fromUtf8(git({"rev-parse", "HEAD~3"})).trimmed();
    const QString c2 = QString::fromUtf8(git({"rev-parse", "HEAD~2"})).trimmed();
    const QString c3 = QString::fromUtf8(git({"rev-parse", "HEAD~1"})).trimmed();
    const QByteArray headBefore = git({"rev-parse", "HEAD"});

    QVERIFY(!svc.squashCommits(c2, root, "x").ok());  // commit gốc không có cha
    QVERIFY(!svc.squashCommits(c3, c2, "   ").ok());  // thiếu message

    write("f1.txt", "dirty\n");
    const GitResult dirty = svc.squashCommits(c3, c2, "x");  // cần replay commit c4 -> phải sạch
    QVERIFY(!dirty.ok());
    QVERIFY(dirty.err.contains("commit"));
    QCOMPARE(git({"rev-parse", "HEAD"}), headBefore);  // không bị đổi gì
    git({"checkout", "-q", "--", "f1.txt"});

    // Có merge commit phía trên vùng chọn.
    git({"switch", "-q", "-c", "side", "HEAD~1"});
    commitFile("s.txt", "s\n", "side s");
    git({"switch", "-q", "main"});
    git({"merge", "-q", "--no-ff", "--no-edit", "side"});
    const QByteArray headMerged = git({"rev-parse", "HEAD"});
    QVERIFY(!svc.squashCommits(c3, c2, "x").ok());
    QCOMPARE(git({"rev-parse", "HEAD"}), headMerged);
}

void TstRewrite::squash_replayOfNewerCommitsOnSquashedTreeIsAlwaysClean()
{
    // Cây của commit gom bằng cây của commit mới nhất trong đoạn nên các commit phía trên luôn áp lại
    // sạch, kể cả khi chúng cùng sửa một file.
    commitFile("a.txt", "1\n", "c1");
    commitFile("a.txt", "2\n", "c2");
    commitFile("a.txt", "3\n", "c3");
    commitFile("a.txt", "4\n", "c4");
    GitService svc(path());
    const QString c2 = QString::fromUtf8(git({"rev-parse", "HEAD~2"})).trimmed();
    const QString c3 = QString::fromUtf8(git({"rev-parse", "HEAD~1"})).trimmed();
    QVERIFY(svc.squashCommits(c3, c2, "gom").ok());
    QCOMPARE(read("a.txt"), QByteArray("4\n"));
    QCOMPARE(subjects(), (QStringList{"c1", "gom", "c4"}));
}

void TstRewrite::pushedDetection_countsRewrittenPushedCommits()
{
    QTemporaryDir remote;
    QVERIFY(GitRunner::run(remote.path(), {"init", "--bare", "-q", "-b", "main"}).ok());
    makeLinear(3);
    git({"remote", "add", "origin", remote.path()});
    git({"push", "-q", "-u", "origin", "main"});
    commitFile("local1.txt", "l\n", "local 1");
    commitFile("local2.txt", "l\n", "local 2");

    GitService svc(path());
    const QString c3 = QString::fromUtf8(git({"rev-parse", "HEAD~2"})).trimmed();
    const QString local1 = QString::fromUtf8(git({"rev-parse", "HEAD~1"})).trimmed();
    QVERIFY(svc.isPushed(c3));
    QVERIFY(!svc.isPushed(local1));

    // Rebase lên c1 sẽ viết lại c2, c3 (đã push) và 2 commit local.
    const QString c1 = QString::fromUtf8(git({"rev-parse", "HEAD~4"})).trimmed();
    QCOMPARE(svc.rewrittenPushedCount(c1), 2);
    // Rebase lên chính HEAD~2 (c3): chỉ viết lại commit local -> không có commit đã push bị đụng.
    QCOMPARE(svc.rewrittenPushedCount(c3), 0);
}

void TstRewrite::forcePush_overwritesOnlyWhatUserConfirmed()
{
    QTemporaryDir remote;
    QVERIFY(GitRunner::run(remote.path(), {"init", "--bare", "-q", "-b", "main"}).ok());
    makeLinear(4);
    git({"remote", "add", "origin", remote.path()});
    git({"push", "-q", "-u", "origin", "main"});

    // Gom c3+c4 (đã push) -> lịch sử local phân kỳ với remote.
    GitService svc(path());
    const QString c3 = QString::fromUtf8(git({"rev-parse", "HEAD~1"})).trimmed();
    const QString c4 = QString::fromUtf8(git({"rev-parse", "HEAD"})).trimmed();
    QVERIFY(svc.squashCommits(c4, c3, "gom c3+c4").ok());
    QVERIFY(!svc.push(false).ok());  // push thường bị từ chối

    QString err;
    const auto info = svc.forcePushInfo(&err);
    QVERIFY2(info.has_value(), qPrintable(err));
    QCOMPARE(info->upstream, "origin/main");
    QCOMPARE(info->lost.size(), 2);  // c3 và c4 cũ sẽ bị ghi đè
    QCOMPARE(info->lost.first().subject, "c4");

    // Người khác đẩy thêm commit sau khi ta xem thông tin -> force push phải bị chặn.
    QTemporaryDir other;
    QVERIFY(GitRunner::run(other.path(), {"clone", "-q", remote.path(), "o"}).ok());
    const QString o = other.path() + "/o";
    for (const auto &kv : QList<QPair<QString, QString>>{{"user.name", "O"}, {"user.email", "o@x.com"}, {"commit.gpgsign", "false"}})
        QVERIFY(GitRunner::run(o, {"config", kv.first, kv.second}).ok());
    QFile f(o + "/theirs.txt");
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("t\n");
    f.close();
    QVERIFY(GitRunner::run(o, {"add", "-A"}).ok());
    QVERIFY(GitRunner::run(o, {"commit", "-q", "-m", "người khác"}).ok());
    QVERIFY(GitRunner::run(o, {"push", "-q"}).ok());

    // Dù fetch nền đã cập nhật origin/main, lease vẫn gắn với commit mà người dùng đã xác nhận.
    git({"fetch", "-q"});
    const GitResult stale = svc.forcePush(info->expectedSha);
    QVERIFY(!stale.ok());
    QCOMPARE(git({"--git-dir", remote.path(), "log", "-1", "--format=%s", "main"}).trimmed(),
             QByteArray("người khác").trimmed());

    // Xem lại thông tin mới rồi xác nhận lại -> thành công.
    const auto fresh = svc.forcePushInfo();
    QVERIFY(fresh.has_value());
    QCOMPARE(fresh->lost.size(), 3);
    const GitResult ok = svc.forcePush(fresh->expectedSha);
    QVERIFY2(ok.ok(), qPrintable(ok.err));
    QCOMPARE(git({"--git-dir", remote.path(), "rev-parse", "main"}).trimmed(),
             git({"rev-parse", "HEAD"}).trimmed());
}

QTEST_APPLESS_MAIN(TstRewrite)
#include "tst_rewrite.moc"
