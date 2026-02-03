#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <string>

int main() {
    constexpr int PORT = 5555;
    constexpr const char* SERVER_IP = "192.168.1.104";

    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &server.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock);
        return 1;
    }

    // Connect to server
    if (connect(sock, reinterpret_cast<sockaddr*>(&server), sizeof(server)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }

    std::string message = "Hello from C++20 client!\n";
    send(sock, message.c_str(), message.size(), 0);

    char buffer[1024]{};
    ssize_t bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes > 0) {
        std::cout << "Server replied: " << buffer << "\n";
    }

    close(sock);
    return 0;
}
