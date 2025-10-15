#include "ESPNowMeshClock.h"

ESPNowMeshClock* ESPNowMeshClock::_instance = nullptr;

static uint64_t defaultClockFn() {
    return fastmicros64_isr();
}

ESPNowMeshClock::ESPNowMeshClock(uint16_t interval_ms, float slew_alpha, uint32_t large_step_us, uint32_t sync_timeout_ms, uint8_t random_variation_percent, ClockFn clkfn)
    : _interval(interval_ms), _alpha(slew_alpha), _largeStep(large_step_us), _syncTimeout(sync_timeout_ms), _randomVariation(random_variation_percent),
      _clock(clkfn ? clkfn : defaultClockFn), _offset(0), _synced(false), _lastSync(0), _lastBroadcast(0), _nextBroadcastDelay(0), _userCallback(nullptr), _debugLog(LOG_SYNC)
{
    _instance = this;
}

void ESPNowMeshClock::begin(bool registerCallback) {
    // Initialize hardware timers FIRST if using default clock
    if (_clock == defaultClockFn) {
        static bool timersInitialized = false;
        if (!timersInitialized) {
            Serial.println("[ESPNowMeshClock] Initializing timer...");
            Serial.print("[ESPNowMeshClock] Chip: ");
            Serial.println(ESP.getChipModel());
            Serial.printf("[ESPNowMeshClock] CPU Freq: %d MHz\r\n", getCpuFrequencyMhz());

            fastinit();
            timersInitialized = true;

            delay(100);  // Give timers time to start counting

            // Test timer multiple times
            uint64_t test1 = fastmicros64_isr();
            delayMicroseconds(1000);
            uint64_t test2 = fastmicros64_isr();

            Serial.printf("[ESPNowMeshClock] Timer test: %llu -> %llu (diff: %llu us)\r\n",
                         test1, test2, test2 - test1);

            if (test1 == 0 && test2 == 0) {
                Serial.println("╔════════════════════════════════════════════════════════════╗");
                Serial.println("║ [CRITICAL ERROR] Hardware timer NOT working!              ║");
                Serial.println("║ fastmicros64_isr() is returning 0.                        ║");
                Serial.println("║ Clock synchronization will NOT function correctly.        ║");
                Serial.println("║ Check timer registers for your ESP32 variant.             ║");
                Serial.println("╚════════════════════════════════════════════════════════════╝");
            }
        }
    }

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
    // Log all received packets for debugging
    if(_debugLog & LOG_RX) {
        Serial.printf("[MeshClock RX] Received %d bytes from %02X:%02X:%02X:%02X:%02X:%02X\r\n",
                      len, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
    
    // Check if this is a mesh clock packet (must be exactly 10 bytes)
    if(len != sizeof(MeshClockPacket)) {
        if(_debugLog & LOG_RX) {
            Serial.printf("[MeshClock RX] Discarded: Wrong size (expected %d bytes)\r\n", sizeof(MeshClockPacket));
        }
        return false;
    }
    
    const MeshClockPacket* packet = (const MeshClockPacket*)data;
    
    // Validate magic header "MCK"
    if(packet->magic[0] != MESHCLOCK_MAGIC_0 ||
       packet->magic[1] != MESHCLOCK_MAGIC_1 ||
       packet->magic[2] != MESHCLOCK_MAGIC_2) {
        if(_debugLog & LOG_RX) {
            Serial.printf("[MeshClock RX] Discarded: Invalid magic header (%02X %02X %02X)\r\n",
                          packet->magic[0], packet->magic[1], packet->magic[2]);
        }
        return false;
    }
    
    // Extract 56-bit timestamp (7 bytes) into uint64_t
    uint64_t remoteMicros = 0;
    for(int i = 0; i < 7; i++) {
        remoteMicros |= ((uint64_t)packet->timestamp[i]) << (i * 8);
    }
    
    if(_debugLog & LOG_RX) {
        uint32_t secs = remoteMicros / 1000000;
        uint32_t usecs = remoteMicros % 1000000;
        Serial.printf("[MeshClock RX] Valid clock packet: %llu us (%u.%06u s)\r\n", 
                      remoteMicros, secs, usecs);
    }
    
    _adjust(remoteMicros);
    return true;  // Packet was handled
}

void ESPNowMeshClock::setUserCallback(ESPNowRecvCallback callback) {
    _userCallback = callback;
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
void ESPNowMeshClock::_onReceive(const esp_now_recv_info *recv_info, const uint8_t *data, int len) {
    if(_instance) {
        // Extract MAC address from recv_info (new API)
        const uint8_t *mac = recv_info->src_addr;

        // Try to handle as clock packet
        bool handled = _instance->handleReceive(mac, data, len);

        // If not a clock packet and user callback is set, forward to user
        if (!handled && _instance->_userCallback) {
            _instance->_userCallback(mac, data, len);
        }
    }
}
#else
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
#endif

void ESPNowMeshClock::_adjust(uint64_t remoteMicros) {
    uint64_t localMicros = _clock() + _offset;
    int64_t  delta = remoteMicros - localMicros;

    // Track last successful sync reception
    _lastSync = millis();

    // Direct clock set needed (first sync or large deviation)
    if(!_synced || abs(delta) > _largeStep) {
        if(delta > 0) {
            // Remote is ahead: adjust forward
            _offset += delta;
            _synced = true;
            if(_debugLog & LOG_SYNC) {
                Serial.printf("[MeshClock SYNC] Direct set forward. Offset: %lld us, Delta: %lld us\r\n",
                             (int64_t)_offset, (int64_t)delta);
            }
        } else {
            // Remote is behind: ignore (forward-only), but mark as synced
            _synced = true;
            if(_debugLog & LOG_SYNC) {
                Serial.printf("[MeshClock SYNC] Ignored (remote behind by %lld us, forward-only)\r\n",
                             (int64_t)(-delta));
            }
        }
        return;
    }

    // Small adjustment: slew forward only
    if(delta > 0) {
        uint64_t step = (uint64_t)(delta * _alpha);
        _offset += step;
        if(_debugLog & LOG_SYNC) {
            Serial.printf("[MeshClock SYNC] Slewed forward. Offset: %lld us, Step: %llu us, Delta: %lld us\r\n",
                          (int64_t)_offset, step, (int64_t)delta);
        }
    } else {
        // Remote is behind or equal: no adjustment (forward-only)
        if(_debugLog & LOG_SYNC) {
            Serial.printf("[MeshClock SYNC] No adjustment (remote behind by %lld us)\r\n",
                         (int64_t)(-delta));
        }
    }
}

void ESPNowMeshClock::_broadcast() {
    uint64_t stamp = meshMicros() + TRANSMISSION_DELAY_US;

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
        if(_debugLog & LOG_BCAST) {
            uint32_t secs = stamp / 1000000;
            uint32_t usecs = stamp % 1000000;
            Serial.printf("[MeshClock BCAST] Sent time: %llu us (%u.%06u s)\\r\\n", stamp, secs, usecs);
        }
    } else {
        if(_debugLog & LOG_BCAST) {
            Serial.println("[MeshClock ERROR] Failed to send time");
        }
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