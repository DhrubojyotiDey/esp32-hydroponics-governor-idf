# 🌱 ESP32 Hydroponic Governor System

A fault-tolerant, real-time hydroponic monitoring and control system built on the ESP32.
Designed for reliability, modularity, and future automation.

---

## 🚀 Overview

This project implements a **multi-tasking ESP32-based hydroponic controller** capable of:

* Real-time sensor monitoring
* Web dashboard (WebSocket-based)
* Telnet logging interface
* OTA firmware updates
* Sensor health detection (fail-safe ready)

The system is built with a **producer → shared state → push architecture**, ensuring efficient and scalable data handling.

---

## 🧠 Architecture

```
[Sensors] → [Sensor Manager (Shared Tray)] → [Push Task]
                                      ↓
                         [WebSocket / Telnet / HTTP API]
```

### Key Concepts

* **Producers**: Sensor tasks update shared state
* **Sensor Tray**: Thread-safe global snapshot
* **Push Task**: Sends updates only when required
* **Viewers**:

  * Web UI (WebSocket)
  * Telnet client
  * HTTP `/data` endpoint

---

## 🔌 Features

### 📡 Real-Time Dashboard

* Web-based UI using WebSockets
* Live updates (temperature, humidity, flow rate)
* Auto-reconnect on disconnection
* Device reboot control from UI

---

### 🔁 Robust WebSocket Communication

* Heartbeat (ping) mechanism prevents idle disconnects
* Periodic fallback push ensures UI stays alive
* Automatic reconnection from frontend

---

### 🧪 Sensor Monitoring

#### Supported Sensors:

* DHT11 (Temperature & Humidity)
* YF-S201 (Water Flow)

#### Capabilities:

* Timestamp-based updates
* Data validation (NaN protection)
* Thread-safe access using mutex

---

### ❤️ Sensor Health Monitoring (NEW)

Detects sensor failures using timeout logic.

```json
{
  "temp": 25.1,
  "hum": 60.0,
  "flow": 1.25,
  "dht_alive": true,
  "flow_alive": true
}
```

#### How it works:

* Each sensor update stores a timestamp
* If no update within threshold → marked as "not alive"

#### Default Timeouts:

* DHT11: 10 seconds
* Flow sensor: 5 seconds

---

### 🌐 Connectivity Modes

#### 1. Station Mode (Normal Operation)

* Connects to saved WiFi
* Hosts:

  * Dashboard (`/`)
  * JSON API (`/data`)
  * WebSocket (`/ws`)
  * Telnet server (port 23)

#### 2. Access Point Mode (Provisioning)

* SSID: `ESP32_SETUP_AP`
* Captive portal for WiFi setup
* Auto-switches after credentials saved

---

### 📟 Telnet Debug Interface

* Live logs over network
* Single-client session control
* Useful for headless debugging

---

### 🔄 OTA Updates

* Firmware updates over WiFi
* No physical access required
* mDNS enabled (`hydroponics-esp32.local`)

---

## 📁 Project Structure

```
├── esp_hydroponic_goveror.ino   # Main application logic
├── sensor_manager.h             # Shared sensor state (tray)
├── ota_manager.h                # WiFi, OTA, Web, WebSocket
├── sensors.txt                  # Sensor references
├── Links,-esp32-datasheet.txt   # ESP32 docs
```

---

## ⚙️ Core Components

### Sensor Manager

* Centralized data structure
* Mutex-protected access
* Notifies push task on updates

---

### OTA Manager

* Handles:

  * WiFi connection
  * Web server
  * WebSocket
  * Telnet
  * OTA updates

---

## 🔐 Reliability Features

* ✅ Mutex-protected shared state
* ✅ Event-driven updates (no polling)
* ✅ Sensor timeout detection
* ✅ WebSocket auto-recovery
* ✅ WiFi reconnect handling (optional)

---

## 🧪 API Endpoints

### `GET /data`

Returns latest sensor snapshot:

```json
{
  "temp": 25.1,
  "hum": 60.0,
  "flow": 1.25,
  "dht_alive": true,
  "flow_alive": true
}
```

---

## 🖥️ Web UI

Accessible via:

```
http://<device-ip>/
```

Displays:

* Temperature
* Humidity
* Water flow
* Live connection status

---

## ⚠️ Known Limitations

* No alerting system yet (planned)
* Single WebSocket broadcast channel
* No persistent logging
* Sensor calibration not implemented

---

## 🔮 Future Enhancements

* 🔔 Alert system (buzzer / Telegram / Pi integration)
* 🧠 Sensor state machine (OK / TIMEOUT / ERROR)
* 📊 Data logging (SD card / cloud)
* ⚙️ Automated nutrient dosing control
* 🌡️ Additional sensors (pH, EC, water level)

---

## 🛠️ Setup Instructions

1. Flash firmware to ESP32
2. On first boot:

   * Connect to `ESP32_SETUP_AP`
   * Open `http://192.168.4.1`
   * Enter WiFi credentials
3. Access dashboard via assigned IP

---

## 🧑‍💻 Development Notes

* Built using:

  * ESPAsyncWebServer
  * AsyncTCP
  * ArduinoOTA
  * FreeRTOS (ESP32 native)

* Event-driven design — avoids unnecessary CPU usage

---

## 📜 License

MIT License (or specify your preference)

---

## 🙌 Acknowledgements

* Espressif Systems (ESP32 platform)
* Open-source async networking libraries

---

## 💡 Final Note

This project is evolving into a **self-governing hydroponic system**.
The current version focuses on **observability, reliability, and fault detection** — forming the foundation for full automation.

---
