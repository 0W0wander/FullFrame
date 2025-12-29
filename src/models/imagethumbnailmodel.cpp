/**
 * ImageThumbnailModel implementation
 * 
 * Key features:
 * - Lazy thumbnail loading (only visible items)
 * - Efficient path-to-index lookup
 * - Tag-based filtering
 */

#include "imagethumbnailmodel.h"
#include "thumbnailloadthread.h"
#include "thumbnailcache.h"
#include "thumbnailcreator.h"
#include "tagmanager.h"

#include <QDirIterator>
#include <QPainter>
#include <QLocale>
#include <QDebug>

namespace FullFrame {

ImageThumbnailModel::ImageThumbnailModel(QObject* parent)
    : QAbstractListModel(parent)
{
    connectThumbnailThread();
    connectTagManager();
    
    // Create placeholder pixmaps
    m_loadingPixmap = QPixmap(m_thumbnailSize, m_thumbnailSize);
    m_loadingPixmap.fill(QColor(40, 40, 40));
    
    m_errorPixmap = QPixmap(m_thumbnailSize, m_thumbnailSize);
    m_errorPixmap.fill(QColor(60, 40, 40));
}

ImageThumbnailModel::~ImageThumbnailModel() = default;

void ImageThumbnailModel::connectThumbnailThread()
{
    connect(ThumbnailLoadThread::instance(), &ThumbnailLoadThread::thumbnailReady,
            this, &ImageThumbnailModel::onThumbnailReady);
    connect(ThumbnailLoadThread::instance(), &ThumbnailLoadThread::thumbnailFailed,
            this, &ImageThumbnailModel::onThumbnailFailed);
}

void ImageThumbnailModel::connectTagManager()
{
    connect(TagManager::instance(), &TagManager::imageTagged,
            this, &ImageThumbnailModel::onImageTagged);
    connect(TagManager::instance(), &TagManager::imageUntagged,
            this, &ImageThumbnailModel::onImageUntagged);
}

// ============== QAbstractListModel Interface ==============

int ImageThumbnailModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_items.size();
}

QVariant ImageThumbnailModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return QVariant();
    }

    const ImageItem& item = m_items.at(index.row());

    switch (role) {
        case Qt::DisplayRole:
        case FileNameRole:
            return item.fileName;
            
        case Qt::DecorationRole:
        case ThumbnailRole: {
            // Fast path: Return cached pixmap if already loaded
            if (item.thumbnailLoaded && !item.cachedPixmap.isNull()) {
                return item.cachedPixmap;
            }
            
            // Generate cache key once
            QString cacheKey = ThumbnailInfo::makeCacheKey(item.filePath, m_thumbnailSize);
            
            // Try pixmap cache first (most common case)
            const QPixmap* cached = ThumbnailCache::instance()->retrievePixmap(cacheKey);
            if (cached && !cached->isNull()) {
                item.cachedPixmap = *cached;
                item.thumbnailLoaded = true;
                return item.cachedPixmap;
            }
            
            // Try image cache as fallback
            const QImage* cachedImage = ThumbnailCache::instance()->retrieveImage(cacheKey);
            if (cachedImage && !cachedImage->isNull()) {
                item.cachedPixmap = QPixmap::fromImage(*cachedImage);
                item.thumbnailLoaded = true;
                ThumbnailCache::instance()->putPixmap(cacheKey, item.cachedPixmap);
                return item.cachedPixmap;
            }
            
            // Request thumbnail load if not already pending
            if (!m_pendingThumbnails.contains(item.filePath)) {
                m_pendingThumbnails.insert(item.filePath);
                ThumbnailLoadThread::instance()->load(item.filePath, m_thumbnailSize);
            }
            
            return m_loadingPixmap;
        }
            
        case FilePathRole:
            return item.filePath;
            
        case FileSizeRole:
            return item.fileSize;
            
        case ModifiedDateRole:
            return item.modifiedDate;
            
        case TagIdsRole:
            return QVariant::fromValue(item.tagIds);
            
        case SelectedRole:
            return item.selected;
            
        case HasTagsRole:
            return !item.tagIds.isEmpty();
            
        case TagListRole: {
            // Only allocate if we have tags (reduces allocations for untagged images)
            if (item.tagIds.isEmpty()) {
                return QVariant();
            }
            
            // Pre-allocate with expected size
            QVariantList tagList;
            tagList.reserve(item.tagIds.size());
            
            for (qint64 tagId : item.tagIds) {
                Tag tag = TagManager::instance()->tag(tagId);
                if (tag.isValid()) {
                    QVariantMap tagInfo;
                    tagInfo.insert(QStringLiteral("name"), tag.name);
                    tagInfo.insert(QStringLiteral("color"), tag.color);
                    tagList.append(tagInfo);
                }
            }
            return tagList;
        }
        
        case MediaTypeRole:
            return static_cast<int>(item.mediaType);
            
        case Qt::ToolTipRole:
            return QString("%1\n%2\n%3")
                .arg(item.fileName)
                .arg(QLocale().formattedDataSize(item.fileSize))
                .arg(QLocale().toString(item.modifiedDate, QLocale::ShortFormat));
    }

    return QVariant();
}

