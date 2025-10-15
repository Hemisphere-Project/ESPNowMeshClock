# ESPNowMeshClock Examples

This folder contains example sketches demonstrating various features of the ESPNowMeshClock library.

## Available Examples

### 1. BasicSync
**File:** `BasicSync/BasicSync.ino`  
**Difficulty:** Beginner

The simplest example to get started with ESPNowMeshClock.

**What you'll learn:**
- How to initialize the mesh clock
- Monitoring sync state changes
- Using `meshMicros()` and `meshMillis()` for synchronized timing
- Basic serial output and debugging

**Hardware:** Any ESP32 board

---

### 2. StateMonitoring
**File:** `StateMonitoring/StateMonitoring.ino`  
**Difficulty:** Intermediate

Advanced monitoring with visual feedback using the built-in LED.

**What you'll learn:**
- LED patterns for different sync states
- Custom configuration parameters
- Statistics and diagnostics
- Detecting and responding to link loss

**Hardware:** 
- ESP32 board with built-in LED
- Modify `LED_PIN` if using external LED

**LED Patterns:**
- Fast blink (100ms): Waiting for initial sync (ALONE)
- Solid ON: Successfully synchronized (SYNCED)
- Slow blink (500ms): Link lost warning (LOST)

---

### 3. SynchronizedLED
**File:** `SynchronizedLED/SynchronizedLED.ino`  
**Difficulty:** Intermediate

Demonstrates perfect synchronization across multiple ESP32 devices.

**What you'll learn:**
- Creating synchronized patterns using mesh time
- Coordinating multiple devices in perfect unison
- Time-based animations and effects
- Practical applications for light shows

**Hardware:**
- 2 or more ESP32 boards
- Built-in LED or external LED on each

**Demo:**
Upload this sketch to multiple ESP32s and watch them all blink in perfect synchronization!

---

### 4. CustomESPNowIntegration_Option1
**File:** `CustomESPNowIntegration_Option1/CustomESPNowIntegration_Option1.ino`  
**Difficulty:** Advanced

Manual ESP-NOW integration for full control.

**What you'll learn:**
- Integrating mesh clock into existing ESP-NOW projects
- Managing your own ESP-NOW callback
- Using `handleReceive()` to route packets
- Sending both clock and custom messages

**When to use:**
- Your project already uses ESP-NOW
- You need full control over packet routing
- Complex multi-protocol applications

---

### 5. CustomESPNowIntegration_Option2
**File:** `CustomESPNowIntegration_Option2/CustomESPNowIntegration_Option2.ino`  
**Difficulty:** Advanced

Automatic callback chaining for simpler ESP-NOW integration.

**What you'll learn:**
- Using `setUserCallback()` for automatic routing
- Letting the library handle packet discrimination
- Simpler integration than Option 1

**When to use:**
- Your project already uses ESP-NOW
- You want automatic packet routing
- Prefer simpler integration code

---

## How to Use These Examples

### Arduino IDE
1. Open Arduino IDE
2. Go to File → Examples → ESPNowMeshClock
3. Select the example you want to try
4. Select your ESP32 board from Tools → Board
5. Upload to your ESP32

### PlatformIO
1. Copy the example `.ino` file to your `src/` folder (rename to `main.cpp`)
2. Or reference the example in your `platformio.ini`:
```ini
[env:esp32]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps = 
    https://github.com/Hemisphere-Project/ESPNowMeshClock.git
```

---

## Testing with Multiple Devices

For the best demonstration of mesh synchronization:

1. **Upload to multiple ESP32s:** Use the same sketch on 2-5 devices
2. **Power them on:** They'll automatically find each other via ESP-NOW
3. **Watch the sync:** Monitor serial output to see them synchronize
4. **Observe coordination:** LEDs or timed actions will be perfectly aligned

---

## Troubleshooting

**"ALONE" state persists:**
- Make sure multiple devices are running
- Check that WiFi is not in AP mode on any device
- Verify ESP-NOW is supported on your ESP32 variant

**"LOST" frequently:**
- Increase `sync_timeout_ms` parameter
- Reduce distance between devices
- Check for WiFi interference

**Devices not syncing:**
- Ensure all devices are using the same library version
- Check serial monitor for error messages
- Verify ESP-NOW initialization succeeded

---

## Next Steps

After trying these examples:
- Modify parameters to see how they affect sync behavior
- Create your own synchronized applications (DMX, MIDI, animations)
- Combine with other libraries for advanced projects
- Share your creations !

---

## Support

- GitHub Issues: [Report bugs or ask questions](https://github.com/Hemisphere-Project/ESPNowMeshClock/issues)
- Documentation: See main [README.md](../README.md)
