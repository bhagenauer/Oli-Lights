/*

  control 2 lights with a bunch of momentary inputs going low
  allow for dimming via pwm analog output
  includes a low batt voltage light cutoff with a voltage-divider analog input

  Use an ardunio pro micro, or figure out pwm pins on other micros

  Use this button library https://github.com/blackketter/Switch
  Use this fade library https://github.com/septillion-git/FadeLed

  TODO:
  -allow dimmer settings scaled with duration of longPress, rather than just dim/bright [moderate]

*/

#define DEBUG
// #define DEBUG_ANALOG_READ
#include <Switch.h>   //blackketter switch library from github
#include <FadeLed.h>

//pin assignments
const int battMonitorPin = A0; // batt input / analog input
const byte btnpin[] = {0, 14, 19, 7, 15, 4}; //1-2 are for led1, 3-4 are led2, 5 is dome light
const byte ledpin[] = {0, 6, 9, 17}; //can only be 5,6,9,10 NOT 3 (cause pwm is 1khz instead of 500hz). ledpin[3] is the on chip RX led
// note that the two arrays have a placeholder @ 0. The real pins are index 1:n

// configs
#define LOWBATT 11.5  // low batt voltage. Lights off below this value
#define VOLTCAL_M 0.0164 // m in y=mx+b
#define VOLTCAL_B 0.928 // b in y=mx+b
#define DIMLEVEL 25  // duty cycle of pwm for dim state, n/255 where n=255 is 100% and n=0 is 0%
#define BRIGHTLEVEL 255  //see above
#define FADETIME 250 // //time (ms) it takes to fade completely off-on and vice versa
#define DEBOUNCEDELAY 45
#define LONGPRESSDELAY 400

// global variables
bool domeFlag = false; //set true when dome light triggers the led (or else closing the door would turn off the light!)
bool lowBattOverride = false;
enum typeBtnState {NONE, PRESS, LONGP, DOUBLE};
typeBtnState btnStateA = NONE;
typeBtnState btnStateB = NONE;
enum typeMode {mON, mDIM, mOFF, mDIMDOME};
typeMode modeA = mOFF;
typeMode modeB = mOFF;
// the state machine has an extra setting than the actual led output brightness since dome is handled diff from just dim
typeMode modeGeneric = mOFF;
enum typeLight {l_BRIGHT, l_DIM, l_OFF};
typeLight light1 = l_OFF; // this is the target. Fade means it isn't always the actual current light setting
typeLight light2 = l_OFF;
long btn1time = 0;
long btn2time = 0;
long btn3time = 0;
long btn4time = 0;
long fadeTimer1 = 0;
long fadeTimer2 = 0;
int led_level[] = {0, 0, 0}; // the 0th position is not used. This actual led setting
int target[] = {0,0,0};
int errorTotal[] = {0,0,0};
int errorRemaining[] = {0,0,0};
int thisFadeDuration[] = {0,0,0};
int fadeStepCount[] = {0,0,0};
bool fadeInProcess[] = {false, false, false};
// const fadeRate = BRIGHTLEVEL/FADETIME; // i dont think i need these. delete me.
#define ARRAYSIZE 10
unsigned long totalTimer[ARRAYSIZE];
int loopTime = 0;

//config the libraries
Switch btn1 = Switch(btnpin[1], INPUT_PULLUP, LOW, DEBOUNCEDELAY, LONGPRESSDELAY); // 10k pull-up resistor, no internal pull-up resistor, LOW polarity (i think.. prob needs work)
Switch btn2 = Switch(btnpin[2], INPUT_PULLUP, LOW, DEBOUNCEDELAY, LONGPRESSDELAY); // 10k pull-up resistor, no internal pull-up resistor, LOW polarity
Switch btn3 = Switch(btnpin[3], INPUT_PULLUP, LOW, DEBOUNCEDELAY, LONGPRESSDELAY); // 10k pull-up resistor, no internal pull-up resistor, LOW polarity
Switch btn4 = Switch(btnpin[4], INPUT_PULLUP, LOW, DEBOUNCEDELAY, LONGPRESSDELAY); // 10k pull-up resistor, no internal pull-up resistor, LOW polarity
Switch btn5 = Switch(btnpin[5]);
FadeLed led1 = ledpin[1];
FadeLed led2 = ledpin[2];


