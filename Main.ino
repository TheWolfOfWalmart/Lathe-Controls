//Static Vars
const byte fButton = 2;
const byte rButton = 3;
const byte fwdRelay = 7;
const byte revRelay = 6;

//Possible States/
const byte idle = 0;
const byte fwd = 1;
const byte rev = 2;
volatile byte motionState = idle; // To keep track of current motion mode. Idle on powerup

//Dynamic Vars
volatile bool fbuttonPressed = false; // Variable to be referenced in loop, but changed outside- Possibly by ISR
volatile bool rbuttonPressed = false; // Variable to be referenced in loop, but changed outside- Possibly by ISR
volatile byte fbuttonValue = 0; // For reading forward button status
volatile byte rbuttonValue = 0; // For reading reverse button status

//Debounce Vars
unsigned long debounceSetting = 50; //Time to wait out debounce; milliseconds
static unsigned long lastDebounce = 0; //Resets state of previous debounce on powerup
unsigned long currentDebounce;

void setup()
{
  pinMode(fwdRelay, OUTPUT); //For actuating relay that switches VFD COM to VFD input terminals
  pinMode(revRelay, OUTPUT); //For actuating relay that switches VFD COM to VFD input terminals
  attachInterrupt(digitalPinToInterrupt(fButton), ISR_moving, CHANGE);
  attachInterrupt(digitalPinToInterrupt(rButton), ISR_moving, CHANGE); // Both interrupts will run the same ISR, in which direction will be determined
  Serial.begin(9600);
}

void loop()
{
  while (motionState == idle)
  {
    if (fbuttonPressed == true) //If FWD button confirmed pressed by ISR...
    {
      motionState = fwd;
      digitalWrite(revRelay, LOW); //Turn off REV relay
      digitalWrite(fwdRelay, HIGH); //Turn on FWD realy
      fbuttonPressed = false; //Reset button state, wait for further input
      Serial.println("State = Forward");
    }

    if (rbuttonPressed == true) //If REV button confirmed pressed by ISR...
    {
      motionState = rev;
      digitalWrite(fwdRelay, LOW); //Turn off FWD relay
      digitalWrite(revRelay, HIGH); //Turn on REV relay
      rbuttonPressed = false; //Reset button state, wait for further input
      Serial.println("State = Reverse");
    }
  }

  while (motionState != idle) //If spindle is running in either direction, i.e. NOT idle...
  {
    if (fbuttonPressed == true || rbuttonPressed == true) //...And any button is pushed
    {
      motionState = idle;
      digitalWrite(revRelay, LOW); //Turn both relays off
      digitalWrite(fwdRelay, LOW);
      fbuttonPressed = false; //Reset both button states, wait for further input
      rbuttonPressed = false;
      Serial.println("State = Idle");
    }
  }

}

void ISR_moving()
{
  Serial.println("Interrupt Detected");

  currentDebounce = millis(); //Store time of interrupt
  fbuttonValue = digitalRead(fButton); //Store button states upon interrupt
  rbuttonValue = digitalRead(rButton);

  if (currentDebounce - lastDebounce > debounceSetting) //If the time since first interrupt toggle exceeds debounce setting...
  {
    if (fbuttonValue == HIGH) fbuttonPressed = true; //...And FWD button still pressed, confirm forward direction
    if (rbuttonValue == HIGH) rbuttonPressed = true; //...And REV button still pressed, confirm reverse direction
  }
  else Serial.println("Spurious input");

  lastDebounce = currentDebounce; //Reset debounce tracker for next interrupt signal

}

