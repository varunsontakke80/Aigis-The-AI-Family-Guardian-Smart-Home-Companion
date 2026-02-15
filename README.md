# Aigis - Smart Home Assistant

Aigis is a comprehensive Smart Home Assistant built on the **ESP32-S3-BOX-3**, designed to act as a central hub for controlling various smart nodes (Lights, Fans, Door Security, Health Monitoring, and Robot movement). It features a rich touch interface and offline speech recognition.

## Overview

This project transforms the ESP32-S3-BOX-3 into an intelligent control center that communicates with other ESP32/ESP8266 nodes via **ESP-NOW**. It supports:
- **Voice Control**: Offline command recognition for home automation.
- **Security**: Integration with a Door Node (ESP32-CAM) for person detection and remote locking/unlocking.
- **Health Monitoring**: Integration with a Health Node (Seeed Studio XIAO ESP32C3) for fall detection and vitals monitoring.
- **Entertainment**: Interactive "Dance" and "Story" modes.
- **Visual Feedback**: A responsive LVGL-based UI showing status, alerts, and animations.

## Hardware Requirements

### Central Hub
- **ESP32-S3-BOX-3** development board

### Peripheral Nodes (Connected via ESP-NOW)
1.  **Door Node**:
    -   Hardware: **ESP32-CAM**
    -   Function: Person detection (TinyML), Door Lock/Unlock control.
2.  **Health Node**:
    -   Hardware: **Seeed Studio XIAO ESP32C3** (or similar)
    -   Function: Fall detection, Heart rate/SpO2 monitoring.
3.  **Control Node**:
    -   Hardware: **NodeMCU / ESP8266**
    -   Function: Controls Lights, Fans, and Sockets.
4.  **Robot Node**:
    -   Function: Movement control (Walk forward, Stop).

## Features

-   ✅ **Offline Speech Recognition**: Supports natural language commands without internet.
-   ✅ **Smart Home Control**:
    -   Lights (On/Off)
    -   Sockets (On/Off)
    -   Fan Speed Control (Level 1, 2, 3, Off)
-   ✅ **Security & Safety**:
    -   **Door Lock/Unlock**: Voice commands to secure the entrance.
    -   **Visitor Recognition**: Displays the name of the person detected at the door (received from Door Node).
    -   **Fall Detection**: Immediate alert on the screen when a fall is detected by the Health Node.
-   ✅ **Interactive Modes**:
    -   **"Let's Dance"**: Plays music and shows a dancing animation.
    -   **"Tell a Story"**: Plays an audio story with accompanying visuals.
-   ✅ **ESP-NOW Communication**: Low-latency, peer-to-peer communication with all peripheral nodes.

## Project Structure

```
Aigis_Project/
├── main/
│   ├── app/
│   │   ├── app_sr.c/h            # Speech Recognition command definitions
│   │   ├── app_espnow.c/h        # ESP-NOW comms with Door, Health, Control nodes
│   │   ├── app_audio.c/h         # Audio playback (MP3/WAV)
│   │   └── app_fall_monitor.c    # Logic for handling fall detection alerts
│   ├── gui/
│   │   ├── main_ui.c/h           # Main dashboard UI
│   │   ├── door_ui.c/h           # Door security interface (Visitor display)
│   │   ├── dance_ui.c/h          # Dance mode animation
│   │   ├── story_ui.c/h          # Story mode interface
│   │   └── fall_ui.c/h           # Fall detection alert screen
│   └── main.c                    # Application entry point
├── spiffs/                       # Filesystem for Audio Assets
│   ├── mp3/                      # Story, Dance, and Door alert audio files
│   └── echo_en_*.wav             # Voice command feedback tones
└── CMakeLists.txt
```

## Voice Commands

Wake Word: **"Hi ESP"** (or configured wake word)

### Home Automation
| Command | Action |
| :--- | :--- |
| "Turn on light one" | Turns ON Learning Light |
| "Turn off light one" | Turns OFF Learning Light |
| "Turn on socket" | Turns ON Smart Socket |
| "Turn off socket" | Turns OFF Smart Socket |
| "Turn on fan at level one" | Sets Fan to Low Speed |
| "Turn on fan at level two" | Sets Fan to Medium Speed |
| "Turn on fan at level three" | Sets Fan to High Speed |
| "Turn off fan" | Turns Fan OFF |

### Security & Health
| Command | Action |
| :--- | :--- |
| "Lock the door" | Sends lock command to Door Node |
| "Unlock the door" | Sends unlock command to Door Node |
| "Check health" | Requests status from Health Node |

### Entertainment & Robot
| Command | Action |
| :--- | :--- |
| "Lets dance" | Starts music and dance animation on screen |
| "Tell a story" | Plays a short story with visuals |
| "Walk forward" | Commands Robot Node to move forward |
| "Stop" | Commands Robot Node to stop |

## Setup & Configuration

1.  **Flash the Aigis Firmware**:
    ```bash
    idf.py set-target esp32s3
    idf.py build
    idf.py flash monitor
    ```

2.  **Configure MAC Addresses**:
    The ESP-NOW peers are defined in `main/app/app_espnow.c`. You must update the following arrays with the MAC addresses of your actual hardware nodes:
    ```c
    static uint8_t remote_mac_control[] = { ... }; // Your NodeMCU MAC
    static const uint8_t remote_mac_health[] = { ... }; // Your Xiao C3 MAC
    static const uint8_t remote_mac_door[] = { ... }; // Your ESP32-CAM MAC
    ```

3.  **Audio Files**:
    Ensure the `spiffs` partition is populated with the required MP3/WAV files for audio feedback and entertainment modes.

## Troubleshooting

-   **ESP-NOW Failures**: Ensure all nodes are on the same WiFi channel (Default: Channel 11). Check `app_espnow.c` initialization.
-   **Audio Issues**: Verify volume settings and `spiffs` partition mounting in `main.c`.
-   **Touchscreen**: If UI is unresponsive, check `bsp_display_start_with_config` settings in `main.c`.

## License

SPDX-License-Identifier: Unlicense OR CC0-1.0
Copyright (c) 2024 Espressif Systems (Shanghai) CO LTD
