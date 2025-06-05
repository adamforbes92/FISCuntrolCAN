
# FISCuntrolCAN
 FIS Controller for the Volkswagen MK4 Golf platform and similar based on an ESP32.  The original version was based on an Arduino Mega, but it struggles memory wise.  Note that the cluster requires 5v on it's data lines, so the ESP requires an external level shifter.

## Concept
Designed to remain an OEM+ system and give custom welcome screens, offer k-line, CAN and OpenHaldex support.  Uses the OEM stalk buttons for inputs.

All of the options are selectable in _config.h; but the main ones are:
> hasK
> hasCAN
> hasFIS
> showBootScreen

Note that ALL of the terminals don't HAVE to be used, just setup/use the ones you want to see.

 ## Libraries
 Other standard ESP libraries are used, but the bulk of the work is carried out with these:
TLBFISLib: https://github.com/domnulvlad/TLBFISLib
KLineKWP1281Lib: https://github.com/domnulvlad/KLineKWP1281Lib
ESP32_CAN: https://github.com/collin80/esp32_can

Huge shoutout to Vlad (who also helped sort the Haldex code!).

## Custom Boot Messages
Current boot message is my personal Instagram handle under 'MK4' in _config.h.  Can be changed to suit.
A graphical boot message is the default, although a Welcome Greeting is available.  

## K-Line Support
The KWP1281 library allows connection to the k-line using a MC33290.  Requires a 560ohm pull-up to 12v to allow multiple modules on the bus.  

## What the Buttons Do
Single press up/down:
	Goes up / down one 'block' on k-line or CAN
Hold up:
	Toggles between CAN / OpenHaldex
Hold reset:
	Turns off/on the controller (to operate the screen as per OEM)
	

## OpenHaldex Support
Will parse and send OpenHaldex data to allow the changing of modes on the fly using the stalk buttons.  Shows current Haldex lock.

## Inputs
K-line
CAN BUS
OEM Stalk Buttons (Up, Down, Reset)

## Outputs
OEM Cluster Buttons (Up, Down, Reset)

## To Do
Add more CAN functionality - currently only displays RPM, Speed, EML, EPC.  Multiple 'blocks' to be created to allow toggling between them.  More CAN addresses to be added/parsed.
