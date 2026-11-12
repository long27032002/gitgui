#include <GitDiff.h>

#include <QtTest>

using namespace gitcore;

class TstGitDiff : public QObject {
    Q_OBJECT
private slots:
    void parse_headerHunkNumbersAndKinds();
    void parse_defaultCountIsOne();
    void parse_noNewlineMarker();
    void parse_bodyLinesStartingWithDashesAreContent();
    void parse_combinedDiffIsUnsupported();
    void parse_binaryHasNoHunks();
    void parse_preservesInvalidUtf8Bytes();
    void build_stage_onlyAddedLine_keepsRemovedAsContext();
    void build_unstage_onlyRemovedLine_keepsAddedAsContext();
    void build_emptySelection_givesEmptyPatch();
    void build_newFileHeader_isNormalizedToModification();
    void build_deletedFileHeader_isNormalizedToModification();
    void build_secondHunkStartShiftsByEarlierHunks();
};

static const QByteArray kSimple =
    "diff --git a/f b/f\n"
    "index 1111111..2222222 100644\n"
    "--- a/f\n"
    "+++ b/f\n"
    "@@ -1,4 +1,4 @@ func()\n"
    " a\n"
    "-b\n"
    "+B\n"
    " c\n"
    " d\n";

// Tìm khoá dòng theo nội dung thô.
static quint64 keyOf(const FileDiff &fd, const QByteArray &raw)
{
    for (int h = 0; h < fd.hunks.size(); ++h)
        for (int l = 0; l < fd.hunks[h].lines.size(); ++l)
            if (fd.hunks[h].lines[l].raw == raw)
                return lineKey(h, l);
    qFatal("không tìm thấy dòng %s", raw.constData());
}

void TstGitDiff::parse_headerHunkNumbersAndKinds()
{
    const FileDiff fd = GitDiff::parse(kSimple);
    QVERIFY(fd.supported);
    QCOMPARE(fd.headerLines.size(), 4);
    QCOMPARE(fd.hunks.size(), 1);
    const DiffHunk &h = fd.hunks[0];
    QCOMPARE(h.oldStart, 1);
    QCOMPARE(h.oldCount, 4);
    QCOMPARE(h.newStart, 1);
    QCOMPARE(h.newCount, 4);
    QCOMPARE(h.heading, QByteArray(" func()"));
    QCOMPARE(h.lines.size(), 5);
    QCOMPARE(h.lines[1].kind, DiffLineKind::Remove);
    QCOMPARE(h.lines[2].kind, DiffLineKind::Add);
    QVERIFY(h.hasChanges());
}

void TstGitDiff::parse_defaultCountIsOne()
{
    const FileDiff fd = GitDiff::parse("--- a/f\n+++ b/f\n@@ -3 +3 @@\n-x\n+y\n");
    QCOMPARE(fd.hunks[0].oldCount, 1);
    QCOMPARE(fd.hunks[0].newCount, 1);
}

void TstGitDiff::parse_noNewlineMarker()
{
    const FileDiff fd = GitDiff::parse(
        "--- a/f\n+++ b/f\n@@ -1,2 +1,2 @@\n a\n-b\n\\ No newline at end of file\n+B\n"
        "\\ No newline at end of file\n");
    const auto &lines = fd.hunks[0].lines;
    QCOMPARE(lines.size(), 5);
    QCOMPARE(lines[2].kind, DiffLineKind::NoNewline);
    QCOMPARE(lines[4].kind, DiffLineKind::NoNewline);
}

void TstGitDiff::parse_bodyLinesStartingWithDashesAreContent()
{
    // Dòng nội dung "-- comment" bị xoá hiển thị là "--- comment" nhưng nằm trong hunk.
    const FileDiff fd = GitDiff::parse("--- a/f\n+++ b/f\n@@ -1,2 +1,1 @@\n--- comment\n a\n");
    QCOMPARE(fd.headerLines.size(), 2);
    QCOMPARE(fd.hunks[0].lines[0].kind, DiffLineKind::Remove);
}

void TstGitDiff::parse_combinedDiffIsUnsupported()
{
    const FileDiff fd = GitDiff::parse("diff --cc f\n--- a/f\n+++ b/f\n@@@ -1,1 -1,1 +1,1 @@@\n+x\n");
    QVERIFY(!fd.supported);
    QVERIFY(fd.hunks.isEmpty());
}

void TstGitDiff::parse_binaryHasNoHunks()
{
    const FileDiff fd = GitDiff::parse("diff --git a/x b/x\nBinary files a/x and b/x differ\n");
    QVERIFY(fd.supported);
    QVERIFY(fd.hunks.isEmpty());
}

