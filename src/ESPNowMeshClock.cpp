#include "ESPNowMeshClock.h"

ESPNowMeshClock* ESPNowMeshClock::_instance = nullptr;

static uint64_t defaultClockFn() {
    return fastmicros64_isr();
}

ESPNowMeshClock::ESPNowMeshClock(uint16_t interval_ms, float slew_alpha, uint32_t large_step_us, uint32_t sync_timeout_ms, uint8_t random_variation_percent, ClockFn clkfn)
    : _interval(interval_ms), _alpha(slew_alpha), _largeStep(large_step_us), _syncTimeout(sync_timeout_ms), _randomVariation(random_variation_percent),
      _clock(clkfn ? clkfn : defaultClockFn), _offset(0), _synced(false), _lastSync(0), _lastBroadcast(0), _nextBroadcastDelay(0)
{
    _instance = this;
}

void ESPNowMeshClock::begin() {
    WiFi.mode(WIFI_STA);
    if(esp_now_init() != ESP_OK) {
        Serial.println("[ERR] ESP-NOW INIT FAILED");
        delay(1000); ESP.restart();
    }
    esp_now_register_recv_cb(_onReceive);

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

void ESPNowMeshClock::_onReceive(const uint8_t *mac, const uint8_t *data, int len) {
    if(len == sizeof(uint64_t) && _instance) {
        uint64_t remoteMicros;
        memcpy(&remoteMicros, data, sizeof(remoteMicros));
        _instance->_adjust(remoteMicros);
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
    esp_err_t result = esp_now_send(bcastAddr, (uint8_t*)&stamp, sizeof(stamp));
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