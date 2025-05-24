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
![mob1 (1)](https://github.com/user-attachments/assets/08725c1b-8876-4d66-918c-63dd39258cae)
![mob1 (2)](https://github.com/user-attachments/assets/b6df1e93-8a0d-418d-bd99-d8c24f12ffd0)
![mob1 (3)](https://github.com/user-attachments/assets/de4d3157-9758-4d6f-a0f3-ecbe5ae8617e)
![mob1 (4)](https://github.com/user-attachments/assets/fd0c3390-63f7-417a-b52c-57816781a8ca)



