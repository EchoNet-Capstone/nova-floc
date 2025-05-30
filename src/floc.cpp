#include <stdint.h>

#include <Arduino.h>

#ifdef min // min
#undef min
#endif //min

#ifdef max //min
#undef max
#endif //min

#include <nmv3_api.hpp>

#include "floc.hpp"
#include "floc_buffer.hpp"
#include "floc_utils.hpp"
#include "bloomfilter.hpp"

uint8_t packet_id = 0;

uint16_t status_response_dest_addr = -1; // Address that has requested modem status info
uint8_t status_request_pid = -1;

uint16_t network_id = 0;
uint16_t device_id = 0;

uint16_t
get_network_id(
    void
){
    return network_id;
}

void
set_network_id(
    uint16_t new_network_id
){
    network_id = new_network_id;
}

uint16_t
get_device_id(
    void
){
    return device_id;
}

void
set_device_id(
    uint16_t new_device_id
){
    device_id = new_device_id;
}

void
init_da(
    void
){
    da.modemRespType = -1;
    da.flocType = -1;
    da.commandType = -1;
    da.srcAddr = -1;
    da.dataSize = -1;
    da.data = NULL;
}

uint8_t
use_packet_id(
    void
){
    return packet_id++;
}

void
floc_status_query(
    uint8_t dest_addr
){
    status_response_dest_addr = dest_addr;
    query_status();
}

void
floc_build_header(
    FlocPacket_t* packet,
    uint8_t ttl,
    FlocPacketType_e type,
    uint16_t dest_addr,
    bool err_packet
){
    memset(packet, 0, sizeof(packet));

    packet->header.ttl = ttl;

    packet->header.type = type;

    packet->header.nid = htons(get_network_id());

    packet->header.pid = use_packet_id();
    packet->header.res = (uint8_t) err_packet;

    packet->header.dest_addr = htons(dest_addr);
    packet->header.src_addr = htons(get_device_id());
    packet->header.last_hop_addr = htons(get_device_id());
}

void
floc_acknowledgement_send(
    uint8_t ttl,
    uint8_t ack_pid,
    uint16_t dest_addr
){
    // Construct the packet
    FlocPacket_t packet;

    floc_build_header(&packet, ttl, FLOC_ACK_TYPE, dest_addr, false);

    packet.payload.ack.header.ack_pid = ack_pid;

    flocBuffer.handlePacket(packet);
    // broadcast(MODEM_SERIAL_CONNECTION, (char*)&packet, ACK_PACKET_ACTUAL_SIZE(&packet));
}

void
floc_status_send(
    uint8_t node_addr,
    float supply_voltage
){
    // Construct the packet
    FlocPacket_t packet;

    floc_build_header(&packet, TTL_START, FLOC_RESPONSE_TYPE, status_response_dest_addr, false);

    packet.payload.response.header.request_pid = packet.header.pid;
    packet.payload.response.header.size = sizeof(node_addr) + sizeof(supply_voltage);

    // Copy the status string into the response data
    memcpy(packet.payload.response.payload, &node_addr, sizeof(node_addr));
    memcpy(packet.payload.response.payload + sizeof(node_addr), &supply_voltage, sizeof(supply_voltage));

    flocBuffer.handlePacket(packet);

    // broadcast(MODEM_SERIAL_CONNECTION, (char*)(&packet), RESPONSE_PACKET_ACTUAL_SIZE(&packet));
}

void
floc_error_send(
    uint8_t ttl,
    uint8_t err_pid,
    uint8_t err_dst_addr
){
    FlocPacket_t packet;

    floc_build_header(&packet, TTL_START, FLOC_RESPONSE_TYPE, err_dst_addr, true);

    packet.payload.response.header.request_pid = err_pid;
    packet.payload.response.header.size = 0;

    flocBuffer.handlePacket(packet);
    //broadcast(MODEM_SERIAL_CONNECTION, (char*)(&packet), RESPONSE_PACKET_ACTUAL_SIZE(&packet));
}

