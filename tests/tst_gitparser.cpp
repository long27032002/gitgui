#include <GitParser.h>

#include <QtTest>

using namespace gitcore;

class TstGitParser : public QObject {
    Q_OBJECT
private slots:
    void header_fields();
    void initialCommit_hasEmptyOid();
    void ordinary_modifiedInWorktree();
    void ordinary_stagedAndModified();
    void renamed_consumesOrigPathToken();
    void untracked_andIgnoredAreHandled();
    void unmerged_isNeitherStagedNorUnstaged();
    void pathWithSpaces_andUtf8();
    void malformedLines_areSkipped();
};

static QByteArray z(const QStringList &parts)
{
    QByteArray out;
    for (const QString &p : parts)
        out += p.toUtf8() + '\0';
    return out;
}

void TstGitParser::header_fields()
{
    const auto st = GitParser::parseStatusV2(z({
        "# branch.oid abc123", "# branch.head main",
        "# branch.upstream origin/main", "# branch.ab +2 -3"}));
    QCOMPARE(st.oid, "abc123");
    QCOMPARE(st.branch, "main");
    QCOMPARE(st.upstream, "origin/main");
    QCOMPARE(st.ahead, 2);
    QCOMPARE(st.behind, 3);
    QVERIFY(st.changes.isEmpty());
}

void TstGitParser::initialCommit_hasEmptyOid()
{
    const auto st = GitParser::parseStatusV2(z({"# branch.oid (initial)", "# branch.head master"}));
    QVERIFY(st.oid.isEmpty());
    QCOMPARE(st.branch, "master");
}

void TstGitParser::ordinary_modifiedInWorktree()
{
    const auto st = GitParser::parseStatusV2(z({
        "1 .M N... 100644 100644 100644 aaaa bbbb src/main.cpp"}));
    QCOMPARE(st.changes.size(), 1);
    const FileChange &c = st.changes.first();
    QCOMPARE(c.path, "src/main.cpp");
    QCOMPARE(c.indexStatus, QLatin1Char('.'));
    QCOMPARE(c.worktreeStatus, QLatin1Char('M'));
    QVERIFY(!c.isStaged());
    QVERIFY(c.isUnstaged());
}

void TstGitParser::ordinary_stagedAndModified()
{
    const auto st = GitParser::parseStatusV2(z({
        "1 MM N... 100644 100644 100644 aaaa bbbb a.txt"}));
    QVERIFY(st.changes.first().isStaged());
    QVERIFY(st.changes.first().isUnstaged());
}

void TstGitParser::renamed_consumesOrigPathToken()
{
    const auto st = GitParser::parseStatusV2(z({
        "2 R. N... 100644 100644 100644 aaaa bbbb R100 new.txt", "old.txt",
        "? other.txt"}));
    QCOMPARE(st.changes.size(), 2);
    QCOMPARE(st.changes[0].kind, ChangeKind::Renamed);
    QCOMPARE(st.changes[0].path, "new.txt");
    QCOMPARE(st.changes[0].origPath, "old.txt");
    QVERIFY(st.changes[0].isStaged());
    // token origPath không được hiểu nhầm là một mục riêng
    QCOMPARE(st.changes[1].path, "other.txt");
}

void TstGitParser::untracked_andIgnoredAreHandled()
{
    const auto st = GitParser::parseStatusV2(z({"? new.txt", "! build/out.o"}));
    QCOMPARE(st.changes.size(), 1);
    QCOMPARE(st.changes[0].kind, ChangeKind::Untracked);
    QVERIFY(!st.changes[0].isStaged());
    QVERIFY(st.changes[0].isUnstaged());
}

void TstGitParser::unmerged_isNeitherStagedNorUnstaged()
{
    const auto st = GitParser::parseStatusV2(z({
        "u UU N... 100644 100644 100644 100644 h1 h2 h3 conflict.txt"}));
    QCOMPARE(st.changes.size(), 1);
    QCOMPARE(st.changes[0].kind, ChangeKind::Unmerged);
    QCOMPARE(st.changes[0].path, "conflict.txt");
    QVERIFY(!st.changes[0].isStaged());
    QVERIFY(!st.changes[0].isUnstaged());
}

void TstGitParser::pathWithSpaces_andUtf8()
{
    const auto st = GitParser::parseStatusV2(z({
        "1 .M N... 100644 100644 100644 aaaa bbbb tài liệu/báo cáo.md"}));
    QCOMPARE(st.changes.first().path, QStringLiteral("tài liệu/báo cáo.md"));
}

void TstGitParser::malformedLines_areSkipped()
{
    const auto st = GitParser::parseStatusV2(z({"1 .M too short", "garbage", "? ok.txt"}));
    QCOMPARE(st.changes.size(), 1);
    QCOMPARE(st.changes[0].path, "ok.txt");
}

QTEST_APPLESS_MAIN(TstGitParser)
#include "tst_gitparser.moc"
