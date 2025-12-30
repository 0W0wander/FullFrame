/**
 * ImageGridView - High-performance grid view for images
 * 
 * Based on DigiKam's DCategorizedView/ItemViewCategorized:
 * - QListView with IconMode for grid layout
 * - Lazy loading of thumbnails (only visible items)
 * - Preloading of nearby items for smooth scrolling
 * - Efficient scrolling with thousands of items
 */

#pragma once

#include <QListView>
#include <QTimer>

namespace FullFrame {

class ImageThumbnailModel;
class ThumbnailDelegate;

/**
 * Optimized grid view for displaying image thumbnails
 */
class ImageGridView : public QListView
{
    Q_OBJECT

public:
    explicit ImageGridView(QWidget* parent = nullptr);
    ~ImageGridView() override;

    // Model access
    void setImageModel(ImageThumbnailModel* model);
    ImageThumbnailModel* imageModel() const { return m_model; }

    // Thumbnail size
    void setThumbnailSize(int size);
    int thumbnailSize() const;

    // Spacing
    void setItemSpacing(int spacing);
    int itemSpacing() const;

    // Display options
    void setShowFilenames(bool show);
    bool showFilenames() const;

    // Get selected paths
    QStringList selectedImagePaths() const;

    // Scroll to specific image
    void scrollToImage(const QString& filePath);

Q_SIGNALS:
    void imageActivated(const QString& filePath);
    void imageSelected(const QString& filePath);
    void selectionChanged(const QStringList& selectedPaths);
    void contextMenuRequested(const QPoint& globalPos, const QString& filePath);
    void thumbnailSizeChanged(int size);
    void deleteRequested();
    void hotkeyPressed(const QString& key);

public Q_SLOTS:
    void zoomIn();
    void zoomOut();
    void selectAll();
    void clearSelection();

protected:
    // Override for lazy loading optimization
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private Q_SLOTS:
    void onSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
    void preloadVisibleThumbnails();
    void updateGridSize();

private:
    void setupView();
    void preloadThumbnails(int startRow, int endRow);
    QModelIndexList visibleIndexes() const;
    int calculateColumnsForWidth(int width) const;

private:
    ImageThumbnailModel* m_model = nullptr;
    ThumbnailDelegate* m_delegate = nullptr;

    int m_thumbnailSize = 256;
    int m_spacing = 8;
    bool m_showFilenames = true;

    // Preloading
    QTimer* m_preloadTimer;
    int m_preloadMargin = 3;  // Increased rows to preload above/below for smoother scrolling

    // Zoom limits
    static constexpr int MinThumbnailSize = 64;
    static constexpr int MaxThumbnailSize = 512;
    static constexpr int ZoomStep = 32;
};

} // namespace FullFrame

