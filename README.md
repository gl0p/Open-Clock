# Open Source Alarm Clock

### About

This open-source alarm clock project combines smart features with a fully customizable interface. Designed to be user-friendly and extendable, it offers features like automatic brightness adjustment, audio streaming, custom alarms, and much more.

Whether you’re a maker, hobbyist, or entrepreneur, this project gives you everything you need to create your own unique alarm clock.
Features

Customizable LED Display: Change digit colors, brightness, and display modes.
Audio Streaming: Stream internet radio or your favorite music via URLs.
Automatic Brightness: Built-in light sensor adjusts brightness based on ambient light.
Alarms: Set daily or weekly alarms, each with custom music and volume levels.
Wi-Fi Integration: Automatically sync time with NTP and update settings remotely.
Open-Source: Modify and extend the functionality to meet your needs.

### Hardware Requirements

    Microcontroller: ESP32 WROOM
    LEDs: WS2812-2020 RGB LEDs
    Amplifier: MAX98357A 3W amplifier
    Speaker: 4-ohm speaker
    Power Supply: 5V USB input
    Additional Components:
        Photoresistor for brightness control
        1200uF capacitor for amplifier stabilization
    Metal plate for touch sensor

### Software Requirements
Python Server

    Backend:    Flask
                Nginx
                Apache2


## Installation
1. Clone the Repository

git clone https://github.com/glop/open-clock.git

cd alarm-clock

2. Install Python Dependencies

pip install -r requirements.txt

3. Upload Code to ESP32

    Use the Arduino IDE or PlatformIO to upload the firmware to your ESP32.

4. Set Up the Server
   
    Run the Flask server:

    python app.py

Contributing

### We welcome contributions from the community! If you'd like to contribute, follow these steps:

Fork the repository.
Create a new branch for your feature or bug fix.
Submit a pull request with a detailed description of your changes.

License

This project is licensed under the GNU General Public License v3.0.

If you use or modify this project, commercially or otherwise, you must retain the original attribution to gl0p.

### Acknowledgments

A special thanks to the open-source community for inspiring and supporting this project. 🎉

Feel free to tweak this template to fit your specific project details. Let me know if you’d like help with any particular section!
