#include <QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    app.setApplicationName("diyPresso Manager");
    app.setApplicationVersion("1.2.0");
    app.setOrganizationName("diyPresso");
    
    MainWindow window;
    window.show();
    
    return app.exec();
} 