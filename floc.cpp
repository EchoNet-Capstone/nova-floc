#include "floc.hpp"

uint8_t packet_id = 0;

uint16_t status_response_dest_addr = -1; // Address that has requested modem status info
uint8_t status_request_pid = -1; 

uint8_t use_packet_id() {
    return packet_id++;
}

uint16_t get_network_id() { 
    // TODO : implement network id functionality
    return 0;
}

void parse_floc_command_packet(FlocHeader_t* floc_header, CommandPacket_t* pkt, uint8_t size) {
    if (debug) Serial.println("Command packet received...");

    // Extract the command header
    CommandHeader_t *header = &pkt->header;

    // Extract command type and size
    uint8_t commandType = header->command_type;
    uint8_t dataSize = header->size;
    
    Serial.printf("\tCommandPacket\r\n\t\tType: %d\r\n\t\tSize: %d\r\n", commandType, dataSize);

    // Validate data size
    if (size < sizeof(FlocHeader_t) + sizeof(CommandHeader_t) + dataSize) {
        if (debug) printf("Invalid Command Packet: Incomplete data\n");
        return;
    }

    // Extract command data
    uint8_t* data = pkt->data;

    // Handle the command based on the type
    switch (commandType) {
        case COMMAND_TYPE_1:
            // TODO : Code to release buoy goes here
            motor_run_to_position(CLOSED_POSITION);

            floc_acknowledgement_send(TTL_START, floc_header->pid, ntohs(floc_header->src_addr), get_modem_address());

            break;
        case COMMAND_TYPE_2:
            // TODO : Process COMMAND_TYPE_2 here
            break;
        default:
            printf("Unknown Command Type: %d\n", commandType);
            break;
    }
}

void parse_floc_acknowledgement_packet(uint8_t *broadcastBuffer, uint8_t size) {
    if (sizeof(FlocHeader_t) + sizeof(AckHeader_t) > FLOC_MAX_SIZE) {
        if (debug) printf("Invalid ACK Packet: Too small\n");
        return;
    }

    // Extract the common header
    FlocHeader_t *header = reinterpret_cast<FlocHeader_t*>(broadcastBuffer);

    // Extract ACK header
    AckHeader_t *ackHeader = reinterpret_cast<AckHeader_t*>(broadcastBuffer + sizeof(FlocHeader_t));

    uint8_t ack_pid = ackHeader->ack_pid;

    if (debug) {
        printf("ACK Packet Received:\r\n");
        printf("\tAcknowledged Packet ID: %d\r\n", ack_pid);
    }

    // TODO : Handle ACK processing (e.g., mark packet as acknowledged)
}

void parse_floc_response_packet(uint8_t *broadcastBuffer, uint8_t size) {
    if (sizeof(FlocHeader_t) + sizeof(ResponseHeader_t) > FLOC_MAX_SIZE) {
        if (debug) printf("Invalid Response Packet: Too small\r\n");
        return;
    }

    // Extract the common header
    FlocHeader_t *header = reinterpret_cast<FlocHeader_t*>(broadcastBuffer);

    // Extract Response header
    ResponseHeader_t *responseHeader = reinterpret_cast<ResponseHeader_t*>(broadcastBuffer + sizeof(FlocHeader_t));

    uint8_t request_pid = responseHeader->request_pid;
    uint8_t dataSize = responseHeader->size;

    if (sizeof(FlocHeader_t) + sizeof(ResponseHeader_t) + dataSize > FLOC_MAX_SIZE) {
        if (debug) printf("Invalid Response Packet: Incomplete data\r\n");
        return;
    }

    // Extract response data
    uint8_t *responseData = reinterpret_cast<uint8_t*>(broadcastBuffer + sizeof(FlocHeader_t) + sizeof(ResponseHeader_t));

    if (debug) {
        printf("Response Packet Received:\r\n");
        printf("  Request Packet ID: %d\r\n", request_pid);
        printf("  Data Size: %d\r\n", dataSize);
    }
    
    // TODO : Handle response data processing
}


