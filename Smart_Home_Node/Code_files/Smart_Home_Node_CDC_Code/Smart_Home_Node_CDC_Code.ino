#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h> // <-- ADDED: Required to force the Wi-Fi Channel

// --- AIGIS SR COMMAND DEFINITIONS ---
typedef enum {
    TURN_ON_LIGHT_ONE = 1,
    TURN_OFF_LIGHT_ONE,
    TURN_ON_SOCKET,
    TURN_OFF_SOCKET,
    TURN_ON_FAN_AT_LEVEL_ONE,
    TURN_ON_FAN_AT_LEVEL_TWO,
    TURN_ON_FAN_AT_LEVEL_THREE,
    TURN_OFF_FAN
} aigis_command_t;

// ESP-NOW Data Structure
typedef struct struct_message {
  int command; 
} struct_message;

struct_message myData;

// --- HARDWARE PIN DEFINITIONS ---
// Touch Pins
const int touchPinLight = 4;   // Touch for Relay 1 (Light)
const int touchPinSocket = 15; // Touch for Relay 2 (Socket)
const int touchPinFan = 33;    // Touch for Fan Dimmer

// Relay Pins
const int relayLightPin = 22;
const int relaySocketPin = 23;

// Fan Control Pins (Dimmer Circuit)
const int zcdPin = 27;    // Zero Crossing Detect (Input)
const int triacPin = 26;  // Triac Gate Control (Output)

// --- CONFIGURATION ---
const int threshold = 20;       // Touch sensitivity
const int debounceDelay = 300;  // 300ms debounce
// Dimmer Timings (for 50Hz AC)
const int minDelay = 1000; // High speed
const int maxDelay = 9000; // Low speed

// --- STATE VARIABLES ---
bool lightState = false;
bool socketState = false;
volatile int fanSpeed = 0; // 0-100%
volatile int dimDelay = 0; // Microseconds

unsigned long lastTouchTimeLight = 0;
unsigned long lastTouchTimeSocket = 0;
unsigned long lastTouchTimeFan = 0;

// --- HARDWARE TIMER ---
hw_timer_t *timer = NULL;

// --- INTERRUPT SERVICE ROUTINES (ISRs) ---
void IRAM_ATTR onTimer() {
  if (fanSpeed > 0) {
    digitalWrite(triacPin, HIGH);
    for(volatile int i=0; i<50; i++); // 10us trigger pulse
    digitalWrite(triacPin, LOW);
  }
}

void IRAM_ATTR onZeroCross() {
  if (fanSpeed >= 95) {
    digitalWrite(triacPin, HIGH);
  } else if (fanSpeed <= 0) {
    digitalWrite(triacPin, LOW);
  } else {
    timerWrite(timer, 0); 
    timerAlarm(timer, dimDelay, false, 0); // V3.0 Timer Syntax
    digitalWrite(triacPin, LOW); 
  }
}

// --- ESP-NOW CALLBACK (RECEIVING FROM AIGIS) ---
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&myData, incomingData, sizeof(myData));
  Serial.print("Aigis Voice Command Received: "); 
  Serial.println(myData.command);

  // LOGIC
  switch(myData.command) {
    // Light Control
    case TURN_ON_LIGHT_ONE:
      lightState = true;
      digitalWrite(relayLightPin, LOW);
      break;
    case TURN_OFF_LIGHT_ONE:
      lightState = false;
      digitalWrite(relayLightPin, HIGH);
      break;
      
    // Socket Control
    case TURN_ON_SOCKET:
      socketState = true;
      digitalWrite(relaySocketPin, LOW);
      break;
    case TURN_OFF_SOCKET:
      socketState = false;
      digitalWrite(relaySocketPin, HIGH);
      break;

    // Fan Control
    case TURN_ON_FAN_AT_LEVEL_ONE:
      fanSpeed = 33; 
      dimDelay = map(fanSpeed, 1, 99, maxDelay, minDelay);
      break;
    case TURN_ON_FAN_AT_LEVEL_TWO:
      fanSpeed = 66; 
      dimDelay = map(fanSpeed, 1, 99, maxDelay, minDelay);
      break;
    case TURN_ON_FAN_AT_LEVEL_THREE:
      fanSpeed = 100; 
      dimDelay = map(fanSpeed, 1, 99, maxDelay, minDelay);
      break;
    case TURN_OFF_FAN:
      fanSpeed = 0;
      break;
  }
}

void setup() {
  Serial.begin(115200);

  // Setup Pins
  pinMode(relayLightPin, OUTPUT);
  pinMode(relaySocketPin, OUTPUT);
  pinMode(triacPin, OUTPUT);
  pinMode(zcdPin, INPUT_PULLUP);
  
  digitalWrite(relayLightPin, HIGH);
  digitalWrite(relaySocketPin, HIGH);
  digitalWrite(triacPin, LOW);

  // --- TIMER SETUP ---
  timer = timerBegin(1000000); 
  timerAttachInterrupt(timer, &onTimer);
  attachInterrupt(digitalPinToInterrupt(zcdPin), onZeroCross, RISING);

  // --- WIFI / ESP-NOW ---
  WiFi.mode(WIFI_STA);
  
  // --- ADDED: FORCE WI-FI TO CHANNEL 11 ---
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(11, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
  // ----------------------------------------

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW Init Failed");
    return;
  }
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));

  Serial.println("Aigis Smart Node Ready on Channel 11.");
}

void loop() {
  // Read Touch Sensors
  int tLight = touchRead(touchPinLight);   // Pin 4
  int tSocket = touchRead(touchPinSocket); // Pin 15
  int tFan = touchRead(touchPinFan);       // Pin 33

  // --- MANUAL LIGHT OVERRIDE (Pin 4) ---
  if (tLight < threshold && (millis() - lastTouchTimeLight > debounceDelay)) {
    lightState = !lightState; 
    digitalWrite(relayLightPin, lightState ? LOW : HIGH);
    lastTouchTimeLight = millis();
    Serial.println("Manual Touch: Light Toggled");
  }

  // --- MANUAL SOCKET OVERRIDE (Pin 15) ---
  if (tSocket < threshold && (millis() - lastTouchTimeSocket > debounceDelay)) {
    socketState = !socketState; 
    digitalWrite(relaySocketPin, socketState ? LOW : HIGH);
    lastTouchTimeSocket = millis();
    Serial.println("Manual Touch: Socket Toggled");
  }

  // --- MANUAL FAN OVERRIDE (Pin 33) ---
  if (tFan < threshold && (millis() - lastTouchTimeFan > debounceDelay)) {
    if (fanSpeed > 0) {
        fanSpeed = 0; 
        Serial.println("Manual Touch: Fan OFF");
    } else {
        fanSpeed = 50; // Set to 50%
        dimDelay = map(fanSpeed, 1, 99, maxDelay, minDelay);
        Serial.println("Manual Touch: Fan 50%");
    }
    lastTouchTimeFan = millis();
  }

  delay(10); 
} 