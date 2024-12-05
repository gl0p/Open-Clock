#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <Preferences.h>
#include <time.h>
#include <Audio.h>
#include "manifest_json.h" // Generated from manifest.json
#include "service-worker.h" // Generated from service-worker.js
#include "esp_task_wdt.h"
#include "esp_system.h"
#include <HTTPClient.h>
#include <Update.h>
#include <esp32-hal-touch.h>

bool setup_device = false;

#define TOUCH_PIN 4 
#define extra_touch 33 //74 LOW Full hand, 100 high open, 82 single finger // 


// Include the HTML content for the setup and time display
#include "clock_setup.h"
#include "clock_time_bin.h"

struct Alarm {
    String time;      // Alarm time in "HH:MM" format
    bool enabled;     // Alarm status
    bool triggered;
    uint8_t days;     // Bitmask for days of the week (0b01111111 for all days)
    int volume;       // Volume for the alarm
    String streamURL; // Stream URL for the alarm
};


// OTA firmware server details
const char* host = "vahub.ca";  // Remove 'https://'
const int port = 80;            // Use 80 for HTTP or 443 for HTTPS
const char* firmware_path = "/esp32-update";
const char* firmwareKey = "fw_filename"; // Key length <= 15


#define MAX_ALARMS 10
Alarm alarms[MAX_ALARMS];
int alarmCount = 0; // Current number of alarms

// Define the LED pin and count
#define LED_PIN 15  // D15 on ESP32
#define LED_COUNT 30
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// I2S Configuration
#define I2S_BCK_PIN 12    // Bit Clock
#define I2S_WS_PIN 14     // Word Select or LRCLK
#define I2S_DOUT_PIN 13   // Serial Data Out

#define PHOTORESISTOR_PIN 34   // Adjust based on your wiring
#define MAX_BRIGHTNESS 255
#define MIN_BRIGHTNESS 0
#define MAX_BRIGHTNESS_MAPPINGS 50
#define MINIMUM_EFFECTIVE_BRIGHTNESS 10

#define SENSOR_READING_THRESHOLD 500 
#define BRIGHTNESS_CHANGE_THRESHOLD 50 //value when auto click's in from manual setting

bool autoBrightnessEnabled = true; 

struct BrightnessMapping {
    int sensorReading;
    int brightness;
};

BrightnessMapping brightnessMappings[MAX_BRIGHTNESS_MAPPINGS];
int brightnessMappingCount = 0; // Current number of mappings
bool manualBrightnessOverride = false;
int userSetBrightness = -1;   // -1 indicates no manual override
unsigned long lastLightReadingTime = 0;
const unsigned long LIGHT_READING_INTERVAL = 15; // 40 hz (15ms +10ms delay in loop)
int findBrightnessMappingIndex(int sensorReading);

int lightReadings[12];        // For averaging over 3 minutes
int lightReadingIndex = 0;
int brightnessHistory[50];    // For storing user-set brightness values
int brightnessHistoryIndex = 0;

String streamURL = "https://streaming.radio.co/s06bd9d805/listen"; // Default stream URL

// Set up web server on port 80
AsyncWebServer server(80);
// AsyncWebSocket wso("/ws");

struct TimeZone {
    const char* name;     // Display name
    const char* tzCode;   // POSIX time zone string
    const char* location; // Example locations
};

// Array of time zones
TimeZone timeZones[] = {
    // UTC−12:00
    {"UTC−12:00", "UTC+12", "Baker Island"},

    // UTC−11:00
    {"UTC−11:00", "UTC+11", "American Samoa"},

    // UTC−10:00 (Hawaii Standard Time, no DST)
    {"Hawaii Standard Time", "HST10", "Hawaii"},

    // UTC−09:00 (Alaska Standard Time)
    {"Alaska Standard Time", "AKST9AKDT,M3.2.0,M11.1.0", "Alaska"},

    // UTC−08:00 (Pacific Time)
    {"Pacific Time (US & Canada)", "PST8PDT,M3.2.0,M11.1.0", "Los Angeles, Vancouver"},

    // UTC−07:00 (Mountain Time)
    {"Mountain Time (US & Canada)", "MST7MDT,M3.2.0,M11.1.0", "Denver, Edmonton"},

    // UTC−06:00 (Central Time)
    {"Central Time (US & Canada)", "CST6CDT,M3.2.0,M11.1.0", "Chicago, Mexico City"},

    // UTC−05:00 (Eastern Time)
    {"Eastern Time (US & Canada)", "EST5EDT,M3.2.0,M11.1.0", "New York, Toronto"},

    // UTC−04:00 (Atlantic Time)
    {"Atlantic Time (Canada)", "AST4ADT,M3.2.0,M11.1.0", "Halifax, Puerto Rico"},

    // UTC−03:00 (Argentina, Greenland)
    {"UTC−03:00", "ART3", "Buenos Aires, Greenland"},

    // UTC−02:00 (South Georgia)
    {"UTC−02:00", "UTC+2", "South Georgia and the South Sandwich Islands"},

    // UTC−01:00 (Azores)
    {"Azores Standard Time", "AZOT1AZOST,M3.5.0/0,M10.5.0/1", "Azores, Cape Verde"},

    // UTC+00:00 (GMT)
    {"Greenwich Mean Time", "GMT0BST,M3.5.0/1,M10.5.0/2", "London, Dublin, Lisbon"},

    // UTC+01:00 (Central European Time)
    {"Central European Time", "CET-1CEST,M3.5.0,M10.5.0/3", "Berlin, Paris, Madrid"},

    // UTC+02:00 (Eastern European Time)
    {"Eastern European Time", "EET-2EEST,M3.5.0/3,M10.5.0/4", "Athens, Cairo, Johannesburg"},

    // UTC+03:00 (Moscow Time)
    {"Moscow Standard Time", "MSK-3", "Moscow, Istanbul, Riyadh"},

    // UTC+04:00
    {"UTC+04:00", "GST-4", "Dubai, Baku, Samara"},

    // UTC+05:00
    {"UTC+05:00", "PKT-5", "Karachi, Islamabad, Tashkent"},

    // UTC+06:00
    {"UTC+06:00", "BST-6", "Dhaka, Almaty"},

    // UTC+07:00
    {"UTC+07:00", "ICT-7", "Bangkok, Jakarta, Hanoi"},

    // UTC+08:00
    {"UTC+08:00", "CST-8", "Beijing, Singapore, Perth"},

    // UTC+09:00
    {"Japan Standard Time", "JST-9", "Tokyo, Seoul, Yakutsk"},

    // UTC+10:00
    {"Australian Eastern Time", "AEST-10AEDT,M10.1.0,M4.1.0/3", "Sydney, Guam, Vladivostok"},

    // UTC+11:00
    {"UTC+11:00", "SBT-11", "Solomon Islands, New Caledonia"},

    // UTC+12:00
    {"New Zealand Standard Time", "NZST-12NZDT,M9.5.0,M4.1.0/3", "Auckland, Fiji, Kamchatka"},

    // UTC+13:00
    {"UTC+13:00", "TOT-13", "Samoa, Tonga"},

    // UTC+14:00
    {"UTC+14:00", "LINT-14", "Kiritimati Island"},
};

Audio audio;

// Wi-Fi credentials
String ssid;
String password;
bool timeReceived = false; // Indicates whether we have successfully obtained the time

// Preferences for storing settings
Preferences preferences;  // For storing settings
String displayMode = "time";  // "time" or "countdown"
uint8_t ledBrightness = 128; // Default brightness (0-255)
uint32_t ledColorRGB = 0x008000; // Default color in RGB (green)

// For individual digit colors
uint32_t digitColorsRGB[4]; // 4 digits
uint32_t colonColorRGB = 0; // Color for the colon

// Audio settings
int audioVolume = 10; // Volume level from 0 to 21
bool isStreaming = false;

int currentHour = -1;
int currentMinute = -1;

// Wi-Fi off settings
bool enableWiFiOff = false;
String wifiOffStartTime = "";
String wifiOffEndTime = "";

String deviceName = "my_clock"; // Default device name
String streamName = ""; 
bool startStream = false;

// For backup sound
unsigned long lastBeepTime = 0;
bool beepState = false; // true = on, false = off

bool is24HourFormat = true; // Default to 24-hour format

String alarmStreamURL = "";

bool firmwareUpdateAvailable = false;
String latestFirmwareName = "";

// Digit map for 7-segment display
const int digitMap[10][7] = {
  {1, 1, 1, 0, 1, 1, 1}, // 0
  {0, 0, 0, 0, 0, 1, 1}, // 1
  {0, 1, 1, 1, 1, 1, 0}, // 2
  {0, 0, 1, 1, 1, 1, 1}, // 3
  {1, 0, 0, 1, 0, 1, 1}, // 4
  {1, 0, 1, 1, 1, 0, 1}, // 5
  {1, 1, 1, 1, 1, 0, 1}, // 6
  {0, 0, 0, 0, 1, 1, 1}, // 7
  {1, 1, 1, 1, 1, 1, 1}, // 8
  {1, 0, 1, 1, 1, 1, 1}  // 9
};

// Add these global variables at the top of your code
int previousHour = -1;
int previousMinute = -1;
unsigned long lastDisplayUpdate = 0; // To throttle updates in countdown mode

void displayTime(int hours, int minutes);
void displayDigit(int digit, int position);
void displayColon();

// Disable zero settings
bool disableZero24hr = false;
bool disableZero12hr = false;
bool disableZeroCountdown = false;

uint32_t hexToRGB(const String& hexColor);
void shutdownAPTask(void * parameter);

void audioTask(void * parameter){
    while(true){
        audio.loop();
        // Serial.print("Audio Status: ");
        // Serial.println(audio.isRunning() ? "Running" : "Stopped");
        // static unsigned long lastPrintTime = 0;
        // unsigned long currentTime = millis();
        // if (currentTime - lastPrintTime >= 1000) { // Every 5 seconds
            // UBaseType_t highWaterMark = uxTaskGetStackHighWaterMark(NULL);
            // Serial.print("Audio Task Stack High Water Mark: ");
            // Serial.println(highWaterMark);
            // lastPrintTime = currentTime;
            // Serial.print("Free heap: ");
            // Serial.print(ESP.getFreeHeap());
            // Serial.println(" bytes");
        // }
        vTaskDelay(1); // Yield to other tasks
    }
}

TaskHandle_t audioTaskHandle = NULL;
SET_LOOP_TASK_STACK_SIZE(6144);

void handleDeletePreferenceGroups(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    static String body;
    if (index == 0) {
        body = "";
        body.reserve(total);
    }
    body += String((char*)data).substring(0, len);

    if (index + len == total) {
        // Parse JSON
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, body);
        if (error) {
            request->send(400, "application/json", "{\"status\":\"error\", \"message\":\"Invalid JSON\"}");
            return;
        }

        JsonArray groupsArray = doc["groups"];
        if (groupsArray.isNull()) {
            request->send(400, "application/json", "{\"status\":\"error\", \"message\":\"Invalid groups data\"}");
            return;
        }

        // Process each selected group
        for (JsonVariant groupVar : groupsArray) {
            String groupName = groupVar.as<String>();
            Serial.printf("Deleting group: %s\n", groupName.c_str());

            // Call the function to remove preferences for the given group
            removeGroupPreferences(groupName);
        }

        request->send(200, "application/json", "{\"status\":\"success\"}");
    }
}


