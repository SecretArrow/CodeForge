#include "project/RecentManager.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

#include "core/AppPaths.h"

namespace cf {

RecentManager::RecentManager(QObject* parent) : QObject(parent) {}

void RecentManager::load()
{
    m_projects.clear();
    m_files.clear();
    QFile f(paths::recentsFile());
    if (!f.open(QIODevice::ReadOnly)) return;
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    m_projects = fromJsonArray(root.value(QStringLiteral("projects")).toArray());
    m_files = fromJsonArray(root.value(QStringLiteral("files")).toArray());
}

void RecentManager::save()
{
    QJsonObject root;
    root.insert(QStringLiteral("projects"), toJsonArray(m_projects));
    root.insert(QStringLiteral("files"), toJsonArray(m_files));
    QFile f(paths::recentsFile());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

QVector<RecentManager::Entry> RecentManager::fromJsonArray(const QJsonArray& arr)
{
    QVector<Entry> out;
    for (const QJsonValue& v : arr) {
        const QJsonObject o = v.toObject();
        Entry e;
        e.path = o.value(QStringLiteral("path")).toString();
        e.pinned = o.value(QStringLiteral("pinned")).toBool();
        e.lastOpened = o.value(QStringLiteral("ts")).toVariant().toLongLong();
        if (!e.path.isEmpty()) out.append(e);
    }
    return out;
}

QJsonArray RecentManager::toJsonArray(const QVector<Entry>& list)
{
    QJsonArray arr;
    for (const Entry& e : list) {
        QJsonObject o;
        o.insert(QStringLiteral("path"), e.path);
        o.insert(QStringLiteral("pinned"), e.pinned);
        o.insert(QStringLiteral("ts"), QString::number(e.lastOpened));
        arr.append(o);
    }
    return arr;
}

void RecentManager::touch(QVector<Entry>& list, const QString& path, int max)
{
    const QString norm = QDir::fromNativeSeparators(path);
    for (Entry& e : list)
        if (e.path == norm) { e.lastOpened = QDateTime::currentMSecsSinceEpoch(); sortList(list); return; }
    Entry e;
    e.path = norm;
    e.lastOpened = QDateTime::currentMSecsSinceEpoch();
    list.prepend(e);
    sortList(list);
    // Prune non-pinned oldest entries.
    while (list.size() > max) {
        int victim = -1;
        for (int i = list.size() - 1; i >= 0; --i)
            if (!list.at(i).pinned) { victim = i; break; }
        if (victim < 0) break;
        list.remove(victim);
    }
}

void RecentManager::sortList(QVector<Entry>& list)
{
    std::stable_sort(list.begin(), list.end(), [](const Entry& a, const Entry& b) {
        if (a.pinned != b.pinned) return a.pinned;      // pinned first
        return a.lastOpened > b.lastOpened;
    });
}

void RecentManager::addProject(const QString& path) { touch(m_projects, path, kMaxProjects); save(); }
void RecentManager::addFile(const QString& path)    { touch(m_files, path, kMaxFiles); save(); }

void RecentManager::removeProject(const QString& path)
{
    const QString norm = QDir::fromNativeSeparators(path);
    m_projects.erase(std::remove_if(m_projects.begin(), m_projects.end(),
                                    [&](const Entry& e) { return e.path == norm; }),
                     m_projects.end());
    save();
}

void RecentManager::clearProjects(bool keepPinned)
{
    if (!keepPinned) { m_projects.clear(); }
    else {
        m_projects.erase(std::remove_if(m_projects.begin(), m_projects.end(),
                                        [](const Entry& e) { return !e.pinned; }),
                         m_projects.end());
    }
    save();
}

void RecentManager::setPinned(const QString& path, bool pinned)
{
    const QString norm = QDir::fromNativeSeparators(path);
    for (Entry& e : m_projects)
        if (e.path == norm) { e.pinned = pinned; break; }
    sortList(m_projects);
    save();
}

QVector<RecentManager::Entry> RecentManager::projects() const { return m_projects; }
QVector<RecentManager::Entry> RecentManager::recentFiles() const { return m_files; }

}  // namespace cf
