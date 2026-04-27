#pragma once
#include <stdint.h>

#define UDP_PORT 9090
#define PACKET_FRAMES 256
#define NUM_CHANNELS 2
#define RING_BUFFER_SIZE 32768 

struct AudioPacket {
    uint32_t sequenceNumber;
    uint32_t numFrames;
    float samples[NUM_CHANNELS * PACKET_FRAMES]; 
};