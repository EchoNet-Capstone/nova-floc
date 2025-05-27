#pragma once

#include <stdint.h>

// -- Defaults ---
#define TTL_START 3

// --- Configuration (Maximum Sizes) ---
#define FLOC_MAX_SIZE 64  // Maximum size of a complete FLOC packet

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
typedef enum
FlocPacketType_e : uint8_t {
    FLOC_DATA_TYPE = 0x0,
    FLOC_COMMAND_TYPE = 0x1,
    FLOC_ACK_TYPE = 0x2,
    FLOC_RESPONSE_TYPE = 0x3
};

typedef enum
CommandType_e: uint8_t {  // Example
    COMMAND_TYPE_1 = 0x1,
    COMMAND_TYPE_2 = 0x2,
    // ...
};

typedef enum
SerialFlocPacketDirection_e: uint8_t {
    SERIAL_NEST_TO_BURD_TYPE = '$',
    SERIAL_BURD_TO_NEST_TYPE = '#',
};

typedef enum
SerialFlocPacketType_e: uint8_t {
    SERIAL_BROADCAST_TYPE = 'B',
    SERIAL_UNICAST_TYPE   = 'U',
    // ...
};

// --- FLOC Packet Headers ---
typedef struct
FlocHeader_t {
    FlocPacketType_e type : FLOC_TYPE_SIZE;
    uint8_t ttl : FLOC_TTL_SIZE;
    uint16_t nid: FLOC_NID_SIZE;
    uint8_t res : FLOC_RES_SIZE;
    uint8_t pid : FLOC_PID_SIZE;
    uint16_t dest_addr;
    uint16_t src_addr;
};

typedef struct
DataHeader_t {
    uint8_t size;
};

typedef struct
CommandHeader_t {
    CommandType_e command_type: COMMAND_TYPE_SIZE;
    uint8_t size;  // Size of the command data
};

typedef struct
AckHeader_t {
    uint8_t ack_pid;
#ifdef ACK_DATA // ACK_DATA
    uint8_t size;
#endif // ACK_DATA
};

