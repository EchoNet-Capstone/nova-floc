/* This is going to be the buffer protocol file.
 *
 * all use FIFO
 *
 * Retransmission buffer
 * - priority 1
 * Response buffer
 * - priority 2
 * Command buffer
 *  - priority 3
 *  - 5 max transmissions
 *  - if ack rm from buffer
 *  - if no ack after 5 transmissions, rm from buffer
 */

#include <Arduino.h>

#ifdef min // min
#undef min
#endif //min

#ifdef max //min
#undef max
#endif //min

#include <stdint.h>
#include <string.h>
#include <queue>
#include <map>

#include <nmv3_api.hpp>

#include "floc_buffer.hpp"
#include "floc_utils.hpp"

FLOCBufferManager flocBuffer;

// Debug help
void 
FLOCBufferManager::printRetransmissionBuffer(
    void
){
    Serial.printf("Retrans Buffer (%d):\r\n", retransmissionBuffer.size());
    if (retransmissionBuffer.empty()) {
        Serial.printf("  (empty)\r\n");
        return;
    }
    
    int count = 0;
    for (auto it = retransmissionBuffer.begin(); 
         it != retransmissionBuffer.end() && count < 5; 
         ++it, ++count) {
        Serial.printf("  [%d] PID:%d TTL:%d\r\n", count, it->header.pid, it->header.ttl);
        Serial.printf("      Src:%d Dst:%d\r\n", ntohs(it->header.src_addr), ntohs(it->header.dest_addr));
    }
    if (retransmissionBuffer.size() > 5) {
        Serial.printf("  ...+%d more\r\n", retransmissionBuffer.size() - 5);
    }
}

void 
FLOCBufferManager::printResponseBuffer(
    void
){
    Serial.printf("Response Buffer (%d):\r\n", responseBuffer.size());
    if (responseBuffer.empty()) {
        Serial.printf("  (empty)\r\n");
        return;
    }
    
    int count = 0;
    for (auto it = responseBuffer.begin(); 
         it != responseBuffer.end() && count < 5; 
         ++it, ++count) {
        Serial.printf("  [%d] PID:%d Type:%d\r\n", count, it->header.pid, it->header.type);
        Serial.printf("      Src:%d Dst:%d\r\n", ntohs(it->header.src_addr), ntohs(it->header.dest_addr));
    }
    if (responseBuffer.size() > 5) {
        Serial.printf("  ...+%d more\r\n", responseBuffer.size() - 5);
    }
}

void 
FLOCBufferManager::printCommandBuffer(
    void
){
    Serial.printf("Command Buffer (%d):\r\n", commandBuffer.size());
    if (commandBuffer.empty()) {
        Serial.printf("  (empty)\r\n");
        return;
    }
    
    int count = 0;
    for (auto it = commandBuffer.begin(); 
         it != commandBuffer.end() && count < 5; 
         ++it, ++count) {
        Serial.printf("  [%d] PID:%d Type:%d\r\n", count, it->header.pid, it->header.type);
        Serial.printf("      Src:%d Dst:%d\r\n", ntohs(it->header.src_addr), ntohs(it->header.dest_addr));
        
        // Check transmission count
        auto tx_it = transmissionCounts.find(it->header.pid);
        if (tx_it != transmissionCounts.end()) {
            Serial.printf("      TX:%d\r\n", tx_it->second);
        }
    }
    if (commandBuffer.size() > 5) {
        Serial.printf("  ...+%d more\r\n", commandBuffer.size() - 5);
    }
}

void 
FLOCBufferManager::printPingDevices(
    void
){
    Serial.printf("Ping Devices:\r\n");
    bool found = false;
    for (int i = 0; i < 3; i++) {
        if (pingDevice[i].devAdd != 0) {
            Serial.printf("  [%d] Dev:%d Mod:%d\r\n", 
                i, pingDevice[i].devAdd, modemIdFromDidNid(pingDevice[i].devAdd, get_network_id()));
            Serial.printf("      Count:%d\r\n", pingDevice[i].pingCount);
            found = true;
        }
    }
    if (!found) {
        Serial.printf("  (none)\r\n");
    }
}

