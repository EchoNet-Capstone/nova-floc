#pragma once

#include <stdint.h>
#include <map>
#include <queue>

#include "floc.hpp"

struct ping_device {
    uint16_t devAdd;
    uint8_t pingCount;
};

class 
FLOCBufferManager {
    public:
        void
        addPacket(
            const FlocPacket_t& packet
        );

        int
        checkQueueStatus(
            void
        );

        void
        addAckID(
            uint8_t ackID
        );

        void
        addToPingList(
            uint8_t index,
            uint16_t devAdd
        );

        int
        queueHandler(
            void
        );

    private:

        void
        printRetransmissionBuffer(
            void
        );

        void
        printResponseBuffer(
            void
        );

        void
        printCommandBuffer(
            void
        );

        void
        printPingDevices(
            void
        );

        void
        printAckIDs(
            void
        );

        void
        printall(
            void
        );

        bool
        checkPingList(
            void
        );

        bool
        checkAckID(
            uint8_t ackID
        );

        bool
        pingHandler(
            void
        );

        void
        retransmissionHandler(
            void
        );

        void
        responseHandler(
            void
        );

        void
        commandHandler(
            void
        );
        
        const int maxTransmissions = 5;
        const int maxSendBuffer    = 5;

        ping_device pingDevice[3];

        std::deque<FlocPacket_t> commandBuffer;
        std::deque<FlocPacket_t> responseBuffer;

        // this is going to be different
        std::deque<FlocPacket_t> retransmissionBuffer;

        std::map<uint8_t, int> ackIDs;
        std::map<uint8_t, int> transmissionCounts;
        

};

extern FLOCBufferManager flocBuffer;
