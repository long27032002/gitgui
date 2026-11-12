#include <GitRunner.h>
#include <RepoController.h>

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace gitapp;
using namespace gitcore;

class TstConflictController : public QObject {
    Q_OBJECT
private slots:
    void init();
    void mergeConflict_isReported_withFilesAndBlocks();
    void blockChoices_resolveFile_thenContinueMergeCreatesMergeCommit();
    void applyChoices_isIgnoredUntilEveryBlockIsChosen();
    void wholeFile_andDeleteConflict_canBeResolved();
    void abort_returnsRepoToCleanState();
    void toggleChecked_refusesUnmergedFile();
    void externalEdit_reloadIfChanged_thenMarkResolved();
    void unsupportedMarkers_fallBackToWholeFileActions();
    void rebase_mineMeansYourCommit_forBlocks();
    void rebase_mineMeansYourCommit_forWholeFile();
    void conflictTab_clearsAfterEverythingIsResolved();

private:
    QByteArray git(const QStringList &args, bool *ok = nullptr);
    void write(const QString &rel, const QByteArray &data);
    QByteArray read(const QString &rel);
    void commitAll(const QString &msg);
    void makeMergeConflict();
    void makeRebaseConflict();
    void openAndWait(RepoController &c);
    static int rowOf(ConflictController *cf, const QString &path);
    QString path() const { return m_dir->path(); }

    std::unique_ptr<QTemporaryDir> m_dir;
};

QByteArray TstConflictController::git(const QStringList &args, bool *ok)
{
    const GitResult r = GitRunner::run(path(), args);
    if (ok)
        *ok = r.ok();
    else
        Q_ASSERT_X(r.ok(), "git", qPrintable(args.join(' ') + ": " + r.err));
    return r.out;
}

void TstConflictController::write(const QString &rel, const QByteArray &data)
{
    QFile f(path() + '/' + rel);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(data);
}

QByteArray TstConflictController::read(const QString &rel)
{
    QFile f(path() + '/' + rel);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

void TstConflictController::commitAll(const QString &msg)
{
    git({"add", "-A"});
    git({"commit", "-q", "-m", msg});
}

void TstConflictController::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    git({"init", "-q", "-b", "main"});
    git({"config", "user.name", "Test"});
    git({"config", "user.email", "test@example.com"});
    git({"config", "commit.gpgsign", "false"});
}

// f.txt có HAI khối xung đột (dòng 2 và 11); d.txt: main sửa, other xoá.
void TstConflictController::makeMergeConflict()
{
    QByteArray base;
    for (int i = 1; i <= 12; ++i)
        base += "line" + QByteArray::number(i) + "\n";
    write("f.txt", base);
    write("d.txt", "keep\n");
    commitAll("base");

    git({"switch", "-q", "-c", "other"});
    QByteArray o = base;
    o.replace("line2\n", "OTHER2\n");
    o.replace("line11\n", "OTHER11\n");
    write("f.txt", o);
    git({"rm", "-q", "d.txt"});
    commitAll("other");

    git({"switch", "-q", "main"});
    QByteArray m = base;
    m.replace("line2\n", "MAIN2\n");
    m.replace("line11\n", "MAIN11\n");
    write("f.txt", m);
    write("d.txt", "keep main\n");
    commitAll("main");

    bool ok = true;
    git({"merge", "other"}, &ok);
    QVERIFY(!ok);
}

void TstConflictController::openAndWait(RepoController &c)
{
    c.openRepo(path());
    QTRY_VERIFY(c.hasRepo());
    QTRY_VERIFY(!c.busy());
}

int TstConflictController::rowOf(ConflictController *cf, const QString &path)
{
    QAbstractItemModel *m = cf->files();
    for (int i = 0; i < m->rowCount(); ++i)
        if (m->data(m->index(i, 0), ConflictFilesModel::PathRole).toString() == path)
            return i;
    return -1;
}

void TstConflictController::mergeConflict_isReported_withFilesAndBlocks()
{
    makeMergeConflict();
    RepoController c;
    openAndWait(c);
    QCOMPARE(c.operation(), "merge");
    QCOMPARE(c.conflictCount(), 2);
    QVERIFY(!c.canContinueOperation());

    ConflictController *cf = c.conflictController();
    QCOMPARE(cf->files()->rowCount(), 2);
    QTRY_VERIFY(!cf->loading());
    QCOMPARE(cf->selectedPath(), "d.txt");  // tự chọn file đầu tiên (theo tên)
    QVERIFY(cf->description().contains("xoá"));

    cf->select(rowOf(cf, "f.txt"));
    QTRY_COMPARE(cf->blockCount(), 2);
    QVERIFY(cf->hasBlocks() && cf->hasMarkers() && !cf->binary());
    const QModelIndex first = cf->blocks()->index(0, 0);
    QCOMPARE(cf->blocks()->data(first, ConflictBlocksModel::MineRole).toString(), "MAIN2");
    QCOMPARE(cf->blocks()->data(first, ConflictBlocksModel::IncomingRole).toString(), "OTHER2");
    QVERIFY(!cf->blocks()->data(first, ConflictBlocksModel::AfterRole).toString().isEmpty());
}