void TstGitDiff::parse_preservesInvalidUtf8Bytes()
{
    const QByteArray latin1 = "+caf\xe9\n";
    const FileDiff fd = GitDiff::parse("--- a/f\n+++ b/f\n@@ -0,0 +1 @@\n" + latin1);
    QCOMPARE(fd.hunks[0].lines[0].raw, QByteArray("+caf\xe9"));
    FileDiff sel = fd;
    const QByteArray patch =
        GitDiff::buildPatch(sel, LineSelection{lineKey(0, 0)}, PatchMode::Stage);
    QVERIFY(patch.contains(latin1));
}

void TstGitDiff::build_stage_onlyAddedLine_keepsRemovedAsContext()
{
    const FileDiff fd = GitDiff::parse(kSimple);
    const QByteArray patch =
        GitDiff::buildPatch(fd, LineSelection{keyOf(fd, "+B")}, PatchMode::Stage);
    QCOMPARE(patch, QByteArray("diff --git a/f b/f\n"
                               "--- a/f\n"
                               "+++ b/f\n"
                               "@@ -1,4 +1,5 @@ func()\n"
                               " a\n"
                               " b\n"
                               "+B\n"
                               " c\n"
                               " d\n"));
}

void TstGitDiff::build_unstage_onlyRemovedLine_keepsAddedAsContext()
{
    const FileDiff fd = GitDiff::parse(kSimple);
    const QByteArray patch =
        GitDiff::buildPatch(fd, LineSelection{keyOf(fd, "-b")}, PatchMode::Unstage);
    QCOMPARE(patch, QByteArray("diff --git a/f b/f\n"
                               "--- a/f\n"
                               "+++ b/f\n"
                               "@@ -1,5 +1,4 @@ func()\n"
                               " a\n"
                               "-b\n"
                               " B\n"
                               " c\n"
                               " d\n"));
}

void TstGitDiff::build_emptySelection_givesEmptyPatch()
{
    const FileDiff fd = GitDiff::parse(kSimple);
    QVERIFY(GitDiff::buildPatch(fd, {}, PatchMode::Stage).isEmpty());
    QVERIFY(GitDiff::buildPatch(fd, {}, PatchMode::Unstage).isEmpty());
}

void TstGitDiff::build_newFileHeader_isNormalizedToModification()
{
    const FileDiff fd = GitDiff::parse(
        "diff --git a/n.txt b/n.txt\nnew file mode 100644\nindex 0000000..1111111\n"
        "--- /dev/null\n+++ b/n.txt\n@@ -0,0 +1,2 @@\n+one\n+two\n");
    const QByteArray patch =
        GitDiff::buildPatch(fd, LineSelection{lineKey(0, 1)}, PatchMode::Unstage);
    QVERIFY(patch.contains("--- a/n.txt\n"));
    QVERIFY(!patch.contains("/dev/null"));
    QVERIFY(!patch.contains("new file mode"));
}

void TstGitDiff::build_deletedFileHeader_isNormalizedToModification()
{
    const FileDiff fd = GitDiff::parse(
        "diff --git a/d.txt b/d.txt\ndeleted file mode 100644\nindex 1111111..0000000\n"
        "--- a/d.txt\n+++ /dev/null\n@@ -1,2 +0,0 @@\n-one\n-two\n");
    const QByteArray patch =
        GitDiff::buildPatch(fd, LineSelection{lineKey(0, 0)}, PatchMode::Stage);
    QVERIFY(patch.contains("+++ b/d.txt\n"));
    QVERIFY(!patch.contains("/dev/null"));
    QVERIFY(!patch.contains("deleted file mode"));
}

void TstGitDiff::build_secondHunkStartShiftsByEarlierHunks()
{
    // Hunk 1 thêm 2 dòng; nếu chỉ stage phần thêm của hunk 1 và toàn bộ hunk 2,
    // vị trí "new" của hunk 2 phải dịch theo +2.
    const FileDiff fd = GitDiff::parse(
        "--- a/f\n+++ b/f\n"
        "@@ -1,3 +1,5 @@\n a\n+x\n+y\n b\n c\n"
        "@@ -20,3 +22,3 @@\n m\n-n\n+N\n o\n");
    LineSelection sel{lineKey(0, 1), lineKey(0, 2), lineKey(1, 1), lineKey(1, 2)};
    const QByteArray patch = GitDiff::buildPatch(fd, sel, PatchMode::Stage);
    QVERIFY(patch.contains("@@ -1,3 +1,5 @@\n"));
    QVERIFY2(patch.contains("@@ -20,3 +22,3 @@\n"), patch.constData());
}

QTEST_APPLESS_MAIN(TstGitDiff)
#include "tst_gitdiff.moc"
