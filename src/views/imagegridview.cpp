/**
 * ImageGridView implementation
 * 
 * Key performance optimizations (like DigiKam):
 * - Only request thumbnails for visible items
 * - Preload thumbnails just outside visible area
 * - Debounce preload requests during fast scrolling
 * - Efficient grid layout using QListView IconMode
 */

#include "imagegridview.h"
#include "imagethumbnailmodel.h"
#include "thumbnaildelegate.h"
#include "thumbnailloadthread.h"
#include "thumbnailcache.h"
#include "thumbnailcreator.h"

#include <QScrollBar>
#include <QWheelEvent>
#include <QContextMenuEvent>
#include <QApplication>
#include <QDebug>

namespace FullFrame {

ImageGridView::ImageGridView(QWidget* parent)
    : QListView(parent)
    , m_preloadTimer(new QTimer(this))
{
    // Create delegate first - setupView calls updateGridSize which uses m_delegate
    m_delegate = new ThumbnailDelegate(this);
    m_delegate->setThumbnailSize(m_thumbnailSize);
    m_delegate->setSpacing(m_spacing);
    m_delegate->setShowFilename(m_showFilenames);
    setItemDelegate(m_delegate);

    setupView();

    // Preload timer - debounce thumbnail requests during scrolling
    m_preloadTimer->setSingleShot(true);
    m_preloadTimer->setInterval(50);  // 50ms debounce
    connect(m_preloadTimer, &QTimer::timeout, this, &ImageGridView::preloadVisibleThumbnails);
    
    // Note: Selection model connection is done in setImageModel() after model is set
}

ImageGridView::~ImageGridView() = default;

void ImageGridView::setupView()
{
    // QListView settings for optimal grid performance with thousands of items
    setViewMode(QListView::IconMode);
    setFlow(QListView::LeftToRight);
    setWrapping(true);
    setResizeMode(QListView::Adjust);
    setLayoutMode(QListView::Batched);  // Key for performance!
    setBatchSize(100);  // Increased batch size for smoother scrolling with large collections

    // Movement and selection
    setMovement(QListView::Static);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSelectionBehavior(QAbstractItemView::SelectItems);

    // Scrolling
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    verticalScrollBar()->setSingleStep(20);

    // Appearance
    setUniformItemSizes(true);  // Important for performance!
    setSpacing(m_spacing);
    
    // Disable mouse tracking to prevent flickering on hover
    setMouseTracking(false);
    viewport()->setMouseTracking(false);
    
    // Reduce unnecessary repaints from hover effects
    viewport()->setAttribute(Qt::WA_Hover, false);
    setAttribute(Qt::WA_Hover, false);
    
    // Note: Don't use WA_NoSystemBackground - it causes stale content when filtering
    // The other optimizations (no hover, no mouse tracking) are sufficient
    
    // Style
    setStyleSheet(R"(
        QListView {
            background-color: #1e1e1e;
            border: none;
            outline: none;
        }
        QListView::item {
            background: transparent;
            border: none;
        }
        QListView::item:selected {
            background: transparent;
        }
        QListView::item:hover {
            background: transparent;
        }
        QScrollBar:vertical {
            background: #2d2d2d;
            width: 12px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: #5a5a5a;
            border-radius: 6px;
            min-height: 30px;
            margin: 2px;
        }
        QScrollBar::handle:vertical:hover {
            background: #6a6a6a;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
    )");

    updateGridSize();
}

// ============== Model ==============

void ImageGridView::setImageModel(ImageThumbnailModel* model)
{
    m_model = model;
    setModel(model);

    if (m_model) {
        connect(m_model, &ImageThumbnailModel::loadingFinished,
                this, [this](int count) {
                    Q_UNUSED(count)
                    m_preloadTimer->start();
                });
        
        // Force full viewport repaint when model is reset (e.g., after filtering)
        connect(m_model, &ImageThumbnailModel::loadingStarted,
                this, [this]() {
                    viewport()->update();
                });
        
        // Connect selection model AFTER model is set (selection model is created by setModel)
        if (selectionModel()) {
            connect(selectionModel(), &QItemSelectionModel::selectionChanged,
                    this, &ImageGridView::onSelectionChanged);
        }
    }
}

// ============== Thumbnail Size ==============

void ImageGridView::setThumbnailSize(int size)
{
    size = qBound(MinThumbnailSize, size, MaxThumbnailSize);
    
    if (m_thumbnailSize != size) {
        m_thumbnailSize = size;
        m_delegate->setThumbnailSize(size);
        
        if (m_model) {
            m_model->setThumbnailSize(size);
        }
        
        updateGridSize();
        Q_EMIT thumbnailSizeChanged(size);
        
        // Trigger preload at new size
        m_preloadTimer->start();
    }
}

int ImageGridView::thumbnailSize() const
{
    return m_thumbnailSize;
}

void ImageGridView::setItemSpacing(int spacing)
{
    m_spacing = spacing;
    m_delegate->setSpacing(spacing);
    setSpacing(spacing);
    updateGridSize();
}

int ImageGridView::itemSpacing() const
{
    return m_spacing;
}

void ImageGridView::setShowFilenames(bool show)
{
    m_showFilenames = show;
    m_delegate->setShowFilename(show);
    updateGridSize();
}

bool ImageGridView::showFilenames() const
{
    return m_showFilenames;
}

void ImageGridView::updateGridSize()
{
    QSize itemSize = m_delegate->sizeHint(QStyleOptionViewItem(), QModelIndex());
    setGridSize(itemSize);
}

// ============== Selection ==============

QStringList ImageGridView::selectedImagePaths() const
{
    QStringList paths;
    for (const QModelIndex& index : selectedIndexes()) {
        QString path = index.data(FilePathRole).toString();
        if (!path.isEmpty()) {
            paths.append(path);
        }
    }
    return paths;
}

void ImageGridView::scrollToImage(const QString& filePath)
{
    if (!m_model) return;
    
    QModelIndex index = m_model->indexForPath(filePath);
    if (index.isValid()) {
        scrollTo(index, QAbstractItemView::PositionAtCenter);
        setCurrentIndex(index);
    }
}

void ImageGridView::selectAll()
{
    QListView::selectAll();
}

void ImageGridView::clearSelection()
{
    QListView::clearSelection();
}

// ============== Zoom ==============

void ImageGridView::zoomIn()
{
    setThumbnailSize(m_thumbnailSize + ZoomStep);
}

void ImageGridView::zoomOut()
{
    setThumbnailSize(m_thumbnailSize - ZoomStep);
}

// ============== Event Handlers ==============

void ImageGridView::paintEvent(QPaintEvent* event)
{
    QListView::paintEvent(event);
    // Don't trigger preload on paint - only on scroll/resize
    // This prevents constant re-requests that cause flickering
}

void ImageGridView::resizeEvent(QResizeEvent* event)
{
    QListView::resizeEvent(event);
    m_preloadTimer->start();
}

void ImageGridView::scrollContentsBy(int dx, int dy)
{
    QListView::scrollContentsBy(dx, dy);
    
    // Restart preload timer on scroll
    m_preloadTimer->start();
}

void ImageGridView::wheelEvent(QWheelEvent* event)
{
    // Ctrl+Wheel zoom disabled - use slider instead
    // Just pass to base class for normal scrolling
    QListView::wheelEvent(event);
}

void ImageGridView::mousePressEvent(QMouseEvent* event)
{
    QListView::mousePressEvent(event);

    QModelIndex index = indexAt(event->pos());
    if (index.isValid()) {
        QString path = index.data(FilePathRole).toString();
        Q_EMIT imageSelected(path);
    }
}

void ImageGridView::mouseDoubleClickEvent(QMouseEvent* event)
{
    QModelIndex index = indexAt(event->pos());
    if (index.isValid()) {
        QString path = index.data(FilePathRole).toString();
        Q_EMIT imageActivated(path);
    }
    
    QListView::mouseDoubleClickEvent(event);
}

void ImageGridView::contextMenuEvent(QContextMenuEvent* event)
{
    QModelIndex index = indexAt(event->pos());
    QString path;
    if (index.isValid()) {
        path = index.data(FilePathRole).toString();
    }
    Q_EMIT contextMenuRequested(event->globalPos(), path);
}

void ImageGridView::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        QModelIndex index = currentIndex();
        if (index.isValid()) {
            QString path = index.data(FilePathRole).toString();
            Q_EMIT imageActivated(path);
            return;
        }
    }

