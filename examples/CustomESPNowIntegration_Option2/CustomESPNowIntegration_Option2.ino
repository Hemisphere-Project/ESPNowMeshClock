/*
 * ESPNowMeshClock - Custom ESP-NOW Integration (Option 2)
 * 
 * This example demonstrates how to integrate ESPNowMeshClock into a project
 * that already uses ESP-NOW for other purposes.
 * 
 * OPTION 2: Callback chaining
 * - Mesh clock registers its callback
 * - You register your callback with the mesh clock
 * - Clock packets are handled automatically
 * - Non-clock packets are forwarded to your callback
 * 
 * Use case: Simpler integration - let the library handle routing
 */

#include <ESPNowMeshClock.h>

// Your custom message structure
struct MyCustomMessage {
    uint8_t type;
    uint8_t data[32];
};

ESPNowMeshClock meshClock;

// Your callback for NON-clock ESP-NOW messages
void onCustomMessage(const uint8_t *mac, const uint8_t *data, int len) {
    // This callback only receives packets that are NOT mesh clock packets
    Serial.printf("[Custom MSG] From %02X:%02X:%02X:%02X:%02X:%02X, length: %d\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], len);
    
    // Your custom packet handling
    if (len == sizeof(MyCustomMessage)) {
        MyCustomMessage msg;
        memcpy(&msg, data, sizeof(msg));
        Serial.printf("  Type: %d, Data[0]: %d\n", msg.type, msg.data[0]);
        // ... process your message ...
    } else {
        Serial.println("  Unknown message format");
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=== Custom ESP-NOW Integration (Option 2) ===");
    Serial.println("Automatic callback chaining\n");
    
    // OPTION 2: Set your callback for non-clock messages
    meshClock.setUserCallback(onCustomMessage);
    
    // Initialize mesh clock - it will register its callback
    // Clock packets: handled by mesh clock automatically
    // Other packets: forwarded to your callback
    meshClock.begin();  // true by default = register callback with chaining
    
    Serial.println(\"Ready! Mesh clock will auto-route packets:\");
    Serial.println(\"  - 10-byte packets with 'MCK' header → mesh clock\");
    Serial.println(\"  - Other packets → your callback\\n\");
}

void loop() {
    meshClock.loop();
    
    // Monitor sync state
    static SyncState lastState = SyncState::ALONE;
    SyncState state = meshClock.getSyncState();
    
    if (state != lastState) {
        lastState = state;
        Serial.printf("[State] Mesh clock: %s\n", 
                     state == SyncState::ALONE ? "ALONE" :
                     state == SyncState::SYNCED ? "SYNCED" : "LOST");
    }
    
    // Example: Send a custom message every 5 seconds
    static uint32_t lastSend = 0;
    if (millis() - lastSend >= 5000) {
        lastSend = millis();
        
        MyCustomMessage msg;
        msg.type = 99;
        msg.data[0] = random(0, 255);
        
        uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        esp_err_t result = esp_now_send(broadcast, (uint8_t*)&msg, sizeof(msg));
        
        if (result == ESP_OK) {
            Serial.printf("[Custom TX] Sent message (type=%d, data[0]=%d)\n", 
                         msg.type, msg.data[0]);
        }
    }
    
    // Display synced time every 3 seconds
    static uint32_t lastDisplay = 0;
    uint32_t meshMs = meshClock.meshMillis();
    if (meshMs - lastDisplay >= 3000 && state == SyncState::SYNCED) {
        lastDisplay = meshMs;
        Serial.printf("[MeshClock] Synced time: %u ms\n", meshMs);
    }
    
    delay(10);
}
