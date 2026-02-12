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
#include <algorithm>

namespace FullFrame {

ImageThumbnailModel::ImageThumbnailModel(QObject* parent)
    : QAbstractListModel(parent)
{
    connectThumbnailThread();
    connectTagManager();
    
    // Batch thumbnail dataChanged signals — instead of firing per-thumbnail
    // (which floods the UI event loop during active loading), accumulate
    // dirty rows and flush every 150ms in a single batch.
    m_thumbBatchTimer = new QTimer(this);
    m_thumbBatchTimer->setSingleShot(true);
    m_thumbBatchTimer->setInterval(150);
    connect(m_thumbBatchTimer, &QTimer::timeout, this, &ImageThumbnailModel::flushThumbnailUpdates);
    
    // Create placeholder pixmaps
    m_loadingPixmap = QPixmap(m_thumbnailSize, m_thumbnailSize);
    m_loadingPixmap.fill(QColor(40, 40, 40));
    
    m_errorPixmap = QPixmap(m_thumbnailSize, m_thumbnailSize);
    m_errorPixmap.fill(QColor(60, 40, 40));
}

ImageThumbnailModel::~ImageThumbnailModel() = default;

void ImageThumbnailModel::connectThumbnailThread()
{
    // Connect to the lightweight thumbnailAvailable signal instead of
    // thumbnailReady.  thumbnailReady creates a QPixmap on the main thread
    // for every completed thumbnail — during initial loading this burst of
    // QPixmap::fromImage() calls starves the event loop.
    // thumbnailAvailable just tells us "the QImage is in the image cache";
    // we defer the QImage→QPixmap conversion to data()'s paint-time path
    // where only visible items are converted.
    connect(ThumbnailLoadThread::instance(), &ThumbnailLoadThread::thumbnailAvailable,
            this, &ImageThumbnailModel::onThumbnailAvailable);
    connect(ThumbnailLoadThread::instance(), &ThumbnailLoadThread::thumbnailFailed,
            this, &ImageThumbnailModel::onThumbnailFailed);
}

void ImageThumbnailModel::connectTagManager()
{
    connect(TagManager::instance(), &TagManager::imageTagged,
            this, &ImageThumbnailModel::onImageTagged);
    connect(TagManager::instance(), &TagManager::imageUntagged,
            this, &ImageThumbnailModel::onImageUntagged);
    connect(TagManager::instance(), &TagManager::tagRenamed,
            this, &ImageThumbnailModel::onTagRenamed);
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
            
            // Return cached tag list if still valid (avoids per-paint heap allocations)
            if (!item.tagListDirty) {
                return item.cachedTagList;
            }
            
            // Rebuild and cache
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
            item.cachedTagList = tagList;
            item.tagListDirty = false;
            return tagList;
        }
        
        case MediaTypeRole:
            return static_cast<int>(item.mediaType);
            
        case IsFavoritedRole:
            return isFavorited(item.filePath);
            
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
    m_allItems.clear();
    m_pathToRow.clear();
    m_pendingThumbnails.clear();
    m_thumbDirtyRows.clear();
    m_currentDir = path;
    
    scanDirectory(path, recursive);
    
    // scanDirectory now populates m_allItems with all items (after tag filter)
    // and m_items with items after album file filter. We need to apply
    // album file filter to m_allItems to get m_items, then apply filename filter.
    
    // Apply album file filter (but always show favorites)
    m_items.clear();
    for (const ImageItem& item : m_allItems) {
        if (m_showAlbumFiles || !isInAlbumFolder(item.filePath) || isFavorited(item.filePath)) {
            m_items.append(item);
        }
    }
    
    // Apply filename filter if one is active
    if (!m_filenameFilter.isEmpty()) {
        QList<ImageItem> filenameFiltered;
        for (const ImageItem& item : m_items) {
            if (item.fileName.contains(m_filenameFilter, Qt::CaseInsensitive)) {
                filenameFiltered.append(item);
            }
        }
        m_items = filenameFiltered;
    }
    
    // Build path lookup
    m_pathToRow.clear();
    for (int i = 0; i < m_items.size(); ++i) {
        m_pathToRow.insert(m_items[i].filePath, i);
    }
    
    endResetModel();
    
    Q_EMIT loadingFinished(m_items.size());
}

