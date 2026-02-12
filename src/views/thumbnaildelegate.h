/**
 * ThumbnailDelegate - Custom delegate for image thumbnails
 * 
 * Features:
 * - Efficient custom painting
 * - Selection highlighting
 * - Tag indicator badges
 * - Hover effects
 * - Filename display
 */

#pragma once

#include <QStyledItemDelegate>
#include <QCache>
#include <QVariantList>
#include <QFont>
#include <QFontMetrics>

namespace FullFrame {

class ThumbnailDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit ThumbnailDelegate(QObject* parent = nullptr);
    ~ThumbnailDelegate() override;

    // Size configuration
    void setThumbnailSize(int size);
    int thumbnailSize() const { return m_thumbnailSize; }
    
    void setSpacing(int spacing);
    int spacing() const { return m_spacing; }
    
    void setShowFilename(bool show);
    bool showFilename() const { return m_showFilename; }
    
    void setShowTagIndicator(bool show);
    bool showTagIndicator() const { return m_showTagIndicator; }

    // QStyledItemDelegate interface
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

Q_SIGNALS:
    void sizeHintChanged();

private:
    void paintThumbnail(QPainter* painter, const QRect& rect,
                        const QPixmap& pixmap) const;
    void paintSelection(QPainter* painter, const QRect& rect,
                        const QStyleOptionViewItem& option) const;
    void paintFilename(QPainter* painter, const QRect& rect,
                       const QString& filename) const;
    void paintTagIndicator(QPainter* painter, const QRect& rect,
                           bool hasTags) const;
    void paintTagBadges(QPainter* painter, const QRect& rect,
                        const QVariantList& tags) const;
    void paintFavoriteStar(QPainter* painter, const QRect& rect) const;
    void paintHoverEffect(QPainter* painter, const QRect& rect) const;

private:
    int m_thumbnailSize = 256;
    int m_spacing = 8;
    int m_filenameHeight = 20;
    bool m_showFilename = true;
    bool m_showTagIndicator = true;
    
    // Colors
    QColor m_selectionColor;
    QColor m_hoverColor;
    QColor m_tagIndicatorColor;
    QColor m_backgroundColor;
    QColor m_textColor;
    
    // Pre-cached fonts and metrics to avoid per-paint allocation
    QFont m_filenameFont;
    QFont m_badgeFont;
    QFontMetrics m_filenameFM;
    QFontMetrics m_badgeFM;
};

} // namespace FullFrame

