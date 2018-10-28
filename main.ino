

/*
  v10 26 oct bjh
  control 2 lights with a bunch of momentary inputs going low
  allow for dimming via pwm analog output
  includes a low batt voltage light cutoff with a voltage-divider analog input

  Use an ardunio pro micro, or figure out pwm pins on other micros

  Use this button library https://github.com/blackketter/Switch
  Use this fade library https://github.com/septillion-git/FadeLed

  TODO:
  -input correct voltage divider calibration VOLTCAL once HW is designed [easy]
  -allow dimmer settings scaled with duration of longPress, rather than just dim/bright [moderate]
  -determine if the built-in 500hz pwm is fast enough, otherwise use the 1kHZ pin3 [easy]
  -finalize i/o pin assignments [easy]

*/

#define DEBUG
#include <Switch.h>   //blackketter switch library from github
#include <FadeLed.h>

//pin assignments
const byte battMonitorPin = A0; // batt input / analog input
const byte btnpin[] = {0, 14, 19, 7, 15, 8}; //1-2 are for led1, 3-4 are led2, 5 is dome light
const byte ledpin[] = {0, 6, 9, 17}; //can only be 5,6,9,10 NOT 3 (pwm is 1khz instead of 500hz). ledpin[3] is the on chip RX led
// note that the two arrays have a placeholder @ 0. The real pins are index 1:n

//configs
#define LOWBATT 11.8  //low batt voltage. Lights off below this value
#define VOLTCAL 0.007415  // V = (R1+R2)*Vsense/R2 where Vsense = 5/1023*Counts  // use a 20k/10k divider. 0.1uF cap across arduino input to gnd   !!!this is wrong!
#define DIMLEVEL 25  // duty cycle of pwm for dim state, n/255 where n=255 is 100% and n=0 is 0%
#define BRIGHTLEVEL 255  //see above
#define FADETIME 250 // //time (ms) it takes to fade off-on and vice versa
#define DEBOUNCEDELAY 45
#define LONGPRESSDELAY 400

//variables
bool domeFlag = false; //if led due to dome or switch (or else closing the door would turn off the light!)
bool lowBattOverride = false;
enum typeBtnState {NONE, PRESS, LONGP, DOUBLE};
typeBtnState btnStateA = NONE;
typeBtnState btnStateB = NONE;
enum typeMode {mON, mDIM, mOFF, mDIMDOME};
typeMode modeA = mOFF;
typeMode modeB = mOFF;
enum lightsTypes {l_BRIGHT, l_DIM, l_OFF};
lightsTypes light1 = l_OFF;
lightsTypes light2 = l_OFF;
long btn1time = 0;
long btn2time = 0;
long btn3time = 0;
long btn4time = 0;


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

  for (int i = 1; i <= 5; i++) {
    digitalWrite(ledpin[3], HIGH);
    delay(500);
    digitalWrite(ledpin[3], LOW);
  }
  modeA = mOFF;
  modeB = mOFF;

#ifdef DEBUG
  Serial.begin(9600);
#endif
}

void loop() {
  FadeLed::update();  //runs the fade routine
  btnStateA = NONE; //initialize the switches to unpushed on each loop
  btnStateB = NONE;
  //read all the input switches
  Btn1Read();
  if (btnStateA == NONE) { //just to make it run faster, dont bother reading other sw
    Btn2Read();
  }
  Btn3Read();
  if (btnStateB == NONE) { //just to make it run faster, dont bother reading other sw
    Btn4Read();
  }
  Btn5Read(); //dome sw

  //low battery safety
  float _battVolts = BattVoltageRead(battMonitorPin); //read battery voltage
  if ( (_battVolts < LOWBATT) && (lowBattOverride == false) ) { //override lights off if batt is low
    if ((btnStateA == DOUBLE) || (btnStateB == DOUBLE) ) { //detect first doubleclick in this mode
      lowBattOverride == true;
    }
    //    else { //if not overridden, clear the variables  //wtf does this do???
    //      modeA = mOFF;
    //      modeB = mOFF;
    //      btnStateA = NONE;
    //      btnStateB = NONE;
    //    }
  }
  if (_battVolts > (LOWBATT + 0.5)) { //reset the batt override if charged
    lowBattOverride = false;
  }

  //run the state machines
  stateMachineA();
  stateMachineB();

  RunLEDs();  //turn on the LED


#ifdef DEBUG
  DEBUG_TIME1 = millis() / 1000;
  if (DEBUG_TIME1 > DEBUG_TIMEL1 + 1) {
    DEBUG_TIMEL1 = millis() / 1000;
    DEBUG_PRINTLN(DEBUG_TIMEL1);
    DEBUG_PRINT("batt V is: ");
    DEBUG_PRINT(_battVolts);
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
    }
  }
