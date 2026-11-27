#include "app/MainWindow.h"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("Open Quant Terminal");
    QApplication::setOrganizationName("OpenQuantTerminal");

    MainWindow window;
    window.show();

    return app.exec();
}
