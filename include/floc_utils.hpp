#pragma once

#include <stdint.h>

#ifdef ARDUINO // ARDUINO
#include <Arduino.h>

#ifdef min // min
#undef min
#endif //min

#ifdef max //min
#undef max
#endif //min

#endif // ARDUINO

static uint8_t
modemIdFromDidNid(
    uint16_t deviceID,
    uint16_t networkID
){
    return (deviceID * 31 + networkID) & 0xFF;
}

static void
printBufferContents(
    uint8_t* buf,
    uint8_t size
){
    if (buf == nullptr){
        Serial.printf("\tOops! The buffer is a null pointer!\r\n"); 
        return;
    }

    if (size == 0) {
        Serial.printf("\tOops! This buffer is empty!\r\n"); 
        return;
    }

    // Header line
    Serial.printf("=== %zu bytes ===\n", size);
    
    // Process 16 bytes per line
    for (size_t i = 0; i < size; i += 16) {
        size_t chunk_size = (size - i < 16) ? (size - i) : 16;
        
        // Print address
        Serial.printf("%04zX: ", i);
        
        // Print hex bytes
        for (size_t j = 0; j < chunk_size; j++) {
            Serial.printf("%02X", buf[i + j]);
            if (j < chunk_size - 1) {
                Serial.printf(" ");
            }
        }
        
        // Pad hex section to 48 characters if needed
        size_t hex_chars = chunk_size * 3 - 1; // 2 chars per byte + spaces, minus last space
        for (size_t pad = hex_chars; pad < 47; pad++) {
            Serial.printf(" ");
        }
        
        // Print separator
        Serial.printf(" |");
        
        // Print ASCII representation
        for (size_t j = 0; j < chunk_size; j++) {
            unsigned char b = buf[i + j];
            if (b >= 32 && b <= 126) {
                Serial.printf("%c", b);
            } else {
                Serial.printf(".");
            }
        }
        
        // Close ASCII section and newline
        Serial.printf("|\n");
    }
}

static inline
uint16_t
htons(
    uint16_t val
){
    return __builtin_bswap16(val);
}

static inline
uint16_t
ntohs(
    uint16_t val
){
    return __builtin_bswap16(val);
}