// Commit theo kiểu GitHub Desktop: chọn file/dòng, app tự stage rồi commit.
#include <GitDiff.h>
#include <GitRunner.h>
#include <GitService.h>

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace gitcore;

class TstCommitSelected : public QObject {
    Q_OBJECT
private slots:
    void init();

    void lineKeys_areContentBasedAndDistinguishDuplicates();
    void lineKeys_surviveLinesShiftingPosition();
    void includedAndExcluded_areInverse();
    void forNewFile_buildsAdditionOnlyDiff();
    void forNewFile_binaryAndEmptyHaveNoLines();

    void wholeFiles_commitOnlyTheChosenOnes();
    void partial_excludedLinesStayInWorkingTree();
    void partial_exclusionsSurviveEditsMadeAfterSelecting();
    void partial_untrackedNewFile();
    void partial_deletedFile();
    void partial_noNewlineAtEof();
    void partial_allExcluded_skipsFile_andNothingSelectedFailsWithoutTouchingIndex();
    void partial_fullyIncluded_behavesLikeWholeFile();
    void firstCommitOnUnbornBranch();
    void indexStagedElsewhereIsReplacedBySelection();
    void renamedFile_stagesBothSides();
    void unicodeMessage_andFailingHook_leaveSelectionStaged();

private:
    QByteArray git(const QStringList &args, bool *ok = nullptr);
    void write(const QString &rel, const QByteArray &data);
    QByteArray read(const QString &rel);
    void commitAll(const QString &msg);
    QByteArray lines(int n, const QByteArray &prefix = "l");
    QString path() const { return m_dir->path(); }
    static CommitEntry whole(const QString &p) { CommitEntry e; e.path = p; return e; }
    CommitEntry partialExcluding(const QString &file, const QList<QByteArray> &raws, int occurrence = 0);

    std::unique_ptr<QTemporaryDir> m_dir;
};

QByteArray TstCommitSelected::git(const QStringList &args, bool *ok)
{
    const GitResult r = GitRunner::run(path(), args);
    if (ok)
        *ok = r.ok();
    else
        Q_ASSERT_X(r.ok(), "git", qPrintable(args.join(' ') + ": " + r.err));
    return r.out;
}

void TstCommitSelected::write(const QString &rel, const QByteArray &data)
{
    QFile f(path() + '/' + rel);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(data);
}

QByteArray TstCommitSelected::read(const QString &rel)
{
    QFile f(path() + '/' + rel);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

void TstCommitSelected::commitAll(const QString &msg)
{
    git({"add", "-A"});
    git({"commit", "-q", "-m", msg});
}

QByteArray TstCommitSelected::lines(int n, const QByteArray &prefix)
{
    QByteArray out;
    for (int i = 1; i <= n; ++i)
        out += prefix + QByteArray::number(i) + "\n";
    return out;
}

// Loại các dòng thô trong `raws` (lần xuất hiện thứ `occurrence`) khỏi commit.
CommitEntry TstCommitSelected::partialExcluding(const QString &file, const QList<QByteArray> &raws, int occurrence)
{
    CommitEntry e;
    e.path = file;
    e.partial = true;
    for (const QByteArray &r : raws)
        e.excluded.insert(r + '\x1f' + QByteArray::number(occurrence));
    return e;
}

void TstCommitSelected::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    git({"init", "-q", "-b", "main"});
    git({"config", "user.name", "Test"});
    git({"config", "user.email", "test@example.com"});
    git({"config", "commit.gpgsign", "false"});
}

void TstCommitSelected::lineKeys_areContentBasedAndDistinguishDuplicates()
{
    const FileDiff fd = GitDiff::parse(
        "--- a/f\n+++ b/f\n@@ -1,2 +1,4 @@\n a\n+x\n+x\n+y\n b\n");
    const auto keys = GitDiff::lineKeys(fd);
    QVERIFY(keys[0][0].isEmpty());  // dòng ngữ cảnh không có khoá
    QCOMPARE(keys[0][1], QByteArray("+x\x1f" "0"));
    QCOMPARE(keys[0][2], QByteArray("+x\x1f" "1"));  // dòng giống hệt: khoá khác nhau
    QCOMPARE(keys[0][3], QByteArray("+y\x1f" "0"));
    QCOMPARE(GitDiff::changeLineCount(fd), 3);
}

