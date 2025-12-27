/**
 * ThumbnailCreator - Efficient thumbnail generation
 * 
 * Inspired by DigiKam's ThumbnailCreator:
 * - Fast JPEG extraction using Qt's built-in EXIF thumbnail
 * - Efficient scaling with smooth transformation
 * - EXIF rotation handling
 * - Disk caching support
 * - Video thumbnail extraction (Qt Multimedia or FFmpeg)
 * - Audio placeholder generation
 */

#pragma once

#include <QObject>
#include <QImage>
#include <QString>
#include <QSize>
#include <QStringList>

namespace FullFrame {

/**
 * Media type classification
 */
enum class MediaType {
    Unknown,
    Image,
    Video,
    Audio
};

struct ThumbnailInfo
{
    QString filePath;
    QString cacheKey;
    int requestedSize = 256;
    bool isValid = false;
    MediaType mediaType = MediaType::Unknown;
    
    static QString makeCacheKey(const QString& path, int size) {
        return QString("%1@%2").arg(path).arg(size);
    }
};

/**
 * Creates thumbnails from image, video, and audio files
 * Thread-safe - can be used from worker threads
 */
class ThumbnailCreator
{
public:
    explicit ThumbnailCreator(int thumbnailSize = 256);
    ~ThumbnailCreator();

    // Set the target thumbnail size
    void setThumbnailSize(int size);
    int thumbnailSize() const { return m_thumbnailSize; }

    // Create a thumbnail from file (handles images, videos, audio)
    QImage create(const QString& filePath) const;
    QImage create(const ThumbnailInfo& info) const;

    // Load from disk cache (FreeDesktop standard location)
    QImage loadFromDiskCache(const QString& filePath) const;
    
    // Save to disk cache
    void saveToDiskCache(const QString& filePath, const QImage& thumbnail) const;

    // File type detection
    static bool isMediaFile(const QString& filePath);
    static bool isImageFile(const QString& filePath);
    static bool isVideoFile(const QString& filePath);
    static bool isAudioFile(const QString& filePath);
    static MediaType getMediaType(const QString& filePath);
    
    // Supported extensions
    static QStringList supportedExtensions();
    static QStringList imageExtensions();
    static QStringList videoExtensions();
    static QStringList audioExtensions();

private:
    // Image thumbnail creation
    QImage createImageThumbnail(const QString& filePath) const;
    QImage loadExifThumbnail(const QString& filePath) const;
    QImage loadAndScale(const QString& filePath) const;
    QImage applyExifRotation(const QImage& image, const QString& filePath) const;
    
    // Video thumbnail creation
    QImage createVideoThumbnail(const QString& filePath) const;
    QImage extractVideoFrameWithFFmpeg(const QString& filePath) const;
    QImage createVideoPlaceholder(const QString& filePath) const;
    
    // FFmpeg helper
    static QString findFFmpegPath();
    static QStringList getFFmpegSearchPaths();
    
    // Audio placeholder creation
    QImage createAudioPlaceholder(const QString& filePath) const;
    
    // Get disk cache path
    QString diskCachePath(const QString& filePath) const;
    QString thumbnailCacheDir() const;

private:
    int m_thumbnailSize;
    bool m_useExifRotation = true;
    bool m_useDiskCache = true;
};

} // namespace FullFrame

