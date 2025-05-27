#pragma once

#include <stdint.h>
#include <map>
#include <queue>

#include "floc.hpp"

struct ping_device {
    uint16_t devAdd;
    uint8_t modAdd;
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
        checkqueueStatus(
            void
        );

        int
        queuehandler(
            void
        );

        void
        add_ackID(
            uint8_t ackID
        );

        void
        add_pinglist(
            uint8_t index,
            uint16_t devAdd,
            uint8_t modAdd
        );

    private:

        int
        ping_handler(
            void
        );

        int
        retransmission_handler(
            void
        );

        int
        response_handler(
            void
        );

        int
        command_handler(
            void
        );

        int
        checkackID(
            uint8_t ackID
        );

        bool
        check_pinglist(
            void
        );

        void
        printall(
            void
        );

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
        
        const int maxTransmissions = 5;
        const int maxSendBuffer    = 5;

        ping_device pingDevice[3];;

        std::queue<FlocPacket_t> commandBuffer;
        std::queue<FlocPacket_t> responseBuffer;

        // this is going to be different
        std::queue<FlocPacket_t> retransmissionBuffer;

        std::map<uint8_t, int> ackIDs;
        std::map<uint8_t, int> transmissionCounts;
        

};

extern FLOCBufferManager flocBuffer;
