/**
 * MainWindow implementation
 */

#include "mainwindow.h"
#include "imagegridview.h"
#include "imagethumbnailmodel.h"
#include "tagsidebar.h"
#include "taggingmodewidget.h"
#include "thumbnailcache.h"
#include "thumbnailloadthread.h"
#include "tagmanager.h"

#include <QApplication>
#include <QMenuBar>
#include <QMenu>
#include <QToolBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QStandardPaths>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QPushButton>
#include <QShortcut>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QTimer>
#include <QDesktopServices>
#include <QKeyEvent>
#include <QFile>
#include <QCheckBox>
#include <QInputDialog>

namespace FullFrame {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("FullFrame - Image Tagging");
    setMinimumSize(1024, 768);
    setAcceptDrops(true);

    initializeDatabase();

    setupUI();
    setupMenuBar();
    setupToolBar();
    setupStatusBar();
    setupShortcuts();
    
    loadSettings();

    // Style the main window
    setStyleSheet(R"(
        QMainWindow {
            background-color: #1e1e1e;
        }
        QMenuBar {
            background-color: #2d2d2d;
            color: #e0e0e0;
            padding: 4px;
        }
        QMenuBar::item {
            padding: 6px 12px;
            border-radius: 4px;
        }
        QMenuBar::item:selected {
            background-color: #3d3d3d;
        }
        QMenu {
            background-color: #2d2d2d;
            color: #e0e0e0;
            border: 1px solid #3d3d3d;
        }
        QMenu::item {
            padding: 8px 24px;
        }
        QMenu::item:selected {
            background-color: #005a9e;
        }
        QToolBar {
            background-color: #2d2d2d;
            border: none;
            spacing: 8px;
            padding: 4px 8px;
        }
        QStatusBar {
            background-color: #252525;
            color: #a0a0a0;
        }
    )");
}

MainWindow::~MainWindow()
{
    saveSettings();
}

void MainWindow::initializeDatabase()
{
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataPath);
    
    QString dbPath = dataPath + "/fullframe.db";
    
    if (!TagManager::instance()->initialize(dbPath)) {
        QMessageBox::warning(this, "Database Error",
            "Failed to initialize tag database. Tagging will be disabled.");
        return;
    }
    
    // Create default tags with pre-assigned hotkeys if none exist
    QList<Tag> existingTags = TagManager::instance()->allTags();
    
    if (existingTags.isEmpty()) {
        qint64 id1 = TagManager::instance()->createTag("Keep", "#4caf50");
        if (id1 >= 0) {
            TagManager::instance()->setTagHotkey(id1, "1");
        }
        
        qint64 id2 = TagManager::instance()->createTag("Delete", "#f44336");
        if (id2 >= 0) {
            TagManager::instance()->setTagHotkey(id2, "2");
        }
        
        qint64 id3 = TagManager::instance()->createTag("Review", "#ff9800");
        if (id3 >= 0) {
            TagManager::instance()->setTagHotkey(id3, "3");
        }
        
        qint64 id4 = TagManager::instance()->createTag("Favorite", "#e91e63");
        if (id4 >= 0) {
            TagManager::instance()->setTagHotkey(id4, "A");
        }
    }
}

void MainWindow::setupUI()
{
    // Central widget with splitter
    QWidget* centralWidget = new QWidget(this);
    QHBoxLayout* mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Tag sidebar
    m_tagSidebar = new TagSidebar(this);
    m_tagSidebar->refresh();
    mainLayout->addWidget(m_tagSidebar);

    // Separator
    QFrame* separator = new QFrame(this);
    separator->setFrameShape(QFrame::VLine);
    separator->setStyleSheet("background-color: #3d3d3d;");
    separator->setFixedWidth(1);
    mainLayout->addWidget(separator);

    // Create model (shared between views)
    m_model = new ImageThumbnailModel(this);

    // Stacked widget for view modes
    m_viewStack = new QStackedWidget(this);
    
    // Gallery mode (index 0)
    m_gridView = new ImageGridView(this);
    m_gridView->setImageModel(m_model);
    m_viewStack->addWidget(m_gridView);
    
    // Tagging mode (index 1)
    m_taggingMode = new TaggingModeWidget(this);
    m_taggingMode->setModel(m_model);
    m_viewStack->addWidget(m_taggingMode);
    
    mainLayout->addWidget(m_viewStack, 1);

    setCentralWidget(centralWidget);

    // Connect signals
    connect(m_model, &ImageThumbnailModel::loadingStarted,
            this, &MainWindow::onLoadingStarted);
    connect(m_model, &ImageThumbnailModel::loadingFinished,
            this, &MainWindow::onLoadingFinished);
    connect(m_gridView, &ImageGridView::selectionChanged,
            this, &MainWindow::onSelectionChanged);
    connect(m_gridView, &ImageGridView::imageActivated,
            this, &MainWindow::onImageActivated);
    connect(m_gridView, &ImageGridView::thumbnailSizeChanged,
            this, &MainWindow::onThumbnailSizeChanged);
    connect(m_gridView, &ImageGridView::contextMenuRequested,
            this, &MainWindow::onContextMenu);
    connect(m_gridView, &ImageGridView::deleteRequested,
            this, &MainWindow::deleteSelectedImages);
    connect(m_gridView, &ImageGridView::hotkeyPressed,
            this, [this](const QString& key) {
                m_tagSidebar->handleHotkey(key);
            });
    connect(m_tagSidebar, &TagSidebar::tagFilterChanged,
            this, &MainWindow::onTagFilterChanged);
    connect(m_tagSidebar, &TagSidebar::showUntaggedChanged,
            this, [this](bool showUntagged) {
                if (showUntagged) {
                    m_model->setShowUntagged(true);
                } else {
                    m_model->clearTagFilter();
                }
            });
    
    // Tagging mode connections
    connect(m_taggingMode, &TaggingModeWidget::imageSelected,
            this, [this](const QString& path) {
                // Update sidebar with single image selection
                m_tagSidebar->setSelectedImagePaths(QStringList() << path);
            });
    connect(m_taggingMode, &TaggingModeWidget::openRequested,
            this, &MainWindow::onImageActivated);
    
    // Sidebar tagging mode button
    connect(m_tagSidebar, &TagSidebar::taggingModeRequested,
            this, [this](bool enabled) {
                if (enabled) {
                    setTaggingMode();
                } else {
                    setGalleryMode();
                }
            });
    
    // Album tag auto-move: when an image is tagged with an album tag, move it
    connect(TagManager::instance(), &TagManager::imageTagged,
            this, &MainWindow::onImageTaggedForAlbum);
    
    // When a tag is linked to a folder, retroactively move all existing tagged images
    connect(TagManager::instance(), &TagManager::tagAlbumPathChanged,
            this, &MainWindow::onTagLinkedToFolder);
    
    // Install event filter to catch all key events for hotkeys
    qApp->installEventFilter(this);
}