typedef struct
ResponseHeader_t {
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
// Ack packets don't have a data field in this example, but you could define a MAX_ACK_DATA_SIZE if needed.
#ifdef ACK_DATA // ACK_DATA
#define MAX_ACK_DATA_SIZE       (FLOC_MAX_SIZE - FLOC_HEADER_COMMON_SIZE - ACK_HEADER_SIZE)
#else
#define MAX_ACK_DATA_SIZE       0
#endif //ACK_DATA


// --- Complete FLOC Packet Structures ---
typedef struct
DataPacket_t {
    DataHeader_t header;
    uint8_t payload[MAX_DATA_DATA_SIZE];
};

typedef struct
CommandPacket_t {
    CommandHeader_t header;
    uint8_t payload[MAX_COMMAND_DATA_SIZE];  // Statically allocated, maximum size
};

typedef struct
AckPacket_t {
    AckHeader_t header;
    // If you add data to acks
#ifdef ACK_DATA // ACK_DATA
    uint8_t payload[MAX_ACK_DATA_SIZE];
#endif // ACK_DATA
};

typedef struct
ResponsePacket_t {
    ResponseHeader_t header;
    uint8_t payload[MAX_RESPONSE_DATA_SIZE]; // Statically allocated, maximum size
};

typedef union
FlocPacketVariant_u {
    DataPacket_t     data;
    CommandPacket_t  command;
    AckPacket_t      ack;
    ResponsePacket_t response;
};

typedef struct
FlocPacket_t {
    FlocHeader_t header;
    FlocPacketVariant_u payload;
};

typedef struct
SerialUnicastPacket_t {
    uint16_t dest_addr;
    FlocPacketVariant_u floc_packet;
};

typedef struct
SerialBroadcastPacket_t {
    FlocPacketVariant_u floc_packet;
};

// Now define the union with complete types.
typedef union
SerialFlocPacketVariant_u {
  typedef struct SerialBroadcastPacket_t broadcast;
  typedef struct SerialUnicastPacket_t unicast;
};

// --- Serial FLOC Structures ---
#define SERIAL_FLOC_NEST_TO_BURD_PRE    '$'
#define SERIAL_FLOC_BURD_TO_NEST_PRE    '#'
#define SERIAL_FLOC_PRE_SIZE            1

// Define the header first.
typedef struct
SerialFlocHeader_t {
    SerialFlocPacketType_e type: SERIAL_FLOC_TYPE_SIZE;
    uint8_t                size;
};

#define SERIAL_FLOC_HEADER_SIZE     (sizeof(SerialFlocHeader_t))

typedef struct
SerialFlocPacket_t {
    SerialFlocHeader_t header;
    SerialFlocPacketVariant_u payload;
};

#pragma pack(pop)

// --- Calculate Packet Sizes

// Maximums
#define FLOC_PACKET_MAX_SIZE                (sizeof(FlocPacket_t))
#define DATA_PACKET_MAX_SIZE                (FLOC_HEADER_COMMON_SIZE + DATA_HEADER_SIZE + MAX_DATA_DATA_SIZE)
#define COMMAND_PACKET_MAX_SIZE             (FLOC_HEADER_COMMON_SIZE + COMMAND_HEADER_SIZE + MAX_COMMAND_DATA_SIZE)
#define RESPONSE_PACKET_MAX_SIZE            (FLOC_HEADER_COMMON_SIZE + RESPONSE_HEADER_SIZE + MAX_RESPONSE_DATA_SIZE)
#define ACK_PACKET_MAX_SIZE                 (FLOC_HEADER_COMMON_SIZE + ACK_HEADER_SIZE + MAX_ACK_DATA_SIZE)

#define SERIAL_FLOC_MAX_SIZE                (SERIAL_FLOC_PRE_SIZE + SERIAL_FLOC_HEADER_SIZE + FLOC_PACKET_MAX_SIZE) // Maximum size for a serial floc packet.

// Actuals (with packet)
#define DATA_PACKET_ACTUAL_SIZE(pkt)        (FLOC_HEADER_COMMON_SIZE + DATA_HEADER_SIZE + (pkt)->payload.data.header.size)
#define COMMAND_PACKET_ACTUAL_SIZE(pkt)     (FLOC_HEADER_COMMON_SIZE + COMMAND_HEADER_SIZE + (pkt)->payload.command.header.size)
#define RESPONSE_PACKET_ACTUAL_SIZE(pkt)    (FLOC_HEADER_COMMON_SIZE + RESPONSE_HEADER_SIZE + (pkt)->payload.response.header.size)
#ifdef ACK_DATA // ACK_DATA
#define ACK_PACKET_ACTUAL_SIZE(pkt)         (FLOC_HEADER_COMMON_SIZE + ACK_HEADER_SIZE + (pkt)->payload.ack.header.size)
#else
#define ACK_PACKET_ACTUAL_SIZE(pkt)         (FLOC_HEADER_COMMON_SIZE + ACK_HEADER_SIZE)
#endif // ACK_DATA

#define SERIAL_FLOC_ACTUAL_SIZE(pkt)        (SERIAL_FLOC_HEADER_SIZE + (pkt)->header.size)

// ----- Device Action -----
struct
DeviceAction_t {
    uint8_t modemRespType;
    uint16_t srcAddr;
    uint8_t flocType;
    uint8_t commandType;
    uint8_t dataSize;
    uint8_t* data;
};

extern DeviceAction_t da;

void
init_da(
    void
);

extern void
act_upon(
    void
);

// ----- FLOC Protocol Functions -----

uint16_t
get_network_id(
    void
);

void
set_network_id(
    uint16_t new_network_id
);

uint16_t
get_device_id(
    void
);

void
set_device_id(
    uint16_t new_device_id
);

uint8_t
use_packet_id(
    void
);

void
floc_status_query(
    uint8_t dest_addr
);

void
floc_acknowledgement_send(
    uint8_t ttl,
    uint8_t ack_pid,
    uint16_t dest_addr
);

void
floc_status_send(
    uint8_t node_addr,
    float supply_voltage
);

void
floc_error_send(
    uint8_t ttl,
    uint8_t err_pid,
    uint8_t err_dst_addr
);

void
parse_floc_command_packet(
    FlocHeader_t* floc_header,
    CommandPacket_t* pkt,
    uint8_t size
);

void
parse_floc_acknowledgement_packet(
    FlocHeader_t* floc_header,
    AckPacket_t* pkt,
    uint8_t size
);

void
parse_floc_response_packet(
    FlocHeader_t* floc_header,
    ResponsePacket_t* pkt,
    uint8_t size
);

void
floc_broadcast_received(
    uint8_t* broadcastBuffer,
    uint8_t size
);

void
floc_unicast_received(
    uint8_t* unicastBuffer,
    uint8_t size
);