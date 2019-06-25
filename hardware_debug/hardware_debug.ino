

/*
  v10 26 oct bjh
  control 2 lights with a bunch of momentary inputs going low
  allow for dimming via pwm analog output
  includes a low batt voltage light cutoff with a voltage-divider analog input

  Use an ardunio pro micro, or figure out pwm pins on other micros

  Use this button library https://github.com/blackketter/Switch
  Use this fade library https://github.com/septillion-git/FadeLed

  TODO:
  -allow dimmer settings scaled with duration of longPress, rather than just dim/bright [moderate]
  -determine if the built-in 500hz pwm is fast enough, otherwise use the 1kHZ pin3 [easy]
  -finalize i/o pin assignments [easy]

*/

//pin assignments
const int battMonitorPin = A0; // batt input / analog input
const byte btnpin[] = {0, 14, 19, 7, 15, 8}; //1-2 are for led1, 3-4 are led2, 5 is dome light
const byte ledpin[] = {0, 6, 9, 17}; //can only be 5,6,9,10 NOT 3 (cause pwm is 1khz instead of 500hz). ledpin[3] is the on chip RX led

void setup() {

  pinMode(ledpin[1], OUTPUT);
  digitalWrite(ledpin[1], LOW); // double ensure off

  pinMode(ledpin[2], OUTPUT);
  digitalWrite(ledpin[2], LOW); //double ensure off

  pinMode(btnpin[1], INPUT_PULLUP);
  pinMode(btnpin[2], INPUT_PULLUP);
  pinMode(btnpin[3], INPUT_PULLUP);
  pinMode(btnpin[4], INPUT_PULLUP);

}

void loop() {

digitalWrite(ledpin[1], HIGH);
delay(250);
digitalWrite(ledpin[1], LOW);
delay(250);
digitalWrite(ledpin[2], HIGH);
delay(250);
digitalWrite(ledpin[2], LOW);
delay(250);

}
