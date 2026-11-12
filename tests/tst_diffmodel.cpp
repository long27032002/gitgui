#include <DiffModel.h>
#include <Theme.h>

#include <QtTest>

using namespace gitapp;
using namespace gitcore;

static const QByteArray kTwoHunks =
    "--- a/f\n+++ b/f\n"
    "@@ -1,3 +1,3 @@\n a\n-b\n+B\n c\n"
    "@@ -20,3 +20,3 @@\n m\n-n\n+N\n o\n";

class TstDiffModel : public QObject {
    Q_OBJECT
private slots:
    void rows_areBuiltFromHunks();
    void toggleLine_flipsSelectionAndHunkBecomesPartial();
    void toggleHunk_selectsAllThenClears();
    void readOnlyDiff_isNotSelectable();
    void plainLines_classifyByFirstChar();
    void reset_clearsSelection();
    void excludedKeys_startUnselected_andSelectAllWorks();
    void html_isEmptyWithoutThemeAndColoredWithOne();
    void lineNumbers_followHunkHeaders();
};

static int rowWithText(const QAbstractItemModel &m, const QString &text)
{
    for (int i = 0; i < m.rowCount(); ++i)
        if (m.data(m.index(i, 0), DiffModel::TextRole).toString() == text)
            return i;
    return -1;
}

void TstDiffModel::rows_areBuiltFromHunks()
{
    DiffModel m;
    m.setFileDiff(GitDiff::parse(kTwoHunks), true);
    QCOMPARE(m.rowCount(), 10);  // 2 đầu khối + 8 dòng
    QCOMPARE(m.data(m.index(0), DiffModel::KindRole).toInt(), int(DiffModel::Hunk));
    const int plus = rowWithText(m, "+B");
    QVERIFY(plus > 0);
    QVERIFY(m.data(m.index(plus), DiffModel::SelectableRole).toBool());
    const int ctx = rowWithText(m, " a");
    QVERIFY(!m.data(m.index(ctx), DiffModel::SelectableRole).toBool());
}

void TstDiffModel::toggleLine_flipsSelectionAndHunkBecomesPartial()
{
    DiffModel m;
    m.setFileDiff(GitDiff::parse(kTwoHunks), true);
    QCOMPARE(m.selectedCount(), 4);  // mặc định mọi dòng thay đổi đều được đưa vào commit
    QCOMPARE(m.data(m.index(0), DiffModel::CheckStateRole).toInt(), 2);
    QSignalSpy spy(&m, &DiffModel::selectionChanged);

    m.toggleRow(rowWithText(m, "+B"));  // loại một dòng
    QCOMPARE(m.selectedCount(), 3);
    QCOMPARE(m.data(m.index(0), DiffModel::CheckStateRole).toInt(), 1);  // hunk 1: một phần
    QCOMPARE(m.data(m.index(5), DiffModel::CheckStateRole).toInt(), 2);  // hunk 2: vẫn đủ
    QCOMPARE(spy.count(), 1);

    m.toggleRow(rowWithText(m, "+B"));  // chọn lại
    QCOMPARE(m.selectedCount(), 4);
}

void TstDiffModel::toggleHunk_selectsAllThenClears()
{
    DiffModel m;
    m.setFileDiff(GitDiff::parse(kTwoHunks), true);
    m.toggleRow(0);  // đang chọn đủ -> bỏ cả khối 1
    QCOMPARE(m.selectedCount(), 2);
    QCOMPARE(m.data(m.index(0), DiffModel::CheckStateRole).toInt(), 0);
    m.toggleRow(0);  // chọn lại cả khối
    QCOMPARE(m.selectedCount(), 4);
    QCOMPARE(m.data(m.index(0), DiffModel::CheckStateRole).toInt(), 2);

    // Đang chọn một phần -> bấm đầu khối sẽ chọn nốt phần còn lại.
    m.toggleRow(rowWithText(m, "-b"));
    QCOMPARE(m.data(m.index(0), DiffModel::CheckStateRole).toInt(), 1);
    m.toggleRow(0);
    QCOMPARE(m.selectedCount(), 4);
}

void TstDiffModel::readOnlyDiff_isNotSelectable()
{
    DiffModel m;
    m.setFileDiff(GitDiff::parse(kTwoHunks), false);
    m.toggleRow(rowWithText(m, "+B"));
    QCOMPARE(m.selectedCount(), 0);  // chỉ xem: không có lựa chọn nào
    QVERIFY(!m.data(m.index(rowWithText(m, "+B")), DiffModel::SelectableRole).toBool());
}

void TstDiffModel::plainLines_classifyByFirstChar()
{
    DiffModel m;
    m.setPlainLines({"@@ tệp mới @@", "+x", "-y", " z", "thông báo"});
    QCOMPARE(m.data(m.index(0), DiffModel::KindRole).toInt(), int(DiffModel::Hunk));
    QCOMPARE(m.data(m.index(1), DiffModel::KindRole).toInt(), int(DiffModel::Add));
    QCOMPARE(m.data(m.index(2), DiffModel::KindRole).toInt(), int(DiffModel::Remove));
    QCOMPARE(m.data(m.index(3), DiffModel::KindRole).toInt(), int(DiffModel::Context));
    QCOMPARE(m.data(m.index(4), DiffModel::KindRole).toInt(), int(DiffModel::Info));
    QVERIFY(!m.selectable());
}

