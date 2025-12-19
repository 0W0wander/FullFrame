#pragma once

#include <QStyledItemDelegate>

namespace FullFrame {

class ThumbnailDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit ThumbnailDelegate(QObject* parent = nullptr);
    ~ThumbnailDelegate() override;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

private:
    void paintThumbnail(QPainter* painter, const QRect& rect,
                        const QPixmap& pixmap, bool selected) const;
    void paintFileName(QPainter* painter, const QRect& rect,
                       const QString& name, bool selected) const;
};

} // namespace FullFrame
