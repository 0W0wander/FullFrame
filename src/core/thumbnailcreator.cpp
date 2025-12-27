/**
 * ThumbnailCreator implementation
 * 
 * Performance optimizations (like DigiKam):
 * - Use embedded EXIF thumbnail when available (instant for JPEGs)
 * - Efficient downscaling with bilinear/smooth transformation
 * - Disk cache using FreeDesktop thumbnail standard
 * - Video frame extraction via Qt Multimedia or FFmpeg
 * - Placeholder generation for audio files
 */

#include "thumbnailcreator.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QImageReader>
#include <QCryptographicHash>
#include <QUrl>
#include <QDebug>
#include <QPainter>
#include <QPainterPath>
#include <QFont>
#include <QFontMetrics>
#include <QProcess>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QPolygon>
#include <QCoreApplication>
#include <QThread>
#include <QEventLoop>
#include <QTimer>

#ifdef HAVE_QT_MULTIMEDIA
#include <QMediaPlayer>
#include <QVideoSink>
#include <QVideoFrame>
#endif

namespace FullFrame {

namespace {
    // FreeDesktop thumbnail standard uses PNG
    const char* THUMBNAIL_FORMAT = "PNG";
    const int THUMBNAIL_QUALITY = 90;
}

ThumbnailCreator::ThumbnailCreator(int thumbnailSize)
    : m_thumbnailSize(thumbnailSize)
{
}

ThumbnailCreator::~ThumbnailCreator() = default;

void ThumbnailCreator::setThumbnailSize(int size)
{
    m_thumbnailSize = size;
}

QImage ThumbnailCreator::create(const QString& filePath) const
{
    ThumbnailInfo info;
    info.filePath = filePath;
    info.requestedSize = m_thumbnailSize;
    info.cacheKey = ThumbnailInfo::makeCacheKey(filePath, m_thumbnailSize);
    info.mediaType = getMediaType(filePath);
    return create(info);
}

QImage ThumbnailCreator::create(const ThumbnailInfo& info) const
{
    if (info.filePath.isEmpty()) {
        return QImage();
    }

    // 1. Try disk cache first
    if (m_useDiskCache) {
        QImage cached = loadFromDiskCache(info.filePath);
        if (!cached.isNull()) {
            return cached;
        }
    }

    QImage thumbnail;
    MediaType mediaType = info.mediaType;
    if (mediaType == MediaType::Unknown) {
        mediaType = getMediaType(info.filePath);
    }

    // 2. Create thumbnail based on media type
    switch (mediaType) {
        case MediaType::Image:
            thumbnail = createImageThumbnail(info.filePath);
            break;
        case MediaType::Video:
            thumbnail = createVideoThumbnail(info.filePath);
            break;
        case MediaType::Audio:
            thumbnail = createAudioPlaceholder(info.filePath);
            break;
        default:
            return QImage();
    }

    if (thumbnail.isNull()) {
        return QImage();
    }

    // 3. Final scale to exact size
    if (thumbnail.width() > m_thumbnailSize || thumbnail.height() > m_thumbnailSize) {
        thumbnail = thumbnail.scaled(
            m_thumbnailSize, m_thumbnailSize,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        );
    }

    // 4. Save to disk cache
    if (m_useDiskCache && !thumbnail.isNull()) {
        saveToDiskCache(info.filePath, thumbnail);
    }

    return thumbnail;
}

QImage ThumbnailCreator::createImageThumbnail(const QString& filePath) const
{
    // Try embedded EXIF thumbnail (fastest for JPEGs)
    QImage thumbnail = loadExifThumbnail(filePath);
    
    // If no EXIF thumbnail or too small, load and scale
    if (thumbnail.isNull() || 
        thumbnail.width() < m_thumbnailSize / 2 ||
        thumbnail.height() < m_thumbnailSize / 2) {
        thumbnail = loadAndScale(filePath);
    }

    // Apply EXIF rotation
    if (m_useExifRotation && !thumbnail.isNull()) {
        thumbnail = applyExifRotation(thumbnail, filePath);
    }

    return thumbnail;
}

QImage ThumbnailCreator::loadExifThumbnail(const QString& filePath) const
{
    QImageReader reader(filePath);
    
    // Enable auto-transform to fix EXIF rotation issues
    reader.setAutoTransform(true);
    
    // Check for embedded thumbnail
    if (reader.supportsOption(QImageIOHandler::Size)) {
        QSize fullSize = reader.size();
        
        // For large images, try to get scaled version directly
        if (fullSize.width() > m_thumbnailSize * 4 || 
            fullSize.height() > m_thumbnailSize * 4) {
            
            // Set scaled size hint for efficient loading
            reader.setScaledSize(QSize(m_thumbnailSize, m_thumbnailSize));
        }
    }
    
    // Read image (may be scaled if reader supports it)
    QImage image = reader.read();
    return image;
}

QImage ThumbnailCreator::loadAndScale(const QString& filePath) const
{
    QImageReader reader(filePath);
    
    // Enable auto-transform based on EXIF
    reader.setAutoTransform(true);
    
    // Calculate scaled size for efficient memory usage
    QSize originalSize = reader.size();
    if (originalSize.isValid()) {
        // Calculate scale factor
        qreal scaleFactor = qMin(
            static_cast<qreal>(m_thumbnailSize) / originalSize.width(),
            static_cast<qreal>(m_thumbnailSize) / originalSize.height()
        );
        
        // Only scale down, not up
        if (scaleFactor < 1.0) {
            QSize scaledSize = originalSize * scaleFactor;
            reader.setScaledSize(scaledSize);
        }
    }
    
    return reader.read();
}

QImage ThumbnailCreator::applyExifRotation(const QImage& image, const QString& filePath) const
{
    // QImageReader with autoTransform should handle this
    // This is a fallback for manual rotation if needed
    Q_UNUSED(filePath)
    return image;
}

QImage ThumbnailCreator::loadFromDiskCache(const QString& filePath) const
{
    QString cachePath = diskCachePath(filePath);
    if (cachePath.isEmpty()) {
        return QImage();
    }

    QFileInfo cacheInfo(cachePath);
    QFileInfo sourceInfo(filePath);

    // Check if cache is valid (exists and newer than source)
    if (!cacheInfo.exists()) {
        return QImage();
    }

    if (cacheInfo.lastModified() < sourceInfo.lastModified()) {
        // Source is newer, invalidate cache
        QFile::remove(cachePath);
        return QImage();
    }

    return QImage(cachePath);
}

void ThumbnailCreator::saveToDiskCache(const QString& filePath, const QImage& thumbnail) const
{
    QString cachePath = diskCachePath(filePath);
    if (cachePath.isEmpty()) {
        return;
    }

    // Ensure cache directory exists
    QDir cacheDir = QFileInfo(cachePath).dir();
    if (!cacheDir.exists()) {
        cacheDir.mkpath(".");
    }

    thumbnail.save(cachePath, THUMBNAIL_FORMAT, THUMBNAIL_QUALITY);
}

QString ThumbnailCreator::diskCachePath(const QString& filePath) const
{
    QString cacheDir = thumbnailCacheDir();
    if (cacheDir.isEmpty()) {
        return QString();
    }

    // Create MD5 hash of file URI (FreeDesktop standard)
    QUrl fileUrl = QUrl::fromLocalFile(filePath);
    QString uri = fileUrl.toString();
    QByteArray hash = QCryptographicHash::hash(uri.toUtf8(), QCryptographicHash::Md5);
    QString hashStr = hash.toHex();

    // Determine subdirectory based on size (FreeDesktop standard)
    QString sizeDir = (m_thumbnailSize <= 128) ? "normal" : "large";

    return QString("%1/%2/%3.png").arg(cacheDir, sizeDir, hashStr);
}

QString ThumbnailCreator::thumbnailCacheDir() const
{
    // FreeDesktop standard: ~/.cache/thumbnails
    QString cacheLocation = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
    if (cacheLocation.isEmpty()) {
        return QString();
    }
    return cacheLocation + "/thumbnails";
}

QImage ThumbnailCreator::createVideoThumbnail(const QString& filePath) const
{
    QImage thumbnail;
    
    // Try FFmpeg frame extraction (most reliable for video thumbnails)
    thumbnail = extractVideoFrameWithFFmpeg(filePath);
    if (!thumbnail.isNull()) {
        return thumbnail;
    }
    
    // Fallback to placeholder if extraction failed
    return createVideoPlaceholder(filePath);
}

QImage ThumbnailCreator::extractVideoFrameWithFFmpeg(const QString& filePath) const
{
    // Find FFmpeg executable (cached for performance)
    static QString ffmpegPath = findFFmpegPath();
    
    if (ffmpegPath.isEmpty()) {
        return QImage();
    }
    
    // Generate unique temp file name
    QString tempPath = QDir::tempPath() + "/ff_thumb_" + 
                       QString::number(qHash(filePath) ^ m_thumbnailSize ^ reinterpret_cast<quintptr>(QThread::currentThreadId())) + ".jpg";
    
    // Build FFmpeg arguments
    // Use -ss before -i for fast seeking (input seeking)
    QStringList args;
    args << "-y"                      // Overwrite output
         << "-ss" << "2"              // Seek to 2 seconds (skip black frames at start)
         << "-i" << filePath          // Input file
         << "-vframes" << "1"         // Extract only 1 frame
         << "-vf" << QString("scale=%1:%1:force_original_aspect_ratio=decrease,pad=%1:%1:(ow-iw)/2:(oh-ih)/2:color=black")
                      .arg(m_thumbnailSize)  // Scale and pad to square
         << "-q:v" << "3"             // Quality (2-31, lower = better)
         << "-f" << "image2"          // Force image output format
         << tempPath;
    
    QProcess ffmpeg;
    ffmpeg.setProcessChannelMode(QProcess::MergedChannels);
    
    // Start FFmpeg
    ffmpeg.start(ffmpegPath, args);
    
    // Wait for process to finish
    // Use 5 second timeout - fast seeking with -ss before -i makes this quick
    bool finished = ffmpeg.waitForFinished(5000);
    
    if (!finished) {
        ffmpeg.kill();
        ffmpeg.waitForFinished(1000);
        QFile::remove(tempPath);
        return QImage();
    }
    
    if (ffmpeg.exitCode() != 0) {
        // Try alternate seek position (some videos have no frame at 2s)
        args.clear();
        args << "-y"
             << "-ss" << "0.1"        // Very beginning
             << "-i" << filePath
             << "-vframes" << "1"
             << "-vf" << QString("scale=%1:%1:force_original_aspect_ratio=decrease,pad=%1:%1:(ow-iw)/2:(oh-ih)/2:color=black")
                          .arg(m_thumbnailSize)
             << "-q:v" << "3"
             << "-f" << "image2"
             << tempPath;
        
        ffmpeg.start(ffmpegPath, args);
        finished = ffmpeg.waitForFinished(3000);  // Shorter timeout for fallback
        
        if (!finished || ffmpeg.exitCode() != 0) {
            QFile::remove(tempPath);
            return QImage();
        }
    }
    
    // Load the generated thumbnail
    QImage thumb(tempPath);
    QFile::remove(tempPath);
    
    if (thumb.isNull()) {
        return QImage();
    }
    
    // Scale to exact thumbnail size
    if (thumb.width() != m_thumbnailSize || thumb.height() != m_thumbnailSize) {
        thumb = thumb.scaled(m_thumbnailSize, m_thumbnailSize, 
                            Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    
    return thumb;
}

QString ThumbnailCreator::findFFmpegPath()
{
    // Cache the result - only search once per process
    static QString cachedPath;
    static bool searched = false;
    
    if (searched) {
        return cachedPath;
    }
    searched = true;
    
    // First, search common installation paths (faster than testing execution)
    QStringList searchPaths = getFFmpegSearchPaths();
    
    for (const QString& path : searchPaths) {
        QString ffmpegExe = path;
#ifdef Q_OS_WIN
        if (!ffmpegExe.endsWith(".exe", Qt::CaseInsensitive)) {
            ffmpegExe += ".exe";
        }
#endif
        if (QFile::exists(ffmpegExe)) {
            cachedPath = ffmpegExe;
            return cachedPath;
        }
    }
    
    // Fallback: Check if ffmpeg is in PATH by testing execution
    QProcess testProcess;
    testProcess.setProcessChannelMode(QProcess::MergedChannels);
    
#ifdef Q_OS_WIN
    testProcess.start("ffmpeg.exe", QStringList() << "-version");
#else
    testProcess.start("ffmpeg", QStringList() << "-version");
#endif
    
    if (testProcess.waitForFinished(2000) && testProcess.exitCode() == 0) {
#ifdef Q_OS_WIN
        cachedPath = "ffmpeg.exe";
#else
        cachedPath = "ffmpeg";
#endif
        return cachedPath;
    }
    
    return QString();
}

QStringList ThumbnailCreator::getFFmpegSearchPaths()
{
    QStringList paths;
    
#ifdef Q_OS_WIN
    // Common Windows FFmpeg installation locations
    paths << "C:/ffmpeg/bin/ffmpeg"
          << "C:/Program Files/ffmpeg/bin/ffmpeg"
          << "C:/Program Files (x86)/ffmpeg/bin/ffmpeg"
          << QDir::homePath() + "/ffmpeg/bin/ffmpeg"
          << QDir::homePath() + "/scoop/apps/ffmpeg/current/bin/ffmpeg"
          << QDir::homePath() + "/scoop/shims/ffmpeg"
          << "C:/tools/ffmpeg/bin/ffmpeg"
          << "C:/ProgramData/chocolatey/bin/ffmpeg"
          << QCoreApplication::applicationDirPath() + "/ffmpeg"
          << QCoreApplication::applicationDirPath() + "/bin/ffmpeg";
    
    // WinGet installation - search dynamically for any FFmpeg version
    QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
    if (!localAppData.isEmpty()) {
        QDir wingetDir(localAppData + "/Microsoft/WinGet/Packages");
        if (wingetDir.exists()) {
            // Look for any folder containing "FFmpeg"
            QStringList ffmpegDirs = wingetDir.entryList(QStringList() << "*FFmpeg*" << "*ffmpeg*", 
                                                          QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString& dir : ffmpegDirs) {
                QDir packageDir(wingetDir.filePath(dir));
                // Search recursively for ffmpeg.exe in bin folders
                QStringList subDirs = packageDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
                for (const QString& subDir : subDirs) {
                    paths << packageDir.filePath(subDir) + "/bin/ffmpeg";
                }
                paths << packageDir.filePath("bin/ffmpeg");
            }
        }
    }
    
    // Search in PATH environment variable
    QString pathEnv = qEnvironmentVariable("PATH");
    QStringList pathDirs = pathEnv.split(';', Qt::SkipEmptyParts);
    for (const QString& dir : pathDirs) {
        paths << QDir(dir).filePath("ffmpeg");
    }
#else
    // Common Unix/Linux/macOS paths
    paths << "/usr/bin/ffmpeg"
          << "/usr/local/bin/ffmpeg"
          << "/opt/homebrew/bin/ffmpeg"
          << "/opt/local/bin/ffmpeg"
          << QDir::homePath() + "/.local/bin/ffmpeg"
          << QCoreApplication::applicationDirPath() + "/ffmpeg";
#endif
    
    return paths;
}

QImage ThumbnailCreator::createVideoPlaceholder(const QString& filePath) const
{
    // Create a styled video placeholder with play button
    QImage placeholder(m_thumbnailSize, m_thumbnailSize, QImage::Format_RGB32);
    placeholder.fill(QColor(35, 42, 52));
    
    QPainter painter(&placeholder);
    if (!painter.isActive()) {
        return placeholder;
    }
    painter.setRenderHint(QPainter::Antialiasing, true);
    
    int centerX = m_thumbnailSize / 2;
    int centerY = m_thumbnailSize / 2;
    int circleRadius = m_thumbnailSize / 4;
    
    // Outer glow
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(80, 100, 130, 40));
    painter.drawEllipse(QPoint(centerX, centerY), circleRadius + 8, circleRadius + 8);
    
    // Main circle
    painter.setBrush(QColor(60, 75, 95));
    painter.drawEllipse(QPoint(centerX, centerY), circleRadius, circleRadius);
    
    // Play triangle
    int triSize = circleRadius / 2;
    painter.setBrush(QColor(200, 210, 225));
    QPolygon triangle;
    triangle << QPoint(centerX - triSize/3, centerY - triSize)
             << QPoint(centerX - triSize/3, centerY + triSize)
             << QPoint(centerX + triSize, centerY);
    painter.drawPolygon(triangle);
    
    // File extension label
    QString ext = QFileInfo(filePath).suffix().toUpper();
    QFont font = painter.font();
    font.setPointSize(8);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(QColor(120, 130, 150));
    painter.drawText(QRect(0, m_thumbnailSize - 18, m_thumbnailSize, 16), 
                     Qt::AlignCenter, ext);
    
    painter.end();
    return placeholder;
}

QImage ThumbnailCreator::createAudioPlaceholder(const QString& filePath) const
{
    // Using Format_RGB32 for better compatibility
    QImage placeholder(m_thumbnailSize, m_thumbnailSize, QImage::Format_RGB32);
    placeholder.fill(QColor(50, 42, 55));
    
    QPainter painter(&placeholder);
    if (!painter.isActive()) {
        return placeholder;
    }
    painter.setRenderHint(QPainter::Antialiasing, true);
    
    // Gradient background (warm dark tone)
    QLinearGradient gradient(0, 0, 0, m_thumbnailSize);
    gradient.setColorAt(0, QColor(55, 45, 55));
    gradient.setColorAt(1, QColor(38, 32, 42));
    painter.fillRect(placeholder.rect(), gradient);
    
    int centerX = m_thumbnailSize / 2;
    int centerY = m_thumbnailSize / 2;
    int circleRadius = m_thumbnailSize / 4;
    
    // Draw circle background
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 20));
    painter.drawEllipse(QPoint(centerX, centerY), circleRadius + 5, circleRadius + 5);
    
