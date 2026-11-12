#include <GitRunner.h>
#include <RepoController.h>

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace gitapp;
using namespace gitcore;

class TstRepoController : public QObject {
    Q_OBJECT
private slots:
    void init();
    void openRepo_rejectsNonRepo();
    void openRepo_listsChanges_andAutoSelectsFirst();
    void untracked_diffShowsContentAsAdditions();
    void toggleChecked_includesAndExcludesWholeFile();
    void commit_validatesInput();
    void commit_succeeds_clearsChanges_andEmitsSignal();
    void modifiedFile_diffShowsAgainstHead();
    void staleDiffResult_isIgnored();
    void diffShowsWorkingTreeAgainstHead_evenWhenPartlyStaged();
    void excludedHunk_isLeftOutOfTheCommit();
    void excludedLines_persistAcrossRefresh_andFileTurnsUncheckedAfterCommit();
    void untrackedFile_canBeCommittedPartially();
    void toggleChecked_updatesDisplayedDiffSelection();
    void commit_isBlockedWhileMergeIsInProgress();
    void push_withoutRemote_reportsError();
    void push_publishesBranch_thenFetchAndPullSync();
    void autoFetch_updatesBehindCountInBackground();
    void switchBranch_withStash_thenRestore();
    void switchBranch_carryingChanges_keepsThem();
    void createBranch_validatesAndSwitches();
    void deleteBranch_unmergedNeedsConfirmation();
    void deleteBranch_currentIsRejected();
    void commit_refreshesHistory();
    void switchBranch_toRemoteBranch_createsTrackingBranch();

private:
    void write(const QString &rel, const QByteArray &data);
    void git(const QStringList &args);
    void openAndWait(RepoController &c);
    static QStringList diffTexts(RepoController &c);
    static int hunkRow(RepoController &c, int nth);
    QByteArray gitOut(const QStringList &args);
    static int checkOf(RepoController &c, int row);
    QString path() const { return m_dir->path(); }

    std::unique_ptr<QTemporaryDir> m_dir;
};

void TstRepoController::write(const QString &rel, const QByteArray &data)
{
    QFile f(path() + QLatin1Char('/') + rel);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

void TstRepoController::git(const QStringList &args)
{
    const GitResult r = GitRunner::run(path(), args);
    QVERIFY2(r.ok(), qPrintable(r.err));
}

void TstRepoController::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    git({"init", "-q"});
    git({"config", "user.name", "Test"});
    git({"config", "user.email", "test@example.com"});
    git({"config", "commit.gpgsign", "false"});
}

QStringList TstRepoController::diffTexts(RepoController &c)
{
    QStringList out;
    QAbstractItemModel *m = c.diff();
    for (int i = 0; i < m->rowCount(); ++i)
        out << m->data(m->index(i, 0), DiffModel::TextRole).toString();
    return out;
}

// Chỉ số dòng của đầu khối @@ thứ `nth` (đếm từ 0), -1 nếu không có.
int TstRepoController::hunkRow(RepoController &c, int nth)
{
    QAbstractItemModel *m = c.diff();
    for (int i = 0; i < m->rowCount(); ++i)
        if (m->data(m->index(i, 0), DiffModel::KindRole).toInt() == DiffModel::Hunk && nth-- == 0)
            return i;
    return -1;
}

int TstRepoController::checkOf(RepoController &c, int row)
{
    return c.changes()->data(c.changes()->index(row, 0), ChangesModel::CheckStateRole).toInt();
}

QByteArray TstRepoController::gitOut(const QStringList &args)
{
    return GitRunner::run(path(), args).out;
}

void TstRepoController::openAndWait(RepoController &c)
{
    c.openRepo(path());
    QTRY_VERIFY(c.hasRepo());
    QTRY_VERIFY(!c.busy());
}

void TstRepoController::openRepo_rejectsNonRepo()
{
    QTemporaryDir plain;
    RepoController c;
    c.openRepo(plain.path());
    QTRY_VERIFY(!c.errorMessage().isEmpty());
    QVERIFY(!c.hasRepo());
    c.clearError();
    QVERIFY(c.errorMessage().isEmpty());
}

void TstRepoController::openRepo_listsChanges_andAutoSelectsFirst()
{
    write("b.txt", "b\n");
    write("a.txt", "a\n");
    RepoController c;
    openAndWait(c);
    QCOMPARE(c.changes()->rowCount(), 2);
    QCOMPARE(c.selectedPath(), "a.txt");  // đã sắp xếp theo tên
    QCOMPARE(c.repoName(), QFileInfo(path()).fileName());
}

