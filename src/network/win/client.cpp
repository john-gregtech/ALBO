// Minimal client example
#include <winsock2.h>
#include <iostream>
// #pragma comment(lib, "ws2_32.lib")

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(5555);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    connect(s, (sockaddr*)&addr, sizeof(addr));

    char buffer[512] = {};
    int received = recv(s, buffer, sizeof(buffer), 0);
    if (received > 0)
        std::cout << "Received: " << std::string(buffer, received) << "\n";

    closesocket(s);
    WSACleanup();
}
