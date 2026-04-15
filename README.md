# ESP32 Hydroponics Governor — Upload Guide
## VS Code + PlatformIO + Native ESP-IDF

---

## Prerequisites

Install these in order before touching the project.

### 1. VS Code
Download from https://code.visualstudio.com  
Install normally. No special settings needed at install time.

### 2. PlatformIO IDE Extension
1. Open VS Code
2. Click the **Extensions** icon in the left sidebar (or `Ctrl+Shift+X`)
3. Search: `PlatformIO IDE`
4. Click **Install** on the result by *PlatformIO*
5. Wait — it downloads the PlatformIO Core toolchain (~200MB)
6. When prompted, **Restart VS Code**

> PlatformIO will automatically download the ESP-IDF toolchain
> (xtensa-esp32-elf-gcc, openocd, esptool) on first build.
> This takes 3–10 minutes on first run only.

### 3. USB Driver (Windows only)
The ESP32-WROOM-32 uses a CP2102 or CH340 USB-to-UART chip.

- **CP2102**: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers
- **CH340**: https://www.wch-ic.com/downloads/CH341SER_EXE.html

Check Device Manager → Ports (COM & LPT) after plugging in the board.
You should see a COM port appear (e.g. `COM3`, `COM7`).

On **macOS/Linux**, no driver needed — the port appears as
`/dev/ttyUSB0` or `/dev/tty.SLAB_USBtoUART` automatically.

---

## Opening the Project

```
File → Open Folder → select the hydro-idf folder
```

PlatformIO detects `platformio.ini` automatically.
The blue status bar at the bottom confirms it is active.

---

## Project Structure

```
hydro-idf/
├── platformio.ini          ← build config (framework, board, libraries)
├── sdkconfig.defaults      ← ESP-IDF Kconfig overrides (WebSocket, mDNS, etc.)
├── partitions.csv          ← 4MB OTA partition layout
└── src/
    ├── main.c              ← app_main() + all FreeRTOS tasks
    ├── app_config.h        ← ALL pin defs, timeouts, sizes in one place
    ├── sensor_manager.c/h  ← sensor tray, health engine, log queue
    ├── dht_driver.c/h      ← native GPIO DHT11 bit-bang driver
    ├── wifi_manager.c/h    ← WiFi state machine (AP/STA, NVS, mDNS)
    ├── web_server.c/h      ← HTTP + WebSocket + OTA handlers
    ├── dns_server.c/h      ← UDP DNS responder for captive portal
    ├── led_manager.c/h     ← LED heartbeat task
    ├── page_setup.html     ← captive portal UI (embedded into firmware)
    └── page_dash.html      ← live dashboard UI (embedded into firmware)
```

The two HTML files are compiled **into the firmware binary** by PlatformIO
via `board_build.embed_txtfiles`. They are NOT uploaded separately — they
live in flash as read-only data sections. No SPIFFS upload needed.

---

## First Build

### Method A — VS Code UI (recommended)

Click the **✓ Build** button in the PlatformIO toolbar at the bottom of VS Code.

Or press: `Ctrl+Alt+B`

First build will:
1. Download ESP-IDF 5.1.x toolchain (~500MB, once only)
2. Generate `sdkconfig` from `sdkconfig.defaults`
3. Compile all source files
4. Link the firmware
5. Output: `.pio/build/esp32dev/firmware.bin`

Expected build time: 2–4 minutes (first), 15–30s (incremental).

### Method B — PlatformIO Terminal