void 
FLOCBufferManager::printAckIDs(
    void
){
    Serial.printf("ACK IDs (%d):\r\n", ackIDs.size());
    if (ackIDs.empty()) {
        Serial.printf("  (none)\r\n");
        return;
    }
    
    int count = 0;
    for (const auto& pair : ackIDs) {
        if (count < 5) {
            Serial.printf("  ID:%d\r\n", pair.first);
        }
        count++;
    }
    if (ackIDs.size() > 5) {
        Serial.printf("  ...+%d more\r\n", ackIDs.size() - 5);
    }
}

void 
FLOCBufferManager::printall(
    void
){
    Serial.printf("=== FLOC Buffers ===\r\n");
    printRetransmissionBuffer();
    printResponseBuffer();
    printCommandBuffer();
    printPingDevices();
    printAckIDs();
    Serial.printf("==================\r\n");
}

void
FLOCBufferManager::addPacket(
    const FlocPacket_t& packet
){
    FlocPacket_t newPacket;
    memset(&newPacket, 0, sizeof(newPacket));

    memcpy(&(newPacket.header), &(packet.header), sizeof(FlocHeader_t));

    size_t payload_max_size;
    switch(packet.header.type){
        case FLOC_DATA_TYPE:
            payload_max_size = sizeof(DataPacket_t);
            break;
        case FLOC_COMMAND_TYPE:
            payload_max_size = sizeof(CommandPacket_t);
            break;
        case FLOC_ACK_TYPE:
            payload_max_size = sizeof(AckPacket_t);
            break;
        case FLOC_RESPONSE_TYPE:
            payload_max_size = sizeof(ResponsePacket_t);
            break;
    }

    memcpy(&(newPacket.payload), &(packet.payload), payload_max_size);

    // identify if the packet is a retransmission
    if (packet.header.dest_addr != get_device_id()) {
        if (retransmissionBuffer.size() > maxSendBuffer){

        #ifdef DEBUG_ON // DEBUG_ON
            Serial.printf("Retransmission buffer to full! \r\n");
        #endif // DEBUG_ON

            return;
        }

    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("Adding packet to retransmission buffer\r\n");
        printBufferContents((uint8_t*) &newPacket, sizeof(newPacket));
    #endif // DEBUG_ON
    
        retransmissionBuffer.push_back(newPacket);

    } else if(newPacket.header.type == FLOC_COMMAND_TYPE) {
        commandBuffer.push_back(newPacket);

    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("Added to the command buffer\r\n");
    #endif // DEBUG_ON

    } else if (newPacket.header.type == FLOC_RESPONSE_TYPE || newPacket.header.type == FLOC_ACK_TYPE) {
        responseBuffer.push_back(newPacket);

    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("Added to the response buffer\r\n");
    #endif // DEBUG_ON

    } else {
    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("Invalid packet type for packet buffer\r\n");
    #endif // DEBUG_ON

    }
}

// check if buffer is empty
int
FLOCBufferManager::checkQueueStatus(
    void
){
    if(!retransmissionBuffer.empty()) {
    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("Retransmission buffer is not empty\r\n");
    #endif // DEBUG_ON

        return 1;
    } else if (!responseBuffer.empty()) {
    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("Response buffer is not empty\r\n");
    #endif // DEBUG_ON

        return 2;
    } else if (!commandBuffer.empty()) {
    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("Command buffer is not empty\r\n");
    #endif // DEBUG_ON

        return 3;
    } else {
        return 0;
    }
}

void
FLOCBufferManager::addToPingList(
    uint8_t index,
    uint16_t devAdd
){
    pingDevice[index].devAdd = devAdd;
    pingDevice[index].pingCount = 0;
}

bool
FLOCBufferManager::checkPingList(
    void
){
    if (pingDevice[0].devAdd != 0) {
        return true;
    }

    return false;
}

// list of ackIDs
void
FLOCBufferManager::addAckID(
    uint8_t ackID
){
    ackIDs[ackID] = 1;
#ifdef DEBUG_ON // DEBUG_ON
    Serial.printf("Ack ID %d added\r\n", ackID);
#endif // DEBUG_ON

}

bool
FLOCBufferManager::checkAckID(
    uint8_t ackID
){
    if (ackIDs.find(ackID) != ackIDs.end()) {
        ackIDs.erase(ackID);
    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("Ack ID %d found and removed\r\n", ackID);
    #endif // DEBUG_ON

        return true;
    }

    return false;
}