void ImageThumbnailModel::scanDirectory(const QString& path, bool recursive)
{
    QDir rootDir(path);
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
        // Show relative path (e.g. "album/photo.jpg") so subfolder files
        // are distinguishable from files in the root folder.
        item.fileName = rootDir.relativeFilePath(info.filePath());
        item.fileSize = info.size();
        item.modifiedDate = info.lastModified();
        item.mediaType = ThumbnailCreator::getMediaType(info.filePath());
        
        // Load tags if TagManager is initialized
        if (TagManager::instance()->isInitialized()) {
            item.tagIds = TagManager::instance()->tagIdsForImage(item.filePath);
        }
        
        // Apply tag filter only - add all matching items to m_allItems
        // Album file filtering will be applied later in loadDirectory/applyFilenameFilter
        if (matchesTagFilter(item)) {
            m_allItems.append(item);
        }
    }
    
    // Sort by name by default
    std::sort(m_allItems.begin(), m_allItems.end(), [](const ImageItem& a, const ImageItem& b) {
        return a.fileName.compare(b.fileName, Qt::CaseInsensitive) < 0;
    });
}

void ImageThumbnailModel::loadFiles(const QStringList& filePaths)
{
    Q_EMIT loadingStarted();
    
    beginResetModel();
    m_items.clear();
    m_allItems.clear();
    m_pathToRow.clear();
    m_pendingThumbnails.clear();
    m_thumbDirtyRows.clear();
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
            m_allItems.append(item);
        }
    }
    
    // Apply album file filter (but always show favorites)
    QList<ImageItem> filteredItems;
    for (const ImageItem& item : m_allItems) {
        if (m_showAlbumFiles || !isInAlbumFolder(item.filePath) || isFavorited(item.filePath)) {
            filteredItems.append(item);
        }
    }
    
    // Apply filename filter
    if (m_filenameFilter.isEmpty()) {
        m_items = filteredItems;
    } else {
        for (const ImageItem& item : filteredItems) {
            if (item.fileName.contains(m_filenameFilter, Qt::CaseInsensitive)) {
                m_items.append(item);
            }
        }
    }
    
    // Build path lookup
    for (int i = 0; i < m_items.size(); ++i) {
        m_pathToRow.insert(m_items[i].filePath, i);
    }
    
    endResetModel();
    Q_EMIT loadingFinished(m_items.size());
}

void ImageThumbnailModel::clear()
{
    beginResetModel();
    m_items.clear();
    m_allItems.clear();
    m_pathToRow.clear();
    m_pendingThumbnails.clear();
    m_thumbDirtyRows.clear();
    m_currentDir.clear();
    endResetModel();
}

