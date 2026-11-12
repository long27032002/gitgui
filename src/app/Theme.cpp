#include "Theme.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QPalette>

namespace gitapp {

namespace {
// Thứ tự phải khớp khai báo trường của Theme::Colors.
}

Theme::Theme(QObject *parent) : QObject(parent) {}

const Theme::Colors &Theme::current() const
{
    static const Colors kDark{
        QColor(QStringLiteral("#24292e")),
        QColor(QStringLiteral("#24292e")),
        QColor(QStringLiteral("#2f363d")),
        QColor(QStringLiteral("#1f2428")),
        QColor(QStringLiteral("#1b1f23")),
        QColor(QStringLiteral("#24292e")),
        QColor(QStringLiteral("#3a424b")),
        QColor(QStringLiteral("#2b3138")),
        QColor(QStringLiteral("#24292e")),
        QColor(QStringLiteral("#2b3137")),
        QColor(QStringLiteral("#f6f8fa")),
        QColor(QStringLiteral("#959da5")),
        QColor(QStringLiteral("#ffffff")),
        QColor(QStringLiteral("#79b8ff")),
        QColor(QStringLiteral("#0366d6")),
        QColor(QStringLiteral("#1a74e0")),
        QColor(QStringLiteral("#24292e")),
        QColor(QStringLiteral("#586069")),
        QColor(QStringLiteral("#3a424b")),
        QColor(QStringLiteral("#0f3a1c")),
        QColor(QStringLiteral("#4a1a1f")),
        QColor(QStringLiteral("#2b3137")),
        QColor(QStringLiteral("#6a737d")),
        QColor(QStringLiteral("#1d5a2c")),
        QColor(QStringLiteral("#6e2a30")),
        QColor(QStringLiteral("#0a58b5")),
        QColor(QStringLiteral("#ffffff")),
        QColor(QStringLiteral("#2b3137")),
        QColor(QStringLiteral("#e3b341")),
        QColor(QStringLiteral("#3fb950")),
        QColor(QStringLiteral("#f85149")),
        QColor(QStringLiteral("#58a6ff")),
        QColor(QStringLiteral("#f0883e")),
        QColor(QStringLiteral("#d99a2b")),
        QColor(QStringLiteral("#d64545")),
        QColor(QStringLiteral("#f97583")),
        QColor(QStringLiteral("#b392f0")),
        QColor(QStringLiteral("#9ecbff")),
        QColor(QStringLiteral("#79b8ff")),
        QColor(QStringLiteral("#6a737d"))
    };
    static const Colors kLight{
        QColor(QStringLiteral("#ffffff")),
        QColor(QStringLiteral("#fafbfc")),
        QColor(QStringLiteral("#eef0f2")),
        QColor(QStringLiteral("#e1e4e8")),
        QColor(QStringLiteral("#e1e4e8")),
        QColor(QStringLiteral("#ffffff")),
        QColor(QStringLiteral("#d7e8fb")),
        QColor(QStringLiteral("#f3f5f7")),
        QColor(QStringLiteral("#ffffff")),
        QColor(QStringLiteral("#f6f8fa")),
        QColor(QStringLiteral("#24292e")),
        QColor(QStringLiteral("#6a737d")),
        QColor(QStringLiteral("#ffffff")),
        QColor(QStringLiteral("#0366d6")),
        QColor(QStringLiteral("#0366d6")),
        QColor(QStringLiteral("#0256b9")),
        QColor(QStringLiteral("#ffffff")),
        QColor(QStringLiteral("#c8ccd0")),
        QColor(QStringLiteral("#e1e4e8")),
        QColor(QStringLiteral("#e6ffed")),
        QColor(QStringLiteral("#ffeef0")),
        QColor(QStringLiteral("#f6f8fa")),
        QColor(QStringLiteral("#959da5")),
        QColor(QStringLiteral("#cdffd8")),
        QColor(QStringLiteral("#ffdce0")),
        QColor(QStringLiteral("#0366d6")),
        QColor(QStringLiteral("#ffffff")),
        QColor(QStringLiteral("#f1f8ff")),
        QColor(QStringLiteral("#b08800")),
        QColor(QStringLiteral("#28a745")),
        QColor(QStringLiteral("#d73a49")),
        QColor(QStringLiteral("#0366d6")),
        QColor(QStringLiteral("#e36209")),
        QColor(QStringLiteral("#b08800")),
        QColor(QStringLiteral("#cb2431")),
        QColor(QStringLiteral("#d73a49")),
        QColor(QStringLiteral("#6f42c1")),
        QColor(QStringLiteral("#032f62")),
        QColor(QStringLiteral("#005cc5")),
        QColor(QStringLiteral("#6a737d"))
    };
    return m_dark ? kDark : kLight;
}

void Theme::setDark(bool dark)
{
    if (m_dark == dark)
        return;
    m_dark = dark;
    applyPalette();
    emit changed();
}

void Theme::applyPalette() const
{
    // `qGuiApp` chỉ ép kiểu con trỏ nên không đủ để biết có phải QGuiApplication (test chạy bằng QCoreApplication).
    if (!qobject_cast<QGuiApplication *>(QCoreApplication::instance()))
        return;
    const Colors &c = current();
    QPalette p;
    p.setColor(QPalette::Window, c.window);
    p.setColor(QPalette::WindowText, c.text);
    p.setColor(QPalette::Base, c.input);
    p.setColor(QPalette::AlternateBase, c.rowHover);
    p.setColor(QPalette::Text, c.text);
    p.setColor(QPalette::Button, c.toolbarHover);
    p.setColor(QPalette::ButtonText, c.text);
    p.setColor(QPalette::BrightText, c.onAccent);
    p.setColor(QPalette::Highlight, c.button);
    p.setColor(QPalette::HighlightedText, c.onAccent);
    p.setColor(QPalette::PlaceholderText, c.textDim);
    p.setColor(QPalette::ToolTipBase, c.expander);
    p.setColor(QPalette::ToolTipText, c.text);
    p.setColor(QPalette::Link, c.accent);
    p.setColor(QPalette::Light, c.border);
    p.setColor(QPalette::Midlight, c.border);
    p.setColor(QPalette::Mid, c.inputBorder);
    p.setColor(QPalette::Dark, c.border);
    p.setColor(QPalette::Shadow, c.border);
    p.setColor(QPalette::Disabled, QPalette::Text, c.textDim);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, c.textDim);
    p.setColor(QPalette::Disabled, QPalette::WindowText, c.textDim);
    QGuiApplication::setPalette(p);
}

} // namespace gitapp