void TstCommitSelected::lineKeys_surviveLinesShiftingPosition()
{
    const FileDiff before = GitDiff::parse("--- a/f\n+++ b/f\n@@ -5,2 +5,3 @@\n c\n+keep\n d\n");
    const FileDiff after = GitDiff::parse("--- a/f\n+++ b/f\n@@ -50,2 +80,3 @@\n c\n+keep\n d\n");
    QCOMPARE(GitDiff::lineKeys(before)[0][1], GitDiff::lineKeys(after)[0][1]);
}

void TstCommitSelected::includedAndExcluded_areInverse()
{
    const FileDiff fd = GitDiff::parse("--- a/f\n+++ b/f\n@@ -1,3 +1,3 @@\n a\n-b\n+B\n c\n");
    QSet<QByteArray> ex{"+B\x1f" "0"};
    const LineSelection inc = GitDiff::includedLines(fd, ex);
    QCOMPARE(inc.size(), 1);
    QVERIFY(inc.contains(lineKey(0, 1)));
    QCOMPARE(GitDiff::excludedKeys(fd, inc), ex);
    QCOMPARE(GitDiff::includedLines(fd, {}).size(), 2);  // mặc định: chọn tất cả
}

void TstCommitSelected::forNewFile_buildsAdditionOnlyDiff()
{
    const FileDiff fd = GitDiff::forNewFile("src/a.txt", "one\ntwo\n");
    QCOMPARE(fd.hunks.size(), 1);
    QCOMPARE(fd.hunks[0].newCount, 2);
    QCOMPARE(fd.hunks[0].lines.size(), 2);
    QCOMPARE(fd.hunks[0].lines[1].raw, QByteArray("+two"));
    QVERIFY(fd.headerLines.contains("new file mode 100644"));

    const FileDiff noNl = GitDiff::forNewFile("a", "x");  // thiếu dòng cuối
    QCOMPARE(noNl.hunks[0].lines.last().kind, DiffLineKind::NoNewline);
    QVERIFY(GitDiff::forNewFile("a", "x", true).headerLines.contains("new file mode 100755"));
}

void TstCommitSelected::forNewFile_binaryAndEmptyHaveNoLines()
{
    QVERIFY(GitDiff::forNewFile("b.bin", QByteArray("ab\0cd", 5)).hunks.isEmpty());
    QVERIFY(GitDiff::forNewFile("empty", "").hunks.isEmpty());
}

void TstCommitSelected::wholeFiles_commitOnlyTheChosenOnes()
{
    write("a.txt", "a\n");
    write("b.txt", "b\n");
    commitAll("base");
    write("a.txt", "a2\n");
    write("b.txt", "b2\n");
    write("c.txt", "new\n");

    GitService svc(path());
    const GitResult r = svc.commitSelected({whole("a.txt"), whole("c.txt")}, "chọn a và c");
    QVERIFY2(r.ok(), qPrintable(r.err));
    QCOMPARE(QString::fromUtf8(git({"show", "--name-only", "--format=", "HEAD"})).split('\n', Qt::SkipEmptyParts),
             (QStringList{"a.txt", "c.txt"}));
    const auto st = svc.status();
    QVERIFY(st && st->changes.size() == 1);  // chỉ còn b.txt thay đổi
    QCOMPARE(st->changes[0].path, "b.txt");
    QCOMPARE(read("b.txt"), QByteArray("b2\n"));
}

void TstCommitSelected::partial_excludedLinesStayInWorkingTree()
{
    write("f.txt", lines(30));
    commitAll("base");
    QByteArray v2 = lines(30);
    v2.replace("l2\n", "L2\n");
    v2.replace("l28\n", "L28\n");
    write("f.txt", v2);

    GitService svc(path());
    const GitResult r = svc.commitSelected({partialExcluding("f.txt", {"-l2", "+L2"})}, "chỉ L28");
    QVERIFY2(r.ok(), qPrintable(r.err));
    const QByteArray committed = git({"show", "HEAD:f.txt"});
    QVERIFY(committed.contains("L28\n") && !committed.contains("L2\n") && committed.contains("l2\n"));
    QCOMPARE(read("f.txt"), v2);  // working tree không bị chạm
    const QByteArray rest = git({"diff", "HEAD"});
    QVERIFY(rest.contains("+L2\n") && rest.contains("-l2") && !rest.contains("L28"));
}