void removeGroupPreferences(const String& groupName) {
    const char* keys[10]; // Adjust size based on the number of preferences per group
    int keyCount = 0;

    // Assign keys based on the group name
    if (groupName == "displaySettings") {
        const char* tempKeys[] = {
            "ledBrightness", "ledColorRGB", "digitColor0", "digitColor1",
            "digitColor2", "digitColor3", "colonColor0", "displayMode", "is24HourFormat"
        };
        memcpy(keys, tempKeys, sizeof(tempKeys));
        keyCount = sizeof(tempKeys) / sizeof(tempKeys[0]);

    } else if (groupName == "audioSettings") {
        const char* tempKeys[] = {
            "audioVolume", "streamURL", "streamName", "isStreaming"
        };
        memcpy(keys, tempKeys, sizeof(tempKeys));
        keyCount = sizeof(tempKeys) / sizeof(tempKeys[0]);

    } else if (groupName == "timeSettings") {
        const char* tempKeys[] = { "timeZoneString" };
        memcpy(keys, tempKeys, sizeof(tempKeys));
        keyCount = sizeof(tempKeys) / sizeof(tempKeys[0]);

    } else if (groupName == "wifiSettings") {
        const char* tempKeys[] = {
            "ssid", "password", "enableWiFiOff", "wifiOffStartTime", "wifiOffEndTime"
        };
        memcpy(keys, tempKeys, sizeof(tempKeys));
        keyCount = sizeof(tempKeys) / sizeof(tempKeys[0]);

    } else if (groupName == "alarms") {
        const char* tempKeys[] = { "alarms" };
        memcpy(keys, tempKeys, sizeof(tempKeys));
        keyCount = sizeof(tempKeys) / sizeof(tempKeys[0]);

    } else if (groupName == "brightnessMappings") {
        const char* tempKeys[] = { "brightnessMappings" };
        memcpy(keys, tempKeys, sizeof(tempKeys));
        keyCount = sizeof(tempKeys) / sizeof(tempKeys[0]);
    } else {
        Serial.printf("Unknown group: %s\n", groupName.c_str());
        return;
    }

    // Remove each preference in the group
    for (int i = 0; i < keyCount; i++) {
        if (preferences.isKey(keys[i])) {
            preferences.remove(keys[i]);
            Serial.printf("Preference '%s' removed.\n", keys[i]);
        }
    }
}

void handleManualBrightnessSet(int brightness) {
    setBrightness(brightness);
    if (!autoBrightnessEnabled) {
        // Save the user-set brightness
        userSetBrightness = brightness;
    } else {
        // Record the brightness mapping for auto brightness
        manualBrightnessOverride = true;  // Add this line
        recordUserBrightnessSetting(brightness);
    }
    
    Serial.printf("Manual brightness set to: %d\n", brightness);
    // No manual override timer needed; mappings are used
}


void setup() {
    // Initialize light readings to current light level
    int initialLightLevel = analogRead(PHOTORESISTOR_PIN);
    for (int i = 0; i < 12; i++) {
        lightReadings[i] = initialLightLevel;
    }

    // Initialize brightness history with the current brightness
    for (int i = 0; i < 50; i++) {
        brightnessHistory[i] = ledBrightness;
    }
    loadBrightnessMappings();

    
    // Optimize Wi-Fi settings for better stability
    WiFi.setSleep(false); // Disable Wi-Fi sleep
    WiFi.setAutoReconnect(true); // Enable auto-reconnect
    WiFi.persistent(true); // Make settings persistent

    pinMode(2, OUTPUT);  // Set the LED pin as output
    digitalWrite(2, LOW);
    pinMode(27, INPUT); // For stopping the audio stream

    Serial.begin(115200);

    // Initialize the NeoPixel strip
    strip.begin();
    strip.show();

    ///  ***  REMOVE DURING PRODUCTION  ***  ///
    // preferences.remove("brightnessMappings");
    ///  ***  REMOVE DURING PRODUCTION  ***  ///

    // Initialize Preferences
    preferences.begin("clock", false);  // "clock" is the namespace
    streamURL = preferences.getString("streamURL", streamURL); // Default to current streamURL if not set
    displayMode = preferences.getString("displayMode", "time");  // Default to "time" if not set
    ledBrightness = preferences.getUChar("ledBrightness", 128); // Default brightness
    ledColorRGB = preferences.getUInt("ledColorRGB", 0x008000); // Default color in RGB (green)
    deviceName = preferences.getString("deviceName", "my_clock");
    String deviceID = preferences.getString("deviceID", "");
    is24HourFormat = preferences.getBool("is24HourFormat", true); // Default to 24-hour format
    // Load audio settings
    audioVolume = preferences.getInt("audioVolume", 10);
    // Load Wi-Fi off settings
    enableWiFiOff = preferences.getBool("enableWiFiOff", false);
    wifiOffStartTime = preferences.getString("wifiOffStartTime", "");
    wifiOffEndTime = preferences.getString("wifiOffEndTime", "");
    streamName = preferences.getString("streamName", "");
    String storedSSID = preferences.getString("ssid", "");
    String storedPassword = preferences.getString("password", ""); 
    autoBrightnessEnabled = preferences.getBool("autoBrightnessEnabled", true); // Default to true
    // Load disable zero settings
    disableZero24hr = preferences.getBool("disableZero24hr", false);
    disableZero12hr = preferences.getBool("disableZero12hr", false);
    disableZeroCountdown = preferences.getBool("disableZeroCountdown", false);


    if (deviceID == "") {
        // Initialize random number generator
        randomSeed(analogRead(0));

        // Generate a 12-character hex string
        deviceID = "";
        for (int i = 0; i < 32; i++) {
            deviceID += String(random(0, 16), HEX);
        }
        preferences.putString("deviceID", deviceID);
    }
    Serial.println(deviceID);

    strip.setBrightness(ledBrightness);
    strip.show();

    
    // Load individual digit colors
    for (int i = 0; i < 4; i++) {
        char key[12];
        snprintf(key, sizeof(key), "digitColor%d", i);
        if (preferences.isKey(key)) {
            digitColorsRGB[i] = preferences.getUInt(key);
        } else {
            digitColorsRGB[i] = 0; // Zero indicates no individual color set
        }
    }

    // Load colon color
    if (preferences.isKey("colonColor0")) {
        colonColorRGB = preferences.getUInt("colonColor0");
    } else {
        colonColorRGB = 0; // Zero indicates no individual color set
    }

    // Attempt to connect to Wi-Fi using stored credentials
    WiFi.mode(WIFI_STA);
    
    if (storedSSID != "") {
        WiFi.setHostname(deviceName.c_str());
        WiFi.begin(storedSSID.c_str(), storedPassword.c_str());
    } else {
        WiFi.begin(); // Start Wi-Fi without credentials
    }

    Serial.println("Attempting to connect to Wi-Fi...");
    int retries = 20;
    while (WiFi.status() != WL_CONNECTED && retries-- > 0) {  
        strip.setPixelColor(28, strip.Color(255,0,0)); 
        strip.show();
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected to Wi-Fi");
        
        String timeZoneStr = preferences.getString("timeZoneString", "UTC0");

        
        // Configure time with time zone and NTP servers
        configTzTime(timeZoneStr.c_str(), "pool.ntp.org", "time.nist.gov");

        struct tm timeInfo;
        if (!getLocalTime(&timeInfo)) {
            Serial.println("Failed to obtain time");
        } else {
            Serial.println(&timeInfo, "Time set: %A, %B %d %Y %H:%M:%S");
            timeReceived = true;
        }

        // Start mDNS responder for the new network
        if (!MDNS.begin(deviceName.c_str())) {
            Serial.println("Error setting up mDNS responder!");
        } else {
            Serial.println("mDNS responder started");
            MDNS.addService("http", "tcp", 80);
        }

        // Check if PSRAM is available
        if (psramFound()) {
            Serial.println("PSRAM is available");
        } else {
            Serial.println("PSRAM is not available");
        }

        audio.setBufsize(24 * 1024, 64 * 1024); // 24 KB RAM buffer, 64 KB PSRAM buffer

        // Initialize Audio
        audio.setPinout(I2S_BCK_PIN, I2S_WS_PIN, I2S_DOUT_PIN);
        audio.setVolume(audioVolume); // Volume level from 0 to 21

        // Start Streaming Audio if enabled
        isStreaming = preferences.getBool("isStreaming", false);
        if (isStreaming) {
            audio.connecttohost(streamURL.c_str());
        }

        xTaskCreatePinnedToCore(
            audioTask,        // Task function
            "AudioTask",      // Name
            6144,             // Stack size
            NULL,             // Parameters
            8,                // Priority (higher value = higher priority)
            &audioTaskHandle, // Task handle
            0                 // Core (0 or 1)
        );
        WiFi.setTxPower(WIFI_POWER_2dBm);
        Serial.print("WiFi Power Level:");
        Serial.println(WiFi.getTxPower());
        // Print the signal strength
        int32_t rssi = WiFi.RSSI();
        Serial.print("Signal Strength (RSSI): ");
        Serial.print(rssi);
        Serial.println(" dBm");
    } else {
        
        Serial.println("Failed to connect to Wi-Fi. Starting Access Point...");

        // Start in Access Point mode for initial setup
        WiFi.disconnect(true);  // Disconnect from any previous connection
        WiFi.mode(WIFI_AP);

        IPAddress apIP(192, 168, 4, 1);       // Static IP address for the Access Point
        IPAddress netMsk(255, 255, 255, 0);   // Subnet mask

        WiFi.softAPConfig(apIP, apIP, netMsk);

        bool apStarted = WiFi.softAP("Clock_Setup");
        if (apStarted) {
            Serial.println("Access Point 'Clock_Setup' started");
            Serial.print("AP IP address: ");
            Serial.println(WiFi.softAPIP());
            setup_device = true;
        } else {
            Serial.println("Failed to start Access Point!");
        }
    }

    // Initialize the display once
    if (timeReceived) {
        struct tm timeInfo;
        if (getLocalTime(&timeInfo)) {
            int currentHour = timeInfo.tm_hour;
            int currentMinute = timeInfo.tm_min;
            displayTime(currentHour, currentMinute);
            lastDisplayUpdate = millis(); // Set the initial timestamp
        } else {
            Serial.println("Failed to obtain time during setup");
        }
    }
    
    loadAlarms();

    //Service Workers
    server.on("/manifest.json", HTTP_GET, handleManifestJSON);
    server.on("/service-worker.js", HTTP_GET, handleServiceWorkerJS);

    // Register handlers
    server.on("/", HTTP_GET, handleRoot);
    server.on("/connect", HTTP_POST, handleConnect);
    server.on("/connecting", HTTP_GET, handleConnecting);
    server.on("/time", HTTP_GET, handleTimePage);
    server.on("/getCurrentTime", HTTP_GET, handleGetCurrentTime);
    server.on("/setTimeZone", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, handleSetTimeZone);
    server.on("/getTimeZone", HTTP_GET, handleGetTimeZone);
    server.on("/setDisplayMode", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, handleSetDisplayMode);
    server.on("/getSettings", HTTP_GET, handleGetSettings);

    // Register handlers for LED settings
    server.on("/setLEDSettings", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, handleSetLEDSettings);
    server.on("/getLEDSettings", HTTP_GET, handleGetLEDSettings);
    server.on("/setDigitColor", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, handleSetDigitColor);

    // Register handlers for audio settings
    server.on("/setAudioSettings", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, handleSetAudioSettings);
    server.on("/getAudioSettings", HTTP_GET, handleGetAudioSettings);

    // Register handler for setting stream state
    server.on("/setStreamState", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, handleSetStreamState);

    // Register alarm handlers
    server.on("/getAlarmSettings", HTTP_GET, handleGetAlarms);
    server.on("/setAlarmSettings", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, handleSetAlarms);

    // Register handlers for Wi-Fi settings
    server.on("/setWiFiSettings", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, handleSetWiFiSettings);
    server.on("/getWiFiSettings", HTTP_GET, handleGetWiFiSettings);
    server.on("/getIPAddress", HTTP_GET, handleGetIPAddress);

    // Setup handler
    server.on("/setup", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, handleSetup);
    server.on("/getDeviceID", HTTP_GET, handleGetDeviceID);

    // Handlers for hour format
    server.on("/setHourFormat", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, handleSetHourFormat);
    server.on("/getHourFormat", HTTP_GET, handleGetHourFormat);

    //UPDATER
    server.on("/getFirmwareStatus", HTTP_GET, handleGetFirmwareStatus);
    server.on("/updateFirmware", HTTP_POST, handleUpdateFirmware);

    // Device settings
    server.on("/deletePreferenceGroups", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, handleDeletePreferenceGroups);
    server.on("/resetDevice", HTTP_POST, [](AsyncWebServerRequest *request) {handleResetDevice(request);});
    
    server.on("/setAutoBrightness", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, handleSetAutoBrightness);
    server.on("/getAutoBrightness", HTTP_GET, handleGetAutoBrightness);

    // Register handlers for disable zero settings
    server.on("/getDisableZeroSettings", HTTP_GET, handleGetDisableZeroSettings);
    server.on("/setDisableZeroSettings", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, handleSetDisableZeroSettings);


    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

    // Initialize WebSocket
    // wso.onEvent(onWebSocketEvent);
    // server.addHandler(&wso);

    // Start the web server
    server.begin();
    Serial.println("HTTP server started");
}

