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
Mooneye's hblank_ly_scx_timing | The test works by executing HALT, waiting for a mode 0 interrupt, and executing a certain number of cycles while expecting LY to either stay the same, or increment. There are actually 4 tests for each SCX value (SCX = 0, 1, 2, 3, 4, 5, 6, 7). (more information in Findings section).
SCX Timing | if SCX % 8 > 0, SCX % 8 number of pixels need to pop out of the background fifo. Each pop takes 1 dot, and rendering is paused during this time. Thus, Mode 3 should be extended by SCX % 8 dots naturally, but mine was extended further at SCX % 8 > 3. The reason was because I mistakenly paused the pixel fetcher as well, which is incorrect. For example, at SCX % 8 = 7, if I paused the pixel fetcher while discarding SCX pixels, I would have 1 dot left to fill my background fifo (which is not enough time), which means the LCD is incremented and waiting for the background fifo to be filled while unable to display pixels, which incorrectly extends my Mode 3.

## Findings [UNVERIFIED]
### Mooneye's hblank_ly_scx_timing
This test only handle background tiles. There are 4 tests for each SCX case: `SCX = 0 - 7`. This can be verified by observing how many times `HALT` is executed at each `SCX` value (despite the source code suggesting 2 tests per `SCX`). The timing of `Mode 0 HBlank` `0x48` interrupt is important. Note that the following assumes a `174` dot default `Mode 3 Drawing` and `LY` increment at dot `456`. We will refer dots as the absolute dot of the current frame of `456` dots unless state otherwise. T-cycle and dots will be used interchangeably.

#### SCX = 0
* `HALT` is executed
* `Mode 3 Drawing` should last `174` dots (dot `254`/`456` of the frame). We will refer dots as the absolute dot of the current frame of `456` dots from here on.
* At the moment that dot `254` ends, we enter `Mode 0 HBlank` the interrupt line should have a rising edge, but the `IF` bit for the `Mode 0 HBlank` interrupt should be set at a later time will be explained:
* Technically, the real hardware DMG samples the `IF` bit at a `T3R` (if we are getting into edge clocks), but for the simplicity, we can say that the CPU will process the `IF` bit **AT THE 2nd** `T3` that follows, which means at dot `259`. The T-cycles are the following: 

    M-Cycle A
    * `T1` - `253` 
    * `T2` - `254` - `Mode 0 HBlank` starts, interrupt line rising edge recognized
    * `T3` - `255` 
    * `T4` - `256`

    M-Cycle B
    * `T1` - `257` 
    * `T2` - `258` 
    * `T3` - `259` - CPU samples the `IF` bit here
    * `T4` - `260`

  However, with an M-cycle accurate CPU it is only important to process this interrupt at `T4` - `260` (The timing is potentially different for other `SCX` values, will be explained in later sections). Therefore, the PPU can set the `IF` bit either at `T1`, `T2`, or `T3` of M-Cycle B.

* The first `SCX = 0` tests that after 49 M-cycles (`196` T-cycles), we should be **exactly** at dot `456` of the frame, and loaded `LY` into register `A` RIGHT BEFORE incrementing `LY`. This is the exact sequences:

    * Dot `260` - `Mode 0 HBlank` interrupt processed by CPU (explained above)
    * Interrupt takes `20` T-cycles, so now are on dot `280`.
    * `NOPs` and other instructions run for `168` T-cycles, we are now on dot `448`.
    * `LD (HL, A)` takes `8` T-cycles, so we loaded the old `LY` (0x41) into register `A` RIGHT before we increment `LY`. a `CP D` operation followed by a `JP NZ` occurs to see that we loaded the *correct* value into `A`, otherwise we will jump to printing the failure onto the screen.

    Note that this first test is not documented in the source code.

* The second `SCX = 0` is almost the same, except that an Extra `NOP` is executed before `LD (HL, A)`:

    * Dot `260` - `Mode 0 HBlank` interrupt processed by CPU (explained above)
    * Interrupt takes `20` T-cycles, so now are on dot `280`.
    * `NOPs` and other instructions run for `172` T-cycles, we are now on dot `452`.
    * `LD (HL, A)` takes `8` T-cycles. Note that the actual value load occurs at the *last* 4 T-Cycles, meaning that by dot `460` `LY` would have already incremented, so we loaded the *new* `LY` (`0x42`) into register `A`.

    There are 2 more tests but the details of the first 2 are sufficient to pass the last 2.

#### SCX = 1 - 4

Here, `Mode 3 Drawing` lasts `1` - `4` dots longer, therefore the `Mode 0 HBlank` interrupt also is processed at a later point. Here are the timings:

M-Cycle A
* `T1` - `253` 
* `T2` - `254`
* `T3` - `255`: if `SCX = 1` - `Mode 0 HBlank` starts, interrupt line rising edge recognized
* `T4` - `256`: if `SCX = 2` - `Mode 0 HBlank` starts, interrupt line rising edge recognized


M-Cycle B
* `T1` - `257` - if `SCX = 3` - `Mode 0 HBlank` starts, interrupt line rising edge recognized
* `T2` - `258` - if `SCX = 4` - `Mode 0 HBlank` starts, interrupt line rising edge recognized
* `T3` - `259`
* `T4` - `260`

M-Cycle C
* `T1` - `261`
* `T2` - `262`
* `T3` - `263` - CPU samples the `IF` bit here
* `T4` - `264`

Is it confusing? If we followed the rule earlier, it makes sense: the CPU samples the `IF` bit **AT THE 2nd** `T3` **THAT FOLLOWS**, even if `Mode 0 HBlank` starts *exactly* at a `T3`. This explains why the test expects the same cycles between `Mode 0 HBlank` interrupt and `LY` increment for values `SCX = 1 - 4`. 

Remember that for an M-cycle accurate CPU, it is only important that the CPU processes the interrupt at `T4` - `264`.

#### SCX = 5 - 7
M-Cycle A
* `T1` - `253` 
* `T2` - `254`
* `T3` - `255`
* `T4` - `256`


M-Cycle B
* `T1` - `257`
* `T2` - `258`
* `T3` - `259`: if `SCX = 5` - `Mode 0 HBlank` starts, interrupt line rising edge recognized
* `T4` - `260`: if `SCX = 6` - `Mode 0 HBlank` starts, interrupt line rising edge recognized

M-Cycle C
* `T1` - `261`: if `SCX = 7` - `Mode 0 HBlank` starts, interrupt line rising edge recognized
* `T2` - `262`
* `T3` - `263`
* `T4` - `264`

M-Cycle D
* `T1` - `265`
* `T2` - `266`
* `T3` - `267` - CPU samples the `IF` bit here
* `T4` - `268`
