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

#ifdef ARDUINO // ARDUINO
#include <Arduino.h>

#ifdef min // min
#undef min
#endif //min

#ifdef max //min
#undef max
#endif //min

#endif // ARDUINO

#include <stdint.h>
#include <string.h>
#include <queue>
#include <map>

#include <nmv3_api.hpp>

#include "floc_buffer.hpp"

FLOCBufferManager flocBuffer;

void
FLOCBufferManager::addPacket(
    const FlocPacket_t& packet,
    bool retrans
){
    // identify if the packet is a retransmission
    if (retrans) {
        retransmissionBuffer.push(packet);
    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("Packet added to retransmission buffer\n");
    #endif // DEBUG_ON

        return;
    }

    // if not a retransmission, check the type of the packet
    if(packet.header.type == FLOC_COMMAND_TYPE) {

        
        commandBuffer.push(packet);

    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("added to the command buffer\r\n");
    #endif // DEBUG_ON


    } else if (packet.header.type == FLOC_RESPONSE_TYPE) {
        responseBuffer.push(packet);

    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("Added to the response buffer\r\n");
    #endif // DEBUG_ON

    } else {
    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("Invalid packet type for command buffer\n");
    #endif // DEBUG_ON
    }
}

// blocking check call
int
FLOCBufferManager::queuehandler(
    void
){
    if (check_pinglist()) {
        ping_handler();
    }

    if(!retransmissionBuffer.empty()) {
    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("Retransmission buffer is not empty\n");
    #endif // DEBUG_ON

        retransmission_handler();
        return 1;
    } else if (!responseBuffer.empty()) {
    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("Response buffer is not empty\n");
    #endif // DEBUG_ON

        response_handler();
        return 2;
    } else if (!commandBuffer.empty()) {
    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("Command buffer is not empty\n");
    #endif // DEBUG_ON

        command_handler();
        return 3;
    } else {
        return 0;
    }
}

// check if buffer is empty
int
FLOCBufferManager::checkqueueStatus(
    void
){
    if(!retransmissionBuffer.empty()) {
    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("Retransmission buffer is not empty\n");
    #endif // DEBUG_ON

        return 1;
    } else if (!responseBuffer.empty()) {
    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("Response buffer is not empty\n");
    #endif // DEBUG_ON

        return 2;
    } else if (!commandBuffer.empty()) {
    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("Command buffer is not empty\n");
    #endif // DEBUG_ON

        return 3;
    } else {
        return 0;
    }
}

// this will send out all the pings
int
FLOCBufferManager::ping_handler(
    void
){
    for (int i = 0; i < 3; i++) {
        ping_device& dev = pingDevice[i]; // Reference the real item

        if (checkackID(dev.devAdd)) {
            memset(&dev, 0, sizeof(dev));
        #ifdef DEBUG_ON
            printf("Ping ID %d found and removed\n", dev.devAdd);
        #endif
        
            return 1; // No need to ping if ACK received
        }

        if (dev.pingCount < maxTransmissions) {
            dev.pingCount++;
            ping(dev.devAdd);
        }
    }

    return 0;
}

bool
FLOCBufferManager::check_pinglist(
    void
){
    if (pingDevice[0].devAdd != 0) {
        return 1;
    } else {
        return 0;
    }
}

// retransmit and remove from vector
int
FLOCBufferManager::retransmission_handler(
    void
){
    FlocPacket_t packet = retransmissionBuffer.front();

    uint16_t packet_id = packet.header.pid;

    // if message is an ack, remove from buffer
    if (checkackID(packet_id)) {
    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("Ack ID %d found and removed\n", packet_id);
    #endif // DEBUG_ON

        commandBuffer.pop(); // Remove from buffer
        return 1;
    }

    // send packet
    // FIX THE SIZE ASPECT
    broadcast((char*)&packet, DATA_PACKET_ACTUAL_SIZE(&packet));

    retransmissionBuffer.pop(); // Remove from buffer
    return 0;
}

int
FLOCBufferManager::response_handler(
    void
){
    FlocPacket_t packet = responseBuffer.front();

    uint16_t packet_id = packet.header.pid;

    // if message is an ack, remove from buffer
    if (checkackID(packet_id)) {
    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("Ack ID %d found and removed\n", packet_id);
    #endif // DEBUG_ON

        commandBuffer.pop(); // Remove from buffer
        return 1;
    }
    // send packet
    broadcast((char*)&packet, RESPONSE_PACKET_ACTUAL_SIZE(&packet));
    responseBuffer.pop(); // Remove from buffer
    return 0;
}

int
FLOCBufferManager::command_handler(
    void
){
    // copy the packet from the front of the queue
    FlocPacket_t packet = commandBuffer.front();

    uint8_t packet_id = packet.header.pid;

    // if message is an ack, remove from buffer
    if (checkackID(packet_id)) {
    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("Ack ID %d found and removed\n", packet_id);
    #endif // DEBUG_ON

        commandBuffer.pop(); // Remove from buffer
        return 1;
    }

    // Check if the packet ID exists in the map, if not initialize it
    if (transmissionCounts.find(packet_id) == transmissionCounts.end()) {
        transmissionCounts[packet_id] = 0; // Initialize count for this packet ID
    }

    // Check if the packet has been transmitted the maximum number of times
    if(transmissionCounts[packet_id] >= maxTransmissions) {
    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("Max transmissions reached for packet ID %d\n", packet_id);
    #endif // DEBUG_ON

        commandBuffer.pop(); // Remove from buffer
        transmissionCounts.erase(packet_id); // Remove from map

        floc_error_send(1, packet_id, packet.header.src_addr); // Send error packet
        return 0;
    }

    transmissionCounts[packet_id]++; // Increment transmission count for this packet ID

    broadcast((char*)&packet, COMMAND_PACKET_ACTUAL_SIZE(&packet));
    // send packet
    return 0;
}

// list of ackIDs
void
FLOCBufferManager::add_ackID(
    uint8_t ackID
){
    ackIDs[ackID] = 1;
#ifdef DEBUG_ON // DEBUG_ON
    Serial.printf("Ack ID %d added\n", ackID);
#endif // DEBUG_ON

}

int
FLOCBufferManager::checkackID(
    uint8_t ackID
){
    if (ackIDs.find(ackID) != ackIDs.end()) {
        ackIDs.erase(ackID);
    #ifdef DEBUG_ON // DEBUG_ON
        Serial.printf("Ack ID %d found and removed\n", ackID);
    #endif // DEBUG_ON

        return 1;
    } else {
        return 0;
    }
}

void
FLOCBufferManager::add_pinglist(
    uint8_t index,
    uint16_t devAdd,
    uint8_t modAdd
){
    pingDevice[index].devAdd = devAdd;
    pingDevice[index].modAdd = modAdd;
    pingDevice[index].pingCount = 0;
}



