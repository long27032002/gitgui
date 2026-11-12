#include <GitRunner.h>
#include <RepoController.h>

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace gitapp;
using namespace gitcore;

class TstPolishController : public QObject {
    Q_OBJECT
private slots:
    void init();

    void splitMessage_separatesSubjectBodyAndCoAuthors();
    void canUndoAndAmend_followHeadState();
    void undoLastCommit_emitsMessageParts_andReturnsChangesToTheList();
    void undoLastCommit_isRefusedForPushedCommit();
    void amend_withSelectionAndWithoutSelection();
    void amend_isRefusedWhenPushed_andCommitStillBlocksEmptySelection();
    void discardFiles_thenUndo();
    void discardAll_skipsConflictedFiles();
    void discardDiffRow_removesOneLineOrHunk_andCanBeUndone();
    void discardDiffRow_workOnUntrackedFileLines();
    void ignore_filesExtensionsAndFolders();
    void ignore_escapesSpecialCharacters_andRejectsImpossibleRequests();
    void createRepository_opensItAndMakesInitialCommit();
    void createRepository_failureIsReported();
    void createRepository_missingIdentityStillOpensRepoWithWarning();

private:
    QByteArray git(const QStringList &args, bool *ok = nullptr);
    void write(const QString &rel, const QByteArray &data);
    QByteArray read(const QString &rel);
    void commitAll(const QString &msg);
    void openAndWait(RepoController &c);
    static QStringList diffTexts(RepoController &c);
    static int rowWithText(RepoController &c, const QString &text);
    QString path() const { return m_dir->path(); }

    std::unique_ptr<QTemporaryDir> m_dir;
};

QByteArray TstPolishController::git(const QStringList &args, bool *ok)
{
    const GitResult r = GitRunner::run(path(), args);
    if (ok)
        *ok = r.ok();
    else
        Q_ASSERT_X(r.ok(), "git", qPrintable(args.join(' ') + ": " + r.err));
    return r.out;
}

void TstPolishController::write(const QString &rel, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(path() + '/' + rel).absolutePath());
    QFile f(path() + '/' + rel);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(data);
}

