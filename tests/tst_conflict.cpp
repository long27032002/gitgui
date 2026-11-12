// Parser dấu xung đột + luồng giải quyết conflict với git thật.
#include <GitConflict.h>
#include <GitRunner.h>
#include <GitService.h>

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace gitcore;

class TstConflict : public QObject {
    Q_OBJECT
private slots:
    void init();

    // --- parser ---
    void parse_noMarkers();
    void parse_singleBlock();
    void parse_twoBlocksAndLabels();
    void parse_diff3BaseSection();
    void parse_crlfPreserved();
    void parse_markerLikeLinesAreText();
    void parse_malformedIsUnsupported();
    void resolve_eachChoice();
    void resolve_unresolvedKeepsMarkersByteExact();
    void resolve_nonUtf8BytesSurvive();

    // --- git thật ---
    void merge_conflict_isDetected_resolvedWithContent_andContinued();
    void merge_abort_restoresCleanState();
    void resolveWithSide_oursAndTheirs();
    void resolveWithSide_handlesDeletedByUs_andDeletedByThem();
    void rebase_conflict_continueFinishesRebase();
    void cherryPick_conflict_abort();
    void continue_withoutOperationFails();

private:
    QByteArray git(const QStringList &args, bool *ok = nullptr);
    void write(const QString &rel, const QByteArray &data);
    QByteArray read(const QString &rel);
    void commitFile(const QString &rel, const QByteArray &data, const QString &msg);
    // main: "main\n", other: "other\n" trên cùng một dòng -> merge other vào main gây conflict.
    void makeContentConflictRepo();
    QString path() const { return m_dir->path(); }

    std::unique_ptr<QTemporaryDir> m_dir;
};

QByteArray TstConflict::git(const QStringList &args, bool *ok)
{
    const GitResult r = GitRunner::run(path(), args);
    if (ok)
        *ok = r.ok();
    else
        Q_ASSERT_X(r.ok(), "git", qPrintable(args.join(' ') + ": " + r.err));
    return r.out;
}

void TstConflict::write(const QString &rel, const QByteArray &data)
{
    QFile f(path() + '/' + rel);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(data);
}

QByteArray TstConflict::read(const QString &rel)
{
    QFile f(path() + '/' + rel);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

void TstConflict::commitFile(const QString &rel, const QByteArray &data, const QString &msg)
{
    write(rel, data);
    git({"add", "-A"});
    git({"commit", "-q", "-m", msg});
}

void TstConflict::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    git({"init", "-q", "-b", "main"});
    git({"config", "user.name", "Test"});
    git({"config", "user.email", "test@example.com"});
    git({"config", "commit.gpgsign", "false"});
}

void TstConflict::makeContentConflictRepo()
{
    commitFile("f.txt", "top\nline\nbottom\n", "base");
    git({"switch", "-q", "-c", "other"});
    commitFile("f.txt", "top\nOTHER\nbottom\n", "other edit");
    git({"switch", "-q", "main"});
    commitFile("f.txt", "top\nMAIN\nbottom\n", "main edit");
}

// ---------- parser ----------

void TstConflict::parse_noMarkers()
{
    const ConflictFile f = GitConflict::parse("a\nb\n");
    QVERIFY(f.supported);
    QCOMPARE(f.conflictCount(), 0);
    QCOMPARE(f.segments.size(), 1);
}

void TstConflict::parse_singleBlock()
{
    const ConflictFile f = GitConflict::parse(
        "top\n<<<<<<< HEAD\nmine\n=======\ntheirs\n>>>>>>> feature\nbottom\n");
    QVERIFY(f.supported);
    QCOMPARE(f.conflictCount(), 1);
    QCOMPARE(f.segments.size(), 3);
    const ConflictBlock &b = f.segments[1].block;
    QCOMPARE(b.ours, QByteArray("mine\n"));
    QCOMPARE(b.theirs, QByteArray("theirs\n"));
    QVERIFY(!b.hasBase);
}

