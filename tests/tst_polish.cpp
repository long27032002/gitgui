// Khâu hoàn thiện ở lõi: amend, hoàn tác commit, bỏ thay đổi (có sao lưu), .gitignore, tạo repo.
#include <GitDiff.h>
#include <GitIgnoreTemplates.h>
#include <GitRunner.h>
#include <GitService.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace gitcore;

class TstPolish : public QObject {
    Q_OBJECT
private slots:
    void init();

    void headCommit_reportsMessageParentsAndPushed();
    void undoLastCommit_keepsChangesInWorkingTree();
    void undoLastCommit_refusesPushedMergeRootAndEmpty();
    void amend_addsSelectedChangesAndReplacesMessage();
    void amend_messageOnlyKeepsTree_andPartialSelectionWorks();
    void amend_refusedForPushedAndMergeCommits();

    void discardFiles_restoresModifiedDeletedAndRemovesUntracked();
    void discardFiles_addedAndRenamedFiles();
    void discardFiles_unmergedIsRefusedBeforeAnyChange();
    void discardUndo_restoresEverythingExactly();
    void discard_keepsOnlyTenBackups();
    void discardLines_revertsOnlyChosenLines_andIsUndoable();
    void discardLines_staleDiffFailsWithoutLeavingBackupOrChanges();

    void addIgnorePattern_createsAppendsAndDeduplicates();
    void ignoredFileDisappearsFromStatus();
    void templates_haveContent();
    void initRepository_withReadmeAndTemplate_makesInitialCommit();
    void initRepository_withoutFiles_makesNoCommit_andValidatesInput();
    void initRepository_commitFailureIsAWarningNotAnError();

private:
    QByteArray git(const QStringList &args, bool *ok = nullptr);
    void write(const QString &rel, const QByteArray &data);
    QByteArray read(const QString &rel);
    void commitAll(const QString &msg);
    QString path() const { return m_dir->path(); }
    static FileChange change(const QString &path, ChangeKind kind = ChangeKind::Ordinary,
                             QChar idx = '.', QChar wt = 'M', const QString &orig = {});

    std::unique_ptr<QTemporaryDir> m_dir;
};

QByteArray TstPolish::git(const QStringList &args, bool *ok)
{
    const GitResult r = GitRunner::run(path(), args);
    if (ok)
        *ok = r.ok();
    else
        Q_ASSERT_X(r.ok(), "git", qPrintable(args.join(' ') + ": " + r.err));
    return r.out;
}

void TstPolish::write(const QString &rel, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(path() + '/' + rel).absolutePath());
    QFile f(path() + '/' + rel);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(data);
}