    QRadialGradient circleGradient(centerX, centerY - circleRadius/3, circleRadius * 1.5);
    circleGradient.setColorAt(0, QColor(100, 85, 110));
    circleGradient.setColorAt(1, QColor(70, 60, 80));
    painter.setBrush(circleGradient);
    painter.drawEllipse(QPoint(centerX, centerY), circleRadius, circleRadius);
    
    // Draw music note
    painter.setBrush(QColor(220, 210, 230));
    painter.setPen(Qt::NoPen);
    
    int noteSize = circleRadius / 2;
    // Note head
    painter.drawEllipse(centerX - noteSize/2, centerY + noteSize/4, noteSize, noteSize * 2/3);
    // Stem
    painter.fillRect(QRect(centerX + noteSize/2 - 3, centerY - noteSize, 4, noteSize + noteSize/2), QColor(220, 210, 230));
    // Flag
    QPainterPath flag;
    flag.moveTo(centerX + noteSize/2, centerY - noteSize);
    flag.cubicTo(centerX + noteSize + 8, centerY - noteSize + 10,
                 centerX + noteSize/2, centerY - noteSize/2 + 5,
                 centerX + noteSize/2, centerY - noteSize/3);
    painter.drawPath(flag);
    
    // Draw file extension label at bottom
    QString ext = QFileInfo(filePath).suffix().toUpper();
    
