/*
 * ESPNowMeshClock - Custom ESP-NOW Integration (Option 1)
 * 
 * This example demonstrates how to integrate ESPNowMeshClock into a project
 * that already uses ESP-NOW for other purposes.
 * 
 * OPTION 1: Manual receive handling
 * - You manage the ESP-NOW callback yourself
 * - Call meshClock.handleReceive() for each packet
 * - Full control over packet routing
 * 
 * Use case: You have existing ESP-NOW communication and want to add mesh clock
 */

#include <ESPNowMeshClock.h>

// Your custom message structure
struct MyCustomMessage {
    uint8_t type;
    uint8_t data[32];
};

ESPNowMeshClock meshClock;

// Your custom ESP-NOW receive callback
void onESPNowReceive(const uint8_t *mac, const uint8_t *data, int len) {
    // OPTION 1: Let mesh clock try to handle the packet first
    // It will check for 10-byte packets with \"MCK\" magic header
    if (meshClock.handleReceive(mac, data, len)) {
        // It was a mesh clock packet (10 bytes: \"MCK\" + timestamp), already processed
        Serial.println(\"[ESP-NOW] Mesh clock packet received\");
        return;
    }
    
    // Not a clock packet - handle your own messages
    Serial.printf("[ESP-NOW] Custom packet from %02X:%02X:%02X:%02X:%02X:%02X, length: %d\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], len);
    
    // Your custom packet handling
    if (len == sizeof(MyCustomMessage)) {
        MyCustomMessage msg;
        memcpy(&msg, data, sizeof(msg));
        Serial.printf("  Type: %d, Data[0]: %d\n", msg.type, msg.data[0]);
        // ... process your message ...
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=== Custom ESP-NOW Integration (Option 1) ===");
    Serial.println("Mesh clock + custom ESP-NOW messages\n");
    
    // Initialize mesh clock WITHOUT registering its own callback
    meshClock.begin(false);  // false = don't register callback
    
    // Register YOUR callback instead
    esp_now_register_recv_cb(onESPNowReceive);
    
    Serial.println("Ready! Send custom ESP-NOW messages to this device.");
}

void loop() {
    meshClock.loop();
    
    // Monitor sync state
    static SyncState lastState = SyncState::ALONE;
    SyncState state = meshClock.getSyncState();
    
    if (state != lastState) {
        lastState = state;
        Serial.printf("Mesh clock state: %s\n", 
                     state == SyncState::ALONE ? "ALONE" :
                     state == SyncState::SYNCED ? "SYNCED" : "LOST");
    }
    
    // Example: Send a custom message every 5 seconds
    static uint32_t lastSend = 0;
    if (millis() - lastSend >= 5000) {
        lastSend = millis();
        
        MyCustomMessage msg;
        msg.type = 42;
        msg.data[0] = random(0, 255);
        
        uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        esp_err_t result = esp_now_send(broadcast, (uint8_t*)&msg, sizeof(msg));
        
        if (result == ESP_OK) {
            Serial.printf("[Custom] Sent custom message (type=%d, data[0]=%d)\n", 
                         msg.type, msg.data[0]);
        }
    }
    
    delay(10);
}