QByteArray TstPolish::read(const QString &rel)
{
    QFile f(path() + '/' + rel);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

void TstPolish::commitAll(const QString &msg)
{
    git({"add", "-A"});
    git({"commit", "-q", "-m", msg});
}

FileChange TstPolish::change(const QString &path, ChangeKind kind, QChar idx, QChar wt, const QString &orig)
{
    FileChange fc;
    fc.path = path;
    fc.kind = kind;
    fc.indexStatus = idx;
    fc.worktreeStatus = wt;
    fc.origPath = orig;
    return fc;
}

void TstPolish::init()
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

// ---------- commit gần nhất ----------

void TstPolish::headCommit_reportsMessageParentsAndPushed()
{
    GitService svc(path());
    QVERIFY(!svc.headCommit().has_value());  // repo chưa có commit

    write("a.txt", "1\n");
    git({"add", "-A"});
    git({"commit", "-q", "-m", "Tiêu đề\n\nThân commit"});
    auto h = svc.headCommit();
    QVERIFY(h);
    QCOMPARE(h->subject, "Tiêu đề");
    QCOMPARE(h->message, "Tiêu đề\n\nThân commit");
    QCOMPARE(h->parentCount, 0);
    QVERIFY(!h->pushed);

    QTemporaryDir remote;
    QVERIFY(GitRunner::run(remote.path(), {"init", "--bare", "-q", "-b", "main"}).ok());
    git({"remote", "add", "origin", remote.path()});
    git({"push", "-q", "-u", "origin", "main"});
    QVERIFY(svc.headCommit()->pushed);
    write("a.txt", "2\n");
    commitAll("local");
    QVERIFY(!svc.headCommit()->pushed);
    QCOMPARE(svc.headCommit()->parentCount, 1);
}

void TstPolish::undoLastCommit_keepsChangesInWorkingTree()
{
    write("a.txt", "1\n");
    commitAll("base");
    write("a.txt", "2\n");
    write("new.txt", "n\n");
    commitAll("sẽ hoàn tác");

    GitService svc(path());
    const GitResult r = svc.undoLastCommit();
    QVERIFY2(r.ok(), qPrintable(r.err));
    QCOMPARE(git({"log", "-1", "--format=%s"}).trimmed(), QByteArray("base"));
    QCOMPARE(read("a.txt"), QByteArray("2\n"));  // nội dung vẫn ở working tree
    const auto st = svc.status();
    QVERIFY(st);
    QCOMPARE(st->changes.size(), 2);  // a.txt đã sửa, new.txt trở lại là file chưa theo dõi
    bool sawUntracked = false;
    for (const FileChange &c : st->changes)
        sawUntracked = sawUntracked || c.kind == ChangeKind::Untracked;
    QVERIFY(sawUntracked);
}

void TstPolish::undoLastCommit_refusesPushedMergeRootAndEmpty()
{
    GitService svc(path());
    QVERIFY(!svc.undoLastCommit().ok());  // chưa có commit nào

    write("a.txt", "1\n");
    commitAll("root");
    QVERIFY(!svc.undoLastCommit().ok());  // commit gốc
    QVERIFY(git({"rev-list", "--count", "HEAD"}).trimmed() == "1");

    git({"switch", "-q", "-c", "side"});
    write("s.txt", "s\n");
    commitAll("side");
    git({"switch", "-q", "main"});
    write("m.txt", "m\n");
    commitAll("main");
    git({"merge", "-q", "--no-ff", "--no-edit", "side"});
    QVERIFY(!svc.undoLastCommit().ok());  // merge commit

    QTemporaryDir remote;
    QVERIFY(GitRunner::run(remote.path(), {"init", "--bare", "-q", "-b", "main"}).ok());
    git({"remote", "add", "origin", remote.path()});
    git({"push", "-q", "-u", "origin", "main"});
    git({"reset", "-q", "--hard", "HEAD~1"});  // bỏ merge commit, về commit "main"
    git({"push", "-q", "-f", "origin", "main"});
    const GitResult pushed = svc.undoLastCommit();
    QVERIFY(!pushed.ok());
    QVERIFY(pushed.err.contains("push"));
    QCOMPARE(git({"log", "-1", "--format=%s"}).trimmed(), QByteArray("main"));
}

void TstPolish::amend_addsSelectedChangesAndReplacesMessage()
{
    write("a.txt", "1\n");
    commitAll("base");
    write("b.txt", "b\n");
    commitAll("quên thêm file");
    write("c.txt", "c\n");
    write("other.txt", "o\n");

    CommitEntry e;
    e.path = "c.txt";
    GitService svc(path());
    const GitResult r = svc.commitSelected({e}, "Tiêu đề mới\n\nThân mới", true);
    QVERIFY2(r.ok(), qPrintable(r.err));
    QCOMPARE(git({"rev-list", "--count", "HEAD"}).trimmed(), QByteArray("2"));  // không thêm commit
    QCOMPARE(git({"log", "-1", "--format=%B"}).trimmed(), QByteArray("Tiêu đề mới\n\nThân mới"));
    QCOMPARE(QString::fromUtf8(git({"show", "--name-only", "--format=", "HEAD"})).split('\n', Qt::SkipEmptyParts),
             (QStringList{"b.txt", "c.txt"}));  // commit cũ + file mới được chọn
    QVERIFY(QFile::exists(path() + "/other.txt"));  // file không chọn vẫn là thay đổi chưa commit
}

void TstPolish::amend_messageOnlyKeepsTree_andPartialSelectionWorks()
{
    QByteArray lines;
    for (int i = 1; i <= 20; ++i)
        lines += "l" + QByteArray::number(i) + "\n";
    write("f.txt", lines);
    commitAll("base");
    write("g.txt", "g\n");
    commitAll("commit sai message");
    const QByteArray treeBefore = git({"rev-parse", "HEAD^{tree}"});

    GitService svc(path());
    QVERIFY(svc.commitSelected({}, "message đã sửa", true).ok());  // không chọn gì: chỉ đổi message
    QCOMPARE(git({"rev-parse", "HEAD^{tree}"}), treeBefore);
    QCOMPARE(git({"log", "-1", "--format=%s"}).trimmed(), QByteArray("message đã sửa"));
    QCOMPARE(git({"rev-list", "--count", "HEAD"}).trimmed(), QByteArray("2"));

    // Amend với một phần dòng của file đã sửa.
    QByteArray v2 = lines;
    v2.replace("l2\n", "L2\n");
    v2.replace("l19\n", "L19\n");
    write("f.txt", v2);
    CommitEntry e;
    e.path = "f.txt";
    e.partial = true;
    e.excluded.insert(QByteArray("-l2\x1f" "0"));
    e.excluded.insert(QByteArray("+L2\x1f" "0"));
    QVERIFY(svc.commitSelected({e}, "gộp L19", true).ok());
    const QByteArray committed = git({"show", "HEAD:f.txt"});
    QVERIFY(committed.contains("L19\n") && committed.contains("l2\n") && !committed.contains("L2\n"));
    QVERIFY(git({"diff", "HEAD"}).contains("+L2\n"));  // phần bị loại vẫn là thay đổi chưa commit
}

void TstPolish::amend_refusedForPushedAndMergeCommits()
{
    write("a.txt", "1\n");
    commitAll("base");
    QTemporaryDir remote;
    QVERIFY(GitRunner::run(remote.path(), {"init", "--bare", "-q", "-b", "main"}).ok());
    git({"remote", "add", "origin", remote.path()});
    git({"push", "-q", "-u", "origin", "main"});
    GitService svc(path());
    write("b.txt", "b\n");
    CommitEntry e;
    e.path = "b.txt";
    const GitResult r = svc.commitSelected({e}, "amend commit đã push", true);
    QVERIFY(!r.ok());
    QVERIFY(r.err.contains("push"));
    QCOMPARE(git({"rev-list", "--count", "HEAD"}).trimmed(), QByteArray("1"));
    QVERIFY(git({"diff", "--cached"}).isEmpty());  // thất bại sớm: index không bị đụng

    git({"switch", "-q", "-c", "side"});
    write("s.txt", "s\n");
    commitAll("side");
    git({"switch", "-q", "main"});
    write("m.txt", "m\n");
    commitAll("main");
    git({"merge", "-q", "--no-ff", "--no-edit", "side"});
    QVERIFY(!svc.commitSelected({}, "sửa merge", true).ok());
    QVERIFY(!svc.commitSelected({}, "", false).ok());  // commit thường không chọn gì vẫn bị từ chối
}

// ---------- bỏ thay đổi ----------

void TstPolish::discardFiles_restoresModifiedDeletedAndRemovesUntracked()
{
    write("mod.txt", "goc\n");
    write("del.txt", "xoa\n");
    commitAll("base");
    write("mod.txt", "da sua\n");
    QVERIFY(QFile::remove(path() + "/del.txt"));
    write("new.txt", "moi\n");

    GitService svc(path());
    QString id;
    const GitResult r = svc.discardFiles({change("mod.txt"), change("del.txt", ChangeKind::Ordinary, '.', 'D'),
                                          change("new.txt", ChangeKind::Untracked, '?', '?')},
                                         &id);
    QVERIFY2(r.ok(), qPrintable(r.err));
    QVERIFY(!id.isEmpty());
    QCOMPARE(read("mod.txt"), QByteArray("goc\n"));
    QCOMPARE(read("del.txt"), QByteArray("xoa\n"));
    QVERIFY(!QFile::exists(path() + "/new.txt"));
    QVERIFY(svc.status()->changes.isEmpty());
}

void TstPolish::discardFiles_addedAndRenamedFiles()
{
    write("keep.txt", "k\n");
    write("old.txt", "content that is long enough to be detected as rename\n");
    commitAll("base");
    write("added.txt", "a\n");
    git({"add", "added.txt"});
    git({"mv", "old.txt", "renamed.txt"});

    GitService svc(path());
    const auto st = svc.status();
    QVERIFY(st);
    QVector<FileChange> files;
    for (const FileChange &c : st->changes)
        files.append(c);
    QCOMPARE(files.size(), 2);  // added.txt (A) và renamed.txt (R)

    QVERIFY(svc.discardFiles(files).ok());
    QVERIFY(!QFile::exists(path() + "/added.txt"));
    QVERIFY(!QFile::exists(path() + "/renamed.txt"));
    QVERIFY(QFile::exists(path() + "/old.txt"));
    QVERIFY(svc.status()->changes.isEmpty());
}

void TstPolish::discardFiles_unmergedIsRefusedBeforeAnyChange()
{
    write("f.txt", "base\n");
    write("ok.txt", "ok\n");
    commitAll("base");
    write("ok.txt", "đã sửa\n");
    GitService svc(path());
    const GitResult r = svc.discardFiles({change("ok.txt"), change("f.txt", ChangeKind::Unmerged, 'U', 'U')});
    QVERIFY(!r.ok());
    QCOMPARE(read("ok.txt"), QByteArray("đã sửa\n"));  // không file nào bị đụng
    QVERIFY(!svc.discardFiles({}).ok());
}

void TstPolish::discardUndo_restoresEverythingExactly()
{
    write("mod.txt", "goc\n");
    write("del.txt", "xoa\n");
    commitAll("base");
    write("mod.txt", "da sua\nnhieu dong\n");
    QVERIFY(QFile::remove(path() + "/del.txt"));
    write("dir/new.txt", "moi\n");
    write("added.txt", "a\n");
    git({"add", "added.txt"});

    GitService svc(path());
    QString id;
    QVERIFY(svc.discardFiles({change("mod.txt"), change("del.txt", ChangeKind::Ordinary, '.', 'D'),
                              change("dir/new.txt", ChangeKind::Untracked, '?', '?'),
                              change("added.txt", ChangeKind::Ordinary, 'A', '.')},
                             &id)
                .ok());
    QVERIFY(svc.status()->changes.isEmpty());

    const GitResult undo = svc.undoDiscard(id);
    QVERIFY2(undo.ok(), qPrintable(undo.err));
    QCOMPARE(read("mod.txt"), QByteArray("da sua\nnhieu dong\n"));
    QVERIFY(!QFile::exists(path() + "/del.txt"));  // trước đó file đã bị xoá -> trả về trạng thái bị xoá
    QCOMPARE(read("dir/new.txt"), QByteArray("moi\n"));
    QCOMPARE(read("added.txt"), QByteArray("a\n"));
    QVERIFY(!svc.undoDiscard(id).ok());            // bản sao lưu đã dùng xong
    QVERIFY(!svc.undoDiscard("khong-co").ok());
}

void TstPolish::discard_keepsOnlyTenBackups()
{
    write("a.txt", "1\n");
    commitAll("base");
    GitService svc(path());
    QStringList ids;
    for (int i = 0; i < 13; ++i) {
        write("a.txt", QByteArray::number(i + 2) + "\n");
        QString id;
        QVERIFY(svc.discardFiles({change("a.txt")}, &id).ok());
        ids << id;
        QTest::qWait(3);  // mã sao lưu theo thời gian (ms): tránh trùng
    }
    const QDir root(QString::fromUtf8(git({"rev-parse", "--absolute-git-dir"})).trimmed() + "/gitgui-backups");
    QCOMPARE(root.entryList(QDir::Dirs | QDir::NoDotAndDotDot).size(), 10);
    QVERIFY(!svc.undoDiscard(ids.first()).ok());  // bản cũ nhất đã bị dọn
    QVERIFY(svc.undoDiscard(ids.last()).ok());
}

void TstPolish::discardLines_revertsOnlyChosenLines_andIsUndoable()
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

    GitService svc(path());
    const FileDiff fd = GitDiff::parse(svc.diffForCommit("f.txt").out);
    QCOMPARE(fd.hunks.size(), 2);
    LineSelection sel;  // chỉ hunk thứ hai (l27 -> L27)
    for (int l = 0; l < fd.hunks[1].lines.size(); ++l)
        if (fd.hunks[1].lines[l].isChange())
            sel.insert(lineKey(1, l));

    QString id;
    const GitResult r = svc.discardLines("f.txt", fd, sel, &id);
    QVERIFY2(r.ok(), qPrintable(r.err));
    const QByteArray after = read("f.txt");
    QVERIFY(after.contains("L3\n") && after.contains("l27\n") && !after.contains("L27\n"));
    QVERIFY(git({"diff", "--cached"}).isEmpty());  // không đụng index

    QVERIFY(svc.undoDiscard(id).ok());
    QCOMPARE(read("f.txt"), v2);
}

