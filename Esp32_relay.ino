#include <EEPROM.h>
#include <WiFiManager.h>
#include <FirebaseESP32.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

// Firebase credentials
#define FIREBASE_HOST "your_db_url"
#define FIREBASE_AUTH "your_api_key"


#define FIREBASE_EMAIL "your_email"
#define FIREBASE_PASSWORD "Your_pass"

// GPIO pin definitions for buttons and devices 13, 12, 14, 27, 26, 25, 33, 32
const int buttonPins[8] = {32, 33, 25, 26, 27, 14, 12, 13};
const int devicePins[8] = {23, 22, 21, 19, 18, 5, 17, 16};
const int ledPin = 2;  // LED pin (GPIO 2)

// State variables to track the status of each button and device
bool buttonStates[8] = {LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW};
bool lastButtonStates[8] = {LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW};
bool deviceStates[8] = {LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW};

// EEPROM memory addresses for storing the state of each device
const int EEPROM_ADDRESSES[8] = {0, 1, 2, 3, 4, 5, 6, 7};

// Firebase objects for authentication and configuration
FirebaseData firebaseData;
FirebaseConfig config;
FirebaseAuth auth;

// NTP client for time retrieval
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org",0, 60000); // Bangladesh (BST) = UTC +6 hours

// Firebase sync interval (to avoid overwhelming Firebase with requests)
unsigned long lastFirebaseSync = 0;
const unsigned long firebaseSyncInterval = 2500; // Sync every 2.5 seconds

// Function declarations for better code structure and readability
void saveStateToEEPROM(int channel, bool state);
bool loadStateFromEEPROM(int channel);
void syncWithFirebase(int channel, bool state);
void checkFirebaseUpdates();
void updateStatesFromFirebase();
void checkWiFiConnection();
unsigned long getEpochTime();
void logRelayStateChange(int channel, bool state);

void setup() {
  Serial.begin(115200);  // Initialize serial communication for debugging

  EEPROM.begin(512);  // Initialize EEPROM with 512 bytes of storage
  pinMode(ledPin, OUTPUT);  // Configure LED pin as output
  digitalWrite(ledPin, LOW);  // Ensure LED is off initially

  // Configure button and device pins
  for (int i = 0; i < 8; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);  // Enable internal pull-up resistors for buttons
    pinMode(devicePins[i], OUTPUT);        // Set device pins as outputs

    // Load and apply the last stored state from EEPROM
    deviceStates[i] = loadStateFromEEPROM(i);
    digitalWrite(devicePins[i], deviceStates[i] ? HIGH : LOW);
  }

  // Wi-Fi Manager handles the Wi-Fi connection via AP mode if needed
  WiFiManager wifiManager;
  wifiManager.setConnectTimeout(120);
  wifiManager.autoConnect("Smart Device");  // Device broadcasts AP named "Smart Device"
  
   // Configure Firebase settings
  config.host = FIREBASE_HOST;
  config.api_key = FIREBASE_AUTH;
  auth.user.email = FIREBASE_EMAIL;        // Set Firebase email
  auth.user.password = FIREBASE_PASSWORD;  // Set Firebase password
  Firebase.begin(&config, &auth);          // Initialize Firebase
  Firebase.reconnectWiFi(true);     // Automatically reconnect if Wi-Fi disconnects

  // Initialize Firebase paths if they don't exist
  for (int i = 0; i < 8; i++) {
    String path = "/deviceState" + String(i);
    if (!Firebase.pathExist(firebaseData, path)) {
      Firebase.setBool(firebaseData, path, deviceStates[i]);  // Set initial state
    }
  }

  timeClient.begin(); // Initialize NTP client
  updateStatesFromFirebase(); // Fetch initial states from Firebase
}

void loop() {
  // Check Wi-Fi connection and update LED status accordingly
  checkWiFiConnection();

  for (int i = 0; i < 8; i++) {
    buttonStates[i] = digitalRead(buttonPins[i]);  // Read the button state

    if (buttonStates[i] != lastButtonStates[i]) {  // If button state has changed
      delay(50);  // Debounce delay

      if (buttonStates[i] == LOW) {  // Button pressed
        deviceStates[i] = !deviceStates[i];  // Toggle device state
        digitalWrite(devicePins[i], deviceStates[i] ? HIGH : LOW);  // Update output pin

        saveStateToEEPROM(i, deviceStates[i]);
        logRelayStateChange(i, deviceStates[i]);

        if (WiFi.status() == WL_CONNECTED) {     
          syncWithFirebase(i, deviceStates[i]);
        }
      }

      lastButtonStates[i] = buttonStates[i];
    }
  }

  if (WiFi.status() == WL_CONNECTED && millis() - lastFirebaseSync > firebaseSyncInterval) {
    checkFirebaseUpdates();
    lastFirebaseSync = millis();
  }
}

