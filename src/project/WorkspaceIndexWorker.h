#pragma once
// Background worker that walks the workspace tree for the file index.
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QObject>
#include <QStringList>

namespace cf {

class WorkspaceIndexWorker : public QObject {
    Q_OBJECT
public:
    QStringList excludes;
    bool includeHidden = false;
    int limit = 50000;
    QString root;

public slots:
    void run()
    {
        QStringList out;
        QDirIterator it(root, QDir::Files | QDir::NoDotAndDotDot | (includeHidden ? QDir::Filters(QDir::Hidden) : QDir::Filters()),
                        QDirIterator::Subdirectories);
        while (it.hasNext() && out.size() < limit) {
            const QString abs = it.next();
            const QString rel = QDir(root).relativeFilePath(abs);
            bool excluded = false;
            for (const QString& g : excludes)
                if (QDir::match(g, rel) || QDir::match(g, QFileInfo(rel).fileName())) { excluded = true; break; }
            if (!excluded) out.append(rel);
        }
        emit done(out);
    }

signals:
    void done(const QStringList& files);
};

}  // namespace cf
