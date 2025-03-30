# Gameboy Emulator
M-cycle accurate DMG-01 Emulator written with C++ and SFML. The goal is to pass as many test roms out there as possible.

## Progress
Components | Complete
:------------ | :-------------|
CPU | :white_check_mark:
PPU | :white_check_mark:
MMU | :white_check_mark:
Interrupts | :white_check_mark:
Timer | :white_check_mark:
APU | :white_large_square:
Joypad | :white_large_square:

## Quirks

Quirk | Complete | Description
:------------ | :-------------| :-------------|
HALT Bug | :white_check_mark: | Occurs when HALT is executed while IME is 0. PC fails to increment when executing the next instruction.
DMA | :white_check_mark: | Fast transfer OAM. When value > `0xE0` is written to DMA, it reads from Echo WRAM instead, implemented as sourcing from the value `-0x20`.
OAM Bug | :white_large_square: | OAM memory gets corrupted when accessing it while the PPU is in Mode 2: OAM Scan.

## Tests
### Blargg
Test | Pass | Remarks
:------------ | :-------------| :-------------|
cgb_sound | :x: | CGB only, not applicable
cpu_instrs | :white_check_mark: | 
dmg_sound | :white_large_square: | APU not implemented yet
halt_bug | :white_check_mark: | 
instr_timing | :white_check_mark: |
interrupt_time | :x: | CGB only, not applicable
mem_timing | :white_check_mark: | 
mem_timing-2 | :white_check_mark: |
oam_bug | :white_large_square: | in progress

### Mooneye
#### acceptance: timer
Test | Pass | Remarks
:------------ | :-------------| :-------------|
div_write | :white_check_mark: |
rapid_toggle | :white_check_mark: |
tim00_div_trigger | :white_check_mark: |
tim00 | :white_check_mark: |
tim01_div_trigger | :white_check_mark: |
tim01 | :white_check_mark: |
tim10_div_trigger | :white_check_mark: |
tim10 | :white_check_mark: |
tim_11_div_trigger | :white_check_mark: |
tim_11 | :white_check_mark: |
tima_reload | :white_check_mark: |
tima_write | :white_check_mark: |
tma_write | :white_check_mark: |