// this will send out all the pings
bool
FLOCBufferManager::pingHandler(
    void
){
    static int curr_device = 0;

    if (curr_device >= 3){
        curr_device = 0;

        return false;
    }

    ping_device& dev = pingDevice[curr_device]; // Reference the real item

    if (dev.pingCount < maxTransmissions) {
        dev.pingCount++;
        
        uint8_t modem_id = modemIdFromDidNid(get_device_id(), get_network_id());
        ping(modem_id);
    } else { // Maximum transmissions reached
        curr_device++;
    }

    return true;
}

// retransmit and remove from queue
void
FLOCBufferManager::retransmissionHandler(
    void
){
    FlocPacket_t packet = retransmissionBuffer.front();

    if (packet.header.ttl > 1){
        packet.header.ttl--;
        #ifdef DEBUG_ON // DEBUG_ON
            Serial.printf("[FLOCBUFF] TTL Decremented to %i\r\n", packet.header.ttl);
        #endif // DEBUG_ON

        uint8_t packet_size;
        switch(packet.header.type){
            case FLOC_DATA_TYPE:
                packet_size = DATA_PACKET_ACTUAL_SIZE(&packet);
                break;
            case FLOC_COMMAND_TYPE:
                packet_size = COMMAND_PACKET_ACTUAL_SIZE(&packet);
                break;
            case FLOC_ACK_TYPE:
                packet_size = ACK_PACKET_ACTUAL_SIZE(&packet);
                break;
            case FLOC_RESPONSE_TYPE:
                packet_size = RESPONSE_PACKET_ACTUAL_SIZE(&packet);
                break;
        }

        #ifdef DEBUG_ON // DEBUG_ON
            Serial.printf("[FLOCBUFF] Retransmitting %i\r\n", packet.header.pid);
        #endif // DEBUG_ON

        packet.header.last_hop_addr = get_device_id();

        broadcast((uint8_t*) &packet, packet_size);
    } 

    retransmissionBuffer.pop_front(); // Remove from buffer
}

void
FLOCBufferManager::responseHandler(
    void
){
    FlocPacket_t packet = responseBuffer.front();

    // send packet
    broadcast((uint8_t*) &packet, RESPONSE_PACKET_ACTUAL_SIZE(&packet));
    responseBuffer.pop_front(); // Remove from buffer
}

void
FLOCBufferManager::commandHandler(
    void
){
    // copy the packet from the front of the queue
    FlocPacket_t packet = commandBuffer.front();

    uint8_t packet_id = packet.header.pid;

    // Check if the packet ID exists in the map, if not initialize it
    if (transmissionCounts.find(packet_id) == transmissionCounts.end()) {
        transmissionCounts[packet_id] = 0; // Initialize count for this packet ID
    }

    // Check if the packet has been transmitted the maximum number of times
    if(transmissionCounts[packet_id] >= maxTransmissions) {
    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("Max transmissions reached for packet ID %d\r\n", packet_id);
    #endif // DEBUG_ON

        commandBuffer.pop_front(); // Remove from buffer
        transmissionCounts.erase(packet_id); // Remove from map

        floc_error_send(1, packet_id, packet.header.src_addr); // Send error packet
        return;
    }

    transmissionCounts[packet_id]++; // Increment transmission count for this packet ID

    // send packet
    broadcast((uint8_t*) &packet, COMMAND_PACKET_ACTUAL_SIZE(&packet));
}

// blocking check call
void
FLOCBufferManager::queueHandler(
    void
){
    if (checkPingList()) { // ranging period started
        if (pingHandler()) {
            return; // Continue with ranging period, don't send other packets
        } else {
            memset(&pingDevice, 0, sizeof(pingDevice)); // ranging period done, clear out ping device array
            // continue with other packets
        }
    }

    #ifdef DEBUG_ON // DEBUG_ON
        flocBuffer.printall();
    #endif // DEBUG_ON

    if(!retransmissionBuffer.empty()) {
        retransmissionHandler();

        return;
    } else if (!responseBuffer.empty()) {
        responseHandler();

        return;
    } else if (!commandBuffer.empty()) {
        commandHandler();

        return;
    } else {
        /* Do Nothing */
    }
}