QStringList ImageThumbnailModel::allFilePaths() const
{
    QStringList paths;
    paths.reserve(m_allItems.size());
    for (const ImageItem& item : m_allItems) {
        paths.append(item.filePath);
    }
    return paths;
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

// ============== Album File Filtering ==============

void ImageThumbnailModel::setShowAlbumFiles(bool show)
{
    if (m_showAlbumFiles == show) {
        return;  // No change
    }
    m_showAlbumFiles = show;
    applyFilenameFilter();  // Reapply filters
}

bool ImageThumbnailModel::isInAlbumFolder(const QString& filePath) const
{
    if (!TagManager::instance()->isInitialized()) {
        return false;
    }
    
    QFileInfo fileInfo(filePath);
    QString fileDir = QDir(fileInfo.absolutePath()).absolutePath();
    
    // Check if the file's directory matches any album path
    QList<Tag> allTags = TagManager::instance()->allTags();
    for (const Tag& tag : allTags) {
        if (tag.isAlbumTag()) {
            QString albumPath = QDir(tag.albumPath).absolutePath();
            if (fileDir == albumPath) {
                return true;
            }
        }
    }
    
    return false;
}

bool ImageThumbnailModel::isFavorited(const QString& filePath) const
{
    // Check favorites set (separate from tags)
    return m_favorites.contains(filePath);
}

void ImageThumbnailModel::setFavorites(const QSet<QString>& favorites)
{
    m_favorites = favorites;
    // Refresh the view to update filtering (this will also update star icons via endResetModel)
    applyFilenameFilter();
    // After filtering, trigger dataChanged for visible items to update star icons
    if (rowCount() > 0) {
        QModelIndex topLeft = index(0, 0);
        QModelIndex bottomRight = index(rowCount() - 1, 0);
        Q_EMIT dataChanged(topLeft, bottomRight, {IsFavoritedRole});
    }
}

// ============== Filename Filtering ==============

void ImageThumbnailModel::setFilenameFilter(const QString& filter)
{
    QString trimmed = filter.trimmed();
    if (m_filenameFilter == trimmed) {
        return;  // No change
    }
    m_filenameFilter = trimmed;
    applyFilenameFilter();
}

void ImageThumbnailModel::applyFilenameFilter()
{
    beginResetModel();
    m_items.clear();
    m_pathToRow.clear();
    m_pendingThumbnails.clear();
    m_thumbDirtyRows.clear();
    
    // Apply album file filter first (but always show favorites)
    QList<ImageItem> filteredItems;
    for (const ImageItem& item : m_allItems) {
        if (m_showAlbumFiles || !isInAlbumFolder(item.filePath) || isFavorited(item.filePath)) {
            filteredItems.append(item);
        }
    }
    
    if (m_filenameFilter.isEmpty()) {
        // No filename filter — show everything after album filter
        m_items = filteredItems;
    } else {
        // Filter by filename (case-insensitive substring match)
        for (const ImageItem& item : filteredItems) {
            if (item.fileName.contains(m_filenameFilter, Qt::CaseInsensitive)) {
                m_items.append(item);
            }
        }
    }
    
    // Rebuild path lookup
    for (int i = 0; i < m_items.size(); ++i) {
        m_pathToRow.insert(m_items[i].filePath, i);
    }
    
    endResetModel();
    Q_EMIT loadingFinished(m_items.size());
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

void ImageThumbnailModel::onThumbnailAvailable(const QString& filePath)
{
    m_pendingThumbnails.remove(filePath);
    
    int row = indexOf(filePath);
    if (row >= 0 && row < m_items.size()) {
        // DON'T store a pixmap here — the QImage is already in the image cache
        // (put there by the worker thread).  When the view repaints, data()
        // will find it in the image cache and do a lazy QPixmap::fromImage()
        // only for the ~20-30 items actually visible on screen.
        //
        // Eagerly converting every completed thumbnail to a QPixmap would
        // monopolise the main thread during the initial loading burst,
        // starving wheel-event processing and causing scroll lag at the top.
        m_thumbDirtyRows.append(row);
        if (!m_thumbBatchTimer->isActive()) {
            m_thumbBatchTimer->start();
        }
    }
}

void ImageThumbnailModel::onThumbnailFailed(const QString& filePath)
{
    m_pendingThumbnails.remove(filePath);
    // Don't emit dataChanged for failures — the placeholder doesn't change,
    // so repainting would just redraw the same loading placeholder.
}

void ImageThumbnailModel::flushThumbnailUpdates()
{
    if (m_thumbDirtyRows.isEmpty()) return;
    
    // Emit ONE ranged dataChanged covering all dirty rows.
    // Previously we emitted one signal per dirty row; each signal makes Qt
    // call visualRect() twice (topLeft, bottomRight) to compute the dirty
    // region.  With 50+ thumbnails completing in a single batch that meant
    // 100+ visualRect() calls, measurable overhead during scroll.
    // A single range signal reduces that to exactly 2 visualRect() calls.
    int minRow = *std::min_element(m_thumbDirtyRows.begin(), m_thumbDirtyRows.end());
    int maxRow = *std::max_element(m_thumbDirtyRows.begin(), m_thumbDirtyRows.end());
    m_thumbDirtyRows.clear();
    
    minRow = qMax(0, minRow);
    maxRow = qMin(maxRow, m_items.size() - 1);
    
    if (minRow <= maxRow) {
        Q_EMIT dataChanged(index(minRow), index(maxRow), {Qt::DecorationRole, ThumbnailRole});
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
        m_items[row].tagIds.insert(tagId);
        m_items[row].tagListDirty = true;  // Invalidate cached tag display data
        
        QModelIndex idx = index(row);
        Q_EMIT dataChanged(idx, idx, {TagIdsRole, HasTagsRole, TagListRole});
    }
}

void ImageThumbnailModel::onImageUntagged(const QString& imagePath, qint64 tagId)
{
    int row = indexOf(imagePath);
    if (row >= 0 && row < m_items.size()) {
        m_items[row].tagIds.remove(tagId);
        m_items[row].tagListDirty = true;  // Invalidate cached tag display data
        
        QModelIndex idx = index(row);
        Q_EMIT dataChanged(idx, idx, {TagIdsRole, HasTagsRole, TagListRole});
    }
}

void ImageThumbnailModel::onTagRenamed(qint64 tagId, const QString& newName)
{
    Q_UNUSED(newName)
    
    // Invalidate and repaint tag badges for every item that has this tag
    for (int row = 0; row < m_items.size(); ++row) {
        if (m_items[row].tagIds.contains(tagId)) {
            m_items[row].tagListDirty = true;
            QModelIndex idx = index(row);
            Q_EMIT dataChanged(idx, idx, {TagListRole});
        }
    }
}

} // namespace FullFrame

