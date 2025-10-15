#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include "libclock/fastmillis.h"

// User can supply their own clock if desired
typedef uint64_t (*ClockFn)();

// Synchronization state enumeration
enum class SyncState {
    ALONE,   // No sync has occurred yet (node is alone)
    SYNCED,  // Currently synchronized
    LOST     // Was synced, but timeout exceeded (link lost)
};

class ESPNowMeshClock {
public:
    ESPNowMeshClock(uint16_t interval_ms = 1000, float slew_alpha = 0.25, uint32_t large_step_us = 10000, uint32_t sync_timeout_ms = 5000, uint8_t random_variation_percent = 10, ClockFn clkfn = nullptr);
    void begin();
    void loop(); // Call this often in main loop
    uint64_t meshMicros();
    uint32_t meshMillis();
    SyncState getSyncState();

private:
    uint8_t bcastAddr[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    uint16_t _interval;
    float    _alpha;
    uint32_t _largeStep;
    uint32_t _syncTimeout;
    uint8_t  _randomVariation;
    ClockFn  _clock;
    uint64_t _offset;
    bool     _synced;
    uint32_t _lastSync;
    uint32_t _lastBroadcast;
    uint32_t _nextBroadcastDelay;

    static void _onReceive(const uint8_t *mac, const uint8_t *data, int len);
    static ESPNowMeshClock* _instance;
    void _adjust(uint64_t remoteMicros);
    void _broadcast();
};