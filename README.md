# ESPNowMeshClock
ESP32 library for synchronizing clocks amongst many units using a mesh approach over ESP-NOW.

---

**Robust monotonic mesh time synchronization library for ESP32 over ESP-NOW radio.**  
Distributed, driverless 64-bit time sync—perfect for synchronized DMX, MIDI, MTC, media, lighting and show control mesh applications.. or whatever you want to do with wireless micro-seconds accuracy sync !

---

## Features

- Distributed (no single master): every node broadcasts its local mesh time, others align (forward-only, monotonic)
- **No rollover risk**: uses embedded [`libclock`](#libclock-by-peufeu-mit) with 64-bit hardware timer (`fastmicros64_isr`). It will actually rollover after.. 584 millennia.
- Slewed/smoothed time alignment: handles radio packet jitter and burst/delay gracefully
- **Collision avoidance**: randomized broadcast intervals prevent packet collisions in dense meshes
- **State monitoring**: query sync status (ALONE, SYNCED, LOST)
- **Configurable timeout**: detect and report lost mesh connectivity
- Robust across network loss/packet drop—self-healing mesh
- Simple API: just call `begin()` + `loop()`
- Plug-and-play with PlatformIO: drop into any project (`lib_deps`)

---

## Quickstart

### PlatformIO setup

Add this to your `platformio.ini`:

'''
lib_deps =
    https://github.com/YOURUSERNAME/ESPNowMeshClock.git
'''

### Example Sketch

'''
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
'''

---

## Why not micros()?

The Arduino `micros()` function on ESP32 is 32-bit and wraps every ~71min (*breaking* any forward-only mesh time sync algorithm after a single wrap)!  
By embedding [`libclock`](#libclock-by-peufeu-mit), this library defaults to `fastmicros64_isr()`, a true 64-bit hardware timer—**no rollover, maximum robustness.**

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

#### `void begin()`

Initializes the mesh clock synchronization. Must be called in `setup()` before using the clock.

**Actions:**
- Sets WiFi to station mode
- Initializes ESP-NOW
- Registers receive callback
- Adds broadcast peer

**Example:**
```cpp
void setup() {
    Serial.begin(115200);
    meshClock.begin();
}
```

---

#### `void loop()`

Handles periodic broadcast of mesh time. Must be called frequently in main `loop()`.

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

## Examples

The library includes several example sketches to help you get started:

### BasicSync
**Location:** `examples/BasicSync/BasicSync.ino`

A simple introduction to the library showing:
- Basic initialization and setup
- Monitoring sync state changes
- Using mesh time for coordinated actions
- Serial output of sync status

Perfect for understanding the fundamentals.

### StateMonitoring
**Location:** `examples/StateMonitoring/StateMonitoring.ino`

Advanced state monitoring with visual feedback:
- LED indicators for each sync state
- Detailed statistics and diagnostics
- Custom configuration for faster sync
- Pretty-printed serial output with boxes and symbols

Great for debugging and understanding mesh behavior.

### SynchronizedLED
**Location:** `examples/SynchronizedLED/SynchronizedLED.ino`

Demonstrates perfect synchronization across multiple devices:
- Synchronized LED blinking patterns
- All devices in the mesh blink in perfect unison
- Shows how to use mesh time for coordinated animations
- Includes alternative pattern examples (breathing, pulses)

Ideal for light shows, DMX control, and coordinated displays.

---

## Implementation Details

- Each node broadcasts its mesh time every N ms (default: 1000ms ± 10% random variation)
- Random variation prevents broadcast collisions in dense meshes
- On receive, any node forward-only slews its offset toward the most advanced clock (large steps only at first sync)
- Smoothing parameter (`slew_alpha`) ensures jumps are absorbed rather than causing AV/motion artifacts
- Sync timeout monitoring allows detection of lost connectivity

---

## Credits

- `ESPNowMeshClock` by Hemisphere-Project
- `libclock` by peufeu
