#pragma once

#include <QListView>

namespace FullFrame {

class ImageThumbnailModel;

class ImageGridView : public QListView
{
    Q_OBJECT

public:
    explicit ImageGridView(QWidget* parent = nullptr);
    ~ImageGridView() override;

    void setThumbnailSize(int size);
    int thumbnailSize() const { return m_thumbnailSize; }

Q_SIGNALS:
    void imageSelected(const QString& filePath);
    void imageDoubleClicked(const QString& filePath);
    void selectionChanged(const QStringList& selectedPaths);

protected:
    void wheelEvent(QWheelEvent* event) override;
    void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    void updateGridSize();

    int m_thumbnailSize = 256;
};

} // namespace FullFrame
