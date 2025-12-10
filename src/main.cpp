#include <QApplication>
#include <QStyleFactory>
#include "mainwindow.h"

using namespace FullFrame;

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("FullFrame");
    app.setStyle(QStyleFactory::create("Fusion"));
    
    MainWindow window;
    window.show();
    
    return app.exec();
}