void MainWindow::setupMenuBar()
{
    QMenuBar* menuBar = this->menuBar();

    // File menu
    QMenu* fileMenu = menuBar->addMenu("&File");
    
    QAction* openAction = fileMenu->addAction("&Open Folder...");
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, qOverload<>(&MainWindow::openFolder));
    
    fileMenu->addSeparator();
    
    QAction* exitAction = fileMenu->addAction("E&xit");
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    // Edit menu
    QMenu* editMenu = menuBar->addMenu("&Edit");
    
    QAction* selectAllAction = editMenu->addAction("Select &All");
    selectAllAction->setShortcut(QKeySequence::SelectAll);
    connect(selectAllAction, &QAction::triggered, m_gridView, &ImageGridView::selectAll);
    
    QAction* clearSelectionAction = editMenu->addAction("&Clear Selection");
    clearSelectionAction->setShortcut(Qt::Key_Escape);
    connect(clearSelectionAction, &QAction::triggered, m_gridView, &ImageGridView::clearSelection);

    // View menu
    QMenu* viewMenu = menuBar->addMenu("&View");
    
    // View mode submenu
    QMenu* viewModeMenu = viewMenu->addMenu("View &Mode");
    
    m_galleryModeAction = viewModeMenu->addAction("📷 &Gallery Mode");
    m_galleryModeAction->setCheckable(true);
    m_galleryModeAction->setChecked(true);
    m_galleryModeAction->setShortcut(QKeySequence("Ctrl+1"));
    connect(m_galleryModeAction, &QAction::triggered, this, &MainWindow::setGalleryMode);
    
    m_taggingModeAction = viewModeMenu->addAction("🏷️ &Tagging Mode");
    m_taggingModeAction->setCheckable(true);
    m_taggingModeAction->setShortcut(QKeySequence("Ctrl+2"));
    connect(m_taggingModeAction, &QAction::triggered, this, &MainWindow::setTaggingMode);
    
    viewMenu->addSeparator();
    
    QAction* zoomInAction = viewMenu->addAction("Zoom &In");
    zoomInAction->setShortcut(QKeySequence::ZoomIn);
    connect(zoomInAction, &QAction::triggered, m_gridView, &ImageGridView::zoomIn);
    
    QAction* zoomOutAction = viewMenu->addAction("Zoom &Out");
    zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    connect(zoomOutAction, &QAction::triggered, m_gridView, &ImageGridView::zoomOut);
    
    viewMenu->addSeparator();
    
    QAction* refreshAction = viewMenu->addAction("&Refresh");
    refreshAction->setShortcut(QKeySequence::Refresh);
    connect(refreshAction, &QAction::triggered, this, [this]() {
        if (!m_currentFolder.isEmpty()) {
            openFolder(m_currentFolder);
        }
    });
    
    viewMenu->addSeparator();
    
    m_toggleSidebarAction = viewMenu->addAction("Toggle &Sidebar");
    m_toggleSidebarAction->setShortcut(QKeySequence("Ctrl+B"));
    m_toggleSidebarAction->setCheckable(true);
    m_toggleSidebarAction->setChecked(true);
    connect(m_toggleSidebarAction, &QAction::triggered, this, &MainWindow::toggleSidebar);

    // Preferences menu
    QMenu* prefsMenu = menuBar->addMenu("&Preferences");
    
    QAction* openDbFolderAction = prefsMenu->addAction("Open &Database Folder...");
    connect(openDbFolderAction, &QAction::triggered, this, [this]() {
        QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDesktopServices::openUrl(QUrl::fromLocalFile(dataPath));
    });
    
    prefsMenu->addSeparator();
    
    QAction* exportDbAction = prefsMenu->addAction("&Export Database...");
    connect(exportDbAction, &QAction::triggered, this, &MainWindow::exportDatabase);
    
    QAction* importDbAction = prefsMenu->addAction("&Import Database...");
    connect(importDbAction, &QAction::triggered, this, &MainWindow::importDatabase);

    // Help menu
    QMenu* helpMenu = menuBar->addMenu("&Help");
    
    QAction* aboutAction = helpMenu->addAction("&About FullFrame");
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAboutDialog);
}

