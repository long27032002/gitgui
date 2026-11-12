#include "GitConflict.h"

namespace gitcore {

namespace {

// Dòng dấu: đúng 7 ký tự `ch`, rồi hết dòng hoặc một dấu cách (kèm nhãn).
bool isMarker(const QByteArray &line, char ch, bool allowLabel)
{
    if (line.size() < 7)
        return false;
    for (int i = 0; i < 7; ++i)
        if (line.at(i) != ch)
            return false;
    if (line.size() == 7)
        return true;
    const char next = line.at(7);
    if (next == '\n' || next == '\r')
        return true;
    return allowLabel && next == ' ';
}

QList<QByteArray> splitKeepingEol(const QByteArray &content)
{
    QList<QByteArray> lines;
    qsizetype start = 0;
    while (start < content.size()) {
        const qsizetype nl = content.indexOf('\n', start);
        if (nl < 0) {
            lines.append(content.mid(start));
            break;
        }
        lines.append(content.mid(start, nl - start + 1));
        start = nl + 1;
    }
    return lines;
}

} // namespace

ConflictFile GitConflict::parse(const QByteArray &content)
{
    enum class State { Text, Ours, Base, Theirs };

    ConflictFile file;
    State state = State::Text;
    ConflictBlock block;
    QByteArray text;

    const auto fail = [&file] {
        file.supported = false;
        file.segments.clear();
        return file;
    };

    for (const QByteArray &line : splitKeepingEol(content)) {
        switch (state) {
        case State::Text:
            if (isMarker(line, '<', true)) {
                if (!text.isEmpty()) {
                    ConflictSegment seg;
                    seg.text = text;
                    file.segments.append(seg);
                    text.clear();
                }
                block = {};
                block.startMarker = line;
                state = State::Ours;
            } else {
                text += line;
            }
            break;
        case State::Ours:
            if (isMarker(line, '<', true))
                return fail();  // lồng nhau
            if (isMarker(line, '|', true)) {
                block.baseMarker = line;
                block.hasBase = true;
                state = State::Base;
            } else if (isMarker(line, '=', false)) {
                block.midMarker = line;
                state = State::Theirs;
            } else {
                block.ours += line;
            }
            break;
        case State::Base:
            if (isMarker(line, '<', true))
                return fail();
            if (isMarker(line, '=', false)) {
                block.midMarker = line;
                state = State::Theirs;
            } else {
                block.base += line;
            }
            break;
        case State::Theirs:
            if (isMarker(line, '<', true))
                return fail();
            if (isMarker(line, '>', true)) {
                block.endMarker = line;
                ConflictSegment seg;
                seg.isConflict = true;
                seg.block = block;
                file.segments.append(seg);
                state = State::Text;
            } else {
                block.theirs += line;
            }
            break;
        }
    }
    if (state != State::Text)
        return fail();  // thiếu dấu kết thúc
    if (!text.isEmpty()) {
        ConflictSegment seg;
        seg.text = text;
        file.segments.append(seg);
    }
    return file;
}

QByteArray GitConflict::resolve(const ConflictFile &file,
                                const QVector<ConflictResolution> &resolutions)
{
    QByteArray out;
    int idx = 0;
    for (const ConflictSegment &seg : file.segments) {
        if (!seg.isConflict) {
            out += seg.text;
            continue;
        }
        const ConflictBlock &b = seg.block;
        const ConflictResolution r = idx < resolutions.size() ? resolutions.at(idx)
                                                              : ConflictResolution::Unresolved;
        ++idx;
        switch (r) {
        case ConflictResolution::Ours:
            out += b.ours;
            break;
        case ConflictResolution::Theirs:
            out += b.theirs;
            break;
        case ConflictResolution::Both:
            out += b.ours + b.theirs;
            break;
        case ConflictResolution::BothReversed:
            out += b.theirs + b.ours;
            break;
        case ConflictResolution::Unresolved:
            out += b.startMarker + b.ours;
            if (b.hasBase)
                out += b.baseMarker + b.base;
            out += b.midMarker + b.theirs + b.endMarker;
            break;
        }
    }
    return out;
}

QString GitConflict::markerLabel(const QByteArray &markerLine)
{
    if (markerLine.size() <= 8)
        return {};
    return QString::fromUtf8(markerLine.mid(8)).trimmed();
}

} // namespace gitcore
