#pragma once
// Recent projects / files with pinning (pinned entries are never pruned).
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QVector>

namespace cf {

class RecentManager : public QObject {
    Q_OBJECT
public:
    struct Entry {
        QString path;
        bool pinned = false;
        qint64 lastOpened = 0;   // epoch ms
    };

    explicit RecentManager(QObject* parent = nullptr);

    void load();
    void save();

    void addProject(const QString& path);
    void addFile(const QString& path);
    void removeProject(const QString& path);
    void clearProjects(bool keepPinned = true);
    void setPinned(const QString& path, bool pinned);

    QVector<Entry> projects() const;   // pinned first, then by recency
    QVector<Entry> recentFiles() const;

private:
    void touch(QVector<Entry>& list, const QString& path, int max);
    static void sortList(QVector<Entry>& list);
    static QVector<Entry> fromJsonArray(const QJsonArray& arr);
    static QJsonArray toJsonArray(const QVector<Entry>& list);

    QVector<Entry> m_projects;
    QVector<Entry> m_files;
    static constexpr int kMaxProjects = 25;
    static constexpr int kMaxFiles = 50;
};

}  // namespace cf
