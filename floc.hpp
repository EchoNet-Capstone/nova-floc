#ifndef FLOC_H
#define FLOC_H

/*
 * FLOC Packet Structure (.hpp) - Generator Script Compatibility
 *
 * This header file is designed to be automatically parsed by a Python script
 * (generate_scapy_clang.py) to generate Scapy packet classes.  Follow these
 * conventions:
 *
 * 1. Includes:  Include `<stdint.h>` (or `<cstdint>`) at the top.
 *
 * 2. Macros: (Optional) Use #define for field sizes (e.g., #define FLOC_TTL_SIZE 4).
 *
 * 3. Packet Type Enum:
 *    - Define an enum (named ending in _e) for packet types *before* `HeaderCommon_t`.
 *    - Use a consistent naming convention: `FLOC_<PACKET_NAME>_TYPE`.
 *    - Example:
 *      ```c++
 *      enum FlocPacketType_e{
 *          FLOC_COMMAND_TYPE = 0x1,
 *          FLOC_ACK_TYPE = 0x2,
 *      };
 *      ```
 *
 * 4. HeaderCommon_t:
 *    - MUST be named `HeaderCommon_t`.
 *    - MUST contain a `type` field of the enum type defined in step 3.
 *    - MUST use `__attribute__((packed))`.
 *
 * 5. Packet-Specific Headers:
 *    - MUST embed `HeaderCommon_t common;` as the *first* member.
 *    - MUST be named `<PacketName>Header_t`.
 *    - MUST use `__attribute__((packed))`.
 *
 * 6. Nested Unions (for Command/Response Payloads):
 *    - Use a nested `union` within the packet-specific header for different payloads.
 *    - Create a new enum to describe those nested types.
 *    - Add a new section to the generator script's `generate_bind_layers` function.
 *    - Example:
 *      ```c++
 *      enum FlocCommandID_e {
 *          FLOC_CMD_SET_LED = 0x01,
 *      } ;
 *
 *      struct CommandHeader_t {
 *          HeaderCommon_t common;
 *          FlocCommandID_e command_id; // <- Discriminator field
 *          // ... other fields ...
 *          union {
 *              CmdSetLED_t set_led;
 *              // ... other command structs ...
 *          } cmd_data;
 *      };
 *      ```
 *
 * 7. Top-Level Union (Optional): A union named `FlocPacket_u` can be used.
 *
 * 8. Naming Conventions:
 *    - Structs:  `<PacketName>Header_t`
 *    - Enums: `<EnumPurpose>_e`
 *    - Common header member: `common`
 *    - Discriminator fields: Descriptive names (e.g., `type`, `command_id`).
 *
 * 9.  No Stray Semicolons: Ensure no stray semicolons (`;`) or incomplete definitions before your structs.
 *
 * 10. Avoid Complex C++: Stick to simple C-style constructs within structs.
 */

#include <Arduino.h>
#include <stdint.h>
#include "globals.hpp"
#include "modem_api.hpp"

// --- Macros for field sizes ---
#define FLOC_TTL_SIZE 4
#define FLOC_TYPE_SIZE 4
#define FLOC_NID_SIZE 16  // Network ID (2 bytes)
#define FLOC_RES_SIZE 2
#define FLOC_PID_SIZE 6
#define FLOC_ADDR_SIZE 16 // 2 bytes each for src and dest
#define FLOC_CRC_SIZE 16
#define FLOC_SEQUENCE_SIZE 16

// --- Packet Types (using a named enum) ---
enum FlocPacketType_e {
    FLOC_COMMAND_TYPE = 0x1,
    FLOC_ACK_TYPE = 0x2,
    FLOC_RESPONSE_TYPE = 0x3,
    // Add new types here
};

struct HeaderCommon_t {
    uint8_t ttl : FLOC_TTL_SIZE;
    FlocPacketType_e type : FLOC_TYPE_SIZE;
    uint16_t nid;
    uint8_t pid : FLOC_PID_SIZE;
    uint8_t res : FLOC_RES_SIZE;
    uint16_t dest_addr;
    uint16_t src_addr;
} __attribute__((packed));

struct CommandHeader_t {
    HeaderCommon_t common;
    uint8_t size;
} __attribute__((packed));

struct AckHeader_t {
    HeaderCommon_t common;
    uint8_t ack_pid;
} __attribute__((packed));

struct ResponseHeader_t {
    HeaderCommon_t common;
    uint8_t request_pid;
    uint8_t size;
} __attribute__((packed));

// --- Union for the entire packet ---
typedef union {
    HeaderCommon_t common;
    CommandHeader_t command;
    AckHeader_t ack;
    ResponseHeader_t response;
    uint8_t raw[64];
} FlocPacket_u;

// --- SerialFlocPacket Types (for the outer packet) ---
enum SerialFlocPacketType_e{
    SERIAL_BROADCAST_TYPE = 0x01,  // Encapsulates a FLOC packet
    SERIAL_UNICAST_TYPE   = 0X02,
    // Add other SerialFlocPacket types if needed (e.g., for control)
};

// --- Outer Packet for Serial Communication (SerialFlocPacket) ---
struct SerialFlocHeader_t {
    SerialFlocPacketType_e type;  // Type of SerialFlocPacket (e.g., data, control)
    uint8_t                size;  // Total size of the SerialFlocPacket (including type, size, and data)
} __attribute__((packed));

struct SerialBroadcastHeader_t {
    SerialFlocHeader_t common;
} __attribute__((packed));

struct SerialUnicastHeader_t {
    SerialFlocHeader_t common;
    uint16_t dest_addr;
} __attribute__((packed));

typedef union {
    SerialFlocHeader_t common;
    SerialBroadcastHeader_t broadcast;
    SerialUnicastHdeader_t unicast;
    uint8_t raw[66];
} SerialFlocPacket_u;

void parse_floc_command_packet(char *broadcastBuffer, uint8_t size);
void parse_floc_acknowledgement_packet(char *broadcastBuffer);
void parse_floc_response_packet(char *broadcastBuffer);
void floc_broadcast_received(char *broadcastBuffer, uint8_t size);
void floc_unicast_received(char *unicastBuffer, uint8_t size);
void floc_acknowledgement_send(HardwareSerial connection, uint8_t dest_addr, uint8_t ack_pid);
void floc_status_queue(HardwareSerial connection, uint8_t dest_addr);
void floc_status_send(String status);

#endif
