#pragma once

#include <stdint.h>
#include <map>
#include <queue>

#include "floc.hpp"

class 
FLOCBufferManager {
    public:
        void
        handlePacket(
            const FlocPacket_t& packet
        );

        bool
        checkQueueStatus(
            void
        );

        void 
        removePacketById(
            uint8_t ackId
        );

        void
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
        printall(
            void
        );

        void
        addPacketToBuffer(
            std::deque<FlocPacket_t>& queue_to_add,
            const FlocPacket_t& packet
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
        
        const uint8_t maxTransmissions = 5;
        const uint8_t maxSendBuffer    = 5;

        std::deque<FlocPacket_t> commandBuffer;
        std::deque<FlocPacket_t> responseBuffer;
        std::deque<FlocPacket_t> retransmissionBuffer;

        std::map<uint8_t, uint8_t> transmissionCounts;
};

extern FLOCBufferManager flocBuffer;
