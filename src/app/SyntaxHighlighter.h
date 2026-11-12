#pragma once

#include <QString>
#include <QVector>

namespace gitapp {

enum class TokenRole { Plain, Keyword, Type, String, Number, Comment };

struct Token {
    int start = 0;
    int length = 0;
    TokenRole role = TokenRole::Plain;
    bool operator==(const Token &) const = default;
};

// Tô màu cú pháp cơ bản theo từng dòng (không dựng cây cú pháp): đủ để diff dễ đọc như GitHub Desktop.
// Chỉ trả về các token KHÁC Plain; phần còn lại giữ màu chữ thường.
class SyntaxHighlighter {
public:
    enum class Language {
        None, CLike, JavaScript, Python, Json, Markdown, Shell, Rust, Go, Java, Qml, Css, CMake, Yaml, Toml
    };

    // Trạng thái mang qua các dòng liên tiếp (chú thích khối /* ... */).
    struct State {
        bool inBlockComment = false;
    };

    [[nodiscard]] static Language languageForPath(const QString &path);
    [[nodiscard]] static QVector<Token> highlightLine(Language lang, const QString &line, State *state);
};

} // namespace gitapp
