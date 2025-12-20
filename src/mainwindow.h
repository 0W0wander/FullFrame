#pragma once

#include <QMainWindow>

class QLabel;

namespace FullFrame {

class ImageGridView;
class ImageThumbnailModel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private Q_SLOTS:
    void openFolder();
    void onImageSelected(const QString& path);

private:
    void setupUI();
    void setupMenuBar();
    void setupStatusBar();
    void loadDirectory(const QString& path);

private:
    ImageGridView* m_gridView = nullptr;
    ImageThumbnailModel* m_model = nullptr;
    QLabel* m_statusLabel = nullptr;
    QString m_currentDirectory;
};

} // namespace FullFrame
