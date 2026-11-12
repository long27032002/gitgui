#pragma once

#include "SyntaxHighlighter.h"

#include <GitDiff.h>

#include <QAbstractListModel>
#include <QStringList>

namespace gitapp {

class Theme;

// Các dòng của diff đang hiển thị, kèm trạng thái chọn từng dòng/hunk.
class DiffModel : public QAbstractListModel {
    Q_OBJECT
public:
    // Giá trị số này được QML dùng trực tiếp (role "kind").
    enum RowKind { Info = 0, Hunk = 1, Add = 2, Remove = 3, Context = 4 };
    Q_ENUM(RowKind)

    enum Role {
        TextRole = Qt::UserRole + 1,
        KindRole,
        SelectableRole,   // hiện checkbox được không
        CheckStateRole,   // 0 chưa chọn, 1 chọn một phần (chỉ hunk), 2 đã chọn
        OldNumberRole,    // số dòng ở bản cũ (0 = không có)
        NewNumberRole,    // số dòng ở bản mới (0 = không có)
        HtmlRole,         // mã đã tô màu cú pháp (StyledText); rỗng nếu không có
    };

    explicit DiffModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void clear();
    // Văn bản thường, chỉ để xem (file mới chưa theo dõi, thông báo lỗi...).
    void setPlainLines(const QStringList &lines);
    // Diff có cấu trúc. `selectable` = true cho phép chọn từng dòng/hunk; khi đó mặc định MỌI dòng
    // thay đổi đều được chọn (đưa vào commit), trừ các dòng có khoá nằm trong `excluded`.
    void setFileDiff(gitcore::FileDiff diff, bool selectable, const QSet<QByteArray> &excluded = {});

    // Đường dẫn file đang hiển thị: dùng để chọn ngôn ngữ tô màu cú pháp (đặt trước setFileDiff).
    void setPath(const QString &path);
    // Màu tô cú pháp lấy từ theme; đổi theme thì HTML được cập nhật.
    void setTheme(const Theme *theme);
    // Chọn / bỏ chọn mọi dòng thay đổi.
    void selectAll(bool on);
    // Các dòng thay đổi ứng với một hàng: dòng +/− -> chính nó; đầu khối @@ -> mọi dòng thay đổi của khối.
    [[nodiscard]] gitcore::LineSelection linesForRow(int row) const;

    [[nodiscard]] bool selectable() const { return m_selectable; }
    [[nodiscard]] int selectedCount() const { return static_cast<int>(m_selection.size()); }
    [[nodiscard]] const gitcore::FileDiff &fileDiff() const { return m_diff; }
    [[nodiscard]] const gitcore::LineSelection &selection() const { return m_selection; }

    // Bấm vào dòng thay đổi: đảo chọn dòng đó. Bấm vào hunk: chọn/bỏ chọn cả hunk.
    void toggleRow(int row);

signals:
    void selectionChanged();

private:
    struct Row {
        RowKind kind = Info;
        QString text;
        int hunk = -1;
        int line = -1;
        int oldNo = 0;
        int newNo = 0;
        QVector<Token> tokens;
        QString code;  // phần mã không kèm ký tự +/-/khoảng trắng đầu dòng
    };
    [[nodiscard]] QString htmlFor(const Row &row) const;
    [[nodiscard]] int selectedInHunk(int hunk) const;

    QVector<Row> m_rows;
    QVector<int> m_hunkChangeCount;
    gitcore::FileDiff m_diff;
    gitcore::LineSelection m_selection;
    bool m_selectable = false;
    QString m_path;
    const Theme *m_theme = nullptr;
};

} // namespace gitapp
