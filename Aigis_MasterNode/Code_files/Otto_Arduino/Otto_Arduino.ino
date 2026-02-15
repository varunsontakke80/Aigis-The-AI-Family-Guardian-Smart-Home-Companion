#include <Otto.h>
Otto Otto;

// --- PIN DEFINITIONS ---
#define LeftLeg 2 
#define RightLeg 3 
#define LeftFoot 4 
#define RightFoot 5 
#define Buzzer 13

// --- SETTINGS ---
#define UART_BAUD_RATE 115200 // Must match ESP32 baud rate

// --- STATE MACHINE VARIABLES ---
char currentState = 'H'; // Start in the 'H' (Home/Stop) state

void setup() {
  // 1. Initialize Serial Communication (UART)
  Serial.begin(UART_BAUD_RATE);
  
  // 2. Initialize Otto Servos
  Otto.init(LeftLeg, RightLeg, LeftFoot, RightFoot, true, Buzzer);
  
  // 3. Move to safe home position
  Otto.home();
  
  // 4. Beep to indicate ready
  Otto.sing(S_happy);
}

void loop() {
  // 1. QUICKLY CHECK FOR NEW COMMANDS
  if (Serial.available() > 0) {
    char incomingCommand = Serial.read();
    
    // Update the state if it's a recognized command
    if (incomingCommand == 'F' || incomingCommand == 'B' || 
        incomingCommand == 'D' || incomingCommand == 'H' || incomingCommand == 'J') {
      
      currentState = incomingCommand;
      
      // If the command is Home, execute it instantly and sing a confirmation
      if (currentState == 'H') {
        Otto.home();
        Otto.sing(S_connection); 
      }
    }
  }

  // 2. EXECUTE ONE STEP OF THE CURRENT STATE
  // This allows the loop to repeat rapidly and check for new commands
  switch (currentState) {
    
    case 'F': // STATE: Walking Forward
      Otto.walk(1, 1000, 1); // Take just 1 step, then check serial again
      break;

    case 'B': // STATE: Walking Backward
      Otto.walk(1, 1000, -1); // Take just 1 step, then check serial again
      break;

    case 'D': // STATE: Dancing
      performContinuousDance(); // Does one move, then checks serial
      break;
      
    case 'J': // STATE: Jitter
      Otto.jitter(1, 1000, 20); // 1 jitter, then check serial
      break;

    case 'H': // STATE: Stopped / Home
      // Do nothing. Just loop and wait for the next command.
      break;
  }
}

// --- CUSTOM NON-BLOCKING DANCE FUNCTION ---
void performContinuousDance() {
  // A static variable remembers its value between function calls
  static int danceStep = 0; 
  
  // Execute just ONE part of the dance so we don't block the UART receiver
  switch(danceStep) {
    case 0: 
      Otto.moonwalker(1, 1000, 25, 1); 
      break;
    case 1: 
      Otto.moonwalker(1, 1000, 25, -1); 
      break;
    case 2: 
      Otto.swing(1, 1000, 25); 
      break;
    case 3: 
      Otto.jitter(1, 1000, 25); 
      break;
  }

  // Move to the next dance step for the next time this function is called
  danceStep++;
  
  // Reset the dance sequence when it reaches the end
  if (danceStep > 3) {
    danceStep = 0;
  }
}