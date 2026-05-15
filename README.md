# ESP32 Handheld Retro Console - Phase 1: Pac-Man

This project is the first phase of a customizable handheld console developed using the ESP32 microcontroller. Currently, core Pac-Man mechanics, LCD screen integration, and keypad controls are fully functional.



Features (Phase 1)
- **Hardware:** ESP32 (30-pin), 1.8" ST7735 TFT LCD, 4x4 Keypad.
- **Software:** C++ / Arduino IDE.
- **Graphics:** Optimized BGR color palette using the Adafruit GFX library.
- **Controls:** Directional movement managed via the 2-5-7-10 keys on the keypad.

Hardware Setup
| Component | ESP32 Pin |
| :--- | :--- |
| TFT CS | GPIO 23 |
| TFT DC | GPIO  2|
| TFT RST | GPIO 4 |

Roadmap
- [ ] Implement game loading from SD Card module .
- [ ] Integration of Li-po battery and charging circuit.
- [ ] Custom 3D-printed portable enclosure design.
- [ ] Sound effect support via buzzer or I2S amplifier.

Installation
1. Add ESP32 board support to your Arduino IDE.
2. Install the `Adafruit_GFX`, `Adafruit_ST7735`, and `Keypad` libraries.
3. Upload the code and step into the world of retro gaming!

---
**Developer:** [BATIKAN]  
**Project Status:** In Development (Step 1)
