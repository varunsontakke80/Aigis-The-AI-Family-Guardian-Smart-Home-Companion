#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <WiFi.h>
#include <esp_now.h>
#include "MAX30105.h" 
#include "spo2_algorithm.h" 

// --- PIN DEFINITIONS FOR XIAO ESP32-C3 ---
#define I2C_SDA_PIN D7 // GPIO 20
#define I2C_SCL_PIN D6 // GPIO 21
#define BUZZER_PIN  D5 // GPIO 8

// --- I2C ADDRESSES ---
#define MPU_ADDR 0x68
#define MAX_ADDR 0x57

// --- WIFI & SMS CONFIGURATION ---
const char* ssid = "xxxxxx";           // Replace with your Wi-Fi Name
const char* password = "xxxxxxx";   // Replace with your Wi-Fi Password

// Circuit Digest API Details
const char* apiKey = "xxxxxx";           // Replace with your API key
const char* templateID = "101";                // Using Template 101: "Your {#var#} is currently at {#var#}."
const char* mobileNumber = "xxxxxxx";     // Replace with recipient number (include Country Code!)
const char* var1 = "HEALTH BAND";              // Custom variable 1
const char* var2 = "FALL DETECTED";            // Custom variable 2

// --- CONFIGURATION ---
const float FALL_THRESHOLD = 25.0; 
const int HR_HIGH_LIMIT = 150;
const int HR_LOW_LIMIT = 45;
const int SPO2_LOW_LIMIT = 85;

// --- OBJECTS ---
Adafruit_MPU6050 mpu;
MAX30105 particleSensor;

// --- ESP-NOW VARIABLES ---
uint8_t broadcastAddress[] = {0xB4, 0x3A, 0x45, 0xF3, 0x9E, 0x50}; // b4:3a:45:f3:9e:50


typedef struct struct_message {
  bool fallDetected;
  bool healthAlarm;
  float heartRate;
  int spo2;
} struct_message;

struct_message myData;
esp_now_peer_info_t peerInfo;

// --- HEALTH VARIABLES ---
uint32_t irBuffer[100]; 
uint32_t redBuffer[100];  
int32_t bufferLength = 100; 
int32_t spo2; 
int8_t validSPO2; 
int32_t heartRate; 
int8_t validHeartRate; 

// Buzzer timing
unsigned long buzzerStartTime = 0;
bool isBuzzing = false;

// Function declarations
void sendSMS();
void triggerAlarm();
void handleBuzzer();

void setup() {
  Serial.begin(115200);
  
  // 1. Init Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // 2. Init I2C
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

  // 3. Init MPU6050
  if (!mpu.begin(MPU_ADDR, &Wire)) {
    Serial.println("Failed to find MPU6050 chip at 0x68!");
    while (1) { delay(10); }
  }
  Serial.println("MPU6050 Found at 0x68!");
  
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  // 4. Init MAX30102 (Logic commented out in loop)
  if (!particleSensor.begin(Wire, 400000, MAX_ADDR)) { 
    Serial.println("MAX30102 not found. (Ignored for Fall Test)");
  } else {
    Serial.println("MAX30102 Found!");
    byte ledBrightness = 60; 
    byte sampleAverage = 4; 
    byte ledMode = 2; 
    byte sampleRate = 100; 
    int pulseWidth = 411; 
    int adcRange = 4096; 
    particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
  }

  // 5. Connect to Wi-Fi FIRST (Crucial for combined ESP-NOW & Wi-Fi)
  WiFi.mode(WIFI_STA);
  Serial.print("Connecting to Wi-Fi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected!");
  Serial.print("Operating on Wi-Fi Channel: ");
  Serial.println(WiFi.channel()); // Take note of this channel for your Receiver!

  // 6. Init ESP-NOW (It will inherit the Wi-Fi router's channel)
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  // 0 means it uses the current active channel (the router's channel)
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
}

void loop() {
  // --- FALL DETECTION ---
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  float accel_magnitude = sqrt(pow(a.acceleration.x, 2) + pow(a.acceleration.y, 2) + pow(a.acceleration.z, 2));
  
  bool isFallen = false;
  if (accel_magnitude > FALL_THRESHOLD) {
    isFallen = true;
    Serial.println("!!! FALL DETECTED !!!");
    triggerAlarm();
  }

  // --- HEALTH MONITORING (COMMENTED OUT FOR TESTING) ---
  /*
  // [Health code is temporarily bypassed for testing fall detection]
  */

  // --- DATA TRANSMISSION (ESP-NOW + SMS) ---
  if (isFallen) {
    // 1. Send ESP-NOW Message
    myData.fallDetected = isFallen;
    myData.healthAlarm = false; 
    myData.heartRate = 0.0;     
    myData.spo2 = 0;            

    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
    if (result == ESP_OK) {
      Serial.println("ESP-NOW: Fall Alert Broadcasted!");
    } else {
      Serial.println("ESP-NOW: Error sending data");
    }

    // 2. Send SMS Alert via HTTP POST
    Serial.println("Triggering SMS Alert...");
    sendSMS();
    
    // Delay to stop flooding. Fall alerts shouldn't trigger multiple times a second.
    delay(3000); 
  }
  
  handleBuzzer();
  delay(10); 
}

// --- SMS SENDING FUNCTION ---
void sendSMS() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    String apiUrl = "/api/v1/send_sms?ID=" + String(templateID);
    
    Serial.print("Connecting to SMS server...");
    if (client.connect("www.circuitdigest.cloud", 80)) {
      Serial.println("connected!");
      
      // Create JSON payload
      String payload = "{\"mobiles\":\"" + String(mobileNumber) + "\",\"var1\":\"" + String(var1) + "\",\"var2\":\"" + String(var2) + "\"}";
      
      // Send HTTP POST request headers
      client.println("POST " + apiUrl + " HTTP/1.1");
      client.println("Host: www.circuitdigest.cloud");
      client.println("Authorization: " + String(apiKey));
      client.println("Content-Type: application/json");
      client.println("Content-Length: " + String(payload.length()));
      client.println(); // Empty line separates headers from payload
      
      // Send the JSON payload
      client.println(payload);
      
      // Wait briefly for response
      int timeout = millis();
      while (client.available() == 0) {
        if (millis() - timeout > 5000) {
          Serial.println(">>> Client Timeout!");
          client.stop();
          return;
        }
      }

      // Read HTTP response code to confirm success
      int responseCode = -1;
      while (client.available()) {
        String line = client.readStringUntil('\n');
        if (line.startsWith("HTTP/")) {
          responseCode = line.substring(9, 12).toInt();
          break; // We only care about the response code
        }
      }
      
      if (responseCode == 200) {
        Serial.println(">>> SMS SENT SUCCESSFULLY! <<<");
      } else {
        Serial.print(">>> Failed to send SMS. HTTP Code: ");
        Serial.println(responseCode);
      }
      
      client.stop();
    } else {
      Serial.println("Connection to SMS server failed!");
    }
  } else {
    Serial.println("Cannot send SMS: Wi-Fi disconnected!");
  }
}

void triggerAlarm() {
  digitalWrite(BUZZER_PIN, HIGH);
  buzzerStartTime = millis();
  isBuzzing = true;
}

void handleBuzzer() {
  if (isBuzzing) {
    if (millis() - buzzerStartTime > 500) {
      digitalWrite(BUZZER_PIN, LOW);
      isBuzzing = false;
    }
  }
}
