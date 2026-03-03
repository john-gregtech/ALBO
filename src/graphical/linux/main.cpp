#include <QApplication>
#include "graphical/universal/main_window.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    prototype::graphical::MainWindow window;
    window.show();

    return QApplication::exec();
}