void TstConflict::parse_twoBlocksAndLabels()
{
    const ConflictFile f = GitConflict::parse(
        "<<<<<<< HEAD\na\n=======\nb\n>>>>>>> origin/main\nmid\n"
        "<<<<<<< HEAD\nc\n=======\nd\n>>>>>>> origin/main\n");
    QCOMPARE(f.conflictCount(), 2);
    QCOMPARE(GitConflict::markerLabel(f.segments[0].block.startMarker), "HEAD");
    QCOMPARE(GitConflict::markerLabel(f.segments[0].block.endMarker), "origin/main");
}

void TstConflict::parse_diff3BaseSection()
{
    const ConflictFile f = GitConflict::parse(
        "<<<<<<< HEAD\nmine\n||||||| merged common ancestors\norig\n=======\ntheirs\n>>>>>>> x\n");
    QVERIFY(f.supported);
    const ConflictBlock &b = f.segments[0].block;
    QVERIFY(b.hasBase);
    QCOMPARE(b.base, QByteArray("orig\n"));
    QCOMPARE(b.ours, QByteArray("mine\n"));
    QCOMPARE(b.theirs, QByteArray("theirs\n"));
}

void TstConflict::parse_crlfPreserved()
{
    const QByteArray src = "a\r\n<<<<<<< HEAD\r\nmine\r\n=======\r\ntheirs\r\n>>>>>>> x\r\nz\r\n";
    const ConflictFile f = GitConflict::parse(src);
    QCOMPARE(f.conflictCount(), 1);
    QCOMPARE(f.segments[1].block.ours, QByteArray("mine\r\n"));
    const QByteArray out = GitConflict::resolve(f, {ConflictResolution::Theirs});
    QCOMPARE(out, QByteArray("a\r\ntheirs\r\nz\r\n"));
}

void TstConflict::parse_markerLikeLinesAreText()
{
    // 8 ký tự, hoặc không đứng đầu dòng -> không phải dấu xung đột.
    const ConflictFile f = GitConflict::parse("<<<<<<<< not a marker\n  <<<<<<< indented\n=======\n");
    QVERIFY(f.supported);
    QCOMPARE(f.conflictCount(), 0);
}

void TstConflict::parse_malformedIsUnsupported()
{
    QVERIFY(!GitConflict::parse("<<<<<<< HEAD\nmine\n=======\ntheirs\n").supported);        // thiếu >>>>>>>
    QVERIFY(!GitConflict::parse("<<<<<<< A\n<<<<<<< B\n=======\n>>>>>>> B\n").supported);  // lồng nhau
    QVERIFY(!GitConflict::parse("<<<<<<< HEAD\nmine\n").supported);
}

void TstConflict::resolve_eachChoice()
{
    const ConflictFile f = GitConflict::parse("t\n<<<<<<< HEAD\nM\n=======\nT\n>>>>>>> x\nb\n");
    QCOMPARE(GitConflict::resolve(f, {ConflictResolution::Ours}), QByteArray("t\nM\nb\n"));
    QCOMPARE(GitConflict::resolve(f, {ConflictResolution::Theirs}), QByteArray("t\nT\nb\n"));
    QCOMPARE(GitConflict::resolve(f, {ConflictResolution::Both}), QByteArray("t\nM\nT\nb\n"));
    QCOMPARE(GitConflict::resolve(f, {ConflictResolution::BothReversed}), QByteArray("t\nT\nM\nb\n"));
}

void TstConflict::resolve_unresolvedKeepsMarkersByteExact()
{
    const QByteArray src =
        "t\n<<<<<<< HEAD\nM\n||||||| base\nB\n=======\nT\n>>>>>>> x\nmid\n"
        "<<<<<<< HEAD\nM2\n=======\nT2\n>>>>>>> x\n";
    const ConflictFile f = GitConflict::parse(src);
    QCOMPARE(GitConflict::resolve(f, {ConflictResolution::Unresolved, ConflictResolution::Unresolved}), src);
    QCOMPARE(GitConflict::resolve(f, {}), src);  // thiếu phần tử = chưa giải quyết
    const QByteArray partial =
        GitConflict::resolve(f, {ConflictResolution::Unresolved, ConflictResolution::Theirs});
    QVERIFY(partial.startsWith("t\n<<<<<<< HEAD\nM\n||||||| base"));
    QVERIFY(partial.endsWith("mid\nT2\n"));
}

