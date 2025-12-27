/**
 * ThumbnailCache implementation
 * 
 * Key performance features (inspired by DigiKam):
 * - Separate image/pixmap caches for thread safety
 * - LRU eviction to bound memory usage
 * - Read-write lock for concurrent read access
 */

#include "thumbnailcache.h"
#include <QThread>
#include <QApplication>

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

// ============== Image Cache (Thread-Safe, O(1) LRU) ==============

const QImage* ThumbnailCache::retrieveImage(const QString& cacheKey) const
{
    QWriteLocker locker(&m_imageLock);
    
    auto* nonConstThis = const_cast<ThumbnailCache*>(this);
    auto it = nonConstThis->m_imageCache.find(cacheKey);
    if (it != nonConstThis->m_imageCache.end()) {
        // Move to front of LRU - O(1) with std::list
        nonConstThis->m_imageLRU.erase(it.value().second);
        nonConstThis->m_imageLRU.push_front(cacheKey);
        it.value().second = nonConstThis->m_imageLRU.begin();
        return &it.value().first;
    }
    return nullptr;
}

void ThumbnailCache::putImage(const QString& cacheKey, const QImage& image)
{
    QWriteLocker locker(&m_imageLock);
    
    // Check if already exists
    auto it = m_imageCache.find(cacheKey);
    if (it != m_imageCache.end()) {
        it.value().first = image;
        m_imageLRU.erase(it.value().second);
        m_imageLRU.push_front(cacheKey);
        it.value().second = m_imageLRU.begin();
        return;
    }
    
    // Add new entry
    m_imageLRU.push_front(cacheKey);
    m_imageCache.insert(cacheKey, {image, m_imageLRU.begin()});
    
    // Evict if needed - O(1) per eviction
    while (static_cast<int>(m_imageCache.size()) > m_maxImages && !m_imageLRU.empty()) {
        QString keyToRemove = m_imageLRU.back();
        m_imageLRU.pop_back();
        m_imageCache.remove(keyToRemove);
    }
}

void ThumbnailCache::removeImage(const QString& cacheKey)
{
    QWriteLocker locker(&m_imageLock);
    auto it = m_imageCache.find(cacheKey);
    if (it != m_imageCache.end()) {
        m_imageLRU.erase(it.value().second);
        m_imageCache.erase(it);
    }
}

bool ThumbnailCache::hasImage(const QString& cacheKey) const
{
    QReadLocker locker(&m_imageLock);
    return m_imageCache.contains(cacheKey);
}

void ThumbnailCache::evictImagesIfNeeded()
{
    QWriteLocker locker(&m_imageLock);
    
    while (static_cast<int>(m_imageCache.size()) > m_maxImages && !m_imageLRU.empty()) {
        QString keyToRemove = m_imageLRU.back();
        m_imageLRU.pop_back();
        m_imageCache.remove(keyToRemove);
    }
}

// ============== Pixmap Cache (Main Thread, O(1) LRU) ==============

const QPixmap* ThumbnailCache::retrievePixmap(const QString& cacheKey) const
{
    Q_ASSERT(QThread::currentThread() == qApp->thread());
    
    QMutexLocker locker(&m_pixmapLock);
    
    auto* nonConstThis = const_cast<ThumbnailCache*>(this);
    auto it = nonConstThis->m_pixmapCache.find(cacheKey);
    if (it != nonConstThis->m_pixmapCache.end()) {
        nonConstThis->m_pixmapLRU.erase(it.value().second);
        nonConstThis->m_pixmapLRU.push_front(cacheKey);
        it.value().second = nonConstThis->m_pixmapLRU.begin();
        return &it.value().first;
    }
    return nullptr;
}

void ThumbnailCache::putPixmap(const QString& cacheKey, const QPixmap& pixmap)
{
    Q_ASSERT(QThread::currentThread() == qApp->thread());
    
    QMutexLocker locker(&m_pixmapLock);
    
    auto it = m_pixmapCache.find(cacheKey);
    if (it != m_pixmapCache.end()) {
        it.value().first = pixmap;
        m_pixmapLRU.erase(it.value().second);
        m_pixmapLRU.push_front(cacheKey);
        it.value().second = m_pixmapLRU.begin();
        return;
    }
    
    m_pixmapLRU.push_front(cacheKey);
    m_pixmapCache.insert(cacheKey, {pixmap, m_pixmapLRU.begin()});
    
    // Evict if needed - O(1) per eviction
    while (static_cast<int>(m_pixmapCache.size()) > m_maxPixmaps && !m_pixmapLRU.empty()) {
        QString keyToRemove = m_pixmapLRU.back();
        m_pixmapLRU.pop_back();
        m_pixmapCache.remove(keyToRemove);
    }
}

void ThumbnailCache::removePixmap(const QString& cacheKey)
{
    QMutexLocker locker(&m_pixmapLock);
    auto it = m_pixmapCache.find(cacheKey);
    if (it != m_pixmapCache.end()) {
        m_pixmapLRU.erase(it.value().second);
        m_pixmapCache.erase(it);
    }
}

bool ThumbnailCache::hasPixmap(const QString& cacheKey) const
{
    QMutexLocker locker(&m_pixmapLock);
    return m_pixmapCache.contains(cacheKey);
}

void ThumbnailCache::evictPixmapsIfNeeded()
{
    QMutexLocker locker(&m_pixmapLock);
    
    while (static_cast<int>(m_pixmapCache.size()) > m_maxPixmaps && !m_pixmapLRU.empty()) {
        QString keyToRemove = m_pixmapLRU.back();
        m_pixmapLRU.pop_back();
        m_pixmapCache.remove(keyToRemove);
    }
}

// ============== Cache Management ==============

void ThumbnailCache::setImageCacheSize(int maxImages)
{
    m_maxImages = maxImages;
    evictImagesIfNeeded();
}

void ThumbnailCache::setPixmapCacheSize(int maxPixmaps)
{
    m_maxPixmaps = maxPixmaps;
    evictPixmapsIfNeeded();
}

void ThumbnailCache::clearAll()
{
    {
        QWriteLocker locker(&m_imageLock);
        m_imageCache.clear();
        m_imageLRU.clear();
    }
    {
        QMutexLocker locker(&m_pixmapLock);
        m_pixmapCache.clear();
        m_pixmapLRU.clear();
    }
    Q_EMIT cacheCleared();
}

int ThumbnailCache::imageCacheCount() const
{
    QReadLocker locker(&m_imageLock);
    return static_cast<int>(m_imageCache.size());
}

int ThumbnailCache::pixmapCacheCount() const
{
    QMutexLocker locker(&m_pixmapLock);
    return static_cast<int>(m_pixmapCache.size());
}

quint64 ThumbnailCache::imageCacheBytes() const
{
    QReadLocker locker(&m_imageLock);
    quint64 bytes = 0;
    for (const auto& entry : m_imageCache) {
        bytes += entry.first.sizeInBytes();
    }
    return bytes;
}

// ============== CacheLock ==============

CacheLock::CacheLock(ThumbnailCache* cache)
    : m_cache(cache)
{
}

CacheLock::~CacheLock()
{
}

} // namespace FullFrame