void TstPolish::discardLines_staleDiffFailsWithoutLeavingBackupOrChanges()
{
    write("f.txt", "a\nb\nc\n");
    commitAll("base");
    write("f.txt", "a\nB\nc\n");
    GitService svc(path());
    const FileDiff fd = GitDiff::parse(svc.diffForCommit("f.txt").out);
    LineSelection sel;
    for (int l = 0; l < fd.hunks[0].lines.size(); ++l)
        if (fd.hunks[0].lines[l].isChange())
            sel.insert(lineKey(0, l));
    write("f.txt", "hoàn toàn khác\n");  // người dùng sửa file ở nơi khác sau khi diff được tải

    QString id;
    const GitResult r = svc.discardLines("f.txt", fd, sel, &id);
    QVERIFY(!r.ok());
    QVERIFY(id.isEmpty());
    QCOMPARE(read("f.txt"), QByteArray("hoàn toàn khác\n"));
    const QDir root(QString::fromUtf8(git({"rev-parse", "--absolute-git-dir"})).trimmed() + "/gitgui-backups");
    QCOMPARE(root.entryList(QDir::Dirs | QDir::NoDotAndDotDot).size(), 0);
}

// ---------- .gitignore ----------

void TstPolish::addIgnorePattern_createsAppendsAndDeduplicates()
{
    GitService svc(path());
    QVERIFY(svc.addIgnorePattern("/build/").ok());
    QCOMPARE(read(".gitignore"), QByteArray("/build/\n"));
    QVERIFY(svc.addIgnorePattern("*.log").ok());
    QVERIFY(svc.addIgnorePattern("/build/").ok());  // trùng: không thêm
    QCOMPARE(read(".gitignore"), QByteArray("/build/\n*.log\n"));

    write(".gitignore", "thiếu-xuống-dòng");  // file sẵn có không kết thúc bằng xuống dòng
    QVERIFY(svc.addIgnorePattern("*.tmp").ok());
    QCOMPARE(read(".gitignore"), QString("thiếu-xuống-dòng\n*.tmp\n").toUtf8());
    QVERIFY(!svc.addIgnorePattern("   ").ok());
}