void TstConflict::resolve_nonUtf8BytesSurvive()
{
    const QByteArray src = "<<<<<<< HEAD\ncaf\xe9 mine\n=======\ncaf\xe9 theirs\n>>>>>>> x\n";
    const ConflictFile f = GitConflict::parse(src);
    QCOMPARE(GitConflict::resolve(f, {ConflictResolution::Both}),
             QByteArray("caf\xe9 mine\ncaf\xe9 theirs\n"));
}

// ---------- git thật ----------

void TstConflict::merge_conflict_isDetected_resolvedWithContent_andContinued()
{
    makeContentConflictRepo();
    GitService svc(path());
    QCOMPARE(svc.operationInProgress(), OperationKind::None);

    bool ok = true;
    git({"merge", "other"}, &ok);
    QVERIFY(!ok);
    QCOMPARE(svc.operationInProgress(), OperationKind::Merge);

    const auto st = svc.status();
    QVERIFY(st && st->changes.size() == 1);
    QCOMPARE(st->changes[0].kind, ChangeKind::Unmerged);

    // Đọc file, giải quyết bằng parser: giữ cả hai (của tôi trước).
    const ConflictFile f = GitConflict::parse(read("f.txt"));
    QCOMPARE(f.conflictCount(), 1);
    const QByteArray merged = GitConflict::resolve(f, {ConflictResolution::Both});
    QCOMPARE(merged, QByteArray("top\nMAIN\nOTHER\nbottom\n"));

    // Chưa giải quyết thì không hoàn tất được.
    QVERIFY(!svc.continueOperation(OperationKind::Merge).ok());

    const GitResult res = svc.resolveWithContent("f.txt", merged);
    QVERIFY2(res.ok(), qPrintable(res.err));
    const auto st2 = svc.status();
    QVERIFY(st2 && st2->changes.size() == 1 && st2->changes[0].kind == ChangeKind::Ordinary);

    const GitResult cont = svc.continueOperation(OperationKind::Merge);
    QVERIFY2(cont.ok(), qPrintable(cont.err));
    QCOMPARE(svc.operationInProgress(), OperationKind::None);
    QCOMPARE(git({"rev-list", "--parents", "-n", "1", "HEAD"}).trimmed().split(' ').size(), 3);  // 2 cha
    QCOMPARE(read("f.txt"), merged);
}

void TstConflict::merge_abort_restoresCleanState()
{
    makeContentConflictRepo();
    GitService svc(path());
    bool ok = true;
    git({"merge", "other"}, &ok);
    QVERIFY(!ok);

    const GitResult r = svc.abortOperation(OperationKind::Merge);
    QVERIFY2(r.ok(), qPrintable(r.err));
    QCOMPARE(svc.operationInProgress(), OperationKind::None);
    QVERIFY(svc.status()->changes.isEmpty());
    QCOMPARE(read("f.txt"), QByteArray("top\nMAIN\nbottom\n"));
}

void TstConflict::resolveWithSide_oursAndTheirs()
{
    makeContentConflictRepo();
    GitService svc(path());
    bool ok = true;
    git({"merge", "other"}, &ok);
    QVERIFY(!ok);
    QVERIFY(svc.resolveWithSide("f.txt", true).ok());
    QCOMPARE(read("f.txt"), QByteArray("top\nMAIN\nbottom\n"));
    QVERIFY(svc.continueOperation(OperationKind::Merge).ok());

    // Lần hai: chọn theirs.
    git({"reset", "-q", "--hard", "HEAD^"});
    git({"merge", "other"}, &ok);
    QVERIFY(!ok);
    QVERIFY(svc.resolveWithSide("f.txt", false).ok());
    QCOMPARE(read("f.txt"), QByteArray("top\nOTHER\nbottom\n"));
}