void TstCommitSelected::partial_exclusionsSurviveEditsMadeAfterSelecting()
{
    write("f.txt", lines(20));
    commitAll("base");
    QByteArray v2 = lines(20);
    v2.replace("l10\n", "L10\n");
    write("f.txt", v2);
    // Người dùng loại thay đổi l10->L10 khi diff đang ở dạng này...
    const CommitEntry entry = partialExcluding("f.txt", {"-l10", "+L10"});
    // ...rồi thêm các dòng mới LÊN TRÊN trước khi bấm Commit: vị trí dịch chỗ nhưng nội dung dòng giữ nguyên.
    QByteArray v3 = "dong-moi-1\ndong-moi-2\n" + v2;
    write("f.txt", v3);

    GitService svc(path());
    QVERIFY(svc.commitSelected({entry}, "chỉ dòng mới").ok());
    const QByteArray committed = git({"show", "HEAD:f.txt"});
    QVERIFY(committed.startsWith("dong-moi-1\ndong-moi-2\n"));
    QVERIFY(committed.contains("l10\n") && !committed.contains("L10\n"));  // L10 vẫn bị loại đúng dòng
}

void TstCommitSelected::partial_untrackedNewFile()
{
    write("keep.txt", "k\n");
    commitAll("base");
    write("new.txt", "one\ntwo\nthree\n");

    GitService svc(path());
    const GitResult r = svc.commitSelected({partialExcluding("new.txt", {"+two"})}, "file mới một phần");
    QVERIFY2(r.ok(), qPrintable(r.err));
    QCOMPARE(git({"show", "HEAD:new.txt"}), QByteArray("one\nthree\n"));
    QCOMPARE(read("new.txt"), QByteArray("one\ntwo\nthree\n"));
    QVERIFY(git({"diff", "HEAD"}).contains("+two"));
}

void TstCommitSelected::partial_deletedFile()
{
    write("d.txt", "l1\nl2\nl3\n");
    commitAll("base");
    QVERIFY(QFile::remove(path() + "/d.txt"));

    GitService svc(path());
    QVERIFY(svc.commitSelected({partialExcluding("d.txt", {"-l2"})}, "xoá một phần").ok());
    QCOMPARE(git({"show", "HEAD:d.txt"}), QByteArray("l2\n"));  // chỉ l1, l3 bị xoá khỏi commit
}

void TstCommitSelected::partial_noNewlineAtEof()
{
    // File cũ thiếu dòng cuối; bản mới thêm dòng "c" cũng thiếu dòng cuối. Loại "c" thì dấu
    // "\ No newline" của nó phải bị bỏ theo, còn "B" (thay "b") phải kết thúc bằng xuống dòng bình thường.
    write("f.txt", "a\nb");
    commitAll("base");
    write("f.txt", "a\nB\nc");
    GitService svc(path());
    QVERIFY(svc.commitSelected({partialExcluding("f.txt", {"+c"})}, "bỏ dòng cuối").ok());
    QCOMPARE(git({"show", "HEAD:f.txt"}), QByteArray("a\nB\n"));
    QCOMPARE(read("f.txt"), QByteArray("a\nB\nc"));
}

void TstCommitSelected::partial_allExcluded_skipsFile_andNothingSelectedFailsWithoutTouchingIndex()
{
    write("a.txt", "a\n");
    write("b.txt", "b\n");
    commitAll("base");
    write("a.txt", "a2\n");
    write("b.txt", "b2\n");
    git({"add", "b.txt"});  // index có sẵn một thay đổi
    const QByteArray indexBefore = git({"diff", "--cached"});

    GitService svc(path());
    const GitResult r = svc.commitSelected({partialExcluding("a.txt", {"-a", "+a2"})}, "không có gì");
    QVERIFY(!r.ok());
    QVERIFY(r.err.contains("Chưa chọn"));
    QCOMPARE(git({"diff", "--cached"}), indexBefore);  // thất bại sớm: index nguyên vẹn
    QVERIFY(!svc.commitSelected({}, "x").ok());
}

