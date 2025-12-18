#include "imagegridview.h"
#include "imagethumbnailmodel.h"
#include <QWheelEvent>
#include <QScrollBar>

namespace FullFrame {

ImageGridView::ImageGridView(QWidget* parent)
    : QListView(parent)
{
    setViewMode(QListView::IconMode);
    setFlow(QListView::LeftToRight);
    setWrapping(true);
    setResizeMode(QListView::Adjust);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setUniformItemSizes(true);
    setSpacing(8);
    
    updateGridSize();
    
    setStyleSheet(R"(
        QListView {
            background-color: #1e1e1e;
            border: none;
        }
        QListView::item:selected {
            background-color: transparent;
        }
    )");
}

ImageGridView::~ImageGridView() = default;

void ImageGridView::setThumbnailSize(int size)
{
    m_thumbnailSize = qBound(64, size, 512);
    updateGridSize();
}

void ImageGridView::wheelEvent(QWheelEvent* event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        int delta = event->angleDelta().y() > 0 ? 32 : -32;
        setThumbnailSize(m_thumbnailSize + delta);
        event->accept();
    } else {
        QListView::wheelEvent(event);
    }
}

void ImageGridView::selectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
    QListView::selectionChanged(selected, deselected);
    
    QStringList paths;
    for (const QModelIndex& idx : selectedIndexes()) {
        QString path = idx.data(Qt::UserRole + 1).toString();
        if (!path.isEmpty()) {
            paths.append(path);
        }
    }
    
    Q_EMIT selectionChanged(paths);
    
    if (paths.count() == 1) {
        Q_EMIT imageSelected(paths.first());
    }
}

void ImageGridView::mouseDoubleClickEvent(QMouseEvent* event)
{
    QModelIndex idx = indexAt(event->pos());
    if (idx.isValid()) {
        QString path = idx.data(Qt::UserRole + 1).toString();
        if (!path.isEmpty()) {
            Q_EMIT imageDoubleClicked(path);
        }
    }
    QListView::mouseDoubleClickEvent(event);
}

void ImageGridView::updateGridSize()
{
    int cellSize = m_thumbnailSize + 40;
    setGridSize(QSize(cellSize, cellSize));
    setIconSize(QSize(m_thumbnailSize, m_thumbnailSize));
}

} // namespace FullFrame