void TstPolish::ignoredFileDisappearsFromStatus()
{
    write("keep.txt", "k\n");
    commitAll("base");
    write("debug.log", "x\n");
    write("src/out.o", "o\n");
    GitService svc(path());
    QCOMPARE(svc.status()->changes.size(), 2);
    QVERIFY(svc.addIgnorePattern("*.log").ok());
    QVERIFY(svc.addIgnorePattern("/src/out.o").ok());
    const auto st = svc.status();
    QVERIFY(st);
    QCOMPARE(st->changes.size(), 1);  // chỉ còn .gitignore (chưa theo dõi)
    QCOMPARE(st->changes[0].path, ".gitignore");
}

void TstPolish::templates_haveContent()
{
    const QStringList names = GitIgnoreTemplates::names();
    QVERIFY(names.size() >= 6);
    for (const QString &n : names)
        QVERIFY2(!GitIgnoreTemplates::content(n).isEmpty(), qPrintable(n));
    QVERIFY(GitIgnoreTemplates::content("Python").contains("__pycache__/"));
    QVERIFY(GitIgnoreTemplates::content("không tồn tại").isEmpty());
}

// ---------- tạo repo mới ----------

void TstPolish::initRepository_withReadmeAndTemplate_makesInitialCommit()
{
    const InitResult r = GitService::initRepository(path(), "du-an-moi", "trunk", true, "Python");
    QVERIFY2(r.git.ok(), qPrintable(r.git.err));
    QVERIFY(r.committed);
    QVERIFY(r.warning.isEmpty());
    QCOMPARE(r.path, QDir(path() + "/du-an-moi").absolutePath());
    const GitResult log = GitRunner::run(r.path, {"log", "--format=%s", "--name-only"});
    QVERIFY(QString::fromUtf8(log.out).contains("Initial commit"));
    QCOMPARE(QString::fromUtf8(GitRunner::run(r.path, {"rev-parse", "--abbrev-ref", "HEAD"}).out).trimmed(), QString("trunk"));
    QVERIFY(QFile::exists(r.path + "/README.md") && QFile::exists(r.path + "/.gitignore"));
    QVERIFY(QString::fromUtf8(GitRunner::run(r.path, {"show", "HEAD:README.md"}).out).startsWith("# du-an-moi"));
    QVERIFY(GitService(r.path).status()->changes.isEmpty());
}

