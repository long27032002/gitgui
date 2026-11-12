#include <GitRunner.h>
#include <RecentRepos.h>
#include <RepoController.h>
#include <Theme.h>

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace gitapp;
using namespace gitcore;

static bool read_all_contains(const QString &file, const QByteArray &needle)
{
    QFile f(file);
    return f.open(QIODevice::ReadOnly) && f.readAll().contains(needle);
}

class TstUiFeatures : public QObject {
    Q_OBJECT
private slots:
    void init();

    void theme_darkAndLightPalettesDiffer_andToggleNotifies();
    void recent_touchMovesToFront_andDeduplicates();
    void recent_isCappedAtMax();
    void recent_persistsAcrossInstances();
    void relativeTime_phrases();
    void recentRepos_populatedOnOpen_andDroppedWhenMissing();
    void selectAllState_andToggleAll();
    void toggleAll_skipsConflictedFiles();
    void commit_appendsCoAuthorTrailers();
    void commit_rejectsMalformedCoAuthor();
    void userName_andLastFetchedText_andSelectedStatus();

private:
    QByteArray git(const QStringList &args, bool *ok = nullptr);
    void write(const QString &rel, const QByteArray &data);
    void openAndWait(RepoController &c);
    QString path() const { return m_dir->path(); }

    std::unique_ptr<QTemporaryDir> m_dir;
};

QByteArray TstUiFeatures::git(const QStringList &args, bool *ok)
{
    const GitResult r = GitRunner::run(path(), args);
    if (ok)
        *ok = r.ok();
    else
        Q_ASSERT_X(r.ok(), "git", qPrintable(args.join(' ') + ": " + r.err));
    return r.out;
}

void TstUiFeatures::write(const QString &rel, const QByteArray &data)
{
    QFile f(path() + '/' + rel);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(data);
}

void TstUiFeatures::openAndWait(RepoController &c)
{
    c.openRepo(path());
    QTRY_VERIFY(c.hasRepo());
    QTRY_VERIFY(!c.busy());
}

void TstUiFeatures::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    git({"init", "-q", "-b", "main"});
    git({"config", "user.name", "Ada Dev"});
    git({"config", "user.email", "ada@example.com"});
    git({"config", "commit.gpgsign", "false"});
}

void TstUiFeatures::theme_darkAndLightPalettesDiffer_andToggleNotifies()
{
    Theme t;
    QVERIFY(t.dark());  // mặc định tối như ảnh tham khảo
    const QColor darkBg = t.sidebar();
    QSignalSpy spy(&t, &Theme::changed);
    t.toggle();
    QVERIFY(!t.dark());
    QCOMPARE(spy.count(), 1);
    QVERIFY(t.sidebar() != darkBg);
    QVERIFY(t.sidebar().lightness() > darkBg.lightness());
    t.setDark(false);  // không đổi -> không phát tín hiệu thừa
    QCOMPARE(spy.count(), 1);
    t.toggle();
    QCOMPARE(t.sidebar(), darkBg);
    // Mọi màu trong bảng đều hợp lệ
    QVERIFY(t.diffAdd().isValid() && t.gutterSelected().isValid() && t.conflict().isValid());
}

void TstUiFeatures::recent_touchMovesToFront_andDeduplicates()
{
    RecentRepos r;
    r.touch("/a");
    r.touch("/b");
    r.touch("/c");
    r.touch("/a");  // đưa lên đầu, không nhân đôi
    QCOMPARE(r.paths(), (QStringList{"/a", "/c", "/b"}));
    QCOMPARE(r.data(r.index(0), RecentRepos::NameRole).toString(), "a");
    r.remove("/c");
    QCOMPARE(r.paths(), (QStringList{"/a", "/b"}));
    r.remove("/khong-co");  // không lỗi
    QCOMPARE(r.rowCount(), 2);
}