// Function to handle firmware update request
void handleUpdateFirmware(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Firmware update initiated. The device will restart if an update is available.");
    xTaskCreatePinnedToCore(
        otaTask,          // Task function
        "OTATask",        // Name
        8192,             // Stack size
        NULL,             // Parameters
        10,               // Priority (higher number = higher priority)
        NULL,             // Task handle
        0                 // Core (0 or 1)
    );
}

// Function to check if a new firmware is available
bool checkForNewFirmware() {
    WiFiClient client;
    String firmwareUrl = String("http://") + host + firmware_path;

    Serial.printf("Connecting to %s:%d\n", host, port);
    if (!client.connect(host, port)) {
        Serial.println("Connection to server failed.");
        return false;
    }

    // Send a HEAD request to check for the latest firmware
    client.print(String("HEAD ") + firmware_path + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "Connection: close\r\n\r\n");

    // Wait for the response
    unsigned long timeout = millis();
    while (client.connected() && !client.available()) {
        delay(10);
        if (millis() - timeout > 5000) { // Timeout after 5 seconds
            Serial.println(">>> Client Timeout!");
            client.stop();
            return false;
        }
    }

    // Read the headers and look for 'Firmware-Name'
    bool firmwareHeaderFound = false;
    String line;
    while (client.connected() || client.available()) {
        line = client.readStringUntil('\n');
        line.trim();
        Serial.println(line);  // Debug: Print each header line

        if (line.startsWith("Firmware-Name: ")) {
            String latestFilename = line.substring(strlen("Firmware-Name: "));
            latestFilename.trim();

            String currentFilename = preferences.getString(firmwareKey, "");
            Serial.printf("Current firmware: %s\n", currentFilename.c_str());
            Serial.printf("Latest firmware: %s\n", latestFilename.c_str());

            if (latestFilename != currentFilename) {
                firmwareHeaderFound = true;
                latestFirmwareName = latestFilename;
            }
        }

        if (line.length() == 0) {
            // Headers have ended
            break;
        }
    }

    client.stop();
    return firmwareHeaderFound;
}

void performOTAUpdate() {
    // Stop the web server to free resources
    server.end();
    delay(1000);

    // Prepare for OTA update
    String firmwareUrl = String("http://") + host + firmware_path;
    Serial.printf("Starting OTA update from: %s\n", firmwareUrl.c_str());

    if (audio.isRunning()) {
        audio.stopSong();  // Stop any audio tasks
    }

    // Delete unnecessary tasks to free up memory
    if (audioTaskHandle != NULL) {
        vTaskDelete(audioTaskHandle);
        audioTaskHandle = NULL;
    }

    HTTPClient http;
    http.begin(firmwareUrl);  // Initialize HTTP connection
    int httpCode = http.GET();  // Send GET request

    if (httpCode == HTTP_CODE_OK) {
        int contentLength = http.getSize();
        bool canBegin = Update.begin(contentLength);

        if (canBegin) {
            WiFiClient *stream = http.getStreamPtr();
            size_t written = 0;
            uint8_t buffer[512];  // Buffer for chunked download

            // Feed watchdog periodically and yield to avoid timeouts
            unsigned long lastFeedTime = millis();
            const unsigned long feedInterval = 1000;  // 1 second

            // Download firmware in chunks
            while (written < contentLength) {
                if (millis() - lastFeedTime > feedInterval) {
                    lastFeedTime = millis();
                }

                size_t bytes = stream->readBytes(buffer, sizeof(buffer));
                if (bytes > 0) {
                    Update.write(buffer, bytes);  // Write chunk to flash
                    written += bytes;
                } else {
                    delay(1);  // Yield to other tasks
                }
            }

            if (Update.end()) {
                Serial.println("OTA Update Complete. Restarting...");
                preferences.putString(firmwareKey, latestFirmwareName);
                ESP.restart();
            } else {
                Serial.printf("OTA Error: %s\n", Update.errorString());
            }
        } else {
            Serial.println("Not enough space for OTA update.");
        }
    } else {
        Serial.printf("HTTP error: %d\n", httpCode);
    }
    http.end();  // Close the connection
    server.begin();
}

void otaTask(void * parameter){
    performOTAUpdate();
    vTaskDelete(NULL); // Delete task after OTA is done
}

void handleManifestJSON(AsyncWebServerRequest *request) {
    request->send_P(200, "application/json", manifest_json, manifest_json_len);
}

void handleServiceWorkerJS(AsyncWebServerRequest *request) {
    request->send_P(200, "application/javascript", service_worker_js, service_worker_js_len);
}

void handleGetDeviceID(AsyncWebServerRequest *request) {
    String deviceID = preferences.getString("deviceID", "");
    if (deviceID == "") {
        // Generate a new device ID if it doesn't exist
        deviceID = "";
        for (int i = 0; i < 32; i++) {
            deviceID += String(random(0, 16), HEX);
        }
        preferences.putString("deviceID", deviceID);
    }

    StaticJsonDocument<100> jsonResponse;
    jsonResponse["deviceID"] = deviceID;
    String jsonResponseStr;
    serializeJson(jsonResponse, jsonResponseStr);
    request->send(200, "application/json", jsonResponseStr);
}

void handleResetDevice(AsyncWebServerRequest *request) {
    // Clear all preferences
    preferences.clear();
    Serial.println("Preferences cleared.");

    // Erase Wi-Fi credentials stored by the Wi-Fi library
    WiFi.disconnect(true, true);  // Erase Wi-Fi credentials from flash
    Serial.println("Wi-Fi credentials erased.");

    // Send response to client
    request->send(200, "application/json", "{\"status\":\"success\"}");

    // Delay to allow the response to be sent
    delay(1000);

    // Restart the device
    ESP.restart();
}

void handleSetup(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    static String body;
    if (index == 0) {
        body = "";
        body.reserve(total);
    }
    body += String((const char*)data, len);

    if (index + len == total) {
        StaticJsonDocument<500> doc;
        DeserializationError error = deserializeJson(doc, body);
        if (!error) {
            // Extract data
            String ssid = doc["ssid"].as<String>();
            String password = doc["password"].as<String>();
            String timeZoneStr = doc["timeZoneString"].as<String>();
            String deviceNameReceived = doc["deviceName"].as<String>();

            // Save to preferences
            preferences.putString("ssid", ssid);
            preferences.putString("password", password);
            preferences.putString("timeZoneString", timeZoneStr);
            preferences.putString("deviceName", deviceNameReceived);

            // Update variables
            deviceName = deviceNameReceived;

            // Keep both AP and STA modes active
            WiFi.mode(WIFI_AP_STA);
            WiFi.setHostname(deviceName.c_str());
            WiFi.begin(ssid.c_str(), password.c_str());
            int retries = 20;
            while (WiFi.status() != WL_CONNECTED && retries-- > 0) {
                delay(500);
                Serial.print(".");
            }
            Serial.println();

            StaticJsonDocument<200> jsonResponse;

            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("Connected to Wi-Fi");

                // Update mDNS with new device name
                MDNS.end(); // Stop existing mDNS responder
                if (!MDNS.begin(deviceName.c_str())) {
                    Serial.println("Error setting up mDNS responder with new device name!");
                } else {
                    Serial.println("mDNS responder started with new device name");
                    MDNS.addService("http", "tcp", 80);
                }

                // Configure time with the new time zone
                configTzTime(timeZoneStr.c_str(), "pool.ntp.org", "time.nist.gov");
                timeReceived = true;

                jsonResponse["status"] = "success";
                jsonResponse["ip"] = WiFi.localIP().toString();
            } else {
                Serial.println("Failed to connect to Wi-Fi.");
                jsonResponse["status"] = "fail";
                jsonResponse["ip"] = "0.0.0.0";
            }

            String jsonResponseStr;
            serializeJson(jsonResponse, jsonResponseStr);
            request->send(200, "application/json", jsonResponseStr);

            // Optional: Schedule AP mode shutdown after a delay
            xTaskCreate(shutdownAPTask, "ShutdownAP", 2048, NULL, 1, NULL);
            setup_device = false;

        } else {
            request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        }
    }
}

void handleSetHourFormat(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    static String body;
    if (index == 0) {
        body = "";
        body.reserve(total);
    }
    body += String((char*)data).substring(0, len);

    if (index + len == total) {
        StaticJsonDocument<100> doc;
        DeserializationError error = deserializeJson(doc, body);
        if (!error) {
            if (doc.containsKey("is24HourFormat")) {
                is24HourFormat = doc["is24HourFormat"].as<bool>();
                preferences.putBool("is24HourFormat", is24HourFormat);
                Serial.printf("Hour format set to %s\n", is24HourFormat ? "24-hour" : "12-hour");

                // Send a JSON response
                StaticJsonDocument<100> jsonResponse;
                jsonResponse["is24HourFormat"] = is24HourFormat;
                String jsonResponseStr;
                serializeJson(jsonResponse, jsonResponseStr);
                request->send(200, "application/json", jsonResponseStr);

                // Update the display immediately
                displayTime(currentHour, currentMinute);
            } else {
                request->send(400, "application/json", "{\"error\":\"Missing 'is24HourFormat' field\"}");
            }
        } else {
            request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        }
    }
}

