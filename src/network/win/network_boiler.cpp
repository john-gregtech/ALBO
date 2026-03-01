#include "network/win/network_boiler.h"

namespace prototype::network {

    uint32_t Ip::ip_network() const { return htonl(ip_host); }
    std::string Ip::toString() const {
        struct in_addr addr;
        addr.S_un.S_addr = ip_network(); // network byte order
        char buf[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &addr, buf, INET_ADDRSTRLEN);
        return std::string(buf);
    }

    bool Socket::sendData(const char* data, int length) {
        if (!open) return false;
        int result = send(handle, data, length, 0);
        if (result == SOCKET_ERROR) {
            open = false;
            return false;
        }
        return true;
    }
    int Socket::receiveData(char* buffer, int length) {
        if (!open) return -1;
        int result = recv(handle, buffer, length, 0);
        if (result <= 0) { // 0 = connection closed, <0 = error
            open = false;
        }
        return result;
    }
    void Socket::close() {
        if (open) {
            closesocket(handle);
            open = false;
        }
    }
    bool Socket::isOpen() const { return open; }
    Ip Socket::getPeerIp() const { return peerIp; }
    int Socket::getPeerPort() const { return peerPort; }

}