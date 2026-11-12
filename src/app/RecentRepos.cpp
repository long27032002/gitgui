#include "RecentRepos.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>

#include <memory>

namespace gitapp {

namespace {
std::unique_ptr<QSettings> makeSettings(const QString &iniFile)
{
    if (iniFile.isEmpty())
        return std::make_unique<QSettings>();
    return std::make_unique<QSettings>(iniFile, QSettings::IniFormat);
}
} // namespace

RecentRepos::RecentRepos(QObject *parent) : QAbstractListModel(parent) {}

int RecentRepos::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_paths.size());
}

QHash<int, QByteArray> RecentRepos::roleNames() const
{
    return {{PathRole, "path"}, {NameRole, "name"}, {ParentRole, "parentDir"}};
}

QVariant RecentRepos::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_paths.size())
        return {};
    const QString &p = m_paths.at(index.row());
    switch (role) {
    case PathRole:
        return p;
    case NameRole:
    case Qt::DisplayRole:
        return QFileInfo(p).fileName();
    case ParentRole:
        return QFileInfo(p).absolutePath();
    default:
        return {};
    }
}

void RecentRepos::enablePersistence(const QString &iniFile)
{
    m_persistent = true;
    m_iniFile = iniFile;
    const QStringList saved = makeSettings(m_iniFile)->value(QStringLiteral("recentRepos")).toStringList();
    beginResetModel();
    m_paths.clear();
    for (const QString &p : saved)
        if (!p.isEmpty() && !m_paths.contains(p) && m_paths.size() < kMax)
            m_paths.append(p);
    endResetModel();
}

void RecentRepos::touch(const QString &path)
{
    if (path.isEmpty())
        return;
    const int existing = static_cast<int>(m_paths.indexOf(path));
    if (existing == 0)
        return;
    if (existing > 0) {
        beginMoveRows({}, existing, existing, {}, 0);
        m_paths.move(existing, 0);
        endMoveRows();
    } else {
        beginInsertRows({}, 0, 0);
        m_paths.prepend(path);
        endInsertRows();
        if (m_paths.size() > kMax) {
            beginRemoveRows({}, kMax, static_cast<int>(m_paths.size()) - 1);
            m_paths = m_paths.mid(0, kMax);
            endRemoveRows();
        }
    }
    save();
}

void RecentRepos::remove(const QString &path)
{
    const int row = static_cast<int>(m_paths.indexOf(path));
    if (row < 0)
        return;
    beginRemoveRows({}, row, row);
    m_paths.removeAt(row);
    endRemoveRows();
    save();
}

void RecentRepos::save() const
{
    if (!m_persistent)
        return;
    auto settings = makeSettings(m_iniFile);
    settings->setValue(QStringLiteral("recentRepos"), m_paths);
    settings->sync();
}

} // namespace gitapp
