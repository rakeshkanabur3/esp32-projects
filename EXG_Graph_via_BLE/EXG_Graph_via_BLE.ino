// ==================== HEADER SECTION ====================
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

// ==================== CONFIGURATION ====================
#define DEVICE_NAME "EXG"                    // BLE broadcast name
#define SERVICE_UUID "12345678-1234-1234-1234-123456789ABC"
#define CHARACTERISTIC_UUID "ABCDEF12-3456-7890-1234-567890ABCDEF"
#define SAMPLE_RATE 125                      // Samples per second
#define INPUT_PIN1 4                         // Channel 1 (primary EXG)
#define INPUT_PIN2 16                        // Channel 2 (secondary)

// ==================== GLOBAL VARIABLES ====================
BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristic = NULL;
bool deviceConnected = false;

// ==================== BLE CALLBACKS ====================
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) { 
        deviceConnected = true; 
    }
    void onDisconnect(BLEServer* pServer) { 
        deviceConnected = false;
        delay(500);
        pServer->startAdvertising();  // Restart advertising
    }
};

// ==================== SETUP FUNCTION ====================
void setup() {
    Serial.begin(115200);
    BLEDevice::init(DEVICE_NAME);
    
    // ADC Configuration
    analogReadResolution(12);           // 0-4095 range
    analogSetPinAttenuation(INPUT_PIN1, ADC_11db);  // 0-3.3V range
    analogSetPinAttenuation(INPUT_PIN2, ADC_11db);
    
    // BLE Server Setup
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    
    BLEService *pService = pServer->createService(SERVICE_UUID);
    
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    
    pCharacteristic->addDescriptor(new BLE2902());
    pService->start();
    
    // Advertising Setup
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setName(DEVICE_NAME);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinInterval(0x20);
    pAdvertising->setMaxInterval(0x40);
    pAdvertising->start();
    
    Serial.println("EXG BLE Server Started");
}

// ==================== MAIN LOOP ====================
void loop() {
    // Timing management
    static unsigned long past = 0;
    unsigned long present = micros();
    unsigned long interval = present - past;
    past = present;

    static long timer = 0;
    timer -= interval;

    if(timer < 0) {
        timer += 1000000 / SAMPLE_RATE;  // 8000 μs for 125 Hz
        
        // Read and filter signals
        float signal1 = ECGFilter1(analogRead(INPUT_PIN1));
        float signal2 = ECGFilter1(analogRead(INPUT_PIN2));
        
        // Simulated additional channels (replace with actual sensors)
        float signal3 = random(500, 1000) / 10.0;
        float signal4 = random(500, 1000) / 10.0;
        
        // BLE Transmission
        if (deviceConnected) {
            char buffer[60];
            sprintf(buffer, "%.1f,%.1f,%.1f,%.1f", signal1, signal2, signal3, signal4);
            pCharacteristic->setValue(buffer);
            pCharacteristic->notify();
        }
        
        // Serial Debug Output (10 Hz)
        static unsigned long serialTimer = 0;
        if(millis() - serialTimer > 100) {
            serialTimer = millis();
            Serial.printf("S1: %.1f, S2: %.1f, S3: %.1f, S4: %.1f\n",
                         signal1, signal2, signal3, signal4);
        }
    }
}

// ==================== DIGITAL FILTER ====================
float ECGFilter1(float input) {
    // Cascaded IIR Biquad Filters
    // Total response: Bandpass 0.5-100 Hz
    
    // Stage 1: Low-pass (cutoff ~40 Hz)
    {
        static float z1, z2;
        float x = input - 0.70682283*z1 - 0.15621030*z2;
        float output = 0.28064917*x + 0.56129834*z1 + 0.28064917*z2;
        z2 = z1;
        z1 = x;
        input = output;
    }
    
    // Stage 2: High-pass (cutoff ~0.5 Hz)
    {
        static float z1, z2;
        float x = input - 0.95028224*z1 - 0.54073140*z2;
        float output = 1.00000000*x + 2.00000000*z1 + 1.00000000*z2;
        z2 = z1;
        z1 = x;
        input = output;
    }
    
    // Additional filtering stages for noise reduction
    // ... (see full code for complete filter chain)
    
    return input;
}