```
Ctrl+` to open terminal, then:
pio run
```

---

## Uploading Firmware

### 1. Connect the ESP32
Plug the board in via USB. Confirm the port:
- **Windows**: Device Manager → Ports → `COMx`
- **macOS**: `ls /dev/tty.*` → look for `SLAB_USBtoUART` or `usbserial`
- **Linux**: `ls /dev/ttyUSB*` → usually `ttyUSB0`

### 2. Upload

Click **→ Upload** in the PlatformIO toolbar.  
Or press: `Ctrl+Alt+U`

Or via terminal:
```
pio run --target upload
```

PlatformIO auto-detects the COM port. If it picks the wrong one, set it in `platformio.ini`:
```ini
upload_port = COM7          ; Windows example
upload_port = /dev/ttyUSB0  ; Linux example
```

### 3. Boot mode (if upload fails)

Some ESP32 boards require manual boot mode entry:
1. Hold the **BOOT** button on the board
2. Press and release **EN** (reset)
3. Release **BOOT**
4. Run upload immediately

After upload completes, press **EN** once to start the firmware normally.

---

## Serial Monitor

Click **🔌 Monitor** in the PlatformIO toolbar.  
Or: `Ctrl+Alt+S`

Or:
```
pio device monitor
```

Expected boot output:
```
I (xxx) MAIN: ══════════════════════════════════════
I (xxx) MAIN:  ESP32 Hydroponics Governor — boot
I (xxx) MAIN: ══════════════════════════════════════
I (xxx) SENSOR: Initialised
I (xxx) SENSOR: Registered sensor 'dht'  timeout=10000ms
I (xxx) SENSOR: Registered sensor 'flow' timeout=5000ms
I (xxx) WIFI: No credentials — starting provisioning AP
I (xxx) WIFI: AP started — SSID: 'Hydroponics_Setup'  IP: 192.168.4.1
I (xxx) DNS:  Listening on UDP port 53
I (xxx) WEB:  HTTP server started on port 80
I (xxx) MAIN: All tasks started
```

---

## OTA (Over-the-Air) Updates

Once the device is on your WiFi, you never need the USB cable again.

### Method 1 — Dashboard Upload
1. Visit `http://hydroponics.local` in your browser
2. Click **OTA Update**
3. Select the `.bin` file from `.pio/build/esp32dev/firmware.bin`
4. Click **Upload & Flash**

### Method 2 — PlatformIO OTA via network
Add to `platformio.ini`:
```ini
upload_protocol = espota
upload_port     = hydroponics.local   ; or the device IP
```
Then `Ctrl+Alt+U` uploads over WiFi.

---

## Provisioning Flow (first boot)

1. Power on → device starts `Hydroponics_Setup` WiFi AP
2. Connect your phone to `Hydroponics_Setup` (no password)
3. Android/iOS shows "Sign in to network" → tap it → captive portal opens
4. If portal does not auto-open: browse to `http://192.168.4.1`
5. Tap **↻** to scan networks → select yours → enter password → **CONNECT**
6. Wait for the 6-stage progress bar to complete
7. Button shows `Copy  http://hydroponics.local` — tap to copy
8. Reconnect your phone to your home WiFi
9. Visit `http://hydroponics.local` (or the IP shown)
10. Live dashboard appears. AP shuts down automatically.

Credentials are stored in NVS flash. On every subsequent boot, the
device connects directly to your WiFi in ~3 seconds — no AP mode.

---

## Clearing Credentials (factory reset)

Option A — Serial monitor: the device auto-clears and reboots if
WiFi connection fails 3 times (wrong password scenario).

Option B — Add a reset endpoint or hold a GPIO button (future feature).

Option C — Erase flash entirely:
```
pio run --target erase
```
Then re-upload firmware.

---

## Common Errors

| Symptom | Cause | Fix |
|---|---|---|
| `No module named esptool` | PlatformIO not installed | Reinstall PlatformIO extension, restart VS Code |
| `A fatal error occurred: Failed to connect` | Wrong boot mode | Hold BOOT, press EN, release BOOT, then upload |
| `error: port is not open` | Wrong COM port | Set `upload_port` in platformio.ini |
| `WIFI: Direct STA failed — clearing credentials` | Stored SSID/password wrong | Device auto-reboots into AP mode for re-provisioning |
| Dashboard shows `--` for all values | DHT/Flow not connected | Check GPIO14 (DHT) and GPIO27 (Flow) wiring |
| `hydroponics.local` not resolving on Android | mDNS | Use IP address directly; Android mDNS is unreliable on some devices |

---

## Changing Pin Assignments

All pins are defined in one place: `src/app_config.h`

```c
#define DHT_GPIO   GPIO_NUM_14   // change here
#define FLOW_GPIO  GPIO_NUM_27   // change here
#define LED_GPIO   GPIO_NUM_2    // change here
```

Rebuild and re-upload after any change.