void handleGetHourFormat(AsyncWebServerRequest *request) {
    StaticJsonDocument<100> jsonResponse;
    jsonResponse["is24HourFormat"] = is24HourFormat;
    String jsonResponseStr;
    serializeJson(jsonResponse, jsonResponseStr);
    request->send(200, "application/json", jsonResponseStr);
}



void shutdownAPTask(void * parameter) {
    // Wait for 5 seconds before shutting down the AP
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    vTaskDelete(NULL);
}

// Define the ring order
const int ledRingOrder[] = {4, 11, 18, 25, 26, 27, 23, 16, 9, 2, 1, 0};
const int ledRingOrder2[] = {23, 16, 9, 2, 1, 0, 4, 11, 18, 25, 26, 27};
const int numRingLEDs = sizeof(ledRingOrder) / sizeof(ledRingOrder[0]);
// Timing variables
unsigned long lastUpdateTime = 0;
const unsigned long updateInterval = 1;    // Update every 20 milliseconds
const unsigned long fadeDuration = 25;     // Total duration for one fade transition (in milliseconds)

// Animation variables
int currentLEDIndex = 0;
float fadeProgress = 0.0;

void updateRingAnimation() {
    // Update fade progress
    fadeProgress += (float)updateInterval / (float)fadeDuration;
    if (fadeProgress >= 1.0) {
        fadeProgress -= 1.0;
        currentLEDIndex = (currentLEDIndex + 1) % numRingLEDs;
    }

    int nextLEDIndex = (currentLEDIndex + 1) % numRingLEDs;

    // Calculate the indices for the second ring
    int currentLEDIndex2 = (currentLEDIndex) % numRingLEDs;
    int nextLEDIndex2 = (currentLEDIndex2 + 1) % numRingLEDs;

    // Calculate brightness levels
    int brightnessCurrent = (int)(255 * (1.0 - fadeProgress));
    int brightnessNext = (int)(255 * fadeProgress);

    // Set colors for the LEDs
    uint32_t colorCurrent = strip.Color(0, brightnessCurrent, 0); // Green color
    uint32_t colorNext = strip.Color(0, brightnessNext, 0);

    // Clear all LEDs first
    for (int i = 0; i < numRingLEDs; i++) {
        strip.setPixelColor(ledRingOrder[i], 0);
        strip.setPixelColor(ledRingOrder2[i], 0);
    }

    // Update LEDs for ring 1
    strip.setPixelColor(ledRingOrder[currentLEDIndex], colorCurrent);
    strip.setPixelColor(ledRingOrder[nextLEDIndex], colorNext);

    // Update LEDs for ring 2
    strip.setPixelColor(ledRingOrder2[currentLEDIndex2], colorCurrent);
    strip.setPixelColor(ledRingOrder2[nextLEDIndex2], colorNext);

    strip.show();
}

unsigned long lastTouchTime = 0;
bool touchTriggered = false; 

void loop() {

    if (WiFi.getMode() == WIFI_MODE_AP) {
      strip.clear();
      strip.show();
      updateRingAnimation();
    }

    if (WiFi.status() == WL_CONNECTED && timeReceived) {
        struct tm timeInfo;
        if (getLocalTime(&timeInfo)) {
            currentHour = timeInfo.tm_hour;
            currentMinute = timeInfo.tm_min;
            
            unsigned long currentMillis = millis();
            if (currentMillis - lastDisplayUpdate >= 20000) {
                displayTime(currentHour, currentMinute);
                lastDisplayUpdate = currentMillis;
            }

            // Handle Wi-Fi off schedule
            if (enableWiFiOff) {
                int currentTotalMinutes = currentHour * 60 + currentMinute;

                // Parse start and end times
                if (wifiOffStartTime.length() == 5 && wifiOffEndTime.length() == 5) {
                    int startHour = wifiOffStartTime.substring(0, 2).toInt();
                    int startMinute = wifiOffStartTime.substring(3, 5).toInt();
                    int startTotalMinutes = startHour * 60 + startMinute;

                    int endHour = wifiOffEndTime.substring(0, 2).toInt();
                    int endMinute = wifiOffEndTime.substring(3, 5).toInt();
                    int endTotalMinutes = endHour * 60 + endMinute;

                    bool wifiShouldBeOff = false;

                    if (startTotalMinutes <= endTotalMinutes) {
                        // Simple case
                        if (currentTotalMinutes >= startTotalMinutes && currentTotalMinutes < endTotalMinutes) {
                            wifiShouldBeOff = true;
                        }
                    } else {
                        // Over midnight
                        if (currentTotalMinutes >= startTotalMinutes || currentTotalMinutes < endTotalMinutes) {
                            wifiShouldBeOff = true;
                        }
                    }

                    // Check alarms to determine if Wi-Fi should stay on for any alarm within the next 5 minutes
                    bool wifiShouldBeOnForAlarm = false;
                    for (int i = 0; i < alarmCount; i++) {
                        if (alarms[i].enabled) {
                            // Check if current day is among the alarm's days
                            int currentDayOfWeek = timeInfo.tm_wday; // Sunday = 0, Monday = 1, ..., Saturday = 6
                            if (alarms[i].days & (1 << currentDayOfWeek)) {
                                // Parse alarm time
                                int alarmHour = alarms[i].time.substring(0, 2).toInt();
                                int alarmMinute = alarms[i].time.substring(3, 5).toInt();
                                int alarmTotalMinutes = alarmHour * 60 + alarmMinute;

                                // Check if alarm is within the next 5 minutes
                                int diff = alarmTotalMinutes - currentTotalMinutes;
                                if (diff <= 5 && diff > 0) {
                                    wifiShouldBeOnForAlarm = true;
                                }
                            }
                        }
                    }

                    if (wifiShouldBeOff && !wifiShouldBeOnForAlarm) {
                        // Wi-Fi should be off
                        if (WiFi.status() == WL_CONNECTED) {
                            Serial.println("Turning Wi-Fi off");
                            WiFi.disconnect(true);
                            WiFi.mode(WIFI_OFF);
                            timeReceived = false;
                        }
                    } else {
                        // Wi-Fi should be on
                        if (WiFi.status() != WL_CONNECTED) {
                            Serial.println("Turning Wi-Fi on");
                            WiFi.mode(WIFI_STA);
                            WiFi.setHostname(deviceName.c_str());
                            WiFi.begin();
                            int retries = 20;
                            while (WiFi.status() != WL_CONNECTED && retries-- > 0) {
                                delay(500);
                                Serial.print(".");
                            }
                            Serial.println();
                            if (WiFi.status() == WL_CONNECTED) {
                                Serial.println("Wi-Fi reconnected");
                                // Reconfigure time
                                String timeZoneStr = preferences.getString("timeZoneString", "UTC0");
                                configTzTime(timeZoneStr.c_str(), "pool.ntp.org", "time.nist.gov");
                                timeReceived = true;
                            } else {
                                Serial.println("Failed to reconnect Wi-Fi");
                            }
                        }
                    }
                }
            }
            // Handle multiple alarms
            bool alarmsChanged = false; // Flag to track if any alarm was triggered or reset
            for (int i = 0; i < alarmCount; i++) {
                if (alarms[i].enabled && !alarms[i].triggered) {
                    int alarmHour = alarms[i].time.substring(0, 2).toInt();
                    int alarmMinute = alarms[i].time.substring(3, 5).toInt();

                    // Check if current day is among the alarm's days
                    int currentDayOfWeek = timeInfo.tm_wday; // Sunday = 0, Monday = 1, ..., Saturday = 6
                    if (alarms[i].days & (1 << currentDayOfWeek)) {
                        if (currentHour == alarmHour && currentMinute == alarmMinute) {
                            // Alarm time reached
                            alarms[i].triggered = true;
                            triggerAlarm(i);
                            alarmsChanged = true;
                        }
                    }
                }

                // Reset triggered flag if time no longer matches
                if (alarms[i].triggered) {
                    int alarmHour = alarms[i].time.substring(0, 2).toInt();
                    int alarmMinute = alarms[i].time.substring(3, 5).toInt();

                    if (currentHour != alarmHour || currentMinute != alarmMinute) {
                        alarms[i].triggered = false;
                        alarmsChanged = true;
                    }
                }
            }

            if (alarmsChanged) {
                saveAlarms(); // Save once after processing all alarms
            }

            // Handle backup sound beeping pattern (implement as needed)
            // if (alarmTriggered && useBackupSound) {
            //     // Implement beeping logic
            // }

        } else {
            Serial.println("Failed to obtain time");
        }
        // Handle Wi-Fi disconnections
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("Wi-Fi disconnected! Attempting to reconnect...");
            WiFi.reconnect();
            int retries = 20;
            while (WiFi.status() != WL_CONNECTED && retries-- > 0) {
                delay(500);
                Serial.print(".");
            }
            Serial.println();

            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("Reconnected to Wi-Fi");
                // Reconfigure time
                String timeZoneStr = preferences.getString("timeZoneString", "UTC0");
                configTzTime(timeZoneStr.c_str(), "pool.ntp.org", "time.nist.gov");
                timeReceived = true;

                // Restart mDNS
                MDNS.end();
                if (!MDNS.begin(deviceName.c_str())) {
                    Serial.println("Error setting up mDNS responder!");
                } else {
                    Serial.println("mDNS responder restarted");
                    MDNS.addService("http", "tcp", 80);
                }
            } else {
                Serial.println("Failed to reconnect to Wi-Fi");
            }
        }
    }

    if (!setup_device){
      int touchValue = touchRead(TOUCH_PIN);
      // int extra_touchValue = touchRead(extra_touch);
      unsigned long currentMillis = millis();

      // Check if the touch value is below the threshold and the debounce time has passed
      if (touchValue < 77 && !touchTriggered && (currentMillis - lastTouchTime > 3000)) {
          lastTouchTime = currentMillis;  // Reset debounce timer
          touchTriggered = true;  // Set the flag to prevent repeated triggering

          if (audio.isRunning()) {
              Serial.println("Stopping audio stream...");
              audio.stopSong();  // Stop audio function
          } else {
              Serial.println("Starting audio stream...");
              audio.connecttohost(streamURL.c_str());  // Start audio function
          }
      }

      // Reset the flag when the touch value goes above the threshold
      if (touchValue > 78) {
          touchTriggered = false;  // Allow new detection when the user removes their hand
      }
      // Read the light sensor every 15 seconds
      if (currentMillis - lastLightReadingTime >= LIGHT_READING_INTERVAL) {
          lastLightReadingTime = currentMillis;
          int lightValue = analogRead(PHOTORESISTOR_PIN);
          if (autoBrightnessEnabled) {
              adjustBrightness(lightValue);
              recordLightReading(lightValue);
          } else {
              // Optionally, maintain the user-set brightness or handle manual brightness adjustments
          }
      }
    }
    delay(10);
}

void handleGetAutoBrightness(AsyncWebServerRequest *request) {
    StaticJsonDocument<100> jsonResponse;
    jsonResponse["autoBrightnessEnabled"] = autoBrightnessEnabled;
    String jsonResponseStr;
    serializeJson(jsonResponse, jsonResponseStr);
    request->send(200, "application/json", jsonResponseStr);
}