void TstRepoController::untracked_diffShowsContentAsAdditions()
{
    write("a.txt", "hello\nworld\n");
    RepoController c;
    openAndWait(c);
    QTRY_VERIFY(diffTexts(c).contains("+hello"));
    QVERIFY(diffTexts(c).contains("+world"));
}

void TstRepoController::toggleChecked_includesAndExcludesWholeFile()
{
    write("a.txt", "x\n");
    RepoController c;
    openAndWait(c);
    QCOMPARE(c.includedCount(), 1);  // kiểu GitHub Desktop: mặc định mọi file được chọn
    QCOMPARE(checkOf(c, 0), 2);

    c.toggleChecked(0);
    QCOMPARE(c.includedCount(), 0);
    QCOMPARE(checkOf(c, 0), 0);
    QCOMPARE(c.selectAllState(), 0);

    c.toggleChecked(0);
    QCOMPARE(c.includedCount(), 1);
    QCOMPARE(checkOf(c, 0), 2);
    QVERIFY(gitOut({"diff", "--cached"}).isEmpty());  // chọn file không đụng tới index của git
}

void TstRepoController::commit_validatesInput()
{
    write("a.txt", "x\n");
    RepoController c;
    openAndWait(c);

    c.commit("   ", "");
    QVERIFY(!c.errorMessage().isEmpty());  // thiếu tiêu đề
    c.clearError();

    c.toggleChecked(0);  // bỏ chọn file duy nhất
    c.commit("tiêu đề", "");
    QVERIFY(c.errorMessage().contains("Chưa chọn"));
    QCOMPARE(gitOut({"rev-list", "--all", "--count"}).trimmed(), QByteArray("0"));
}

void TstRepoController::commit_succeeds_clearsChanges_andEmitsSignal()
{
    write("a.txt", "x\n");
    RepoController c;
    openAndWait(c);
    QCOMPARE(c.includedCount(), 1);

    QSignalSpy committed(&c, &RepoController::committed);
    c.commit("Thêm a.txt", "Mô tả chi tiết");
    QTRY_COMPARE(committed.count(), 1);
    QTRY_VERIFY(!c.busy());
    QCOMPARE(c.changes()->rowCount(), 0);
    QVERIFY(c.errorMessage().isEmpty());
    QVERIFY(c.selectedPath().isEmpty());

    const GitResult log = GitRunner::run(path(), {"log", "-1", "--format=%B"});
    QVERIFY(QString::fromUtf8(log.out).contains("Thêm a.txt"));
    QVERIFY(QString::fromUtf8(log.out).contains("Mô tả chi tiết"));
}

void TstRepoController::modifiedFile_diffShowsAgainstHead()
{
    write("a.txt", "one\n");
    git({"add", "a.txt"});
    git({"commit", "-q", "-m", "init"});
    write("a.txt", "two\n");

    RepoController c;
    openAndWait(c);
    QTRY_VERIFY(diffTexts(c).contains("+two"));
    QVERIFY(diffTexts(c).contains("-one"));
}

void TstRepoController::staleDiffResult_isIgnored()
{
    write("a.txt", "aaa\n");
    write("b.txt", "bbb\n");
    RepoController c;
    openAndWait(c);

    c.selectRow(0);
    c.selectRow(1);  // yêu cầu sau cùng phải thắng dù yêu cầu trước về muộn
    QTRY_COMPARE(c.selectedPath(), "b.txt");
    QTRY_VERIFY(diffTexts(c).contains("+bbb"));
    QTest::qWait(200);
    QVERIFY(diffTexts(c).contains("+bbb"));
    QVERIFY(!diffTexts(c).contains("+aaa"));
}

void TstRepoController::diffShowsWorkingTreeAgainstHead_evenWhenPartlyStaged()
{
    write("a.txt", "one\n");
    git({"add", "a.txt"});
    git({"commit", "-q", "-m", "init"});
    write("a.txt", "two\n");
    git({"add", "a.txt"});     // một phần đã stage...
    write("a.txt", "three\n");  // ...và thêm thay đổi chưa stage

    RepoController c;
    openAndWait(c);
    // Không còn khái niệm "chưa stage / đã stage": chỉ có một diff duy nhất, working tree so với HEAD.
    QTRY_VERIFY(diffTexts(c).contains("+three"));
    QVERIFY(diffTexts(c).contains("-one"));
    QVERIFY(!diffTexts(c).contains("+two"));
}

