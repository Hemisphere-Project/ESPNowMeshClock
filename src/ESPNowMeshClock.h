#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include "libclock/fastmillis.h"

// Magic header for mesh clock packets: "MCK"
#define MESHCLOCK_MAGIC_0 0x4D  // 'M'
#define MESHCLOCK_MAGIC_1 0x43  // 'C'
#define MESHCLOCK_MAGIC_2 0x4B  // 'K'

#ifndef TRANSMISSION_DELAY_US
    #define TRANSMISSION_DELAY_US 1000  // Estimated one-way transmission delay in microseconds
#endif

// Mesh clock packet structure (10 bytes total)
// 3-byte magic header + 7-byte timestamp (56-bit) = ~2283 years rollover
struct MeshClockPacket {
    uint8_t magic[3];      // "MCK" identifier
    uint8_t timestamp[7];  // 56-bit microseconds (little-endian)
};

// User can supply their own clock if desired
typedef uint64_t (*ClockFn)();

// User callback for ESP-NOW messages (for callback chaining)
typedef void (*ESPNowRecvCallback)(const uint8_t *mac, const uint8_t *data, int len);

// Synchronization state enumeration
enum class SyncState {
    ALONE,   // No sync has occurred yet (node is alone)
    SYNCED,  // Currently synchronized
    LOST     // Was synced, but timeout exceeded (link lost)
};

// Debug log flags
enum DebugLog {
    LOG_BCAST = 0x01,  // Broadcast messages
    LOG_RX    = 0x02,  // Receive messages
    LOG_SYNC  = 0x04,  // Sync adjustments
    LOG_ALL   = 0xFF   // All messages
};

class ESPNowMeshClock {
public:
    ESPNowMeshClock(uint16_t interval_ms = 1000, float slew_alpha = 0.25, uint32_t large_step_us = 10000, uint32_t sync_timeout_ms = 5000, uint8_t random_variation_percent = 10, ClockFn clkfn = nullptr);
    void begin(bool registerCallback = true);
    void loop(); // Call this often in main loop
    uint64_t meshMicros();
    uint32_t meshMillis();
    SyncState getSyncState();
    
    // Debug log control
    void setDebugLog(uint8_t flags) { _debugLog = flags; }
    
    // Option 1: Manual receive handling for custom ESP-NOW integration
    bool handleReceive(const uint8_t *mac, const uint8_t *data, int len);
    
    // Option 2: Callback chaining for automatic forwarding of non-clock packets
    void setUserCallback(ESPNowRecvCallback callback);

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
    ESPNowRecvCallback _userCallback;
    uint8_t  _debugLog;

    #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    static void _onReceive(const esp_now_recv_info *recv_info, const uint8_t *data, int len);
    #else
    static void _onReceive(const uint8_t *mac, const uint8_t *data, int len);
    #endif
    static ESPNowMeshClock* _instance;
    void _adjust(uint64_t remoteMicros);
    void _broadcast();
};