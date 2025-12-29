/**
 * ThumbnailLoadThread implementation
 * 
 * Key performance features (like DigiKam):
 * - Thread pool for parallel loading
 * - Duplicate request elimination
 * - Priority-based scheduling
 * - Automatic caching of results
 */

#include "thumbnailloadthread.h"
#include "thumbnailcache.h"
#include <QApplication>
#include <QDebug>

namespace FullFrame {

ThumbnailLoadThread* ThumbnailLoadThread::s_instance = nullptr;

// ============== ThumbnailWorker ==============

ThumbnailWorker::ThumbnailWorker(const ThumbnailTask& task)
    : m_task(task)
{
    setAutoDelete(true);
}

void ThumbnailWorker::run()
{
    ThumbnailResult result;
    result.filePath = m_task.filePath;
    result.cacheKey = m_task.cacheKey;

    // Create thumbnail
    ThumbnailCreator creator(m_task.size);
    result.image = creator.create(m_task.filePath);
    result.success = !result.image.isNull();

    // Cache the result (thread-safe image cache)
    if (result.success) {
        ThumbnailCache::instance()->putImage(result.cacheKey, result.image);
    }

    Q_EMIT finished(result);
}

// ============== ThumbnailLoadThread ==============

ThumbnailLoadThread* ThumbnailLoadThread::instance()
{
    if (!s_instance) {
        s_instance = new ThumbnailLoadThread();
    }
    return s_instance;
}

void ThumbnailLoadThread::cleanup()
{
    delete s_instance;
    s_instance = nullptr;
}

ThumbnailLoadThread::ThumbnailLoadThread(QObject* parent)
    : QObject(parent)
    , m_threadPool(new QThreadPool(this))
{
    // Set reasonable thread count (DigiKam uses similar approach)
    int idealThreads = QThread::idealThreadCount();
    m_threadPool->setMaxThreadCount(qMax(2, idealThreads - 1));
}

ThumbnailLoadThread::~ThumbnailLoadThread()
{
    cancelAll();
    m_threadPool->waitForDone();
}

void ThumbnailLoadThread::load(const QString& filePath, int size, LoadPriority priority)
{
    ThumbnailTask task;
    task.filePath = filePath;
    task.size = size;
    task.priority = priority;
    task.cacheKey = makeCacheKey(filePath, size);
    load(task);
}

void ThumbnailLoadThread::load(const ThumbnailTask& task)
{
    // Check cache first
    if (ThumbnailCache::instance()->hasImage(task.cacheKey) ||
        ThumbnailCache::instance()->hasPixmap(task.cacheKey)) {
        // Already cached, emit signal directly
        const QImage* cached = ThumbnailCache::instance()->retrieveImage(task.cacheKey);
        if (cached && !cached->isNull()) {
            Q_EMIT thumbnailLoaded(task.filePath, *cached);
            
            // Also emit pixmap if we're on main thread
            if (QThread::currentThread() == qApp->thread()) {
                QPixmap pixmap = QPixmap::fromImage(*cached);
                // Cache the pixmap in case it was evicted (image cache is larger than pixmap cache)
                ThumbnailCache::instance()->putPixmap(task.cacheKey, pixmap);
                Q_EMIT thumbnailReady(task.filePath, pixmap);
            }
            return;
        }
    }

    scheduleTask(task);
}

void ThumbnailLoadThread::loadBatch(const QStringList& filePaths, int size, LoadPriority priority)
{
    for (const QString& path : filePaths) {
        load(path, size, priority);
    }
}

void ThumbnailLoadThread::preload(const QStringList& filePaths, int size)
{
    loadBatch(filePaths, size, LoadPriority::Low);
}

void ThumbnailLoadThread::cancel(const QString& filePath)
{
    // Note: QThreadPool doesn't support cancellation of individual tasks
    // We just remove from pending set - already running tasks will complete
    QMutexLocker locker(&m_pendingMutex);
    
    // Remove all sizes
    QSet<QString> toRemove;
    for (const QString& key : m_pendingKeys) {
        if (key.startsWith(filePath + "@")) {
            toRemove.insert(key);
        }
    }
    for (const QString& key : toRemove) {
        m_pendingKeys.remove(key);
    }
}

void ThumbnailLoadThread::cancelAll()
{
    QMutexLocker locker(&m_pendingMutex);
    m_pendingKeys.clear();
    m_threadPool->clear();
}

bool ThumbnailLoadThread::find(const QString& filePath, int size, QPixmap& pixmap)
{
    QString cacheKey = makeCacheKey(filePath, size);
    
    // Check pixmap cache first
    const QPixmap* cachedPixmap = ThumbnailCache::instance()->retrievePixmap(cacheKey);
    if (cachedPixmap && !cachedPixmap->isNull()) {
        pixmap = *cachedPixmap;
        return true;
    }
    
    // Check image cache and convert
    const QImage* cachedImage = ThumbnailCache::instance()->retrieveImage(cacheKey);
    if (cachedImage && !cachedImage->isNull()) {
        pixmap = QPixmap::fromImage(*cachedImage);
        // Cache the pixmap for future use
        ThumbnailCache::instance()->putPixmap(cacheKey, pixmap);
        return true;
    }
    
    // Not found - trigger load
    load(filePath, size);
    return false;
}

bool ThumbnailLoadThread::find(const QString& filePath, int size, QImage& image)
{
    QString cacheKey = makeCacheKey(filePath, size);
    
    const QImage* cached = ThumbnailCache::instance()->retrieveImage(cacheKey);
    if (cached && !cached->isNull()) {
        image = *cached;
        return true;
    }
    
    // Not found - trigger load
    load(filePath, size);
    return false;
}

void ThumbnailLoadThread::setMaxThreads(int threads)
{
    m_threadPool->setMaxThreadCount(threads);
}

void ThumbnailLoadThread::setThumbnailSize(int size)
{
    m_defaultSize = size;
}

void ThumbnailLoadThread::scheduleTask(const ThumbnailTask& task)
{
    // Check if already pending
    {
        QMutexLocker locker(&m_pendingMutex);
        if (m_pendingKeys.contains(task.cacheKey)) {
            return; // Already scheduled
        }
        m_pendingKeys.insert(task.cacheKey);
    }

    // Create worker
    ThumbnailWorker* worker = new ThumbnailWorker(task);
    
    // Connect signal (Qt::QueuedConnection ensures delivery in main thread)
    connect(worker, &ThumbnailWorker::finished,
            this, &ThumbnailLoadThread::slotWorkerFinished,
            Qt::QueuedConnection);
    
    // Set priority
    int queuePriority = 0;
    switch (task.priority) {
        case LoadPriority::Low:    queuePriority = -1; break;
        case LoadPriority::Normal: queuePriority = 0;  break;
        case LoadPriority::High:   queuePriority = 1;  break;
    }
    
    // Start worker
    m_threadPool->start(worker, queuePriority);
}

void ThumbnailLoadThread::slotWorkerFinished(const ThumbnailResult& result)
{
    // Remove from pending
    {
        QMutexLocker locker(&m_pendingMutex);
        m_pendingKeys.remove(result.cacheKey);
    }

    if (result.success) {
        Q_EMIT thumbnailLoaded(result.filePath, result.image);
        
        // Create and cache pixmap (we're on main thread now)
        QPixmap pixmap = QPixmap::fromImage(result.image);
        ThumbnailCache::instance()->putPixmap(result.cacheKey, pixmap);
        
        Q_EMIT thumbnailReady(result.filePath, pixmap);
    } else {
        Q_EMIT thumbnailFailed(result.filePath);
    }
}

QString ThumbnailLoadThread::makeCacheKey(const QString& filePath, int size) const
{
    return ThumbnailInfo::makeCacheKey(filePath, size);
}

} // namespace FullFrame

