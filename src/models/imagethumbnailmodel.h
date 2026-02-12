/**
 * ImageThumbnailModel - Model for image/video/audio thumbnails
 * 
 * Based on DigiKam's ItemModel pattern:
 * - Provides data for QListView-based views
 * - Integrates with ThumbnailLoadThread for async loading
 * - Supports filtering and sorting
 * - Efficient for thousands of media files
 */

#pragma once

#include <QAbstractListModel>
#include <QFileInfo>
#include <QPixmap>
#include <QSet>
#include <QDir>
#include <QTimer>

#include "thumbnailcreator.h"

namespace FullFrame {

/**
 * Data for a single media item (image, video, or audio)
 */
struct ImageItem
{
    QString filePath;
    QString fileName;
    qint64 fileSize = 0;
    QDateTime modifiedDate;
    QSet<qint64> tagIds;
    bool selected = false;
    MediaType mediaType = MediaType::Unknown;
    
    // Cached thumbnail to avoid repeated lookups
    mutable QPixmap cachedPixmap;
    mutable bool thumbnailLoaded = false;
    
    // Cached tag display data to avoid QVariantList/QVariantMap allocations per paint
    mutable QVariantList cachedTagList;
    mutable bool tagListDirty = true;
    
    bool isValid() const { return !filePath.isEmpty(); }
    bool isImage() const { return mediaType == MediaType::Image; }
    bool isVideo() const { return mediaType == MediaType::Video; }
    bool isAudio() const { return mediaType == MediaType::Audio; }
};

/**
 * Custom roles for media data
 */
enum ImageRole
{
    FilePathRole = Qt::UserRole + 1,
    FileNameRole,
    FileSizeRole,
    ModifiedDateRole,
    ThumbnailRole,
    TagIdsRole,
    SelectedRole,
    HasTagsRole,
    TagListRole,      // Returns QVariantList of tag info (name, color)
    MediaTypeRole     // Returns MediaType enum value
};

/**
 * Model providing image items with thumbnails
 */
class ImageThumbnailModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit ImageThumbnailModel(QObject* parent = nullptr);
    ~ImageThumbnailModel() override;

    // QAbstractListModel interface
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Load images from directory
    void loadDirectory(const QString& path, bool recursive = false);
    void loadFiles(const QStringList& filePaths);
    void clear();
    
    // Get current directory
    QString currentDirectory() const { return m_currentDir; }
    
    // Get all file paths (before filters, for tag counting)
    QStringList allFilePaths() const;
    
    // Access items
    ImageItem itemAt(int row) const;
    ImageItem itemAt(const QModelIndex& index) const;
    int indexOf(const QString& filePath) const;
    QModelIndex indexForPath(const QString& filePath) const;
    
    // Selection
    void setSelected(int row, bool selected);
    void setSelected(const QModelIndex& index, bool selected);
    void selectAll();
    void clearSelection();
    QStringList selectedPaths() const;
    QModelIndexList selectedIndexes() const;
    int selectedCount() const;
    
    // Thumbnail size
    void setThumbnailSize(int size);
    int thumbnailSize() const { return m_thumbnailSize; }
    
    // Tag filtering
    void setTagFilter(const QSet<qint64>& tagIds, bool requireAll = false);
    void setShowUntagged(bool showUntagged);
    void clearTagFilter();
    
    // Filename filtering (in-memory, instant — no disk I/O)
    void setFilenameFilter(const QString& filter);

Q_SIGNALS:
    void loadingStarted();
    void loadingFinished(int count);
    void thumbnailUpdated(const QModelIndex& index);
    void selectionChanged();

public Q_SLOTS:
    void refreshThumbnail(const QString& filePath);

private Q_SLOTS:
    void onThumbnailAvailable(const QString& filePath);
    void onThumbnailFailed(const QString& filePath);
    void flushThumbnailUpdates();
    void onImageTagged(const QString& imagePath, qint64 tagId);
    void onImageUntagged(const QString& imagePath, qint64 tagId);
    void onTagRenamed(qint64 tagId, const QString& newName);

private:
    void connectThumbnailThread();
    void connectTagManager();
    void requestThumbnail(int row) const;
    void scanDirectory(const QString& path, bool recursive);
    bool matchesTagFilter(const ImageItem& item) const;
    void applyFilenameFilter();

private:
    QList<ImageItem> m_items;         // Currently visible items (after all filters)
    QList<ImageItem> m_allItems;      // All items after tag filter (before filename filter)
    QHash<QString, int> m_pathToRow;
    QString m_currentDir;
    
    int m_thumbnailSize = 256;
    mutable QSet<QString> m_pendingThumbnails;
    
    // Tag filter
    QSet<qint64> m_tagFilter;
    bool m_requireAllTags = false;
    bool m_showUntagged = false;
    
    // Filename filter (applied in-memory on top of tag filter)
    QString m_filenameFilter;
    
    // Thumbnail update batching — reduces UI thread pressure during active loading
    QTimer* m_thumbBatchTimer = nullptr;
    QVector<int> m_thumbDirtyRows;
    
    // Placeholder pixmaps
    QPixmap m_loadingPixmap;
    QPixmap m_errorPixmap;
};

} // namespace FullFrame