void MainWindow::setupToolBar()
{
    // Instead of a separate toolbar, we add widgets to the right side of the menu bar
    QMenuBar* menu = menuBar();
    
    // Create a container widget for all controls
    QWidget* menuBarWidget = new QWidget(this);
    QHBoxLayout* layout = new QHBoxLayout(menuBarWidget);
    layout->setContentsMargins(8, 0, 8, 0);
    layout->setSpacing(8);

    // Open folder button
    QPushButton* openButton = new QPushButton("📁 Open", this);
    openButton->setStyleSheet(R"(
        QPushButton {
            background-color: #005a9e;
            border: none;
            border-radius: 4px;
            padding: 4px 12px;
            color: white;
            font-weight: bold;
            font-size: 12px;
        }
        QPushButton:hover {
            background-color: #0068b8;
        }
        QPushButton:pressed {
            background-color: #004c87;
        }
    )");
    connect(openButton, &QPushButton::clicked, this, qOverload<>(&MainWindow::openFolder));
    layout->addWidget(openButton);

    // Path display
    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setReadOnly(true);
    m_pathEdit->setPlaceholderText("No folder selected");
    m_pathEdit->setMinimumWidth(250);
    m_pathEdit->setStyleSheet(R"(
        QLineEdit {
            background-color: #252525;
            border: 1px solid #3d3d3d;
            border-radius: 4px;
            padding: 4px 10px;
            color: #e0e0e0;
            font-size: 12px;
        }
    )");
    layout->addWidget(m_pathEdit, 1);

    // Search bar
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("🔎 Search by filename...");
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setMinimumWidth(160);
    m_searchEdit->setMaximumWidth(280);
    m_searchEdit->setStyleSheet(R"(
        QLineEdit {
            background-color: #252525;
            border: 1px solid #3d3d3d;
            border-radius: 4px;
            padding: 4px 10px;
            color: #e0e0e0;
            font-size: 12px;
        }
        QLineEdit:focus {
            border: 1px solid #005a9e;
        }
    )");
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        m_model->setFilenameFilter(text);
    });
    layout->addWidget(m_searchEdit);

    // Zoom controls
    QLabel* zoomIcon = new QLabel("🔍", this);
    zoomIcon->setStyleSheet("color: #e0e0e0; font-size: 14px;");
    layout->addWidget(zoomIcon);

    m_zoomSlider = new QSlider(Qt::Horizontal, this);
    m_zoomSlider->setRange(64, 512);
    m_zoomSlider->setValue(256);
    m_zoomSlider->setFixedWidth(100);
    m_zoomSlider->setStyleSheet(R"(
        QSlider::groove:horizontal {
            background: #3d3d3d;
            height: 4px;
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            background: #005a9e;
            width: 12px;
            height: 12px;
            margin: -4px 0;
            border-radius: 6px;
        }
        QSlider::handle:horizontal:hover {
            background: #0068b8;
        }
    )");
    connect(m_zoomSlider, &QSlider::valueChanged, this, &MainWindow::onZoomSliderChanged);
    layout->addWidget(m_zoomSlider);

    m_zoomLabel = new QLabel("256px", this);
    m_zoomLabel->setFixedWidth(45);
    m_zoomLabel->setStyleSheet("color: #a0a0a0; font-size: 11px;");
    layout->addWidget(m_zoomLabel);

    // Set as corner widget of the menu bar
    menu->setCornerWidget(menuBarWidget, Qt::TopRightCorner);
}

void MainWindow::setupStatusBar()
{
    QStatusBar* status = statusBar();

    m_statusLabel = new QLabel("Ready", this);
    status->addWidget(m_statusLabel);

    // Loading progress bar
    m_loadingLabel = new QLabel("", this);
    m_loadingLabel->setStyleSheet("color: #a0a0a0; margin-left: 8px;");
    status->addWidget(m_loadingLabel);
    m_loadingLabel->hide();
    
    m_loadingProgressBar = new QProgressBar(this);
    m_loadingProgressBar->setFixedWidth(200);
    m_loadingProgressBar->setFixedHeight(16);
    m_loadingProgressBar->setTextVisible(false);
    m_loadingProgressBar->setStyleSheet(R"(
        QProgressBar {
            background-color: #3d3d3d;
            border: none;
            border-radius: 3px;
        }
        QProgressBar::chunk {
            background-color: #005a9e;
            border-radius: 3px;
        }
    )");
    status->addWidget(m_loadingProgressBar);
    m_loadingProgressBar->hide();

    m_selectionLabel = new QLabel("", this);
    status->addPermanentWidget(m_selectionLabel);

    m_cacheLabel = new QLabel("", this);
    status->addPermanentWidget(m_cacheLabel);

    // Update cache stats periodically
    QTimer* cacheTimer = new QTimer(this);
    connect(cacheTimer, &QTimer::timeout, this, [this]() {
        int imageCount = ThumbnailCache::instance()->imageCacheCount();
        int pixmapCount = ThumbnailCache::instance()->pixmapCacheCount();
        m_cacheLabel->setText(QString("Cache: %1 images, %2 pixmaps")
            .arg(imageCount).arg(pixmapCount));
    });
    cacheTimer->start(1000);
    
    // Connect to thumbnail loading thread for progress updates
    connect(ThumbnailLoadThread::instance(), &ThumbnailLoadThread::thumbnailAvailable,
            this, &MainWindow::onThumbnailReady);
    connect(ThumbnailLoadThread::instance(), &ThumbnailLoadThread::thumbnailFailed,
            this, &MainWindow::onThumbnailFailed);
}

