# Mario Maker 2 Chaos Mod
A mod for MM2 version 3.0.3 to beat chaos lock levels. Built using [LibHakkun](https://github.com/fruityloops1/LibHakkun).
## Features
 - Save clear condition count to file
 - Transform blocks to automate lock input
 - Built in TAS functionality to automate input
 - Stub functions for increased performance
## Installation
Follow the typical installation process for ExeFS mods. Usually involves copying main.npdm and subsdk4 to the correct directory. To install the config files see the below config section.
## Config Files
All config files should be created in the root directory of the SD card.
### command.txt
This file is a fixed width text file. It can be used to edit blocks in specific positions with certain block IDs. It supports changing the block attributes for the purpose of making some blocks intangible. This is achieved by using 0x06000240 instead of 0x06000040. The x and y positions are float values converted to hex.
```
    0=world, 1=subworld
    | x pos    y pos    id       attributes
map 1 40000000 40000000 FFFF0005 06000040
```
TAS inputs need to be in order. They can either be button up (u) or button down (d) at a specific frame offset from the start of the command.
```
d   000000 -
u   000150 -
d   000150 r
u   000330 r
```
To write the current clear condition count (c) at a specific frame of the TAS.
```
o   002900
```

### cc.txt
This file is to dump the clear condition count. It must be created before using the mod. Clear counts are appened to this file.
```
0004
0002
0005
```
## Controls
For these button combos to work you may need to be using a Pro Controller.
 - Left stick button: Toggle stubbing function for performance
## Automation
See bruteforce_combo.py for a python script that automates the brute forcing process. Requires numpy.
## Build
See [LibHakkun](https://github.com/fruityloops1/LibHakkun) for the prerequisites and how to set up the library.
 - `mkdir build`
 - `cd build`
 - `cmake .. -DCMAKE_BUILD_TYPE=Release -GNinja` for Ninja or `cmake .. -DCMAKE_BUILD_TYPE=Release` for make
 - `ninja` or `make` respectively
 ## Credits
 Thanks to Mario Possamodder and domthewiz for their assistance!
 ## License
 The LICENSE file applies to all files unless otherwise noted.