void TstConflictController::blockChoices_resolveFile_thenContinueMergeCreatesMergeCommit()
{
    makeMergeConflict();
    RepoController c;
    openAndWait(c);
    ConflictController *cf = c.conflictController();

    cf->select(rowOf(cf, "f.txt"));
    QTRY_COMPARE(cf->blockCount(), 2);
    cf->choose(0, 1);  // bản của tôi
    cf->choose(1, 3);  // giữ cả hai (của tôi trước)
    QCOMPARE(cf->chosenCount(), 2);
    QVERIFY(cf->allChosen());
    cf->applyChoices();
    QTRY_COMPARE(c.conflictCount(), 1);
    QTRY_VERIFY(!c.busy());

    QByteArray expected;
    for (int i = 1; i <= 12; ++i) {
        if (i == 2)
            expected += "MAIN2\n";
        else if (i == 11)
            expected += "MAIN11\nOTHER11\n";
        else
            expected += "line" + QByteArray::number(i) + "\n";
    }
    QCOMPARE(read("f.txt"), expected);

    // d.txt: other xoá, main sửa -> giữ bản của tôi.
    QTRY_COMPARE(cf->selectedPath(), "d.txt");
    cf->resolveWholeFile(true);
    QTRY_COMPARE(c.conflictCount(), 0);
    QTRY_VERIFY(!c.busy());
    QCOMPARE(read("d.txt"), QByteArray("keep main\n"));
    QVERIFY(c.canContinueOperation());

    c.continueOperation();
    QTRY_VERIFY(c.operation().isEmpty());
    QTRY_VERIFY(!c.busy());
    QVERIFY(c.errorMessage().isEmpty());
    QCOMPARE(git({"rev-list", "--parents", "-n", "1", "HEAD"}).trimmed().split(' ').size(), 3);
}

void TstConflictController::applyChoices_isIgnoredUntilEveryBlockIsChosen()
{
    makeMergeConflict();
    RepoController c;
    openAndWait(c);
    ConflictController *cf = c.conflictController();
    cf->select(rowOf(cf, "f.txt"));
    QTRY_COMPARE(cf->blockCount(), 2);

    cf->choose(0, 2);
    QVERIFY(!cf->allChosen());
    cf->applyChoices();  // mới chọn 1/2 khối -> không được ghi gì
    QTest::qWait(150);
    QCOMPARE(c.conflictCount(), 2);
    QVERIFY(read("f.txt").contains("<<<<<<<"));

    cf->choose(0, 0);  // bỏ chọn
    QCOMPARE(cf->chosenCount(), 0);
}

void TstConflictController::wholeFile_andDeleteConflict_canBeResolved()
{
    makeMergeConflict();
    RepoController c;
    openAndWait(c);
    ConflictController *cf = c.conflictController();

    QTRY_COMPARE(cf->selectedPath(), "d.txt");
    QVERIFY(cf->mineAction().contains("bản của tôi"));
    QVERIFY(cf->incomingAction().contains("Xoá file"));  // bên kia đã xoá d.txt
    cf->resolveWholeFile(false);                          // theo bên kia: xoá file
    QTRY_COMPARE(c.conflictCount(), 1);
    QTRY_VERIFY(!c.busy());
    QVERIFY(!QFile::exists(path() + "/d.txt"));

    QTRY_COMPARE(cf->selectedPath(), "f.txt");
    cf->resolveWholeFile(false);  // toàn bộ bản của họ
    QTRY_COMPARE(c.conflictCount(), 0);
    QVERIFY(read("f.txt").contains("OTHER2"));
}

void TstConflictController::abort_returnsRepoToCleanState()
{
    makeMergeConflict();
    RepoController c;
    openAndWait(c);
    c.abortOperation();
    QTRY_VERIFY(c.operation().isEmpty());
    QTRY_VERIFY(!c.busy());
    QCOMPARE(c.conflictCount(), 0);
    QVERIFY(read("f.txt").contains("MAIN2"));
    QVERIFY(!read("f.txt").contains("<<<<<<<"));
}

void TstConflictController::toggleChecked_refusesUnmergedFile()
{
    makeMergeConflict();
    RepoController c;
    openAndWait(c);
    // Hàng đầu tiên của danh sách thay đổi là một file đang xung đột.
    c.toggleChecked(0);
    QVERIFY(!c.errorMessage().isEmpty());
    QTest::qWait(100);
    QCOMPARE(c.conflictCount(), 2);  // không file nào bị `git add` nhầm
}

void TstConflictController::externalEdit_reloadIfChanged_thenMarkResolved()
{
    makeMergeConflict();
    RepoController c;
    openAndWait(c);
    ConflictController *cf = c.conflictController();
    cf->select(rowOf(cf, "f.txt"));
    QTRY_COMPARE(cf->blockCount(), 2);

    cf->reloadIfChanged();  // chưa đổi gì: giữ nguyên trạng thái
    cf->choose(0, 1);
    QCOMPARE(cf->chosenCount(), 1);

    write("f.txt", "sửa tay bằng editor, không còn dấu xung đột\n");
    cf->reloadIfChanged();
    QTRY_VERIFY(!cf->hasBlocks());
    QVERIFY(!cf->hasMarkers());

    cf->markResolved();
    QTRY_COMPARE(c.conflictCount(), 1);  // chỉ còn d.txt
    QTRY_VERIFY(!c.busy());
}

