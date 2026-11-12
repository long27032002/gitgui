#include "DiffModel.h"

#include "Theme.h"

using namespace gitcore;

namespace gitapp {

DiffModel::DiffModel(QObject *parent) : QAbstractListModel(parent) {}

int DiffModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QHash<int, QByteArray> DiffModel::roleNames() const
{
    return {
        {TextRole, "text"},
        {KindRole, "kind"},
        {SelectableRole, "selectable"},
        {CheckStateRole, "checkState"},
        {OldNumberRole, "oldNumber"},
        {NewNumberRole, "newNumber"},
        {HtmlRole, "html"},
    };
}

QVariant DiffModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const Row &r = m_rows.at(index.row());
    switch (role) {
    case TextRole:
    case Qt::DisplayRole:
        return r.text;
    case KindRole:
        return r.kind;
    case SelectableRole:
        if (!m_selectable)
            return false;
        if (r.kind == Hunk)
            return m_hunkChangeCount.value(r.hunk) > 0;
        return r.kind == Add || r.kind == Remove;
    case CheckStateRole:
        if (r.kind == Add || r.kind == Remove)
            return m_selection.contains(lineKey(r.hunk, r.line)) ? 2 : 0;
        if (r.kind == Hunk) {
            const int total = m_hunkChangeCount.value(r.hunk);
            const int sel = selectedInHunk(r.hunk);
            return sel == 0 ? 0 : (sel == total ? 2 : 1);
        }
        return 0;
    case OldNumberRole:
        return r.oldNo;
    case NewNumberRole:
        return r.newNo;
    case HtmlRole:
        return htmlFor(r);
    default:
        return {};
    }
}

void DiffModel::clear()
{
    beginResetModel();
    m_rows.clear();
    m_hunkChangeCount.clear();
    m_diff = {};
    m_selection.clear();
    m_selectable = false;
    endResetModel();
    emit selectionChanged();
}

void DiffModel::setPlainLines(const QStringList &lines)
{
    beginResetModel();
    m_rows.clear();
    m_hunkChangeCount.clear();
    m_diff = {};
    m_selection.clear();
    m_selectable = false;
    for (const QString &l : lines) {
        Row r;
        r.text = l;
        const QChar c = l.isEmpty() ? QLatin1Char(' ') : l.at(0);
        r.kind = c == QLatin1Char('+')   ? Add
                 : c == QLatin1Char('-') ? Remove
                 : c == QLatin1Char('@') ? Hunk
                 : c == QLatin1Char(' ') ? Context
                                         : Info;
        m_rows.append(r);
    }
    endResetModel();
    emit selectionChanged();
}

void DiffModel::setFileDiff(FileDiff diff, bool selectable, const QSet<QByteArray> &excluded)
{
    beginResetModel();
    m_rows.clear();
    m_hunkChangeCount.clear();
    m_diff = std::move(diff);
    m_selectable = selectable;
    m_selection = selectable ? GitDiff::includedLines(m_diff, excluded) : LineSelection{};

    const auto lang = SyntaxHighlighter::languageForPath(m_path);
    for (int h = 0; h < m_diff.hunks.size(); ++h) {
        const DiffHunk &hunk = m_diff.hunks.at(h);
        Row head;
        head.kind = Hunk;
        head.text = QString::fromUtf8(hunk.header);
        head.hunk = h;
        m_rows.append(head);

        int changes = 0;
        int oldNo = hunk.oldStart;
        int newNo = hunk.newStart;
        // Hai trạng thái chú thích khối: một cho bản cũ, một cho bản mới (đặt lại ở mỗi khối @@).
        SyntaxHighlighter::State oldState, newState;
        for (int l = 0; l < hunk.lines.size(); ++l) {
            const DiffLine &dl = hunk.lines.at(l);
            Row r;
            r.text = QString::fromUtf8(dl.raw);  // chỉ để hiển thị; patch dùng byte thô
            r.hunk = h;
            r.line = l;
            switch (dl.kind) {
            case DiffLineKind::Add:
                r.kind = Add;
                r.newNo = newNo++;
                ++changes;
                break;
            case DiffLineKind::Remove:
                r.kind = Remove;
                r.oldNo = oldNo++;
                ++changes;
                break;
            case DiffLineKind::Context:
                r.kind = Context;
                r.oldNo = oldNo++;
                r.newNo = newNo++;
                break;
            case DiffLineKind::NoNewline:
                r.kind = Info;
                break;
            }
            if (r.kind == Add || r.kind == Remove || r.kind == Context) {
                r.code = r.text.size() > 0 ? r.text.mid(1) : QString();
                if (lang != SyntaxHighlighter::Language::None) {
                    SyntaxHighlighter::State &st = r.kind == Remove ? oldState : newState;
                    r.tokens = SyntaxHighlighter::highlightLine(lang, r.code, &st);
                    if (r.kind == Context)
                        oldState = newState;  // dòng ngữ cảnh giống nhau ở cả hai phía
                }
            }
            m_rows.append(r);
        }
        m_hunkChangeCount.append(changes);
    }
    endResetModel();
    emit selectionChanged();
}

