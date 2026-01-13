/**
 * @file LatheController.ino
 * @brief Safety and Direction Control Logic for Engine Lathe VFD Retrofit.
 * 
 * This firmware manages the control interface between physical push-buttons (Front/Rear stations),
 * a Variable Frequency Drive (VFD), and the safety loop.
 * 
 * Features:
 * - Dual-station control (Headstock & Carriage)
 * - State machine for Spindle Motion (Idle, Forward, Reverse)
 * - Hardware Fault Monitoring (VFD Relay Input)
 * - Non-blocking Fault LED status (Blink without Delay)
 * - Software debounce for industrial switches
 * - Interrupt-driven input handling for immediate response
 * 
 * @author TheWolfOfWalmart
 * @date 2019-09-06
 * @platform Arduino Mega 2560
 */

// --- Pin Definitions ---
// Inputs (Buttons & Sensors)
const uint8_t PIN_BTN_FWD_FRONT = 2;   // Front Forward (Interrupt)
const uint8_t PIN_BTN_REV_FRONT = 3;   // Front Reverse (Interrupt)
const uint8_t PIN_BTN_RESET     = 4;   // Fault Reset Button
const uint8_t PIN_FAULT_MONITOR = 18;  // VFD Fault Output (Interrupt) - Configured on VFD P6.02=4
const uint8_t PIN_BTN_FWD_REAR  = 19;  // Rear Forward (Interrupt)
const uint8_t PIN_BTN_REV_REAR  = 20;  // Rear Reverse (Interrupt)

// Outputs
const uint8_t PIN_LED_FAULT     = 5;   // Fault Indicator LED
const uint8_t PIN_RELAY_REV     = 6;   // VFD Reverse Command
const uint8_t PIN_RELAY_FWD     = 7;   // VFD Forward Command
const uint8_t PIN_RELAY_FAULT_CLR = 8; // Send Fault Clear Signal to VFD

// --- Constants ---
const uint8_t STATE_IDLE = 0;
const uint8_t STATE_FWD  = 1;
const uint8_t STATE_REV  = 2;

const unsigned long DEBOUNCE_MS = 50; // ms
const unsigned long LED_BLINK_INTERVAL = 1000; // ms

// --- Global State ---
volatile uint8_t motionState = STATE_IDLE;
volatile bool faultState = true; // Default to Fault on power-up for safety

// ISR State Flags
volatile bool flagFwdPressed = false;
volatile bool flagRevPressed = false;

// Debounce Tracking
volatile unsigned long lastInterruptTime = 0;

// Fault LED State
int ledState = LOW;
unsigned long lastLedUpdate = 0;

// Reset Button Debounce
int resetBtnState = HIGH; // Active Low (Input Pullup)
int resetBtnLastState = HIGH;
unsigned long resetLastDebounce = 0;

/**
 * @brief Interrupt Service Routine for Direction Buttons.
 * Triggers on CHANGE state of any motion button.
 */
void ISR_MotionControl()
{
  // Safety Check: Ignore inputs if in Fault state
  if (faultState) return;

  unsigned long currentTime = millis();
  
  if (currentTime - lastInterruptTime > DEBOUNCE_MS)
  {
    // Check for Active Low (Input Pullup) press
    bool fwdFront = (digitalRead(PIN_BTN_FWD_FRONT) == LOW);
    bool revFront = (digitalRead(PIN_BTN_REV_FRONT) == LOW);
    bool fwdRear  = (digitalRead(PIN_BTN_FWD_REAR) == LOW);
    bool revRear  = (digitalRead(PIN_BTN_REV_REAR) == LOW);

    if (fwdFront || fwdRear)
    {
      flagFwdPressed = true;
    }
    if (revFront || revRear)
    {
      flagRevPressed = true;
    }
    
    lastInterruptTime = currentTime;
  }
}

/**
 * @brief Interrupt Service Routine for VFD Fault Monitor.
 * Triggers if VFD reports a fault.
 */
void ISR_FaultMonitor()
{
  faultState = true;
  motionState = STATE_IDLE;
  // Shut down outputs immediately
  digitalWrite(PIN_RELAY_FWD, LOW);
  digitalWrite(PIN_RELAY_REV, LOW);
}

