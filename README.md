# Gameboy DMG Emulator
Cycle accurate DMG-01 Emulator written with C++ and SFML. The goal is to be able to run Pinball Fantasies.

## Progress
Features | Complete | Remarks
:------------ | :-------------|:-------------|
CPU | :white_check_mark: | M-cycle accurate
PPU | :white_check_mark: | T-cycle accurate
MMU | :white_check_mark: |
Interrupts | :white_check_mark: |
Timer | :white_check_mark: |
APU | :white_large_square: |
Joypad | :white_large_square: |
MBC1 | :white_check_mark: |

## Quirks
Quirk | Complete | Description
:------------ | :-------------| :-------------|
Dummy Fetch | :white_check_mark: | There is an 6-8 cycle process in which a fetch of the first tile occurs and is then discarded. I currently have it set to 8.
HALT Bug | :white_check_mark: | Occurs when HALT is executed while IME is 0. PC fails to increment when executing the next instruction.
DMA | :white_check_mark: | Fast transfer OAM. When value > `0xE0` is written to DMA, it reads from Echo WRAM instead, implemented as sourcing from the value `-0x20`. Further, the bus at which the OAM sources from is locked. It's either the 
OAM Bug | :white_large_square: | OAM memory gets corrupted when accessing it while the PPU is in Mode 2: OAM Scan.
LCD Toggle | :white_large_square: | LCDC bit 7 enables and disables the LCD. It should only be done in VBlank, LY and STAT are set to 0 and writes to it are ignoredwhile LCD is off (to be confirmed). The first frame when LCD is toggled on is still processed but not drawn to LCD, so the first frame actually drawn will be the second frame.

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

#### acceptance: ppu
Test | Pass | Remarks
:------------ | :-------------| :-------------|
hblank_ly_scx_timing-GS | :white_check_mark: |
intr_1_2_timing-GS | :white_check_mark: |
intr_2_0_timing | :white_check_mark: |
intr_2_mode3_timing | :white_check_mark_: |
lcdon_write_timing-GS | :white_large_square: |
lcdon_timing-dmgABCmgbS | :white_large_square: |
stat_irq_blocking | :white_check_mark: |
vblank_stat_intr-GS | :white_check_mark: |

#### acceptance: manual-only
Test | Pass | Remarks
:------------ | :-------------| :-------------|
sprite_priority | :white_check_mark: | will include screenshot once palette is finalized

#### acceptance: interrupts
Test | Pass | Remarks
:------------ | :-------------| :-------------|
ie_push | :white_check_mark: | There are 2 checks for IE, the first is corresponding bits with IE and IF, but when actually servicing the interrupt, if IE was written to it, it actually changes the interrupt vector from the original to the new interrupt vector based on the new IE. If 0 was written to IE, then it jumps to reset vector 0. But if IE was written to on T3 then the interrupt vector fails to do the switcharoo.

#### acceptance: oam_dma
Test | Pass | Remarks
:------------ | :-------------| :-------------|
basic | :white_check_mark: | 
reg_read | :white_check_mark: | 
sources-dmgABCmgbS | :white_check_mark: | 


### Matt Currie
#### acid
Test | Pass | Remarks
:------------ | :-------------| :-------------|
dmg-acid2 | :white_check_mark: | will include screenshot once palette is finalized
which | :white_large_square: |

## Challenges
Problem | Solution 
:------------ | :-------------|
First column of tiles missing | Reset the pixel fetcher to FetchTileNo mode if a sprite fetch is initiated.
Blargg's cpu_intrs infinite loop | Must correctly implement MBC1 to stop looping.
Mooneye's sprite priority test - rectangles incorrect | Sprite FIFO is always filled in with transparent pixels to maintain a fixed size of 8. This affects the rules of sprite (or obj) priority on overlapping pixels between different objects, where transparent pixels should always be replaced with non transparent ones regardless of priority.
Mooneye's ppu timing tests hang | The PPU didn't set the IF bit correctly because it was comparing last PPU mode with current PPU mode to set the IF bit (but the mode didn't change), so the emulator was stuck in HALT mode after EI HALT.
Mooneye's oam dma reg_read test was failing | I accidentally didn't process writes to FF46, or any address that wasn't in the DMA source bus because I only focused on writing to the DMA source bus the current DMA transfer address (which is correct). 
Can't pass blargg's oam_bug 1-lcd_sync or mooneyes lcdon_timing |
Mooneye's hblank_ly_scx_timing | The test works by executing HALT, waiting for a mode 0 interrupt, and executing a certain number of cycles while expecting LY to either stay the same, or increment. There are actually 4 tests for each SCX value (SCX = 0, 1, 2, 3, 4, 5, 6, 7). I was able to pass by just incrementing LY at frame dot 452 instead of dot 456. The other way I was able to pass was by delaying IF bit set to the next M-cycle* (see commit 3b32613 for details) but this fails some of the mooneye intr timing tests.
SCX Timing | if SCX % 8 > 0, SCX % 8 number of pixels need to pop out of the background fifo. Each pop takes 1 dot, and rendering is paused during this time. Thus, Mode 3 should be extended by SCX % 8 dots naturally, but mine was extended further at SCX % 8 > 3. The reason was because I mistakenly paused the pixel fetcher as well, which is incorrect. For example, at SCX % 8 = 7, if I paused the pixel fetcher while discarding SCX pixels, I would have 1 dot left to fill my background fifo (which is not enough time), which means the LCD is incremented and waiting for the background fifo to be filled while unable to display pixels, which incorrectly extends my Mode 3.

