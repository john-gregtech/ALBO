#include "network/win/network_boiler.h"

namespace prototype::network {

    uint32_t Ip::ip_network() const { return htonl(ip_host); }

}