#pragma once

#include <stdint.h>

uint32_t 
hash_packet_buffer(
    uint8_t pid, 
    uint16_t dest_addr,
    uint16_t src_addr
);

bool 
bloom_check_packet(
    uint8_t pid, 
    uint16_t dest_addr,
    uint16_t src_addr
);

void bloom_add_packet(
    uint8_t pid, 
    uint16_t dest_addr,
    uint16_t src_addr
);

void 
bloom_add(
    uint32_t key
);

void maybe_reset_bloom_filter(
    void
);

uint32_t
H(
    uint32_t a,
    uint32_t b
);