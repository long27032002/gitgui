#include <BranchesModel.h>
#include <HistoryModels.h>

#include <QtTest>

using namespace gitapp;
using namespace gitcore;

class TstListModels : public QObject {
    Q_OBJECT
private slots:
    void branches_sortCurrentFirstThenLocalThenRemote();
    void branches_find_distinguishesLocalFromRemote();
    void branches_sameContent_doesNotReset();
    void commits_appendAndLookup();
    void commits_roles();
    void commits_graphLanesFollowParents();
    void commitFiles_roles();
};

static BranchInfo br(const QString &n, bool remote = false, bool cur = false)
{
    BranchInfo b;
    b.name = n;
    b.isRemote = remote;
    b.isCurrent = cur;
    return b;
}

void TstListModels::branches_sortCurrentFirstThenLocalThenRemote()
{
    BranchesModel m;
    m.setBranches({br("origin/z", true), br("zeta"), br("alpha"), br("main", false, true),
                   br("origin/a", true)});
    QStringList names;
    for (int i = 0; i < m.rowCount(); ++i)
        names << m.data(m.index(i), BranchesModel::NameRole).toString();
    QCOMPARE(names, (QStringList{"main", "alpha", "zeta", "origin/a", "origin/z"}));
}

void TstListModels::branches_find_distinguishesLocalFromRemote()
{
    BranchesModel m;
    m.setBranches({br("dev"), br("origin/dev", true)});
    QVERIFY(m.find("dev", false).has_value());
    QVERIFY(!m.find("dev", true).has_value());
    QVERIFY(m.find("origin/dev", true).has_value());
}

void TstListModels::branches_sameContent_doesNotReset()
{
    BranchesModel m;
    m.setBranches({br("a"), br("b")});
    QSignalSpy reset(&m, &QAbstractItemModel::modelReset);
    m.setBranches({br("b"), br("a")});  // cùng nội dung, khác thứ tự đầu vào
    QCOMPARE(reset.count(), 0);
    m.setBranches({br("a")});
    QCOMPARE(reset.count(), 1);
}

static CommitInfo ci(const QString &hash, const QString &subject, int parents = 1)
{
    CommitInfo c;
    c.hash = hash;
    c.shortHash = hash.left(4);
    c.subject = subject;
    c.author = "A";
    c.timestamp = 1700000000;
    c.parentCount = parents;
    return c;
}

void TstListModels::commits_appendAndLookup()
{
    CommitsModel m;
    m.setCommits({ci("aaaa1", "one")});
    QSignalSpy inserted(&m, &QAbstractItemModel::rowsInserted);
    m.append({ci("bbbb2", "two"), ci("cccc3", "three")});
    QCOMPARE(inserted.count(), 1);
    QCOMPARE(m.rowCount(), 3);
    QCOMPARE(m.rowOfHash("cccc3"), 2);
    QCOMPARE(m.rowOfHash("none"), -1);
    m.append({});  // không phát tín hiệu thừa
    QCOMPARE(inserted.count(), 1);
}

void TstListModels::commits_roles()
{
    CommitsModel m;
    m.setCommits({ci("aaaa1", "Tiêu đề", 2)});
    QCOMPARE(m.data(m.index(0), CommitsModel::SubjectRole).toString(), QStringLiteral("Tiêu đề"));
    QVERIFY(m.data(m.index(0), CommitsModel::IsMergeRole).toBool());
    QVERIFY(!m.data(m.index(0), CommitsModel::DateRole).toString().isEmpty());
    QCOMPARE(m.roleNames().value(CommitsModel::ShortHashRole), QByteArray("shortHash"));
}

void TstListModels::commits_graphLanesFollowParents()
{
    CommitInfo merge = ci("merge", "merge", 2);
    merge.parents = {"feature", "main"};
    CommitInfo feature = ci("feature", "feature");
    feature.parents = {"base"};
    CommitInfo main = ci("main", "main");
    main.parents = {"base"};
    CommitInfo base = ci("base", "base", 0);
    base.parents.clear();
    CommitsModel m;
    m.setCommits({merge, feature, main, base});

    const QModelIndex mergeIndex = m.index(0);
    QCOMPARE(m.data(mergeIndex, CommitsModel::GraphNodeLaneRole).toInt(), 0);
    QCOMPARE(m.data(mergeIndex, CommitsModel::GraphEdgesRole).toList().size(), 2);
    QCOMPARE(m.data(m.index(1), CommitsModel::GraphNodeLaneRole).toInt(), 0);
    QCOMPARE(m.data(m.index(2), CommitsModel::GraphNodeLaneRole).toInt(), 1);
    QVERIFY(m.data(m.index(2), CommitsModel::GraphVerticalLanesRole).toList().contains(0));
}

void TstListModels::commitFiles_roles()
{
    CommitFilesModel m;
    CommitFile f;
    f.status = 'R';
    f.path = "new.txt";
    f.origPath = "old.txt";
    m.setFiles({f});
    QCOMPARE(m.data(m.index(0), CommitFilesModel::StatusRole).toString(), "R");
    QCOMPARE(m.data(m.index(0), CommitFilesModel::OrigPathRole).toString(), "old.txt");
}

QTEST_APPLESS_MAIN(TstListModels)
#include "tst_listmodels.moc"