    // Delete key
    if (event->key() == Qt::Key_Delete) {
        Q_EMIT deleteRequested();
        return;
    }

    // Ctrl+A for select all
    if (event->key() == Qt::Key_A && event->modifiers() & Qt::ControlModifier) {
        selectAll();
        return;
    }

    // Plus/Minus for zoom
    if (event->key() == Qt::Key_Plus || event->key() == Qt::Key_Equal) {
        zoomIn();
        return;
    }
    if (event->key() == Qt::Key_Minus) {
        zoomOut();
        return;
    }

    // Check for tag hotkeys (0-9, A-Z without Ctrl/Alt, F1-F12)
    QString hotkeyText;
    
    if (event->key() >= Qt::Key_0 && event->key() <= Qt::Key_9) {
        hotkeyText = QString::number(event->key() - Qt::Key_0);
    }
    else if (event->key() >= Qt::Key_A && event->key() <= Qt::Key_Z &&
             !(event->modifiers() & Qt::ControlModifier) &&
             !(event->modifiers() & Qt::AltModifier)) {
        hotkeyText = QChar('A' + (event->key() - Qt::Key_A));
    }
    else if (event->key() >= Qt::Key_F1 && event->key() <= Qt::Key_F12) {
        hotkeyText = QString("F%1").arg(event->key() - Qt::Key_F1 + 1);
    }
    
