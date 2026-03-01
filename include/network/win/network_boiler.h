//This file is used for both server and client startup procedure, it could be split if it is longer then i want
#include <winsock2.h>
#include <ws2tcpip.h> // for inet_pton
#include <iostream>
#include <string>
#include <cstdint>
#include <vector>

namespace prototype::network {
    struct Ip {
        uint32_t ip_host{};// example of how it would look 0xC0A80105;
        uint32_t ip_network() const;
        std::string toString() const;
    };
    

    //Socket Information such as holding information and status and passing data through
    class Socket {
    public:
        Socket(SOCKET s, const Ip& ip = {}, int port = 0) : handle(s), open(true), peerIp(ip), peerPort(port) {}
        ~Socket() { close(); }

        Socket(const Socket&) = delete;
        Socket& operator=(const Socket&) = delete;
        Socket(Socket&&) = default;
        Socket& operator=(Socket&&) = default;

        bool sendData(const char* data, int length);
        int receiveData(char* buffer, int length);
        void close();
        bool isOpen() const;
        Ip getPeerIp() const;
        int getPeerPort() const;
    private:
        SOCKET handle;
        bool open;
        Ip peerIp; //optional
        int peerPort; //optional
    };

    //Manages Sockets based on what we want them to do closes and opens sockets and stores an index of them
    std::vector<Socket> socket_list{};

    //(we need a ) + x
    //we need a list of sockets


    //This class will contain basic server information such as pointers to targeted ports/sockets and ip
    //Later down the line this can be expanded
    class Server {
    public:

    private:

    };

    //Similar situation to the server class but Client focused
    class Client {
    public:

    private:

    };
}
