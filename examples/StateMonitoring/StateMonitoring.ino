/*
 * ESPNowMeshClock - State Monitoring with LED Example
 * 
 * This example demonstrates advanced state monitoring with visual feedback.
 * Features:
 * - LED indicators for sync state (built-in LED)
 * - Detailed statistics and diagnostics
 * - Custom configuration for faster sync
 * - Serial output formatting
 * 
 * LED Pattern:
 * - Fast blink (100ms): ALONE - waiting for sync
 * - Solid ON: SYNCED - successfully synchronized
 * - Slow blink (500ms): LOST - lost connection to mesh
 */

#include <ESPNowMeshClock.h>

// Pin configuration
#define LED_PIN LED_BUILTIN  // Usually GPIO 2 on most ESP32 boards

// Custom configuration for faster sync and tighter monitoring
ESPNowMeshClock meshClock(
    500,   // 500ms broadcast interval (faster updates)
    0.5,   // 0.5 slew rate (faster convergence)
    5000,  // 5ms large step threshold
    2000,  // 2s sync timeout (detect loss faster)
    15     // ±15% random variation (better collision avoidance)
);

unsigned long lastLedUpdate = 0;
bool ledState = false;

void updateLedPattern(SyncState state) {
    unsigned long now = millis();
    unsigned long interval;
    
    switch(state) {
        case SyncState::ALONE:
            // Fast blink - waiting for sync
            interval = 100;
            if (now - lastLedUpdate >= interval) {
                lastLedUpdate = now;
                ledState = !ledState;
                digitalWrite(LED_PIN, ledState);
            }
            break;
            
        case SyncState::SYNCED:
            // Solid ON - synchronized
            digitalWrite(LED_PIN, HIGH);
            break;
            
        case SyncState::LOST:
            // Slow blink - warning
            interval = 500;
            if (now - lastLedUpdate >= interval) {
                lastLedUpdate = now;
                ledState = !ledState;
                digitalWrite(LED_PIN, ledState);
            }
            break;
    }
}

void printStateInfo(SyncState state) {
    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║     ESPNowMeshClock - STATE CHANGE     ║");
    Serial.println("╚════════════════════════════════════════╝");
    
    Serial.print("  New State: ");
    switch(state) {
        case SyncState::ALONE:
            Serial.println("🔴 ALONE");
            Serial.println("  Status: Waiting for mesh network...");
            Serial.println("  LED: Fast blink (100ms)");
            break;
            
        case SyncState::SYNCED:
            Serial.println("🟢 SYNCED");
            Serial.println("  Status: Successfully synchronized!");
            Serial.println("  LED: Solid ON");
            break;
            
        case SyncState::LOST:
            Serial.println("🟡 LOST");
            Serial.println("  Status: Connection to mesh lost!");
            Serial.println("  LED: Slow blink (500ms)");
            break;
    }
    Serial.println();
}

void printStatistics() {
    static unsigned long lastPrint = 0;
    unsigned long now = millis();
    
    // Print stats every 5 seconds when synced
    if (now - lastPrint >= 5000) {
        lastPrint = now;
        
        SyncState state = meshClock.getSyncState();
        if (state == SyncState::SYNCED) {
            uint64_t meshUs = meshClock.meshMicros();
            uint32_t meshMs = meshClock.meshMillis();
            
            Serial.println("┌─────────── MESH STATISTICS ───────────┐");
            Serial.printf("│ Mesh Time (µs): %19llu │\n", meshUs);
            Serial.printf("│ Mesh Time (ms): %19u │\n", meshMs);
            Serial.printf("│ Uptime    (ms): %19lu │\n", millis());
            Serial.println("│ State:          SYNCED ✓              │");
            Serial.println("└───────────────────────────────────────┘\n");
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // Configure LED pin
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    
    Serial.println("\n");
    Serial.println("╔══════════════════════════════════════════════╗");
    Serial.println("║  ESPNowMeshClock - Advanced State Monitor   ║");
    Serial.println("╚══════════════════════════════════════════════╝");
    Serial.println();
    Serial.println("Configuration:");
    Serial.println("  • Broadcast interval: 500ms (±15%)");
    Serial.println("  • Slew rate: 0.5");
    Serial.println("  • Sync timeout: 2000ms");
    Serial.println("  • LED: State indicator enabled");
    Serial.println();
    
    // Initialize mesh clock
    meshClock.begin();
    
    Serial.println("Mesh clock initialized. Waiting for sync...\n");
}

void loop() {
    // Update mesh clock (handles broadcasts and receives)
    meshClock.loop();
    
    // Get current state
    SyncState currentState = meshClock.getSyncState();
    
    // Monitor state changes
    static SyncState lastState = SyncState::ALONE;
    if (currentState != lastState) {
        lastState = currentState;
        printStateInfo(currentState);
    }
    
    // Update LED pattern based on state
    updateLedPattern(currentState);
    
    // Print statistics periodically when synced
    printStatistics();
    
    // Example: Coordinated action every 2 seconds
    static uint32_t lastAction = 0;
    uint32_t meshMs = meshClock.meshMillis();
    
    if (meshMs - lastAction >= 2000) {
        lastAction = meshMs;
        
        if (currentState == SyncState::SYNCED) {
            Serial.printf("⏱  Synchronized tick at mesh time: %u ms\n", meshMs);
        }
    }
    
    delay(10);
}
