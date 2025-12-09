#include <QApplication>
#include <QMainWindow>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("FullFrame");
    
    QMainWindow window;
    window.setWindowTitle("FullFrame");
    window.setMinimumSize(800, 600);
    window.show();
    
    return app.exec();
}
