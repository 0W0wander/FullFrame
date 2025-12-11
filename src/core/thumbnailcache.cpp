#include "thumbnailcache.h"

namespace FullFrame {

ThumbnailCache* ThumbnailCache::s_instance = nullptr;

ThumbnailCache* ThumbnailCache::instance()
{
    if (!s_instance) {
        s_instance = new ThumbnailCache();
    }
    return s_instance;
}

void ThumbnailCache::cleanup()
{
    delete s_instance;
    s_instance = nullptr;
}

ThumbnailCache::ThumbnailCache(QObject* parent)
    : QObject(parent)
{
}

ThumbnailCache::~ThumbnailCache()
{
    clearAll();
}

const QImage* ThumbnailCache::retrieveImage(const QString& cacheKey) const
{
    QReadLocker locker(&m_imageLock);
    auto it = m_imageCache.find(cacheKey);
    if (it != m_imageCache.end()) {
        return &it.value();
    }
    return nullptr;
}

void ThumbnailCache::putImage(const QString& cacheKey, const QImage& image)
{
    QWriteLocker locker(&m_imageLock);
    m_imageCache.insert(cacheKey, image);
    m_imageLRU.prepend(cacheKey);
    
    while (m_imageCache.size() > m_maxImages && !m_imageLRU.isEmpty()) {
        QString key = m_imageLRU.takeLast();
        m_imageCache.remove(key);
    }
}

bool ThumbnailCache::hasImage(const QString& cacheKey) const
{
    QReadLocker locker(&m_imageLock);
    return m_imageCache.contains(cacheKey);
}

void ThumbnailCache::clearAll()
{
    QWriteLocker locker(&m_imageLock);
    m_imageCache.clear();
    m_imageLRU.clear();
    Q_EMIT cacheCleared();
}

int ThumbnailCache::imageCacheCount() const
{
    QReadLocker locker(&m_imageLock);
    return m_imageCache.size();
}

} // namespace FullFrame