void MainWindow::setupShortcuts()
{
    // Additional shortcuts
    new QShortcut(QKeySequence("Ctrl+R"), this, [this]() {
        if (!m_currentFolder.isEmpty()) {
            openFolder(m_currentFolder);
        }
    });
}

// ============== Public Slots ==============

void MainWindow::openFolder()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Select Image Folder",
        m_currentFolder.isEmpty() ? QDir::homePath() : m_currentFolder);
    
    if (!dir.isEmpty()) {
        openFolder(dir);
    }
}

void MainWindow::openFolder(const QString& path)
{
    m_currentFolder = path;
    m_pathEdit->setText(path);
    
    // Clear the search bar when opening a new folder
    m_searchEdit->clear();
    
    m_model->loadDirectory(path);
    m_tagSidebar->setCurrentDirectoryPaths(m_model->allFilePaths());
    m_tagSidebar->refresh();
}

// ============== Private Slots ==============

void MainWindow::onLoadingStarted()
{
    m_statusLabel->setText("Scanning folder...");
    QApplication::setOverrideCursor(Qt::WaitCursor);
    
    // Reset progress tracking
    m_pendingThumbnails = 0;
    m_totalThumbnails = 0;
}

void MainWindow::onLoadingFinished(int count)
{
    m_statusLabel->setText(QString("Loaded %1 images").arg(count));
    QApplication::restoreOverrideCursor();
    
    // Set up progress tracking for thumbnails
    m_totalThumbnails = count;
    m_pendingThumbnails = count;
    
    if (count > 0) {
        m_loadingProgressBar->setRange(0, count);
        m_loadingProgressBar->setValue(0);
        m_loadingProgressBar->show();
        m_loadingLabel->setText(QString("Loading %1 thumbnails...").arg(count));
        m_loadingLabel->show();
    }
}

void MainWindow::onThumbnailReady(const QString& filePath)
{
    Q_UNUSED(filePath)
    
    if (m_pendingThumbnails > 0) {
        m_pendingThumbnails--;
        
        // Throttle progress bar + label updates — only every 20th thumbnail or
        // when done. Previously this ran on EVERY thumbnail completion (hundreds/sec),
        // which flooded the event loop with widget setText/setValue updates that
        // trigger geometry recalcs and repaints, competing with scroll events.
        if (m_pendingThumbnails == 0) {
            m_loadingProgressBar->hide();
            m_loadingLabel->hide();
            m_statusLabel->setText(QString("Ready - %1 images").arg(m_totalThumbnails));
        } else if (m_pendingThumbnails % 20 == 0) {
            int loaded = m_totalThumbnails - m_pendingThumbnails;
            m_loadingProgressBar->setValue(loaded);
            m_loadingLabel->setText(QString("Loading... %1 remaining").arg(m_pendingThumbnails));
        }
    }
}

void MainWindow::onThumbnailFailed(const QString& filePath)
{
    Q_UNUSED(filePath)
    
    if (m_pendingThumbnails > 0) {
        m_pendingThumbnails--;
        
        if (m_pendingThumbnails <= 0) {
            m_loadingProgressBar->hide();
            m_loadingLabel->hide();
            m_statusLabel->setText(QString("Ready - %1 images").arg(m_totalThumbnails));
        }
        // Don't update progress bar on every failure — throttled same as ready
    }
}

void MainWindow::onSelectionChanged(const QStringList& paths)
{
    m_selectionLabel->setText(QString("%1 selected").arg(paths.size()));
    m_tagSidebar->setSelectedImagePaths(paths);
}

void MainWindow::onImageActivated(const QString& filePath)
{
    // Open image with system default viewer
    QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
}

void MainWindow::onThumbnailSizeChanged(int size)
{
    m_zoomSlider->blockSignals(true);
    m_zoomSlider->setValue(size);
    m_zoomSlider->blockSignals(false);
    m_zoomLabel->setText(QString("%1px").arg(size));
}

void MainWindow::onZoomSliderChanged(int value)
{
    m_gridView->setThumbnailSize(value);
    m_zoomLabel->setText(QString("%1px").arg(value));
}

void MainWindow::onTagFilterChanged(const QSet<qint64>& tagIds)
{
    // Remember current selection so we can restore it after the model reloads
    QString currentPath;
    if (!m_isTaggingMode) {
        QStringList sel = m_gridView->selectedImagePaths();
        if (!sel.isEmpty()) {
            currentPath = sel.first();
        }
    }

    if (tagIds.isEmpty()) {
        m_model->clearTagFilter();
    } else {
        m_model->setTagFilter(tagIds, false);  // false = any tag matches
    }

    // Restore grid view position if possible
    if (!m_isTaggingMode && !currentPath.isEmpty()) {
        m_gridView->scrollToImage(currentPath);
    }
}

