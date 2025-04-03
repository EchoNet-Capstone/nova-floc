#ifndef FLOC_H
#define FLOC_H

#include <stdint.h>
#include <globals.hpp>
#include <device_actions.hpp>
#include <nmv3_api.hpp>
#include <Arduino.h>
#include <motor.hpp>

// -- Defaults ---
#define TTL_START 3

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

#define COMMAND_TYPE_SIZE 8

#define SERIAL_FLOC_TYPE_SIZE 8

// --- Packet Structures ---
#pragma pack(push, 1)
// --- Packet Type Enums ---

// The 4 types of floc packets
typedef enum FlocPacketType_e : uint8_t{
    FLOC_DATA_TYPE = 0x0,
    FLOC_COMMAND_TYPE = 0x1,
    FLOC_ACK_TYPE = 0x2,
    FLOC_RESPONSE_TYPE = 0x3
  };
  
typedef enum CommandType_e: uint8_t {  // Example
    COMMAND_TYPE_1 = 0x1,
    COMMAND_TYPE_2 = 0x2,
    // ...
};

typedef enum SerialFlocPacketType_e: uint8_t {
    SERIAL_BROADCAST_TYPE = 'B',
    SERIAL_UNICAST_TYPE   = 'U',
    // ...
};

// --- FLOC Packet Headers ---
typedef struct FlocHeader_t {
    uint8_t ttl : FLOC_TTL_SIZE;
    FlocPacketType_e type : FLOC_TYPE_SIZE;
    uint16_t nid: FLOC_NID_SIZE;
    uint8_t res : FLOC_RES_SIZE;
    uint8_t pid : FLOC_PID_SIZE;
    uint16_t dest_addr;
    uint16_t src_addr;
};

typedef struct DataHeader_t {
    uint8_t size;
};

typedef struct CommandHeader_t {
    CommandType_e command_type: COMMAND_TYPE_SIZE;
    uint8_t size;  // Size of the command data
};

typedef struct AckHeader_t {
    uint8_t ack_pid;
};

typedef struct ResponseHeader_t {
    uint8_t request_pid;
    uint8_t size;  // Size of the response data
};

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
typedef struct DataPacket_t {
    DataHeader_t header;
    uint8_t data[MAX_DATA_DATA_SIZE];
};

typedef struct CommandPacket_t {
    CommandHeader_t header;
    uint8_t data[MAX_COMMAND_DATA_SIZE];  // Statically allocated, maximum size
};

typedef struct AckPacket_t {
    AckHeader_t header;
    // uint8_t data[MAX_ACK_DATA_SIZE]; // If you add data to acks
};

typedef struct ResponsePacket_t {
    ResponseHeader_t header;
    uint8_t data[MAX_RESPONSE_DATA_SIZE]; // Statically allocated, maximum size
};

typedef union FlocPacketVariant_u {
    DataPacket_t     data;
    CommandPacket_t  command;
    AckPacket_t      ack;
    ResponsePacket_t response;
};

typedef struct FlocPacket_t {
    FlocHeader_t header;
    FlocPacketVariant_u payload;
};

typedef struct SerialUnicastPacket_t {
    uint16_t dest_addr;
    FlocPacketVariant_u floc_packet;
};

typedef struct SerialBroadcastPacket_t {
    FlocPacketVariant_u floc_packet;
};

// Now define the union with complete types.
typedef union SerialFlocPacketVariant_u {
  typedef struct SerialBroadcastPacket_t broadcast;
  typedef struct SerialUnicastPacket_t unicast;
};

// --- Serial FLOC Structures ---
// Define the header first.
typedef struct SerialFlocHeader_t {
    SerialFlocPacketType_e type: SERIAL_FLOC_TYPE_SIZE;
    uint8_t                size;
};

typedef struct SerialFlocPacket_t {
    SerialFlocHeader_t header;
    SerialFlocPacketVariant_u payload;
};
#pragma pack(pop)

uint16_t get_network_id();
uint8_t use_packet_id();
uint16_t get_device_id();

void floc_status_query(uint8_t dest_addr);

void floc_acknowledgement_send(uint8_t ttl, uint8_t ack_pid, uint16_t dest_addr);
void floc_status_send(QueryStatusResponseFullPacket_t* statusResponse);

void parse_floc_command_packet(FlocHeader_t* floc_header, CommandPacket_t* pkt, uint8_t size, DeviceAction_t* da) ;
void parse_floc_acknowledgement_packet(FlocHeader_t* floc_header, AckPacket_t* pkt, uint8_t size, DeviceAction_t* da);
void parse_floc_response_packet(FlocHeader_t* floc_header, ResponsePacket_t* pkt, uint8_t size, DeviceAction_t* da);

void floc_broadcast_received(uint8_t* broadcastBuffer, uint8_t size, DeviceAction_t* da);
void floc_unicast_received(uint8_t* unicastBuffer, uint8_t size, DeviceAction_t* da);

void packet_received_nest(uint8_t* packetBuffer, uint8_t size, DeviceAction_t* da);

#endif