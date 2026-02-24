# Mario Maker 2 Chaos Mod
A mod for MM2 version 3.0.3 to beat chaos lock levels. Built using [LibHakkun](https://github.com/fruityloops1/LibHakkun).
## Features
 - Save clear condition count to file
 - Log object spawns and deaths
 - Transform blocks to automate lock input
 - Built in TAS functionality to automate input
 - Stub functions for increased performance
## Installation
Follow the typical installation process for ExeFS mods. Usually involves copying main.npdm and subsdk4 to the correct directory. To install the other files see the below Files section.
## Files
All files should be created in the root directory of the SD card.
### command.txt
This file is a fixed width text file. 
#### map
this command can be used to edit blocks in specific positions with certain block IDs. It supports changing the block attributes for the purpose of making some blocks intangible. This is achieved by using 0x06000240 instead of 0x06000040. The x and y positions are float values converted to hex.
```
    0=world, 1=subworld
    | x pos    y pos    id       attributes
map 1 40000000 40000000 FFFF0005 06000040
```
#### d and u
TAS inputs need to be in order. They can either be button up (u) or button down (d) at a specific frame offset from the time command.txt is done processing.
```
d   000000 -
u   000150 -
d   000150 r
u   000330 r
```
#### o
To write the current clear condition count (c) at a specific frame of the TAS. Do not use unless in a level or else it will crash.
```
o   002900
```
Sample output:
```
0004
```
#### s
This command will output a SYNC line to the log file and flush the buffer immediately.
```
s   002900
```
Sample output:
```
SYNC
```
#### as
This command outputs a line when an Actor (most game objects) with the specified id spawns. If the id is set to 9999, every Actor will be logged.
```
as  0123
```
The output format is:
```
C|object id|spawn x|spawn y
```
Sample goomba spawning:
```
C|95|168|32
```
#### ad
This command outputs a line when an EnemyUber (most game objects) with the specified id "dies". If the id is set to 9999, every Actor will be logged.
```
ad  0123
```
The output format is:
```
D|object id|current x|current y|spawn x|spawn y
```
Sample goomba being stomped:
```
D|95|82|32|168|32
```
#### fi
Set how many frames to wait before automatically flushing the output. If set to the default of 0 output is only flushed when the buffer is full or on a sync command.
```
fi  000300
```

### out.log
All outputs are appended as new lines to this file. It must be created before using the mod. All output to this file is buffered. It can be flushed on an interval or by the sync command. The log will output a line `LOAD` once the game loads the main menu.

## Controls
For these button combos to work you may need to be using a Pro Controller.
 - Left stick button: Toggle stubbing function for performance. Performance mode is on by default so you may need to press this button to see anything.
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