void TstRepoController::excludedHunk_isLeftOutOfTheCommit()
{
    QByteArray base;
    for (int i = 1; i <= 30; ++i)
        base += "line" + QByteArray::number(i) + "\n";
    write("f.txt", base);
    git({"add", "f.txt"});
    git({"commit", "-q", "-m", "init"});
    QByteArray v2 = base;
    v2.replace("line2\n", "LINE2\n");
    v2.replace("line28\n", "LINE28\n");
    write("f.txt", v2);

    RepoController c;
    openAndWait(c);
    QTRY_VERIFY(diffTexts(c).contains("+LINE28"));
    QVERIFY(c.diffSelectable());
    QCOMPARE(c.diff()->rowCount() > 0, true);

    const int first = hunkRow(c, 0);
    QVERIFY(first >= 0);
    c.toggleDiffRow(first);  // loại khối đầu (LINE2) khỏi commit
    QCOMPARE(checkOf(c, 0), 1);  // file hiện trạng thái "một phần"
    QCOMPARE(c.includedCount(), 1);
    QCOMPARE(c.selectAllState(), 1);

    c.commit("chỉ LINE28", "");
    QTRY_VERIFY(!c.busy());
    QVERIFY(c.errorMessage().isEmpty());
    const QByteArray committed = gitOut({"show", "HEAD:f.txt"});
    QVERIFY(committed.contains("LINE28\n") && committed.contains("line2\n") && !committed.contains("LINE2\n"));
    QCOMPARE(QFile(path() + "/f.txt").size(), v2.size());  // working tree không đổi
    QVERIFY(gitOut({"diff", "HEAD"}).contains("+LINE2\n"));
}

void TstRepoController::excludedLines_persistAcrossRefresh_andFileTurnsUncheckedAfterCommit()
{
    write("f.txt", "a\nb\nc\nd\n");
    write("g.txt", "g\n");
    git({"add", "-A"});
    git({"commit", "-q", "-m", "init"});
    write("f.txt", "a\nB\nc\nD\n");
    write("g.txt", "g2\n");

    RepoController c;
    openAndWait(c);
    QCOMPARE(c.selectedPath(), "f.txt");
    QTRY_VERIFY(diffTexts(c).contains("+D"));

    // Loại cặp (-d, +D) khỏi commit.
    QAbstractItemModel *m = c.diff();
    for (int i = 0; i < m->rowCount(); ++i) {
        const QString t = m->data(m->index(i, 0), DiffModel::TextRole).toString();
        if (t == "-d" || t == "+D")
            c.toggleDiffRow(i);
    }
    QCOMPARE(checkOf(c, 0), 1);

    // Làm mới, rồi chọn file khác và quay lại: lựa chọn được nhớ.
    c.refresh();
    QTRY_VERIFY(!c.busy());
    QCOMPARE(checkOf(c, 0), 1);
    c.selectRow(1);
    QTRY_COMPARE(c.selectedPath(), "g.txt");
    c.selectRow(0);
    QTRY_COMPARE(c.selectedPath(), "f.txt");
    QTRY_VERIFY(diffTexts(c).contains("+D"));
    QCOMPARE(checkOf(c, 0), 1);

    c.commit("chỉ b->B và g", "");
    QTRY_VERIFY(!c.busy());
    QVERIFY(c.errorMessage().isEmpty());
    QCOMPARE(gitOut({"show", "HEAD:f.txt"}), QByteArray("a\nB\nc\nd\n"));
    QCOMPARE(gitOut({"show", "HEAD:g.txt"}), QByteArray("g2\n"));

    // Phần còn lại (d->D) vẫn là thay đổi chưa commit và vẫn đang bị loại: ô tích bỏ trống.
    QCOMPARE(c.changes()->rowCount(), 1);
    QTRY_COMPARE(checkOf(c, 0), 0);
    QCOMPARE(c.includedCount(), 0);
}

void TstRepoController::untrackedFile_canBeCommittedPartially()
{
    write("keep.txt", "k\n");
    git({"add", "-A"});
    git({"commit", "-q", "-m", "base"});
    write("new.txt", "one\ntwo\nthree\n");

    RepoController c;
    openAndWait(c);
    QCOMPARE(c.selectedPath(), "new.txt");
    QTRY_VERIFY(diffTexts(c).contains("+two"));
    QVERIFY(c.diffSelectable());

    QAbstractItemModel *m = c.diff();
    for (int i = 0; i < m->rowCount(); ++i)
        if (m->data(m->index(i, 0), DiffModel::TextRole).toString() == "+two")
            c.toggleDiffRow(i);
    QCOMPARE(checkOf(c, 0), 1);

    c.commit("file mới một phần", "");
    QTRY_VERIFY(!c.busy());
    QVERIFY(c.errorMessage().isEmpty());
    QCOMPARE(gitOut({"show", "HEAD:new.txt"}), QByteArray("one\nthree\n"));
    QCOMPARE(QFile(path() + "/new.txt").size(), qint64(14));
}

