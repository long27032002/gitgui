#include "GitParser.h"

#include <QList>

namespace gitcore {

namespace {

// Cắt theo dấu cách, tối đa `parts` phần (phần cuối giữ nguyên, có thể chứa khoảng trắng).
QStringList splitLimited(const QString &s, int parts)
{
    QStringList out;
    int start = 0;
    for (int i = 1; i < parts; ++i) {
        const int idx = s.indexOf(QLatin1Char(' '), start);
        if (idx < 0)
            break;
        out << s.mid(start, idx - start);
        start = idx + 1;
    }
    out << s.mid(start);
    return out;
}

void parseHeader(const QString &line, RepoStatus &st)
{
    // "# branch.oid <oid|(initial)>", "# branch.head <name|(detached)>",
    // "# branch.upstream <name>", "# branch.ab +<a> -<b>"
    if (line.startsWith(QStringLiteral("# branch.oid "))) {
        const QString v = line.mid(13);
        st.oid = (v == QStringLiteral("(initial)")) ? QString() : v;
    } else if (line.startsWith(QStringLiteral("# branch.head "))) {
        st.branch = line.mid(14);
    } else if (line.startsWith(QStringLiteral("# branch.upstream "))) {
        st.upstream = line.mid(18);
    } else if (line.startsWith(QStringLiteral("# branch.ab "))) {
        const QStringList p = line.mid(12).split(QLatin1Char(' '));
        if (p.size() == 2) {
            st.ahead = p[0].mid(1).toInt();
            st.behind = p[1].mid(1).toInt();
        }
    }
}

} // namespace

RepoStatus GitParser::parseStatusV2(const QByteArray &raw)
{
    RepoStatus st;
    const QList<QByteArray> tokens = raw.split('\0');

    for (qsizetype i = 0; i < tokens.size(); ++i) {
        const QString line = QString::fromUtf8(tokens[i]);
        if (line.isEmpty())
            continue;

        FileChange fc;
        switch (line.at(0).toLatin1()) {
        case '#':
            parseHeader(line, st);
            continue;
        case '1': {
            // 1 XY sub mH mI mW hH hI path
            const QStringList f = splitLimited(line, 9);
            if (f.size() != 9)
                continue;
            fc.kind = ChangeKind::Ordinary;
            fc.indexStatus = f[1].at(0);
            fc.worktreeStatus = f[1].at(1);
            fc.path = f[8];
            break;
        }
        case '2': {
            // 2 XY sub mH mI mW hH hI Xscore path  \0 origPath
            const QStringList f = splitLimited(line, 10);
            if (f.size() != 10)
                continue;
            fc.kind = ChangeKind::Renamed;
            fc.indexStatus = f[1].at(0);
            fc.worktreeStatus = f[1].at(1);
            fc.path = f[9];
            if (i + 1 < tokens.size())
                fc.origPath = QString::fromUtf8(tokens[++i]);
            break;
        }
        case 'u': {
            // u XY sub m1 m2 m3 mW h1 h2 h3 path
            const QStringList f = splitLimited(line, 11);
            if (f.size() != 11)
                continue;
            fc.kind = ChangeKind::Unmerged;
            fc.indexStatus = f[1].at(0);
            fc.worktreeStatus = f[1].at(1);
            fc.path = f[10];
            break;
        }
        case '?':
            fc.kind = ChangeKind::Untracked;
            fc.indexStatus = QLatin1Char('?');
            fc.worktreeStatus = QLatin1Char('?');
            fc.path = line.mid(2);
            break;
        default: // '!' (ignored) và dòng lạ: bỏ qua
            continue;
        }
        st.changes.append(fc);
    }
    return st;
}

QVector<BranchInfo> GitParser::parseBranches(const QByteArray &raw)
{
    QVector<BranchInfo> out;
    const QStringList lines = QString::fromUtf8(raw).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QStringList f = line.split(QLatin1Char('\t'));
        if (f.size() < 4)
            continue;
        const QString &refname = f[0];
        if (refname.endsWith(QStringLiteral("/HEAD")))
            continue;  // origin/HEAD chỉ là con trỏ, không phải nhánh để chuyển tới
        BranchInfo b;
        b.isRemote = refname.startsWith(QStringLiteral("refs/remotes/"));
        b.name = f[1];
        b.upstream = f[2];
        b.isCurrent = f[3] == QStringLiteral("*");
        out.append(b);
    }
    return out;
}

QVector<CommitInfo> GitParser::parseLog(const QByteArray &raw)
{
    QVector<CommitInfo> out;
    const QList<QByteArray> records = raw.split('\x1e');
    for (const QByteArray &rec : records) {
        const QString text = QString::fromUtf8(rec).trimmed();
        if (text.isEmpty())
            continue;
        const QStringList f = QString::fromUtf8(rec).remove(QLatin1Char('\n')).split(QChar(0x1f));
        if (f.size() < 7)
            continue;
        CommitInfo c;
        c.hash = f[0];
        c.shortHash = f[1];
        c.author = f[2];
        c.timestamp = f[3].toLongLong();
        const QStringList parents = f[4].split(QLatin1Char(' '), Qt::SkipEmptyParts);
        c.parentCount = static_cast<int>(parents.size());
        c.parents = parents;
        c.firstParent = parents.value(0);
        c.subject = f[5];
        c.refs = f[6];
        out.append(c);
    }
    return out;
}

QVector<CommitFile> GitParser::parseNameStatus(const QByteArray &raw)
{
    QVector<CommitFile> out;
    QList<QByteArray> tokens = raw.split('\0');
    for (qsizetype i = 0; i < tokens.size(); ++i) {
        const QString status = QString::fromUtf8(tokens[i]).trimmed();
        if (status.isEmpty())
            continue;
        CommitFile cf;
        cf.status = status.at(0);
        const bool twoPaths = cf.status == QLatin1Char('R') || cf.status == QLatin1Char('C');
        if (i + 1 >= tokens.size())
            break;
        if (twoPaths) {
            if (i + 2 >= tokens.size())
                break;
            cf.origPath = QString::fromUtf8(tokens[++i]);
            cf.path = QString::fromUtf8(tokens[++i]);
        } else {
            cf.path = QString::fromUtf8(tokens[++i]);
        }
        out.append(cf);
    }
    return out;
}

} // namespace gitcore
