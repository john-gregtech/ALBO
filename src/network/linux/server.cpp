// server.cpp super chopped ai generated code
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

// RAII wrapper for a socket file descriptor
class Socket {
    int fd_;
public:
    explicit Socket(int fd = -1) : fd_(fd) {}
    ~Socket() { if (fd_ != -1) close(fd_); }
    int get() const { return fd_; }
    void reset(int fd = -1) { 
        if (fd_ != -1) close(fd_); 
        fd_ = fd; 
    }
    // disable copy
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    // enable move
    Socket(Socket&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }
    Socket& operator=(Socket&& other) noexcept { 
        if (this != &other) { 
            reset(other.fd_); 
            other.fd_ = -1; 
        }
        return *this;
    }
};

int main() {
    constexpr int PORT = 4000;
    Socket server_fd(::socket(AF_INET, SOCK_STREAM, 0));
    if (server_fd.get() == -1) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    if (setsockopt(server_fd.get(), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt");
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(server_fd.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
        perror("bind");
        return 1;
    }

    if (listen(server_fd.get(), 10) == -1) {
        perror("listen");
        return 1;
    }

    std::cout << "Server listening on port " << PORT << "...\n";

    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        Socket client_fd(::accept(server_fd.get(), reinterpret_cast<sockaddr*>(&client_addr), &client_len));
        if (client_fd.get() == -1) {
            perror("accept");
            continue;
        }

        char buffer[1024];
        ssize_t n = read(client_fd.get(), buffer, sizeof(buffer)-1);
        if (n > 0) {
            buffer[n] = '\0';
            std::cout << "Received: " << buffer << "\n";

            std::string reply = "Echo: ";
            reply += buffer;
            send(client_fd.get(), reply.c_str(), reply.size(), 0);
        }
    }

    return 0;
}