void TstRepoController::toggleChecked_updatesDisplayedDiffSelection()
{
    write("a.txt", "x\ny\n");
    RepoController c;
    openAndWait(c);
    QTRY_VERIFY(diffTexts(c).contains("+x"));
    DiffModel *dm = qobject_cast<DiffModel *>(c.diff());
    QVERIFY(dm);
    QCOMPARE(dm->selectedCount(), 2);  // mặc định chọn hết

    c.toggleChecked(0);  // bỏ chọn cả file: các dòng trong diff cũng bỏ chọn
    QCOMPARE(dm->selectedCount(), 0);
    c.selectRow(0);      // tải lại diff: vẫn nhớ trạng thái không chọn
    QTRY_VERIFY(diffTexts(c).contains("+x"));
    QTRY_COMPARE(dm->selectedCount(), 0);
    QCOMPARE(checkOf(c, 0), 0);

    c.toggleChecked(0);
    QCOMPARE(dm->selectedCount(), 2);
}

void TstRepoController::commit_isBlockedWhileMergeIsInProgress()
{
    git({"checkout", "-q", "-b", "main"});
    write("f.txt", "base\n");
    git({"add", "-A"});
    git({"commit", "-q", "-m", "base"});
    git({"switch", "-q", "-c", "other"});
    write("f.txt", "other\n");
    git({"commit", "-q", "-am", "o"});
    git({"switch", "-q", "main"});
    write("f.txt", "main\n");
    git({"commit", "-q", "-am", "m"});
    QVERIFY(!GitRunner::run(path(), {"merge", "other"}).ok());

    RepoController c;
    openAndWait(c);
    c.commit("không được", "");
    QVERIFY(c.errorMessage().contains("merge"));
    QTRY_VERIFY(!c.busy());
    QCOMPARE(c.operation(), "merge");  // trạng thái merge còn nguyên
}

void TstRepoController::push_withoutRemote_reportsError()
{
    write("a.txt", "x\n");
    git({"add", "a.txt"});
    git({"commit", "-q", "-m", "init"});
    RepoController c;
    openAndWait(c);
    QVERIFY(!c.hasUpstream());
    c.push();
    QTRY_VERIFY(!c.errorMessage().isEmpty());
    QTRY_VERIFY(!c.busy());
}

void TstRepoController::push_publishesBranch_thenFetchAndPullSync()
{
    QTemporaryDir remoteDir;
    QVERIFY(remoteDir.isValid());
    QVERIFY(GitRunner::run(remoteDir.path(), {"init", "--bare", "-q", "-b", "main"}).ok());

    git({"checkout", "-q", "-b", "main"});
    write("a.txt", "x\n");
    git({"add", "a.txt"});
    git({"commit", "-q", "-m", "init"});
    git({"remote", "add", "origin", remoteDir.path()});

    RepoController c;
    openAndWait(c);
    QVERIFY(c.canSync());
    QVERIFY(!c.hasUpstream());

    c.push();
    QTRY_VERIFY(c.hasUpstream());
    QTRY_VERIFY(!c.busy());
    QVERIFY(c.errorMessage().isEmpty());
    QVERIFY(!c.notice().isEmpty());

    // Người khác đẩy thêm một commit lên remote.
    QTemporaryDir other;
    QVERIFY(GitRunner::run(other.path(), {"clone", "-q", remoteDir.path(), "o"}).ok());
    const QString o = other.path() + "/o";
    QVERIFY(GitRunner::run(o, {"config", "user.name", "O"}).ok());
    QVERIFY(GitRunner::run(o, {"config", "user.email", "o@example.com"}).ok());
    QVERIFY(GitRunner::run(o, {"config", "commit.gpgsign", "false"}).ok());
    QFile f(o + "/b.txt");
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("b\n");
    f.close();
    QVERIFY(GitRunner::run(o, {"add", "b.txt"}).ok());
    QVERIFY(GitRunner::run(o, {"commit", "-q", "-m", "o1"}).ok());
    QVERIFY(GitRunner::run(o, {"push", "-q"}).ok());

    c.fetch();
    QTRY_COMPARE(c.behind(), 1);
    QTRY_VERIFY(!c.busy());

    c.pull();
    QTRY_COMPARE(c.behind(), 0);
    QTRY_VERIFY(!c.busy());
    QVERIFY(QFile::exists(path() + "/b.txt"));
    QVERIFY(c.errorMessage().isEmpty());
}

