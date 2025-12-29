/**
 * ThumbnailLoadThread - Asynchronous thumbnail loading
 * 
 * Inspired by DigiKam's ThumbnailLoadThread:
 * - Background thread pool for parallel loading
 * - Task queue with priority (visible items first)
 * - Deduplication of pending requests
 * - Signals for thumbnail availability
 */

#pragma once

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QQueue>
#include <QSet>
#include <QImage>
#include <QPixmap>
#include <QThreadPool>

#include "thumbnailcreator.h"

namespace FullFrame {

/**
 * Priority levels for thumbnail loading
 */
enum class LoadPriority
{
    Low,        // Preloading off-screen items
    Normal,     // Regular visible items
    High        // Currently hovered or selected
};

/**
 * Task description for thumbnail loading
 */
struct ThumbnailTask
{
    QString filePath;
    QString cacheKey;
    int size = 256;
    LoadPriority priority = LoadPriority::Normal;
    
    bool operator==(const ThumbnailTask& other) const {
        return cacheKey == other.cacheKey;
    }
};

/**
 * Result of thumbnail loading
 */
struct ThumbnailResult
{
    QString filePath;
    QString cacheKey;
    QImage image;
    bool success = false;
};

/**
 * Worker for loading thumbnails in thread pool
 */
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

/**
 * Main thumbnail loading thread manager
 * Singleton - use instance() to access
 */
class ThumbnailLoadThread : public QObject
{
    Q_OBJECT

public:
    static ThumbnailLoadThread* instance();
    static void cleanup();

    // Request thumbnail loading
    void load(const QString& filePath, int size = 256, LoadPriority priority = LoadPriority::Normal);
    void load(const ThumbnailTask& task);
    
    // Batch loading (more efficient)
    void loadBatch(const QStringList& filePaths, int size = 256, LoadPriority priority = LoadPriority::Normal);
    
    // Preload thumbnails (low priority)
    void preload(const QStringList& filePaths, int size = 256);
    
    // Cancel pending loads for file
    void cancel(const QString& filePath);
    void cancelAll();
    
    // Check if thumbnail is ready in cache
    bool find(const QString& filePath, int size, QPixmap& pixmap);
    bool find(const QString& filePath, int size, QImage& image);
    
    // Configuration
    void setMaxThreads(int threads);
    void setThumbnailSize(int size);
    int thumbnailSize() const { return m_defaultSize; }

Q_SIGNALS:
    // Emitted when thumbnail is ready (image version - any thread)
    void thumbnailLoaded(const QString& filePath, const QImage& image);
    
    // Emitted when thumbnail is ready (pixmap version - main thread only)
    void thumbnailReady(const QString& filePath, const QPixmap& pixmap);
    
    // Emitted on load failure
    void thumbnailFailed(const QString& filePath);

private Q_SLOTS:
    void slotWorkerFinished(const ThumbnailResult& result);

private:
    explicit ThumbnailLoadThread(QObject* parent = nullptr);
    ~ThumbnailLoadThread() override;

    // Disable copy
    ThumbnailLoadThread(const ThumbnailLoadThread&) = delete;
    ThumbnailLoadThread& operator=(const ThumbnailLoadThread&) = delete;

    void scheduleTask(const ThumbnailTask& task);
    QString makeCacheKey(const QString& filePath, int size) const;

private:
    static ThumbnailLoadThread* s_instance;

    QThreadPool* m_threadPool;
    int m_defaultSize = 256;
    
    // Track pending tasks to avoid duplicates
    mutable QMutex m_pendingMutex;
    QSet<QString> m_pendingKeys;
};

} // namespace FullFrame

