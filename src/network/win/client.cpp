// client.cpp - Minimal Winsock TCP client
#include <winsock2.h>
#include <ws2tcpip.h> // for inet_pton
#include <iostream>
#include <string>

#pragma comment(lib, "ws2_32.lib") // Link Winsock library

int main() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Failed to create socket: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(5555);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

    if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Connection failed: " << WSAGetLastError() << "\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    std::cout << "Connected to server!\n";

    // Example: Send a message to server
    std::string msg = "Hello from client!";
    int sent = send(sock, msg.c_str(), static_cast<int>(msg.size()), 0);
    if (sent == SOCKET_ERROR) {
        std::cerr << "Send failed: " << WSAGetLastError() << "\n";
    }

    // Receive message from server
    char buffer[512] = {};
    int received = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (received > 0) {
        buffer[received] = '\0'; // Null-terminate
        std::cout << "Received: " << buffer << "\n";
    } else if (received == 0) {
        std::cout << "Server closed connection\n";
    } else {
        std::cerr << "Recv failed: " << WSAGetLastError() << "\n";
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}