void TstRepoController::autoFetch_updatesBehindCountInBackground()
{
    QTemporaryDir remoteDir;
    QVERIFY(GitRunner::run(remoteDir.path(), {"init", "--bare", "-q", "-b", "main"}).ok());
    git({"checkout", "-q", "-b", "main"});
    write("a.txt", "x\n");
    git({"add", "a.txt"});
    git({"commit", "-q", "-m", "init"});
    git({"remote", "add", "origin", remoteDir.path()});
    git({"push", "-q", "-u", "origin", "main"});

    RepoController c;
    openAndWait(c);
    QVERIFY(c.hasUpstream());
    QCOMPARE(c.behind(), 0);

    QTemporaryDir other;
    QVERIFY(GitRunner::run(other.path(), {"clone", "-q", remoteDir.path(), "o"}).ok());
    const QString o = other.path() + "/o";
    QVERIFY(GitRunner::run(o, {"config", "user.name", "O"}).ok());
    QVERIFY(GitRunner::run(o, {"config", "user.email", "o@example.com"}).ok());
    QVERIFY(GitRunner::run(o, {"config", "commit.gpgsign", "false"}).ok());
    QFile f(o + "/b.txt");
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("b\n");
    f.close();
    QVERIFY(GitRunner::run(o, {"add", "b.txt"}).ok());
    QVERIFY(GitRunner::run(o, {"commit", "-q", "-m", "o1"}).ok());
    QVERIFY(GitRunner::run(o, {"push", "-q"}).ok());

    c.setAutoFetchIntervalMs(100);
    QTRY_COMPARE_WITH_TIMEOUT(c.behind(), 1, 8000);
    QVERIFY(c.errorMessage().isEmpty());
}

void TstRepoController::switchBranch_withStash_thenRestore()
{
    git({"checkout", "-q", "-b", "main"});
    write("a.txt", "one\n");
    git({"add", "a.txt"});
    git({"commit", "-q", "-m", "init"});
    git({"branch", "other"});
    write("a.txt", "edited\n");
    write("new.txt", "untracked\n");

    RepoController c;
    openAndWait(c);
    QCOMPARE(c.changeCount(), 2);

    c.switchBranch("other", true);
    QTRY_COMPARE(c.branch(), "other");
    QTRY_VERIFY(!c.busy());
    QCOMPARE(c.changeCount(), 0);
    QVERIFY(!c.hasSavedChanges());

    c.switchBranch("main", false);
    QTRY_COMPARE(c.branch(), "main");
    QTRY_VERIFY(!c.busy());
    QVERIFY(c.hasSavedChanges());  // thay đổi đang chờ ở nhánh main

    c.restoreSavedChanges();
    QTRY_COMPARE(c.changeCount(), 2);
    QTRY_VERIFY(!c.busy());
    QVERIFY(!c.hasSavedChanges());
    QVERIFY(c.errorMessage().isEmpty());
}

void TstRepoController::switchBranch_carryingChanges_keepsThem()
{
    git({"checkout", "-q", "-b", "main"});
    write("a.txt", "one\n");
    git({"add", "a.txt"});
    git({"commit", "-q", "-m", "init"});
    git({"branch", "other"});
    write("a.txt", "edited\n");

    RepoController c;
    openAndWait(c);
    c.switchBranch("other", false);
    QTRY_COMPARE(c.branch(), "other");
    QTRY_VERIFY(!c.busy());
    QCOMPARE(c.changeCount(), 1);
    QVERIFY(!c.hasSavedChanges());
}

void TstRepoController::createBranch_validatesAndSwitches()
{
    git({"checkout", "-q", "-b", "main"});
    write("a.txt", "one\n");
    git({"add", "a.txt"});
    git({"commit", "-q", "-m", "init"});

    RepoController c;
    openAndWait(c);

    c.createBranch("   ", false);
    QVERIFY(!c.errorMessage().isEmpty());
    c.clearError();

    c.createBranch("bad name~", false);
    QTRY_VERIFY(!c.errorMessage().isEmpty());
    QTRY_VERIFY(!c.busy());
    QCOMPARE(c.branch(), "main");
    c.clearError();

    c.createBranch("feature/x", false);
    QTRY_COMPARE(c.branch(), "feature/x");
    QTRY_VERIFY(!c.busy());
    QVERIFY(c.errorMessage().isEmpty());
}