void handleSetAutoBrightness(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    static String body;
    if (index == 0) {
        body = "";
        body.reserve(total);
    }
    body += String((char*)data).substring(0, len);

    if (index + len == total) {
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, body);
        if (!error) {
            if (doc.containsKey("autoBrightnessEnabled")) {
                autoBrightnessEnabled = doc["autoBrightnessEnabled"].as<bool>();
                preferences.putBool("autoBrightnessEnabled", autoBrightnessEnabled);

                // Send response
                StaticJsonDocument<100> jsonResponse;
                jsonResponse["autoBrightnessEnabled"] = autoBrightnessEnabled;
                String jsonResponseStr;
                serializeJson(jsonResponse, jsonResponseStr);
                request->send(200, "application/json", jsonResponseStr);

                Serial.printf("Auto brightness set to %s\n", autoBrightnessEnabled ? "enabled" : "disabled");

                // Optionally, if auto brightness is disabled, set brightness to last user-set value
                if (!autoBrightnessEnabled && userSetBrightness >= 0) {
                    setBrightness(userSetBrightness);
                    displayTime(currentHour, currentMinute);
                }

            } else {
                request->send(400, "application/json", "{\"error\":\"Missing 'autoBrightnessEnabled' field\"}");
            }
        } else {
            request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        }
    }
}

void adjustBrightness(int lightValue) {
    int avgLight = calculateAverageLight();
    int newBrightness;

    int matchingBrightness = getMatchingBrightness(avgLight);
    if (matchingBrightness != -1) {
        newBrightness = matchingBrightness;
        // Use the user's mapped brightness
        // Serial.printf("Using mapped brightness %d for sensor reading %d\n", newBrightness, avgLight);
    } else {
        // Map light to brightness automatically
        newBrightness = mapLightToBrightness(avgLight);
        // Serial.printf("Auto-calculated brightness %d for sensor reading %d\n", newBrightness, avgLight);
    }

    // Update brightness if it has changed significantly
    if (abs(newBrightness - ledBrightness) >= 5) {
        setBrightness(newBrightness);
        ledBrightness = newBrightness;
        displayTime(currentHour, currentMinute);
    }
}

int findBrightnessMappingIndex(int sensorReading) {
    int count = min(brightnessMappingCount, MAX_BRIGHTNESS_MAPPINGS);
    for (int i = 0; i < count; i++) {
        int storedSensorReading = brightnessMappings[i].sensorReading;
        if (abs(sensorReading - storedSensorReading) <= SENSOR_READING_THRESHOLD) {
            return i; // Return the index of the existing mapping
        }
    }
    return -1; // No existing mapping found
}


int mapLightToBrightness(int lightValue) {
    // Calculate the average light over the last readings
    int avgLight = calculateAverageLight();

    // Map the average light value to brightness (0-255)
    int brightness = map(avgLight, 4095, 0, MIN_BRIGHTNESS, MAX_BRIGHTNESS);

    // Use adaptive brightness based on user history
    return getAdaptiveBrightness(avgLight, brightness);
}

int calculateAverageLight() {
    long sum = 0;
    for (int i = 0; i < 12; i++) {
        sum += lightReadings[i];
    }
    return sum / 12;
}

int getMatchingBrightness(int sensorReading) {
    int count = min(brightnessMappingCount, MAX_BRIGHTNESS_MAPPINGS);
    for (int i = 0; i < count; i++) {
        int storedSensorReading = brightnessMappings[i].sensorReading;
        if (abs(sensorReading - storedSensorReading) <= SENSOR_READING_THRESHOLD) {
            // Found a matching sensor reading within threshold
            // Serial.printf("Match found: Current Sensor = %d, Stored Sensor = %d, Brightness = %d\n", sensorReading, storedSensorReading, brightnessMappings[i].brightness);
            return brightnessMappings[i].brightness;
        }
    }
    // No matching brightness found
    return -1;
}

void recordLightReading(int lightValue) {
    // Serial.printf("Recorded light reading: %d\n", lightValue);
    lightReadings[lightReadingIndex] = lightValue;
    lightReadingIndex = (lightReadingIndex + 1) % 12;  // Circular buffer
}

int getAdaptiveBrightness(int lightValue, int mappedBrightness) {
    int avgUserBrightness = calculateAverageBrightnessHistory();
    // If the mapped brightness is within 10% of the user average, use the user preference
    if (abs(mappedBrightness - avgUserBrightness) <= (MAX_BRIGHTNESS * 0.1)) {
        return avgUserBrightness;
    }
    return mappedBrightness;
}

int calculateAverageBrightnessHistory() {
    long sum = 0;
    for (int i = 0; i < 50; i++) {
        sum += brightnessHistory[i];
    }
    return sum / 50;
}

void recordUserBrightnessSetting(int brightness) {
    // Read current sensor value
    int sensorReading = analogRead(PHOTORESISTOR_PIN);

    // Check if a mapping for this sensor reading already exists
    int existingIndex = findBrightnessMappingIndex(sensorReading);
    if (existingIndex != -1) {
        // Update the existing mapping
        brightnessMappings[existingIndex].sensorReading = sensorReading; // Optional: Update sensor reading
        brightnessMappings[existingIndex].brightness = brightness;

        Serial.printf("Updated existing brightness mapping at index %d: Sensor Reading = %d, Brightness = %d\n", existingIndex, sensorReading, brightness);
    } else {
        // Add a new mapping (using modulo for circular buffer behavior)
        int index = brightnessMappingCount % MAX_BRIGHTNESS_MAPPINGS;
        brightnessMappings[index].sensorReading = sensorReading;
        brightnessMappings[index].brightness = brightness;
        brightnessMappingCount++;

        Serial.printf("Added new brightness mapping at index %d: Sensor Reading = %d, Brightness = %d\n", index, sensorReading, brightness);
    }

    // Save the mappings to Preferences
    saveBrightnessMappings();
    Serial.printf("Recorded user brightness setting: %d at sensor reading: %d\n", brightness, sensorReading);
}

void saveBrightnessMappings() {
    StaticJsonDocument<4096> doc; // Adjust size as needed
    JsonArray mappingsArray = doc.createNestedArray("mappings");

    int count = min(brightnessMappingCount, MAX_BRIGHTNESS_MAPPINGS);

    for (int i = 0; i < count; i++) {
        JsonObject mappingObj = mappingsArray.createNestedObject();
        mappingObj["sensorReading"] = brightnessMappings[i].sensorReading;
        mappingObj["brightness"] = brightnessMappings[i].brightness;
    }

    String jsonString;
    serializeJson(doc, jsonString);

    preferences.putString("brightnessMappings", jsonString);

    Serial.println("Brightness mappings saved to Preferences.");
}

void loadBrightnessMappings() {
    String jsonString = preferences.getString("brightnessMappings", "");
    if (jsonString.length() > 0) {
        StaticJsonDocument<4096> doc; // Adjust size as needed
        DeserializationError error = deserializeJson(doc, jsonString);
        if (!error) {
            JsonArray mappingsArray = doc["mappings"];
            brightnessMappingCount = 0;
            for (JsonObject mappingObj : mappingsArray) {
                if (brightnessMappingCount >= MAX_BRIGHTNESS_MAPPINGS) break;
                int sensorReading = mappingObj["sensorReading"];
                int brightness = mappingObj["brightness"];

                brightnessMappings[brightnessMappingCount].sensorReading = sensorReading;
                brightnessMappings[brightnessMappingCount].brightness = brightness;
                brightnessMappingCount++;
            }
            Serial.printf("Loaded %d brightness mappings from Preferences.\n", brightnessMappingCount);
        } else {
            Serial.println("Failed to parse brightness mappings from Preferences.");
        }
    } else {
        Serial.println("No brightness mappings found in Preferences.");
    }
}

void setBrightness(int brightness) {
    brightness = max(brightness, MINIMUM_EFFECTIVE_BRIGHTNESS);  // Ensure brightness doesn't go below the minimum
    ledBrightness = constrain(brightness, MIN_BRIGHTNESS, MAX_BRIGHTNESS);
    preferences.putUChar("ledBrightness", ledBrightness);
    strip.setBrightness(ledBrightness);
    strip.show();

    // Serial.print("Brightness set to: ");
    // Serial.println(ledBrightness);
}


// void notifyClientsBrightness() {
//     StaticJsonDocument<64> doc;
//     doc["brightness"] = ledBrightness;
//     String jsonString;
//     serializeJson(doc, jsonString);
//     wso.textAll(jsonString);
// }

// Update your handleGetFirmwareStatus function
void handleGetFirmwareStatus(AsyncWebServerRequest *request) {
    firmwareUpdateAvailable = checkForNewFirmware();
    StaticJsonDocument<100> jsonResponse;
    jsonResponse["updateAvailable"] = firmwareUpdateAvailable;
    String jsonResponseStr;
    serializeJson(jsonResponse, jsonResponseStr);
    request->send(200, "application/json", jsonResponseStr);
}


void handleRoot(AsyncWebServerRequest *request) {
    if (WiFi.getMode() == WIFI_AP) {
        request->send(200, "text/html; charset=UTF-8", clock_setup_html);
    } else {
        // Redirect to /time without checking for firmware here
        request->redirect("/time");
    }
}

void handleConnect(AsyncWebServerRequest *request) {
    if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
        ssid = request->getParam("ssid", true)->value();
        password = request->getParam("password", true)->value();

        // Redirect the user while still in AP mode
        request->redirect("/connecting");

        // Attempt to connect to the user-provided Wi-Fi
        WiFi.softAPdisconnect(true);
        delay(100);
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        WiFi.setHostname(deviceName.c_str());
        WiFi.begin(ssid.c_str(), password.c_str());

        int retries = 20;
        Serial.println("Attempting to connect to new Wi-Fi credentials...");
        while (WiFi.status() != WL_CONNECTED && retries-- > 0) {
            delay(500);
            Serial.print(".");
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("Connected to new Wi-Fi");

            // Start mDNS responder for the new network
            MDNS.end(); // Stop existing mDNS responder
            if (!MDNS.begin(deviceName.c_str())) {
                Serial.println("Error setting up mDNS responder!");
            } else {
                Serial.println("mDNS responder started");
                MDNS.addService("http", "tcp", 80);
            }

            // Configure time with the stored time zone
            String timeZoneStr = preferences.getString("timeZoneString", "UTC0");
            configTzTime(timeZoneStr.c_str(), "pool.ntp.org", "time.nist.gov");
            timeReceived = true;

        } else {
            Serial.println("Failed to connect to new Wi-Fi credentials.");
            // Restart the device to retry or handle the failure appropriately
            ESP.restart();
        }
    } else {
        request->send(400, "text/plain", "Missing SSID or Password");
    }
}

void handleConnecting(AsyncWebServerRequest *request) {
    request->send(200, "text/html", "<html><body><h1>Connecting to Wi-Fi...</h1><p>Please wait while the device attempts to connect to your Wi-Fi network.</p></body></html>");
}

void handleTimePage(AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse_P(
        200, "text/html", clock_time_html_gz, clock_time_html_gz_len
    );
    response->addHeader("Content-Type", "text/html; charset=UTF-8");  // Add charset
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
}


