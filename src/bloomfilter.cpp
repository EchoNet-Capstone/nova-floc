/* 
 * The purpose of this is to reduce overhead and allow the device to 
 * filter out messages it has already seen
 * Uses the size and packetbuffer to create a hash
 * 
 * There is around a 15% false positive rate
 * if you want to reduce this, increase the size of the bloom filter or add 3rd hash
 * this is at the risk of memory and CPU usage (which we are trying to avoid)
 * 
 * ~15-20 unique entries per bloom filter
 * 
 * Also if you want to increase accuracy you can reset the bloom filter
 * every 5-10 minutes?
 * */
#include "bloomfilter.hpp"
#include "globals.hpp"
#include "floc.hpp"

#define BLOOM_FILTER_BITS 64
#define BLOOM_FILTER_BYTES (BLOOM_FILTER_BITS / 8)

static uint8_t bloom_filter[BLOOM_FILTER_BYTES] = {0};

uint8_t 
hash1(
    uint32_t key
) {
    return (key * 31) % 64;
}

uint8_t 
hash2(
    uint32_t key
) {
    return ((key >> 3) ^ (key * 17)) % 64;
}

bool bloom_check(uint32_t key) {
    return (bloom_filter[hash1(key) / 8] & (1 << (hash1(key) % 8))) &&
           (bloom_filter[hash2(key) / 8] & (1 << (hash2(key) % 8)));
}


// uint32_t 
// hash_packet_buffer(
//     uint8_t* buf, 
//     uint8_t size
// ) {
//     uint32_t hash = 5381;
//     for (int i = 0; i < size; i++) {
//         hash = ((hash << 5) + hash) ^ buf[i]; // djb2 variant (fast and light)
//     }
//     return hash;
// }

uint32_t
H(
    uint32_t a,
    uint32_t b
) {
    return .5*(a + b) * (a + b + 1) + b;
}

// might want to add nid later
uint32_t 
hash_packet_buffer(
    uint8_t pid, 
    uint16_t dest_addr,
    uint16_t src_addr
) {
    uint32_t hash = 0;

    hash = .5*(H(pid, H(dest_addr, src_addr)));
    
    return hash;
}

bool 
bloom_check_packet(
    uint8_t pid, 
    uint16_t dest_addr,
    uint16_t src_addr
) {
    uint32_t key = hash_packet_buffer(pid, dest_addr, src_addr); // or make_key_from_buffer
    return bloom_check(key);
}

void bloom_add_packet(
    uint8_t pid, 
    uint16_t dest_addr,
    uint16_t src_addr
) {

    uint32_t key = hash_packet_buffer(pid, dest_addr, src_addr); // or make_key_from_buffer
    bloom_add(key);
}

void 
bloom_add(
    uint32_t key
) {
    bloom_filter[hash1(key) / 8] |= (1 << (hash1(key) % 8));
    bloom_filter[hash2(key) / 8] |= (1 << (hash2(key) % 8));
}

// MAYBE ADD

#define BLOOM_RESET_INTERVAL_MS 5 * 60 * 1000 // 5 mins

unsigned long last_reset = 0;

void bloom_reset(void) {
    memset(bloom_filter, 0, sizeof(bloom_filter));
}

void maybe_reset_bloom_filter(
    void
) {
    if (millis() - last_reset > BLOOM_RESET_INTERVAL_MS) {
        bloom_reset();
        last_reset = millis();
    }
}