void TstCommitSelected::partial_fullyIncluded_behavesLikeWholeFile()
{
    write("f.txt", lines(5));
    commitAll("base");
    write("f.txt", lines(5, "x"));
    CommitEntry e;
    e.path = "f.txt";
    e.partial = true;  // không loại dòng nào
    GitService svc(path());
    QVERIFY(svc.commitSelected({e}, "đủ").ok());
    QVERIFY(git({"diff", "HEAD"}).isEmpty());
}

void TstCommitSelected::firstCommitOnUnbornBranch()
{
    write("a.txt", "1\n2\n3\n");
    write("b.txt", "b\n");
    GitService svc(path());
    const GitResult r = svc.commitSelected({partialExcluding("a.txt", {"+2"}), whole("b.txt")}, "commit đầu tiên");
    QVERIFY2(r.ok(), qPrintable(r.err));
    QCOMPARE(git({"show", "HEAD:a.txt"}), QByteArray("1\n3\n"));
    QCOMPARE(git({"show", "HEAD:b.txt"}), QByteArray("b\n"));
    QCOMPARE(git({"rev-list", "--count", "HEAD"}).trimmed(), QByteArray("1"));
}

void TstCommitSelected::indexStagedElsewhereIsReplacedBySelection()
{
    write("a.txt", "a\n");
    write("b.txt", "b\n");
    commitAll("base");
    write("a.txt", "a2\n");
    write("b.txt", "b2\n");
    git({"add", "b.txt"});  // người dùng stage b.txt ngoài terminal, nhưng chỉ chọn a.txt trong app

    GitService svc(path());
    QVERIFY(svc.commitSelected({whole("a.txt")}, "chỉ a").ok());
    QCOMPARE(QString::fromUtf8(git({"show", "--name-only", "--format=", "HEAD"})).trimmed(), QString("a.txt"));
    QCOMPARE(read("b.txt"), QByteArray("b2\n"));  // thay đổi của b vẫn còn ở working tree
}

void TstCommitSelected::renamedFile_stagesBothSides()
{
    write("old.txt", "content\n");
    commitAll("base");
    git({"mv", "old.txt", "new.txt"});
    CommitEntry e;
    e.path = "new.txt";
    e.origPath = "old.txt";
    GitService svc(path());
    QVERIFY(svc.commitSelected({e}, "đổi tên").ok());
    QVERIFY(!QFile::exists(path() + "/old.txt"));
    QVERIFY(git({"show", "--name-status", "--format=", "-M", "HEAD"}).startsWith("R"));
}

void TstCommitSelected::unicodeMessage_andFailingHook_leaveSelectionStaged()
{
    write("a.txt", "a\n");
    commitAll("base");
    write("a.txt", "a2\n");
    GitService svc(path());
    QVERIFY(svc.commitSelected({whole("a.txt")}, "Tiêu đề tiếng Việt\n\nNội dung ✓").ok());
    QVERIFY(QString::fromUtf8(git({"log", "-1", "--format=%B"})).contains("Nội dung ✓"));

    // Hook pre-commit từ chối: commit thất bại, phần đã chọn nằm sẵn ở vùng stage, working tree nguyên vẹn.
    write("a.txt", "a3\n");
    QFile hook(path() + "/.git/hooks/pre-commit");
    QVERIFY(hook.open(QIODevice::WriteOnly));
    hook.write("#!/bin/sh\necho 'chặn bởi hook' >&2\nexit 1\n");
    hook.close();
    QVERIFY(hook.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    const GitResult r = svc.commitSelected({whole("a.txt")}, "bị chặn");
    QVERIFY(!r.ok());
    QVERIFY(r.err.contains("hook"));
    QCOMPARE(read("a.txt"), QByteArray("a3\n"));
    QVERIFY(git({"diff", "--cached"}).contains("+a3"));
}

QTEST_APPLESS_MAIN(TstCommitSelected)
#include "tst_commitselected.moc"