#endif

  //end main loop
}





void stateMachineA() {

  //run state machine A
  switch (modeA) {
    case mON:
      if (btnStateA == NONE) {
        light1 = l_BRIGHT;
      }
      else if (btnStateA == PRESS) {
        modeA = mOFF;
      }
      else if (btnStateA == LONGP) {
        modeA = mDIM;
      }
      break;
    case mDIM:
      if (btnStateA == NONE) {
        light1 = l_DIM;
      }
      else if (btnStateA == PRESS) {
        modeA = mOFF;
      }
      else if (btnStateA == LONGP) {
        modeA = mON;
      }
      break;
    case mOFF:
      if (btnStateA == NONE) {
        light1 = l_OFF;
      }
      else if (btnStateA == PRESS) {
        modeA = mON;
      }
      else if (btnStateA == LONGP) {
        modeA = mDIM;
      }
      if (domeFlag) {
        modeA = mDIMDOME;
      }
      break;
    case mDIMDOME:
      if (btnStateA == NONE) {
        light1 = l_DIM;
      }
      else if (btnStateA == PRESS) {
        modeA = mOFF;
      }
      else if (btnStateA == LONGP) {
        modeA = mON;
      }
      if (!domeFlag) {
        modeA = mOFF;
      }
      break;
  } //end state machine
}





void stateMachineB() {

  //run state machine B (led2)
  switch (modeB) {
    case mON:
      if (btnStateB == NONE) {
        light2 = l_BRIGHT;
      }
      else if (btnStateB == PRESS) {
        modeB = mOFF;
      }
      else if (btnStateB == LONGP) {
        modeB = mDIM;
      }
      break;
    case mDIM:
      if (btnStateB == NONE) {
        light2 = l_DIM;
      }
      else if (btnStateB == PRESS) {
        modeB = mOFF;
      }
      else if (btnStateB == LONGP) {
        modeB = mON;
      }
      break;
    case mOFF:
      if (btnStateB == NONE) {
        light2 = l_OFF;
      }
      else if (btnStateB == PRESS) {
        modeB = mON;
      }
      else if (btnStateB == LONGP) {
        modeB = mDIM;
      }
      if (domeFlag) {
        modeB = mDIMDOME;
      }
      break;
    case mDIMDOME:
      if (btnStateB == NONE) {
        light2 = l_DIM;
      }
      else if (btnStateB == PRESS) {
        modeB = mOFF;
      }
      else if (btnStateB == LONGP) {
        modeB = mON;
      }
      if (!domeFlag) {
        modeB = mOFF;
      }
      break;
  } //end state machine
}



void RunLEDs() {
  // turn led1 on/off
  //bright
  if (light1 == l_BRIGHT) {
    led1.set(BRIGHTLEVEL);
    digitalWrite(ledpin[3], HIGH);
  }
  //dim
  if (light1 == l_DIM) {
    led1.set(DIMLEVEL);
    digitalWrite(ledpin[3], HIGH);
  }
  //off
  if (light1 == l_OFF) {
    led1.off();
    digitalWrite(ledpin[3], LOW);
  }
  // turn led2 on/off
  //bright
  if (light2 == l_BRIGHT) {
    led2.set(BRIGHTLEVEL);
  }
  //dim
  if (light2 == l_DIM) {
    led2.set(DIMLEVEL);
  }
  //off
  if (light2 == l_OFF) {
    led2.off();
  }
}


void Btn1Read() {
  btn1.poll();
  if (btn1.pushed() && (btn1time == 0) ) {
    btn1time = millis();
  }
  //watch for btn release, to make sure it isn't a long press
  if ( (  millis() > ( btn1time + LONGPRESSDELAY) ) && ( btn1time != 0 ) ) { //lngpress delay takes 300ms, so wait to do anything. Also make sure btn isn't still pushed
    //    if ( !btn1.on() ) {
    btn1time = 0;
    if ( btn1.longPress() ) {
      btnStateA = LONGP;
    }
    else {
      btnStateA = PRESS;
    }
    if (btn1.doubleClick()) {
      btnStateA = DOUBLE;
    }
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




int BattVoltageRead (int _pin) {
  //read batt voltage, average over several readings
  int n = 5; //how many times to avg over, plus 1
  int _battCounts = 0;
  int i;
  for (i = 1; i <= n; i++) {
    _battCounts = _battCounts + analogRead(_pin);
  }
  _battCounts = _battCounts / i;
  float _battVolts = _battCounts * VOLTCAL;
  if (_battVolts < 1) {
    // if batt circuit is disconnected, disable low batt protection
    // note that there's an external pull-down
    _battVolts = 20; //use a clearly fake number
  }
  return _battVolts;
}
