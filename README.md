# FLOC Protocol Library for Underwater Communication

This library provides a C++ implementation of the FLOC (Frequency Locked Oceanic Communication) protocol for underwater robotics and sensor networks. It handles packet creation, transmission, reception, and parsing for reliable communication between underwater devices using acoustic modems via the NMv3 API.

**Note**: The header file (`floc.hpp`) contains complete packet structure definitions for all FLOC packet types including Data, Command, Acknowledgment, and Response packets. The implementation includes intelligent buffering, automatic retransmission, and acknowledgment tracking for reliable underwater communication.

## Features

- **Packet Management**: Creates, sends, and parses FLOC protocol packets
- **Reliable Transmission**: Automatic acknowledgment and retransmission handling
- **Network Addressing**: Hierarchical network and device ID management
- **Buffer Management**: Intelligent packet queuing with separate buffers for different packet types
- **Status Monitoring**: Device status queries and voltage reporting
- **Command Processing**: Structured command execution with acknowledgments
- **Serial Integration**: Works with NMv3 API for acoustic modem communication

## Ignored Protocol Features

Data fields defined in the protocol spec but not fully utilized in basic operations:
- Extended error reporting beyond basic packet validation
- Optional ACK data payloads (conditionally compiled)

## Unimplemented Features

The following features are defined in the header but require application-specific implementation:
- Custom command type handlers beyond `COMMAND_TYPE_1` and `COMMAND_TYPE_2`
- Complex ping management beyond basic device availability
- Application-specific data packet processing

## Usage

### Initialization

The library requires proper network and device configuration before use:

```c
#include "floc.hpp"

void setup() {
    // Configure network identity
    set_network_id(0x1234);
    set_device_id(0x0001);
    
    // Initialize device action structure
    init_da();
}
```

### Basic Functions

#### Status Query - Send & Parse
```c
void floc_status_query(uint8_t dest_addr);
```
Sends a status query to the specified device and sets up response handling for voltage and address information.

#### Send Acknowledgment - Send Only
```c
void floc_acknowledgement_send(uint8_t ttl, uint8_t ack_pid, uint16_t dest_addr);
```
Sends an acknowledgment packet for a received packet with the specified packet ID.

#### Send Status Response - Send Only
```c
void floc_status_send(uint8_t node_addr, float supply_voltage);
```
Sends device status information including address and supply voltage to the requesting device.

#### Send Error Response - Send Only
```c
void floc_error_send(uint8_t ttl, uint8_t err_pid, uint8_t err_dst_addr);
```
Sends an error response packet indicating a problem with the specified packet ID.

#### Packet Parser - Parse Only
```c
void floc_broadcast_received(uint8_t* broadcastBuffer, uint8_t size);
```
Pure parsing function that processes incoming FLOC packets and populates the global `DeviceAction_t` structure with parsed data. Handles all packet types and automatic acknowledgment generation.

### Device Action System

The library uses a global structure to communicate parsed packet information:

```c
extern DeviceAction_t da;

// Check parsed packet data
if (da.flocType == FLOC_COMMAND_TYPE) {
    handle_command(da.commandType, da.data, da.dataSize);
}

// Process any required actions
act_upon();
```

## Protocol Details

### Packet Types

FLOC defines four packet types for different communication needs:

1. **Data Packets** (`FLOC_DATA_TYPE`): Raw data transmission between devices
2. **Command Packets** (`FLOC_COMMAND_TYPE`): Device control and configuration commands
3. **Acknowledgment Packets** (`FLOC_ACK_TYPE`): Transmission confirmation and delivery receipt
4. **Response Packets** (`FLOC_RESPONSE_TYPE`): Command responses and status information

### Packet Structure

All packets follow a standard format:
- **Header** (8 bytes): Type, TTL, Network ID, Packet ID, addresses
- **Payload Header** (1-2 bytes): Packet-specific header with size information
- **Data** (variable length): Actual payload data

### Buffer Management

The library includes sophisticated buffering through `FLOCBufferManager`:

```c
// Add packet to transmission queue
flocBuffer.addPacket(packet, retransmit_flag);

// Process all queues (commands, responses, retransmissions)
flocBuffer.queuehandler();

// Track acknowledgments
flocBuffer.add_ackID(ack_packet_id);
```

### Maximum Sizes

- Maximum packet size: 64 bytes
- Maximum data payload varies by packet type (calculated automatically)
- Packet ID range: 0-63 (6-bit field)

### Network Structure

```
Network ID (16-bit) → Device ID (16-bit) → Packet ID (6-bit)
```

- **Network ID**: Identifies the communication network/swarm that buoy belongs to
- **Device ID**: Unique identifier for each device in the network  
- **Packet ID**: Sequential identifier for packet tracking and acknowledgment

## Integration with NMv3 API

FLOC is designed to work with the NMv3 API for acoustic modem communication:

```c
#include <nmv3_api.hpp>

// FLOC integrates with NMv3 functions:
// - query_status() for modem status
// - broadcast() for packet transmission
// - Serial communication handling
```

### Debugging

Enable debugging output with the `DEBUG_ON` flag for:
- Packet header information
- Data payload hex dumps  
- Transmission status
- Error conditions

## Notes

- All multi-byte fields use network byte order (big-endian)
- Packet IDs automatically increment and wrap at 64
- TTL decrements during packet forwarding (routing logic application-specific)
- Buffer management handles automatic retransmission up to 5 attempts
- Serial protocol supports bidirectional communication with Nest/Burd prefixes
- Error handling focuses on packet validation and acknowledgment tracking

## Dependencies

- **Nmv3**: Naval Modem Version 3 API for acoustic communication
- **Standard C++**: `stdint.h`, `map`, `queue` for data structures
- **Arduino** (optional): For embedded Arduino platforms
