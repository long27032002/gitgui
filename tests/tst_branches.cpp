// Nhánh, stash-khi-chuyển-nhánh và lịch sử — kiểm thử với git thật.
#include <GitParser.h>
#include <GitRunner.h>
#include <GitService.h>

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace gitcore;

class TstBranches : public QObject {
    Q_OBJECT
private slots:
    void init();

    void parseBranches_skipsRemoteHead_andMarksCurrent();
    void parseLog_readsAllFields();
    void parseNameStatus_handlesRenames();

    void branches_listsLocalAndRemote();
    void logAll_includesOtherBranchesAndMarksCurrentReachability();
    void create_switchesAndCarriesChanges();
    void create_rejectsInvalidName_withoutStashing();
    void create_failsWhenNameExists();
    void createFromCommit_startsAtSelectedCommit();
    void switchExisting_blockedByConflictingChanges_reportsGitError();
    void switchWithStash_cleansTree_andRestoreBringsBackEverything();
    void switchWithStash_onCleanTree_createsNoStash();
    void switchWithStash_restoresChangesWhenSwitchFails();
    void trackRemote_createsLocalBranchWithUpstream();
    void delete_safeRefusesUnmerged_forceDeletes();
    void delete_currentBranchFails();
    void rename_movesBranch();
    void discardSavedChanges_dropsStash();

    void log_pagesAndHandlesEmptyRepo();
    void commitFiles_andDiff_forNormalRootAndMergeCommits();

private:
    QByteArray git(const QStringList &args, bool *ok = nullptr);
    void write(const QString &rel, const QByteArray &data);
    void commitFile(const QString &rel, const QByteArray &data, const QString &msg);
    QString path() const { return m_dir->path(); }

    std::unique_ptr<QTemporaryDir> m_dir;
};

QByteArray TstBranches::git(const QStringList &args, bool *ok)
{
    const GitResult r = GitRunner::run(path(), args);
    if (ok)
        *ok = r.ok();
    else
        Q_ASSERT_X(r.ok(), "git", qPrintable(args.join(' ') + ": " + r.err));
    return r.out;
}

void TstBranches::write(const QString &rel, const QByteArray &data)
{
    QFile f(path() + '/' + rel);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(data);
}

void TstBranches::commitFile(const QString &rel, const QByteArray &data, const QString &msg)
{
    write(rel, data);
    git({"add", "-A"});
    git({"commit", "-q", "-m", msg});
}

void TstBranches::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    git({"init", "-q", "-b", "main"});
    git({"config", "user.name", "Test"});
    git({"config", "user.email", "test@example.com"});
    git({"config", "commit.gpgsign", "false"});
}

// ---------- parser ----------

void TstBranches::parseBranches_skipsRemoteHead_andMarksCurrent()
{
    const QByteArray raw =
        "refs/heads/main\tmain\torigin/main\t*\n"
        "refs/heads/dev\tdev\t\t \n"
        "refs/remotes/origin/HEAD\torigin\t\t \n"
        "refs/remotes/origin/main\torigin/main\t\t \n";
    const auto b = GitParser::parseBranches(raw);
    QCOMPARE(b.size(), 3);
    QCOMPARE(b[0].name, "main");
    QVERIFY(b[0].isCurrent && !b[0].isRemote);
    QCOMPARE(b[0].upstream, "origin/main");
    QVERIFY(!b[1].isCurrent && b[1].upstream.isEmpty());
    QVERIFY(b[2].isRemote);
}

void TstBranches::parseLog_readsAllFields()
{
    const QByteArray raw =
        "aaaa1111\x1f" "aaaa\x1f" "Nguyễn A\x1f" "1700000000\x1f" "p1 p2\x1f"
        "Merge nhánh\x1f" "HEAD -> main\x1e\n"
        "bbbb2222\x1f" "bbbb\x1f" "B\x1f" "1600000000\x1f\x1f" "Gốc\x1f\x1e\n";
    const auto c = GitParser::parseLog(raw);
    QCOMPARE(c.size(), 2);
    QCOMPARE(c[0].hash, "aaaa1111");
    QCOMPARE(c[0].author, QStringLiteral("Nguyễn A"));
    QCOMPARE(c[0].timestamp, qint64(1700000000));
    QCOMPARE(c[0].parentCount, 2);
    QCOMPARE(c[0].refs, "HEAD -> main");
    QCOMPARE(c[1].parentCount, 0);
    QCOMPARE(c[1].subject, QStringLiteral("Gốc"));
}

