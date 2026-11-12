// Kiểm thử tích hợp với git thật: sinh patch từ diff rồi `git apply --cached`.
#include <GitDiff.h>
#include <GitRunner.h>
#include <GitService.h>

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace gitcore;

class TstPartialStaging : public QObject {
    Q_OBJECT
private slots:
    void init();
    void stagePairOfOneHunk_leavesRestUnstaged();
    void stageOnlyAddedLine_keepsOldLineInIndex();
    void unstagePairOfOneHunk_leavesRestStaged();
    void twoHunks_partialSelectionInOnePatch();
    void noNewlineAtEof_roundTrips();
    void crlfLines_arePreserved();
    void nonUtf8Bytes_arePreserved();
    void partialUnstageOfNewFile();
    void partialStageOfDeletedFile();
    void staleDiff_failsWithoutTouchingIndex();

private:
    void write(const QString &rel, const QByteArray &data);
    QByteArray read(const QString &rel);
    QByteArray git(const QStringList &args, bool *ok = nullptr);
    void commitAll(const QString &msg);
    QByteArray lines(int n, const QByteArray &prefix = "l", const char *eol = "\n");
    // Stage (hoặc unstage) các dòng có nội dung thô nằm trong `raws`.
    GitResult apply(const QString &file, bool staged, const QList<QByteArray> &raws);
    QString path() const { return m_dir->path(); }

    std::unique_ptr<QTemporaryDir> m_dir;
};

void TstPartialStaging::write(const QString &rel, const QByteArray &data)
{
    QFile f(path() + QLatin1Char('/') + rel);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(data);
}

QByteArray TstPartialStaging::read(const QString &rel)
{
    QFile f(path() + QLatin1Char('/') + rel);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return f.readAll();
}

QByteArray TstPartialStaging::git(const QStringList &args, bool *ok)
{
    const GitResult r = GitRunner::run(path(), args);
    if (ok)
        *ok = r.ok();
    else
        Q_ASSERT_X(r.ok(), "git", qPrintable(r.err));
    return r.out;
}

void TstPartialStaging::commitAll(const QString &msg)
{
    git({"add", "-A"});
    git({"commit", "-q", "-m", msg});
}

QByteArray TstPartialStaging::lines(int n, const QByteArray &prefix, const char *eol)
{
    QByteArray out;
    for (int i = 1; i <= n; ++i)
        out += prefix + QByteArray::number(i) + eol;
    return out;
}

GitResult TstPartialStaging::apply(const QString &file, bool staged, const QList<QByteArray> &raws)
{
    GitService svc(path());
    const GitResult d = svc.diff(file, staged);
    const FileDiff fd = GitDiff::parse(d.out);
    LineSelection sel;
    for (int h = 0; h < fd.hunks.size(); ++h)
        for (int l = 0; l < fd.hunks[h].lines.size(); ++l)
            if (raws.contains(fd.hunks[h].lines[l].raw))
                sel.insert(lineKey(h, l));
    const QByteArray patch =
        GitDiff::buildPatch(fd, sel, staged ? PatchMode::Unstage : PatchMode::Stage);
    return svc.applyPatch(patch, staged);
}

void TstPartialStaging::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    git({"init", "-q"});
    git({"config", "user.name", "Test"});
    git({"config", "user.email", "test@example.com"});
    git({"config", "commit.gpgsign", "false"});
    git({"config", "core.autocrlf", "false"});
}

void TstPartialStaging::stagePairOfOneHunk_leavesRestUnstaged()
{
    write("f.txt", lines(30));
    commitAll("init");
    QByteArray v2 = lines(30);
    v2.replace("l2\n", "L2\n");
    v2.replace("l28\n", "L28\n");
    write("f.txt", v2);

    const GitResult r = apply("f.txt", false, {"-l28", "+L28"});
    QVERIFY2(r.ok(), qPrintable(r.err));

    const QByteArray cached = git({"diff", "--cached"});
    QVERIFY(cached.contains("+L28") && cached.contains("-l28"));
    QVERIFY(!cached.contains("L2\n"));
    const QByteArray unstaged = git({"diff"});
    QVERIFY(unstaged.contains("+L2\n") && !unstaged.contains("L28"));
    QCOMPARE(read("f.txt"), v2);  // working tree không bị chạm
}

void TstPartialStaging::stageOnlyAddedLine_keepsOldLineInIndex()
{
    write("f.txt", lines(5));
    commitAll("init");
    QByteArray v2 = lines(5);
    v2.replace("l3\n", "L3\n");
    write("f.txt", v2);

    QVERIFY(apply("f.txt", false, {"+L3"}).ok());
    const QByteArray index = git({"show", ":f.txt"});
    QVERIFY(index.contains("l3\n") && index.contains("L3\n"));
}

void TstPartialStaging::unstagePairOfOneHunk_leavesRestStaged()
{
    write("f.txt", lines(30));
    commitAll("init");
    QByteArray v2 = lines(30);
    v2.replace("l2\n", "L2\n");
    v2.replace("l28\n", "L28\n");
    write("f.txt", v2);
    git({"add", "f.txt"});

    const GitResult r = apply("f.txt", true, {"-l2", "+L2"});
    QVERIFY2(r.ok(), qPrintable(r.err));

    const QByteArray cached = git({"diff", "--cached"});
    QVERIFY(cached.contains("+L28") && !cached.contains("+L2\n"));
    QVERIFY(git({"diff"}).contains("+L2\n"));
    QCOMPARE(read("f.txt"), v2);
}

