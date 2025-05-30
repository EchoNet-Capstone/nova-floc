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
#include <algorithm>
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
FLOCBufferManager::printall(
    void
){
    Serial.printf("=== FLOC Buffers ===\r\n");
    printRetransmissionBuffer();
    printResponseBuffer();
    printCommandBuffer();
    Serial.printf("==================\r\n");
}

void
FLOCBufferManager::addPacketToBuffer(
    std::deque<FlocPacket_t>& buffer,
    const FlocPacket_t& packet
){
    if (buffer.size() > maxSendBuffer){

    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("Buffer full! Not adding...\r\n");
    #endif // DEBUG_ON

        /* Do nothing */
        return;
    }

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

    FlocPacket_t newPacket;
    memset(&newPacket, 0, sizeof(newPacket));

    memcpy(&(newPacket.header), &(packet.header), sizeof(FlocHeader_t));
    memcpy(&(newPacket.payload), &(packet.payload), payload_max_size);

#ifdef DEBUG_ON // DEBUG_ON
    printBufferContents((uint8_t*) &newPacket, sizeof(newPacket));
#endif // DEBUG_ON

    buffer.push_back(newPacket);
}

void
FLOCBufferManager::handlePacket(
    const FlocPacket_t& packet
){
    // identify if the packet is a retransmission
    if (ntohs(packet.header.dest_addr) != get_device_id()) {

        addPacketToBuffer(retransmissionBuffer, packet);

    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("Added to the retransmission buffer\r\n");
    #endif // DEBUG_ON

        return;
    } 

    if (ntohs(packet.header.src_addr) != get_device_id() 
        || ntohs(packet.header.dest_addr) == get_device_id()) {
    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("Not a packet we're sending or retransmitting. "); 
        Serial.printf("Handled in device_actions...\r\n");
    #endif // DEBUG_ON
        
        /* Do nothing */
        return;
    }

    switch(packet.header.type) {
        case FLOC_COMMAND_TYPE:

            addPacketToBuffer(commandBuffer, packet);

        #ifdef DEBUG_ON // DEBUG_ON
            Serial.printf("Added to the command buffer\r\n");
        #endif // DEBUG_ON
            break;

        case FLOC_RESPONSE_TYPE:
        case FLOC_DATA_TYPE:
        case FLOC_ACK_TYPE:

            addPacketToBuffer(responseBuffer, packet);

        #ifdef DEBUG_ON // DEBUG_ON
            Serial.printf("Added to the response buffer\r\n");
        #endif // DEBUG_ON

            break;
        default:
        #ifdef DEBUG_ON // DEBUG_ON
            Serial.printf("Invalid packet type for packet buffer\r\n");
        #endif // DEBUG_ON

            break;
    }
}

// check if buffer is empty
bool
FLOCBufferManager::checkQueueStatus(
    void
){
    if(!retransmissionBuffer.empty()) {
    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("Retransmission buffer is not empty\r\n");
    #endif // DEBUG_ON

        return true;
    } else if (!responseBuffer.empty()) {
    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("Response buffer is not empty\r\n");
    #endif // DEBUG_ON

        return true;
    } else if (!commandBuffer.empty()) {
    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("Command buffer is not empty\r\n");
    #endif // DEBUG_ON

        return true;
    }

    return false;
}

void 
FLOCBufferManager::removePacketById(
    uint8_t ackId
){
    auto it = std::find_if(commandBuffer.begin(), commandBuffer.end(),
        [ackId](const FlocPacket_t& packet) {
            return packet.header.pid == ackId;
        });
    
    if (it != commandBuffer.end()) {
        commandBuffer.erase(it);
    }
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

        packet.header.last_hop_addr = ntohs(get_device_id());

        #ifdef DEBUG_ON // DEBUG_ON
            printBufferContents((uint8_t*) &packet, packet_size);
        #endif // DEBUG_ON

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
