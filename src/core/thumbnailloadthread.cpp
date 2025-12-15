#include "thumbnailloadthread.h"
#include "thumbnailcache.h"
#include <QApplication>
#include <QThread>

namespace FullFrame {

ThumbnailLoadThread* ThumbnailLoadThread::s_instance = nullptr;

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

    ThumbnailCreator creator(m_task.size);
    result.image = creator.create(m_task.filePath);
    result.success = !result.image.isNull();

    if (result.success) {
        ThumbnailCache::instance()->putImage(result.cacheKey, result.image);
    }

    Q_EMIT finished(result);
}

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
    int threads = QThread::idealThreadCount();
    m_threadPool->setMaxThreadCount(qMax(2, threads - 1));
}

ThumbnailLoadThread::~ThumbnailLoadThread()
{
    cancelAll();
    m_threadPool->waitForDone();
}

void ThumbnailLoadThread::load(const QString& filePath, int size)
{
    ThumbnailTask task;
    task.filePath = filePath;
    task.size = size;
    task.cacheKey = makeCacheKey(filePath, size);
    
    if (ThumbnailCache::instance()->hasImage(task.cacheKey)) {
        const QImage* cached = ThumbnailCache::instance()->retrieveImage(task.cacheKey);
        if (cached) {
            Q_EMIT thumbnailLoaded(filePath, *cached);
            Q_EMIT thumbnailReady(filePath, QPixmap::fromImage(*cached));
            return;
        }
    }
    
    scheduleTask(task);
}

void ThumbnailLoadThread::cancel(const QString& filePath)
{
    QMutexLocker locker(&m_pendingMutex);
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

void ThumbnailLoadThread::scheduleTask(const ThumbnailTask& task)
{
    {
        QMutexLocker locker(&m_pendingMutex);
        if (m_pendingKeys.contains(task.cacheKey)) {
            return;
        }
        m_pendingKeys.insert(task.cacheKey);
    }

    ThumbnailWorker* worker = new ThumbnailWorker(task);
    connect(worker, &ThumbnailWorker::finished,
            this, &ThumbnailLoadThread::slotWorkerFinished,
            Qt::QueuedConnection);
    
    m_threadPool->start(worker);
}

void ThumbnailLoadThread::slotWorkerFinished(const ThumbnailResult& result)
{
    {
        QMutexLocker locker(&m_pendingMutex);
        m_pendingKeys.remove(result.cacheKey);
    }

    if (result.success) {
        Q_EMIT thumbnailLoaded(result.filePath, result.image);
        QPixmap pixmap = QPixmap::fromImage(result.image);
        Q_EMIT thumbnailReady(result.filePath, pixmap);
    }
}

} // namespace FullFrame
