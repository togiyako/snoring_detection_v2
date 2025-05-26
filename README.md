
### Snoring detection ###

Public Project Link: https://studio.edgeimpulse.com/studio/662810

## Installation

git clone https://github.com/togiyako/snoring_detection_v2.git

Install ArduinoCLI https://arduino.github.io/arduino-cli/latest/installation/

Install Impulse CLI https://docs.edgeimpulse.com/docs/edge-impulse-cli/cli-installation

To install the Arduino Core for the Nicla board and the pyserial package required to update the NDP120 chip, execute the commands below

cd arduino-nicla-voice-firmware

for mac:
./install_lib_mac.command 

for linux: 
./install_lib_linux.sh

for windows:
./install_lib_win.bat

## Build and deploy

For build follow the instructions

$ cd firmware-arduino-nicla-voice

$ ./arduino-build.sh --build

For deploy connect the Nicla Voice to the computer using a USB cable and execute the script (OS-specific) to flash the board

cd ../arduino-nicla-voice-firmware

$ ./flash_mac.command 