void TstConflictController::unsupportedMarkers_fallBackToWholeFileActions()
{
    write("f.txt", "x\n");
    commitAll("base");
    git({"switch", "-q", "-c", "other"});
    write("f.txt", "other\n");
    commitAll("o");
    git({"switch", "-q", "main"});
    write("f.txt", "main\n");
    commitAll("m");
    bool ok = true;
    git({"merge", "other"}, &ok);
    QVERIFY(!ok);
    write("f.txt", "<<<<<<< HEAD\nmain\n");  // làm hỏng cấu trúc dấu (thiếu phần đóng)

    RepoController c;
    openAndWait(c);
    ConflictController *cf = c.conflictController();
    QTRY_COMPARE(cf->selectedPath(), "f.txt");
    QTRY_VERIFY(!cf->loading());
    QVERIFY(!cf->hasBlocks());
    QVERIFY(cf->hasMarkers());   // vẫn nhận ra là còn dấu để cảnh báo
    cf->resolveWholeFile(true);  // vẫn có lối thoát: dùng nguyên một bên
    QTRY_COMPARE(c.conflictCount(), 0);
    QCOMPARE(read("f.txt"), QByteArray("main\n"));
}

// Đang ở nhánh feature, rebase lên main và gặp conflict ở f.txt.
void TstConflictController::makeRebaseConflict()
{
    write("f.txt", "top\nline\nbottom\n");
    commitAll("base");
    git({"switch", "-q", "-c", "feature"});
    write("f.txt", "top\nFEATURE\nbottom\n");
    commitAll("feature edit");
    git({"switch", "-q", "main"});
    write("f.txt", "top\nMAIN\nbottom\n");
    commitAll("main edit");
    git({"switch", "-q", "feature"});
    bool ok = true;
    git({"rebase", "main"}, &ok);
    QVERIFY(!ok);
}

void TstConflictController::rebase_mineMeansYourCommit_forBlocks()
{
    makeRebaseConflict();
    RepoController c;
    openAndWait(c);
    QCOMPARE(c.operation(), "rebase");
    ConflictController *cf = c.conflictController();
    QTRY_COMPARE(cf->blockCount(), 1);
    const QModelIndex b = cf->blocks()->index(0, 0);
    // "Của tôi" là commit của mình (feature), dù git gọi nó là "theirs" khi rebase.
    QCOMPARE(cf->blocks()->data(b, ConflictBlocksModel::MineRole).toString(), "FEATURE");
    QCOMPARE(cf->blocks()->data(b, ConflictBlocksModel::IncomingRole).toString(), "MAIN");

    cf->choose(0, 1);
    cf->applyChoices();
    QTRY_COMPARE(c.conflictCount(), 0);
    QTRY_VERIFY(!c.busy());
    QCOMPARE(read("f.txt"), QByteArray("top\nFEATURE\nbottom\n"));

    c.continueOperation();
    QTRY_VERIFY(c.operation().isEmpty());
    QTRY_VERIFY(!c.busy());
    QVERIFY(c.errorMessage().isEmpty());
    QCOMPARE(git({"rev-parse", "--abbrev-ref", "HEAD"}).trimmed(), QByteArray("feature"));
    QCOMPARE(git({"rev-list", "--count", "main..feature"}).trimmed(), QByteArray("1"));
}

void TstConflictController::rebase_mineMeansYourCommit_forWholeFile()
{
    makeRebaseConflict();
    RepoController c;
    openAndWait(c);
    ConflictController *cf = c.conflictController();
    QTRY_COMPARE(cf->selectedPath(), "f.txt");

    cf->resolveWholeFile(true);  // "toàn bộ bản của tôi" = commit của mình
    QTRY_COMPARE(c.conflictCount(), 0);
    QTRY_VERIFY(!c.busy());
    QCOMPARE(read("f.txt"), QByteArray("top\nFEATURE\nbottom\n"));
}

void TstConflictController::conflictTab_clearsAfterEverythingIsResolved()
{
    makeMergeConflict();
    RepoController c;
    openAndWait(c);
    c.conflictController()->resolveWholeFile(true);
    QTRY_VERIFY(c.conflictCount() < 2);
    QTRY_VERIFY(!c.busy());
    c.conflictController()->resolveWholeFile(true);
    QTRY_COMPARE(c.conflictCount(), 0);
    QTRY_VERIFY(!c.busy());
    QVERIFY(c.conflictController()->selectedPath().isEmpty());
    QCOMPARE(c.conflictController()->files()->rowCount(), 0);
}

QTEST_GUILESS_MAIN(TstConflictController)
#include "tst_conflictcontroller.moc"
