#pragma once

#include <QColor>
#include <QObject>

namespace gitapp {

// Bảng màu giao diện (tối/sáng) theo phong cách GitHub Desktop; QML đọc qua context property `theme`.
class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool dark READ dark WRITE setDark NOTIFY changed)
    Q_PROPERTY(QColor window READ window NOTIFY changed)
    Q_PROPERTY(QColor toolbar READ toolbar NOTIFY changed)
    Q_PROPERTY(QColor toolbarHover READ toolbarHover NOTIFY changed)
    Q_PROPERTY(QColor toolbarPressed READ toolbarPressed NOTIFY changed)
    Q_PROPERTY(QColor border READ border NOTIFY changed)
    Q_PROPERTY(QColor sidebar READ sidebar NOTIFY changed)
    Q_PROPERTY(QColor rowSelected READ rowSelected NOTIFY changed)
    Q_PROPERTY(QColor rowHover READ rowHover NOTIFY changed)
    Q_PROPERTY(QColor content READ content NOTIFY changed)
    Q_PROPERTY(QColor expander READ expander NOTIFY changed)
    Q_PROPERTY(QColor text READ text NOTIFY changed)
    Q_PROPERTY(QColor textDim READ textDim NOTIFY changed)
    Q_PROPERTY(QColor onAccent READ onAccent NOTIFY changed)
    Q_PROPERTY(QColor accent READ accent NOTIFY changed)
    Q_PROPERTY(QColor button READ button NOTIFY changed)
    Q_PROPERTY(QColor buttonHover READ buttonHover NOTIFY changed)
    Q_PROPERTY(QColor input READ input NOTIFY changed)
    Q_PROPERTY(QColor inputBorder READ inputBorder NOTIFY changed)
    Q_PROPERTY(QColor badge READ badge NOTIFY changed)
    Q_PROPERTY(QColor diffAdd READ diffAdd NOTIFY changed)
    Q_PROPERTY(QColor diffDel READ diffDel NOTIFY changed)
    Q_PROPERTY(QColor gutter READ gutter NOTIFY changed)
    Q_PROPERTY(QColor gutterText READ gutterText NOTIFY changed)
    Q_PROPERTY(QColor gutterAdd READ gutterAdd NOTIFY changed)
    Q_PROPERTY(QColor gutterDel READ gutterDel NOTIFY changed)
    Q_PROPERTY(QColor gutterSelected READ gutterSelected NOTIFY changed)
    Q_PROPERTY(QColor gutterSelectedText READ gutterSelectedText NOTIFY changed)
    Q_PROPERTY(QColor hunk READ hunk NOTIFY changed)
    Q_PROPERTY(QColor modified READ modified NOTIFY changed)
    Q_PROPERTY(QColor added READ added NOTIFY changed)
    Q_PROPERTY(QColor deleted READ deleted NOTIFY changed)
    Q_PROPERTY(QColor renamed READ renamed NOTIFY changed)
    Q_PROPERTY(QColor conflict READ conflict NOTIFY changed)
    Q_PROPERTY(QColor warning READ warning NOTIFY changed)
    Q_PROPERTY(QColor danger READ danger NOTIFY changed)
    Q_PROPERTY(QColor synKeyword READ synKeyword NOTIFY changed)
    Q_PROPERTY(QColor synType READ synType NOTIFY changed)
    Q_PROPERTY(QColor synString READ synString NOTIFY changed)
    Q_PROPERTY(QColor synNumber READ synNumber NOTIFY changed)
    Q_PROPERTY(QColor synComment READ synComment NOTIFY changed)