void TstDiffModel::reset_clearsSelection()
{
    DiffModel m;
    m.setFileDiff(GitDiff::parse(kTwoHunks), true);
    m.toggleRow(0);
    QCOMPARE(m.selectedCount(), 2);
    m.setFileDiff(GitDiff::parse(kTwoHunks), true);  // nạp diff mới: lựa chọn đặt lại về mặc định (chọn hết)
    QCOMPARE(m.selectedCount(), 4);
    m.clear();
    QCOMPARE(m.selectedCount(), 0);
    QCOMPARE(m.rowCount(), 0);
}

void TstDiffModel::excludedKeys_startUnselected_andSelectAllWorks()
{
    DiffModel m;
    const FileDiff fd = GitDiff::parse(kTwoHunks);
    const QSet<QByteArray> excluded{"+N\x1f" "0", "-n\x1f" "0"};  // loại cặp đổi dòng ở hunk 2
    m.setFileDiff(fd, true, excluded);
    QCOMPARE(m.selectedCount(), 2);
    QCOMPARE(m.data(m.index(5), DiffModel::CheckStateRole).toInt(), 0);  // hunk 2: không chọn
    QCOMPARE(m.data(m.index(0), DiffModel::CheckStateRole).toInt(), 2);
    QCOMPARE(GitDiff::excludedKeys(m.fileDiff(), m.selection()), excluded);  // ánh xạ ngược khớp

    QSignalSpy spy(&m, &DiffModel::selectionChanged);
    m.selectAll(true);
    QCOMPARE(m.selectedCount(), 4);
    QCOMPARE(spy.count(), 1);
    m.selectAll(true);  // không đổi -> không phát tín hiệu
    QCOMPARE(spy.count(), 1);
    m.selectAll(false);
    QCOMPARE(m.selectedCount(), 0);

    DiffModel readOnly;
    readOnly.setFileDiff(fd, false);
    readOnly.selectAll(true);  // chỉ xem: bỏ qua
    QCOMPARE(readOnly.selectedCount(), 0);
}

void TstDiffModel::html_isEmptyWithoutThemeAndColoredWithOne()
{
    const FileDiff fd = GitDiff::parse(
        "--- a/x.cpp\n+++ b/x.cpp\n@@ -1,2 +1,2 @@\n int a;\n-return 1; // cũ\n+return 2 < 3 && \"ok\"; // mới\n");
    DiffModel plain;
    plain.setPath("x.cpp");
    plain.setFileDiff(fd, false);
    const int addRow = rowWithText(plain, "+return 2 < 3 && \"ok\"; // mới");
    QVERIFY(addRow > 0);
    QVERIFY(plain.data(plain.index(addRow), DiffModel::HtmlRole).toString().isEmpty());  // chưa gắn theme

    Theme theme;
    DiffModel m;
    m.setTheme(&theme);
    m.setPath("x.cpp");
    m.setFileDiff(fd, false);
    const QString html = m.data(m.index(addRow), DiffModel::HtmlRole).toString();
    QVERIFY(html.contains(QString("<font color=\"%1\">return</font>").arg(theme.synKeyword().name())));
    QVERIFY(html.contains(theme.synComment().name()));  // chú thích // mới
    QVERIFY(html.contains("&lt;"));                    // ký tự < được escape
    QVERIFY(html.contains("&amp;&amp;"));
    QVERIFY(html.contains("&nbsp;"));                  // khoảng trắng được đổi thành &nbsp; để giữ thụt đầu dòng

    // Đổi theme: màu đổi theo và model báo dữ liệu thay đổi.
    QSignalSpy changed(&m, &QAbstractItemModel::dataChanged);
    theme.toggle();
    QVERIFY(changed.count() >= 1);
    QVERIFY(m.data(m.index(addRow), DiffModel::HtmlRole).toString().contains(theme.synKeyword().name()));

    // File không nhận ra ngôn ngữ: không tô màu.
    DiffModel unknown;
    unknown.setTheme(&theme);
    unknown.setPath("README.unknown");
    unknown.setFileDiff(fd, false);
    QVERIFY(unknown.data(unknown.index(addRow), DiffModel::HtmlRole).toString().isEmpty());
}

void TstDiffModel::lineNumbers_followHunkHeaders()
{
    DiffModel m;
    m.setFileDiff(GitDiff::parse(kTwoHunks), false);
    const auto oldNo = [&](int row) { return m.data(m.index(row), DiffModel::OldNumberRole).toInt(); };
    const auto newNo = [&](int row) { return m.data(m.index(row), DiffModel::NewNumberRole).toInt(); };
    // hunk 1 (-1,3 +1,3): " a", "-b", "+B", " c"
    QCOMPARE(oldNo(0), 0);  // dòng đầu khối không có số
    QCOMPARE(newNo(0), 0);
    QCOMPARE(oldNo(1), 1); QCOMPARE(newNo(1), 1);   // ngữ cảnh: cả hai
    QCOMPARE(oldNo(2), 2); QCOMPARE(newNo(2), 0);   // dòng xoá: chỉ số cũ
    QCOMPARE(oldNo(3), 0); QCOMPARE(newNo(3), 2);   // dòng thêm: chỉ số mới
    QCOMPARE(oldNo(4), 3); QCOMPARE(newNo(4), 3);
    // hunk 2 (-20,3 +20,3): bắt đầu lại từ số của hunk
    QCOMPARE(oldNo(6), 20); QCOMPARE(newNo(6), 20);
    QCOMPARE(oldNo(7), 21);
    QCOMPARE(newNo(8), 21);
}

QTEST_APPLESS_MAIN(TstDiffModel)
#include "tst_diffmodel.moc"