void TstBranches::parseNameStatus_handlesRenames()
{
    const QByteArray raw = QByteArray("M\0a.txt\0A\0b.txt\0R100\0old.txt\0new.txt\0D\0c.txt\0", 44);
    const auto f = GitParser::parseNameStatus(raw);
    QCOMPARE(f.size(), 4);
    QCOMPARE(f[1].status, QLatin1Char('A'));
    QCOMPARE(f[2].status, QLatin1Char('R'));
    QCOMPARE(f[2].origPath, "old.txt");
    QCOMPARE(f[2].path, "new.txt");
    QCOMPARE(f[3].path, "c.txt");
}

// ---------- nhánh ----------

void TstBranches::branches_listsLocalAndRemote()
{
    commitFile("f.txt", "1\n", "c1");
    QTemporaryDir remote;
    QVERIFY(GitRunner::run(remote.path(), {"init", "--bare", "-q", "-b", "main"}).ok());
    git({"remote", "add", "origin", remote.path()});
    git({"push", "-q", "-u", "origin", "main"});
    git({"branch", "dev"});

    const auto b = GitService(path()).branches();
    QVERIFY(b.has_value());
    int locals = 0, remotes = 0;
    for (const BranchInfo &x : *b) {
        (x.isRemote ? remotes : locals)++;
        if (x.name == "main" && !x.isRemote)
            QVERIFY(x.isCurrent && x.upstream == "origin/main");
    }
    QCOMPARE(locals, 2);
    QCOMPARE(remotes, 1);
}

void TstBranches::logAll_includesOtherBranchesAndMarksCurrentReachability()
{
    commitFile("base.txt", "base\n", "base");
    git({"switch", "-q", "-c", "feature"});
    commitFile("feature.txt", "feature\n", "feature work");
    const QString featureCommit = QString::fromUtf8(git({"rev-parse", "HEAD"})).trimmed();
    git({"switch", "-q", "main"});
    commitFile("main.txt", "main\n", "main work");

    GitService svc(path());
    const auto commits = svc.logAll(0, 100);
    const QSet<QString> currentHashes = svc.currentBranchHashes();
    QVERIFY(commits.has_value());
    QCOMPARE(commits->size(), 3);
    bool sawFeature = false;
    for (const CommitInfo &commit : *commits) {
        if (commit.hash == featureCommit) {
            sawFeature = true;
            QVERIFY(!currentHashes.contains(commit.hash));
            QVERIFY(commit.refs.contains("feature"));
        }
        if (commit.subject == "main work" || commit.subject == "base")
            QVERIFY(currentHashes.contains(commit.hash));
    }
    QVERIFY(sawFeature);
    QCOMPARE(currentHashes.size(), 2);
}

void TstBranches::create_switchesAndCarriesChanges()
{
    commitFile("f.txt", "1\n", "c1");
    write("f.txt", "changed\n");
    write("new.txt", "untracked\n");

    const GitResult r = GitService(path()).switchTo("feature", SwitchKind::CreateNew);
    QVERIFY2(r.ok(), qPrintable(r.err));
    QCOMPARE(git({"rev-parse", "--abbrev-ref", "HEAD"}).trimmed(), QByteArray("feature"));
    QCOMPARE(GitService(path()).status()->changes.size(), 2);  // thay đổi đi theo sang nhánh mới
}

void TstBranches::create_rejectsInvalidName_withoutStashing()
{
    commitFile("f.txt", "1\n", "c1");
    write("f.txt", "changed\n");
    GitService svc(path());
    const GitResult r = svc.switchTo("bad name~^", SwitchKind::CreateNew, "main");
    QVERIFY(!r.ok());
    QVERIFY(r.err.contains("không hợp lệ"));
    QVERIFY(!svc.hasSavedChanges("main"));
    QCOMPARE(svc.status()->changes.size(), 1);  // thay đổi còn nguyên tại chỗ
}

void TstBranches::create_failsWhenNameExists()
{
    commitFile("f.txt", "1\n", "c1");
    git({"branch", "dev"});
    QVERIFY(!GitService(path()).switchTo("dev", SwitchKind::CreateNew).ok());
}

void TstBranches::createFromCommit_startsAtSelectedCommit()
{
    commitFile("f.txt", "1\n", "c1");
    const QByteArray base = git({"rev-parse", "HEAD"}).trimmed();
    commitFile("f.txt", "2\n", "c2");
    const GitResult r = GitService(path()).switchTo("older-point", SwitchKind::CreateNew, {},
                                                    QString::fromLatin1(base));
    QVERIFY2(r.ok(), qPrintable(r.err));
    QCOMPARE(git({"rev-parse", "HEAD"}).trimmed(), base);
    QCOMPARE(git({"rev-parse", "--abbrev-ref", "HEAD"}).trimmed(), QByteArray("older-point"));
}

