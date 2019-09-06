/*
  Having issues with getting rear set of buttons to work- Still seems to be shorting out somewhere whenever a button is pressed because
  green LED turns off. UNO only has intterupts on pins 2 & 3. Switching from UNO to Mega2560 from old RAMPS printer due to greater availability
  of interrupt pins:

  2, 3, 18, 19, 20, 21

  -Mega is a knock off so interrupt funtionality is yet to be verified.

  -Instead of trying to track 4 direction buttons through 2 interrupts, will be tracking each button, including EMOs , to dedicated interrupt pin(19,20,21)

  -Will have to solder jumper from interrupt pins <13 to though holes of terminal shield; possible soldering additional terminals
*/



//Static Vars(Buttons)
const byte fButton = 2;
const byte rButton = 3;
const byte faultReset = 4; //button dedicated to fault reset on arduino side(as opposed to VFD side). Handled in loop
const byte indicatorLED = 5; //Will indicate power on; steady on. Fault will blink
const byte fButtonRear = 19;
const byte rButtonRear = 20;
const byte emo = 21; //Still not sure if I will monitor EMO on interrupt; will mostly just wire it to relay
//Static Vars(Relays)
const byte fwdRelay = 7;
const byte revRelay = 6;

//Possible Motion States
const byte idle = 0;
const byte fwd = 1;
const byte rev = 2;
volatile byte motionState = idle; // To keep track of current motion mode. Idle on powerup

//Dynamic Vars
/* Experimenting with fault state to integrate with VFD emo and fault functions. Fault state on by default; will require reset before use */
volatile bool faultState = true;
/* Variables to be referenced in loop, but changed in ISR */
volatile bool fButtonPressed = false;
volatile bool rButtonPressed = false;
volatile bool fButtonRearPressed = false;
volatile bool rButtonRearPressed = false;
volatile bool faultResetPressed = false;
/*Variables for reading actual button state */
volatile byte fButtonValue = 0;
volatile byte rButtonValue = 0;
volatile byte fButtonRearValue = 0;
volatile byte rButtonRearValue = 0;


//Debounce Vars
unsigned long debounceSetting = 100; //Time to wait out debounce; milliseconds
static unsigned long lastDebounce = 0; //Resets state of previous debounce on powerup
unsigned long currentDebounce;
unsigned long faultResetDebounce = 100; //Debounce value specifically for faultReset button

void setup()
{

  /*Relays will switch VFD COM to VFD switches */

  pinMode(fwdRelay, OUTPUT);
  pinMode(revRelay, OUTPUT);
  pinMode(faultReset, INPUT);


  /* Interrupts will trigger the same ISR for handling motion states */

  attachInterrupt(digitalPinToInterrupt(fButton), ISR_moving, CHANGE);
  attachInterrupt(digitalPinToInterrupt(rButton), ISR_moving, CHANGE);
  attachInterrupt(digitalPinToInterrupt(fButtonRear), ISR_moving, CHANGE);
  attachInterrupt(digitalPinToInterrupt(rButtonRear), ISR_moving, CHANGE);

  Serial.begin(9600);
  Serial.println("Press reset button to begin");

}

void loop()
{
  while (faultstate == TRUE) //While the tool is in a fault state
  {
    faultResetPressed = digitalRead(faultReset); //Save state of fault reset button
    
    while(faultResetPressed == FALSE) // While tool in fault state and faultReset NOT pressed, blink LED. Button press will escapes this loop
    {
    digitalWrite(indicatorLED, HIGH);
    delay(1000);
    digitalWrite(indicatorLED, LOW);
    delay(1000);
    }
    
    if(faultResetPressed == TRUE) //If button is pressed, assess if debounce or not
    {
       delay(faultResetDebounce) //Wait to see if button still pressed
       
       if(faultResetPressed == TRUE) // If still pressed.... 
       {
         faultState = FALSE; //Clear fault
         Serial.println("Fault Cleared");
       }
    }
  }
  
  while (faultState == false)
  {
    digitalWrite(indicatorLED, HIGH); //LEd constant on to indicate running state
    
    while (motionState == idle) //If spindle is at rest
    {
      if (fbuttonPressed == true || fButtonRearPressed == true) //If either FWD button confirmed pressed by ISR...
      {
        motionState = fwd;
        digitalWrite(revRelay, LOW); //Turn off REV relay
        digitalWrite(fwdRelay, HIGH); //Turn on FWD realy

        /* For serial print; Check to see if it was front Forward Button that was pushed */

        if (fButtonPressed == true)
        {
          Serial.println("Front Forward Button Pressed")
        }
        else Serial.println("Rear Forward Button Pressed") // if it wasn't front, it was rear

          fButtonPressed = false; //Reset button state, wait for further input
        fButtonRearPressed = false; //Reset button state, wait for further input
        Serial.println("State = Forward");
      }

      if (rbuttonPressed == true || rButtonRearPressed == true) //If either REV button confirmed pressed by ISR...
      {
        motionState = rev;
        digitalWrite(fwdRelay, LOW); //Turn off FWD relay
        digitalWrite(revRelay, HIGH); //Turn on REV relay

        /* For serial print Check to see if it was front button that was pushed */

        if (rButtonPressed == true)
        {
          Serial.println("Front Reverse Button Pressed")
        }
        else Serial.println("Rear Reverse Button Pressed") // if it wasn't front, it was rear

          rButtonPressed = false; //Reset button state, wait for further input
        rButtonRearPressed = false; //Reset button state, wait for further input
        Serial.println("State = Reverse");
      }
    }

    while (motionState != idle) //If spindle is running in either direction, i.e. NOT idle...
    {
      if (fButtonPressed == true || rButtonPressed == true) //...And any button is pushed
      {
        motionState = idle;
        digitalWrite(revRelay, LOW); //Turn both relays off
        digitalWrite(fwdRelay, LOW);
        fButtonPressed = false; //Reset both button states, wait for further input
        rButtonPressed = false;
        fButtonRearPressed = false; //Reset both button states, wait for further input
        rButtonRearPressed = false;
        Serial.println("State = Idle");
      }
    }
  }
}

void ISR_moving()
{

  if (faultState == false); //As long as the lathe isn't in a fault state, carry out ISR funtions
  {
    Serial.println("Interrupt Detected");

    currentDebounce = millis(); //Store time of interrupt
    fButtonValue = digitalRead(fButton); //Store button states upon interrupt
    rButtonValue = digitalRead(rButton);
    fButtonRearValue = digitalRead(fButtonRear);
    rButtonRearValue = digitalRead(rButtonRear);

    if (currentDebounce - lastDebounce > debounceSetting) //If the time since first interrupt toggle exceeds debounce setting...
    {
      if (fButtonValue == HIGH) fButtonPressed = true; //...And FWD button still pressed, confirm forward direction
      if (rButtonValue == HIGH) rButtonPressed = true; //...And REV button still pressed, confirm reverse direction
      if (fButtonRearValue == HIGH ) fButtonRearPressed = true; //...And FWD button still pressed, confirm forward direction
      if (rButtonRearValue == HIGH ) rButtonRearPressed = true; //...And REV button still pressed, confirm reverse direction
    }
  }
}
/* else Serial.println("Spurious input"); */ // Commenting out for now because it spams on every debounce and delays don't work in ISRs

lastDebounce = currentDebounce; //Reset debounce tracker for next interrupt signal

}