QByteArray TstPolishController::read(const QString &rel)
{
    QFile f(path() + '/' + rel);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

void TstPolishController::commitAll(const QString &msg)
{
    git({"add", "-A"});
    git({"commit", "-q", "-m", msg});
}

void TstPolishController::openAndWait(RepoController &c)
{
    c.openRepo(path());
    QTRY_VERIFY(c.hasRepo());
    QTRY_VERIFY(!c.busy());
}

QStringList TstPolishController::diffTexts(RepoController &c)
{
    QStringList out;
    QAbstractItemModel *m = c.diff();
    for (int i = 0; i < m->rowCount(); ++i)
        out << m->data(m->index(i, 0), DiffModel::TextRole).toString();
    return out;
}

int TstPolishController::rowWithText(RepoController &c, const QString &text)
{
    QAbstractItemModel *m = c.diff();
    for (int i = 0; i < m->rowCount(); ++i)
        if (m->data(m->index(i, 0), DiffModel::TextRole).toString() == text)
            return i;
    return -1;
}

void TstPolishController::init()
{
    qputenv("GIT_AUTHOR_NAME", "Test");
    qputenv("GIT_AUTHOR_EMAIL", "test@example.com");
    qputenv("GIT_COMMITTER_NAME", "Test");
    qputenv("GIT_COMMITTER_EMAIL", "test@example.com");
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    git({"init", "-q", "-b", "main"});
    git({"config", "user.name", "Test"});
    git({"config", "user.email", "test@example.com"});
    git({"config", "commit.gpgsign", "false"});
}

void TstPolishController::splitMessage_separatesSubjectBodyAndCoAuthors()
{
    auto m = RepoController::splitMessage("Tiêu đề\n\nThân 1\nThân 2\n\nCo-authored-by: A <a@x.io>\nCo-authored-by: B <b@x.io>\n");
    QCOMPARE(m["subject"].toString(), "Tiêu đề");
    QCOMPARE(m["body"].toString(), "Thân 1\nThân 2");
    QCOMPARE(m["coAuthors"].toString(), "A <a@x.io>, B <b@x.io>");

    m = RepoController::splitMessage("Chỉ có tiêu đề");
    QCOMPARE(m["subject"].toString(), "Chỉ có tiêu đề");
    QVERIFY(m["body"].toString().isEmpty() && m["coAuthors"].toString().isEmpty());

    m = RepoController::splitMessage("Fix\n\nSigned-off-by: X <x@y.z>");  // trailer khác được giữ trong thân
    QCOMPARE(m["body"].toString(), "Signed-off-by: X <x@y.z>");
    QVERIFY(RepoController::splitMessage("").value("subject").toString().isEmpty());
}

void TstPolishController::canUndoAndAmend_followHeadState()
{
    RepoController c;
    openAndWait(c);
    QVERIFY(!c.canUndoCommit() && !c.canAmend());  // chưa có commit

    write("a.txt", "1\n");
    commitAll("root");
    c.refresh();
    QTRY_VERIFY(c.canAmend());
    QVERIFY(!c.canUndoCommit());  // commit gốc: không hoàn tác được, nhưng sửa được

    write("a.txt", "2\n");
    commitAll("tiêu đề thứ hai\n\nthân");
    c.refresh();
    QTRY_VERIFY(c.canUndoCommit());
    QCOMPARE(c.lastCommitSubject(), "tiêu đề thứ hai");
    QCOMPARE(c.lastCommitParts().value("body").toString(), "thân");
}

void TstPolishController::undoLastCommit_emitsMessageParts_andReturnsChangesToTheList()
{
    write("a.txt", "1\n");
    commitAll("base");
    write("b.txt", "b\n");
    commitAll("Thêm b\n\nGhi chú dài\n\nCo-authored-by: Bob <bob@example.com>");

    RepoController c;
    openAndWait(c);
    QVERIFY(c.canUndoCommit());
    QSignalSpy undone(&c, &RepoController::commitUndone);
    c.undoLastCommit();
    QTRY_COMPARE(undone.count(), 1);
    QTRY_VERIFY(!c.busy());
    QCOMPARE(undone.first()[0].toString(), "Thêm b");
    QCOMPARE(undone.first()[1].toString(), "Ghi chú dài");
    QCOMPARE(undone.first()[2].toString(), "Bob <bob@example.com>");
    QCOMPARE(c.changeCount(), 1);      // b.txt trở lại danh sách thay đổi (chưa theo dõi)
    QCOMPARE(c.includedCount(), 1);    // và mặc định được chọn sẵn
    QVERIFY(c.errorMessage().isEmpty());
    QTRY_VERIFY(!c.canUndoCommit());   // giờ HEAD là commit gốc "base"
}

void TstPolishController::undoLastCommit_isRefusedForPushedCommit()
{
    write("a.txt", "1\n");
    commitAll("base");
    write("a.txt", "2\n");
    commitAll("đã push");
    QTemporaryDir remote;
    QVERIFY(GitRunner::run(remote.path(), {"init", "--bare", "-q", "-b", "main"}).ok());
    git({"remote", "add", "origin", remote.path()});
    git({"push", "-q", "-u", "origin", "main"});

    RepoController c;
    openAndWait(c);
    QVERIFY(!c.canUndoCommit());
    QVERIFY(!c.canAmend());
    c.undoLastCommit();
    QVERIFY(!c.errorMessage().isEmpty());
    QCOMPARE(git({"log", "-1", "--format=%s"}).trimmed(), QByteArray("đã push"));
}

void TstPolishController::amend_withSelectionAndWithoutSelection()
{
    write("a.txt", "1\n");
    commitAll("base");
    write("b.txt", "b\n");
    commitAll("quên c");
    write("c.txt", "c\n");

    RepoController c;
    openAndWait(c);
    QVERIFY(c.canAmend());
    c.commit("Thêm b và c", "", "", true);
    QTRY_VERIFY(!c.busy());
    QVERIFY(c.errorMessage().isEmpty());
    QCOMPARE(git({"rev-list", "--count", "HEAD"}).trimmed(), QByteArray("2"));
    QCOMPARE(git({"log", "-1", "--format=%s"}).trimmed(), QByteArray("Thêm b và c"));
    QVERIFY(QString::fromUtf8(git({"show", "--name-only", "--format=", "HEAD"})).contains("c.txt"));

    // Không chọn gì (bỏ chọn mọi file) vẫn amend được để đổi message.
    write("d.txt", "d\n");
    c.refresh();
    QTRY_COMPARE(c.changeCount(), 1);
    c.toggleAll();
    QCOMPARE(c.includedCount(), 0);
    c.commit("Chỉ đổi message", "", "", true);
    QTRY_VERIFY(!c.busy());
    QVERIFY(c.errorMessage().isEmpty());
    QCOMPARE(git({"log", "-1", "--format=%s"}).trimmed(), QByteArray("Chỉ đổi message"));
    QVERIFY(QFile::exists(path() + "/d.txt"));
    QVERIFY(!QString::fromUtf8(git({"show", "--name-only", "--format=", "HEAD"})).contains("d.txt"));
}

void TstPolishController::amend_isRefusedWhenPushed_andCommitStillBlocksEmptySelection()
{
    write("a.txt", "1\n");
    commitAll("base");
    QTemporaryDir remote;
    QVERIFY(GitRunner::run(remote.path(), {"init", "--bare", "-q", "-b", "main"}).ok());
    git({"remote", "add", "origin", remote.path()});
    git({"push", "-q", "-u", "origin", "main"});
    write("b.txt", "b\n");

    RepoController c;
    openAndWait(c);
    c.commit("x", "", "", true);
    QVERIFY(c.errorMessage().contains("push"));
    c.clearError();

    c.toggleAll();  // bỏ chọn hết rồi commit thường: vẫn bị từ chối
    c.commit("y", "");
    QVERIFY(c.errorMessage().contains("Chưa chọn"));
}

void TstPolishController::discardFiles_thenUndo()
{
    write("a.txt", "goc\n");
    write("b.txt", "goc b\n");
    commitAll("base");
    write("a.txt", "đã sửa\n");
    write("b.txt", "đã sửa b\n");
    write("new.txt", "mới\n");

    RepoController c;
    openAndWait(c);
    QCOMPARE(c.changeCount(), 3);
    QVERIFY(!c.canUndoDiscard());

    c.discardFiles({"a.txt", "new.txt"});
    QTRY_VERIFY(c.canUndoDiscard());
    QTRY_VERIFY(!c.busy());
    QCOMPARE(read("a.txt"), QByteArray("goc\n"));
    QVERIFY(!QFile::exists(path() + "/new.txt"));
    QCOMPARE(read("b.txt"), QByteArray("đã sửa b\n"));  // file không chọn không bị đụng
    QCOMPARE(c.changeCount(), 1);
    QVERIFY(c.lastDiscardText().contains("2 file"));

    c.undoDiscard();
    QTRY_VERIFY(!c.canUndoDiscard());
    QTRY_VERIFY(!c.busy());
    QCOMPARE(read("a.txt"), QByteArray("đã sửa\n"));
    QCOMPARE(read("new.txt"), QByteArray("mới\n"));
    QCOMPARE(c.changeCount(), 3);
}

void TstPolishController::discardAll_skipsConflictedFiles()
{
    write("f.txt", "base\n");
    write("ok.txt", "ok\n");
    commitAll("base");
    git({"switch", "-q", "-c", "other"});
    write("f.txt", "other\n");
    git({"commit", "-q", "-am", "o"});
    git({"switch", "-q", "main"});
    write("f.txt", "main\n");
    git({"commit", "-q", "-am", "m"});
    QVERIFY(!GitRunner::run(path(), {"merge", "other"}).ok());
    write("ok.txt", "đã sửa\n");

    RepoController c;
    openAndWait(c);
    c.discardAll();
    QTRY_VERIFY(c.canUndoDiscard());
    QTRY_VERIFY(!c.busy());
    QCOMPARE(read("ok.txt"), QByteArray("ok\n"));
    QVERIFY(read("f.txt").contains("<<<<<<<"));  // file xung đột không bị bỏ
    QCOMPARE(c.conflictCount(), 1);
}

void TstPolishController::discardDiffRow_removesOneLineOrHunk_andCanBeUndone()
{
    QByteArray base;
    for (int i = 1; i <= 30; ++i)
        base += "l" + QByteArray::number(i) + "\n";
    write("f.txt", base);
    commitAll("base");
    QByteArray v2 = base;
    v2.replace("l3\n", "L3\n");
    v2.replace("l27\n", "L27\n");
    write("f.txt", v2);

    RepoController c;
    openAndWait(c);
    QTRY_VERIFY(diffTexts(c).contains("+L27"));

    c.discardDiffRow(rowWithText(c, "+L27"));  // bỏ riêng dòng thêm L27 (dòng xoá l27 vẫn còn)
    QTRY_VERIFY(c.canUndoDiscard());
    QTRY_VERIFY(!c.busy());
    QVERIFY(c.errorMessage().isEmpty());
    QVERIFY(!read("f.txt").contains("L27\n"));
    QVERIFY(read("f.txt").contains("L3\n"));

    c.undoDiscard();
    QTRY_VERIFY(!c.canUndoDiscard());
    QTRY_VERIFY(!c.busy());
    QCOMPARE(read("f.txt"), v2);

    // Bỏ cả khối: hàng đầu khối thứ hai (@@) gồm l27 -> L27.
    QTRY_VERIFY(diffTexts(c).contains("+L27"));
    int hunkRow = -1;
    QAbstractItemModel *m = c.diff();
    int seen = 0;
    for (int i = 0; i < m->rowCount(); ++i)
        if (m->data(m->index(i, 0), DiffModel::KindRole).toInt() == DiffModel::Hunk && seen++ == 1)
            hunkRow = i;
    QVERIFY(hunkRow > 0);
    c.discardDiffRow(hunkRow);
    QTRY_VERIFY(c.canUndoDiscard());
    QTRY_VERIFY(!c.busy());
    QVERIFY(read("f.txt").contains("l27\n") && !read("f.txt").contains("L27\n"));
    QVERIFY(read("f.txt").contains("L3\n"));
}

void TstPolishController::discardDiffRow_workOnUntrackedFileLines()
{
    write("keep.txt", "k\n");
    commitAll("base");
    write("new.txt", "một\nhai\nba\n");

    RepoController c;
    openAndWait(c);
    QTRY_VERIFY(diffTexts(c).contains("+hai"));
    c.discardDiffRow(rowWithText(c, "+hai"));
    QTRY_VERIFY(c.canUndoDiscard());
    QTRY_VERIFY(!c.busy());
    QVERIFY(c.errorMessage().isEmpty());
    QCOMPARE(read("new.txt"), QByteArray("một\nba\n"));
}

void TstPolishController::ignore_filesExtensionsAndFolders()
{
    write("keep.txt", "k\n");
    commitAll("base");
    write("logs/a.log", "1\n");
    write("logs/b.log", "2\n");
    write("build/out.o", "o\n");

    RepoController c;
    openAndWait(c);
    QCOMPARE(c.changeCount(), 3);

    c.ignoreExtension("logs/a.log");
    QTRY_VERIFY(!c.busy());
    QTRY_COMPARE(c.changeCount(), 2);  // .gitignore (mới) + build/out.o; cả hai file .log biến mất
    QCOMPARE(read(".gitignore"), QByteArray("*.log\n"));

    c.ignoreFolder("build/out.o");
    QTRY_VERIFY(!c.busy());
    QCOMPARE(read(".gitignore"), QByteArray("*.log\n/build/\n"));
    QTRY_COMPARE(c.changeCount(), 1);  // chỉ còn .gitignore

    write("secret.env", "x\n");
    c.refresh();
    QTRY_COMPARE(c.changeCount(), 2);
    c.ignoreFile("secret.env");
    QTRY_VERIFY(!c.busy());
    QVERIFY(read(".gitignore").endsWith("/secret.env\n"));
    QTRY_COMPARE(c.changeCount(), 1);
}

void TstPolishController::ignore_escapesSpecialCharacters_andRejectsImpossibleRequests()
{
    write("keep.txt", "k\n");
    commitAll("base");
    write("#hash [1].txt", "x\n");
    RepoController c;
    openAndWait(c);
    c.ignoreFile("#hash [1].txt");
    QTRY_VERIFY(!c.busy());
    QCOMPARE(read(".gitignore"), QByteArray("/\\#hash \\[1\\].txt\n"));
    // Mẫu đã escape thực sự khớp file đó
    const QString status = QString::fromUtf8(git({"status", "--porcelain", "--ignored"}));
    QVERIFY(status.contains("!!") && status.contains("hash [1].txt"));

    c.ignoreExtension("Makefile");  // không có đuôi
    QVERIFY(c.errorMessage().contains("phần mở rộng"));
    c.clearError();
    c.ignoreFolder("README.md");  // ở thư mục gốc
    QVERIFY(c.errorMessage().contains("thư mục gốc"));
}

void TstPolishController::createRepository_opensItAndMakesInitialCommit()
{
    RepoController c;
    QVERIFY(c.gitignoreTemplates().contains("Python"));
    QVERIFY(!c.homeDir().isEmpty());
    QSignalSpy creating(&c, &RepoController::creatingChanged);
    c.createRepository(path(), "du-an", "main", true, "Python");
    QTRY_VERIFY(c.hasRepo());
    QTRY_VERIFY(!c.busy());
    QTRY_VERIFY(!c.creating());
    QCOMPARE(c.repoName(), "du-an");
    QCOMPARE(c.branch(), "main");
    QVERIFY(c.errorMessage().isEmpty());
    QTRY_COMPARE(c.lastCommitSubject(), "Initial commit");
    QCOMPARE(c.changeCount(), 0);
    QVERIFY(creating.count() >= 2);
    QVERIFY(c.recent()->paths().first().endsWith("/du-an"));
}

void TstPolishController::createRepository_failureIsReported()
{
    RepoController c;
    c.createRepository(path(), "a/b", "main", false, "");
    QTRY_VERIFY(!c.errorMessage().isEmpty());
    QTRY_VERIFY(!c.creating());
    QVERIFY(!c.hasRepo());
}

void TstPolishController::createRepository_missingIdentityStillOpensRepoWithWarning()
{
    qunsetenv("GIT_AUTHOR_NAME");
    qunsetenv("GIT_AUTHOR_EMAIL");
    qunsetenv("GIT_COMMITTER_NAME");
    qunsetenv("GIT_COMMITTER_EMAIL");
    QTemporaryDir home;
    qputenv("HOME", home.path().toUtf8());
    qputenv("XDG_CONFIG_HOME", home.path().toUtf8());
    qputenv("GIT_CONFIG_NOSYSTEM", "1");
    qputenv("GIT_CONFIG_COUNT", "1");
    qputenv("GIT_CONFIG_KEY_0", "user.useConfigOnly");
    qputenv("GIT_CONFIG_VALUE_0", "true");

    RepoController c;
    c.createRepository(path(), "khong-danh-tinh", "main", true, "");
    QTRY_VERIFY(c.hasRepo());
    QTRY_VERIFY(!c.busy());
    QVERIFY(c.errorMessage().contains("commit đầu tiên"));  // cảnh báo không bị openRepo xoá mất
    QCOMPARE(c.changeCount(), 1);                            // README.md chờ commit
    qunsetenv("GIT_CONFIG_COUNT");
}

QTEST_GUILESS_MAIN(TstPolishController)
#include "tst_polishcontroller.moc"
