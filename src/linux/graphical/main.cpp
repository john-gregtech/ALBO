#ifdef _WIN32
    #include <winsock2.h>
#endif
#include <QApplication>
#include "universal/graphical/main_window.h"

int main(int argc, char *argv[]) {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return 1;
    }
#endif

    QApplication app(argc, argv);

    prototype::graphical::MainWindow window;
    window.show();

    int result = QApplication::exec();

#ifdef _WIN32
    WSACleanup();
#endif

    return result;
}