    if (!hotkeyText.isEmpty()) {
        Q_EMIT hotkeyPressed(hotkeyText);
        return;
    }

    QListView::keyPressEvent(event);
}

// ============== Selection Handling ==============

void ImageGridView::onSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
    Q_UNUSED(selected)
    Q_UNUSED(deselected)
    Q_EMIT selectionChanged(selectedImagePaths());
}

// ============== Thumbnail Preloading ==============

void ImageGridView::preloadVisibleThumbnails()
{
    if (!m_model || m_model->rowCount() == 0) {
        return;
    }

    // Get visible rect
    QRect visibleRect = viewport()->rect();
    
    // Find first and last visible indexes
    QModelIndex firstVisible = indexAt(visibleRect.topLeft());
    QModelIndex lastVisible = indexAt(visibleRect.bottomRight());

    if (!firstVisible.isValid()) {
        firstVisible = m_model->index(0);
    }
    if (!lastVisible.isValid()) {
        lastVisible = m_model->index(m_model->rowCount() - 1);
    }

    int firstRow = firstVisible.row();
    int lastRow = lastVisible.row();

    // Calculate columns for preload margin
    int columns = calculateColumnsForWidth(viewport()->width());
    int preloadItems = columns * m_preloadMargin;

    // Expand range for preloading
    int preloadStart = qMax(0, firstRow - preloadItems);
    int preloadEnd = qMin(m_model->rowCount() - 1, lastRow + preloadItems);

    // Request thumbnails for visible + margin items
    preloadThumbnails(preloadStart, preloadEnd);
}

void ImageGridView::preloadThumbnails(int startRow, int endRow)
{
    if (!m_model) return;

    QStringList pathsToLoad;
    
    for (int row = startRow; row <= endRow; ++row) {
        QModelIndex index = m_model->index(row);
        if (index.isValid()) {
            QString path = index.data(FilePathRole).toString();
            
            // Check if already cached - use consistent cache key format
            QString cacheKey = ThumbnailInfo::makeCacheKey(path, m_thumbnailSize);
            if (!ThumbnailCache::instance()->hasPixmap(cacheKey) &&
                !ThumbnailCache::instance()->hasImage(cacheKey)) {
                pathsToLoad.append(path);
            }
        }
    }

    if (!pathsToLoad.isEmpty()) {
        // Load in batches with normal priority
        ThumbnailLoadThread::instance()->loadBatch(pathsToLoad, m_thumbnailSize);
    }
}

QModelIndexList ImageGridView::visibleIndexes() const
{
    QModelIndexList indexes;
    if (!model() || model()->rowCount() == 0) {
        return indexes;
    }
    
    QRect visibleRect = viewport()->rect();
    
    // Use binary search to find first visible item for O(log n) instead of O(n)
    int totalRows = model()->rowCount();
    int low = 0, high = totalRows - 1;
    int firstVisible = 0;
    
    // Binary search for first visible row
    while (low <= high) {
        int mid = (low + high) / 2;
        QModelIndex midIndex = model()->index(mid, 0);
        QRect midRect = visualRect(midIndex);
        
        if (midRect.bottom() < visibleRect.top()) {
            low = mid + 1;
        } else if (midRect.top() > visibleRect.bottom()) {
            high = mid - 1;
        } else {
            // midRect intersects visibleRect, search for first
            firstVisible = mid;
            high = mid - 1;
        }
    }
    
    // If binary search didn't find intersection, use low as starting point
    if (firstVisible == 0 && low > 0) {
        firstVisible = qMin(low, totalRows - 1);
    }
    
    // Collect all visible items from firstVisible onwards
    for (int row = firstVisible; row < totalRows; ++row) {
        QModelIndex index = model()->index(row, 0);
        QRect itemRect = visualRect(index);
        
        // Stop when we're past the visible area
        if (itemRect.top() > visibleRect.bottom()) {
            break;
        }
        
        if (visibleRect.intersects(itemRect)) {
            indexes.append(index);
        }
    }

    return indexes;
}

int ImageGridView::calculateColumnsForWidth(int width) const
{
    QSize itemSize = gridSize();
    if (itemSize.width() <= 0) {
        return 1;
    }
    return qMax(1, width / itemSize.width());
}

} // namespace FullFrame

