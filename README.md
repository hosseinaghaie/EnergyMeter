# EnergyMeter
Local energy meter with esp32s3 and PZEM-004T-100A

Power Meter Dashboard
A real-time power monitoring system using ESP32-S3 and PZEM-004T module to measure electrical parameters including voltage, current, power, energy, and power factor.
Features
Dual Network Modes: Works in both Access Point (AP) and Client mode
Real-time Monitoring: View voltage, current, active/reactive/apparent power, energy consumption, power factor, and frequency
Dynamic Charts: Interactive multi-axis charts with adjustable time periods (1m, 5m, 15m, 1h)
Configurable Settings: Adjust network settings, sample rates, voltage/current thresholds, and more
Energy Reset: Ability to reset the energy counter
Responsive Design: Works on desktop and mobile devices
Hardware Requirements
ESP32-S3 (or compatible ESP32 board)
PZEM-004T v3 power monitoring module
Power supply
Setup
Flash the firmware to your ESP32-S3
On first boot, connect to the device's AP network (default SSID: "PowerMeter")
Access the dashboard at http://192.168.4.1
Configure network settings to connect to your home WiFi (optional)
After configuration, the device can be accessed via your local network
Configuration
The system provides configuration options for:
Network settings (AP/Client modes, IP settings)
User authentication
Sensor parameters (sample rate, thresholds)
Time settings

This project provides an affordable and customizable solution for monitoring power consumption in your home or office.

Screenshots:
![Alt text] https://github.com/hosseinaghaie/EnergyMeter/blob/main/Doc/screenshot/mob1%20(1).jpg
https://github.com/hosseinaghaie/EnergyMeter/blob/main/Doc/screenshot/mob1%20(2).jpg
https://github.com/hosseinaghaie/EnergyMeter/blob/main/Doc/screenshot/mob1%20(3).jpg
https://github.com/hosseinaghaie/EnergyMeter/blob/main/Doc/screenshot/mob1%20(4).jpg
