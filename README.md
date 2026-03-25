# Motorboot

# Setting Up Arduino IDE and the Required Libraries

If you need a quick rundown of the UI click [click here](#arduino-ui-rundown)

In the following section i will always refer to the driver version i have installed and tested.

## Installation

[this](https://support.arduino.cc/hc/en-us/articles/360019833020-Download-and-install-Arduino-IDE)

## Board Manager

You can find these in your sidebar

- esp32 by Espressif Systems 3.3.0

## Libraries

You can find these in your sidebar

- TFT_eSPI by Bodmer 2.5.43
- ESP32Servo by Kevin Harrington 3.0.9
- ezButton by ArduinoGetStarted 1.0.6
- RC_ESC by Eric Nantel 1.1.0 (only if using the Propeller)

## Configuring TFT_eSPI

This library is a bit weird, you need to go to `Arduino/libraries/TFT_eSPI` where you will find a `User_Setup.h`. There you will have to set certain parameters for the Screen. You can find my configuration [here](TFT_eSPI/User_Setup.h)

## Testing and finding Mac Adress

Before assemblin anything the best way you can test your Board Manager is by running a basic program. As we will need the Mac Adress of both boards anyway, try to flash this code [here](readMacAdress/readMacAdress.ino)

# Arduino UI rundown

IF YOU KNOW HOW ARDUINO WORKS AND HOW TO FLASH CODE YOU CAN SKIP THIS

![Aruidno Screenshot](readme/arduino.png)

1. Compiles the code
2. Compiles and flashes the code to the board selected in 4.
3. Run Debugger. never used it, probably should
4. Selects the Board, click on it, then on "Select other board and port...". If no port is discovered, either the cable doesnt support data or Arduino has no right to access peripherals. Else you can just select the board used, in our case #TODO
5. idk
6. This is the Serial Monitor. It is basically the standart output you are used to from other languages. Make sure the baud Rate is the same as specified in the code `Serial.begin(115200)`, so in our case 115200. The Baud rate specifies how many times a signal changes per second between sender and receiver
7. just all your "sketches" which are just folders in your main Arduino folder
8. Here you can install Board Managers. By default Arduino IDE supports only Arduino microcontrollers. These are essentially packages that contain all pin mappings, clock speed and other hardware related information aswell as specific compiler and uploader for you code to work.
9. Here you can install additional Libraries. Some are already Arduino Core utilities, like `LittleFS.h` or `WiFi.h`. If the needed library is a bit niche and not officially uploaded, its also viable to download it from github etc. navigate to your `Arduino/libraries` Folder, put it in, and import like any normal Header file with `#include "something.h"`.
10. Debugger. never used it, probably should
11. Sketchwide search and replace. Since this is a VSCode fork, you might be familiar with it.
12. Prefered way of creating new projects. Also there is an incredible amount of sample code and examples. Most libraries you download also come with examples. Great source to start from.
13. if you cant use your keyboard
14. idk why this exists
15. Useful, more low level hardware specifications. Not really needed for this project except (2.). Most Practical are probably the following.
    1. Partition Scheme: the memory allocated to filesystem, the app itself (binary,stack,heap), or space for over the air updates. Depending on the specific project this might be worth thinking about
    2. Erase All Flash before Sketch upload: important when you save things to your filesystem and want to preserve it when reflashing the ESP. In our case the calibration should be preserved.
    3. PSRAM: stands for pseudo static RAM, which is external RAM that is mapped to the programs adress space and communicates to the CPU through a SPI bus. Its much slower but needed in certain situations (audio buffer, camera buffer, large JSON)
16. doesnt help. here are great sources #TODO
    1. 
    2. https://randomnerdtutorials.com/

# THE BOAT

We will build and test the controller and the boat seperatly

General Security Stuff

- The moment the Boat is powered on be very careful (especially with the big propeller)
- Test the range of controller and boat beforehand

Quick Rundown of what we will do

- Get mac adresses of both ESPs (maybe label them)
- Put the mac adress of the respectively other ESP into the code
- Flash to Test the network protocol
- assemble ONLY the electronics of boat and controller
- tape or in any other way secure the motors to the table and test functionality
  - throttle and turn
  - calibration controller and boat
- (if you want to reset calibration, just reflash with and under tools, erase flash memory)
- assemble the electrionics into their respective hull, shell, case, whatever

## MAC Adress config

The network protocol we are using is broadcasting the packages, for that we need to specify the receiver.
flash [this](readMacAdress/readMacAdress.ino) code on both ESPs.

Decide which one is the boat esp, and which one is the controller.
[This] is for the controller and [this] is for the boat esp, here you will be specifying the receiver.

An example output would be `[DEFAULT] ESP32 Board MAC Address: b4:e6:2d:a9:ff:fd`
In the code meant for the other ESP find `uint8_t broadcastAddress[]` and put in the respective MAC Adress, for example `uint8_t broadcastAddress[] = {0xB4, 0xE6, 0x2D, 0xA9, 0xFF, 0xFD}`.

Do that for both ESPs

#TODO
Now flash the code on both ESPs and read the serial output

## Boat Electronics


Servo - GPIO 13

Motor1
ENA - GPIO 14
IN1 - GPIO 27
IN2 - GPIO 26

Motor2
ENB - GPIO 25
IN3 - GPIO 33
IN4 - GPIO 32

## Controller Electronics

Left Joystick (Throttle)
VRy - GPIO35
SW - GPIO17
GND - GND
5V+ - 3.3V

Left Joystick (Turn)
VRx - GPIO34
SW - GPIO16
GND - GND
5V+ - 3.3V

Display
GND - GND
VCC - VIN
SCL / SCK - GPIO14
SDA -  GPIO13
RES / RESET - GPIO33
DC / A0 - GPIO32
CS - GPIO15
BLK / LED - 3.3V

#TODO

## Testing Electronics

When testing the electronics keep in mind to test both of them ONLY when the motors are somehow stabilized and no one is too close

Use tape or the clamps from the laser cutter room to

Here you can test the [Controls](#controls) to see if everything is working as indended.

Some remarks

- The two propellers are asymmetric and counter-rotating by design: one spins clockwise, the other counter-clockwise when moving forward. This cancels out the torque reaction that would otherwise cause the boat to yaw to one side. To make one spin the other direction by default, you can swap the two wires from the motor to the motor controller.

## Controls
