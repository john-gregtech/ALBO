#pragma once
#include <cstdint>
#include <array>

namespace prototype::network {
    // Every ALBO packet MUST start with these 4 bytes
    constexpr std::array<uint8_t, 4> MAGIC_BYTES = {0x41, 0x4C, 0x42, 0x4F}; // "ALBO"
    constexpr uint8_t PROTOCOL_VERSION = 0x01;

    enum class PacketType : uint8_t {
        AUTH_CHALLENGE    = 0x00, // Server -> Client
        AUTH_RESPONSE     = 0x01, // Client -> Server
        LOGIN_REQUEST     = 0x02,
        LOGIN_SUCCESS     = 0x03,
        LOGIN_FAIL        = 0x04,
        REGISTER_REQUEST  = 0x05,
        REGISTER_SUCCESS  = 0x06,
        REGISTER_FAIL     = 0x07,
        MESSAGE_DATA      = 0x08, 
        FILE_HEADER       = 0x09, // Size, Total Checksum, Chunk Count, Filename
        FILE_CHUNK        = 0x0A, // Chunk Index, Chunk Checksum, Data
        FILE_FOOTER       = 0x0B, // End of transfer
        PING              = 0x0C,
        ROUTE_FAIL        = 0x0D,
        RESOLVE_FAIL      = 0x0E,
        INBOX_FETCH       = 0x0F,
        PREKEY_UPLOAD     = 0x10,
        PREKEY_FETCH      = 0x11,
        PREKEY_RESPONSE   = 0x12,
        GROUP_CREATE      = 0x13,
        GROUP_INVITE      = 0x14,
        GROUP_MSG         = 0x15,
        CONTACT_ADD       = 0x16, // Client -> Server (Payload: Username)
        CONTACT_REMOVE    = 0x17,
        CONTACT_LIST_REQ  = 0x18, // Client -> Server
        CONTACT_LIST_RESP = 0x19, // Server -> Client (Payload: serialized list)
        DISCONNECT        = 0xFF
    };

    #pragma pack(push, 1) // Ensure no padding for the wire
    struct PacketHeader {
        uint8_t magic[4];      // Must be "ALBO"
        uint8_t version;       // Protocol version
        PacketType type;       // What kind of packet is this?
        uint32_t payload_size; // How many bytes follow this header?
        uint64_t target_high;  // UUID (High)
        uint64_t target_low;   // UUID (Low)
        char sender_name[16];  // Sender's username for display
    };
    #pragma pack(pop)

    constexpr size_t HEADER_SIZE = sizeof(PacketHeader); // 42 Bytes now
}
