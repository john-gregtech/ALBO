#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>

// #pragma comment(lib, "ws2_32.lib")

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }

    SOCKET ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ListenSocket == INVALID_SOCKET) {
        std::cerr << "Error creating socket\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(5555); // Change port if needed

    if (bind(ListenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed\n";
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    if (listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed\n";
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server listening on port 5555...\n";

    while (true) {
        SOCKET ClientSocket = accept(ListenSocket, nullptr, nullptr);
        if (ClientSocket == INVALID_SOCKET) {
            std::cerr << "Accept failed\n";
            break;
        }

        const char* reply = "Hello";
        if (send(ClientSocket, reply, (int)strlen(reply), 0) == 5)
            std::cout << "connection\n";
        closesocket(ClientSocket);
    }

    closesocket(ListenSocket);
    WSACleanup();
    return 0;
}