void TstConflict::resolveWithSide_handlesDeletedByUs_andDeletedByThem()
{
    commitFile("a.txt", "a\n", "base a");
    commitFile("b.txt", "b\n", "base b");
    git({"switch", "-q", "-c", "other"});
    git({"rm", "-q", "a.txt"});                       // other xoá a.txt
    commitFile("b.txt", "b other\n", "other edits b"); // other sửa b.txt
    git({"switch", "-q", "main"});
    commitFile("a.txt", "a main\n", "main edits a");   // main sửa a.txt
    git({"rm", "-q", "b.txt"});                        // main xoá b.txt
    git({"commit", "-q", "-m", "main deletes b"});

    GitService svc(path());
    bool ok = true;
    git({"merge", "other"}, &ok);
    QVERIFY(!ok);
    const auto st = svc.status();
    QVERIFY(st);
    QCOMPARE(st->changes.size(), 2);

    // a.txt: other xoá, main sửa ("deleted by them") -> chọn theirs = xoá file.
    QVERIFY(svc.resolveWithSide("a.txt", false).ok());
    QVERIFY(!QFile::exists(path() + "/a.txt"));
    // b.txt: main xoá, other sửa ("deleted by us") -> chọn theirs = giữ bản của other.
    QVERIFY(svc.resolveWithSide("b.txt", false).ok());
    QCOMPARE(read("b.txt"), QByteArray("b other\n"));

    const GitResult cont = svc.continueOperation(OperationKind::Merge);
    QVERIFY2(cont.ok(), qPrintable(cont.err));

    // Chọn ours cho trường hợp xoá: main xoá b.txt -> ours = xoá.
    git({"reset", "-q", "--hard", "HEAD^"});
    git({"merge", "other"}, &ok);
    QVERIFY(!ok);
    QVERIFY(svc.resolveWithSide("b.txt", true).ok());
    QVERIFY(!QFile::exists(path() + "/b.txt"));
}

void TstConflict::rebase_conflict_continueFinishesRebase()
{
    makeContentConflictRepo();  // main có "MAIN"; other có "OTHER"
    git({"switch", "-q", "other"});
    GitService svc(path());
    bool ok = true;
    git({"rebase", "main"}, &ok);
    QVERIFY(!ok);
    QCOMPARE(svc.operationInProgress(), OperationKind::Rebase);

    // Khi rebase: "ours" là nhánh đích (main), "theirs" là commit của mình (other).
    QVERIFY(svc.resolveWithSide("f.txt", false).ok());
    QCOMPARE(read("f.txt"), QByteArray("top\nOTHER\nbottom\n"));
    const GitResult cont = svc.continueOperation(OperationKind::Rebase);
    QVERIFY2(cont.ok(), qPrintable(cont.err));
    QCOMPARE(svc.operationInProgress(), OperationKind::None);
    QCOMPARE(git({"rev-parse", "--abbrev-ref", "HEAD"}).trimmed(), QByteArray("other"));
    QCOMPARE(git({"rev-list", "--count", "main..other"}).trimmed(), QByteArray("1"));
}

void TstConflict::cherryPick_conflict_abort()
{
    makeContentConflictRepo();
    const QByteArray pick = git({"rev-parse", "other"}).trimmed();
    GitService svc(path());
    bool ok = true;
    git({"cherry-pick", pick}, &ok);
    QVERIFY(!ok);
    QCOMPARE(svc.operationInProgress(), OperationKind::CherryPick);
    QVERIFY(svc.abortOperation(OperationKind::CherryPick).ok());
    QCOMPARE(svc.operationInProgress(), OperationKind::None);
    QCOMPARE(read("f.txt"), QByteArray("top\nMAIN\nbottom\n"));

    // Làm lại và hoàn tất bằng cách lấy bản của commit được pick (theirs).
    // (Chọn ours sẽ cho commit rỗng và git từ chối tiếp tục — đúng hành vi của git.)
    git({"cherry-pick", pick}, &ok);
    QVERIFY(!ok);
    QVERIFY(svc.resolveWithSide("f.txt", false).ok());
    const GitResult cont = svc.continueOperation(OperationKind::CherryPick);
    QVERIFY2(cont.ok(), qPrintable(cont.err));
    QCOMPARE(svc.operationInProgress(), OperationKind::None);
}

void TstConflict::continue_withoutOperationFails()
{
    commitFile("f.txt", "x\n", "c1");
    GitService svc(path());
    QVERIFY(!svc.continueOperation(OperationKind::None).ok());
    QVERIFY(!svc.abortOperation(OperationKind::None).ok());
}

QTEST_APPLESS_MAIN(TstConflict)
#include "tst_conflict.moc"