#ifdef DEBUG
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_PRINT(x) Serial.print(x)
  long DEBUG_TIME1 = 0;
  long DEBUG_TIMEL1 = 0;
  long DEBUG_TIME2 = 0;
  long DEBUG_TIMEL2 = 0;
  long DEBUG_TIME3 = 0;
  long DEBUG_TIMEL3 = 0;
  typeMode DEBUG_A = mOFF;
  typeMode DEBUG_A_LAST = mOFF;
  typeMode DEBUG_B = mOFF;
  typeMode DEBUG_B_LAST = mOFF;
  bool debugPrintFlag;  
#else
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINT(x)
#endif


void setup() {

  pinMode(ledpin[1], OUTPUT);
  led1.setInterval(25); //time of fade
  led1.setTime(FADETIME, true); //time (ms) it takes to fade off-on and vice versa
  led1.begin(0); //start off
  analogWrite(ledpin[1], 0); // double ensure off

  pinMode(ledpin[2], OUTPUT);
  led2.setInterval(25); //time of fade
  led2.setTime(FADETIME, true); //time (ms) it takes to fade off-on and vice versa
  led2.begin(0); //start off
  analogWrite(ledpin[2], 0); //double ensure off

  pinMode(ledpin[3], OUTPUT);

// blink the on-board led at powerup. Remember that its polarity is reversed
  for (int i = 1; i <= 5; i++) {
    digitalWrite(ledpin[3], LOW);
    delay(500);
    digitalWrite(ledpin[3], HIGH);
  }
  modeA = mOFF;
  modeB = mOFF;

  #ifdef DEBUG
    Serial.begin(9600);
    debugPrintFlag = false;
  #endif
}


void loop() {

  // only print debug at 1hz 
  #ifdef DEBUG
    DEBUG_TIME1 = millis() / 1000;
    if (DEBUG_TIME1 > DEBUG_TIMEL1 + 1) {
      DEBUG_TIMEL1 = millis() / 1000;
      debugPrintFlag = true;
    }
    else {
      debugPrintFlag = false;
    }
  #endif

  FadeLed::update();  //runs the fade routine
  btnStateA = NONE; //initialize the switches to unpushed on each loop
  btnStateB = NONE;
  //read all the input switches
  Btn1Read();
  // if (btnStateA == NONE) { //just to make it run faster, dont bother reading other sw
    Btn2Read();
  // }
  Btn3Read();
  // if (btnStateB == NONE) { //just to make it run faster, dont bother reading other sw
    Btn4Read();
  // }
  Btn5Read(); //dome sw

  

  //// low battery cutoff system ////

  // read battery voltage
  double battVolts = BattVoltageRead(battMonitorPin);


  // override lights off if batt voltage is low
  
  if (battVolts < LOWBATT) {
    if (lowBattOverride == false) {
      // turn off the all the lights
      modeA = mOFF;
      modeB = mOFF;
      // btnStateA = NONE;
      // btnStateB = NONE;
    }
  
      // override is nested within low batt mode so that you can only engage it once you've acknowledged that you've got a low battery
    if ((btnStateA == DOUBLE) || (btnStateB == DOUBLE) ) { 
      lowBattOverride == true;
      // just toggling status here allows you to turn the override on and off. 
      // TODO: this is a clumsy HMI: double
      // click to enable with no feedback, then press single to see if lights
      // will turn on.
    }
  }

  // disable the batt override once the batteries recover
  if (battVolts > (LOWBATT + 0.5) && lowBattOverride == true ) {
    lowBattOverride = false;
  }

  //run the led state machines

  // note: these always run. The "do you need to change setting" occurs inside FadeLEDs(). This is prob a bit inefficient and could be pulled out. Oh well.
  modeGeneric = modeA;
  light1 = lightStateMachine(modeA,btnStateA);
  modeA = modeGeneric; // this is to return 2 vars from the fn. learn pointers!

  modeGeneric = modeB; //what does this do?
  light2 = lightStateMachine(modeB,btnStateB);
  modeB = modeGeneric; // this is to return 2 vars from the fn. learn pointers!

  FadeLEDs();
  SetLEDs();

// note to self. double pressed works on (see debug) but why can't I 
// trigger lowBattOverride?


  #ifdef DEBUG
    if (btnStateA == DOUBLE) {
      DEBUG_PRINTLN("*****double pressed*****");
    }
    if (debugPrintFlag) {
      DEBUG_TIMEL1 = millis() / 1000;
      DEBUG_PRINTLN(DEBUG_TIMEL1);
      DEBUG_PRINT("batt V is: ");
      DEBUG_PRINT(battVolts);
      DEBUG_PRINT(", Low Batt Override: ");
      DEBUG_PRINTLN(lowBattOverride);
      DEBUG_PRINT("State Machine A: ");
      switch (modeA) {
        case mON:
          DEBUG_PRINT("Led1 ON");
          break;
        case mDIM:
          DEBUG_PRINT("Led1 DIM");
          break;
        case mOFF:
          DEBUG_PRINT("Led1 OFF");
          break;
        case mDIMDOME:
          DEBUG_PRINT("Led1 OFF-DOME");
          break;
      }
      DEBUG_PRINT(", State Machine B: ");
      switch (modeB) {
        case mON:
          DEBUG_PRINTLN("Led2 ON");
          break;
        case mDIM:
          DEBUG_PRINTLN("Led2 DIM");
          break;
        case mOFF:
          DEBUG_PRINTLN("Led2 OFF");
          break;
        case mDIMDOME:
          DEBUG_PRINTLN("Led2 OFF-DOME");
          break;
      DEBUG_PRINT("loop time takes: ");
      DEBUG_PRINTLN(loopTime);
      }
    }
  #endif

  //end main loop
}