bool ImageThumbnailModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return false;
    }

    ImageItem& item = m_items[index.row()];

    switch (role) {
        case SelectedRole:
            item.selected = value.toBool();
            Q_EMIT dataChanged(index, index, {SelectedRole});
            Q_EMIT selectionChanged();
            return true;
            
        case TagIdsRole:
            item.tagIds = value.value<QSet<qint64>>();
            Q_EMIT dataChanged(index, index, {TagIdsRole, HasTagsRole});
            return true;
    }

    return false;
}

Qt::ItemFlags ImageThumbnailModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QHash<int, QByteArray> ImageThumbnailModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[FilePathRole] = "filePath";
    roles[FileNameRole] = "fileName";
    roles[FileSizeRole] = "fileSize";
    roles[ModifiedDateRole] = "modifiedDate";
    roles[ThumbnailRole] = "thumbnail";
    roles[TagIdsRole] = "tagIds";
    roles[SelectedRole] = "selected";
    roles[HasTagsRole] = "hasTags";
    return roles;
}

// ============== Loading ==============

void ImageThumbnailModel::loadDirectory(const QString& path, bool recursive)
{
    Q_EMIT loadingStarted();
    
    beginResetModel();
    m_items.clear();
    m_pathToRow.clear();
    m_pendingThumbnails.clear();
    m_currentDir = path;
    
    scanDirectory(path, recursive);
    
    // Build path lookup
    for (int i = 0; i < m_items.size(); ++i) {
        m_pathToRow.insert(m_items[i].filePath, i);
    }
    
    endResetModel();
    
    Q_EMIT loadingFinished(m_items.size());
}

void ImageThumbnailModel::scanDirectory(const QString& path, bool recursive)
{
    QDir::Filters filters = QDir::Files | QDir::Readable;
    QDirIterator::IteratorFlags flags = recursive ? 
        QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags;
    
    QDirIterator it(path, filters, flags);
    
    while (it.hasNext()) {
        it.next();
        QFileInfo info = it.fileInfo();
        
        if (!ThumbnailCreator::isMediaFile(info.filePath())) {
            continue;
        }
        
        ImageItem item;
        item.filePath = info.filePath();
        item.fileName = info.fileName();
        item.fileSize = info.size();
        item.modifiedDate = info.lastModified();
        item.mediaType = ThumbnailCreator::getMediaType(info.filePath());
        
        // Load tags if TagManager is initialized
        if (TagManager::instance()->isInitialized()) {
            item.tagIds = TagManager::instance()->tagIdsForImage(item.filePath);
        }
        
        // Apply tag filter
        if (!matchesTagFilter(item)) {
            continue;
        }
        
        m_items.append(item);
    }
    
    // Sort by name by default
    std::sort(m_items.begin(), m_items.end(), [](const ImageItem& a, const ImageItem& b) {
        return a.fileName.compare(b.fileName, Qt::CaseInsensitive) < 0;
    });
}

