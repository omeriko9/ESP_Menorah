# ESP32 Menorah Project

https://github.com/user-attachments/assets/7ba0647f-31a6-4da7-8345-d67b56a75962

This project simulates a Menorah using an ESP32. It features:
- **9 LEDs**: 1 Shamash + 8 Candles.
- **Flicker Effect**: Uses PWM (LEDC) to simulate candle fire.
- **State Persistence**: Remembers the current day across power cycles using NVS.
- **Button Control**: Uses the onboard BOOT button to cycle through the days.

## Hardware Setup

### Board
- ESP32 WROOM-32 DevKit (or similar).

### Wiring
Connect LEDs (with appropriate resistors, e.g., 220Ω or 330Ω) to the following GPIO pins:

| LED | GPIO Pin | Note |
|---|---|---|
| **Shamash** | **GPIO 21** | Always on |
| Candle 1 | GPIO 22 | |
| Candle 2 | GPIO 23 | |
| Candle 3 | GPIO 19 | |
| Candle 4 | GPIO 18 | |
| Candle 5 | GPIO 5 | |
| Candle 6 | GPIO 17 | |
| Candle 7 | GPIO 16 | |
| Candle 8 | GPIO 4 | |

**Button**: The project uses the onboard **BOOT** button (GPIO 0). No external button is required.

## Usage
1.  **Power On**: The Menorah will light up the Shamash and the number of candles corresponding to the last saved day (defaults to 1).
2.  **Change Day**: Press the **BOOT** button to increment the day (1 -> 2 -> ... -> 8 -> 1).
3.  **Reset**: The state is saved automatically. If you unplug the ESP32 and plug it back in, it will resume the correct day.

## Building and Flashing

1.  Set up the ESP-IDF environment.
2.  Build the project:
    ```bash
    idf.py build
    ```
3.  Flash to the ESP32:
    ```bash
    idf.py -p COMx flash monitor
    ```
    (Replace `COMx` with your ESP32's serial port).

## Notes
- The "En" button on the ESP32 is a hardware reset button and cannot be used for software input without resetting the board. Therefore, the "Boot" button (GPIO 0) is used instead.