void handleGetCurrentTime(AsyncWebServerRequest *request) {
    if (WiFi.status() == WL_CONNECTED && timeReceived) {
        struct tm timeInfo;
        if (getLocalTime(&timeInfo)) {
            int currentHour = timeInfo.tm_hour;
            int currentMinute = timeInfo.tm_min;

            // Create a JSON response with the current hour and minute
            StaticJsonDocument<200> jsonResponse;
            jsonResponse["hour"] = currentHour;
            jsonResponse["minute"] = currentMinute;

            String jsonResponseStr;
            serializeJson(jsonResponse, jsonResponseStr);

            // Send JSON response to the client
            request->send(200, "application/json", jsonResponseStr);
        } else {
            // Failed to obtain time
            request->send(500, "application/json", "{\"error\":\"Failed to obtain time.\"}");
        }
    } else {
        // Time not received yet
        request->send(500, "application/json", "{\"error\":\"Time not received.\"}");
    }
}


void handleSetTimeZone(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    static String body;
    if (index == 0) {
        body = "";
        body.reserve(total);
    }
    body += String((char*)data).substring(0, len);

    if (index + len == total) {
        // Parse JSON
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, body);
        if (!error) {
            String timeZoneStr = doc["timeZoneString"].as<String>();
            preferences.putString("timeZoneString", timeZoneStr);

            // Reconfigure time with the new time zone
            configTzTime(timeZoneStr.c_str(), "pool.ntp.org", "time.nist.gov");
            displayTime(currentHour, currentMinute);

            // Send response
            StaticJsonDocument<100> jsonResponse;
            jsonResponse["timeZoneString"] = timeZoneStr;
            String jsonResponseStr;
            serializeJson(jsonResponse, jsonResponseStr);
            request->send(200, "application/json", jsonResponseStr);
            Serial.printf("Time zone set to %s\n", timeZoneStr.c_str());

            
        } else {
            request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        }
    }
}

void handleGetTimeZone(AsyncWebServerRequest *request) {
    String timeZoneStr = preferences.getString("timeZoneString", "UTC0");
    StaticJsonDocument<100> jsonResponse;
    jsonResponse["timeZoneString"] = timeZoneStr;
    String jsonResponseStr;
    serializeJson(jsonResponse, jsonResponseStr);
    request->send(200, "application/json", jsonResponseStr);
}

void handleSetDisplayMode(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    static String body;
    if (index == 0) {
        body = "";
        body.reserve(total);
    }
    body += String((char*)data).substring(0, len);

    if (index + len == total) {

        // Parse JSON
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, body);
        if (!error) {
            String mode = doc["mode"].as<String>();
            if (mode == "time" || mode == "countdown") {
                displayMode = mode;
                // Save to preferences
                preferences.putString("displayMode", displayMode);
                // Send a JSON response
                StaticJsonDocument<100> jsonResponse;
                jsonResponse["mode"] = displayMode;
                String jsonResponseStr;
                serializeJson(jsonResponse, jsonResponseStr);
                request->send(200, "application/json", jsonResponseStr);
                Serial.printf("Display mode set to %s\n", displayMode.c_str());
            } else {
            request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        }
      }
    }
    displayTime(currentHour, currentMinute);
}

void handleGetSettings(AsyncWebServerRequest *request) {
    StaticJsonDocument<1024> jsonResponse;
    jsonResponse["mode"] = displayMode;
    jsonResponse["brightness"] = ledBrightness;
    jsonResponse["volume"] = audioVolume;
    jsonResponse["isStreaming"] = isStreaming;
    jsonResponse["streamURL"] = streamURL;
    jsonResponse["enableWiFiOff"] = enableWiFiOff;
    jsonResponse["wifiOffStartTime"] = wifiOffStartTime;
    jsonResponse["wifiOffEndTime"] = wifiOffEndTime;
    jsonResponse["is24HourFormat"] = is24HourFormat;
    jsonResponse["alarms"] = JsonArray(); // Initialize alarms array
    jsonResponse["streamURL"] = streamURL;
    jsonResponse["timeZoneString"] = preferences.getString("timeZoneString", "UTC0");
    for (int i = 0; i < alarmCount; i++) {
        JsonObject alarmObj = jsonResponse["alarms"].createNestedObject();
        alarmObj["time"] = alarms[i].time;
        alarmObj["enabled"] = alarms[i].enabled;
    }
    // Add hour format
    jsonResponse["is24HourFormat"] = is24HourFormat;
    // Convert RGB color back to hex string
    uint8_t r = (ledColorRGB >> 16) & 0xFF;
    uint8_t g = (ledColorRGB >> 8) & 0xFF;
    uint8_t b = ledColorRGB & 0xFF;
    char colorHex[8];
    snprintf(colorHex, sizeof(colorHex), "#%02X%02X%02X", r, g, b);
    jsonResponse["color"] = String(colorHex);

    // Add digitColors to the response
    JsonArray digitColorsArray = jsonResponse.createNestedArray("digitColors");
    for (int i = 0; i < 4; i++) {
        if (digitColorsRGB[i] != 0) {
            uint8_t dr = (digitColorsRGB[i] >> 16) & 0xFF;
            uint8_t dg = (digitColorsRGB[i] >> 8) & 0xFF;
            uint8_t db = digitColorsRGB[i] & 0xFF;
            char digitColorHex[8];
            snprintf(digitColorHex, sizeof(digitColorHex), "#%02X%02X%02X", dr, dg, db);
            digitColorsArray.add(String(digitColorHex));
        } else {
            digitColorsArray.add(nullptr); // No individual color set
        }
    }

    // Add colonColor to the response
    JsonArray colonColorsArray = jsonResponse.createNestedArray("colonColors");
    if (colonColorRGB != 0) {
        uint8_t cr = (colonColorRGB >> 16) & 0xFF;
        uint8_t cg = (colonColorRGB >> 8) & 0xFF;
        uint8_t cb = colonColorRGB & 0xFF;
        char colonColorHex[8];
        snprintf(colonColorHex, sizeof(colonColorHex), "#%02X%02X%02X", cr, cg, cb);
        colonColorsArray.add(String(colonColorHex));
    } else {
        colonColorsArray.add(nullptr); // No individual color set
    }

    String jsonResponseStr;
    serializeJson(jsonResponse, jsonResponseStr);
    request->send(200, "application/json", jsonResponseStr);
}

void handleSetLEDSettings(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    static String body;
    if (index == 0) {
        body = "";
        body.reserve(total);
    }
    body += String((char*)data).substring(0, len);

    if (index + len == total) {
      StaticJsonDocument<300> doc;
      DeserializationError error = deserializeJson(doc, body);
      if (!error) {
          // Handle brightness
          if (doc.containsKey("brightness")) {
              int brightness = doc["brightness"];
              if (brightness >= MIN_BRIGHTNESS && brightness <= MAX_BRIGHTNESS) {
                  ledBrightness = brightness;
                  preferences.putUChar("ledBrightness", ledBrightness);
                  strip.setBrightness(ledBrightness);
                  strip.show();

                  if (!autoBrightnessEnabled) {
                      // Save the user-set brightness
                      userSetBrightness = brightness;
                  } else {
                      // Record the brightness mapping for auto brightness
                      manualBrightnessOverride = true;
                      recordUserBrightnessSetting(brightness);
                  }

                  Serial.printf("Manual brightness set to %d\n", brightness);
              } else {
                  request->send(400, "application/json", "{\"error\":\"Brightness out of range\"}");
                  return;
              }
          }

          // Handle color
          if (doc.containsKey("color")) {
              String hexColor = doc["color"];
              // Convert hex color to RGB
              if (hexColor.length() == 7 && hexColor[0] == '#') {
                  ledColorRGB = hexToRGB(hexColor);
                  preferences.putUInt("ledColorRGB", ledColorRGB);

                  // Reset individual digit and colon colors
                  for (int i = 0; i < 4; i++) {
                      digitColorsRGB[i] = 0;
                      char key[12];
                      snprintf(key, sizeof(key), "digitColor%d", i);
                      if (preferences.isKey(key)) {  // Check if the key exists before removing
                          preferences.remove(key);
                      }
                  }

                  colonColorRGB = 0;
                  if (preferences.isKey("colonColor0")) {
                    preferences.remove("colonColor0");
                  }

              } else {
                  request->send(400, "application/json", "{\"error\":\"Invalid color format\"}");
                  return;
              }
          }
          // Send a JSON response
          StaticJsonDocument<100> jsonResponse;
          jsonResponse["brightness"] = ledBrightness;
          jsonResponse["color"] = doc["color"].as<String>();
          String jsonResponseStr;
          serializeJson(jsonResponse, jsonResponseStr);
          request->send(200, "application/json", jsonResponseStr);
          Serial.printf("LED settings updated: Brightness=%d, Color=%s\n", ledBrightness, doc["color"].as<String>().c_str());
          // Update the display to reflect new settings
          // displayTime(currentHour, currentMinute);
      } else {
              request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
          }
      }
  displayTime(currentHour, currentMinute);
}

void handleGetLEDSettings(AsyncWebServerRequest *request) {
    StaticJsonDocument<300> jsonResponse;
    jsonResponse["brightness"] = ledBrightness;
    // Convert RGB color back to hex string
    uint8_t r = (ledColorRGB >> 16) & 0xFF;
    uint8_t g = (ledColorRGB >> 8) & 0xFF;
    uint8_t b = ledColorRGB & 0xFF;
    char colorHex[8];
    snprintf(colorHex, sizeof(colorHex), "#%02X%02X%02X", r, g, b);
    jsonResponse["color"] = String(colorHex);
    String jsonResponseStr;
    serializeJson(jsonResponse, jsonResponseStr);
    request->send(200, "application/json", jsonResponseStr);
}

void handleSetDigitColor(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    static String body;
    if (index == 0) {
        body = "";
        body.reserve(total);
    }
    body += String((char*)data).substring(0, len);

    if (index + len == total) {
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, body);
      if (!error) {
          String type = doc["type"].as<String>();
          int index = doc["index"];
          String hexColor = doc["color"].as<String>();

          if (hexColor.length() == 7 && hexColor[0] == '#') {
              uint32_t colorValue = hexToRGB(hexColor);
              if (type == "digit" && index >= 0 && index < 4) {
                  // Update digit color
                  digitColorsRGB[index] = colorValue;
                  // Save to preferences
                  char key[12];
                  snprintf(key, sizeof(key), "digitColor%d", index);
                  preferences.putUInt(key, digitColorsRGB[index]);
              } else if (type == "colon" && index == 0) {
                  // Update colon color
                  colonColorRGB = colorValue;
                  // Save to preferences
                  preferences.putUInt("colonColor0", colonColorRGB);
              } else {
                  request->send(400, "application/json", "{\"error\":\"Invalid index or type\"}");
                  return;
              }
              // Send a JSON response
              StaticJsonDocument<100> jsonResponse;
              jsonResponse["type"] = type;
              jsonResponse["index"] = index;
              jsonResponse["color"] = hexColor;
              String jsonResponseStr;
              serializeJson(jsonResponse, jsonResponseStr);
              request->send(200, "application/json", jsonResponseStr);
              Serial.printf("%s %d color set to %s\n", type.c_str(), index, hexColor.c_str());

              // Update the display immediately
              displayTime(currentHour, currentMinute);
          }
        } else {
            request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        }
    }
}