void
parse_floc_data_packet(
    FlocHeader_t* floc_header,
    DataPacket_t* pkt, 
    uint8_t size
){
#ifdef DEBUG_ON // DEBUG_ON
    Serial.printf("Data packet received...\r\n");
#endif // DEBUG_ON

    if (size < sizeof(DataHeader_t)) {
    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("Invalid Data Packet: Too small\r\n");
    #endif // DEBUG_ON

        return;
    }

    DataHeader_t* header = &pkt->header;

    // Extract data size
    uint8_t dataSize = header->size;

    if (size < sizeof(DataHeader_t) + dataSize){
    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("Invalid Data Packet: Incomplete data\r\n");
    #endif // DEBUG_ON

        return;
    }

    // Extract data
    uint8_t* data = pkt->payload;

    // Setup DeviceAction
    da.flocType = FLOC_DATA_TYPE;
    da.srcAddr = floc_header->src_addr;
    da.dataSize = dataSize;
    da.data = data;
}

void
parse_floc_command_packet(
    FlocHeader_t* floc_header,
    CommandPacket_t* pkt,
    uint8_t size
){
#ifdef DEBUG_ON // DEBUG_ON
    Serial.printf("Command packet received...\r\n");
#endif // DEBUG_ON

    if (size < sizeof(CommandHeader_t)) {
    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("Invalid Command Packet: Too small\r\n");
    #endif // DEBUG_ON

        return;
    }

    // Extract the command header
    CommandHeader_t *header = &pkt->header;

    // Extract command type and size
    uint8_t commandType = header->command_type;
    uint8_t dataSize = header->size;

#ifdef DEBUG_ON // DEBUG_ON
    Serial.printf("\tCommandPacket\r\n");
    Serial.printf("\t\tType: %d\r\n", commandType);
    Serial.printf("\t\tSize: %d\r\n", dataSize);
#endif // DEBUG_ON

    // Validate data size
    if (size < sizeof(CommandHeader_t) + dataSize) {
    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("Invalid Command Packet: Incomplete data\r\n");
    #endif // DEBUG_ON

        return;
    }

    // Extract command data
    uint8_t* data = pkt->payload;

    bool valid_cmd = true;
    // Handle the command based on the type
    switch (commandType) {
        case COMMAND_TYPE_1:
            da.commandType = commandType;

            floc_acknowledgement_send(TTL_START, floc_header->pid, ntohs(floc_header->src_addr));
            break;
        case COMMAND_TYPE_2:
            da.commandType = commandType;

            floc_acknowledgement_send(TTL_START, floc_header->pid, ntohs(floc_header->src_addr));
            break;
        //...

        default:
        #ifdef DEBUG_ON // DEBUG_ON
            Serial.printf("Unknown FLOC Command Type! Type: [%01u]\r\n", commandType);
        #endif // DEBUG_ON

            valid_cmd = false;

            break;
    }

    if (valid_cmd) {
        // Setup DeviceAction
        da.flocType = FLOC_COMMAND_TYPE;
        da.data = data;
        da.dataSize = dataSize;
    }
}

void
parse_floc_acknowledgement_packet(
    FlocHeader_t* floc_header,
    AckPacket_t* pkt,
    uint8_t size
){
    if (size < sizeof(AckHeader_t)) {
    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("Invalid ACK Packet: Too small\r\n");
    #endif // DEBUG_ON

        return;
    }

    // Extract ACK header
    AckHeader_t *ackHeader = (AckHeader_t*)&pkt->header;

    uint8_t ack_pid = ackHeader->ack_pid;

#ifdef ACK_DATA // ACK_DATA
    uint8_t dataSize = ackHeader->size;

    if (size < sizeof(AckHeader_t) + dataSize) {
    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("Invalid Ack Packet: Incomplete data\r\n");
    #endif // DEBUG_ON

    }

    uint8_t* data = pkt->payload;
#endif // ACK_DATA

    // Setup DeviceAction
    da.flocType = FLOC_ACK_TYPE;

#ifdef ACK_DATA // ACK_DATA
    da.dataSize = dataSize;
    da.data = data;
#endif //ACK_DATA

#ifdef DEBUG_ON // DEBUG_ON
    Serial.printf("ACK Packet Received:\r\n");
    Serial.printf("\tAcknowledged Packet ID: %d\r\n", ack_pid);
    #ifdef ACK_DATA // ACK_DATA
        printBufferContents(data, dataSize);
    #endif
#endif // DEBUG_ON
}

