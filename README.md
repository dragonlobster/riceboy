# Chip-8 Emulator
Chip-8 Emulator written in C++ with the SFML Library. I wrote this emulator as a learning experiment. Big shoutout to the [EmuDev](https://discord.gg/dkmJAes) for being so helpful and kind.

## Quickstart
Once you build the project with `cmake` you must copy the `assets` folder into the root directory of the executable. If running in Debug mode in `Visual Studio`, make sure to set the `currentDir` to `${workspaceRoot}` in the Debug Configuration: `launch.vs.json`. This will ensure the program is able to read `assets/sound.mp3`.

## Chip-8
This project emulates the original Chip-8 opcodes with no quirks.

### Components:

#### Memory
* 4096 bytes of RAM

#### Registers
* 16 8-bit registers (commonly referred to as `V0` to `VF`)
* A special 16-bit register `I` 

#### Timers
* A sound timer which decrements once per frame (based on target 60 FPS); sound plays in every moment that sound timer is > 0
* A delay timer which decrements once per frame (based on target 60 FPS)

#### Display
* 64 x 32 pixel screen; this emulator will scale it up by `16` times (which can be modified in `src/DrawUtils.h`)

#### Keyboard Input
Mapping:
```
Chip-8                Keyboard
╔═══╦═══╦═══╦═══╗     ╔═══╦═══╦═══╦═══╗
║ 1 ║ 2 ║ 3 ║ C ║ --> ║ 1 ║ 2 ║ 3 ║ 4 ║ 
╠═══╬═══╬═══╬═══╣     ╠═══╬═══╬═══╬═══╣
║ 4 ║ 5 ║ 6 ║ D ║ --> ║ Q ║ W ║ E ║ R ║
╠═══╬═══╬═══╬═══╣     ╠═══╬═══╬═══╬═══╣
║ 7 ║ 8 ║ 9 ║ E ║ --> ║ A ║ S ║ D ║ F ║ 
╠═══╬═══╬═══╬═══╣     ╠═══╬═══╬═══╬═══╣
║ A ║ 0 ║ B ║ F ║ --> ║ Z ║ X ║ C ║ V ║
╚═══╩═══╩═══╩═══╝     ╚═══╩═══╩═══╩═══╝
```

## 3rd Party Libraries

### SFML
Used for:
* Rendering window
* Drawing pixels
* Clearing pixels
* Playing audio
* Handling Input
* Timing the frame via `sf::Clock`

### tinyfiledialogs
Used for:
* opening a Chip-8 ROM in the beginning of the program

## References:
Matthew Mikolay:
* [Technical Reference](https://github.com/mattmikolay/chip-8/wiki/CHIP%E2%80%908-Technical-Reference)
* [Instruction Set](https://github.com/mattmikolay/chip-8/wiki/CHIP%E2%80%908-Instruction-Set)

Timendus:
* [Test Suite](https://github.com/Timendus/chip8-test-suite)