void ImageThumbnailModel::loadFiles(const QStringList& filePaths)
{
    Q_EMIT loadingStarted();
    
    beginResetModel();
    m_items.clear();
    m_pathToRow.clear();
    m_pendingThumbnails.clear();
    m_currentDir.clear();
    
    for (const QString& path : filePaths) {
        QFileInfo info(path);
        if (!info.exists() || !ThumbnailCreator::isMediaFile(path)) {
            continue;
        }
        
        ImageItem item;
        item.filePath = info.filePath();
        item.fileName = info.fileName();
        item.fileSize = info.size();
        item.modifiedDate = info.lastModified();
        item.mediaType = ThumbnailCreator::getMediaType(path);
        
        if (TagManager::instance()->isInitialized()) {
            item.tagIds = TagManager::instance()->tagIdsForImage(item.filePath);
        }
        
        if (matchesTagFilter(item)) {
            m_pathToRow.insert(item.filePath, m_items.size());
            m_items.append(item);
        }
    }
    
    endResetModel();
    Q_EMIT loadingFinished(m_items.size());
}

void ImageThumbnailModel::clear()
{
    beginResetModel();
    m_items.clear();
    m_pathToRow.clear();
    m_pendingThumbnails.clear();
    m_currentDir.clear();
    endResetModel();
}

// ============== Item Access ==============

ImageItem ImageThumbnailModel::itemAt(int row) const
{
    if (row >= 0 && row < m_items.size()) {
        return m_items.at(row);
    }
    return ImageItem();
}

ImageItem ImageThumbnailModel::itemAt(const QModelIndex& index) const
{
    return itemAt(index.row());
}

int ImageThumbnailModel::indexOf(const QString& filePath) const
{
    return m_pathToRow.value(filePath, -1);
}

QModelIndex ImageThumbnailModel::indexForPath(const QString& filePath) const
{
    int row = indexOf(filePath);
    if (row >= 0) {
        return index(row);
    }
    return QModelIndex();
}

// ============== Selection ==============

void ImageThumbnailModel::setSelected(int row, bool selected)
{
    if (row >= 0 && row < m_items.size()) {
        setData(index(row), selected, SelectedRole);
    }
}

void ImageThumbnailModel::setSelected(const QModelIndex& index, bool selected)
{
    setData(index, selected, SelectedRole);
}

void ImageThumbnailModel::selectAll()
{
    for (int i = 0; i < m_items.size(); ++i) {
        m_items[i].selected = true;
    }
    Q_EMIT dataChanged(index(0), index(m_items.size() - 1), {SelectedRole});
    Q_EMIT selectionChanged();
}

void ImageThumbnailModel::clearSelection()
{
    for (int i = 0; i < m_items.size(); ++i) {
        m_items[i].selected = false;
    }
    Q_EMIT dataChanged(index(0), index(m_items.size() - 1), {SelectedRole});
    Q_EMIT selectionChanged();
}

QStringList ImageThumbnailModel::selectedPaths() const
{
    QStringList paths;
    for (const ImageItem& item : m_items) {
        if (item.selected) {
            paths.append(item.filePath);
        }
    }
    return paths;
}

QModelIndexList ImageThumbnailModel::selectedIndexes() const
{
    QModelIndexList indexes;
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].selected) {
            indexes.append(index(i));
        }
    }
    return indexes;
}

int ImageThumbnailModel::selectedCount() const
{
    int count = 0;
    for (const ImageItem& item : m_items) {
        if (item.selected) {
            ++count;
        }
    }
    return count;
}

// ============== Thumbnail Size ==============

void ImageThumbnailModel::setThumbnailSize(int size)
{
    if (m_thumbnailSize != size) {
        m_thumbnailSize = size;
        
        // Update placeholder
        m_loadingPixmap = QPixmap(size, size);
        m_loadingPixmap.fill(QColor(40, 40, 40));
        
        // Clear pending thumbnails and cached pixmaps - they'll be re-requested at new size
        m_pendingThumbnails.clear();
        for (ImageItem& item : m_items) {
            item.cachedPixmap = QPixmap();
            item.thumbnailLoaded = false;
        }
        
        // Notify view to refresh all items
        Q_EMIT dataChanged(index(0), index(m_items.size() - 1), {Qt::DecorationRole, ThumbnailRole});
    }
}

// ============== Tag Filtering ==============

