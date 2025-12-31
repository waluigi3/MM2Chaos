# Mario Maker 2 Chaos Mod
A mod for MM2 version 3.0.3 to beat chaos lock levels. Built using [LibHakkun](https://github.com/fruityloops1/LibHakkun).
## Features
 - Record object positions
 - Save clear condition count to file
 - Transform blocks to automate lock input
 - Discard draw calls to run faster
## Installation
Follow the typical installation process for ExeFS mods. Usually involves copying main.npdm and subsdk4 to the correct directory. To install the config files see the below config section.
## Config Files
All config files should be created in the root directory of the SD card.
### blockmap.txt
This file is a fixed width text file used to edit blocks in specific positions with certain block IDs. It supports changing the block attributes for the purpose of making some blocks intangible. This is achieved by using 0x06000240 instead of 0x06000040. The x and y positions are float values converted to hex. The format of each line is:
```
0=world, 1=subworld
| x pos    y pos    id       attributes
1 40000000 40000000 FFFF0005 06000040
```
### cc.txt
This file is to dump the clear condition count. It must be created before using the mod and at least 4 bytes long.  Example:
```
0000
```
## Controls
For these button combos to work you may need to be using a Pro Controller.
 - Plus button \+ left button: Begin / stop recording object positions to .bin file on sd card
 - Plus button \+ right button: Map blocks according to blockmap.txt
 - Plus button \+ dowb button: Dump clear condition to cc.txt
## Automation
See bruteforce.py for a python script that automates the brute forcing process. Requires numpy and pyautogui.
## Build
See [LibHakkun](https://github.com/fruityloops1/LibHakkun) for the prerequisites and how to set up the library.
 - `mkdir build`
 - `cd build`
 - `cmake .. -DCMAKE_BUILD_TYPE=Release -GNinja` for Ninja or `cmake .. -DCMAKE_BUILD_TYPE=Release` for make
 - `ninja` or `make` respectively
 ## License
 The LICENSE file applies to all files unless otherwise noted.