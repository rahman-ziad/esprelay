// Including necessary libraries
#include <EEPROM.h>            // Used to store and retrieve data even after a restart
#include <WiFiManager.h>        // Simplifies Wi-Fi setup using AP (Access Point) mode
#include <FirebaseESP32.h>      // Firebase library for ESP32

// Firebase credentials
#define FIREBASE_HOST "https://sdaaf-85d64-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define FIREBASE_AUTH "AIzaSyAPZTZ62nGvvghn8hWUmNWGQMMO8wpKI44"

// GPIO pin definitions for buttons and devices
const int buttonPins[4] = {4, 5, 12, 13};   // Input pins for wall/touch/push buttons
const int devicePins[4] = {2, 14, 16, 17};  // Output pins for controlling devices

// State variables to track the status of each button and device
bool buttonStates[4] = {LOW, LOW, LOW, LOW};       // Current state of each button
bool lastButtonStates[4] = {LOW, LOW, LOW, LOW};   // Previous state of each button
bool deviceStates[4] = {LOW, LOW, LOW, LOW};       // Current state of each device (ON/OFF)

// EEPROM memory addresses for storing the state of each device
const int EEPROM_ADDRESSES[4] = {0, 1, 2, 3};  // One address per device

// Firebase objects for authentication and configuration
FirebaseData firebaseData;
FirebaseConfig config;
FirebaseAuth auth;

// Firebase sync interval (to avoid overwhelming Firebase with requests)
unsigned long lastFirebaseSync = 0;  
const unsigned long firebaseSyncInterval = 2500;  // Sync every 2.5 seconds

// Function declarations for better code structure and readability
void saveStateToEEPROM(int channel, bool state);  
bool loadStateFromEEPROM(int channel);
void syncWithFirebase(int channel, bool state);
void checkFirebaseUpdates();
void updateStatesFromFirebase();
void checkWiFiConnection();

void setup() {
  Serial.begin(115200);  // Initialize serial communication for debugging
  
  EEPROM.begin(512);  // Initialize EEPROM with 512 bytes of storage

  // Configure button and device pins
  for (int i = 0; i < 4; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);  // Enable internal pull-up resistors for buttons
    pinMode(devicePins[i], OUTPUT);        // Set device pins as outputs
    
    // Load and apply the last stored state from EEPROM
    deviceStates[i] = loadStateFromEEPROM(i);  
    digitalWrite(devicePins[i], deviceStates[i] ? HIGH : LOW);
  }

  // Wi-Fi Manager handles the Wi-Fi connection via AP mode if needed
  WiFiManager wifiManager;
  wifiManager.autoConnect("Smart Device");  // Device broadcasts AP named "Smart Device"

  // Configure Firebase settings
  config.host = FIREBASE_HOST;  
  config.signer.tokens.legacy_token = FIREBASE_AUTH;  
  Firebase.begin(&config, &auth);  // Initialize Firebase
  Firebase.reconnectWiFi(true);    // Automatically reconnect if Wi-Fi disconnects

  // Fetch initial states from Firebase and override EEPROM values if needed
  updateStatesFromFirebase();
}

void loop() {
  // Iterate over all buttons and manage their states
  for (int i = 0; i < 4; i++) {
    buttonStates[i] = digitalRead(buttonPins[i]);  // Read the button state

    // Check if the button state has changed from the last state
    if (buttonStates[i] != lastButtonStates[i]) {
      delay(50);  // Debounce delay to prevent false triggers

      // Toggle device state if the button is pressed
      if (buttonStates[i] == LOW) {
        deviceStates[i] = !deviceStates[i];  // Toggle device state
        digitalWrite(devicePins[i], deviceStates[i] ? HIGH : LOW);  // Update output pin

        // Save the new state to EEPROM
        saveStateToEEPROM(i, deviceStates[i]);

        // Print the new state to the Serial Monitor
        Serial.print("Device ");
        Serial.print(i);
        Serial.println(deviceStates[i] ? " ON (Switch Control)" : " OFF (Switch Control)");

        // Sync the state with Firebase if Wi-Fi is connected
        if (WiFi.status() == WL_CONNECTED) {
          syncWithFirebase(i, deviceStates[i]);
        }
      }

      // Update the last known button state
      lastButtonStates[i] = buttonStates[i];
    }
  }

  // Periodically check for updates from Firebase if Wi-Fi is connected
  if (WiFi.status() == WL_CONNECTED && millis() - lastFirebaseSync > firebaseSyncInterval) {
    checkFirebaseUpdates();
    lastFirebaseSync = millis();  // Reset the sync timer
  }
}

