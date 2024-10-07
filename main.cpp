#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <U8g2lib.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

// Pin Definitions
const int TRIG_PIN = D6;  // Trigger pin of the ultrasonic sensor
const int ECHO_PIN = D7;  // Echo pin of the ultrasonic sensor
const int BUTTON_PIN = D10; // Button pin for OLED display control
//const int LED_PIN = D9;   // Optional: LED pin for BLE notifications (remove if not needed)

// BLE Service and Characteristic UUIDs
#define SERVICE_UUID               "2066a18f-7e73-4320-81a2-43d5a5f7af01"
#define SENSOR_CHARACTERISTIC_UUID "feb60072-ab7c-42c4-9d86-6c2f1e5fadf1"
#define var buttonCharacteristic = 'ff763476-baa5-4516-8a13-a95a344d0ad6'; // Replace with actual UUID for button pin

// BLE Globals
BLEServer* pServer = NULL;
BLECharacteristic* pSensorCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
long currentDistance = 0; // Variable to store the current distance

// OLED Display Setup
U8G2_SSD1306_128X64_NONAME_1_SW_I2C u8g2(U8G2_R0, SCL, SDA, U8X8_PIN_NONE); // OLED display configuration

// Display timeout variables
unsigned long displayStartTime = 0;
bool displayActive = false;

// Function declarations
void initialize();
long measureDistance();
long microsecondsToCentimeters(long microseconds);
void displayDistance();
void checkDisplayTimeout();

// BLE Server Callbacks for connection handling
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
  };

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
  }
};

// BLE setup
void setup() {
  initialize(); // Initialize hardware and display

  // Create the BLE Device
  BLEDevice::init("DistanceSensor");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create the Sensor Characteristic (distance)
  pSensorCharacteristic = pService->createCharacteristic(
                      SENSOR_CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );

  // Add a descriptor for BLE characteristic
  pSensorCharacteristic->addDescriptor(new BLE2902());

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // Optional: to avoid advertising this param
  BLEDevice::startAdvertising();
  Serial.println("Waiting for a client connection to notify...");
}

void loop() {
  // Measure distance and display when button is pressed
  if (digitalRead(BUTTON_PIN) == HIGH) {
    currentDistance = measureDistance(); // Measure distance when button is pressed
    displayDistance(); // Display the measured distance
    displayStartTime = millis(); // Record the time when the display was activated
    displayActive = true; // Set the flag to indicate display is active

    // Wait for button release to avoid multiple triggers in one press
    while (digitalRead(BUTTON_PIN) == HIGH) {
      delay(10); // Debounce delay
    }
  }

  // Notify BLE client with distance measurement every 2 seconds if connected
  if (deviceConnected) {
    currentDistance = measureDistance();
    pSensorCharacteristic->setValue(String(currentDistance).c_str());
    pSensorCharacteristic->notify();
    Serial.print("New distance notified: ");
    Serial.print(currentDistance);
    Serial.println(" cm");
    delay(2000);  // Notify every 2 seconds
  }

  // Check if the display should time out after 10 seconds
  if (displayActive) {
    checkDisplayTimeout();
  }

  // Reconnect BLE if device disconnected
  if (!deviceConnected && oldDeviceConnected) {
    Serial.println("Device disconnected. Restarting advertising...");
    delay(500); // Allow some time for the stack to stabilize
    pServer->startAdvertising(); // Restart BLE advertising
    oldDeviceConnected = deviceConnected;
  }

  // Update connection status
  if (deviceConnected && !oldDeviceConnected) {
    Serial.println("Device connected.");
    oldDeviceConnected = deviceConnected;
  }
}

// Function to initialize the serial, pins, and display
void initialize() {
  Serial.begin(9600); // Start serial communication
  pinMode(TRIG_PIN, OUTPUT); // Set the trigger pin as output
  pinMode(ECHO_PIN, INPUT); // Set the echo pin as input
  pinMode(BUTTON_PIN, INPUT_PULLUP); // Set the button pin as input with pull-up resistor
  u8g2.begin(); // Initialize the display
  u8g2.setFont(u8g2_font_5x7_tr); // Set the font for the display
}

// Function to measure distance using the ultrasonic sensor
long measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH); // Measure the duration of the echo
  return microsecondsToCentimeters(duration); // Convert duration to centimeters
}

// Function to convert microseconds to centimeters
long microsecondsToCentimeters(long microseconds) {
  return microseconds / 29 / 2; // Sound travels at 29 microseconds per centimeter round-trip
}

// Function to display the measured distance on the OLED
void displayDistance() {
  Serial.print(currentDistance);
  Serial.println(" cm");

  u8g2.clearBuffer(); // Clear the internal memory
  u8g2.setCursor(0, 10); // Set cursor position
  u8g2.print(currentDistance); // Print the distance
  u8g2.print(" cm");
  u8g2.sendBuffer(); // Transfer the internal memory to the display
}

// Function to check if the display should time out after 10 seconds
void checkDisplayTimeout() {
  if (millis() - displayStartTime >= 10000) {
    displayActive = false; // Reset the flag
    u8g2.clearBuffer(); // Clear the display
    u8g2.sendBuffer(); // Transfer the internal memory to the display
  }
}