void
parse_floc_response_packet(
    FlocHeader_t* floc_header,
    ResponsePacket_t* pkt,
    uint8_t size
){
    if (size < sizeof(ResponseHeader_t)) {
    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("Invalid Response Packet: Too small\r\n");
    #endif // DEBUG_ON

        return;
    }

    // Extract Response header
    ResponseHeader_t* responseHeader = (ResponseHeader_t*)&pkt->header;

    uint8_t request_pid = responseHeader->request_pid;
    uint8_t dataSize = responseHeader->size;

    if (size < sizeof(ResponseHeader_t) + dataSize) {
    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("Invalid Response Packet: Incomplete data\r\n");
    #endif // DEBUG_ON

        return;
    }

    // Extract response data
    uint8_t* responseData = pkt->payload;

    // Setup DeviceAction_t struct
    da.flocType = FLOC_RESPONSE_TYPE;
    da.data = responseData;
    da.dataSize = dataSize;

#ifdef DEBUG_ON // DEBUG_ON
    Serial.printf("Response Packet Received:\r\n");
    Serial.printf("  Request Packet ID: %d\r\n", request_pid);
    printBufferContents(responseData, dataSize);
#endif // DEBUG_ON
}

void
floc_broadcast_received(
    uint8_t* broadcastBuffer,
    uint8_t size
){
    if (size < sizeof(FlocHeader_t)) {
        // Packet is too small to contain a valid header
    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("Packet too small to contain valid header!\r\n");
        printBufferContents(broadcastBuffer, size);
    #endif // DEBUG_ON

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
    uint16_t last_hop_addr = ntohs(header->last_hop_addr);

#ifdef DEBUG_ON // DEBUG_ON
    Serial.printf("FLOC Packet Header\r\n");
    Serial.printf("\tTTL:%d\r\n", ttl);
    Serial.printf("\tType: %d\r\n", type);
    Serial.printf("\tNID: %d\r\n", nid);
    Serial.printf("\tPID: %d\r\n", pid);
    Serial.printf("\tDST: %d\r\n", dest_addr);
    Serial.printf("\tSRC: %d\r\n", src_addr);
    Serial.printf("\tLH: %d\r\n", last_hop_addr);
    printBufferContents((uint8_t*) pkt, size);
#endif // DEBUG_ON
    
    // Setup DeviceAction
    da.srcAddr = src_addr;
    da.lastHopAddr = last_hop_addr;

    if (bloom_check_packet(pid, dest_addr, src_addr)) {
    #ifdef DEBUG_ON
        Serial.printf("Duplicate packet (raw hash), dropping.\n");
    #endif
        return;
    }

    // adds timeout
    maybe_reset_bloom_filter();
    bloom_add_packet(pid, dest_addr, src_addr);

    if (nid != get_network_id()){
    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("Not on our network. %i Dropping...\r\n", nid);
    #endif // DEBUG_ON

        return;
    }

    if (src_addr == get_device_id()){
    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("Recv retrans from self. Dropping...\r\n");
    #endif // DEBUG_ON

        return;
    }

    // Determine the type of the packet
    switch (type) {
        case FLOC_DATA_TYPE:
        {
            DataPacket_t* data_pkt = (DataPacket_t*) &pkt->payload;
            parse_floc_data_packet(header, data_pkt, size - FLOC_HEADER_COMMON_SIZE);
            break;
        }
        case FLOC_COMMAND_TYPE:
        {
            CommandPacket_t* cmd_pkt = (CommandPacket_t*)&pkt->payload;
            parse_floc_command_packet(header, cmd_pkt, size - FLOC_HEADER_COMMON_SIZE);
            break;
        }
        case FLOC_ACK_TYPE:
        {
            AckPacket_t* ack_pkt = (AckPacket_t*)&pkt->payload;
            parse_floc_acknowledgement_packet(header, ack_pkt, size - FLOC_HEADER_COMMON_SIZE);
            break;
        }
        case FLOC_RESPONSE_TYPE:
        {
            ResponsePacket_t* resp_pkt = (ResponsePacket_t*)&pkt->payload;
            parse_floc_response_packet(header, resp_pkt, size - FLOC_HEADER_COMMON_SIZE);
            break;
        }
        default:
        #ifdef DEBUG_ON // DEBUG_ON
            Serial.printf("Unknown FLOC packet type! Type: [%03u]\r\n", type);
        #endif // DEBUG_ON

            break;
    }

    if (da.flocType >= FLOC_DATA_TYPE && da.flocType <= FLOC_RESPONSE_TYPE) // Is a valid packet
    {
        flocBuffer.handlePacket(*pkt);
    }
}

void
floc_unicast_received(
    uint8_t* unicastBuffer,
    uint8_t size
){
    // May not be used
}
