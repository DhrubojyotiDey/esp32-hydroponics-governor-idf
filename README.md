# 🌱 ESP32 Hydroponic Governor

I believe this is an engineering & research project to build a self-regulating hydroponic system using ESP32 and Raspberry Pi 4. The theory is to push hardware and software to their limits, iterating continuously to expose strengths and weaknesses, while achieving a “leave-it-alone” growing system.

---

## 🚀 Project Goal

To design a hydroponic tower that can:

* Operate autonomously with minimal human intervention
* Maintain plant health using sensor-driven decisions
* Detect anomalies and respond intelligently
* Require human input only for:

  * Refilling water / nutrients
  * Harvesting

This is not just automation — it is an attempt to build a **self-governing system**.

---

## 🧠 System Architecture

### 🔹 ESP32 (Control Layer)

* Real-time sensor monitoring
* Pump/misting control (non-blocking logic)
* Local web dashboard (HTTP)
* OTA updates + captive portal provisioning
* First-level anomaly detection

### 🔹 Raspberry Pi 4 (Cognitive Layer)

* Data aggregation and logging
* AI-assisted reasoning (only when needed)
* Telegram-based interaction
* System summaries and alerts

---

## 🔬 Sensor Stack

* Light intensity (BH1750)
* Water flow (YF-S201)
* TDS (nutrient concentration)
* Water temperature (DS18B20)
* Pump current (ACS712)
* Ambient temp/humidity (DHT11)

---

## ⚙️ Core Features (Planned & In Progress)

### 🌊 Intelligent Irrigation

* Timed misting cycles
* Flow + current correlation for diagnostics
* Dry-run and blockage detection

### 🧪 Nutrient Feedback Loop

* TDS-based nutrient control
* Temperature compensation
* Iterative dosing and verification

### 🧠 Sensor Fusion

* Combine multiple sensor readings
* Detect system imbalance (not just raw values)
* Build a “health score” instead of relying on single metrics

### 📊 Local Dashboard

* ESP32-hosted webpage
* Real-time graphs:

  * Pump activity
  * Flow rate
  * TDS trends
  * Light levels

### 🔄 OTA & Provisioning

* Wireless firmware updates
* Captive portal for WiFi setup
* Telnet logging for debugging

---

## 🤖 AI Philosophy

AI is **not the primary controller**.

It is used only when:

* Sensor data shows anomalies
* The system reaches conditions outside predefined logic

The goal is:

> Deterministic control first, AI as fallback reasoning.

---

## 🔁 Data Flow (Simplified)

```text
Sensors → ESP32 → (Local decisions)
                 → Raspberry Pi → (AI / logging / alerts)
```

---

## 🧩 Design Principles

* Non-blocking embedded architecture
* Edge-first (works without internet)
* Fail-safe operation
* Minimal human dependency
* Iterative improvement through real-world testing

---

## 📌 Current Status

* [x] ESP32 base firmware (sensors + OTA + web server)
* [x] WiFi provisioning system
* [ ] Pump + flow + current correlation logic
* [ ] Nutrient feedback loop
* [ ] Dashboard graph system
* [ ] Raspberry Pi integration
* [ ] AI anomaly handling

---

## 🧭 Future Scope

* Multi-tower scalability
* Long-term data logging
* Predictive maintenance
* Vision-based plant monitoring (optional)
* Fully closed-loop nutrient system

---

## 🧠 Final Note

This project is a continuous experiment.

The intention is not just to build a working system, but to:

* Understand limits
* Break assumptions
* Refine architecture through iteration

> The system should eventually require attention only when absolutely necessary.