void TstBranches::switchExisting_blockedByConflictingChanges_reportsGitError()
{
    commitFile("f.txt", "1\n", "c1");
    git({"switch", "-q", "-c", "other"});
    commitFile("f.txt", "other\n", "c2");
    git({"switch", "-q", "main"});
    write("f.txt", "local edit\n");

    const GitResult r = GitService(path()).switchTo("other", SwitchKind::Existing);
    QVERIFY(!r.ok());
    QVERIFY(!r.err.trimmed().isEmpty());
    QCOMPARE(git({"rev-parse", "--abbrev-ref", "HEAD"}).trimmed(), QByteArray("main"));
}

void TstBranches::switchWithStash_cleansTree_andRestoreBringsBackEverything()
{
    commitFile("f.txt", "1\n", "c1");
    git({"branch", "other"});
    write("f.txt", "edited\n");
    write("untracked.txt", "u\n");

    GitService svc(path());
    const GitResult r = svc.switchTo("other", SwitchKind::Existing, "main");
    QVERIFY2(r.ok(), qPrintable(r.err));
    QCOMPARE(git({"rev-parse", "--abbrev-ref", "HEAD"}).trimmed(), QByteArray("other"));
    QVERIFY(svc.status()->changes.isEmpty());  // cây làm việc sạch
    QVERIFY(svc.hasSavedChanges("main"));
    QVERIFY(!svc.hasSavedChanges("other"));

    QVERIFY(svc.switchTo("main", SwitchKind::Existing).ok());
    const GitResult restore = svc.restoreSavedChanges("main");
    QVERIFY2(restore.ok(), qPrintable(restore.err));
    QVERIFY(!svc.hasSavedChanges("main"));
    QCOMPARE(svc.status()->changes.size(), 2);
    QVERIFY(QFile::exists(path() + "/untracked.txt"));
}

void TstBranches::switchWithStash_onCleanTree_createsNoStash()
{
    commitFile("f.txt", "1\n", "c1");
    git({"branch", "other"});
    GitService svc(path());
    QVERIFY(svc.switchTo("other", SwitchKind::Existing, "main").ok());
    QVERIFY(!svc.hasSavedChanges("main"));
    QVERIFY(git({"stash", "list"}).isEmpty());  // không được tạo stash rỗng/thừa
}

void TstBranches::switchWithStash_restoresChangesWhenSwitchFails()
{
    commitFile("f.txt", "1\n", "c1");
    write("f.txt", "edited\n");
    GitService svc(path());
    const GitResult r = svc.switchTo("khong-ton-tai", SwitchKind::Existing, "main");
    QVERIFY(!r.ok());
    QCOMPARE(svc.status()->changes.size(), 1);  // thay đổi đã được trả lại
    QVERIFY(git({"stash", "list"}).isEmpty());
    QCOMPARE(QFile(path() + "/f.txt").size() > 0, true);
}

void TstBranches::trackRemote_createsLocalBranchWithUpstream()
{
    commitFile("f.txt", "1\n", "c1");
    QTemporaryDir remote;
    QVERIFY(GitRunner::run(remote.path(), {"init", "--bare", "-q", "-b", "main"}).ok());
    git({"remote", "add", "origin", remote.path()});
    git({"push", "-q", "-u", "origin", "main"});
    git({"push", "-q", "origin", "main:feature"});
    git({"fetch", "-q"});

    const GitResult r = GitService(path()).switchTo("origin/feature", SwitchKind::TrackRemote);
    QVERIFY2(r.ok(), qPrintable(r.err));
    QCOMPARE(git({"rev-parse", "--abbrev-ref", "HEAD"}).trimmed(), QByteArray("feature"));
    QCOMPARE(git({"rev-parse", "--abbrev-ref", "@{u}"}).trimmed(), QByteArray("origin/feature"));
}

void TstBranches::delete_safeRefusesUnmerged_forceDeletes()
{
    commitFile("f.txt", "1\n", "c1");
    git({"switch", "-q", "-c", "topic"});
    commitFile("t.txt", "t\n", "unmerged");
    git({"switch", "-q", "main"});

    GitService svc(path());
    const GitResult safe = svc.deleteBranch("topic", false);
    QVERIFY(!safe.ok());
    QVERIFY(safe.err.contains("not fully merged"));
    QVERIFY(svc.deleteBranch("topic", true).ok());
    QVERIFY(!git({"branch", "--list", "topic"}).contains("topic"));
}