// Save device state to EEPROM
void saveStateToEEPROM(int channel, bool state) {
  EEPROM.write(EEPROM_ADDRESSES[channel], state);
  EEPROM.commit();
  Serial.print("State for device ");
  Serial.print(channel);
  Serial.println(" saved to EEPROM");
}

// Load device state from EEPROM
bool loadStateFromEEPROM(int channel) {
  bool state = EEPROM.read(EEPROM_ADDRESSES[channel]);
  Serial.print("State for device ");
  Serial.print(channel);
  Serial.println(" loaded from EEPROM");
  return state;
}

// Sync device state with Firebase
void syncWithFirebase(int channel, bool state) {
  String path = "/deviceState" + String(channel);
  if (Firebase.setBool(firebaseData, path, state)) {
    Serial.print("State for device ");
    Serial.print(channel);
    Serial.println(" synced with Firebase");
    digitalWrite(ledPin, HIGH);  // LED on when synced
  } else {
    Serial.print("Failed to sync device ");
    Serial.print(channel);
    Serial.print(" with Firebase. Reason: ");
    Serial.println(firebaseData.errorReason());
    digitalWrite(ledPin, LOW);  // LED off if failed to sync
  }
}

// Check for updates from Firebase
void checkFirebaseUpdates() {
  for (int i = 0; i < 8; i++) {
    String path = "/deviceState" + String(i);
    if (Firebase.getBool(firebaseData, path)) {
      bool firebaseState = firebaseData.boolData();
      if (firebaseState != deviceStates[i]) {
        deviceStates[i] = firebaseState;
        digitalWrite(devicePins[i], firebaseState ? HIGH : LOW);
        saveStateToEEPROM(i, firebaseState);
        Serial.print("Device ");
        Serial.print(i);
        Serial.println(firebaseState ? " ON (from Firebase)" : " OFF (from Firebase)");
      }
    } else {
      Serial.print("Failed to load data for device ");
      Serial.print(i);
      Serial.print(" from Firebase. Reason: ");
      Serial.println(firebaseData.errorReason());
    }
  }
}

// Update device states from Firebase
void updateStatesFromFirebase() {
  for (int i = 0; i < 8; i++) {
    String path = "/deviceState" + String(i);
    if (Firebase.getBool(firebaseData, path)) {
      bool firebaseState = firebaseData.boolData();
      if (firebaseState != deviceStates[i]) {
        deviceStates[i] = firebaseState;
        digitalWrite(devicePins[i], firebaseState ? HIGH : LOW);
        saveStateToEEPROM(i, firebaseState);
        Serial.print("Device ");
        Serial.print(i);
        Serial.println(firebaseState ? " ON (Firebase Sync)" : " OFF (Firebase Sync)");
      }
    }
  }
}

// Get the current epoch time
unsigned long getEpochTime() {
  timeClient.update();  // Update time
  return timeClient.getEpochTime(); // Get the current epoch time
}

void logRelayStateChange(int channel, bool state) {
  unsigned long epochTime = getEpochTime();  // Get the epoch time
  String status = "relay" + String(channel + 1) + (state ? "_ON" : "_OFF");  // Define the relay state message

  // Create the path with the epoch time as the key
  String path = "/log/" + String(epochTime);

  // Save the log message (state) to Firebase at the path created by epoch time
  if (Firebase.setString(firebaseData, path, status)) {
    Serial.print("Log for device ");
    Serial.print(channel);
    Serial.print(" saved with timestamp: ");
    Serial.println(epochTime);
  } else {
    Serial.print("Failed to save log for device ");
    Serial.print(channel);
    Serial.print(" at timestamp: ");
    Serial.println(epochTime);
    Serial.print("Reason: ");
    Serial.println(firebaseData.errorReason());
  }
}

// Function to check Wi-Fi connection and update LED
void checkWiFiConnection() {
  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(ledPin, HIGH);  // LED on if Wi-Fi is connected
  } else {
    digitalWrite(ledPin, LOW);   // LED off if Wi-Fi is not connected
  }
}