void TstUiFeatures::recent_isCappedAtMax()
{
    RecentRepos r;
    for (int i = 0; i < RecentRepos::kMax + 5; ++i)
        r.touch(QString("/repo%1").arg(i));
    QCOMPARE(r.rowCount(), RecentRepos::kMax);
    QCOMPARE(r.paths().first(), QString("/repo%1").arg(RecentRepos::kMax + 4));  // mới nhất ở đầu
    QVERIFY(!r.paths().contains("/repo0"));                                      // cũ nhất bị bỏ
}

void TstUiFeatures::recent_persistsAcrossInstances()
{
    const QString ini = path() + "/recent.ini";
    {
        RecentRepos r;
        r.enablePersistence(ini);
        r.touch("/x");
        r.touch("/y");
    }
    RecentRepos again;
    again.enablePersistence(ini);
    QCOMPARE(again.paths(), (QStringList{"/y", "/x"}));
    again.remove("/y");
    RecentRepos third;
    third.enablePersistence(ini);
    QCOMPARE(third.paths(), (QStringList{"/x"}));
}

void TstUiFeatures::relativeTime_phrases()
{
    const QDateTime now(QDate(2026, 11, 7), QTime(12, 0, 0));
    QVERIFY(RepoController::relativeTimeText({}, now).contains("Chưa"));
    QVERIFY(RepoController::relativeTimeText(now.addSecs(-20), now).contains("Vừa"));
    QVERIFY(RepoController::relativeTimeText(now.addSecs(-8 * 60), now).contains("8 phút"));
    QVERIFY(RepoController::relativeTimeText(now.addSecs(-3 * 3600), now).contains("3 giờ"));
    QVERIFY(RepoController::relativeTimeText(now.addDays(-2), now).contains("2 ngày"));
}

void TstUiFeatures::recentRepos_populatedOnOpen_andDroppedWhenMissing()
{
    write("a.txt", "a\n");
    git({"add", "a.txt"});
    git({"commit", "-q", "-m", "c1"});
    RepoController c;
    openAndWait(c);
    QCOMPARE(c.recent()->paths().size(), 1);
    QCOMPARE(c.recent()->paths().first(), QFileInfo(path()).canonicalFilePath());

    QTemporaryDir gone;
    const QString missing = gone.path() + "/da-xoa";
    c.recent()->touch(missing);
    QCOMPARE(c.recent()->paths().size(), 2);
    c.openRepo(missing);  // không còn tồn tại -> báo lỗi và gỡ khỏi danh sách
    QTRY_VERIFY(!c.errorMessage().isEmpty());
    QVERIFY(!c.recent()->paths().contains(missing));
}

void TstUiFeatures::selectAllState_andToggleAll()
{
    write("a.txt", "a\n");
    write("b.txt", "b\n");
    RepoController c;
    openAndWait(c);
    QCOMPARE(c.selectAllState(), 2);  // mặc định chọn hết
    QCOMPARE(c.includedCount(), 2);

    c.toggleAll();  // đã chọn hết -> bỏ hết
    QCOMPARE(c.selectAllState(), 0);
    QCOMPARE(c.includedCount(), 0);

    c.toggleChecked(0);  // chọn lại một file -> một phần
    QCOMPARE(c.selectAllState(), 1);
    QCOMPARE(c.includedCount(), 1);

    c.toggleAll();  // đang một phần -> chọn hết
    QCOMPARE(c.selectAllState(), 2);
    c.toggleAll();
    QCOMPARE(c.selectAllState(), 0);
    QVERIFY(git({"diff", "--cached"}).isEmpty());  // chỉ là lựa chọn trong app, không đụng index
}