public:
    explicit Theme(QObject *parent = nullptr);

    [[nodiscard]] bool dark() const { return m_dark; }
    // Đồng thời đặt QPalette của ứng dụng để các control mặc định (ô nhập, menu...) cùng tông màu.
    void setDark(bool dark);
    Q_INVOKABLE void toggle() { setDark(!m_dark); }
    // Áp bảng màu hiện tại lên QPalette của ứng dụng (gọi một lần lúc khởi động).
    void apply() const { applyPalette(); }

    [[nodiscard]] QColor window() const { return current().window; }
    [[nodiscard]] QColor toolbar() const { return current().toolbar; }
    [[nodiscard]] QColor toolbarHover() const { return current().toolbarHover; }
    [[nodiscard]] QColor toolbarPressed() const { return current().toolbarPressed; }
    [[nodiscard]] QColor border() const { return current().border; }
    [[nodiscard]] QColor sidebar() const { return current().sidebar; }
    [[nodiscard]] QColor rowSelected() const { return current().rowSelected; }
    [[nodiscard]] QColor rowHover() const { return current().rowHover; }
    [[nodiscard]] QColor content() const { return current().content; }
    [[nodiscard]] QColor expander() const { return current().expander; }
    [[nodiscard]] QColor text() const { return current().text; }
    [[nodiscard]] QColor textDim() const { return current().textDim; }
    [[nodiscard]] QColor onAccent() const { return current().onAccent; }
    [[nodiscard]] QColor accent() const { return current().accent; }
    [[nodiscard]] QColor button() const { return current().button; }
    [[nodiscard]] QColor buttonHover() const { return current().buttonHover; }
    [[nodiscard]] QColor input() const { return current().input; }
    [[nodiscard]] QColor inputBorder() const { return current().inputBorder; }
    [[nodiscard]] QColor badge() const { return current().badge; }
    [[nodiscard]] QColor diffAdd() const { return current().diffAdd; }
    [[nodiscard]] QColor diffDel() const { return current().diffDel; }
    [[nodiscard]] QColor gutter() const { return current().gutter; }
    [[nodiscard]] QColor gutterText() const { return current().gutterText; }
    [[nodiscard]] QColor gutterAdd() const { return current().gutterAdd; }
    [[nodiscard]] QColor gutterDel() const { return current().gutterDel; }
    [[nodiscard]] QColor gutterSelected() const { return current().gutterSelected; }
    [[nodiscard]] QColor gutterSelectedText() const { return current().gutterSelectedText; }
    [[nodiscard]] QColor hunk() const { return current().hunk; }
    [[nodiscard]] QColor modified() const { return current().modified; }
    [[nodiscard]] QColor added() const { return current().added; }
    [[nodiscard]] QColor deleted() const { return current().deleted; }
    [[nodiscard]] QColor renamed() const { return current().renamed; }
    [[nodiscard]] QColor conflict() const { return current().conflict; }
    [[nodiscard]] QColor warning() const { return current().warning; }
    [[nodiscard]] QColor danger() const { return current().danger; }
    [[nodiscard]] QColor synKeyword() const { return current().synKeyword; }
    [[nodiscard]] QColor synType() const { return current().synType; }
    [[nodiscard]] QColor synString() const { return current().synString; }
    [[nodiscard]] QColor synNumber() const { return current().synNumber; }
    [[nodiscard]] QColor synComment() const { return current().synComment; }

signals:
    void changed();

private:
    struct Colors {
        QColor window;
        QColor toolbar;
        QColor toolbarHover;
        QColor toolbarPressed;
        QColor border;
        QColor sidebar;
        QColor rowSelected;
        QColor rowHover;
        QColor content;
        QColor expander;
        QColor text;
        QColor textDim;
        QColor onAccent;
        QColor accent;
        QColor button;
        QColor buttonHover;
        QColor input;
        QColor inputBorder;
        QColor badge;
        QColor diffAdd;
        QColor diffDel;
        QColor gutter;
        QColor gutterText;
        QColor gutterAdd;
        QColor gutterDel;
        QColor gutterSelected;
        QColor gutterSelectedText;
        QColor hunk;
        QColor modified;
        QColor added;
        QColor deleted;
        QColor renamed;
        QColor conflict;
        QColor warning;
        QColor danger;
        QColor synKeyword;
        QColor synType;
        QColor synString;
        QColor synNumber;
        QColor synComment;
    };
    [[nodiscard]] const Colors &current() const;
    void applyPalette() const;

    bool m_dark = true;
};

} // namespace gitapp
