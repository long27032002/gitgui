#include <GitRunner.h>
#include <HistoryController.h>

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace gitapp;
using namespace gitcore;

class TstHistoryController : public QObject {
    Q_OBJECT
private slots:
    void init();
    void reload_loadsCommits_selectsNewest_andShowsFilesAndDiff();
    void emptyRepo_hasNoCommits();
    void loadMore_pagesThroughHistory();
    void selectFile_switchesDiff();
    void rapidSelection_lastRequestWins();
    void setService_clearsEverything();
    void reload_keepsSelectedCommit();

private:
    void git(const QStringList &args);
    void commitFile(const QString &rel, const QByteArray &data, const QString &msg);
    static QStringList diffTexts(HistoryController &h);
    QString path() const { return m_dir->path(); }

    std::unique_ptr<QTemporaryDir> m_dir;
};

void TstHistoryController::git(const QStringList &args)
{
    const GitResult r = GitRunner::run(path(), args);
    QVERIFY2(r.ok(), qPrintable(args.join(' ') + ": " + r.err));
}

void TstHistoryController::commitFile(const QString &rel, const QByteArray &data, const QString &msg)
{
    QFile f(path() + '/' + rel);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(data);
    f.close();
    git({"add", "-A"});
    git({"commit", "-q", "-m", msg});
}

QStringList TstHistoryController::diffTexts(HistoryController &h)
{
    QStringList out;
    QAbstractItemModel *m = h.diff();
    for (int i = 0; i < m->rowCount(); ++i)
        out << m->data(m->index(i, 0), DiffModel::TextRole).toString();
    return out;
}

void TstHistoryController::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    git({"init", "-q", "-b", "main"});
    git({"config", "user.name", "Test"});
    git({"config", "user.email", "test@example.com"});
    git({"config", "commit.gpgsign", "false"});
}

void TstHistoryController::reload_loadsCommits_selectsNewest_andShowsFilesAndDiff()
{
    commitFile("a.txt", "one\n", "first");
    commitFile("a.txt", "two\n", "second\n\nthân commit");

    HistoryController h;
    h.setService(GitService(path()));
    h.reload();
    QTRY_COMPARE(h.commits()->rowCount(), 2);
    QTRY_VERIFY(!h.selectedHash().isEmpty());
    QTRY_VERIFY(h.message().startsWith("second"));
    QVERIFY(h.message().contains("thân commit"));
    QTRY_COMPARE(h.files()->rowCount(), 1);
    QTRY_VERIFY(diffTexts(h).contains("+two"));
    QCOMPARE(h.selectedFile(), "a.txt");
    QVERIFY(!h.loading());
}

void TstHistoryController::emptyRepo_hasNoCommits()
{
    HistoryController h;
    h.setService(GitService(path()));
    h.reload();
    QTRY_VERIFY(!h.loading());
    QCOMPARE(h.commits()->rowCount(), 0);
    QVERIFY(h.selectedHash().isEmpty());
}

void TstHistoryController::loadMore_pagesThroughHistory()
{
    for (int i = 1; i <= 5; ++i)
        commitFile("f.txt", QByteArray::number(i) + "\n", QString("c%1").arg(i));

    HistoryController h;
    h.setPageSize(2);
    h.setService(GitService(path()));
    h.reload();
    QTRY_COMPARE(h.commits()->rowCount(), 2);
    QVERIFY(h.canLoadMore());

    h.loadMore();
    QTRY_COMPARE(h.commits()->rowCount(), 4);
    h.loadMore();
    QTRY_COMPARE(h.commits()->rowCount(), 5);
    QTRY_VERIFY(!h.canLoadMore());  // trang cuối ngắn hơn -> hết lịch sử
}

void TstHistoryController::selectFile_switchesDiff()
{
    commitFile("a.txt", "a\n", "base");
    QFile f1(path() + "/a.txt"), f2(path() + "/b.txt");
    QVERIFY(f1.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f1.write("a2\n");
    f1.close();
    QVERIFY(f2.open(QIODevice::WriteOnly));
    f2.write("bbb\n");
    f2.close();
    git({"add", "-A"});
    git({"commit", "-q", "-m", "two files"});

    HistoryController h;
    h.setService(GitService(path()));
    h.reload();
    QTRY_COMPARE(h.files()->rowCount(), 2);
    QTRY_VERIFY(!diffTexts(h).isEmpty());
    h.selectFile(1);
    QTRY_COMPARE(h.selectedFile(), "b.txt");
    QTRY_VERIFY(diffTexts(h).contains("+bbb"));
}

void TstHistoryController::rapidSelection_lastRequestWins()
{
    commitFile("a.txt", "1\n", "c1");
    commitFile("a.txt", "2\n", "c2");
    commitFile("a.txt", "3\n", "c3");

    HistoryController h;
    h.setService(GitService(path()));
    h.reload();
    QTRY_COMPARE(h.commits()->rowCount(), 3);
    h.selectCommit(1);
    h.selectCommit(2);  // commit "c1" — yêu cầu cuối cùng
    QTRY_VERIFY(diffTexts(h).contains("+1"));
    QTest::qWait(200);
    QCOMPARE(h.message(), "c1");
    QVERIFY(diffTexts(h).contains("+1"));
    QVERIFY(!diffTexts(h).contains("+2"));
}

void TstHistoryController::setService_clearsEverything()
{
    commitFile("a.txt", "1\n", "c1");
    HistoryController h;
    h.setService(GitService(path()));
    h.reload();
    QTRY_VERIFY(!h.selectedHash().isEmpty());

    h.setService(std::nullopt);
    QCOMPARE(h.commits()->rowCount(), 0);
    QCOMPARE(h.files()->rowCount(), 0);
    QCOMPARE(h.diff()->rowCount(), 0);
    QVERIFY(h.selectedHash().isEmpty());
    h.reload();  // không có repo: không làm gì, không crash
    QCOMPARE(h.commits()->rowCount(), 0);
}

void TstHistoryController::reload_keepsSelectedCommit()
{
    commitFile("a.txt", "1\n", "c1");
    commitFile("a.txt", "2\n", "c2");
    HistoryController h;
    h.setService(GitService(path()));
    h.reload();
    QTRY_COMPARE(h.commits()->rowCount(), 2);
    h.selectCommit(1);
    QTRY_COMPARE(h.message(), "c1");
    const QString hash = h.selectedHash();

    commitFile("a.txt", "3\n", "c3");
    h.reload();
    QTRY_COMPARE(h.commits()->rowCount(), 3);
    QCOMPARE(h.selectedHash(), hash);  // vẫn xem commit cũ, không nhảy về đầu danh sách
}

QTEST_GUILESS_MAIN(TstHistoryController)
#include "tst_historycontroller.moc"
