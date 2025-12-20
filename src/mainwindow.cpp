#include "mainwindow.h"
#include "imagegridview.h"
#include "imagethumbnailmodel.h"
#include "thumbnaildelegate.h"
#include "thumbnailcache.h"
#include "thumbnailloadthread.h"
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QStatusBar>
#include <QLabel>
#include <QVBoxLayout>
#include <QDragEnterEvent>
#include <QMimeData>

namespace FullFrame {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("FullFrame - Image Tagging");
    setMinimumSize(1024, 768);
    setAcceptDrops(true);
    
    setupUI();
    setupMenuBar();
    setupStatusBar();
    
    setStyleSheet(R"(
        QMainWindow { background-color: #1e1e1e; }
        QMenuBar { background-color: #2d2d2d; color: #e0e0e0; }
        QMenu { background-color: #2d2d2d; color: #e0e0e0; }
        QStatusBar { background-color: #252525; color: #a0a0a0; }
    )");
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUI()
{
    QWidget* central = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    
    m_model = new ImageThumbnailModel(this);
    m_gridView = new ImageGridView(this);
    m_gridView->setModel(m_model);
    m_gridView->setItemDelegate(new ThumbnailDelegate(this));
    
    layout->addWidget(m_gridView);
    setCentralWidget(central);
    
    connect(m_gridView, &ImageGridView::imageSelected,
            this, &MainWindow::onImageSelected);
}

void MainWindow::setupMenuBar()
{
    QMenu* fileMenu = menuBar()->addMenu("&File");
    
    QAction* openAction = fileMenu->addAction("&Open Folder...");
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::openFolder);
    
    fileMenu->addSeparator();
    
    QAction* exitAction = fileMenu->addAction("E&xit");
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
}

void MainWindow::setupStatusBar()
{
    m_statusLabel = new QLabel("Ready", this);
    statusBar()->addWidget(m_statusLabel);
}

void MainWindow::openFolder()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Open Folder");
    if (!dir.isEmpty()) {
        loadDirectory(dir);
    }
}

void MainWindow::loadDirectory(const QString& path)
{
    m_currentDirectory = path;
    m_model->setDirectory(path);
    m_statusLabel->setText(QString("%1 images").arg(m_model->rowCount()));
    setWindowTitle(QString("FullFrame - %1").arg(path));
}

void MainWindow::onImageSelected(const QString& path)
{
    m_statusLabel->setText(QFileInfo(path).fileName());
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent* event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    if (!urls.isEmpty()) {
        QString path = urls.first().toLocalFile();
        if (QFileInfo(path).isDir()) {
            loadDirectory(path);
        }
    }
}

} // namespace FullFrame
