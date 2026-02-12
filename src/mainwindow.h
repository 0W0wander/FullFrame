/**
 * MainWindow - Main application window
 * 
 * Layout:
 * - Left: Tag sidebar
 * - Center: Image grid view
 * - Top: Toolbar with folder selection, zoom, etc.
 * - Bottom: Status bar
 */

#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QSlider>
#include <QLineEdit>
#include <QProgressBar>
#include <QStackedWidget>
#include <QAction>

namespace FullFrame {

class ImageGridView;
class ImageThumbnailModel;
class TagSidebar;
class TaggingModeWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

public Q_SLOTS:
    void openFolder();
    void openFolder(const QString& path);

private Q_SLOTS:
    void onLoadingStarted();
    void onLoadingFinished(int count);
    void onSelectionChanged(const QStringList& paths);
    void onImageActivated(const QString& filePath);
    void onThumbnailSizeChanged(int size);
    void onZoomSliderChanged(int value);
    void onTagFilterChanged(const QSet<qint64>& tagIds);
    void onContextMenu(const QPoint& pos, const QString& filePath);
    void showAboutDialog();
    void deleteSelectedImages();
    void onThumbnailReady(const QString& filePath);
    void onThumbnailFailed(const QString& filePath);
    void exportDatabase();
    void importDatabase();
    void toggleViewMode();
    void setGalleryMode();
    void setTaggingMode();
    void toggleSidebar();
    void createAlbumFromSelection();
    void onImageTaggedForAlbum(const QString& imagePath, qint64 tagId);
    void onTagLinkedToFolder(qint64 tagId, const QString& albumPath);
    void toggleFavoriteSelected();

private:
    void setupUI();
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void setupShortcuts();
    void initializeDatabase();
    void loadSettings();
    void saveSettings();

protected:
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    // Main components
    ImageGridView* m_gridView = nullptr;
    ImageThumbnailModel* m_model = nullptr;
    TagSidebar* m_tagSidebar = nullptr;
    TaggingModeWidget* m_taggingMode = nullptr;
    
    // View mode
    QStackedWidget* m_viewStack = nullptr;
    QAction* m_galleryModeAction = nullptr;
    QAction* m_taggingModeAction = nullptr;
    QAction* m_toggleSidebarAction = nullptr;
    QAction* m_showAlbumFilesAction = nullptr;
    bool m_isTaggingMode = false;
    bool m_showAlbumFiles = true;

    // Toolbar widgets
    QLineEdit* m_pathEdit = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QSlider* m_zoomSlider = nullptr;
    QLabel* m_zoomLabel = nullptr;

    // Status bar widgets
    QLabel* m_statusLabel = nullptr;
    QLabel* m_selectionLabel = nullptr;
    QLabel* m_cacheLabel = nullptr;
    QProgressBar* m_loadingProgressBar = nullptr;
    QLabel* m_loadingLabel = nullptr;

    // Current state
    QString m_currentFolder;
    int m_pendingThumbnails = 0;
    int m_totalThumbnails = 0;
    
    // Favorites system (separate from tags)
    QSet<QString> m_favorites;
    
    // Album auto-move batching
    QTimer* m_albumRefreshTimer = nullptr;
};

} // namespace FullFrame

