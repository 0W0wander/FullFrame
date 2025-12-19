#include "thumbnaildelegate.h"
#include <QPainter>

namespace FullFrame {

ThumbnailDelegate::ThumbnailDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

ThumbnailDelegate::~ThumbnailDelegate() = default;

void ThumbnailDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                              const QModelIndex& index) const
{
    painter->save();
    
    bool selected = option.state & QStyle::State_Selected;
    
    // Background
    if (selected) {
        painter->fillRect(option.rect, QColor(0, 120, 215, 50));
        painter->setPen(QPen(QColor(0, 120, 215), 2));
        painter->drawRect(option.rect.adjusted(1, 1, -1, -1));
    }
    
    // Thumbnail
    QVariant thumbData = index.data(Qt::DecorationRole);
    if (thumbData.canConvert<QPixmap>()) {
        QPixmap pixmap = thumbData.value<QPixmap>();
        QRect thumbRect = option.rect.adjusted(4, 4, -4, -24);
        paintThumbnail(painter, thumbRect, pixmap, selected);
    }
    
    // File name
    QString name = index.data(Qt::DisplayRole).toString();
    QRect nameRect = option.rect;
    nameRect.setTop(nameRect.bottom() - 20);
    paintFileName(painter, nameRect, name, selected);
    
    painter->restore();
}

QSize ThumbnailDelegate::sizeHint(const QStyleOptionViewItem& option,
                                   const QModelIndex& index) const
{
    Q_UNUSED(index)
    return option.decorationSize + QSize(8, 28);
}

void ThumbnailDelegate::paintThumbnail(QPainter* painter, const QRect& rect,
                                        const QPixmap& pixmap, bool selected) const
{
    Q_UNUSED(selected)
    
    if (pixmap.isNull()) {
        painter->fillRect(rect, QColor(45, 45, 45));
        return;
    }
    
    QPixmap scaled = pixmap.scaled(rect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    int x = rect.x() + (rect.width() - scaled.width()) / 2;
    int y = rect.y() + (rect.height() - scaled.height()) / 2;
    painter->drawPixmap(x, y, scaled);
}

void ThumbnailDelegate::paintFileName(QPainter* painter, const QRect& rect,
                                       const QString& name, bool selected) const
{
    painter->setPen(selected ? QColor(255, 255, 255) : QColor(180, 180, 180));
    QFont font = painter->font();
    font.setPointSize(9);
    painter->setFont(font);
    
    QString elidedName = painter->fontMetrics().elidedText(name, Qt::ElideMiddle, rect.width() - 8);
    painter->drawText(rect, Qt::AlignCenter, elidedName);
}

} // namespace FullFrame