void handleSetAudioSettings(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    static String body;
    if (index == 0) {
        body = "";
        body.reserve(total);
    }
    body += String((char*)data).substring(0, len);

    if (index + len == total) {
        StaticJsonDocument<500> doc; // Increased size to accommodate longer URLs
        DeserializationError error = deserializeJson(doc, body);
        if (!error) {
            // Handle volume
            if (doc.containsKey("volume")) {
                int volume = doc["volume"];
                if (volume >= 0 && volume <= 21) {
                    audioVolume = volume;
                    preferences.putInt("audioVolume", audioVolume);
                    audio.setVolume(audioVolume);
                    Serial.printf("Audio volume set to %d\n", audioVolume);
                } else {
                    request->send(400, "application/json", "{\"error\":\"Volume out of range\"}");
                    return;
                }
            }

            // Handle streamName
            if (doc.containsKey("streamName")) {
                String newStreamName = doc["streamName"].as<String>();
                if (newStreamName.length() > 0) {
                    streamName = newStreamName;
                    preferences.putString("streamName", streamName);
                    Serial.printf("Stream NAME set to %s\n", streamName.c_str());
                }
            }

            // Handle streamURL
            if (doc.containsKey("streamURL")) {
                String newStreamURL = doc["streamURL"].as<String>();
                if (newStreamURL.length() > 0) {
                    streamURL = newStreamURL;
                    preferences.putString("streamURL", streamURL);
                    Serial.printf("Stream URL set to %s\n", streamURL.c_str());
                    // If streaming, restart the stream
                    if (audio.isRunning()) {
                        audio.stopSong();
                        delay(75);
                        audio.connecttohost(streamURL.c_str());
                    }
                    else{
                      audio.connecttohost(streamURL.c_str());
                    }
                }
            }

            // Handle isStreaming
            if (doc.containsKey("isStreaming")) {
                Serial.print("Streaming state toggled: ");
                bool newStreamingState = doc["isStreaming"];
                if (newStreamingState != isStreaming) {
                    isStreaming = newStreamingState;
                    preferences.putBool("isStreaming", isStreaming);
                    if (isStreaming) {
                        if (!audio.isRunning()) {
                            audio.connecttohost(streamURL.c_str());
                        }
                    } else {
                        if (audio.isRunning()) {
                            audio.stopSong();
                        }
                    }
                }
            }

            // Send a JSON response
            StaticJsonDocument<200> jsonResponse;
            jsonResponse["volume"] = audioVolume;
            jsonResponse["streamURL"] = streamURL;
            jsonResponse["streamName"] = streamName;
            jsonResponse["isStreaming"] = isStreaming;
            String jsonResponseStr;
            serializeJson(jsonResponse, jsonResponseStr);
            request->send(200, "application/json", jsonResponseStr);
          } else {
            request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        }
    }
}

void handleGetAudioSettings(AsyncWebServerRequest *request) {
    StaticJsonDocument<200> jsonResponse;
    jsonResponse["volume"] = audioVolume;
    jsonResponse["isStreaming"] = isStreaming;
    jsonResponse["streamURL"] = streamURL;
    jsonResponse["streamName"] = streamName;
    String jsonResponseStr;
    serializeJson(jsonResponse, jsonResponseStr);
    request->send(200, "application/json", jsonResponseStr);
}

void saveAlarms() {
    StaticJsonDocument<4096> doc; // Adjust size if necessary
    JsonArray alarmArray = doc.createNestedArray("alarms");
    for (int i = 0; i < alarmCount; i++) {
        JsonObject alarmObj = alarmArray.createNestedObject();
        alarmObj["time"] = alarms[i].time;
        alarmObj["enabled"] = alarms[i].enabled;
        alarmObj["volume"] = alarms[i].volume;
        alarmObj["streamURL"] = alarms[i].streamURL;
        // Add 'days' as an array
        JsonArray daysArray = alarmObj.createNestedArray("days");
        for (int day = 0; day < 7; day++) {
            if (alarms[i].days & (1 << day)) {
                daysArray.add(day);
            }
        }
    }

    String output;
    serializeJson(doc, output);
    preferences.putString("alarms", output);
    Serial.println("Alarms saved to Preferences.");
    
}

bool addAlarm(String time, bool enabled) {
    if (alarmCount >= MAX_ALARMS) {
        Serial.println("Maximum number of alarms reached.");
        return false;
    }
    // Basic validation for time format "HH:MM"
    if (time.length() != 5 || time.charAt(2) != ':') {
        Serial.println("Invalid time format. Use 'HH:MM'.");
        return false;
    }
    // Check for duplicate alarms
    for (int i = 0; i < alarmCount; i++) {
        if (alarms[i].time == time) {
            Serial.println("Duplicate alarm time detected.");
            return false;
        }
    }
    alarms[alarmCount].time = time;
    alarms[alarmCount].enabled = enabled;
    alarms[alarmCount].triggered = false;
    alarmCount++;
    saveAlarms();
    return true;
}

bool removeAlarm(int index) {
    if (index < 0 || index >= alarmCount) {
        Serial.println("Invalid alarm index.");
        return false;
    }
    for (int i = index; i < alarmCount - 1; i++) {
        alarms[i] = alarms[i + 1];
    }
    alarmCount--;
    saveAlarms();
    return true;
}

void triggerAlarm(int index) {
    if (index < 0 || index >= alarmCount) return;

    Serial.printf("Triggering Alarm %d at %s\n", index + 1, alarms[index].time.c_str());

    // Set the volume to the alarm's volume
    audio.setVolume(alarms[index].volume);
    isStreaming = true;
    preferences.putBool("isStreaming", isStreaming);
    audioVolume = alarms[index].volume;
    preferences.putInt("audioVolume", audioVolume);

    alarmStreamURL = alarms[index].streamURL;
    if (alarmStreamURL.length() == 0) {
        // Use the main streamURL from preferences
        alarmStreamURL = streamURL;
    }

    // Start streaming audio with the alarm's streamURL
    if (audio.isRunning()) {
        audio.stopSong();
        delay(100);
        audio.connecttohost(alarms[index].streamURL.c_str());
    }
    if (!audio.isRunning()) {
        audio.connecttohost(alarms[index].streamURL.c_str());
    }
    

    // Additional actions can be implemented here, such as flashing LEDs, notifications, etc.
}


void loadAlarms() {
    String alarmsString = preferences.getString("alarms", "");
    if (alarmsString.length() > 0) {
        StaticJsonDocument<4096> doc; // Adjust size if necessary
        DeserializationError error = deserializeJson(doc, alarmsString);
        if (!error) {
            alarmCount = 0; // Reset count before loading
            JsonArray alarmsArray = doc["alarms"];
            for (JsonObject alarmObj : alarmsArray) {
                if (alarmCount >= MAX_ALARMS) break;
                String time = alarmObj["time"].as<String>();
                bool enabled = alarmObj["enabled"].as<bool>();

                // Read 'days' array
                uint8_t daysBitmask = 0;
                if (alarmObj.containsKey("days")) {
                    JsonArray daysArray = alarmObj["days"];
                    for (JsonVariant dayValue : daysArray) {
                        int dayIndex = dayValue.as<int>();
                        if (dayIndex >= 0 && dayIndex <= 6) {
                            daysBitmask |= (1 << dayIndex);
                        }
                    }
                } else {
                    // Default to all days
                    daysBitmask = 0b01111111; // All days
                }

                // Read new properties with defaults
                int volume = alarmObj["volume"] | audioVolume; // Default to current global volume
                alarmStreamURL = alarmObj["streamURL"] | ""; // Default to current global streamURL
                
                // If the alarm's streamURL is empty, default to the main streamURL
                if (alarmStreamURL.length() == 0) {
                    alarmStreamURL = streamURL;
                }

                // Basic validation
                if (time.length() == 5 && time.charAt(2) == ':') {
                    alarms[alarmCount].time = time;
                    alarms[alarmCount].enabled = enabled;
                    alarms[alarmCount].days = daysBitmask;
                    alarms[alarmCount].volume = volume;
                    alarms[alarmCount].streamURL = streamURL;
                    alarms[alarmCount].triggered = false; // Reset triggered flag
                    alarmCount++;
                }
            }
            Serial.println("Alarms loaded from Preferences.");
        } else {
            Serial.println("Failed to parse alarms from Preferences.");
            alarmCount = 0; // Reset count on failure
        }
    } else {
        alarmCount = 0; // No alarms saved
        Serial.println("No alarms found in Preferences.");
    }
}

void handleGetAlarms(AsyncWebServerRequest *request) {
    StaticJsonDocument<4096> jsonResponse;
    JsonArray alarmArray = jsonResponse.createNestedArray("alarms");
    for (int i = 0; i < alarmCount; i++) {
        JsonObject alarmObj = alarmArray.createNestedObject();
        alarmObj["time"] = alarms[i].time;
        alarmObj["enabled"] = alarms[i].enabled;

        // Convert 'days' bitmask to array
        JsonArray daysArray = alarmObj.createNestedArray("days");
        for (int day = 0; day < 7; day++) {
            if (alarms[i].days & (1 << day)) {
                daysArray.add(day);
            }
        }

        // Add new properties
        alarmObj["volume"] = alarms[i].volume;
        alarmObj["streamURL"] = alarms[i].streamURL;
    }
    String response;
    serializeJson(jsonResponse, response);
    request->send(200, "application/json", response);
}

void handleSetAlarms(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    static String body;
    if (index == 0) {
        body = "";
        body.reserve(total);
    }
    body += String((char*)data).substring(0, len);

    if (index + len == total) {
        StaticJsonDocument<4096> doc; // Increase size if necessary
        DeserializationError error = deserializeJson(doc, body);
        if (!error) {
            // Expecting an array of alarms in the JSON
            JsonArray alarmsArray = doc["alarms"];
            if (alarmsArray.isNull()) {
                request->send(400, "application/json", "{\"error\":\"Invalid alarms format\"}");
                return;
            }

            // Clear existing alarms
            alarmCount = 0;
            for (JsonObject alarmObj : alarmsArray) {
                if (alarmCount >= MAX_ALARMS) break;
                String time = alarmObj["time"].as<String>();
                bool enabled = alarmObj["enabled"].as<bool>();

                // Read 'days' array
                uint8_t daysBitmask = 0;
                if (alarmObj.containsKey("days")) {
                    JsonArray daysArray = alarmObj["days"];
                    if (!daysArray.isNull()) {
                        for (JsonVariant dayValue : daysArray) {
                            int dayIndex = dayValue.as<int>();
                            if (dayIndex >= 0 && dayIndex <= 6) {
                                daysBitmask |= (1 << dayIndex);
                            }
                        }
                    }
                }
                if (daysBitmask == 0) {
                    // Default to all days if 'days' is missing or empty
                    daysBitmask = 0b01111111;
                }

                // Read new properties with defaults
                int volume = alarmObj["volume"] | audioVolume; // Default to current global volume
                alarmStreamURL = alarmObj["streamURL"] | ""; // Read the streamURL from the alarm object

                // If the alarm's streamURL is empty, default to the main streamURL
                if (alarmStreamURL.length() == 0) {
                    alarmStreamURL = streamURL; // Use the global streamURL
                }

                // Basic validation
                if (time.length() == 5 && time.charAt(2) == ':') {
                    // Prevent duplicate alarms (based on time and days)
                    bool duplicate = false;
                    for (int i = 0; i < alarmCount; i++) {
                        if (alarms[i].time == time && alarms[i].days == daysBitmask) {
                            duplicate = true;
                            break;
                        }
                    }
                    if (!duplicate) {
                        alarms[alarmCount].time = time;
                        alarms[alarmCount].enabled = enabled;
                        alarms[alarmCount].days = daysBitmask;
                        alarms[alarmCount].volume = volume;
                        alarms[alarmCount].streamURL = streamURL;
                        alarms[alarmCount].triggered = false;
                        alarmCount++;
                    }
                }
            }

            saveAlarms();

            // Send a JSON response
            StaticJsonDocument<200> jsonResponse;
            jsonResponse["status"] = "success";
            jsonResponse["alarmCount"] = alarmCount;
            String jsonResponseStr;
            serializeJson(jsonResponse, jsonResponseStr);
            request->send(200, "application/json", jsonResponseStr);
            Serial.println("Alarms updated via web interface.");
        } else {
            request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        }
    }
}

