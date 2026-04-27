// NetworkAudio.h
#pragma once
#include <stdint.h>

#define UDP_PORT 9090
#define PACKET_FRAMES 256 // Enviamos audio en bloques de 256 samples
#define NUM_CHANNELS 2
#define RING_BUFFER_SIZE 32768 // Buffer elástico gigante para evitar tirones

// La "caja" que enviaremos por la red
struct AudioPacket {
    uint32_t sequenceNumber;
    uint32_t numFrames;
    float samples[NUM_CHANNELS * PACKET_FRAMES]; // Audio entrelazado (L, R, L, R...)
};