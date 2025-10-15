#include "ESPNowMeshClock.h"

ESPNowMeshClock* ESPNowMeshClock::_instance = nullptr;

static uint64_t defaultClockFn() {
    return fastmicros64_isr();
}

ESPNowMeshClock::ESPNowMeshClock(uint16_t interval_ms, float slew_alpha, uint32_t large_step_us, uint32_t sync_timeout_ms, uint8_t random_variation_percent, ClockFn clkfn)
    : _interval(interval_ms), _alpha(slew_alpha), _largeStep(large_step_us), _syncTimeout(sync_timeout_ms), _randomVariation(random_variation_percent),
      _clock(clkfn ? clkfn : defaultClockFn), _offset(0), _synced(false), _lastSync(0), _lastBroadcast(0), _nextBroadcastDelay(0), _userCallback(nullptr)
{
    _instance = this;
}

void ESPNowMeshClock::begin(bool registerCallback) {
    WiFi.mode(WIFI_STA);
    if(esp_now_init() != ESP_OK) {
        Serial.println("[ERR] ESP-NOW INIT FAILED");
        delay(1000); ESP.restart();
    }
    
    // Only register callback if requested (allows user to handle ESP-NOW manually)
    if (registerCallback) {
        esp_now_register_recv_cb(_onReceive);
    }

    // Add broadcast peer
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, bcastAddr, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if(!esp_now_is_peer_exist(bcastAddr)) esp_now_add_peer(&peerInfo);
    Serial.println("[ESPNowMeshClock] Started.");
}

uint64_t ESPNowMeshClock::meshMicros() { return _clock() + _offset; }
uint32_t ESPNowMeshClock::meshMillis() { return meshMicros() / 1000; }

SyncState ESPNowMeshClock::getSyncState() {
    if (!_synced) {
        return SyncState::ALONE;
    }
    uint32_t nowMs = millis();
    if (nowMs - _lastSync > _syncTimeout) {
        return SyncState::LOST;
    }
    return SyncState::SYNCED;
}

bool ESPNowMeshClock::handleReceive(const uint8_t *mac, const uint8_t *data, int len) {
    // Check if this is a mesh clock packet (must be exactly 10 bytes)
    if(len == sizeof(MeshClockPacket)) {
        const MeshClockPacket* packet = (const MeshClockPacket*)data;
        
        // Validate magic header "MCK"
        if(packet->magic[0] == MESHCLOCK_MAGIC_0 &&
           packet->magic[1] == MESHCLOCK_MAGIC_1 &&
           packet->magic[2] == MESHCLOCK_MAGIC_2) {
            
            // Extract 56-bit timestamp (7 bytes) into uint64_t
            uint64_t remoteMicros = 0;
            for(int i = 0; i < 7; i++) {
                remoteMicros |= ((uint64_t)packet->timestamp[i]) << (i * 8);
            }
            
            _adjust(remoteMicros);
            return true;  // Packet was handled
        }
    }
    return false;  // Not a clock packet
}

void ESPNowMeshClock::setUserCallback(ESPNowRecvCallback callback) {
    _userCallback = callback;
}

void ESPNowMeshClock::_onReceive(const uint8_t *mac, const uint8_t *data, int len) {
    if(_instance) {
        // Try to handle as clock packet
        bool handled = _instance->handleReceive(mac, data, len);
        
        // If not a clock packet and user callback is set, forward to user
        if (!handled && _instance->_userCallback) {
            _instance->_userCallback(mac, data, len);
        }
    }
}

void ESPNowMeshClock::_adjust(uint64_t remoteMicros) {
    uint64_t localMicros = _clock() + _offset;
    int64_t  delta = remoteMicros - localMicros;
    
    // Track last successful sync reception
    _lastSync = millis();
    
    if(!_synced || abs(delta) > _largeStep) {
        _offset += delta; _synced = true;
        Serial.printf("[MeshClock SYNC] Direct clock set. Offset now: %lld\n", (int64_t)_offset);
        return;
    }
    if(delta > 0) {
        uint64_t step = (uint64_t)(delta * _alpha);
        _offset += step;
        Serial.printf("[MeshClock SYNC] Slewed clock: Offset %lld, Step %llu, Delta %lld\n",
                      (int64_t)_offset, step, (int64_t)delta);
    }
    // If delta < 0, local is already ahead - no adjustment needed (forward-only)
}

void ESPNowMeshClock::_broadcast() {
    uint64_t stamp = meshMicros();
    
    // Prepare packet with magic header and 7-byte timestamp
    MeshClockPacket packet;
    packet.magic[0] = MESHCLOCK_MAGIC_0;
    packet.magic[1] = MESHCLOCK_MAGIC_1;
    packet.magic[2] = MESHCLOCK_MAGIC_2;
    
    // Pack 56-bit timestamp (7 bytes, little-endian)
    for(int i = 0; i < 7; i++) {
        packet.timestamp[i] = (stamp >> (i * 8)) & 0xFF;
    }
    
    esp_err_t result = esp_now_send(bcastAddr, (uint8_t*)&packet, sizeof(packet));
    if(result == ESP_OK) {
        Serial.printf("[MeshClock BCAST] Sent time: %llu\n", stamp);
    } else {
        Serial.println("[MeshClock ERROR] Failed to send time");
    }
}

void ESPNowMeshClock::loop() {
    uint32_t nowMs = millis();
    
    // Calculate randomized interval on first call or after each broadcast
    if (_nextBroadcastDelay == 0) {
        // Add random variation: interval ± random_variation_percent
        int32_t variation = (_interval * _randomVariation) / 100;
        int32_t randomOffset = random(-variation, variation + 1);
        _nextBroadcastDelay = _interval + randomOffset;
    }
    
    if (nowMs - _lastBroadcast >= _nextBroadcastDelay) {
        _lastBroadcast = nowMs;
        _nextBroadcastDelay = 0; // Reset to recalculate next time
        _broadcast();
    }
}