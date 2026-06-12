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
#include <QComboBox>
#include <QProgressBar>
#include <QStackedWidget>
#include <QAction>
#include <QHBoxLayout>

class QSplitter;

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
    void showShortcutsDialog();
    void deleteSelectedImages();
    void onThumbnailReady(const QString& filePath);
    void onThumbnailFailed(const QString& filePath);
    void exportDatabase();
    void importDatabase();
    void toggleViewMode();
    void setGalleryMode();
    void setTaggingMode();
    void toggleSidebar();
    void setSidebarCollapsed(bool collapsed);
    void createAlbumFromSelection();
    void onImageTaggedForAlbum(const QString& imagePath, qint64 tagId);
    void onTagLinkedToFolder(qint64 tagId, const QString& albumPath);
    void toggleFavoriteSelected();
    void setRatingSelected(int rating);
    void showCombineTagsDialog();

private:
    void setupUI();
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void setupShortcuts();
    void initializeDatabase();
    void initializeDatabase(const QString& folderPath);
    void loadSettings();
    void saveSettings();
    void loadRatingsFromDb();
    void reapplySort();

    // Fullscreen / immersive display modes
    void cycleDisplayMode();
    void applyDisplayMode();
    void checkImmersiveHover();

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
    QComboBox* m_sortCombo = nullptr;
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
    
    // Rating system (separate from tags, 1-5)
    QHash<QString, int> m_ratings;
    bool m_ratingHotkeysEnabled = true;
    QAction* m_ratingHotkeysAction = nullptr;
    
    // Sort mode
    QString m_sortMode = "default";
    
    // Album auto-move batching
    QTimer* m_albumRefreshTimer = nullptr;

    // Display mode (fullscreen / immersive)
    enum DisplayMode { DisplayNormal, DisplayFullscreen, DisplayImmersive };
    DisplayMode m_displayMode = DisplayNormal;
    QTimer* m_immersiveTimer = nullptr;

    // Top bar layout (menu bar corner widget) for relocating sidebar buttons
    QHBoxLayout* m_topBarLayout = nullptr;
    int m_sidebarButtonInsertIndex = 0;

    // Sidebar splitter / collapse state
    QSplitter* m_splitter = nullptr;
    bool m_sidebarCollapsed = false;
    int m_sidebarWidth = 240;
};

} // namespace FullFrame

