/**
 * ThumbnailDelegate implementation
 * 
 * Optimized painting for smooth scrolling
 */

#include "thumbnaildelegate.h"
#include "imagethumbnailmodel.h"
#include "tagmanager.h"

#include <QPainter>
#include <QApplication>
#include <QPainterPath>
#include <cmath>

namespace FullFrame {

ThumbnailDelegate::ThumbnailDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
    , m_selectionColor(0, 120, 215)      // Windows accent blue
    , m_hoverColor(255, 255, 255, 30)    // Subtle white overlay
    , m_tagIndicatorColor(76, 175, 80)   // Material green
    , m_backgroundColor(30, 30, 30)       // Dark background
    , m_textColor(200, 200, 200)         // Light gray text
    , m_filenameFM(QFont())
    , m_badgeFM(QFont())
{
    // Pre-create fonts and metrics once — avoids heap allocations during paint
    m_filenameFont = QFont();
    m_filenameFont.setPointSize(9);
    m_filenameFM = QFontMetrics(m_filenameFont);

    m_badgeFont = QFont();
    m_badgeFont.setPointSize(8);
    m_badgeFont.setBold(true);
    m_badgeFM = QFontMetrics(m_badgeFont);
}

ThumbnailDelegate::~ThumbnailDelegate() = default;

void ThumbnailDelegate::setThumbnailSize(int size)
{
    if (m_thumbnailSize != size) {
        m_thumbnailSize = size;
        Q_EMIT sizeHintChanged();
    }
}

void ThumbnailDelegate::setSpacing(int spacing)
{
    if (m_spacing != spacing) {
        m_spacing = spacing;
        Q_EMIT sizeHintChanged();
    }
}

void ThumbnailDelegate::setShowFilename(bool show)
{
    if (m_showFilename != show) {
        m_showFilename = show;
        Q_EMIT sizeHintChanged();
    }
}

void ThumbnailDelegate::setShowTagIndicator(bool show)
{
    m_showTagIndicator = show;
}

// ============== Painting ==============

void ThumbnailDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                               const QModelIndex& index) const
{
    painter->save();
    
    // Background
    QRect itemRect = option.rect;
    painter->fillRect(itemRect, m_backgroundColor);

    // Calculate thumbnail rect (centered with spacing)
    int thumbX = itemRect.x() + m_spacing;
    int thumbY = itemRect.y() + m_spacing;
    QRect thumbRect(thumbX, thumbY, m_thumbnailSize, m_thumbnailSize);

    // Get thumbnail pixmap - use type() check to avoid expensive canConvert
    QVariant thumbVar = index.data(Qt::DecorationRole);
    if (thumbVar.userType() == QMetaType::QPixmap) {
        QPixmap pixmap = thumbVar.value<QPixmap>();
        if (!pixmap.isNull()) {
            // No SmoothPixmapTransform — thumbnails are already created at
            // the target size, so drawPixmap is a 1:1 blit (no scaling).
            paintThumbnail(painter, thumbRect, pixmap);
        } else {
            painter->fillRect(thumbRect, QColor(50, 50, 50));
        }
    }

    // Selection effects
    bool selected = option.state & QStyle::State_Selected;
    if (selected) {
        painter->setRenderHint(QPainter::Antialiasing, true);
        paintSelection(painter, thumbRect, option);
        painter->setRenderHint(QPainter::Antialiasing, false);
    }

    // Check if item is favorited (using favorites set, not tags)
    bool isFavorited = index.data(IsFavoritedRole).toBool();
    
    // Tag badges — only query model if item has tags (fast HasTagsRole check first)
    if (index.data(HasTagsRole).toBool()) {
        QVariantList tagList = index.data(TagListRole).toList();
        if (!tagList.isEmpty()) {
            painter->setRenderHint(QPainter::Antialiasing, true);
            paintTagBadges(painter, thumbRect, tagList);
            painter->setRenderHint(QPainter::Antialiasing, false);
        }
    }
    
    // Draw favorite star icon (always show if favorited, even if no other tags)
    if (isFavorited) {
        painter->setRenderHint(QPainter::Antialiasing, true);
        paintFavoriteStar(painter, thumbRect);
        painter->setRenderHint(QPainter::Antialiasing, false);
    }
    
    // Draw rating indicator (top-right corner)
    int ratingValue = index.data(RatingRole).toInt();
    if (ratingValue > 0) {
        painter->setRenderHint(QPainter::Antialiasing, true);
        paintRating(painter, thumbRect, ratingValue);
        painter->setRenderHint(QPainter::Antialiasing, false);
    }

    // Filename
    if (m_showFilename) {
        QRect filenameRect(
            itemRect.x() + m_spacing,
            itemRect.y() + m_spacing + m_thumbnailSize + 4,
            m_thumbnailSize,
            m_filenameHeight
        );
        QString filename = index.data(FileNameRole).toString();
        paintFilename(painter, filenameRect, filename);
    }

    painter->restore();
}

