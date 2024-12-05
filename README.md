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
    Custom PCB and 3d printed files.

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

   Make sure the Arduino IDE settings are correct.

   Tools:    Events Run on core 0
             Arduino runs on Core 1
             Partition Scheme: No FS 4MB (2MB APP x2)
   
    Use the Arduino IDE or PlatformIO to upload the firmware to your ESP32.
   
5. Run these commands to install the necessary system-level packages:

        sudo apt update
   
        sudo apt install -y python3 python3-pip ffmpeg

6. Set Up the Server

   After installing all dependencies, you should be able to run the flask server. 

        python main.py

## How To Use

There is a detailed usermanual attached to this repository. 

### TLDR; 

1. The display will flash green and it is set up in AP mode.
2. Connect to Clock_Setup wifi network.
3. Enter wifi credentials and setup information.
4. After setup you should be redirected to the main page.
5. Save to your home screen or bookmark on desktop.
6. Youtube Search and stream will be from your server.

## Tweaking service workers or HTML?

Clock_setup.h is plain text, easy to modify.

Clock_time_bin.h needs to be a compressed binary html file and run in program memory.

    gzip -9 clock_time.html

    xxd -i clock_time.html.gz | sed '1i #include <pgmspace.h>' | sed 's/^unsigned /const unsigned /' | sed 's/;$/ PROGMEM;/' > clock_time_bin.h

The manifest.json and serviceworker.js need to be converted to a binary file aswell.

    xxd -i serviceworker.js > service-worker.h

    xxd -i manifest.json > manifest_json.h

Kepp all these binary files when uploading the sketch in the same directory as the .ino file.

## Contributing

### We welcome contributions from the community! If you'd like to contribute, follow these steps:

Fork the repository.
Create a new branch for your feature or bug fix.
Submit a pull request with a detailed description of your changes.

License

This project is licensed under the GNU General Public License v3.0.

If you use or modify this project, commercially or otherwise, you must retain the original attribution to gl0p.

### Acknowledgments

A special thanks to the open-source community for inspiring and supporting this project. 🎉

This is still in devolopment so check back regularly for new updates.

Lots to come!




    
