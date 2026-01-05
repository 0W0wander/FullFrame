/**
 * FullFrame - High-performance Image Tagging Application
 * 
 * Main entry point
 * 
 * Architecture inspired by DigiKam:
 * - Threaded thumbnail loading (ThumbnailLoadThread)
 * - Multi-level caching (ThumbnailCache)
 * - Lazy loading in view (ImageGridView)
 * - Efficient model/view pattern (ImageThumbnailModel)
 */

#include <QApplication>
#include <QStyleFactory>
#include <QSurfaceFormat>
#include <QDebug>
#include <QTimer>
#include <QIcon>
#include <iostream>
#include <fstream>
#include <chrono>

#include "mainwindow.h"
#include "thumbnailcache.h"
#include "thumbnailloadthread.h"
#include "tagmanager.h"

// #region agent log
inline void agent_log(const std::string& message, const std::string& hypothesisId, const std::string& location, const std::string& data = "{}") {
    std::ofstream logFile("c:\\code\\FullFrame\\.cursor\\debug.log", std::ios::app);
    if (logFile.is_open()) {
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        logFile << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"" << hypothesisId 
                << "\",\"location\":\"" << location << "\",\"message\":\"" << message 
                << "\",\"data\":" << data << ",\"timestamp\":" << now << "}" << std::endl;
    } else {
        std::cerr << "FAILED TO OPEN LOG FILE: c:\\code\\FullFrame\\.cursor\\debug.log" << std::endl;
    }
}
// #endregion

using namespace FullFrame;

int main(int argc, char *argv[])
{
    // #region agent log
    agent_log("Main started", "A/D", "main.cpp:27");
    // #endregion
    std::cout << "Starting FullFrame..." << std::endl;
    
    // Enable high DPI support
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    // #region agent log
    agent_log("Creating QApplication", "A", "main.cpp:35");
    // #endregion
    QApplication app(argc, argv);
    // #region agent log
    agent_log("QApplication created", "A", "main.cpp:37");
    // #endregion
    
    std::cout << "QApplication created" << std::endl;
    
    // Application metadata
    app.setApplicationName("FullFrame");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("FullFrame");
    app.setOrganizationDomain("fullframe.app");
    
    // Set application icon (shows in taskbar and window title)
    app.setWindowIcon(QIcon(":/icons/icon256.png"));

    // Use Fusion style for consistent cross-platform look
    app.setStyle(QStyleFactory::create("Fusion"));

    // Dark palette
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(30, 30, 30));
    darkPalette.setColor(QPalette::WindowText, QColor(224, 224, 224));
    darkPalette.setColor(QPalette::Base, QColor(45, 45, 45));
    darkPalette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ToolTipBase, QColor(45, 45, 45));
    darkPalette.setColor(QPalette::ToolTipText, QColor(224, 224, 224));
    darkPalette.setColor(QPalette::Text, QColor(224, 224, 224));
    darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ButtonText, QColor(224, 224, 224));
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::Highlight, QColor(0, 120, 215));
    darkPalette.setColor(QPalette::HighlightedText, Qt::white);
    darkPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(128, 128, 128));
    darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(128, 128, 128));
    app.setPalette(darkPalette);

    std::cout << "Palette set" << std::endl;

    // Initialize singletons
    // #region agent log
    agent_log("Initializing singletons", "B", "main.cpp:70");
    // #endregion
    ThumbnailCache::instance();
    ThumbnailLoadThread::instance();

    // Configure cache sizes based on available memory
    // Default: 500 images, 200 pixmaps
    ThumbnailCache::instance()->setImageCacheSize(500);
    ThumbnailCache::instance()->setPixmapCacheSize(200);

    std::cout << "Creating MainWindow..." << std::endl;
    
    // Create and show main window
    // #region agent log
    agent_log("Creating MainWindow", "C", "main.cpp:81");
    // #endregion
    MainWindow mainWindow;
    // #region agent log
    agent_log("MainWindow created", "C", "main.cpp:83");
    // #endregion
    
    std::cout << "MainWindow created, showing..." << std::endl;
    
    mainWindow.show();
    
    // Stability monitor: prints heartbeat to terminal for the first 10 seconds
    QTimer heartbeatTimer;
    int elapsed = 0;
    QObject::connect(&heartbeatTimer, &QTimer::timeout, [&elapsed]() {
        elapsed++;
        if (elapsed <= 10) {
            std::cout << "FullFrame stability monitor: " << elapsed << "s (running)" << std::endl;
        }
        if (elapsed == 10) {
            std::cout << "SUCCESS: Application has passed the 10-second stability test!" << std::endl;
        }
    });
    heartbeatTimer.start(1000);

    // #region agent log
    agent_log("Starting event loop", "A/E", "main.cpp:101");
    // #endregion
    int result = app.exec();
    // #region agent log
    agent_log("Event loop finished", "A/E", "main.cpp:103", "{\"result\":" + std::to_string(result) + "}");
    // #endregion

    // Cleanup singletons
    ThumbnailLoadThread::cleanup();
    ThumbnailCache::cleanup();
    TagManager::cleanup();

    return result;
}