void TstPartialStaging::twoHunks_partialSelectionInOnePatch()
{
    write("f.txt", lines(40));
    commitAll("init");
    QByteArray v2 = lines(40);
    v2.replace("l3\n", "l3\nx1\nx2\n");   // hunk 1: thêm 2 dòng
    v2.replace("l4\n", "L4\n");           // cùng hunk 1: thay dòng (KHÔNG stage)
    v2.replace("l35\n", "L35\n");         // hunk 2
    write("f.txt", v2);

    const GitResult r = apply("f.txt", false, {"+x1", "+x2", "-l35", "+L35"});
    QVERIFY2(r.ok(), qPrintable(r.err));

    const QByteArray cached = git({"diff", "--cached"});
    QVERIFY(cached.contains("+x1") && cached.contains("+x2") && cached.contains("+L35"));
    QVERIFY(!cached.contains("L4\n"));
    const QByteArray unstaged = git({"diff"});
    QVERIFY(unstaged.contains("+L4\n") && !unstaged.contains("+x1") && !unstaged.contains("L35"));
}

void TstPartialStaging::noNewlineAtEof_roundTrips()
{
    write("f.txt", "a\nb");
    commitAll("init");
    write("f.txt", "a\nB");

    QVERIFY(apply("f.txt", false, {"-b", "+B"}).ok());
    QVERIFY(git({"diff"}).isEmpty());  // index khớp working tree
    QCOMPARE(git({"show", ":f.txt"}), QByteArray("a\nB"));
}

void TstPartialStaging::crlfLines_arePreserved()
{
    write("f.txt", lines(30, "l", "\r\n"));
    commitAll("init");
    QByteArray v2 = lines(30, "l", "\r\n");
    v2.replace("l10\r\n", "L10\r\n");
    write("f.txt", v2);

    QVERIFY(apply("f.txt", false, {"-l10\r", "+L10\r"}).ok());
    QVERIFY(git({"diff"}).isEmpty());
    QCOMPARE(git({"show", ":f.txt"}), v2);
}

void TstPartialStaging::nonUtf8Bytes_arePreserved()
{
    write("f.txt", "caf\xe9\nplain\n");
    commitAll("init");
    write("f.txt", "caf\xe9 au lait\nplain\n");

    QVERIFY(apply("f.txt", false, {"-caf\xe9", "+caf\xe9 au lait"}).ok());
    QVERIFY(git({"diff"}).isEmpty());
    QCOMPARE(git({"show", ":f.txt"}), QByteArray("caf\xe9 au lait\nplain\n"));
}

void TstPartialStaging::partialUnstageOfNewFile()
{
    write("keep.txt", "k\n");
    commitAll("init");
    write("n.txt", "l1\nl2\nl3\n");
    git({"add", "n.txt"});

    const GitResult r = apply("n.txt", true, {"+l2"});
    QVERIFY2(r.ok(), qPrintable(r.err));
    QCOMPARE(git({"show", ":n.txt"}), QByteArray("l1\nl3\n"));
    QCOMPARE(read("n.txt"), QByteArray("l1\nl2\nl3\n"));
}

void TstPartialStaging::partialStageOfDeletedFile()
{
    write("d.txt", "l1\nl2\nl3\n");
    commitAll("init");
    QVERIFY(QFile::remove(path() + "/d.txt"));

    const GitResult r = apply("d.txt", false, {"-l2"});
    QVERIFY2(r.ok(), qPrintable(r.err));
    QCOMPARE(git({"show", ":d.txt"}), QByteArray("l1\nl3\n"));
}

void TstPartialStaging::staleDiff_failsWithoutTouchingIndex()
{
    write("f.txt", lines(10));
    commitAll("init");
    QByteArray v2 = lines(10);
    v2.replace("l5\n", "L5\n");
    write("f.txt", v2);

    GitService svc(path());
    const FileDiff fd = GitDiff::parse(svc.diff("f.txt", false).out);
    // Người dùng sửa file ở nơi khác trước khi bấm "Stage": index đổi so với diff đang hiển thị.
    write("f.txt", lines(10).replace("l5\n", "other\n"));
    git({"add", "f.txt"});

    const QByteArray before = git({"show", ":f.txt"});
    LineSelection sel{lineKey(0, 3), lineKey(0, 4)};
    for (int l = 0; l < fd.hunks[0].lines.size(); ++l)
        if (fd.hunks[0].lines[l].isChange())
            sel.insert(lineKey(0, l));
    const GitResult r = svc.applyPatch(GitDiff::buildPatch(fd, sel, PatchMode::Stage), false);
    QVERIFY(!r.ok());
    QCOMPARE(git({"show", ":f.txt"}), before);
}

QTEST_APPLESS_MAIN(TstPartialStaging)
#include "tst_partialstaging.moc"