void MainWindow::onContextMenu(const QPoint& pos, const QString& filePath)
{
    QMenu menu(this);

    if (!filePath.isEmpty()) {
        QAction* openAction = menu.addAction("Open in Default Viewer");
        connect(openAction, &QAction::triggered, this, [filePath]() {
            QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
        });

        QAction* openFolderAction = menu.addAction("Show in Explorer");
        connect(openFolderAction, &QAction::triggered, this, [filePath]() {
            QFileInfo info(filePath);
            QDesktopServices::openUrl(QUrl::fromLocalFile(info.absolutePath()));
        });

        menu.addSeparator();

        // Tag submenu
        QMenu* tagMenu = menu.addMenu("Add Tag");
        QList<Tag> tags = TagManager::instance()->allTags();
        for (const Tag& tag : tags) {
            QString prefix = tag.isAlbumTag() ? QString::fromUtf8("\xF0\x9F\x93\x81 ") : QString();
            QAction* tagAction = tagMenu->addAction(prefix + tag.name);
            connect(tagAction, &QAction::triggered, this, [filePath, tag]() {
                TagManager::instance()->tagImage(filePath, tag.id);
            });
        }

        QMenu* removeTagMenu = menu.addMenu("Remove Tag");
        QList<Tag> imageTags = TagManager::instance()->tagsForImage(filePath);
        for (const Tag& tag : imageTags) {
            QAction* tagAction = removeTagMenu->addAction(tag.name);
            connect(tagAction, &QAction::triggered, this, [filePath, tag]() {
                TagManager::instance()->untagImage(filePath, tag.id);
            });
        }
        removeTagMenu->setEnabled(!imageTags.isEmpty());

        // Album actions
        menu.addSeparator();

        // "Move to Album" submenu — list existing album tags
        QList<Tag> allTags = TagManager::instance()->allTags();
        QMenu* moveToAlbumMenu = menu.addMenu(QString::fromUtf8("\xF0\x9F\x93\x81 Move to Album"));
        bool hasAlbumTags = false;
        QStringList selectedPaths = m_gridView->selectedImagePaths();
        if (selectedPaths.isEmpty()) {
            selectedPaths << filePath;
        }
        for (const Tag& tag : allTags) {
            if (tag.isAlbumTag()) {
                hasAlbumTags = true;
                QAction* albumAction = moveToAlbumMenu->addAction(
                    QString::fromUtf8("\xF0\x9F\x93\x81 ") + tag.name);
                connect(albumAction, &QAction::triggered, this, [this, selectedPaths, tag]() {
                    for (const QString& path : selectedPaths) {
                        TagManager::instance()->tagImage(path, tag.id);
                    }
                });
            }
        }
        moveToAlbumMenu->setEnabled(hasAlbumTags);

        // "Create Album from Selection"
        if (selectedPaths.size() >= 2) {
            QAction* createAlbumAction = menu.addAction(
                QString::fromUtf8("\xF0\x9F\x93\x81 Create Album from Selection..."));
            connect(createAlbumAction, &QAction::triggered,
                    this, &MainWindow::createAlbumFromSelection);
        }
    }

    menu.exec(pos);
}

void MainWindow::showAboutDialog()
{
    QMessageBox::about(this, "About FullFrame",
        "<h2>FullFrame</h2>"
        "<p>A high-performance image tagging application</p>"
        "<p>Inspired by DigiKam's efficient thumbnail loading system.</p>"
        "<p><b>Features:</b></p>"
        "<ul>"
        "<li>Lazy thumbnail loading (only visible items)</li>"
        "<li>Multi-threaded thumbnail generation</li>"
        "<li>LRU caching for instant re-display</li>"
        "<li>Tag-based image organization</li>"
        "</ul>"
        "<p>Version 1.0.0</p>");
}

// ============== Settings ==============

void MainWindow::loadSettings()
{
    QSettings settings("FullFrame", "FullFrame");
    
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
    
    int thumbnailSize = settings.value("thumbnailSize", 256).toInt();
    m_gridView->setThumbnailSize(thumbnailSize);
    
    QString lastFolder = settings.value("lastFolder").toString();
    if (!lastFolder.isEmpty() && QDir(lastFolder).exists()) {
        openFolder(lastFolder);
    }
}

void MainWindow::saveSettings()
{
    QSettings settings("FullFrame", "FullFrame");
    
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
    settings.setValue("thumbnailSize", m_gridView->thumbnailSize());
    settings.setValue("lastFolder", m_currentFolder);
}

// ============== Events ==============

