# ESPNowMeshClock

**Robust monotonic mesh time synchronization library for ESP32 over ESP-NOW radio.** 

Distributed, driverless 64-bit time sync—perfect for synchronized DMX, MIDI, MTC, media, lighting and show control mesh applications.. or whatever you want to do with wireless micro-seconds accuracy sync !

---

## Features

- Distributed: every node broadcasts its local mesh time, others align (forward-only, monotonic)
- **No rollover risk**: uses embedded **libclock** with 64-bit hardware timer (`fastmicros64_isr`), and clock value is broadcasted as 56-bit value (~2283 years rollover).
- Slewed/smoothed time alignment: handles radio packet jitter and burst/delay gracefully
- **Collision avoidance**: randomized broadcast intervals prevent packet collisions in dense meshes
- **State monitoring**: query sync status (ALONE, SYNCED, LOST)
- **Configurable timeout**: detect and report lost mesh connectivity
- Robust across network loss/packet drop—self-healing mesh
- Simple API: just call `begin()` + `loop()`
- **ESP-NOW compatible**: works alongside existing ESP-NOW code via callback delegation/chaining with magic header packet identification
- Plug-and-play with PlatformIO: drop into any project (`lib_deps`)

---

## Quickstart

### PlatformIO setup

Add this to your `platformio.ini`:

```
lib_deps =
    https://github.com/Hemisphere-Project/ESPNowMeshClock.git
```

### Example Sketch

```
#include <ESPNowMeshClock.h>

// Create mesh clock with:
// - 1000ms broadcast interval
// - 0.25 slew rate
// - 10ms large step threshold
// - 5000ms sync timeout
// - ±10% random variation
ESPNowMeshClock meshClock(1000, 0.25, 10000, 5000, 10);

void setup() {
    Serial.begin(115200);
    meshClock.begin();
    Serial.println("ESPNowMeshClock started");
}

void loop() {
    meshClock.loop();

    // Check sync state
    SyncState state = meshClock.getSyncState();
    static SyncState lastState = SyncState::ALONE;
    
    if(state != lastState) {
        lastState = state;
        switch(state) {
            case SyncState::ALONE:
                Serial.println("Status: Waiting for sync...");
                break;
            case SyncState::SYNCED:
                Serial.println("Status: SYNCED");
                break;
            case SyncState::LOST:
                Serial.println("Status: LINK LOST!");
                break;
        }
    }

    // Use mesh-synced time for your application
    uint64_t meshTime = meshClock.meshMicros();
    uint32_t meshMs = meshClock.meshMillis();
    
    // Example: Synchronized action every second
    static uint32_t lastAction = 0;
    if(meshMs - lastAction >= 1000) {
        lastAction = meshMs;
        Serial.printf("Synchronized tick at %llu µs\n", meshTime);
    }
    
    delay(10);
}
```

---

## Why not micros()?

The Arduino `micros()` function on ESP32 is 32-bit and wraps every ~71min (*breaking* any forward-only mesh time sync algorithm after a single wrap)!  
By embedding **libclock**, this library defaults to `fastmicros64_isr()`, a true 64-bit hardware timer—**no rollover, maximum robustness.**

---

## API

### Constructor

```cpp
ESPNowMeshClock(uint16_t interval_ms = 1000, 
                float slew_alpha = 0.25, 
                uint32_t large_step_us = 10000, 
                uint32_t sync_timeout_ms = 5000,
                uint8_t random_variation_percent = 10,
                ClockFn clkfn = nullptr)
```

Creates a new ESPNowMeshClock instance with configurable parameters.

**Parameters:**
- `interval_ms` (default: 1000): Broadcast interval in milliseconds. How often this node broadcasts its mesh time to the network.
- `slew_alpha` (default: 0.25): Slew rate for smooth time adjustments (0.0 to 1.0). Lower values = smoother but slower convergence. Higher values = faster convergence but more abrupt changes.
- `large_step_us` (default: 10000): Threshold in microseconds for direct time adjustment vs slewing. Deltas larger than this will use direct adjustment instead of gradual slewing.
- `sync_timeout_ms` (default: 5000): Time in milliseconds after which the sync link is considered lost if no messages are received.
- `random_variation_percent` (default: 10): Percentage of random variation (±) applied to broadcast interval to avoid packet collisions. Higher values = better collision avoidance in dense meshes.
- `clkfn` (default: nullptr): Optional custom clock function. If null, uses the built-in 64-bit hardware timer (`fastmicros64_isr`).

**Example:**
```cpp
// Default settings (1s broadcast, 0.25 slew, 10ms threshold, 5s timeout, ±10% variation)
ESPNowMeshClock meshClock;

// Custom settings for faster sync with tighter timeout and more collision avoidance
ESPNowMeshClock meshClock(500, 0.5, 5000, 2000, 20);

// Dense mesh with many nodes - increase variation
ESPNowMeshClock meshClock(1000, 0.25, 10000, 5000, 25);
```