void handleSetStreamState(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    static String body;
    if (index == 0) {
        body = "";
        body.reserve(total);
    }
    body += String((char*)data).substring(0, len);

    if (index + len == total) {

        // Parse JSON
        StaticJsonDocument<100> doc;
        DeserializationError error = deserializeJson(doc, body);
        if (!error) {
            bool newState = doc["isStreaming"];
            isStreaming = newState;
            preferences.putBool("isStreaming", isStreaming);
            Serial.println(isStreaming);
            if (isStreaming) {
                if (!audio.isRunning()) {
                    audio.connecttohost(streamURL.c_str());
                }
            } else {
                if (audio.isRunning()) {
                    audio.stopSong();
                }
            }

            StaticJsonDocument<100> jsonResponse;
            jsonResponse["isStreaming"] = isStreaming;
            String jsonResponseStr;
            serializeJson(jsonResponse, jsonResponseStr);
            request->send(200, "application/json", jsonResponseStr);
        } else {
            request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        }
    }
}


void handleSetWiFiSettings(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    static String body;
    if (index == 0) {
        body = "";
        body.reserve(total);
    }
    body += String((char*)data).substring(0, len);

    if (index + len == total) {
        StaticJsonDocument<300> doc;
        DeserializationError error = deserializeJson(doc, body);
        if (!error) {
            // Handle SSID and Password
            if (doc.containsKey("ssid") && doc.containsKey("password")) {
                String newSSID = doc["ssid"].as<String>();
                String newPassword = doc["password"].as<String>();
                // Save to preferences
                preferences.putString("ssid", newSSID);
                preferences.putString("password", newPassword);
                ssid = newSSID;
                password = newPassword;
                // Attempt to connect to the new Wi-Fi credentials
                WiFi.setHostname(deviceName.c_str());
                WiFi.begin(ssid.c_str(), password.c_str());
                int retries = 20;
                while (WiFi.status() != WL_CONNECTED && retries-- > 0) {
                    delay(500);
                    Serial.print(".");
                }
                Serial.println();
                if (WiFi.status() == WL_CONNECTED) {
                    Serial.println("Connected to new Wi-Fi");
                    // Reconfigure time
                    String timeZoneStr = preferences.getString("timeZoneString", "UTC0");
                    configTzTime(timeZoneStr.c_str(), "pool.ntp.org", "time.nist.gov");
                    timeReceived = true;
                } else {
                    Serial.println("Failed to connect to new Wi-Fi credentials.");
                    // Handle connection failure if necessary
                }
            }

            // Handle Wi-Fi Off Schedule
            enableWiFiOff = doc["enableWiFiOff"] | false;
            wifiOffStartTime = doc["wifiOffStartTime"].as<String>();
            wifiOffEndTime = doc["wifiOffEndTime"].as<String>();
            preferences.putBool("enableWiFiOff", enableWiFiOff);
            preferences.putString("wifiOffStartTime", wifiOffStartTime);
            preferences.putString("wifiOffEndTime", wifiOffEndTime);

            // Serial Output
            Serial.println("Wi-Fi Off settings updated:");
            Serial.printf("enableWiFiOff: %s\n", enableWiFiOff ? "true" : "false");
            Serial.printf("wifiOffStartTime: %s\n", wifiOffStartTime.c_str());
            Serial.printf("wifiOffEndTime: %s\n", wifiOffEndTime.c_str());

            // Send response
            StaticJsonDocument<200> jsonResponse;
            jsonResponse["enableWiFiOff"] = enableWiFiOff;
            jsonResponse["wifiOffStartTime"] = wifiOffStartTime;
            jsonResponse["wifiOffEndTime"] = wifiOffEndTime;
            String jsonResponseStr;
            serializeJson(jsonResponse, jsonResponseStr);
            request->send(200, "application/json", jsonResponseStr);
          } else {
            request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        }
    }
}

void handleGetIPAddress(AsyncWebServerRequest *request) {
    StaticJsonDocument<100> jsonResponse;
    if (WiFi.status() == WL_CONNECTED) {
        jsonResponse["ip"] = WiFi.localIP().toString();
    } else {
        jsonResponse["ip"] = "Not Connected";
    }
    String jsonResponseStr;
    serializeJson(jsonResponse, jsonResponseStr);
    request->send(200, "application/json", jsonResponseStr);
}

void handleGetWiFiSettings(AsyncWebServerRequest *request) {
    StaticJsonDocument<100> jsonResponse;
    jsonResponse["enableWiFiOff"] = enableWiFiOff;
    jsonResponse["wifiOffStartTime"] = wifiOffStartTime;
    jsonResponse["wifiOffEndTime"] = wifiOffEndTime;
    String jsonResponseStr;
    serializeJson(jsonResponse, jsonResponseStr);
    request->send(200, "application/json", jsonResponseStr);
}

void displayTime(int hours, int minutes) {
    bool isPM = false;
    int displayHours = hours; // Copy of hours to manipulate

    if (displayMode == "time") {
        if (!is24HourFormat) {
            // 12-hour format adjustments
            if (hours == 0) {
                displayHours = 12;
            } else if (hours >= 12) {
                if (hours > 12) {
                    displayHours = hours - 12;
                } else {
                    displayHours = hours;
                }
                isPM = true;
            } else {
                displayHours = hours;
            }
        } else {
            displayHours = hours;
        }
    } else if (displayMode == "countdown") {
        // Calculate time remaining until midnight
        int hoursLeft = 23 - hours;
        int minutesLeft = 59 - minutes;

        // Adjust for rollover
        if (minutesLeft < 0) {
            minutesLeft += 60;
            hoursLeft -= 1;
        }
        if (hoursLeft < 0) hoursLeft = 23;

        displayHours = hoursLeft;
        minutes = minutesLeft;
    }

    int first = displayHours / 10;       // Tens place of hours
    int second = displayHours % 10;      // Ones place of hours
    int third = minutes / 10;            // Tens place of minutes
    int fourth = minutes % 10;           // Ones place of minutes

    bool hideFirstDigit = false;

    // Determine whether to hide the first digit
    if (first == 0) {
        if (displayMode == "countdown" && disableZeroCountdown) {
            hideFirstDigit = true;
        } else if (displayMode == "time") {
            if (is24HourFormat && disableZero24hr) {
                hideFirstDigit = true;
            } else if (!is24HourFormat && disableZero12hr) {
                hideFirstDigit = true;
            }
        }
    }

    if (hideFirstDigit) {
        clearDigit(0);
    } else {
        displayDigit(first, 0);
    }

    displayDigit(second, 1);  // Position 1
    displayDigit(third, 2);   // Position 2
    displayDigit(fourth, 3);  // Position 3

    // Display colon
    displayColon();

    // Refresh strip
    strip.show();
}


void clearDigit(int position) {
    int startIndex = position * 7;  // Calculate starting index for the digit

    for (int i = 0; i < 7; i++) {
        strip.setPixelColor(startIndex + i, strip.Color(0, 0, 0));  // Turn off segment
    }
}

void displayDigit(int digit, int position) {
    int startIndex = position * 7;  // Calculate starting index for the digit

    uint32_t color = digitColorsRGB[position];
    if (color == 0) {
        color = ledColorRGB; // Use global color if digit color is not set
    }

    for (int i = 0; i < 7; i++) {
        if (digitMap[digit][i] == 1) {
            strip.setPixelColor(startIndex + i, color);  // Turn on segment
        } else {
            strip.setPixelColor(startIndex + i, strip.Color(0, 0, 0));  // Turn off segment
        }
    }
}

void displayColon() {
    uint32_t color = colonColorRGB;
    if (color == 0) {
        color = ledColorRGB; // Use global color if colon color is not set
    }

    strip.setPixelColor(28, color);     // First dot of colon (LED index 28)
    strip.setPixelColor(29, color);     // Second dot of colon (LED index 29)
}

// Handler to get disable zero settings
void handleGetDisableZeroSettings(AsyncWebServerRequest *request) {
    StaticJsonDocument<200> jsonResponse;
    jsonResponse["disable24hr"] = disableZero24hr;
    jsonResponse["disable12hr"] = disableZero12hr;
    jsonResponse["disableCountdown"] = disableZeroCountdown;
    String jsonResponseStr;
    serializeJson(jsonResponse, jsonResponseStr);
    request->send(200, "application/json", jsonResponseStr);
}

// Handler to set disable zero settings
void handleSetDisableZeroSettings(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    static String body;
    if (index == 0) {
        body = "";
        body.reserve(total);
    }
    body += String((char*)data).substring(0, len);

    if (index + len == total) {
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, body);
        if (!error) {
            // Update settings based on received JSON
            if (doc.containsKey("disable24hr")) {
                disableZero24hr = doc["disable24hr"].as<bool>();
                preferences.putBool("disableZero24hr", disableZero24hr);
            }
            if (doc.containsKey("disable12hr")) {
                disableZero12hr = doc["disable12hr"].as<bool>();
                preferences.putBool("disableZero12hr", disableZero12hr);
            }
            if (doc.containsKey("disableCountdown")) {
                disableZeroCountdown = doc["disableCountdown"].as<bool>();
                preferences.putBool("disableZeroCountdown", disableZeroCountdown);
            }

            // Send a JSON response
            StaticJsonDocument<200> jsonResponse;
            jsonResponse["disable24hr"] = disableZero24hr;
            jsonResponse["disable12hr"] = disableZero12hr;
            jsonResponse["disableCountdown"] = disableZeroCountdown;
            String jsonResponseStr;
            serializeJson(jsonResponse, jsonResponseStr);
            request->send(200, "application/json", jsonResponseStr);
            Serial.println("Disable zero settings updated.");

            // Update the display to reflect the new settings
            displayTime(currentHour, currentMinute);
        } else {
            request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        }
    }
}

uint32_t hexToRGB(const String& hexColor) {
    const char* cstr = hexColor.c_str();
    unsigned long colorValue = strtoul(cstr + 1, NULL, 16); // Skip the '#' character
    return colorValue;
}