void MainWindow::closeEvent(QCloseEvent* event)
{
    saveSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent* event)
{
    const QMimeData* mimeData = event->mimeData();
    
    if (mimeData->hasUrls()) {
        QList<QUrl> urls = mimeData->urls();
        if (!urls.isEmpty()) {
            QString path = urls.first().toLocalFile();
            QFileInfo info(path);
            
            if (info.isDir()) {
                openFolder(path);
            } else if (info.isFile()) {
                // Open parent folder and scroll to file
                openFolder(info.absolutePath());
                QTimer::singleShot(500, this, [this, path]() {
                    m_gridView->scrollToImage(path);
                });
            }
        }
    }
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    QMainWindow::keyPressEvent(event);
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        
        // Don't intercept if typing in a text field
        QWidget* focusWidget = QApplication::focusWidget();
        if (qobject_cast<QLineEdit*>(focusWidget)) {
            return QMainWindow::eventFilter(obj, event);
        }
        
        // Handle Delete key
        if (keyEvent->key() == Qt::Key_Delete) {
            deleteSelectedImages();
            return true;
        }
        
        // Handle tag hotkeys (0-9, A-Z, F1-F12)
        QString hotkeyText;
        
        // Number keys
        if (keyEvent->key() >= Qt::Key_0 && keyEvent->key() <= Qt::Key_9) {
            hotkeyText = QString::number(keyEvent->key() - Qt::Key_0);
        }
        // Letter keys (only without Ctrl/Alt modifiers)
        else if (keyEvent->key() >= Qt::Key_A && keyEvent->key() <= Qt::Key_Z &&
                 !(keyEvent->modifiers() & Qt::ControlModifier) &&
                 !(keyEvent->modifiers() & Qt::AltModifier)) {
            hotkeyText = QChar('A' + (keyEvent->key() - Qt::Key_A));
        }
        // Function keys
        else if (keyEvent->key() >= Qt::Key_F1 && keyEvent->key() <= Qt::Key_F12) {
            hotkeyText = QString("F%1").arg(keyEvent->key() - Qt::Key_F1 + 1);
        }
        
        if (!hotkeyText.isEmpty()) {
            if (m_tagSidebar->handleHotkey(hotkeyText)) {
                return true;
            }
        }
    }
    
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::deleteSelectedImages()
{
    QStringList selectedPaths;
    int currentRow = 0;
    
    if (m_isTaggingMode) {
        // In tagging mode, get the current image from the tagging widget
        QString currentPath = m_taggingMode->currentImagePath();
        if (!currentPath.isEmpty()) {
            selectedPaths << currentPath;
        }
        currentRow = m_taggingMode->currentRow();
    } else {
        selectedPaths = m_gridView->selectedImagePaths();
        QModelIndex currentIdx = m_gridView->currentIndex();
        currentRow = currentIdx.isValid() ? currentIdx.row() : 0;
    }
    
    if (selectedPaths.isEmpty()) {
        return;
    }
    
    // Check if we should skip the confirmation
    QSettings settings("FullFrame", "FullFrame");
    bool skipConfirmation = settings.value("skipDeleteConfirmation", false).toBool();
    
    if (!skipConfirmation) {
        // Confirmation dialog with "don't show again" checkbox
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("Move to Recycle Bin");
        msgBox.setIcon(QMessageBox::Warning);
        
        QString message = selectedPaths.size() == 1 
            ? QString("Move \"%1\" to the Recycle Bin?").arg(QFileInfo(selectedPaths.first()).fileName())
            : QString("Move %1 selected images to the Recycle Bin?").arg(selectedPaths.size());
        msgBox.setText(message);
        msgBox.setInformativeText("You can restore them from the Recycle Bin if needed.");
        
        QCheckBox* dontAskAgain = new QCheckBox("Don't ask me again");
        msgBox.setCheckBox(dontAskAgain);
        
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::No);
        
        int result = msgBox.exec();
        
        if (result != QMessageBox::Yes) {
            return;
        }
        
        // Save preference if checkbox is checked
        if (dontAskAgain->isChecked()) {
            settings.setValue("skipDeleteConfirmation", true);
        }
    }
    
    int successCount = 0;
    int failCount = 0;
    
    for (const QString& path : selectedPaths) {
        QFile file(path);
        if (file.moveToTrash()) {
            successCount++;
        } else {
            failCount++;
        }
    }
    
    // Refresh the view but maintain position
    if (!m_currentFolder.isEmpty()) {
        // Tell tagging mode to select the same row after model reset
        if (m_isTaggingMode) {
            m_taggingMode->setPendingSelectRow(currentRow);
        }
        
        openFolder(m_currentFolder);
        int totalAfter = m_model->rowCount();
        
        // Try to maintain selection at same row or closest (for gallery mode)
        if (!m_isTaggingMode && totalAfter > 0) {
            int newRow = qMin(currentRow, totalAfter - 1);
            QModelIndex newIdx = m_model->index(newRow);
            m_gridView->setCurrentIndex(newIdx);
            m_gridView->scrollTo(newIdx);
        }
    }
    
    // Show result
    if (failCount > 0) {
        QMessageBox::warning(this, "Recycle Bin",
            QString("Moved %1 file(s) to Recycle Bin. Failed for %2 file(s).")
                .arg(successCount).arg(failCount));
    } else {
        m_statusLabel->setText(QString("Moved %1 file(s) to Recycle Bin").arg(successCount));
    }
}

void MainWindow::exportDatabase()
{
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString dbPath = dataPath + "/fullframe.db";
    
    if (!QFile::exists(dbPath)) {
        QMessageBox::warning(this, "Export Database", "No database file found.");
        return;
    }
    
    QString savePath = QFileDialog::getSaveFileName(this, "Export Database",
        QDir::homePath() + "/fullframe_backup.db",
        "SQLite Database (*.db)");
    
    if (savePath.isEmpty()) {
        return;
    }
    
    // Remove existing file if it exists
    if (QFile::exists(savePath)) {
        QFile::remove(savePath);
    }
    
    if (QFile::copy(dbPath, savePath)) {
        QMessageBox::information(this, "Export Database",
            QString("Database exported successfully to:\n%1").arg(savePath));
    } else {
        QMessageBox::warning(this, "Export Database",
            "Failed to export database. Check file permissions.");
    }
}