typeLight lightStateMachine(typeMode stateMachine, typeBtnState btnState){
  typeLight lightState;
  switch (stateMachine) {
    case mON:
      if (btnState == NONE) {
        lightState = l_BRIGHT;
      }
      else if (btnState == PRESS) {
        stateMachine = mOFF;
      }
      else if (btnState == LONGP) {
        stateMachine = mDIM;
      }
      break;
    case mDIM:
      if (btnState == NONE) {
        lightState = l_DIM;
      }
      else if (btnState == PRESS) {
        stateMachine = mOFF;
      }
      else if (btnState == LONGP) {
        stateMachine = mON;
      }
      break;
    case mOFF:
      if (btnState == NONE) {
        lightState = l_OFF;
      }
      else if (btnState == PRESS) {
        stateMachine = mON;
      }
      else if (btnState == LONGP) {
        stateMachine = mDIM;
      }
      if (domeFlag) {
        stateMachine = mDIMDOME;
      }
      break;
    case mDIMDOME:
      if (btnState == NONE) {
        lightState = l_DIM;
      }
      else if (btnState == PRESS) {
        stateMachine = mOFF;
      }
      else if (btnState == LONGP) {
        stateMachine = mON;
      }
      if (!domeFlag) {
        stateMachine = mOFF;
      }
      break;
  } //end state machine
  modeGeneric = stateMachine; // setting this global var is to return 2 vars from the fn. learn pointers!
  return lightState;
}


