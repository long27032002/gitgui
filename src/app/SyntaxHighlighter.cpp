#include "SyntaxHighlighter.h"

#include <QFileInfo>
#include <QHash>
#include <QSet>

namespace gitapp {

namespace {

using Language = SyntaxHighlighter::Language;

struct LangDef {
    QSet<QString> keywords;
    QSet<QString> types;
    QStringList lineComments;
    QString blockStart, blockEnd;
    QString quotes = QStringLiteral("\"'");
    bool capitalizedAreTypes = false;  // Tên viết hoa chữ đầu (RowIndexPath, String...) coi là kiểu
    bool caseInsensitive = false;
    bool dollarVariables = false;      // $VAR của shell
    bool cPreprocessor = false;        // dòng #include/#define
    bool atWords = false;              // @decorator, @media
};

QSet<QString> words(const char *list)
{
    const QStringList parts = QString::fromLatin1(list).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    return QSet<QString>(parts.begin(), parts.end());
}

const QHash<Language, LangDef> &definitions()
{
    static const QHash<Language, LangDef> defs = [] {
        QHash<Language, LangDef> d;

        LangDef c;
        c.keywords = words(
            "alignas alignof and asm auto break case catch class const consteval constexpr constinit "
            "const_cast continue co_await co_return co_yield decltype default delete do dynamic_cast else "
            "enum explicit export extern false for friend goto if inline mutable namespace new noexcept "
            "nullptr operator override final private protected public register reinterpret_cast return "
            "sizeof static static_assert static_cast struct switch template this thread_local throw true "
            "try typedef typeid typename union using virtual volatile while concept requires");
        c.types = words("bool char double float int long short signed unsigned void wchar_t size_t "
                        "ssize_t ptrdiff_t uint8_t uint16_t uint32_t uint64_t int8_t int16_t int32_t int64_t");
        c.lineComments = {QStringLiteral("//")};
        c.blockStart = QStringLiteral("/*");
        c.blockEnd = QStringLiteral("*/");
        c.capitalizedAreTypes = true;
        c.cPreprocessor = true;
        d[Language::CLike] = c;

        LangDef js;
        js.keywords = words(
            "break case catch class const continue debugger default delete do else enum export extends "
            "false finally for function if implements import in instanceof interface let new null of "
            "package private protected public readonly return static super switch this throw true try "
            "typeof undefined var while with yield async await as from type namespace declare "
            "abstract keyof is constructor get set");
        js.types = words("any boolean number string object symbol unknown never bigint void");
        js.lineComments = {QStringLiteral("//")};
        js.blockStart = QStringLiteral("/*");
        js.blockEnd = QStringLiteral("*/");
        js.quotes = QStringLiteral("\"'`");
        js.capitalizedAreTypes = true;
        d[Language::JavaScript] = js;

        LangDef qml = js;
        qml.keywords += words("property signal required id on component pragma alias");
        d[Language::Qml] = qml;

        LangDef py;
        py.keywords = words(
            "and as assert async await break class continue def del elif else except finally for from "
            "global if import in is lambda nonlocal not or pass raise return try while with yield None "
            "True False self cls match case");
        py.types = words("int float str bool list dict set tuple bytes object complex");
        py.lineComments = {QStringLiteral("#")};
        py.capitalizedAreTypes = true;
        py.atWords = true;
        d[Language::Python] = py;

        LangDef rs;
        rs.keywords = words(
            "as async await break const continue crate dyn else enum extern false fn for if impl in let "
            "loop match mod move mut pub ref return self static struct super trait true type unsafe use "
            "where while");
        rs.types = words("i8 i16 i32 i64 i128 isize u8 u16 u32 u64 u128 usize f32 f64 bool char str");
        rs.lineComments = {QStringLiteral("//")};
        rs.blockStart = QStringLiteral("/*");
        rs.blockEnd = QStringLiteral("*/");
        rs.capitalizedAreTypes = true;
        d[Language::Rust] = rs;

        LangDef go;
        go.keywords = words(
            "break case chan const continue default defer else fallthrough for func go goto if import "
            "interface map package range return select struct switch type var true false nil");
        go.types = words("int int8 int16 int32 int64 uint uint8 uint16 uint32 uint64 uintptr float32 "
                         "float64 complex64 complex128 string bool byte rune error any");
        go.lineComments = {QStringLiteral("//")};
        go.blockStart = QStringLiteral("/*");
        go.blockEnd = QStringLiteral("*/");
        go.quotes = QStringLiteral("\"'`");
        go.capitalizedAreTypes = true;
        d[Language::Go] = go;

        LangDef java;
        java.keywords = words(
            "abstract assert break case catch class const continue default do else enum extends final "
            "finally for goto if implements import instanceof interface native new package private "
            "protected public return static strictfp super switch synchronized this throw throws "
            "transient try volatile while true false null var record fun val object when");
        java.types = words("int long short byte float double boolean char void String Object");
        java.lineComments = {QStringLiteral("//")};
        java.blockStart = QStringLiteral("/*");
        java.blockEnd = QStringLiteral("*/");
        java.capitalizedAreTypes = true;
        java.atWords = true;
        d[Language::Java] = java;

        LangDef sh;
        sh.keywords = words(
            "if then else elif fi for while until do done case esac in function select time return exit "
            "export local readonly declare unset source alias set shift break continue true false");
        sh.lineComments = {QStringLiteral("#")};
        sh.quotes = QStringLiteral("\"'`");
        sh.dollarVariables = true;
        d[Language::Shell] = sh;

        LangDef json;
        json.keywords = words("true false null");
        json.quotes = QStringLiteral("\"");
        d[Language::Json] = json;

        LangDef css;
        css.lineComments = {};
        css.blockStart = QStringLiteral("/*");
        css.blockEnd = QStringLiteral("*/");
        css.atWords = true;
        d[Language::Css] = css;

        LangDef cmake;
        cmake.keywords = words(
            "if else elseif endif foreach endforeach while endwhile function endfunction macro endmacro "
            "return break continue set unset option project cmake_minimum_required add_executable "
            "add_library add_subdirectory add_test target_link_libraries target_include_directories "
            "target_compile_options find_package include install file message list string");
        cmake.lineComments = {QStringLiteral("#")};
        cmake.quotes = QStringLiteral("\"");
        cmake.caseInsensitive = true;
        d[Language::CMake] = cmake;

        LangDef yaml;
        yaml.keywords = words("true false null yes no on off");
        yaml.lineComments = {QStringLiteral("#")};
        d[Language::Yaml] = yaml;
        d[Language::Toml] = yaml;
        return d;
    }();
    return defs;
}

bool isIdentStart(QChar c) { return c.isLetter() || c == QLatin1Char('_'); }
bool isIdentChar(QChar c) { return c.isLetterOrNumber() || c == QLatin1Char('_'); }

QVector<Token> highlightMarkdown(const QString &line)
{
    QVector<Token> out;
    const QString t = line.trimmed();
    const int lead = static_cast<int>(line.size() - line.trimmed().size());  // chỉ đúng khi thụt đầu dòng
    if (t.startsWith(QStringLiteral("```")) || t.startsWith(QStringLiteral("~~~"))) {
        out.append({lead, static_cast<int>(t.size()), TokenRole::Comment});
        return out;
    }
    if (t.startsWith(QLatin1Char('#'))) {
        out.append({lead, static_cast<int>(t.size()), TokenRole::Keyword});
        return out;
    }
    if (t.startsWith(QStringLiteral("- ")) || t.startsWith(QStringLiteral("* ")) || t.startsWith(QStringLiteral("+ ")))
        out.append({lead, 1, TokenRole::Keyword});
    // `mã nội dòng`
    int i = 0;
    while (i < line.size()) {
        if (line.at(i) == QLatin1Char('`')) {
            const int end = static_cast<int>(line.indexOf(QLatin1Char('`'), i + 1));
            if (end > i) {
                out.append({i, end - i + 1, TokenRole::String});
                i = end + 1;
                continue;
            }
        }
        ++i;
    }
    return out;
}

} // namespace

SyntaxHighlighter::Language SyntaxHighlighter::languageForPath(const QString &path)
{
    const QString name = QFileInfo(path).fileName();
    if (name.compare(QStringLiteral("CMakeLists.txt"), Qt::CaseInsensitive) == 0)
        return Language::CMake;
    const QString ext = QFileInfo(name).suffix().toLower();
    static const QHash<QString, Language> byExt = {
        {"c", Language::CLike}, {"h", Language::CLike}, {"cpp", Language::CLike}, {"cc", Language::CLike},
        {"cxx", Language::CLike}, {"hpp", Language::CLike}, {"hh", Language::CLike}, {"hxx", Language::CLike},
        {"ipp", Language::CLike}, {"inl", Language::CLike}, {"cs", Language::CLike},
        {"js", Language::JavaScript}, {"jsx", Language::JavaScript}, {"mjs", Language::JavaScript},
        {"cjs", Language::JavaScript}, {"ts", Language::JavaScript}, {"tsx", Language::JavaScript},
        {"mts", Language::JavaScript}, {"cts", Language::JavaScript},
        {"py", Language::Python}, {"pyw", Language::Python},
        {"json", Language::Json}, {"jsonc", Language::Json},
        {"md", Language::Markdown}, {"markdown", Language::Markdown},
        {"sh", Language::Shell}, {"bash", Language::Shell}, {"zsh", Language::Shell},
        {"rs", Language::Rust}, {"go", Language::Go}, {"java", Language::Java}, {"kt", Language::Java},
        {"qml", Language::Qml}, {"css", Language::Css}, {"scss", Language::Css}, {"less", Language::Css},
        {"cmake", Language::CMake}, {"yml", Language::Yaml}, {"yaml", Language::Yaml},
        {"toml", Language::Toml}, {"ini", Language::Toml}, {"cfg", Language::Toml}, {"conf", Language::Toml},
    };
    return byExt.value(ext, Language::None);
}

QVector<Token> SyntaxHighlighter::highlightLine(Language lang, const QString &line, State *state)
{
    if (lang == Language::None || line.isEmpty())
        return {};
    if (lang == Language::Markdown)
        return highlightMarkdown(line);

    const LangDef &def = definitions().value(lang);
    QVector<Token> out;
    const int n = static_cast<int>(line.size());
    int i = 0;
    State dummy;
    State &st = state ? *state : dummy;

    const auto add = [&out](int start, int length, TokenRole role) {
        if (length > 0)
            out.append({start, length, role});
    };

    // Chỉ thị tiền xử lý C/C++: "#include", "#define"...
    if (def.cPreprocessor && !st.inBlockComment) {
        int j = 0;
        while (j < n && line.at(j).isSpace())
            ++j;
        if (j < n && line.at(j) == QLatin1Char('#')) {
            int k = j + 1;
            while (k < n && line.at(k).isSpace())
                ++k;
            int e = k;
            while (e < n && isIdentChar(line.at(e)))
                ++e;
            if (e > k) {
                add(j, e - j, TokenRole::Keyword);
                i = e;
            }
        }
    }

    while (i < n) {
        if (st.inBlockComment) {
            const int end = static_cast<int>(line.indexOf(def.blockEnd, i));
            if (end < 0) {
                add(i, n - i, TokenRole::Comment);
                return out;
            }
            add(i, end + static_cast<int>(def.blockEnd.size()) - i, TokenRole::Comment);
            i = end + static_cast<int>(def.blockEnd.size());
            st.inBlockComment = false;
            continue;
        }
        const QChar c = line.at(i);

        bool matched = false;
        for (const QString &lc : def.lineComments) {
            if (line.mid(i, lc.size()) == lc) {
                add(i, n - i, TokenRole::Comment);
                return out;
            }
        }
        if (!def.blockStart.isEmpty() && line.mid(i, def.blockStart.size()) == def.blockStart) {
            st.inBlockComment = true;
            const int end = static_cast<int>(line.indexOf(def.blockEnd, i + static_cast<int>(def.blockStart.size())));
            if (end < 0) {
                add(i, n - i, TokenRole::Comment);
                st.inBlockComment = true;
                return out;
            }
            add(i, end + static_cast<int>(def.blockEnd.size()) - i, TokenRole::Comment);
            i = end + static_cast<int>(def.blockEnd.size());
            st.inBlockComment = false;
            continue;
        }

        if (def.quotes.contains(c)) {  // chuỗi: tới dấu đóng không bị escape, hoặc hết dòng
            int j = i + 1;
            while (j < n) {
                if (line.at(j) == QLatin1Char('\\')) {
                    j += 2;
                    continue;
                }
                if (line.at(j) == c)
                    break;
                ++j;
            }
            const int end = std::min(j + 1, n);
            add(i, end - i, TokenRole::String);
            i = end;
            continue;
        }

        if (def.dollarVariables && c == QLatin1Char('$') && i + 1 < n
            && (isIdentStart(line.at(i + 1)) || line.at(i + 1) == QLatin1Char('{'))) {
            int j = i + 1;
            if (line.at(j) == QLatin1Char('{')) {
                const int close = static_cast<int>(line.indexOf(QLatin1Char('}'), j));
                j = close < 0 ? n : close + 1;
            } else {
                while (j < n && isIdentChar(line.at(j)))
                    ++j;
            }
            add(i, j - i, TokenRole::Type);
            i = j;
            continue;
        }

        if (def.atWords && c == QLatin1Char('@') && i + 1 < n && isIdentStart(line.at(i + 1))) {
            int j = i + 1;
            while (j < n && isIdentChar(line.at(j)))
                ++j;
            add(i, j - i, TokenRole::Keyword);
            i = j;
            continue;
        }

        if (c.isDigit() || (c == QLatin1Char('.') && i + 1 < n && line.at(i + 1).isDigit())) {
            int j = i + 1;
            while (j < n && (line.at(j).isLetterOrNumber() || line.at(j) == QLatin1Char('.') || line.at(j) == QLatin1Char('_')))
                ++j;
            add(i, j - i, TokenRole::Number);
            i = j;
            continue;
        }

        if (isIdentStart(c)) {
            int j = i + 1;
            while (j < n && isIdentChar(line.at(j)))
                ++j;
            const QString word = line.mid(i, j - i);
            const QString key = def.caseInsensitive ? word.toLower() : word;
            if (def.keywords.contains(key))
                add(i, j - i, TokenRole::Keyword);
            else if (def.types.contains(key)
                     || (def.capitalizedAreTypes && word.size() > 1 && word.at(0).isUpper()
                         && !word.isUpper())  // IMouseClickSource, RowIndexPath; bỏ qua hằng số VIẾT_HOA
                     || (lang == Language::CLike && word.endsWith(QStringLiteral("_t"))))
                add(i, j - i, TokenRole::Type);
            i = j;
            matched = true;
        }
        if (!matched)
            ++i;
    }
    return out;
}

} // namespace gitapp
