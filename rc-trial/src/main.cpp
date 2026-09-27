#include <QApplication>
#include <QStandardPaths>

#include "MainWindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("qpb-rc-trial");
    QApplication::setApplicationVersion("0.1");
    MainWindow window(
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/settings.ini");
    window.show();
    return app.exec();
}
