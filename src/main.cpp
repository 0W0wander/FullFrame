#include <QApplication>
#include <QStyleFactory>
#include "mainwindow.h"
#include "thumbnailcache.h"
#include "thumbnailloadthread.h"

using namespace FullFrame;

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("FullFrame");
    app.setApplicationVersion("1.0.0");
    app.setStyle(QStyleFactory::create("Fusion"));
    
    // Dark palette
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(30, 30, 30));
    darkPalette.setColor(QPalette::WindowText, QColor(224, 224, 224));
    darkPalette.setColor(QPalette::Base, QColor(45, 45, 45));
    darkPalette.setColor(QPalette::Text, QColor(224, 224, 224));
    darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ButtonText, QColor(224, 224, 224));
    darkPalette.setColor(QPalette::Highlight, QColor(0, 120, 215));
    darkPalette.setColor(QPalette::HighlightedText, Qt::white);
    app.setPalette(darkPalette);
    
    ThumbnailCache::instance();
    ThumbnailLoadThread::instance();
    
    MainWindow window;
    window.show();
    
    int result = app.exec();
    
    ThumbnailLoadThread::cleanup();
    ThumbnailCache::cleanup();
    
    return result;
}