    QFont font = painter.font();
    font.setPointSize(9);
    font.setBold(true);
    painter.setFont(font);
    QFontMetrics fm(font);
    int textWidth = fm.horizontalAdvance(ext) + 16;
    int pillHeight = 20;
    int pillY = m_thumbnailSize - pillHeight - 8;
    int pillX = (m_thumbnailSize - textWidth) / 2;
    
    painter.setBrush(QColor(0, 0, 0, 150));
    painter.drawRoundedRect(pillX, pillY, textWidth, pillHeight, 4, 4);
    
    painter.setPen(QColor(210, 200, 220));
    painter.drawText(QRect(pillX, pillY, textWidth, pillHeight), Qt::AlignCenter, ext);
    
    painter.end();
    return placeholder;
}

bool ThumbnailCreator::isMediaFile(const QString& filePath)
{
    return getMediaType(filePath) != MediaType::Unknown;
}

bool ThumbnailCreator::isImageFile(const QString& filePath)
{
    return getMediaType(filePath) == MediaType::Image;
}

bool ThumbnailCreator::isVideoFile(const QString& filePath)
{
    return getMediaType(filePath) == MediaType::Video;
}

bool ThumbnailCreator::isAudioFile(const QString& filePath)
{
    return getMediaType(filePath) == MediaType::Audio;
}