void ThumbnailDelegate::paintThumbnail(QPainter* painter, const QRect& rect,
                                        const QPixmap& pixmap) const
{
    // Calculate scaled size maintaining aspect ratio
    QSize pixmapSize = pixmap.size();
    QSize targetSize = pixmapSize.scaled(rect.size(), Qt::KeepAspectRatio);
    
    // Center in rect
    int x = rect.x() + (rect.width() - targetSize.width()) / 2;
    int y = rect.y() + (rect.height() - targetSize.height()) / 2;
    QRect targetRect(x, y, targetSize.width(), targetSize.height());

    // Draw pixmap directly — no QPainterPath clip (major perf bottleneck removed).
    // The tiny 4px rounded corners aren't worth the CPU cost of path clipping
    // on every visible item during every scroll repaint.
    painter->drawPixmap(targetRect, pixmap);
}

void ThumbnailDelegate::paintSelection(QPainter* painter, const QRect& rect,
                                        const QStyleOptionViewItem& option) const
{
    bool selected = option.state & QStyle::State_Selected;
    // Disabled hover effect to prevent flickering
    // bool hovered = option.state & QStyle::State_MouseOver;

    if (selected) {
        // Selection border
        painter->setPen(QPen(m_selectionColor, 3));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(rect.adjusted(-2, -2, 2, 2), 6, 6);
        
        // Selection overlay
        painter->fillRect(rect, QColor(m_selectionColor.red(), 
                                       m_selectionColor.green(), 
                                       m_selectionColor.blue(), 40));
    }
    // Hover effect disabled to prevent flickering
}

void ThumbnailDelegate::paintFilename(QPainter* painter, const QRect& rect,
                                       const QString& filename) const
{
    painter->setPen(m_textColor);
    painter->setFont(m_filenameFont);
    
    // Elide text using pre-cached font metrics
    QString elidedText = m_filenameFM.elidedText(filename, Qt::ElideMiddle, rect.width());
    painter->drawText(rect, Qt::AlignHCenter | Qt::AlignTop, elidedText);
}

void ThumbnailDelegate::paintTagIndicator(QPainter* painter, const QRect& rect,
                                           bool hasTags) const
{
    Q_UNUSED(painter)
    Q_UNUSED(rect)
    Q_UNUSED(hasTags)
    // Deprecated - now using paintTagBadges instead
}

void ThumbnailDelegate::paintTagBadges(QPainter* painter, const QRect& rect,
                                        const QVariantList& tags) const
{
    if (tags.isEmpty()) {
        return;
    }

    painter->setFont(m_badgeFont);

    int badgeHeight = 16;
    int badgePadding = 6;
    int badgeSpacing = 3;
    int badgeRadius = 3;
    int margin = 4;

    // Start from bottom-left of the thumbnail, going right
    int x = rect.left() + margin;
    int y = rect.bottom() - badgeHeight - margin;
    int maxX = rect.right() - margin;
    int minY = rect.top() + margin;  // Don't draw above the thumbnail

    for (const QVariant& tagVar : tags) {
        QVariantMap tagInfo = tagVar.toMap();
        QString name = tagInfo["name"].toString();
        QString colorStr = tagInfo["color"].toString();
        
        // Calculate badge width
        int textWidth = m_badgeFM.horizontalAdvance(name);
        int badgeWidth = textWidth + badgePadding * 2;
        
        // Clamp badge width to available row width so very long tag names still fit
        if (badgeWidth > maxX - (rect.left() + margin)) {
            badgeWidth = maxX - (rect.left() + margin);
        }
        
        // Wrap to the next row above if this badge doesn't fit
        if (x + badgeWidth > maxX) {
            x = rect.left() + margin;
            y -= (badgeHeight + badgeSpacing);
            
            // Stop if we've run out of vertical space
            if (y < minY) {
                break;
            }
        }
        
        QRect badgeRect(x, y, badgeWidth, badgeHeight);
        
        // Determine badge color
        QColor bgColor = colorStr.isEmpty() ? QColor(100, 100, 100) : QColor(colorStr);
        
        // Draw badge background with slight transparency
        painter->setPen(Qt::NoPen);
        QColor fillColor = bgColor;
        fillColor.setAlpha(220);
        painter->setBrush(fillColor);
        painter->drawRoundedRect(badgeRect, badgeRadius, badgeRadius);
        
        // Draw text - use white or black depending on background brightness
        int brightness = (bgColor.red() * 299 + bgColor.green() * 587 + bgColor.blue() * 114) / 1000;
        painter->setPen(brightness > 128 ? Qt::black : Qt::white);
        // Elide text if badge was clamped
        QString displayName = m_badgeFM.elidedText(name, Qt::ElideRight, badgeWidth - badgePadding * 2);
        painter->drawText(badgeRect, Qt::AlignCenter, displayName);
        
        x += badgeWidth + badgeSpacing;
    }
}

