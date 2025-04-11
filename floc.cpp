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

uint16_t get_device_id() {
    // TODO : implement device id functionality
    return 0;
}

void floc_status_query(uint8_t dest_addr) {
    status_response_dest_addr = dest_addr;
    query_status(MODEM_SERIAL_CONNECTION);
}

void floc_acknowledgement_send(uint8_t ttl, uint8_t ack_pid, uint16_t dest_addr) {
    // Construct the packet
    FlocPacket_t packet;
    packet.header.ttl = ttl;
    packet.header.type = FLOC_ACK_TYPE;
    packet.header.nid = htons(get_network_id());
    packet.header.pid = use_packet_id();
    packet.header.res = 0;
    packet.header.dest_addr = htons(dest_addr);
    packet.header.src_addr = htons(get_device_id());

    packet.payload.ack.header.ack_pid = ack_pid;

    broadcast(MODEM_SERIAL_CONNECTION, (char*)&packet, ACK_PACKET_ACTUAL_SIZE(&packet));
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
    packet.header.src_addr = htons(get_device_id());

    packet.payload.response.header.request_pid = packet.header.pid;
    packet.payload.response.header.size = QUERY_STATUS_RESP_MAX;

    // Copy the status string into the response data
    memcpy(packet.payload.response.data, statusResponse, QUERY_STATUS_RESP_MAX);

    broadcast(MODEM_SERIAL_CONNECTION, (char*)(&packet), sizeof(FlocHeader_t) + sizeof(ResponseHeader_t) + QUERY_STATUS_RESP_MAX);
}

void parse_floc_data_packet(FlocHeader_t* floc_header, DataPacket_t* pkt, uint8_t size, DeviceAction_t* da) {
    if (size < sizeof(FlocHeader_t) + sizeof(DataHeader_t)) {
        if (debug) printf("Invalid Command Packet: Too small\n");
        return;
    }

    DataHeader_t* header = &pkt->header;

    // Extract data size
    uint8_t dataSize = header->size;

    if (size < sizeof(FlocHeader_t) + sizeof(DataHeader_t) + dataSize){
        if (debug) printf("Invalid Data Packet: Incomplete data\n");
        return;
    }

    // Extract data
    uint8_t* data = pkt->data;

    // Setup DeviceAction
    da->flocType = FLOC_DATA_TYPE;
    da->srcAddr = floc_header->src_addr;
    da->dataSize = dataSize;
    da->data = data;
}

void parse_floc_command_packet(FlocHeader_t* floc_header, CommandPacket_t* pkt, uint8_t size, DeviceAction_t* da) {
    if (debug) Serial.println("Command packet received...");

    if (size < sizeof(FlocHeader_t) + sizeof(CommandHeader_t)) {
        if (debug) printf("Invalid Command Packet: Too small\n");
        return;
    }

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

    // Setup DeviceAction
    da->flocType = FLOC_COMMAND_TYPE;
    da->data = data;
    da->dataSize = dataSize;

    // Handle the command based on the type
    switch (commandType) {
        case COMMAND_TYPE_1:
            da->commandType = commandType;

            floc_acknowledgement_send(TTL_START, floc_header->pid, ntohs(floc_header->src_addr));
            break;
        case COMMAND_TYPE_2:
            da->commandType = commandType;

            floc_acknowledgement_send(TTL_START, floc_header->pid, ntohs(floc_header->src_addr));
            break;
        //...

        default:
            printf("Unknown Command Type: %d\n", commandType);
            break;
    }
}

void parse_floc_acknowledgement_packet(FlocHeader_t* floc_header, AckPacket_t* pkt, uint8_t size, DeviceAction_t* da) {
    if (size < sizeof(FlocHeader_t) + sizeof(AckHeader_t)) {
        if (debug) printf("Invalid ACK Packet: Too small\n");
        return;
    }

    // Extract ACK header
    AckHeader_t *ackHeader = (AckHeader_t*)&pkt->header;

    uint8_t ack_pid = ackHeader->ack_pid;

    // Setup DeviceAction
    da->flocType = FLOC_ACK_TYPE;

    if (debug) {
        printf("ACK Packet Received:\r\n");
        printf("\tAcknowledged Packet ID: %d\r\n", ack_pid);
    }
}

