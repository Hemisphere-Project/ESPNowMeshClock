/*
 * ESPNowMeshClock - Basic Sync Example
 * 
 * This example demonstrates the basic usage of the ESPNowMeshClock library.
 * It shows how to:
 * - Initialize the mesh clock
 * - Monitor synchronization state
 * - Use mesh-synchronized time for coordinated actions
 * - Validate sync accuracy with LED pulses (visual or oscilloscope)
 * 
 * Upload this sketch to multiple ESP32 boards and watch them synchronize!
 * The LED will pulse for 100ms every second in perfect sync across all nodes.
 */

#include <ESPNowMeshClock.h>

// Pin configuration - adjust for your board
#define SYNC_LED_PIN 2  // GPIO2 (built-in LED on most ESP32 boards)

// Create mesh clock with default settings:
// - 1000ms broadcast interval (±10% random variation)
// - 0.25 slew rate for smooth adjustments
// - 10ms large step threshold
// - 5000ms sync timeout
ESPNowMeshClock meshClock;

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // Configure LED pin
    pinMode(SYNC_LED_PIN, OUTPUT);
    digitalWrite(SYNC_LED_PIN, LOW);
    
    Serial.println("\n=== ESPNowMeshClock Basic Example ===");
    Serial.println("Starting mesh clock synchronization...");
    Serial.printf("Sync LED on GPIO %d\n\n", SYNC_LED_PIN);
    
    // Initialize the mesh clock
    meshClock.begin();
}

void loop() {
    // IMPORTANT: Call this regularly to handle broadcasts
    meshClock.loop();
    
    // Monitor and display sync state changes
    static SyncState lastState = SyncState::ALONE;
    SyncState currentState = meshClock.getSyncState();
    
    if (currentState != lastState) {
        lastState = currentState;
        
        Serial.print(">>> State Change: ");
        switch(currentState) {
            case SyncState::ALONE:
                Serial.println("ALONE - Waiting for first sync message...");
                break;
            case SyncState::SYNCED:
                Serial.println("SYNCED - Successfully synchronized with mesh!");
                break;
            case SyncState::LOST:
                Serial.println("LOST - No sync messages received recently!");
                break;
        }
    }
    
    // Synchronized LED pulse every second
    // All nodes will pulse their LED at EXACTLY the same time
    // Perfect for visual validation or oscilloscope measurement
    static uint32_t lastPulse = 0;
    uint32_t meshMs = meshClock.meshMillis();
    
    // Check if it's time for a pulse (every 1000ms)
    if (meshMs - lastPulse >= 1000) {
        lastPulse = meshMs;
        
        // HIGH for 100ms - DO THIS FIRST before any Serial.print
        digitalWrite(SYNC_LED_PIN, HIGH);
        uint64_t meshUs = meshClock.meshMicros();  // Capture precise time
        delayMicroseconds(100000);  // 100ms = 100,000µs
        digitalWrite(SYNC_LED_PIN, LOW);
        
        // Now safe to print (after pin changes to preserve timing accuracy)
        Serial.printf("[PULSE] Mesh time: %llu µs (%u ms) - State: ", 
                      meshUs, meshMs);
        
        switch(currentState) {
            case SyncState::ALONE:
                Serial.println("ALONE");
                break;
            case SyncState::SYNCED:
                Serial.println("SYNCED ✓");
                break;
            case SyncState::LOST:
                Serial.println("LOST ⚠");
                break;
        }
    }
    
    delay(10);
}