MediaType ThumbnailCreator::getMediaType(const QString& filePath)
{
    static QSet<QString> imageExts;
    static QSet<QString> videoExts;
    static QSet<QString> audioExts;
    
    if (imageExts.isEmpty()) {
        for (const QString& ext : imageExtensions()) {
            imageExts.insert(ext.toLower());
        }
        for (const QString& ext : videoExtensions()) {
            videoExts.insert(ext.toLower());
        }
        for (const QString& ext : audioExtensions()) {
            audioExts.insert(ext.toLower());
        }
    }
    
    QString ext = QFileInfo(filePath).suffix().toLower();
    
    if (imageExts.contains(ext)) return MediaType::Image;
    if (videoExts.contains(ext)) return MediaType::Video;
    if (audioExts.contains(ext)) return MediaType::Audio;
    
    return MediaType::Unknown;
}

QStringList ThumbnailCreator::supportedExtensions()
{
    QStringList all;
    all << imageExtensions() << videoExtensions() << audioExtensions();
    return all;
}

QStringList ThumbnailCreator::imageExtensions()
{
    return {
        "jpg", "jpeg", "png", "gif", "bmp", "tiff", "tif",
        "webp", "svg", "ico", "pbm", "pgm", "ppm", "xbm", "xpm"
    };
}

QStringList ThumbnailCreator::videoExtensions()
{
    return {
        // Common video formats
        "mp4", "m4v", "mkv", "webm", "mov", "avi", "wmv", "flv",
        // Mobile/camera formats
        "3gp", "3g2", "mts", "m2ts", "ts",
        // Professional/broadcast formats
        "mpg", "mpeg", "vob", "ogv", "ogm",
        // Windows Media formats
        "asf", "wm",
        // RealMedia
        "rm", "rmvb",
        // Flash
        "f4v", "swf",
        // Other formats
        "divx", "xvid", "dv", "mxf", "qt", "yuv",
        // Apple formats
        "m4p",
        // HEVC/H.265 containers
        "hevc", "h264", "h265", "265",
        // AV1 containers  
        "av1", "ivf",
        // Animated images (treated as video)
        "apng", "mng"
    };
}

QStringList ThumbnailCreator::audioExtensions()
{
    return {
        "mp3", "m4a", "wav", "flac", "ogg", "aac", "wma"
    };
}

} // namespace FullFrame