void ThumbnailDelegate::paintFavoriteStar(QPainter* painter, const QRect& rect) const
{
    // Draw a small star icon in the bottom left corner
    int starSize = 16;
    int margin = 4;
    int x = rect.left() + margin;
    int y = rect.bottom() - starSize - margin;
    
    // Create a star shape
    QPainterPath starPath;
    QPointF center(x + starSize / 2.0, y + starSize / 2.0);
    double outerRadius = starSize / 2.0;
    double innerRadius = outerRadius * 0.4;
    int points = 5;
    
    for (int i = 0; i < points * 2; ++i) {
        double angle = (i * M_PI) / points;
        double radius = (i % 2 == 0) ? outerRadius : innerRadius;
        double px = center.x() + radius * cos(angle - M_PI / 2.0);
        double py = center.y() + radius * sin(angle - M_PI / 2.0);
        
        if (i == 0) {
            starPath.moveTo(px, py);
        } else {
            starPath.lineTo(px, py);
        }
    }
    starPath.closeSubpath();
    
    // Draw star with yellow/gold color and slight shadow
    painter->setPen(QPen(QColor(0, 0, 0, 100), 1));
    painter->setBrush(QColor(255, 215, 0)); // Gold color
    painter->drawPath(starPath);
}

void ThumbnailDelegate::paintRating(QPainter* painter, const QRect& rect, int rating) const
{
    // Draw small filled circles in the top-right corner representing the rating (1-5)
    // Each dot is colored based on the overall rating level
    static const QColor ratingColors[] = {
        QColor(0, 0, 0, 0),     // 0 - unused
        QColor(244, 67, 54),     // 1 - red (reject/poor)
        QColor(255, 152, 0),     // 2 - orange (below average)
        QColor(255, 235, 59),    // 3 - yellow (average)
        QColor(139, 195, 74),    // 4 - light green (good)
        QColor(76, 175, 80)      // 5 - green (excellent)
    };
    
    int dotSize = 6;
    int dotSpacing = 3;
    int margin = 5;
    
    // Calculate total width of dots
    int totalWidth = rating * dotSize + (rating - 1) * dotSpacing;
    
    // Position in top-right corner
    int startX = rect.right() - margin - totalWidth;
    int y = rect.top() + margin;
    
    QColor dotColor = ratingColors[qBound(1, rating, 5)];
    
    // Draw shadow/background pill behind dots for visibility
    QRect pillRect(startX - 3, y - 2, totalWidth + 6, dotSize + 4);
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(0, 0, 0, 160));
    painter->drawRoundedRect(pillRect, (dotSize + 4) / 2.0, (dotSize + 4) / 2.0);
    
    // Draw the dots
    painter->setPen(Qt::NoPen);
    painter->setBrush(dotColor);
    for (int i = 0; i < rating; ++i) {
        int dotX = startX + i * (dotSize + dotSpacing);
        painter->drawEllipse(dotX, y, dotSize, dotSize);
    }
}

void ThumbnailDelegate::paintHoverEffect(QPainter* painter, const QRect& rect) const
{
    painter->fillRect(rect, m_hoverColor);
}

// ============== Size Hint ==============

QSize ThumbnailDelegate::sizeHint(const QStyleOptionViewItem& option,
                                   const QModelIndex& index) const
{
    Q_UNUSED(option)
    Q_UNUSED(index)

    int width = m_thumbnailSize + m_spacing * 2;
    int height = m_thumbnailSize + m_spacing * 2;
    
    if (m_showFilename) {
        height += m_filenameHeight + 4;
    }

    return QSize(width, height);
}

} // namespace FullFrame

