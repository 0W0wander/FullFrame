#pragma once

#include <QImage>
#include <QString>

namespace FullFrame {

class ThumbnailCreator
{
public:
    explicit ThumbnailCreator(int thumbnailSize = 256);
    ~ThumbnailCreator();

    void setThumbnailSize(int size);
    int thumbnailSize() const { return m_thumbnailSize; }

    QImage create(const QString& filePath) const;
    
    static bool isImageFile(const QString& filePath);
    static QStringList imageExtensions();

private:
    QImage loadAndScale(const QString& filePath) const;
    
    int m_thumbnailSize;
};

} // namespace FullFrame
