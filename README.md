# Oli-Lights

Bryan Hagenauer, 2019

Drives two different leds from two pairs of pushbutton switches
Allows for various functions such as dimming
Lights will be turned off by the dome light (ie door switch) in vehicle
Protects against low battery discharge


Buttons:
Btn 1 and 2 are linked to led1, and Btn 3 and 4 are linked to led2. Each pair of switches are viewed by the code as the same switch, so led1 can be turned on with Btn 1 and off with Btn 2. There is tunable debounce timer to each button press. Tune the value for each installation to be the minimum possible. There is a delay to actually changing the light due to the code waiting to determine if the press is a single or double.

Modes:
Holding the button down in a long press will cycle the lights between Regular and Nightlight modes.

Regular Mode:
In regular mode, the lights operate normally, and at full brightness. Pressing the button a single time will cycle the lights on and off.

Nightlight Mode:
The lights turn on at only a very dim level.

Door Switch Mode:
The system monitors the dome light status. When the dome light is on (due to an open door), all lights are turned on in dim mode. When the dome light turns off, the lights will also turn off. Pressing a button will turn the corresponding light off but leave the other in door switch mode. Holding a button for a long press will turn the corresponding light on 
and return it to Regular Mode (it will not turn off with the door switch).

Battery Protection:
The circuit monitors the battery voltage and will turn the lights off if the low battery threshold is reached. Double pressing the switch will override the low battery protection, and allow for FULL battery depletion. Raising the voltage during charging will automatically reset this condition to normal.

Edit the config values in main.ino to adjust the brightness level, fade characteristics, battery voltage threholds and so on.