void MainWindow::importDatabase()
{
    QString importPath = QFileDialog::getOpenFileName(this, "Import Database",
        QDir::homePath(),
        "SQLite Database (*.db)");
    
    if (importPath.isEmpty()) {
        return;
    }
    
    int result = QMessageBox::warning(this, "Import Database",
        "This will replace your current tag database.\n\n"
        "All existing tags and image associations will be lost.\n\n"
        "Do you want to continue?",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    
    if (result != QMessageBox::Yes) {
        return;
    }
    
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString dbPath = dataPath + "/fullframe.db";
    
    // Close the current database connection
    // Note: TagManager would need a method to close/reopen, for now we just copy
    
    // Backup current database
    QString backupPath = dbPath + ".backup";
    if (QFile::exists(dbPath)) {
        QFile::remove(backupPath);
        QFile::copy(dbPath, backupPath);
        QFile::remove(dbPath);
    }
    
    if (QFile::copy(importPath, dbPath)) {
        QMessageBox::information(this, "Import Database",
            "Database imported successfully.\n\n"
            "Please restart FullFrame for changes to take effect.");
    } else {
        // Restore backup
        if (QFile::exists(backupPath)) {
            QFile::copy(backupPath, dbPath);
        }
        QMessageBox::warning(this, "Import Database",
            "Failed to import database. Your original database has been restored.");
    }
}

// ============== View Mode Switching ==============

void MainWindow::toggleViewMode()
{
    if (m_isTaggingMode) {
        setGalleryMode();
    } else {
        setTaggingMode();
    }
}

void MainWindow::setGalleryMode()
{
    // Get current image from tagging mode before switching
    QString currentImage = m_taggingMode->currentImagePath();
    
    m_isTaggingMode = false;
    m_viewStack->setCurrentIndex(0);
    
    m_galleryModeAction->setChecked(true);
    m_taggingModeAction->setChecked(false);
    m_tagSidebar->setTaggingModeActive(false);
    
    // Scroll to the image we were viewing in tagging mode
    if (!currentImage.isEmpty()) {
        m_gridView->scrollToImage(currentImage);
    }
    
    setWindowTitle("FullFrame - Gallery");
}

void MainWindow::setTaggingMode()
{
    // Get currently selected image from gallery
    QStringList selectedPaths = m_gridView->selectedImagePaths();
    QString targetImage;
    if (!selectedPaths.isEmpty()) {
        targetImage = selectedPaths.first();
    }
    
    m_isTaggingMode = true;
    m_viewStack->setCurrentIndex(1);
    
    m_galleryModeAction->setChecked(false);
    m_taggingModeAction->setChecked(true);
    m_tagSidebar->setTaggingModeActive(true);
    
    // Navigate to the selected image in tagging mode
    if (!targetImage.isEmpty()) {
        m_taggingMode->selectImage(targetImage);
    } else {
        m_taggingMode->selectFirst();
    }
    
    setWindowTitle("FullFrame - Tagging Mode");
}

void MainWindow::toggleSidebar()
{
    bool visible = m_tagSidebar->isVisible();
    m_tagSidebar->setVisible(!visible);
    m_toggleSidebarAction->setChecked(!visible);
}

// ============== Album Support ==============

void MainWindow::createAlbumFromSelection()
{
    QStringList selectedPaths = m_gridView->selectedImagePaths();
    if (selectedPaths.size() < 2) {
        QMessageBox::information(this, "Create Album",
            "Please select at least 2 images to create an album.");
        return;
    }
    
    if (m_currentFolder.isEmpty()) {
        QMessageBox::warning(this, "Create Album",
            "No folder is currently open.");
        return;
    }
    
    // Ask for album name
    bool ok = false;
    QString albumName = QInputDialog::getText(this, "Create Album",
        QString("Album name (will create a subfolder in current directory):"),
        QLineEdit::Normal, "", &ok);
    
    albumName = albumName.trimmed();
    if (!ok || albumName.isEmpty()) {
        return;
    }
    
    // Create the album folder
    QDir currentDir(m_currentFolder);
    QString albumPath = currentDir.filePath(albumName);
    
    if (QDir(albumPath).exists()) {
        QMessageBox::warning(this, "Create Album",
            QString("A folder named \"%1\" already exists.").arg(albumName));
        return;
    }
    
    if (!currentDir.mkdir(albumName)) {
        QMessageBox::warning(this, "Create Album",
            "Failed to create album folder. Check permissions.");
        return;
    }
    
    // Move selected files into the album folder
    int successCount = 0;
    int failCount = 0;
    QStringList movedNewPaths;
    QStringList movedOldPaths;
    
    for (const QString& srcPath : selectedPaths) {
        QFileInfo srcInfo(srcPath);
        QString destPath = QDir(albumPath).filePath(srcInfo.fileName());
        
        // Handle filename conflicts
        if (QFile::exists(destPath)) {
            QString baseName = srcInfo.completeBaseName();
            QString suffix = srcInfo.suffix();
            int counter = 1;
            do {
                destPath = QDir(albumPath).filePath(
                    QString("%1_%2.%3").arg(baseName).arg(counter).arg(suffix));
                counter++;
            } while (QFile::exists(destPath));
        }
        
        if (QFile::rename(srcPath, destPath)) {
            successCount++;
            movedOldPaths << srcPath;
            movedNewPaths << destPath;
            
            // Update the image path in the tag database
            TagManager::instance()->updateImagePath(srcPath, destPath);
        } else {
            failCount++;
        }
    }
    
    // Create an album tag linked to this folder
    if (successCount > 0) {
        QString tagColor = "#5c6bc0";  // Indigo for album tags
        qint64 tagId = TagManager::instance()->createTag(albumName, tagColor);
        if (tagId >= 0) {
            TagManager::instance()->setTagAlbumPath(tagId, albumPath);
            
            // Tag all moved images with the album tag
            TagManager::instance()->tagImages(movedNewPaths, tagId);
        }
    }
    
    // Refresh the view
    if (!m_currentFolder.isEmpty()) {
        openFolder(m_currentFolder);
    }
    
    // Show result
    if (failCount > 0) {
        QMessageBox::warning(this, "Create Album",
            QString("Created album \"%1\". Moved %2 file(s), %3 failed.")
                .arg(albumName).arg(successCount).arg(failCount));
    } else {
        m_statusLabel->setText(
            QString("Created album \"%1\" with %2 images")
                .arg(albumName).arg(successCount));
    }
}

void MainWindow::onImageTaggedForAlbum(const QString& imagePath, qint64 tagId)
{
    Tag tag = TagManager::instance()->tag(tagId);
    if (!tag.isAlbumTag()) {
        return;
    }
    
    QFileInfo fileInfo(imagePath);
    QDir albumDir(tag.albumPath);
    
    // Ensure album directory exists
    if (!albumDir.exists()) {
        QDir().mkpath(tag.albumPath);
    }
    
    // Check if file is already in the album folder
    if (fileInfo.absolutePath() == albumDir.absolutePath()) {
        return;
    }
    
    // Check if the source file still exists (might have been moved already)
    if (!fileInfo.exists()) {
        return;
    }
    
    // Determine destination path
    QString destPath = albumDir.filePath(fileInfo.fileName());
    
    // Handle filename conflicts
    if (QFile::exists(destPath)) {
        QString baseName = fileInfo.completeBaseName();
        QString suffix = fileInfo.suffix();
        int counter = 1;
        do {
            destPath = albumDir.filePath(
                QString("%1_%2.%3").arg(baseName).arg(counter).arg(suffix));
            counter++;
        } while (QFile::exists(destPath));
    }
    
    // Move the file
    if (QFile::rename(imagePath, destPath)) {
        TagManager::instance()->updateImagePath(imagePath, destPath);
        
        // Schedule a batched view refresh (avoids refreshing per-file in bulk operations)
        if (!m_albumRefreshTimer) {
            m_albumRefreshTimer = new QTimer(this);
            m_albumRefreshTimer->setSingleShot(true);
            m_albumRefreshTimer->setInterval(300);
            connect(m_albumRefreshTimer, &QTimer::timeout, this, [this]() {
                if (!m_currentFolder.isEmpty()) {
                    openFolder(m_currentFolder);
                }
            });
        }
        m_albumRefreshTimer->start();
    }
}

void MainWindow::onTagLinkedToFolder(qint64 tagId, const QString& albumPath)
{
    if (albumPath.isEmpty()) {
        return;  // Tag was unlinked, nothing to move
    }
    
    // Get all images that already have this tag
    QStringList taggedImages = TagManager::instance()->imagesWithTag(tagId);
    if (taggedImages.isEmpty()) {
        return;
    }
    
    QDir albumDir(albumPath);
    if (!albumDir.exists()) {
        QDir().mkpath(albumPath);
    }
    
    int movedCount = 0;
    
    for (const QString& imagePath : taggedImages) {
        QFileInfo fileInfo(imagePath);
        
        // Skip if already in the album folder
        if (fileInfo.absolutePath() == albumDir.absolutePath()) {
            continue;
        }
        
        // Skip if the source file doesn't exist
        if (!fileInfo.exists()) {
            continue;
        }
        
        QString destPath = albumDir.filePath(fileInfo.fileName());
        
        // Handle filename conflicts
        if (QFile::exists(destPath)) {
            QString baseName = fileInfo.completeBaseName();
            QString suffix = fileInfo.suffix();
            int counter = 1;
            do {
                destPath = albumDir.filePath(
                    QString("%1_%2.%3").arg(baseName).arg(counter).arg(suffix));
                counter++;
            } while (QFile::exists(destPath));
        }
        
        if (QFile::rename(imagePath, destPath)) {
            TagManager::instance()->updateImagePath(imagePath, destPath);
            movedCount++;
        }
    }
    
    if (movedCount > 0) {
        Tag tag = TagManager::instance()->tag(tagId);
        m_statusLabel->setText(
            QString("Moved %1 existing image(s) to album \"%2\"")
                .arg(movedCount).arg(tag.name));
        
        // Refresh the view
        if (!m_albumRefreshTimer) {
            m_albumRefreshTimer = new QTimer(this);
            m_albumRefreshTimer->setSingleShot(true);
            m_albumRefreshTimer->setInterval(300);
            connect(m_albumRefreshTimer, &QTimer::timeout, this, [this]() {
                if (!m_currentFolder.isEmpty()) {
                    openFolder(m_currentFolder);
                }
            });
        }
        m_albumRefreshTimer->start();
    }
}

} // namespace FullFrame

