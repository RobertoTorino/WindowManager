#include <QApplication>

#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QApplication::setApplicationName("WindowManager");
    QApplication::setOrganizationName("WindowManager");

    MainWindow window;
    window.show();

    return app.exec();
}