void TstRepoController::deleteBranch_unmergedNeedsConfirmation()
{
    git({"checkout", "-q", "-b", "main"});
    write("a.txt", "one\n");
    git({"add", "a.txt"});
    git({"commit", "-q", "-m", "init"});
    git({"checkout", "-q", "-b", "topic"});
    write("t.txt", "t\n");
    git({"add", "t.txt"});
    git({"commit", "-q", "-m", "unmerged"});
    git({"checkout", "-q", "main"});

    RepoController c;
    openAndWait(c);
    c.deleteBranch("topic");
    QTRY_COMPARE(c.pendingForceDelete(), "topic");
    QTRY_VERIFY(!c.busy());
    QVERIFY(c.errorMessage().isEmpty());  // không phải lỗi, mà là câu hỏi xác nhận
    QVERIFY(gitOut({"branch", "--list", "topic"}).contains("topic"));

    c.cancelForceDelete();
    QVERIFY(c.pendingForceDelete().isEmpty());
    QVERIFY(gitOut({"branch", "--list", "topic"}).contains("topic"));

    c.deleteBranch("topic");
    QTRY_COMPARE(c.pendingForceDelete(), "topic");
    QTRY_VERIFY(!c.busy());
    c.confirmForceDelete();
    QTRY_VERIFY(!c.busy());
    QTRY_VERIFY(!gitOut({"branch", "--list", "topic"}).contains("topic"));
}

void TstRepoController::deleteBranch_currentIsRejected()
{
    git({"checkout", "-q", "-b", "main"});
    write("a.txt", "one\n");
    git({"add", "a.txt"});
    git({"commit", "-q", "-m", "init"});
    RepoController c;
    openAndWait(c);
    c.deleteBranch("main");
    QVERIFY(!c.errorMessage().isEmpty());
    QVERIFY(gitOut({"branch", "--list", "main"}).contains("main"));
}

void TstRepoController::commit_refreshesHistory()
{
    write("a.txt", "one\n");
    git({"add", "a.txt"});
    git({"commit", "-q", "-m", "first"});
    write("b.txt", "two\n");

    RepoController c;
    openAndWait(c);
    HistoryController *h = c.historyController();
    QTRY_COMPARE(h->commits()->rowCount(), 1);

    QCOMPARE(c.includedCount(), 1);
    c.commit("second", "");
    QTRY_COMPARE(h->commits()->rowCount(), 2);  // HEAD đổi -> lịch sử tự nạp lại
    QCOMPARE(h->commits()->data(h->commits()->index(0, 0), CommitsModel::SubjectRole).toString(),
             "second");
}

void TstRepoController::switchBranch_toRemoteBranch_createsTrackingBranch()
{
    QTemporaryDir remoteDir;
    QVERIFY(GitRunner::run(remoteDir.path(), {"init", "--bare", "-q", "-b", "main"}).ok());
    git({"checkout", "-q", "-b", "main"});
    write("a.txt", "one\n");
    git({"add", "a.txt"});
    git({"commit", "-q", "-m", "init"});
    git({"remote", "add", "origin", remoteDir.path()});
    git({"push", "-q", "-u", "origin", "main"});
    git({"push", "-q", "origin", "main:feature"});
    git({"fetch", "-q"});

    RepoController c;
    openAndWait(c);
    c.switchBranch("origin/feature", false);
    QTRY_COMPARE(c.branch(), "feature");
    QTRY_VERIFY(!c.busy());
    QCOMPARE(c.upstream(), "origin/feature");
    QVERIFY(c.errorMessage().isEmpty());

    // Chuyển lại main rồi chọn lại nhánh remote đó: dùng nhánh local sẵn có, không báo lỗi.
    c.switchBranch("main", false);
    QTRY_COMPARE(c.branch(), "main");
    QTRY_VERIFY(!c.busy());
    c.switchBranch("origin/feature", false);
    QTRY_COMPARE(c.branch(), "feature");
    QTRY_VERIFY(!c.busy());
    QVERIFY(c.errorMessage().isEmpty());
}

QTEST_GUILESS_MAIN(TstRepoController)
#include "tst_repocontroller.moc"