void ImageThumbnailModel::setTagFilter(const QSet<qint64>& tagIds, bool requireAll)
{
    m_tagFilter = tagIds;
    m_requireAllTags = requireAll;
    
    // Reload with filter
    if (!m_currentDir.isEmpty()) {
        loadDirectory(m_currentDir);
    }
}

void ImageThumbnailModel::setShowUntagged(bool showUntagged)
{
    m_showUntagged = showUntagged;
    m_tagFilter.clear();  // Clear tag filter when showing untagged
    
    if (!m_currentDir.isEmpty()) {
        loadDirectory(m_currentDir);
    }
}

void ImageThumbnailModel::clearTagFilter()
{
    m_tagFilter.clear();
    m_requireAllTags = false;
    m_showUntagged = false;
    
    if (!m_currentDir.isEmpty()) {
        loadDirectory(m_currentDir);
    }
}

bool ImageThumbnailModel::matchesTagFilter(const ImageItem& item) const
{
    // Show only untagged images
    if (m_showUntagged) {
        return item.tagIds.isEmpty();
    }
    
    // No filter - show all
    if (m_tagFilter.isEmpty()) {
        return true;
    }
    
    if (m_requireAllTags) {
        // Must have all tags
        for (qint64 tagId : m_tagFilter) {
            if (!item.tagIds.contains(tagId)) {
                return false;
            }
        }
        return true;
    } else {
        // Must have at least one tag
        for (qint64 tagId : m_tagFilter) {
            if (item.tagIds.contains(tagId)) {
                return true;
            }
        }
        return false;
    }
}

// ============== Thumbnail Slots ==============

void ImageThumbnailModel::onThumbnailReady(const QString& filePath, const QPixmap& pixmap)
{
    m_pendingThumbnails.remove(filePath);
    
    int row = indexOf(filePath);
    if (row >= 0 && row < m_items.size()) {
        // Store the pixmap directly in the item to avoid cache lookups
        m_items[row].cachedPixmap = pixmap;
        m_items[row].thumbnailLoaded = true;
        
        QModelIndex idx = index(row);
        Q_EMIT dataChanged(idx, idx, {Qt::DecorationRole, ThumbnailRole});
        Q_EMIT thumbnailUpdated(idx);
    }
}

void ImageThumbnailModel::onThumbnailFailed(const QString& filePath)
{
    m_pendingThumbnails.remove(filePath);
    
    int row = indexOf(filePath);
    if (row >= 0) {
        QModelIndex idx = index(row);
        Q_EMIT dataChanged(idx, idx, {Qt::DecorationRole, ThumbnailRole});
    }
}

void ImageThumbnailModel::refreshThumbnail(const QString& filePath)
{
    // Remove from cache
    QString cacheKey = ThumbnailInfo::makeCacheKey(filePath, m_thumbnailSize);
    ThumbnailCache::instance()->removeImage(cacheKey);
    ThumbnailCache::instance()->removePixmap(cacheKey);
    
    // Re-request
    m_pendingThumbnails.remove(filePath);
    int row = indexOf(filePath);
    if (row >= 0) {
        QModelIndex idx = index(row);
        Q_EMIT dataChanged(idx, idx, {Qt::DecorationRole, ThumbnailRole});
    }
}

// ============== Tag Change Slots ==============

void ImageThumbnailModel::onImageTagged(const QString& imagePath, qint64 tagId)
{
    int row = indexOf(imagePath);
    if (row >= 0 && row < m_items.size()) {
        // Update the cached tag IDs
        m_items[row].tagIds.insert(tagId);
        
        // Emit dataChanged to update the view
        QModelIndex idx = index(row);
        Q_EMIT dataChanged(idx, idx, {TagIdsRole, HasTagsRole, TagListRole});
    }
}

void ImageThumbnailModel::onImageUntagged(const QString& imagePath, qint64 tagId)
{
    int row = indexOf(imagePath);
    if (row >= 0 && row < m_items.size()) {
        // Update the cached tag IDs
        m_items[row].tagIds.remove(tagId);
        
        // Emit dataChanged to update the view
        QModelIndex idx = index(row);
        Q_EMIT dataChanged(idx, idx, {TagIdsRole, HasTagsRole, TagListRole});
    }
}

} // namespace FullFrame

