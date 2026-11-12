#include <SyntaxHighlighter.h>

#include <QtTest>

using namespace gitapp;
using Lang = SyntaxHighlighter::Language;

class TstSyntax : public QObject {
    Q_OBJECT
private slots:
    void languageDetection();
    void cpp_keywordsTypesStringsNumbersAndComments();
    void cpp_preprocessorDirective();
    void blockComment_spansLinesViaState();
    void blockComment_endsAndCodeContinues();
    void typescript_typesAndTemplateStrings();
    void python_decoratorsAndHashComments();
    void shell_variablesAndComments();
    void json_stringsNumbersLiterals();
    void markdown_headingsFencesAndInlineCode();
    void escapedQuotes_doNotEndStrings();
    void unknownLanguageAndEmptyLines();
    void tokensNeverOverlapOrExceedLine();
};

// Tìm role của token bao phủ đoạn `text` trong `line`; Plain nếu không có token.
static TokenRole roleOf(Lang lang, const QString &line, const QString &text, SyntaxHighlighter::State *st = nullptr)
{
    const int pos = static_cast<int>(line.indexOf(text));
    Q_ASSERT(pos >= 0);
    for (const Token &t : SyntaxHighlighter::highlightLine(lang, line, st))
        if (pos >= t.start && pos < t.start + t.length)
            return t.role;
    return TokenRole::Plain;
}

void TstSyntax::languageDetection()
{
    QCOMPARE(SyntaxHighlighter::languageForPath("src/a.cpp"), Lang::CLike);
    QCOMPARE(SyntaxHighlighter::languageForPath("a.HPP"), Lang::CLike);
    QCOMPARE(SyntaxHighlighter::languageForPath("app/x/section-list.tsx"), Lang::JavaScript);
    QCOMPARE(SyntaxHighlighter::languageForPath("run.py"), Lang::Python);
    QCOMPARE(SyntaxHighlighter::languageForPath("CMakeLists.txt"), Lang::CMake);
    QCOMPARE(SyntaxHighlighter::languageForPath("notes.txt"), Lang::None);
    QCOMPARE(SyntaxHighlighter::languageForPath("Makefile"), Lang::None);
    QCOMPARE(SyntaxHighlighter::languageForPath("a.qml"), Lang::Qml);
}

void TstSyntax::cpp_keywordsTypesStringsNumbersAndComments()
{
    const QString line = QStringLiteral("static const size_t N = 42; auto s = \"hi\"; // ghi chú");
    QCOMPARE(roleOf(Lang::CLike, line, "static"), TokenRole::Keyword);
    QCOMPARE(roleOf(Lang::CLike, line, "size_t"), TokenRole::Type);
    QCOMPARE(roleOf(Lang::CLike, line, "42"), TokenRole::Number);
    QCOMPARE(roleOf(Lang::CLike, line, "\"hi\""), TokenRole::String);
    QCOMPARE(roleOf(Lang::CLike, line, "ghi chú"), TokenRole::Comment);
    QCOMPARE(roleOf(Lang::CLike, line, " N "), TokenRole::Plain);  // hằng VIẾT_HOA không bị coi là kiểu
    QCOMPARE(roleOf(Lang::CLike, QStringLiteral("QString name;"), "QString"), TokenRole::Type);
}

void TstSyntax::cpp_preprocessorDirective()
{
    const QString line = QStringLiteral("#include <QString>");
    QCOMPARE(roleOf(Lang::CLike, line, "#include"), TokenRole::Keyword);
    QCOMPARE(roleOf(Lang::CLike, QStringLiteral("  #  define X 1"), "define"), TokenRole::Keyword);
}

void TstSyntax::blockComment_spansLinesViaState()
{
    SyntaxHighlighter::State st;
    const auto first = SyntaxHighlighter::highlightLine(Lang::CLike, QStringLiteral("int a; /* bắt đầu"), &st);
    QVERIFY(st.inBlockComment);
    QCOMPARE(first.last().role, TokenRole::Comment);
    const auto mid = SyntaxHighlighter::highlightLine(Lang::CLike, QStringLiteral("  vẫn là chú thích return"), &st);
    QVERIFY(st.inBlockComment);
    QCOMPARE(mid.size(), 1);
    QCOMPARE(mid[0].role, TokenRole::Comment);  // "return" bên trong chú thích không bị coi là từ khoá
}

void TstSyntax::blockComment_endsAndCodeContinues()
{
    SyntaxHighlighter::State st;
    st.inBlockComment = true;
    const QString line = QStringLiteral("kết thúc */ return 7;");
    QCOMPARE(roleOf(Lang::CLike, line, "kết thúc", &st), TokenRole::Comment);
    QVERIFY(!st.inBlockComment);
    st.inBlockComment = true;
    QCOMPARE(roleOf(Lang::CLike, line, "return", &st), TokenRole::Keyword);
    st.inBlockComment = true;
    QCOMPARE(roleOf(Lang::CLike, line, "7", &st), TokenRole::Number);
    // Chú thích khối trọn trong một dòng.
    SyntaxHighlighter::State st2;
    QCOMPARE(roleOf(Lang::JavaScript, QStringLiteral("/** doc */ const x"), "doc", &st2), TokenRole::Comment);
    QVERIFY(!st2.inBlockComment);
}

