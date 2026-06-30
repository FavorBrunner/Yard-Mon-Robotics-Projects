# Yard Mon Robotics — Project Code Archive

All source code for projects built by Favor, organized by project.
Each folder contains the best/most complete version of that project's code.

## Projects

| Folder | Project | Platform | Description |
|--------|---------|----------|-------------|
| `precipitate-smart-lamp/` | Precipitate | ESP32 | Weather-reactive RGB lamp with self-hosted Wi-Fi portal, live Open-Meteo data, and ambient light effects for rain, snow, thunder, and temperature gradients. |
| `annes-passwords-vault/` | Anne's Passwords | ESP32 | Offline password vault secured by RFID key card, navigated by joystick + LCD, with Wi-Fi captive portal for managing entries. |
| `smart-gas-mask/` | Smart Gas Mask | ESP32 | Wearable air quality monitor using MQ-2 sensor with auto-calibration, breathing green LED in safe mode, and red blink + buzzer alarm on gas detection. |
| `311-brooklyn-scanner/` | NYC 311 Live Scanner | ESP32 | Real-time public data terminal pulling Brooklyn 311 service requests and displaying them on an LCD with buzzer alerts for new incidents. |
| `traffic-light/` | Traffic Light | Arduino | Foundational timed-state LED control system demonstrating sequential logic. |
| `color-sensor-blind/` | Color Sensor for the Blind | Arduino | Accessibility device using TCS3200 sensor to identify and announce colors via Serial output. |
| `mini-hoop-arcade/` | Mini Hoop Arcade Game | Arduino | Basketball arcade game with MAX7219 LED matrix score display, TM1637 timer, IR hoop sensor, and EEPROM high score persistence. |
| `heart-rate-monitor/` | Heart Rate Monitor | Arduino | Pulse monitor with PulseSensor, U8g2 OLED display, and adaptive-duration buzzer beeps synced to heartbeat. First TikTok build. |
| `fire-extinguisher/` | Fire Extinguisher | Arduino | Flame-detecting servo that automatically actuates when fire is sensed. |
| `esp32-smartwatch/` | Interview Smartwatch | ESP32 | Round-display (GC9A01) smartwatch with scrolling bio text and animated JPEG splash, built for a professional interview. |
| `proximity-alarm/` | Proximity Security Alarm | ESP32 | Ultrasonic distance alarm with web dashboard (mDNS), IFTTT push notifications, and remote silence/continue controls. |
| `face-tracking-turret/` | Face-Tracking Turret | Arduino + Python | OpenCV face detection driving Arduino servos via pyFirmata, with zone-based X-axis control, laser targeting, urgency-scaled buzzer, and auto-fire on stable lock. Built with CrunchLabs partnership. |

## Tech Stack

Arduino, ESP32, Raspberry Pi, Python, C/C++, OpenCV, pyFirmata, ArduinoJson, Blynk IoT, Open-Meteo API, IFTTT, LittleFS, MFRC522, various sensors (MQ-2, TCS3200, HC-SR04, PulseSensor, IR, flame)

## About

Built in Brooklyn, NY — everything on this list was made in a bedroom lab.
Follow the journey: [TikTok @YardMonRobotics](https://www.tiktok.com/@yardmonrobotics)
