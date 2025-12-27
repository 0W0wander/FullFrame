/**
 * ThumbnailCache - High-performance thumbnail caching system
 * 
 * Inspired by DigiKam's LoadingCache, this implements:
 * - Thread-safe QImage cache for background loading
 * - Main-thread QPixmap cache for display
 * - LRU eviction policy
 * - Configurable cache sizes
 */

#pragma once

#include <QObject>
#include <QImage>
#include <QPixmap>
#include <QMutex>
#include <QHash>
#include <QString>
#include <QReadWriteLock>
#include <list>

namespace FullFrame {

/**
 * Thread-safe LRU cache for thumbnails
 * Similar to DigiKam's LoadingCache but simplified
 */
class ThumbnailCache : public QObject
{
    Q_OBJECT

public:
    static ThumbnailCache* instance();
    static void cleanup();

    // Thread-safe QImage cache (can be accessed from any thread)
    const QImage* retrieveImage(const QString& cacheKey) const;
    void putImage(const QString& cacheKey, const QImage& image);
    void removeImage(const QString& cacheKey);
    bool hasImage(const QString& cacheKey) const;

    // QPixmap cache (main thread only)
    const QPixmap* retrievePixmap(const QString& cacheKey) const;
    void putPixmap(const QString& cacheKey, const QPixmap& pixmap);
    void removePixmap(const QString& cacheKey);
    bool hasPixmap(const QString& cacheKey) const;

    // Cache management
    void setImageCacheSize(int maxImages);
    void setPixmapCacheSize(int maxPixmaps);
    void clearAll();

    // Statistics
    int imageCacheCount() const;
    int pixmapCacheCount() const;
    quint64 imageCacheBytes() const;

Q_SIGNALS:
    void cacheCleared();

private:
    explicit ThumbnailCache(QObject* parent = nullptr);
    ~ThumbnailCache() override;

    // Disable copy
    ThumbnailCache(const ThumbnailCache&) = delete;
    ThumbnailCache& operator=(const ThumbnailCache&) = delete;

    void evictImagesIfNeeded();
    void evictPixmapsIfNeeded();

private:
    static ThumbnailCache* s_instance;

    // Image cache (thread-safe) - O(1) LRU using std::list + hash map
    mutable QReadWriteLock m_imageLock;
    std::list<QString> m_imageLRU;  // Front = most recently used
    QHash<QString, std::pair<QImage, std::list<QString>::iterator>> m_imageCache;
    int m_maxImages = 1000;  // Increased for large collections

    // Pixmap cache (main thread only)
    mutable QMutex m_pixmapLock;
    std::list<QString> m_pixmapLRU;
    QHash<QString, std::pair<QPixmap, std::list<QString>::iterator>> m_pixmapCache;
    int m_maxPixmaps = 500;  // Increased for large collections
};

/**
 * RAII-style cache lock for batch operations
 */
class CacheLock
{
public:
    explicit CacheLock(ThumbnailCache* cache);
    ~CacheLock();

private:
    ThumbnailCache* m_cache;
};

} // namespace FullFrame