void FadeLEDs() {

// delete this stuff once everything works
  // turn led1 on/off
  // this section to be replaced by prev section
  //bright
//   if (light1 == l_BRIGHT) {
//     // led1.set(BRIGHTLEVEL);
//     target[1] = BRIGHTLEVEL; 
//     // digitalWrite(ledpin[1], HIGH);
//     // digitalWrite(ledpin[3], HIGH); // on-chip led
//   }
//   //dim
//   if (light1 == l_DIM) {
//     // led1.set(DIMLEVEL);
//     target[1] = DIMLEVEL;
//     // digitalWrite(ledpin[1], HIGH);
//     // digitalWrite(ledpin[3], HIGH);
//   }
//   //off
//   if (light1 == l_OFF) {    
//     // led1.off();
//     target[1] = 0;
//     // digitalWrite(ledpin[1], LOW);
//     // digitalWrite(ledpin[3], LOW);
//   }
//   // turn led2 on/off
//   //bright
//   if (light2 == l_BRIGHT) {
//     // led2.set(BRIGHTLEVEL);
//     target[2] = BRIGHTLEVEL;
//   }
//   //dim
//   if (light2 == l_DIM) {
//     // led2.set(DIMLEVEL);
//     target[2] = DIMLEVEL;
//   }
//   //off
//   if (light2 == l_OFF) {
//     // led2.off();
//     target[2] = 0;
//   }
// }



  for (int i = 1; i <= 2; i++) {
    if (i == 1) { 
      // this checks sets the desired state (target[i]) to the output of the state machine.
      switch (light1) {
        case l_BRIGHT:
          target[i] = BRIGHTLEVEL;
        break;
        case l_DIM:
          target[i] = DIMLEVEL;
        break;
        case l_OFF:
          target[i] = 0;
        break;
      }
    }
    if (i == 2) { 
      switch (light2) {
        case l_BRIGHT:
          target[i] = BRIGHTLEVEL;
        break;
        case l_DIM:
          target[i] = DIMLEVEL;
        break;
        case l_OFF:
          target[i] = 0;
        break;
      }
    }

    if (led_level[i] != target[i] && fadeInProcess[i] == false) {
      // this is a new transition
      // above checks if the new target is changed from the led_level setting
      errorTotal[i] = target[i] - led_level[i]; // total change over entire transition. Only calculate this on the first pass
      thisFadeDuration[i] = errorTotal[i] / 255 * FADETIME;
      // max possible fade is 255: this is relative to ms per total fade const
      fadeStepCount[i] = errorTotal[i] / (loopTime/thisFadeDuration[i]);
      // (each loop duration / total fade duration) = NumLoopsToFade
      // errorTotal/(NumLoopsToFade)* = fade counts per loop
      // will be (-) for decreasing brightness
      fadeInProcess[i] = true;
    }


    if (fadeInProcess[i] == true) {
        // this is an existing transition
      errorRemaining[i] = target[i] - led_level[i];

      if ( abs(errorRemaining[i]) >= fadeStepCount[i]) {
        // not there yet: adjust level by step size
        // fadeStep is directional: (-) is decrease
          led_level[i] = led_level[i] + fadeStepCount[i];
      }
      else { 
        // transition complete
        fadeInProcess == false;
        led_level[i] = target[i];
      }
    }
  }
}


void SetLEDs() {
  for (int i = 1; i <= 2; i++) {
    if (i == 1){
      led1.set(led_level[i]);
    }
    if (i == 2){
      led2.set(led_level[i]);
    }
  }
}

void Btn1Read() {
  btn1.poll();
  if (btn1.pushed() && (btn1time == 0) ) {
    btn1time = millis();
  }
  if (btn1.doubleClick()) {
    btnStateA = DOUBLE;
  }
  //watch for btn release, to make sure it isn't a long press
  if ( (  millis() > ( btn1time + LONGPRESSDELAY) ) && ( btn1time != 0 ) ) { //lngpress delay takes 300ms, so wait to do anything. Also make sure btn isn't still pushed
    // if ( !btn1.on() ) {
      btn1time = 0;
      if ( btn1.longPress() ) {
        btnStateA = LONGP;
      }
      else {
        btnStateA = PRESS;
      }
    // }
  }
}


void Btn2Read() {
  //read btn2
  btn2.poll();
  if (btn2.pushed() && (btn2time == 0) ) {
    btn2time = millis();
  }
  //watch for btn release, to make sure it isn't a long press
  if ( (  millis() > ( btn2time + LONGPRESSDELAY) ) && ( btn2time != 0 ) ) { //lngpress delay takes 300ms, so wait to do anything. Also make sure btn isn't still pushed
    btn2time = 0;
    if ( btn2.longPress() ) {
      btnStateA = LONGP;
    }
    else {
      btnStateA = PRESS;
    }
    if (btn2.doubleClick()) {
      btnStateA = DOUBLE;
    }
  }
}