void floc_broadcast_received(uint8_t *broadcastBuffer, uint8_t size) {
    if (size < sizeof(FlocHeader_t)) {
        // Packet is too small to contain a valid header
        if (debug) Serial.println("Packet too small to contain valid header");
        return;
    }

    // Cast buffer to FLOC packet
    FlocPacket_t* pkt = (FlocPacket_t *) broadcastBuffer;
    // Extract the common header
    FlocHeader_t* header = &pkt->header;

    uint8_t ttl = header->ttl;
    uint8_t type = header->type;
    uint16_t nid = ntohs(header->nid);
    uint8_t pid = header->pid;
    uint16_t dest_addr = ntohs(header->dest_addr);
    uint16_t src_addr = ntohs(header->src_addr);

    if (debug) {
        Serial.printf(
            "FLOC Packet Header\r\n\tTTL:%d\r\n\tType: %d\r\n\tNID: %d\r\n\tPID: %d\r\n\tDST: %d\r\n\tSRC: %d\r\n",
            ttl,
            type,
            nid,
            pid,
            dest_addr,
            src_addr);
    }

    // Determine the type of the packet
    switch (type) {
        case FLOC_DATA_TYPE:
            // Handle data packet if needed
            break;
        case FLOC_COMMAND_TYPE: {
            if (size < sizeof(FlocHeader_t) + sizeof(CommandHeader_t)) {
                if (debug) printf("Invalid Command Packet: Too small\n");
                return;
            }
            CommandPacket_t* cmd_pkt = (CommandPacket_t*)malloc(sizeof(CommandPacket_t));
            memset(cmd_pkt,0, sizeof(CommandPacket_t));
            memcpy(cmd_pkt, &pkt->payload.command, pkt->payload.command.header.size + COMMAND_HEADER_SIZE);
            parse_floc_command_packet(header, cmd_pkt, size);
            free(cmd_pkt);
            break;
        }
        case FLOC_ACK_TYPE:
            parse_floc_acknowledgement_packet(broadcastBuffer, size);
            break;
        case FLOC_RESPONSE_TYPE:
            parse_floc_response_packet(broadcastBuffer, size);
            break;
        default:
            // Unknown packet type
            break;
    }
}

void floc_unicast_received(uint8_t* unicastBuffer, uint8_t size) {
    // May not be used
}

void floc_acknowledgement_send(uint8_t ttl, uint8_t ack_pid, uint16_t dest_addr, uint16_t src_addr) {
    // Construct the packet
    FlocPacket_t packet;
    packet.header.ttl = ttl;
    packet.header.type = FLOC_ACK_TYPE;
    packet.header.nid = htons(get_network_id());
    packet.header.pid = use_packet_id();
    packet.header.res = 0;
    packet.header.dest_addr = htons(dest_addr);
    packet.header.src_addr = htons(src_addr);

    packet.payload.ack.header.ack_pid = ack_pid;

    broadcast(MODEM_SERIAL_CONNECTION, reinterpret_cast<char*>(&packet), sizeof(FlocHeader_t) + sizeof(AckHeader_t));
}


void floc_status_queue(HardwareSerial connection, uint8_t dest_addr) {
    status_response_dest_addr = dest_addr;
    query_status(connection);
}

void floc_status_send(QueryStatusResponseFullPacket_t* statusResponse) {
    // Construct the packet
    FlocPacket_t packet;
    packet.header.ttl = TTL_START;
    packet.header.type = FLOC_RESPONSE_TYPE;
    packet.header.nid = htons(get_network_id());
    packet.header.pid = use_packet_id();
    packet.header.res = 0;
    packet.header.dest_addr = htons(status_response_dest_addr);
    packet.header.src_addr = htons(get_modem_address());

    packet.payload.response.header.request_pid = packet.header.pid;
    packet.payload.response.header.size = QUERY_STATUS_RESP_MAX;

    // Copy the status string into the response data
    memcpy(packet.payload.response.data, statusResponse, QUERY_STATUS_RESP_MAX);

    broadcast(MODEM_SERIAL_CONNECTION, (char*)(&packet), sizeof(FlocHeader_t) + sizeof(ResponseHeader_t) + QUERY_STATUS_RESP_MAX);
}
