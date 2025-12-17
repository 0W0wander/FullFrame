#include "imagethumbnailmodel.h"
#include "thumbnailloadthread.h"
#include "thumbnailcreator.h"
#include <QDir>
#include <QFileInfo>

namespace FullFrame {

ImageThumbnailModel::ImageThumbnailModel(QObject* parent)
    : QAbstractListModel(parent)
{
    connect(ThumbnailLoadThread::instance(), &ThumbnailLoadThread::thumbnailReady,
            this, &ImageThumbnailModel::onThumbnailReady);
}

ImageThumbnailModel::~ImageThumbnailModel() = default;

int ImageThumbnailModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return m_files.count();
}

QVariant ImageThumbnailModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_files.count())
        return QVariant();

    const QString& path = m_files.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
    case FileNameRole:
        return QFileInfo(path).fileName();
    case FilePathRole:
        return path;
    case Qt::DecorationRole:
    case ThumbnailRole:
        if (m_thumbnails.contains(path)) {
            return m_thumbnails.value(path);
        }
        ThumbnailLoadThread::instance()->load(path, m_thumbnailSize);
        return QVariant();
    default:
        return QVariant();
    }
}

void ImageThumbnailModel::setDirectory(const QString& path)
{
    beginResetModel();
    clear();
    scanDirectory(path);
    endResetModel();
}

void ImageThumbnailModel::clear()
{
    m_files.clear();
    m_thumbnails.clear();
    m_pathToIndex.clear();
}

QString ImageThumbnailModel::filePath(int index) const
{
    if (index >= 0 && index < m_files.count()) {
        return m_files.at(index);
    }
    return QString();
}

int ImageThumbnailModel::indexOf(const QString& path) const
{
    return m_pathToIndex.value(path, -1);
}

void ImageThumbnailModel::onThumbnailReady(const QString& filePath, const QPixmap& pixmap)
{
    int idx = indexOf(filePath);
    if (idx >= 0) {
        m_thumbnails.insert(filePath, pixmap);
        QModelIndex modelIdx = index(idx);
        Q_EMIT dataChanged(modelIdx, modelIdx, {Qt::DecorationRole, ThumbnailRole});
    }
}

void ImageThumbnailModel::scanDirectory(const QString& path)
{
    QDir dir(path);
    QStringList filters;
    for (const QString& ext : ThumbnailCreator::imageExtensions()) {
        filters << "*." + ext;
    }
    
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Name);
    
    Q_EMIT loadingStarted(files.count());
    
    int index = 0;
    for (const QFileInfo& info : files) {
        m_files.append(info.absoluteFilePath());
        m_pathToIndex.insert(info.absoluteFilePath(), index++);
        Q_EMIT loadingProgress(index, files.count());
    }
    
    Q_EMIT loadingFinished();
}

} // namespace FullFrame
