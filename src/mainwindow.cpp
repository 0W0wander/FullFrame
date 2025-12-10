#include "mainwindow.h"
#include <QLabel>
#include <QStatusBar>

namespace FullFrame {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("FullFrame - Image Tagging");
    setMinimumSize(1024, 768);
    
    setupUI();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUI()
{
    m_statusLabel = new QLabel("Ready", this);
    statusBar()->addWidget(m_statusLabel);
    
    setStyleSheet(R"(
        QMainWindow {
            background-color: #1e1e1e;
        }
        QStatusBar {
            background-color: #252525;
            color: #a0a0a0;
        }
    )");
}

} // namespace FullFrame
