# VCR-1 CPU reference

The VCR-1 is the 8-bit CPU inside RetroTube 90's virtual VCR. It runs the
firmware in `firmware/vcr1.asm`, which the build embeds into the app and
assembles at startup.

## Registers

| Register | Size | Purpose |
|---|---|---|
| `A` `B` `C` `D` | 8-bit | general purpose (`A` is used as the accumulator by convention) |
| `X` | 16-bit | index register for `[X]` addressing |
| `PC` | 16-bit | program counter (reset: `$0000`) |
| `SP` | 16-bit | stack pointer (reset: `$FF00`, grows down) |
| `F` | flags | `Z` zero, `C` carry/borrow, `N` negative (bit 7) |

## Memory map

| Range | Contents |
|---|---|
| `$0000-$7FFF` | ROM (firmware). Writes are dropped and counted as violations. |
| `$8000-$FDFF` | work RAM |
| `$FE00-$FEFF` | stack (256 bytes; overflow/underflow fault the CPU) |
| `$FF00-$FFFF` | memory-mapped I/O. `IN`/`OUT` are short forms for this page. |

## Clock

The CPU runs at a fixed **1 MHz**. The host's main loop measures how much real
time has passed and runs that many cycles. Vertical sync happens every
1/29.97 s of emulated time (33,366⅔ cycles, computed exactly). `WAI` puts
the CPU to sleep until the next vsync, so the firmware runs once per video
frame and spends over 90% of its cycles asleep.

## Instruction set (43 opcodes)

`r` = register, `imm` = 8-bit value, `addr` = 16-bit address, `port` = I/O port (0–255).

| Assembly | Op | Bytes | Cycles | Flags | Effect |
|---|---|---|---|---|---|
| `NOP` | `00` | 1 | 1 | | |
| `HLT` | `01` | 1 | 1 | | stop the CPU |
| `WAI` | `02` | 1 | 1 | | sleep until vsync |
| `LD r, imm` | `10` | 3 | 2 | ZN | r = imm |
| `LD rd, rs` | `11` | 2 | 1 | ZN | rd = rs |
| `LD r, [addr]` | `12` | 4 | 3 | ZN | r = mem[addr] |
| `ST [addr], r` | `13` | 4 | 3 | | mem[addr] = r |
| `LD r, [X]` | `14` | 2 | 2 | ZN | r = mem[X] |
| `ST [X], r` | `15` | 2 | 2 | | mem[X] = r |
| `LDX addr` | `16` | 3 | 3 | | X = addr |
| `INX` / `DEX` | `17`/`18` | 1 | 1 | | X ± 1 |
| `IN r, port` | `20` | 3 | 2 | ZN | r = mem[$FF00 + port] |
| `OUT port, r` | `21` | 3 | 2 | | mem[$FF00 + port] = r |
| `OUT port, imm` | `22` | 3 | 2 | | mem[$FF00 + port] = imm |
| `ADD rd, rs` / `ADD r, imm` | `30`/`38` | 2/3 | 1/2 | ZNC | C = carry out |
| `SUB rd, rs` / `SUB r, imm` | `31`/`39` | 2/3 | 1/2 | ZNC | C = borrow |
| `AND` (rr / imm) | `32`/`3A` | 2/3 | 1/2 | ZN | |
| `OR` (rr / imm) | `33`/`3B` | 2/3 | 1/2 | ZN | |
| `XOR rd, rs` | `34` | 2 | 1 | ZN | |
| `CMP` (rr / imm) | `35`/`3C` | 2/3 | 1/2 | ZNC | like SUB, but the result is discarded |
| `INC r` / `DEC r` | `40`/`41` | 2 | 1 | ZN | |
| `SHL r` / `SHR r` | `42`/`43` | 2 | 1 | ZNC | C = bit shifted out |
| `NOT r` | `44` | 2 | 1 | ZN | |
| `JMP addr` | `50` | 3 | 3 | | |
| `JZ` `JNZ` `JC` `JNC` `JN` | `51`–`55` | 3 | 3 | | conditional jump |
| `CALL addr` / `RET` | `58`/`59` | 3/1 | 5/4 | | return address on the stack |
| `PUSH r` / `POP r` | `5A`/`5B` | 2 | 3 | | |
| `PHX` / `PLX` | `5C`/`5D` | 1 | 4 | | push / pop X |

Any other opcode raises an **illegal opcode** fault and halts the CPU. Other
faults: bad register index, stack overflow/underflow, executing from the
I/O page.

## Assembler syntax

```asm
; comment
.equ  V_MODE, $8001         ; constant
.org  $0000                 ; location counter
start:
    LD A, 'P'               ; immediate (decimal, $hex, 0xhex, %binary, 'c')
    ST [V_MODE], A          ; absolute memory
    LDX message
    LD B, [X]               ; indexed
    OUT $23, A              ; I/O port
    JMP start
message: .db "PLAY \x10", 0 ; strings support \n \t \0 \\ \" \xHH
table:   .dw start, message
```

Expressions support `+`, `-`, parentheses, unary `-`, `<expr` (low byte) and
`>expr` (high byte). Labels may be used before they are defined, except in
`.org` and `.equ`. Mnemonics and register names are case-insensitive;
symbols are case-sensitive.

The standalone assembler is `vcrasm input.asm [-o out.bin] [--org ADDR] [--symbols]`.

## I/O ports

| Port | Dir | Name | Meaning |
|---|---|---|---|
| `$00` | R | BTN_HELD | buttons currently held |
| `$01` | R | BTN_EDGE | buttons pressed since the last read (reading clears it) |
| `$02`/`$03` | R | FRAME_LO/HI | vsync counter |
| `$04` | W | POWER_LED | 1 = on |
| `$10` | R | DECK_STATUS | bit0 tape in, bit1 motor, bit2 beginning of tape, bit3 end of tape, bit4 tape changed |
| `$11` | W | DECK_CMD | 0 stop, 1 play, 2 pause, 3 rew, 4 ff, 5 eject, 6 ack tape change |
| `$12`–`$15` | R | DECK_H/M/S/F | tape counter |
| `$16` | R/W | DECK_SPEED | REW/FF speed in frames per vsync (default 12) |
| `$20` | W | GPU_SCREEN | 0 off, 1 static, 2 blue screen, 3 tape picture |
| `$21`/`$22` | W | OSD_X/OSD_Y | OSD cursor (40×30 grid of 8×8 cells) |
| `$23` | W | OSD_CHAR | write a character at the cursor and advance it |
| `$24` | W | OSD_CLEAR | clear the OSD layer |
| `$25` | W | OSD_COLOR | 0 green, 1 white, 2 red |
| `$26` | W | GPU_FX | bit0 = run tracking |

Button bits: POWER `$01`, PLAY `$02`, STOP `$04`, REW `$08`, FF `$10`,
EJECT `$20`, TRACK `$40`. OSD glyphs: `$10` ▶, `$11` ◀, `$12` ■, `$13` ‖, `$14` ⏏.

The GPU latches the screen mode and OSD at vsync, so the display never
shows a half-drawn OSD.