// Function to save a device state to EEPROM
void saveStateToEEPROM(int channel, bool state) {
  EEPROM.write(EEPROM_ADDRESSES[channel], state);  // Write the state to EEPROM
  EEPROM.commit();  // Ensure the data is saved
  Serial.print("State for device ");
  Serial.print(channel);
  Serial.println(" saved to EEPROM");
}

// Function to load a device state from EEPROM
bool loadStateFromEEPROM(int channel) {
  bool state = EEPROM.read(EEPROM_ADDRESSES[channel]);  // Read the stored state
  Serial.print("State for device ");
  Serial.print(channel);
  Serial.println(" loaded from EEPROM");
  return state;
}

// Function to sync the state of a device with Firebase
void syncWithFirebase(int channel, bool state) {
  Serial.print("Attempting to sync device ");
  Serial.print(channel);
  Serial.println(" with Firebase...");

  String path = "/deviceState" + String(channel);  // Unique path for each device

  // Send the new state to Firebase
  if (Firebase.setBool(firebaseData, path, state)) {
    Serial.print("State for device ");
    Serial.print(channel);
    Serial.println(" synced with Firebase");
  } else {
    // Print error message if the sync fails
    Serial.print("Failed to sync device ");
    Serial.print(channel);
    Serial.print(" with Firebase. Reason: ");
    Serial.println(firebaseData.errorReason());
  }
}

// Function to check for updates from Firebase and apply them if necessary
void checkFirebaseUpdates() {
  Serial.println("Checking for Firebase updates...");

  for (int i = 0; i < 4; i++) {
    String path = "/deviceState" + String(i);  // Unique path for each device

    // Retrieve the state from Firebase
    if (Firebase.getBool(firebaseData, path)) {
      bool firebaseState = firebaseData.boolData();

      // Update the device state if it differs from the current state
      if (firebaseState != deviceStates[i]) {
        deviceStates[i] = firebaseState;
        digitalWrite(devicePins[i], deviceStates[i] ? HIGH : LOW);
        saveStateToEEPROM(i, deviceStates[i]);  // Save the new state to EEPROM

        Serial.print("Device ");
        Serial.print(i);
        Serial.println(deviceStates[i] ? " ON (from Firebase)" : " OFF (from Firebase)");
      }
    } else {
      // Print error if data retrieval fails
      Serial.print("Failed to load data for device ");
      Serial.print(i);
      Serial.print(" from Firebase. Reason: ");
      Serial.println(firebaseData.errorReason());
    }
  }
}

// Function to update device states after Wi-Fi connection
void updateStatesFromFirebase() {
  for (int i = 0; i < 4; i++) {
    String path = "/deviceState" + String(i);  // Firebase path for each device

    // Retrieve the state from Firebase and apply it
    if (Firebase.getBool(firebaseData, path)) {
      bool firebaseState = firebaseData.boolData();
      if (firebaseState != deviceStates[i]) {
        deviceStates[i] = firebaseState;
        digitalWrite(devicePins[i], firebaseState ? HIGH : LOW);
        saveStateToEEPROM(i, firebaseState);  // Save the state to EEPROM

        Serial.print("Device ");
        Serial.print(i);
        Serial.println(firebaseState ? " ON (Firebase Sync)" : " OFF (Firebase Sync)");
      }
    }
  }
}
