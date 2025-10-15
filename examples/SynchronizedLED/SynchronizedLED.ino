/*
 * ESPNowMeshClock - Synchronized LED Pattern Example
 * 
 * This example demonstrates perfect synchronization across multiple ESP32s
 * by using mesh time to coordinate LED patterns.
 * 
 * All devices in the mesh will blink their LEDs in perfect unison!
 * 
 * Hardware:
 * - Built-in LED (or external LED on GPIO 2)
 * 
 * Use case: Synchronized light shows, DMX control, coordinated animations
 */

#include <ESPNowMeshClock.h>

#define LED_PIN LED_BUILTIN

// Fast sync configuration for tight coordination
ESPNowMeshClock meshClock(
    250,   // 250ms broadcast interval
    0.6,   // Higher slew rate for faster sync
    3000,  // 3ms threshold
    1500,  // 1.5s timeout
    20     // ±20% variation
);

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    pinMode(LED_PIN, OUTPUT);
    
    Serial.println("\n=== Synchronized LED Pattern Demo ===");
    Serial.println("All devices will blink in perfect sync!\n");
    
    meshClock.begin();
}

void loop() {
    meshClock.loop();
    
    SyncState state = meshClock.getSyncState();
    
    if (state == SyncState::SYNCED) {
        // Get mesh time and create synchronized pattern
        uint64_t meshUs = meshClock.meshMicros();
        
        // Pattern 1: Simple blink every 500ms
        // All devices will have LED ON/OFF at exactly the same time
        uint32_t phase = (meshUs / 1000) % 1000;  // 0-999ms cycle
        bool ledOn = phase < 500;
        digitalWrite(LED_PIN, ledOn);
        
        // You can create more complex patterns:
        /*
        // Pattern 2: Fast pulse every 2 seconds
        uint32_t cycle = (meshUs / 1000000) % 2;  // 0-1 (2 second cycle)
        uint32_t phase2 = (meshUs / 1000) % 2000;
        bool pulse = (cycle == 0 && phase2 < 100);  // 100ms pulse every 2s
        digitalWrite(LED_PIN, pulse);
        */
        
        /*
        // Pattern 3: Breathing effect (PWM)
        uint32_t breathCycle = (meshUs / 10000) % 200;  // 0-199 (2s cycle)
        uint8_t brightness;
        if (breathCycle < 100) {
            brightness = map(breathCycle, 0, 99, 0, 255);
        } else {
            brightness = map(breathCycle, 100, 199, 255, 0);
        }
        analogWrite(LED_PIN, brightness);
        */
        
        // Log sync status every 5 seconds
        static uint32_t lastLog = 0;
        uint32_t meshMs = meshClock.meshMillis();
        if (meshMs - lastLog >= 5000) {
            lastLog = meshMs;
            Serial.printf("[SYNCED] Mesh time: %llu µs - LED Pattern Active\n", meshUs);
        }
        
    } else {
        // Not synced - show waiting pattern
        static unsigned long lastBlink = 0;
        if (millis() - lastBlink >= 100) {
            lastBlink = millis();
            digitalWrite(LED_PIN, !digitalRead(LED_PIN));
        }
        
        // Log state
        static SyncState lastState = SyncState::SYNCED;
        if (state != lastState) {
            lastState = state;
            Serial.println(state == SyncState::ALONE ? 
                          "Waiting for sync..." : "WARNING: Link lost!");
        }
    }
    
    delay(1);
}
