#include "thumbnailcreator.h"
#include <QImageReader>
#include <QFileInfo>

namespace FullFrame {

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
    if (filePath.isEmpty() || !isImageFile(filePath)) {
        return QImage();
    }
    return loadAndScale(filePath);
}

QImage ThumbnailCreator::loadAndScale(const QString& filePath) const
{
    QImageReader reader(filePath);
    reader.setAutoTransform(true);
    
    QSize originalSize = reader.size();
    if (originalSize.isValid()) {
        qreal scaleFactor = qMin(
            static_cast<qreal>(m_thumbnailSize) / originalSize.width(),
            static_cast<qreal>(m_thumbnailSize) / originalSize.height()
        );
        if (scaleFactor < 1.0) {
            reader.setScaledSize(originalSize * scaleFactor);
        }
    }
    
    return reader.read();
}

bool ThumbnailCreator::isImageFile(const QString& filePath)
{
    QString ext = QFileInfo(filePath).suffix().toLower();
    return imageExtensions().contains(ext);
}

QStringList ThumbnailCreator::imageExtensions()
{
    return { "jpg", "jpeg", "png", "gif", "bmp", "tiff", "tif", "webp" };
}

} // namespace FullFrame
