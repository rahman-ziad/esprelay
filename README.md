# ESP32 Relay Control Project

This project allows you to control home appliances using an ESP32 microcontroller, both locally via buttons and remotely through Firebase. In case Wi-Fi is unavailable, you can still operate the devices using physical buttons, with the state saved locally in EEPROM and synced to Firebase when the network reconnects.
## Features
- Local Control: Use buttons to toggle devices.
- Remote Control: Sync device states with Firebase and control from anywhere.
- Wi-Fi Auto-Connect: Uses WiFiManager to connect to Wi-Fi or set up an AP for configuration.
- State Persistence: Saves device states in EEPROM for recovery after reboot or power loss.


## Folder Structure
Esp32_relay/ <br>
├── Esp32_relay.ino # Main entry point <br>
└── README.md # Project documentation<br>

## Hardware Requirements
- ESP32 microcontroller
- 4-channel relay module
- 4 push buttons/ touch switches/ mechanical switches
- Connecting wires
- Appliances (e.g., lights, fans, etc.)

## Pin Configuration
|  Device	 | GPIO Pin | Button Pin |
|----------|----------|------------|
| Relay 1  | 2        | 4          |
| Relay 2  | 14       | 5          |
| Relay 3  | 16       | 12         |
| Relay 4  | 17       | 13         |

## Software Requirements
1. Arduino IDE
2. ESP32 Core installed on Arduino IDE
3. Libraries: <br>
    -FirebaseESP32<br>
    -WiFiManager<br>
    -EEPROM<br>
## Installation & Setup
1. Clone the repository or download the ZIP file.
2. Open the project in Arduino IDE.
3. Install required libraries through Library Manager.
4. Update Firebase credentials 
5. Upload the code to your ESP32.

## How It Works
1. On Startup:<br> - Restores device states from EEPROM. <br> - Attempts to connect to Wi-Fi. <br> - Syncs states with Firebase if Wi-Fi is available.
2. Button Control: <br> - Press a button to toggle the corresponding device state.<br> - State is saved to EEPROM and synced with Firebase (if connected).
3. Remote Control via Firebase: <br> -Control devices remotely using Firebase. <br> - Device states are synced periodically in the loop.

## Usage
- **Local Control:** Use buttons to toggle devices.
- **Remote Control:** Use Firebase to control devices remotely.
- **Offline Mode:** Devices remain operational even without Wi-Fi.

## Troubleshooting
- If Wi-Fi doesn't connect, the ESP32 will start in AP mode.
- Verify Firebase credentials if syncing fails.

## License
This project is licensed under the MIT License.

## Credits
Thanks to the open-source community for the FirebaseESP32 and WiFiManager libraries.
