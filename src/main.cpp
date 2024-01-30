#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <stdlib.h>

#define SAMPLES 10 // # of samples for moving average filter
#define SERVICE_UUID        "8552e3be-a094-43ca-80be-6e21c69d7874"
#define CHARACTERISTIC_UUID "f0cee6f7-dd4a-4146-8efb-2bdb450fec95"

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
unsigned long previousMillis = 0;
const long interval = 1000;
int sensorPin = 0; // TODO: replace 0
int TRIGGER_PIN = 12; // 12 is used to trigger the sensor
int ECHO_PIN = 13; // 13 is used to read the sensor's echo pulse
float readings[SAMPLES]; // readings from the sensor
int readIndex = 0; // index of the current reading
float runningTotal = 0; // the running total

int sensorReading;
float processedData;

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
    }
};

float movingAverageFilter(float newReading) {
    // subtract the last reading
    runningTotal -= readings[readIndex];
    // read from the sensor
    readings[readIndex] = newReading;
    // add the reading to the total
    runningTotal += readings[readIndex];
    // move to the next position in the array
    readIndex = (readIndex + 1) % SAMPLES;

    return runningTotal/SAMPLES;
}

void setup() {
    Serial.begin(115200);
    Serial.println("Starting BLE work!");
    
    pinMode(sensorPin, INPUT);

    BLEDevice::init("ASH_ESP32");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    BLEService *pService = pServer->createService(SERVICE_UUID);
    
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pCharacteristic->addDescriptor(new BLE2902());
    pCharacteristic->setValue("Hello World");
    pService->start();
    
    // BLEAdvertising *pAdvertising = pServer->getAdvertising();  // this still is working for backward compatibility
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
 
    Serial.println("Characteristic defined! Now you can read it in your phone!");
}

void loop() {
    // clear the trigger pin
    digitalWrite(TRIGGER_PIN, LOW);
    delayMicroseconds(2);

    // sending a 10 microsecond pulse
    digitalWrite(TRIGGER_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIGGER_PIN, LOW);

    // read the echo pin and calculate the duration of the pulse
    long duration = pulseIn(ECHO_PIN, HIGH);

    // calculate the distance in cm
    float distanceCm = (duration / 2) / 29.1;

    // Apply the moving average filter to the distance
    float filteredDistanceCm = movingAverageFilter(distanceCm);

    Serial.print("Distance: ");
    Serial.print(distanceCm);
    Serial.println(" cm");

    Serial.print("Filtered Distance: ");
    Serial.print(filteredDistanceCm);
    Serial.println(" cm");


    if (deviceConnected) {
        // Send new readings to database
        String dataToSend = "Raw Distance Reading: " + String(distanceCm) + "cm, Processed Data: " + String(filteredDistanceCm) + "cm";
        pCharacteristic->setValue(dataToSend.c_str());

        pCharacteristic->notify();
        Serial.println("Notify value: " + dataToSend);
    }
    
    // disconnecting
    if (!deviceConnected && oldDeviceConnected) {
        delay(500);  // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising();  // advertise again
        Serial.println("Start advertising");
        oldDeviceConnected = deviceConnected;
    }
    
    // connecting
    if (deviceConnected && !oldDeviceConnected) {
        // do stuff here on connecting
        oldDeviceConnected = deviceConnected;
    }
    delay(1000);
}