void parse_floc_response_packet(FlocHeader_t* floc_header, ResponsePacket_t* pkt, uint8_t size, DeviceAction_t* da) {
    if (size < sizeof(FlocHeader_t) + sizeof(ResponseHeader_t)) {
        if (debug) printf("Invalid Response Packet: Too small\r\n");
        return;
    }

    // Extract Response header
    ResponseHeader_t* responseHeader = (ResponseHeader_t*)&pkt->header;

    uint8_t request_pid = responseHeader->request_pid;
    uint8_t dataSize = responseHeader->size;

    if (size < sizeof(FlocHeader_t) + sizeof(ResponseHeader_t) + dataSize) {
        if (debug) printf("Invalid Response Packet: Incomplete data\r\n");
        return;
    }

    // Extract response data
    uint8_t* responseData = pkt->data;

    // Setup DeviceAction_t struct
    da->flocType = FLOC_RESPONSE_TYPE;
    da->data = responseData;
    da->dataSize = dataSize;

    if (debug) {
        printf("Response Packet Received:\r\n");
        printf("  Request Packet ID: %d\r\n", request_pid);
        printf("  Data Size: %d\r\n", dataSize);
    }
}


void floc_broadcast_received(uint8_t *broadcastBuffer, uint8_t size, DeviceAction_t* da) {
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

    // Setup DeviceAction
    da->srcAddr = src_addr;

    // Determine the type of the packet
    switch (type) {
        case FLOC_DATA_TYPE:
            // Handle data packet if needed

            break;
        case FLOC_COMMAND_TYPE: {
            CommandPacket_t* cmd_pkt = (CommandPacket_t*)&pkt->payload;
            parse_floc_command_packet(header, cmd_pkt, size, da);
            break;
        }
        case FLOC_ACK_TYPE: {
            AckPacket_t* ack_pkt = (AckPacket_t*)&pkt->payload;
            parse_floc_acknowledgement_packet(header, ack_pkt, size, da);
            break;
        }
        case FLOC_RESPONSE_TYPE: {
            ResponsePacket_t* resp_pkt = (ResponsePacket_t*)&pkt->payload;
            parse_floc_response_packet(header, resp_pkt, size, da);
            break;

        }
        default:
            // Unknown packet type
            break;
    }
}

void floc_unicast_received(uint8_t* unicastBuffer, uint8_t size, DeviceAction_t* da) {
    // May not be used
}

// BEGIN NeST SERIAL CONNECTION FUNCTIONS -----------------

void packet_received_nest(uint8_t* packetBuffer, uint8_t size, DeviceAction_t* da) {
    
    if (size < 3) {
        // Need a prefix character, a casting type, and at least one byte of data e.g. $BX for a broadcast with data 'X'
        if (debug) Serial.println("NeST packet too small. Minimum size : 3.");
        return;
    }

    uint8_t pkt_type = *(packetBuffer++); // Remove '$' prefix

    SerialFlocPacket_t* pkt = (SerialFlocPacket_t*)(packetBuffer);

    if (pkt_type == '$') {
        switch (pkt->header.type) {
            // Broadcast the data received on the serial line
            case SERIAL_BROADCAST_TYPE: // 'B'
            {
                SerialBroadcastPacket_t* broadcastPacket = (SerialBroadcastPacket_t* )&pkt->payload;
                broadcast(MODEM_SERIAL_CONNECTION, (char*) broadcastPacket, pkt->header.size);
                // display_modem_packet_data(packetBuffer);
                break;
            }
            case SERIAL_UNICAST_TYPE:   // 'U'
                // TODO : need to extract dst from packet in order to send packet
                // May not need to implement, depending on networking strategy
                break;
            default:
                if (debug) {
                    Serial.printf("Unhandled serial broadcast packet type [NeST] : prefix [%c]\r\n", (char) pkt->header.type);
                    Serial.printf("Full Packet: ");
                    packetBuffer--;
                    for(int i = 0; i < size; i++){
                        Serial.printf("%02X", packetBuffer[i]);
                    }
                }
                return;
        }
    } else {
        Serial.printf("Unhandled packet type [NeST] : prefix [%c]\r\n", pkt_type);
        Serial.printf("Full Packet: ");
        packetBuffer--;
        for(int i = 0; i < size; i++){
            Serial.printf("%02X", packetBuffer[i]);
        }
    }
}
