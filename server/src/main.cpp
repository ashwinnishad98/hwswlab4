#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <stdlib.h>

#define SAMPLES 10 // # of samples for moving average filter
#define SERVICE_UUID "5ca34f0c-6765-4cc3-9ee5-ad60cf75b82f"
#define CHARACTERISTIC_UUID "c56fbead-87ce-46b0-ae42-820e4170fc53"

BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
unsigned long previousMillis = 0;
const long interval = 1000;
int TRIGGER_PIN = D1;
int ECHO_PIN = D2;
float readings[SAMPLES]; // readings from the sensor
int readIndex = 0;       // index of the current reading
float runningTotal = 0;  // the running total
float average = 0;

class MyServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer)
    {
        deviceConnected = true;
    };

    void onDisconnect(BLEServer *pServer)
    {
        deviceConnected = false;
    }
};

float movingAverageFilter(float newReading)
{
    // subtract the last reading
    runningTotal -= readings[readIndex];
    // read from the sensor
    readings[readIndex] = newReading;
    // add the reading to the total
    runningTotal += readings[readIndex];
    // move to the next position in the array
    readIndex = (readIndex + 1) % SAMPLES;

    return runningTotal / SAMPLES;
}

void setup()
{
    Serial.begin(115200);
    Serial.println("Starting BLE work!");

    pinMode(TRIGGER_PIN, OUTPUT);
    digitalWrite(TRIGGER_PIN, LOW);
    delayMicroseconds(2);
    pinMode(ECHO_PIN, INPUT);

    for (int i = 0; i < SAMPLES; i++)
    {
        readings[i] = 0;
    }

    BLEDevice::init("ASH_ESP32");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    BLEService *pService = pServer->createService(SERVICE_UUID);

    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_WRITE |
            BLECharacteristic::PROPERTY_NOTIFY);
    pCharacteristic->addDescriptor(new BLE2902());
    pCharacteristic->setValue("Hello World");
    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06); // functions that help with iPhone connections issue
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.println("Characteristic defined! Now you can read it in your phone!");
}

void loop()
{

    // clear the trigger pin
    digitalWrite(TRIGGER_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIGGER_PIN, LOW);

    // read the echo pin and calculate the duration of the pulse
    long duration = pulseIn(ECHO_PIN, HIGH);

    if (duration >= 38000)
    {
        Serial.println("Readings out of range");
        return;
    }
    else
    {
        float distanceCm = duration / 58.0;
        // Apply the moving average filter to the distance
        float filteredDistanceCm = movingAverageFilter(distanceCm);
        Serial.print("Raw Distance: ");
        Serial.print(distanceCm);
        Serial.println(" cm, ");
        Serial.print("Denoised Distance: ");
        Serial.print(filteredDistanceCm);
        Serial.println(" cm");
        if (deviceConnected && filteredDistanceCm < 30)
        {
            // Send new readings to database
            unsigned long currentMillis = millis();
            if (currentMillis - previousMillis >= interval)
            {
                char buffer[32];
                snprintf(buffer, sizeof(buffer), "Distance: %.2f cm", average);
                pCharacteristic->setValue(buffer);
                pCharacteristic->notify();
                Serial.print("Notify value: ");
                Serial.println(buffer);

                previousMillis = currentMillis;
            }
        }
    }

    // disconnecting
    if (!deviceConnected && oldDeviceConnected)
    {
        delay(500);                  // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // advertise again
        Serial.println("Start advertising");
        oldDeviceConnected = deviceConnected;
    }

    // connecting
    if (deviceConnected && !oldDeviceConnected)
    {
        // do stuff here on connecting
        oldDeviceConnected = deviceConnected;
    }
    delay(1000);
}