void TstSyntax::typescript_typesAndTemplateStrings()
{
    const QString line = QStringLiteral("  readonly onRowFocus?: (indexPath: RowIndexPath, e: React.FocusEvent<any>) => void");
    QCOMPARE(roleOf(Lang::JavaScript, line, "readonly"), TokenRole::Keyword);
    QCOMPARE(roleOf(Lang::JavaScript, line, "RowIndexPath"), TokenRole::Type);
    QCOMPARE(roleOf(Lang::JavaScript, line, "FocusEvent"), TokenRole::Type);
    QCOMPARE(roleOf(Lang::JavaScript, line, "any"), TokenRole::Type);
    QCOMPARE(roleOf(Lang::JavaScript, line, "void"), TokenRole::Type);  // 'void' là kiểu trong danh sách kiểu của JS/TS
    QCOMPARE(roleOf(Lang::JavaScript, line, "onRowFocus"), TokenRole::Plain);
    QCOMPARE(roleOf(Lang::JavaScript, "const s = `a ${b}`;", "`a ${b}`"), TokenRole::String);
}

void TstSyntax::python_decoratorsAndHashComments()
{
    QCOMPARE(roleOf(Lang::Python, "@dataclass", "@dataclass"), TokenRole::Keyword);
    QCOMPARE(roleOf(Lang::Python, "def f(self): # note", "def"), TokenRole::Keyword);
    QCOMPARE(roleOf(Lang::Python, "def f(self): # note", "note"), TokenRole::Comment);
    QCOMPARE(roleOf(Lang::Python, "x = 'a#b'", "a#b"), TokenRole::String);  // # trong chuỗi không phải chú thích
    QCOMPARE(roleOf(Lang::Python, "class Foo(Base):", "Foo"), TokenRole::Type);
}

void TstSyntax::shell_variablesAndComments()
{
    const QString line = QStringLiteral("if [ -n \"$HOME\" ]; then echo ${USER} $PATH # xong");
    QCOMPARE(roleOf(Lang::Shell, line, "if"), TokenRole::Keyword);
    QCOMPARE(roleOf(Lang::Shell, line, "then"), TokenRole::Keyword);
    QCOMPARE(roleOf(Lang::Shell, line, "${USER}"), TokenRole::Type);
    QCOMPARE(roleOf(Lang::Shell, line, "$PATH"), TokenRole::Type);
    QCOMPARE(roleOf(Lang::Shell, line, "xong"), TokenRole::Comment);
}

void TstSyntax::json_stringsNumbersLiterals()
{
    const QString line = QStringLiteral("  \"count\": 12.5, \"ok\": true, \"x\": null");
    QCOMPARE(roleOf(Lang::Json, line, "\"count\""), TokenRole::String);
    QCOMPARE(roleOf(Lang::Json, line, "12.5"), TokenRole::Number);
    QCOMPARE(roleOf(Lang::Json, line, "true"), TokenRole::Keyword);
    QCOMPARE(roleOf(Lang::Json, line, "null"), TokenRole::Keyword);
}

void TstSyntax::markdown_headingsFencesAndInlineCode()
{
    QCOMPARE(roleOf(Lang::Markdown, "## Tiêu đề", "Tiêu"), TokenRole::Keyword);
    QCOMPARE(roleOf(Lang::Markdown, "```cpp", "cpp"), TokenRole::Comment);
    QCOMPARE(roleOf(Lang::Markdown, "dùng `git add` nhé", "git add"), TokenRole::String);
    QCOMPARE(roleOf(Lang::Markdown, "- mục một", "-"), TokenRole::Keyword);
    QCOMPARE(roleOf(Lang::Markdown, "văn bản thường", "thường"), TokenRole::Plain);
}

void TstSyntax::escapedQuotes_doNotEndStrings()
{
    const QString line = QStringLiteral("auto s = \"a\\\"b\"; int n = 3;");
    QCOMPARE(roleOf(Lang::CLike, line, "a\\\"b"), TokenRole::String);
    QCOMPARE(roleOf(Lang::CLike, line, "3"), TokenRole::Number);  // code sau chuỗi vẫn được tô đúng
    // Chuỗi không đóng: tô đến hết dòng, không crash.
    QCOMPARE(roleOf(Lang::CLike, QStringLiteral("auto s = \"chưa đóng"), "chưa"), TokenRole::String);
}

void TstSyntax::unknownLanguageAndEmptyLines()
{
    QVERIFY(SyntaxHighlighter::highlightLine(Lang::None, "return 1;", nullptr).isEmpty());
    QVERIFY(SyntaxHighlighter::highlightLine(Lang::CLike, "", nullptr).isEmpty());
    QVERIFY(SyntaxHighlighter::highlightLine(Lang::CLike, "   ", nullptr).isEmpty());
}

void TstSyntax::tokensNeverOverlapOrExceedLine()
{
    const QStringList samples = {
        "int main(int argc, char **argv) { return argc > 1 ? 1 : 0; } // end",
        "const a = `x${y}` + 'z' /* c */ + 0x1F;", "x = \"\\\\\" # \\", "\"unterminated", "/* open",
        "#include <a>/*x*/ int", "$(( 1 + 2 ))", "ü ö ơ ư — unicode 漢字 \"字\"",
    };
    for (Lang lang : {Lang::CLike, Lang::JavaScript, Lang::Python, Lang::Shell, Lang::Json, Lang::Rust,
                      Lang::Go, Lang::Java, Lang::Css, Lang::CMake, Lang::Yaml, Lang::Markdown}) {
        for (const QString &s : samples) {
            SyntaxHighlighter::State st;
            int end = 0;
            for (const Token &t : SyntaxHighlighter::highlightLine(lang, s, &st)) {
                QVERIFY2(t.start >= end, qPrintable(s));
                QVERIFY2(t.length > 0 && t.start + t.length <= s.size(), qPrintable(s));
                end = t.start + t.length;
            }
        }
    }
}

QTEST_APPLESS_MAIN(TstSyntax)
#include "tst_syntax.moc"