void setup()
{
  // Output Setup
  pinMode(PIN_RELAY_FWD, OUTPUT);
  pinMode(PIN_RELAY_REV, OUTPUT);
  pinMode(PIN_RELAY_FAULT_CLR, OUTPUT);
  pinMode(PIN_LED_FAULT, OUTPUT);
  
  // Safe State (Relays Off)
  digitalWrite(PIN_RELAY_FWD, LOW);
  digitalWrite(PIN_RELAY_REV, LOW);
  digitalWrite(PIN_RELAY_FAULT_CLR, HIGH); // Assuming Active Low trigger for relay? Or High? Kept standard.

  // Input Setup (Internal Pullups)
  pinMode(PIN_BTN_FWD_FRONT, INPUT_PULLUP);
  pinMode(PIN_BTN_REV_FRONT, INPUT_PULLUP);
  pinMode(PIN_BTN_FWD_REAR,  INPUT_PULLUP);
  pinMode(PIN_BTN_REV_REAR,  INPUT_PULLUP);
  pinMode(PIN_BTN_RESET,     INPUT_PULLUP);
  pinMode(PIN_FAULT_MONITOR, INPUT_PULLUP);

  // Interrupt Attachment
  attachInterrupt(digitalPinToInterrupt(PIN_BTN_FWD_FRONT), ISR_MotionControl, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_BTN_REV_FRONT), ISR_MotionControl, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_BTN_FWD_REAR),  ISR_MotionControl, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_BTN_REV_REAR),  ISR_MotionControl, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_FAULT_MONITOR), ISR_FaultMonitor, CHANGE);

  Serial.begin(9600);
  delay(1000); 
  Serial.println("System Initialized. Status: FAULT (Waiting for Reset)");
}

void loop()
{
  // --- Fault Mode ---
  while (faultState == true)
  {
    unsigned long currentMillis = millis();

    // Blink LED (Non-blocking)
    if (currentMillis - lastLedUpdate >= LED_BLINK_INTERVAL)
    {
      lastLedUpdate = currentMillis;
      ledState = (ledState == LOW) ? HIGH : LOW;
      digitalWrite(PIN_LED_FAULT, ledState);
    }

    // Check Reset Button with Debounce
    int reading = digitalRead(PIN_BTN_RESET);

    if (reading != resetBtnLastState)
    {
      resetLastDebounce = currentMillis;
    }

    if ((currentMillis - resetLastDebounce) > DEBOUNCE_MS)
    {
      if (reading != resetBtnState)
      {
        resetBtnState = reading;

        // Button Pressed (Active Low)
        if (resetBtnState == LOW)
        {
          Serial.println("Reset Command Received. Clearing Faults...");
          
          // Toggle Fault Clear Output to VFD
          digitalWrite(PIN_RELAY_FAULT_CLR, LOW);
          delay(100); 
          digitalWrite(PIN_RELAY_FAULT_CLR, HIGH);
          
          faultState = false;
          digitalWrite(PIN_LED_FAULT, HIGH); // Solid ON indicates Ready
          Serial.println("Fault Cleared. System Ready.");
        }
      }
    }
    resetBtnLastState = reading;
  }

  // --- Operational Mode ---
  if (faultState == false)
  {
    // 1. Idle State: Waiting for Motion Command
    if (motionState == STATE_IDLE)
    {
      if (flagFwdPressed)
      {
        motionState = STATE_FWD;
        digitalWrite(PIN_RELAY_REV, LOW);  // Safety interlock
        delay(50);                         // Deadtime
        digitalWrite(PIN_RELAY_FWD, HIGH); // Engage FWD
        
        Serial.println("State: FORWARD");
        flagFwdPressed = false;
        flagRevPressed = false; // Clear flags
      }
      else if (flagRevPressed)
      {
        motionState = STATE_REV;
        digitalWrite(PIN_RELAY_FWD, LOW);  // Safety interlock
        delay(50);                         // Deadtime
        digitalWrite(PIN_RELAY_REV, HIGH); // Engage REV
        
        Serial.println("State: REVERSE");
        flagFwdPressed = false;
        flagRevPressed = false;
      }
    }

    // 2. Running State: Monitor for Stop Command
    // Any button press while running acts as a STOP command
    else if (motionState != STATE_IDLE)
    {
      if (flagFwdPressed || flagRevPressed)
      {
        // Stop Command Received
        motionState = STATE_IDLE;
        digitalWrite(PIN_RELAY_FWD, LOW);
        digitalWrite(PIN_RELAY_REV, LOW);
        
        Serial.println("State: IDLE (Stop Command)");
        
        // Clear Flags
        flagFwdPressed = false;
        flagRevPressed = false;
      }
    }
  }
}