void DiffModel::setPath(const QString &path)
{
    m_path = path;
}

void DiffModel::setTheme(const Theme *theme)
{
    m_theme = theme;
    if (theme)
        connect(theme, &Theme::changed, this, [this] {
            if (!m_rows.isEmpty())
                emit dataChanged(index(0), index(static_cast<int>(m_rows.size()) - 1), {HtmlRole});
        });
}

void DiffModel::selectAll(bool on)
{
    if (!m_selectable)
        return;
    const LineSelection before = m_selection;
    m_selection = on ? GitDiff::includedLines(m_diff, {}) : LineSelection{};
    if (m_selection == before)
        return;
    if (!m_rows.isEmpty())
        emit dataChanged(index(0), index(static_cast<int>(m_rows.size()) - 1), {CheckStateRole});
    emit selectionChanged();
}

LineSelection DiffModel::linesForRow(int row) const
{
    LineSelection out;
    if (row < 0 || row >= m_rows.size())
        return out;
    const Row &r = m_rows.at(row);
    if (r.kind == Add || r.kind == Remove) {
        out.insert(lineKey(r.hunk, r.line));
    } else if (r.kind == Hunk && r.hunk >= 0 && r.hunk < m_diff.hunks.size()) {
        const DiffHunk &h = m_diff.hunks.at(r.hunk);
        for (int l = 0; l < h.lines.size(); ++l)
            if (h.lines.at(l).isChange())
                out.insert(lineKey(r.hunk, l));
    }
    return out;
}

QString DiffModel::htmlFor(const Row &row) const
{
    if (!m_theme || row.tokens.isEmpty())
        return {};
    const auto colorOf = [this](TokenRole role) -> QColor {
        switch (role) {
        case TokenRole::Keyword: return m_theme->synKeyword();
        case TokenRole::Type: return m_theme->synType();
        case TokenRole::String: return m_theme->synString();
        case TokenRole::Number: return m_theme->synNumber();
        case TokenRole::Comment: return m_theme->synComment();
        case TokenRole::Plain: break;
        }
        return {};
    };
    // StyledText gộp khoảng trắng liên tiếp, nên dùng &nbsp; để giữ nguyên thụt đầu dòng.
    const auto escape = [](const QString &t) {
        QString out;
        out.reserve(t.size() + 8);
        for (const QChar ch : t) {
            if (ch == QLatin1Char('&')) out += QStringLiteral("&amp;");
            else if (ch == QLatin1Char('<')) out += QStringLiteral("&lt;");
            else if (ch == QLatin1Char('>')) out += QStringLiteral("&gt;");
            else if (ch == QLatin1Char(' ')) out += QStringLiteral("&nbsp;");
            else if (ch == QLatin1Char('\t')) out += QStringLiteral("&nbsp;&nbsp;&nbsp;&nbsp;");
            else out += ch;
        }
        return out;
    };

    QString html;
    int pos = 0;
    for (const Token &t : row.tokens) {
        if (t.start < pos || t.start + t.length > row.code.size())
            continue;
        html += escape(row.code.mid(pos, t.start - pos));
        html += QStringLiteral("<font color=\"%1\">").arg(colorOf(t.role).name())
                + escape(row.code.mid(t.start, t.length)) + QStringLiteral("</font>");
        pos = t.start + t.length;
    }
    html += escape(row.code.mid(pos));
    return html;
}

int DiffModel::selectedInHunk(int hunk) const
{
    int n = 0;
    if (hunk < 0 || hunk >= m_diff.hunks.size())
        return 0;
    const DiffHunk &h = m_diff.hunks.at(hunk);
    for (int l = 0; l < h.lines.size(); ++l)
        if (h.lines.at(l).isChange() && m_selection.contains(lineKey(hunk, l)))
            ++n;
    return n;
}

void DiffModel::toggleRow(int row)
{
    if (!m_selectable || row < 0 || row >= m_rows.size())
        return;
    const Row &r = m_rows.at(row);
    if (r.kind == Add || r.kind == Remove) {
        const quint64 key = lineKey(r.hunk, r.line);
        if (!m_selection.remove(key))
            m_selection.insert(key);
    } else if (r.kind == Hunk) {
        const int total = m_hunkChangeCount.value(r.hunk);
        if (total == 0)
            return;
        const bool all = selectedInHunk(r.hunk) == total;
        const DiffHunk &h = m_diff.hunks.at(r.hunk);
        for (int l = 0; l < h.lines.size(); ++l) {
            if (!h.lines.at(l).isChange())
                continue;
            if (all)
                m_selection.remove(lineKey(r.hunk, l));
            else
                m_selection.insert(lineKey(r.hunk, l));
        }
    } else {
        return;
    }
    emit dataChanged(index(0), index(static_cast<int>(m_rows.size()) - 1), {CheckStateRole});
    emit selectionChanged();
}

} // namespace gitapp
