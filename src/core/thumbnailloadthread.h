#pragma once

#include <QObject>
#include <QThreadPool>
#include <QRunnable>
#include <QMutex>
#include <QSet>
#include "thumbnailcreator.h"

namespace FullFrame {

struct ThumbnailTask
{
    QString filePath;
    QString cacheKey;
    int size = 256;
};

struct ThumbnailResult
{
    QString filePath;
    QString cacheKey;
    QImage image;
    bool success = false;
};

class ThumbnailWorker : public QObject, public QRunnable
{
    Q_OBJECT
public:
    explicit ThumbnailWorker(const ThumbnailTask& task);
    void run() override;
Q_SIGNALS:
    void finished(const ThumbnailResult& result);
private:
    ThumbnailTask m_task;
};

class ThumbnailLoadThread : public QObject
{
    Q_OBJECT
public:
    static ThumbnailLoadThread* instance();
    static void cleanup();

    void load(const QString& filePath, int size = 256);
    void cancel(const QString& filePath);
    void cancelAll();
    
    static QString makeCacheKey(const QString& path, int size) {
        return QString("%1@%2").arg(path).arg(size);
    }

Q_SIGNALS:
    void thumbnailLoaded(const QString& filePath, const QImage& thumbnail);
    void thumbnailReady(const QString& filePath, const QPixmap& pixmap);

private:
    explicit ThumbnailLoadThread(QObject* parent = nullptr);
    ~ThumbnailLoadThread() override;

    void scheduleTask(const ThumbnailTask& task);
    
private Q_SLOTS:
    void slotWorkerFinished(const ThumbnailResult& result);

private:
    static ThumbnailLoadThread* s_instance;
    QThreadPool* m_threadPool;
    QMutex m_pendingMutex;
    QSet<QString> m_pendingKeys;
};

} // namespace FullFrame
