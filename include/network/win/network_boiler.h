//This file is used for both server and client startup procedure, it could be split if it is longer then i want
#include <winsock2.h>
#include <ws2tcpip.h> // for inet_pton
#include <iostream>
#include <string>
#include <vector>

struct Ip {
    uint32_t ip_host = 0xC0A80105;         // host order
    uint32_t ip_network = htonl(ip_host);  // network order
};

//Socket Information such as holding information and status and passing data through
class Socket {
public:

private:
    SOCKET handle;
    bool open;
    std::string peerID;
    int peerPort;
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

