#ifndef FLOC_H
#define FLOC_H

#include <stdint.h>

#ifdef ON_DEVICE
#include "globals.hpp"
#include "modem_api.hpp"
#include <Arduino.h>
#endif // ON_DEVICE

// --- Configuration (Maximum Sizes) ---
#define FLOC_MAX_SIZE 64  // Maximum size of a complete FLOC packet
#define SERIAL_FLOC_MAX_SIZE 64 // Maximum size for a serial floc packet.

// --- Macros for field sizes (optional, for documentation) ---
#define FLOC_TTL_SIZE 4
#define FLOC_TYPE_SIZE 4
#define FLOC_NID_SIZE 16
#define FLOC_RES_SIZE 2
#define FLOC_PID_SIZE 6
#define FLOC_ADDR_SIZE 16

// --- Packet Type Enums ---

// The 4 types of floc packets
enum FlocPacketType_e {
    FLOC_DATA_TYPE = 0x0,
    FLOC_COMMAND_TYPE = 0x1,
    FLOC_ACK_TYPE = 0x2,
    FLOC_RESPONSE_TYPE = 0x3
};

enum CommandType_e {  // Example
    COMMAND_TYPE_1 = 0x1,
    COMMAND_TYPE_2 = 0x2,
    // ...
};

enum SerialFlocPacketType_e {
    SERIAL_BROADCAST_TYPE = 'B',
    SERIAL_UNICAST_TYPE   = 'U',
    // ...
};

// --- FLOC Packet Structures ---

// 1. Common Header
struct FlocHeader_t {
    uint8_t ttl : FLOC_TTL_SIZE;
    FlocPacketType_e type : FLOC_TYPE_SIZE;
    uint16_t nid;
    uint8_t pid : FLOC_PID_SIZE;
    uint8_t res : FLOC_RES_SIZE;
    uint16_t dest_addr;
    uint16_t src_addr;
} __attribute__((packed));

// 2. Specific Headers
struct DataHeader_t {
    uint8_t size;
} __attribute__((packed));

struct CommandHeader_t {
    CommandType_e command_type;
    uint8_t size;  // Size of the command data
} __attribute__((packed));

struct AckHeader_t {
    uint8_t ack_pid;
} __attribute__((packed));

struct ResponseHeader_t {
    uint8_t request_pid;
    uint8_t size;  // Size of the response data
} __attribute__((packed));

// --- Calculate Maximum Data Sizes ---
// This is the key improvement:  We calculate the maximum data sizes
// *statically*, based on FLOC_MAX_SIZE and the sizes of the headers.

#define FLOC_HEADER_COMMON_SIZE (sizeof(FlocHeader_t))

#define DATA_HEADER_SIZE        (sizeof(DataHeader_t))
#define COMMAND_HEADER_SIZE     (sizeof(CommandHeader_t))
#define RESPONSE_HEADER_SIZE    (sizeof(ResponseHeader_t))
#define ACK_HEADER_SIZE         (sizeof(AckHeader_t))

#define MAX_DATA_DATA_SIZE      (FLOC_MAX_SIZE - FLOC_HEADER_COMMON_SIZE - DATA_HEADER_SIZE)
#define MAX_COMMAND_DATA_SIZE   (FLOC_MAX_SIZE - FLOC_HEADER_COMMON_SIZE - COMMAND_HEADER_SIZE)
#define MAX_RESPONSE_DATA_SIZE  (FLOC_MAX_SIZE - FLOC_HEADER_COMMON_SIZE - RESPONSE_HEADER_SIZE)
#define MAX_ACK_DATA_SIZE       (FLOC_MAX_SIZE - FLOC_HEADER_COMMON_SIZE - ACK_HEADER_SIZE)
// Ack packets don't have a data field in this example, but you could define a MAX_ACK_DATA_SIZE if needed.

// --- Complete FLOC Packet Structures ---
struct DataPacket_t {
    DataHeader_t header;
    uint8_t data[MAX_DATA_DATA_SIZE];
} __attribute__((packed));

struct CommandPacket_t {
    CommandHeader_t header;
    uint8_t data[MAX_COMMAND_DATA_SIZE];  // Statically allocated, maximum size
} __attribute__((packed));

struct AckPacket_t {
    AckHeader_t header;
    //  uint8_t data[MAX_ACK_DATA_SIZE]; // If you add data to acks
} __attribute__((packed));

struct ResponsePacket_t {
    ResponseHeader_t header;
    uint8_t data[MAX_RESPONSE_DATA_SIZE]; // Statically allocated, maximum size
} __attribute__((packed));

union FlocPacketVariant_u {
    DataPacket_t     data;
    CommandPacket_t  command;
    AckPacket_t      ack;
    ResponsePacket_t response;
};

struct FlocPacket_t {
    FlocHeader_t header;
    FlocPacketVariant_u payload;
} __attribute__((packed));

// --- Serial FLOC Packet Structures ---
struct SerialFlocHeader_t {
    SerialFlocPacketType_e type;
    uint8_t                size;
} __attribute__((packed));

struct SerialBroadcastPacket_t {
    FlocPacketVariant_u floc_packet;
} __attribute__((packed));

struct SerialUnicastPacket_t {
    uint16_t dest_addr;
    FlocPacketVariant_u floc_packet;
} __attribute__((packed));

union SerialFlocPacketVariant_u {
    SerialBroadcastPacket_t broadcast;
    SerialUnicastPacket_t unicast;
};

struct SerialFlocPacket {
    SerialFlocHeader_t header;
    SerialFlocPacketVariant_u payload;
} __attribute__((packed));

#ifdef ON_DEVICE
void parse_floc_command_packet(char *broadcastBuffer, uint8_t size);
void parse_floc_acknowledgement_packet(char *broadcastBuffer);
void parse_floc_response_packet(char *broadcastBuffer);
void floc_broadcast_received(char *broadcastBuffer, uint8_t size);
void floc_unicast_received(char *unicastBuffer, uint8_t size);
void floc_acknowledgement_send(HardwareSerial connection, uint8_t dest_addr, uint8_t ack_pid);
void floc_status_queue(HardwareSerial connection, uint8_t dest_addr);
void floc_status_send(String status);
#endif // ON_DEVICE

#endif