void TstPolish::initRepository_withoutFiles_makesNoCommit_andValidatesInput()
{
    const InitResult r = GitService::initRepository(path(), "trong", "", false, "");
    QVERIFY(r.git.ok());
    QVERIFY(!r.committed);
    QVERIFY(!GitService(r.path).headCommit().has_value());
    QCOMPARE(QString::fromUtf8(GitRunner::run(r.path, {"symbolic-ref", "--short", "HEAD"}).out).trimmed(), QString("main"));

    QVERIFY(!GitService::initRepository(path(), "", "main", false, "").git.ok());
    QVERIFY(!GitService::initRepository(path(), "a/b", "main", false, "").git.ok());
    QVERIFY(!GitService::initRepository(path(), "..", "main", false, "").git.ok());
    QVERIFY(!GitService::initRepository(path() + "/khong-ton-tai", "x", "main", false, "").git.ok());
    QVERIFY(!GitService::initRepository(path(), "tên", "tên nhánh~xấu", false, "").git.ok());

    write("dang-co/file.txt", "x\n");  // thư mục đã tồn tại và không trống
    QVERIFY(!GitService::initRepository(path(), "dang-co", "main", true, "").git.ok());
    QCOMPARE(read("dang-co/file.txt"), QByteArray("x\n"));
}

void TstPolish::initRepository_commitFailureIsAWarningNotAnError()
{
    // Không có danh tính git: commit thất bại nhưng repo và file vẫn được tạo.
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

    const InitResult r = GitService::initRepository(path(), "khong-danh-tinh", "main", true, "");
    QVERIFY(r.git.ok());
    QVERIFY(!r.committed);
    QVERIFY(!r.warning.isEmpty());
    QVERIFY(QFile::exists(r.path + "/README.md"));
    qunsetenv("GIT_CONFIG_COUNT");
}

QTEST_APPLESS_MAIN(TstPolish)
#include "tst_polish.moc"