---

### Public Methods

#### `void begin(bool registerCallback = true)`

Initializes the mesh clock synchronization. Must be called in `setup()` before using the clock.

**Parameters:**
- `registerCallback` (default: true): If true, registers internal ESP-NOW receive callback. Set to false if you want to manage ESP-NOW callbacks yourself (see [ESP-NOW Integration](#esp-now-integration)).

**Actions:**
- Sets WiFi to station mode
- Initializes ESP-NOW
- Registers receive callback (if `registerCallback` is true)
- Adds broadcast peer

**Example:**
```cpp
void setup() {
    Serial.begin(115200);
    meshClock.begin();  // Standard usage
}

// OR for custom ESP-NOW integration:
void setup() {
    meshClock.begin(false);  // Don't register callback
    esp_now_register_recv_cb(myCallback);  // Use your own
}
```

---

#### `void loop()`

Handles periodic broadcast of mesh time. Must be called frequently in main `loop()`.
It is a low priority task, since it only handles periodic broadcasting.

**Actions:**
- Checks if broadcast interval has elapsed
- Broadcasts current mesh time to all peers

**Example:**
```cpp
void loop() {
    meshClock.loop();  // Call this first
    // ... rest of your code ...
}
```

---

#### `uint64_t meshMicros()`

Returns the current mesh-synchronized time in microseconds.

**Returns:** 64-bit unsigned integer representing microseconds since mesh epoch.

**Example:**
```cpp
uint64_t now = meshClock.meshMicros();
Serial.printf("Mesh time: %llu µs\n", now);
```

---

#### `uint32_t meshMillis()`

Returns the current mesh-synchronized time in milliseconds.

**Returns:** 32-bit unsigned integer representing milliseconds since mesh epoch.

**Note:** This is derived from `meshMicros() / 1000`. For precision work, use `meshMicros()`.

**Example:**
```cpp
uint32_t now = meshClock.meshMillis();
Serial.printf("Mesh time: %u ms\n", now);
```

---

#### `SyncState getSyncState()`

Returns the current synchronization state of the mesh clock.

**Returns:** One of three `SyncState` enum values:
- `SyncState::ALONE` - No sync messages have been received yet (node is alone)
- `SyncState::SYNCED` - Currently synchronized with the mesh
- `SyncState::LOST` - Was previously synced, but no messages received within timeout period

**Example:**
```cpp
SyncState state = meshClock.getSyncState();

switch(state) {
    case SyncState::ALONE:
        Serial.println("Waiting for initial sync...");
        break;
    case SyncState::SYNCED:
        Serial.println("Synchronized!");
        break;
    case SyncState::LOST:
        Serial.println("Warning: Sync link lost!");
        break;
}
```

**Use Cases:**
- Display sync status on LED/display
- Trigger fallback behavior when link is lost
- Monitor mesh health in diagnostics
- Conditional logic based on sync reliability

---

#### `bool handleReceive(const uint8_t *mac, const uint8_t *data, int len)`

Manually process an ESP-NOW packet to check if it's a mesh clock packet. Use this when managing your own ESP-NOW callbacks.

**Packet Identification:**
- Checks for exactly 10 bytes
- Validates "MCK" magic header (0x4D, 0x43, 0x4B)
- Extracts 56-bit timestamp if valid

**Parameters:**
- `mac`: MAC address of sender
- `data`: Packet data
- `len`: Packet length

**Returns:** `true` if the packet was a valid mesh clock packet (processed), `false` otherwise

**Example:**
```cpp
void myESPNowCallback(const uint8_t *mac, const uint8_t *data, int len) {
    if (meshClock.handleReceive(mac, data, len)) {
        return;  // Was a clock packet (10 bytes with "MCK" header)
    }
    // Handle your own packets here
}
```

See [ESP-NOW Integration](#esp-now-integration) for complete examples.

---

#### `void setUserCallback(ESPNowRecvCallback callback)`

Register a callback to receive non-clock ESP-NOW packets. The library will automatically route 8-byte packets to the mesh clock and forward all other packets to your callback.

**Parameters:**
- `callback`: Function pointer with signature `void callback(const uint8_t *mac, const uint8_t *data, int len)`

**Example:**
```cpp
void myCallback(const uint8_t *mac, const uint8_t *data, int len) {
    // Only receives packets that are NOT mesh clock packets
    Serial.printf("Custom packet: %d bytes\n", len);
}

void setup() {
    meshClock.setUserCallback(myCallback);
    meshClock.begin();  // Auto-routing enabled
}
```

See [ESP-NOW Integration](#esp-now-integration) for complete examples.

---

## Examples

The library includes several example sketches to help you get started:

### BasicSync
**Location:** `examples/BasicSync/BasicSync.ino`

A simple introduction to the library showing:
- Basic initialization and setup
- Monitoring sync state changes
- Using mesh time for coordinated actions
- Serial output of sync status

### StateMonitoring
**Location:** `examples/StateMonitoring/StateMonitoring.ino`

Advanced state monitoring with visual feedback:
- LED indicators for each sync state
- Detailed statistics and diagnostics
- Custom configuration for faster sync
- Pretty-printed serial output with boxes and symbols

### SynchronizedLED
**Location:** `examples/SynchronizedLED/SynchronizedLED.ino`

Demonstrates perfect synchronization across multiple devices:
- Synchronized LED blinking patterns
- All devices in the mesh blink in perfect unison
- Shows how to use mesh time for coordinated animations
- Includes alternative pattern examples (breathing, pulses)

### CustomESPNowIntegration_Option1
**Location:** `examples/CustomESPNowIntegration_Option1/CustomESPNowIntegration_Option1.ino`

For projects that already use ESP-NOW - manual integration:
- You manage your own ESP-NOW callback
- Call `meshClock.handleReceive()` to process clock packets
- Full control over packet routing
- Perfect for complex ESP-NOW applications

### CustomESPNowIntegration_Option2
**Location:** `examples/CustomESPNowIntegration_Option2/CustomESPNowIntegration_Option2.ino`

For projects that already use ESP-NOW - automatic chaining:
- Register your callback with `meshClock.setUserCallback()`
- Library automatically routes packets
- 8-byte packets → mesh clock (handled internally)
- Other packets → your callback
- Simpler integration than Option 1

---

## ESP-NOW Integration

**Important:** ESP-NOW only supports a single receive callback. If your project already uses ESP-NOW, choose one of these integration methods:

### Option 1: Manual Receive Handling (Full Control)

```cpp
ESPNowMeshClock meshClock;

void myESPNowCallback(const uint8_t *mac, const uint8_t *data, int len) {
    // Let mesh clock process if it's a clock packet
    if (meshClock.handleReceive(mac, data, len)) {
        return;  // Was a clock packet, done
    }
    
    // Otherwise handle your own ESP-NOW messages
    // ... your code ...
}

void setup() {
    meshClock.begin(false);  // false = don't register own callback
    esp_now_register_recv_cb(myESPNowCallback);
}
```

### Option 2: Callback Chaining (Automatic Routing)

```cpp
ESPNowMeshClock meshClock;

void myCustomCallback(const uint8_t *mac, const uint8_t *data, int len) {
    // Only receives NON-clock packets (not 8 bytes)
    // ... handle your messages ...
}

void setup() {
    meshClock.setUserCallback(myCustomCallback);
    meshClock.begin();  // Registers callback with auto-chaining
}
```

**New API Methods:**
- `bool handleReceive(mac, data, len)` - Returns true if packet was a clock packet (10 bytes with "MCK" magic header)
- `void setUserCallback(callback)` - Set callback for non-clock packets
- `void begin(bool registerCallback = true)` - Optional callback registration

---

## Packet Format

Mesh clock packets are identified by a unique magic header to prevent conflicts with other ESP-NOW messages.

**Packet Structure (10 bytes total):**
```
Offset | Size | Description
-------|------|-------------
0-2    | 3    | Magic header: "MCK" (0x4D, 0x43, 0x4B)
3-9    | 7    | Timestamp: 56-bit microseconds (little-endian)
```

**Why 56-bit timestamp?**
- Rollover period: ~2,283 years (vs 584,000 years for 64-bit)
- Compact packet size: 10 bytes total
- More than sufficient for any practical application

**Magic Header "MCK":**
- Prevents misidentification of random 10-byte packets
- Allows multiple ESP-NOW protocols to coexist
- Future-proof for protocol versioning

Only packets matching this exact format will be processed as mesh clock packets. All other ESP-NOW packets will be ignored or forwarded to your custom callback (if using ESP-NOW integration).

---

## Implementation Details

- Each node broadcasts its mesh time every N ms (default: 1000ms ± 10% random variation)
- Broadcast packet: 10 bytes ("MCK" + 56-bit timestamp)
- Random variation prevents broadcast collisions in dense meshes
- On receive, any node forward-only slews its offset toward the most advanced clock (large steps only at first sync)
- Smoothing parameter (`slew_alpha`) ensures jumps are absorbed rather than causing AV/motion artifacts
- Sync timeout monitoring allows detection of lost connectivity

---

## Credits

- `ESPNowMeshClock` by Hemisphere-Project
- `libclock` by peufeu
