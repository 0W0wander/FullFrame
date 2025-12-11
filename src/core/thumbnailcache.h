#pragma once

#include <QObject>
#include <QImage>
#include <QPixmap>
#include <QHash>
#include <QMutex>
#include <QReadWriteLock>

namespace FullFrame {

class ThumbnailCache : public QObject
{
    Q_OBJECT

public:
    static ThumbnailCache* instance();
    static void cleanup();

    const QImage* retrieveImage(const QString& cacheKey) const;
    void putImage(const QString& cacheKey, const QImage& image);
    bool hasImage(const QString& cacheKey) const;
    
    void clearAll();
    int imageCacheCount() const;

Q_SIGNALS:
    void cacheCleared();

private:
    explicit ThumbnailCache(QObject* parent = nullptr);
    ~ThumbnailCache() override;

    static ThumbnailCache* s_instance;
    
    mutable QReadWriteLock m_imageLock;
    QHash<QString, QImage> m_imageCache;
    QList<QString> m_imageLRU;
    int m_maxImages = 500;
};

} // namespace FullFrame