void Btn3Read() {
  //read btn3
  btn3.poll();
  if (btn3.pushed() && (btn3time == 0) ) {
    btn3time = millis();
  }
  //watch for btn release, to make sure it isn't a long press
  if ( (  millis() > ( btn3time + LONGPRESSDELAY) ) && ( btn3time != 0 ) ) { //lngpress delay takes 300ms, so wait to do anything. Also make sure btn isn't still pushed
    btn3time = 0;
    if ( btn3.longPress() ) {
      btnStateB = LONGP;
    }
    else {
      btnStateB = PRESS;
    }
    if (btn3.doubleClick()) {
      btnStateB = DOUBLE;
    }
  }
}


void Btn4Read() {
  //read btn4
  btn4.poll();
  if (btn4.pushed() && (btn4time == 0) ) {
    btn4time = millis();
  }
  //watch for btn release, to make sure it isn't a long press
  if ( (  millis() > ( btn4time + LONGPRESSDELAY) ) && ( btn4time != 0 ) ) { //lngpress delay takes 300ms, so wait to do anything. Also make sure btn isn't still pushed
    btn4time = 0;
    if ( btn4.longPress() ) {
      btnStateB = LONGP;
    }
    else {
      btnStateB = PRESS;
    }
    if (btn4.doubleClick()) {
      btnStateB = DOUBLE;
    }
  }
}


void Btn5Read() {
  // this is the dome light
  btn5.poll();
  if (btn5.on()) {
    domeFlag = true;
  }
  else {
    domeFlag = false;
  }
}


double BattVoltageRead (int _pin) {
  //read batt voltage, average over several readings
  //int _pin1 = A0;
  int n = 5; //how many times to avg over, plus 1
  int _battCounts = 0;
  double _battVolts = 0;
  int i;
  for (i = 1; i <= n; i++) {
    _battCounts = _battCounts + analogRead(_pin);
  }

  _battCounts = _battCounts / i;
  
  //_battCounts = analogRead(_pin);

  _battVolts = _battCounts * VOLTCAL_M + VOLTCAL_B; // y=mx+b


  #ifdef DEBUG_ANALOG_READ
    DEBUG_TIME2 = millis() / 1000;
    if (DEBUG_TIME2 > DEBUG_TIMEL2 + 1) {
      DEBUG_TIMEL2 = millis() / 1000;
      DEBUG_PRINTLN();
      DEBUG_PRINT("batt V counts are: ");
      DEBUG_PRINTLN(_battCounts);
      DEBUG_PRINT("batt V is: ");
      DEBUG_PRINTLN(_battVolts);
    }
  #endif
  

  if (_battVolts < 1) {
    // if batt circuit is disconnected, disable low batt protection
    // note that there's an external pull-down
    _battVolts = 99; //use a clearly fake number
  }
  return _battVolts;
}

void timer() {
  // calc avg time per total sw loop
  unsigned long loopTimeLongAvg = 0;
  unsigned long loopTimeLongSum = 0;
  // use long because int (micros) will wrap at 32 sec
  for (int i = 0; i < (ARRAYSIZE-1); i++) {
    totalTimer[i] = totalTimer[i+1];
    // shift all values left to record a new one
    loopTimeLongSum = loopTimeLongSum + totalTimer[i];
  }

  totalTimer[(ARRAYSIZE-1)] = millis() - totalTimer[(ARRAYSIZE-2)];
  // record the latest loop time
  loopTimeLongAvg = loopTimeLongSum / ARRAYSIZE;
  loopTime = int(loopTimeLongAvg);
}

/*
Test data for volt cal:
counts,actual volts
567,10.12
595,10.61
653,11.65
686,12.24
757,13.50
820,14.14

Trendline: volts=0.164*counts+0.93
*/