void TstUiFeatures::toggleAll_skipsConflictedFiles()
{
    write("f.txt", "base\n");
    write("ok.txt", "ok\n");
    git({"add", "-A"});
    git({"commit", "-q", "-m", "base"});
    git({"switch", "-q", "-c", "other"});
    write("f.txt", "other\n");
    git({"commit", "-q", "-am", "o"});
    git({"switch", "-q", "main"});
    write("f.txt", "main\n");
    git({"commit", "-q", "-am", "m"});
    bool ok = true;
    git({"merge", "other"}, &ok);
    QVERIFY(!ok);
    write("ok.txt", "changed\n");

    RepoController c;
    openAndWait(c);
    QCOMPARE(c.conflictCount(), 1);
    QCOMPARE(c.includedCount(), 1);  // file xung đột không tính là được chọn

    c.toggleAll();  // bỏ hết (trừ file xung đột)
    QCOMPARE(c.includedCount(), 0);
    c.toggleAll();
    QCOMPARE(c.includedCount(), 1);
    QCOMPARE(c.conflictCount(), 1);  // file xung đột KHÔNG bị `git add`
    QVERIFY(read_all_contains(path() + "/f.txt", "<<<<<<<"));
    QVERIFY(git({"diff", "--cached", "--name-only"}).isEmpty() || !QString::fromUtf8(git({"diff", "--cached", "--name-only"})).contains("ok.txt"));
}

void TstUiFeatures::commit_appendsCoAuthorTrailers()
{
    write("a.txt", "a\n");
    RepoController c;
    openAndWait(c);
    QCOMPARE(c.includedCount(), 1);

    c.commit("Thêm a", "mô tả", "Bob Lee <bob@example.com>, Cy <cy@example.org>\nDi <di@x.io>");
    QTRY_COMPARE(c.changeCount(), 0);
    QTRY_VERIFY(!c.busy());
    QVERIFY(c.errorMessage().isEmpty());
    const QString msg = QString::fromUtf8(git({"log", "-1", "--format=%B"}));
    QVERIFY(msg.startsWith("Thêm a\n\nmô tả\n\nCo-authored-by: Bob Lee <bob@example.com>"));
    QVERIFY(msg.contains("Co-authored-by: Cy <cy@example.org>"));
    QVERIFY(msg.contains("Co-authored-by: Di <di@x.io>"));
    // git nhận ra trailer
    const QString trailers = QString::fromUtf8(git({"log", "-1", "--format=%(trailers:key=Co-authored-by)"}));
    QCOMPARE(trailers.count("Co-authored-by"), 3);
}

void TstUiFeatures::commit_rejectsMalformedCoAuthor()
{
    write("a.txt", "a\n");
    RepoController c;
    openAndWait(c);
    QCOMPARE(c.includedCount(), 1);

    c.commit("x", "", "không có email");
    QVERIFY(c.errorMessage().contains("định dạng"));
    c.clearError();
    c.commit("x", "", "Tên <khong-co-a>");
    QVERIFY(!c.errorMessage().isEmpty());
    QTest::qWait(100);
    QCOMPARE(c.includedCount(), 1);  // chưa commit gì
    QVERIFY(git({"rev-list", "--all", "--count"}).trimmed() == "0");
}

void TstUiFeatures::userName_andLastFetchedText_andSelectedStatus()
{
    QTemporaryDir remote;
    QVERIFY(GitRunner::run(remote.path(), {"init", "--bare", "-q", "-b", "main"}).ok());
    write("a.txt", "a\n");
    git({"add", "a.txt"});
    git({"commit", "-q", "-m", "c1"});
    git({"remote", "add", "origin", remote.path()});
    git({"push", "-q", "-u", "origin", "main"});
    write("a.txt", "b\n");
    write("new.txt", "n\n");

    RepoController c;
    QVERIFY(c.lastFetchedText().contains("Chưa"));
    openAndWait(c);
    QCOMPARE(c.userName(), "Ada Dev");
    QVERIFY(c.lastFetchedText().contains("Chưa"));  // push không tạo FETCH_HEAD nên chưa tính là đã fetch
    QCOMPARE(c.selectedStatus(), "M");  // file đầu tiên theo tên: a.txt (đã sửa)
    c.selectRow(1);
    QTRY_COMPARE(c.selectedStatus(), "?");

    QSignalSpy spy(&c, &RepoController::lastFetchedTextChanged);
    c.fetch();
    QTRY_VERIFY(spy.count() >= 1);
    QVERIFY(c.lastFetchedText().contains("Vừa"));
}

QTEST_GUILESS_MAIN(TstUiFeatures)
#include "tst_uifeatures.moc"