void TstBranches::delete_currentBranchFails()
{
    commitFile("f.txt", "1\n", "c1");
    QVERIFY(!GitService(path()).deleteBranch("main", true).ok());
}

void TstBranches::rename_movesBranch()
{
    commitFile("f.txt", "1\n", "c1");
    git({"branch", "old"});
    GitService svc(path());
    QVERIFY(svc.renameBranch("old", "new-name").ok());
    QVERIFY(git({"branch", "--list", "new-name"}).contains("new-name"));
    QVERIFY(!svc.renameBranch("new-name", "bad name~").ok());
}

void TstBranches::discardSavedChanges_dropsStash()
{
    commitFile("f.txt", "1\n", "c1");
    git({"branch", "other"});
    write("f.txt", "edited\n");
    GitService svc(path());
    QVERIFY(svc.switchTo("other", SwitchKind::Existing, "main").ok());
    QVERIFY(svc.hasSavedChanges("main"));
    QVERIFY(svc.discardSavedChanges("main").ok());
    QVERIFY(!svc.hasSavedChanges("main"));
    QVERIFY(!svc.discardSavedChanges("main").ok());
}

// ---------- lịch sử ----------

void TstBranches::log_pagesAndHandlesEmptyRepo()
{
    GitService svc(path());
    const auto empty = svc.log(0, 10);
    QVERIFY(empty.has_value() && empty->isEmpty());

    for (int i = 1; i <= 5; ++i)
        commitFile("f.txt", QByteArray::number(i) + "\n", QString("commit %1").arg(i));
    const auto page1 = svc.log(0, 2);
    const auto page2 = svc.log(2, 2);
    const auto page3 = svc.log(4, 2);
    QCOMPARE(page1->size(), 2);
    QCOMPARE(page2->size(), 2);
    QCOMPARE(page3->size(), 1);
    QCOMPARE(page1->at(0).subject, "commit 5");
    QCOMPARE(page3->at(0).subject, "commit 1");
    QCOMPARE(page3->at(0).parentCount, 0);
    QCOMPARE(page1->at(0).author, "Test");
    QVERIFY(page1->at(0).timestamp > 0);
}

void TstBranches::commitFiles_andDiff_forNormalRootAndMergeCommits()
{
    commitFile("a.txt", "a1\n", "root");
    commitFile("b.txt", "b1\n", "add b");
    git({"switch", "-q", "-c", "side"});
    commitFile("c.txt", "c1\n", "add c");
    git({"switch", "-q", "main"});
    commitFile("a.txt", "a2\n", "edit a");
    git({"merge", "-q", "--no-edit", "side"});

    GitService svc(path());
    const auto log = svc.log(0, 10);
    QVERIFY(log && log->size() == 5);

    // commit gốc
    const CommitInfo root = log->last();
    const auto rootFiles = svc.commitFiles(root.hash);
    QVERIFY(rootFiles && rootFiles->size() == 1);
    QCOMPARE(rootFiles->at(0).status, QLatin1Char('A'));
    QVERIFY(svc.commitFileDiff(root.hash, "a.txt").out.contains("+a1"));
    QCOMPARE(svc.commitMessage(root.hash), "root");

    // commit thường
    const CommitInfo editA = *std::find_if(log->begin(), log->end(),
                                           [](const CommitInfo &c) { return c.subject == "edit a"; });
    const auto editFiles = svc.commitFiles(editA.hash);
    QCOMPARE(editFiles->at(0).status, QLatin1Char('M'));
    const QByteArray d = svc.commitFileDiff(editA.hash, "a.txt").out;
    QVERIFY(d.contains("-a1") && d.contains("+a2"));

    // merge commit: so với cha đầu tiên -> thấy file kéo từ nhánh side
    const CommitInfo merge = log->first();
    QCOMPARE(merge.parentCount, 2);
    const auto mergeFiles = svc.commitFiles(merge.hash);
    QVERIFY(mergeFiles && mergeFiles->size() == 1);
    QCOMPARE(mergeFiles->at(0).path, "c.txt");
    QVERIFY(svc.commitFileDiff(merge.hash, "c.txt").out.contains("+c1"));
}

QTEST_APPLESS_MAIN(TstBranches)
#include "tst_branches.moc"
