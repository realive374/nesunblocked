# RetroTube 90

A Windows desktop app that emulates a 1990s TV and VCR. It works like a small
virtual machine: an emulated 8-bit CPU runs the VCR firmware, and emulated
GPU, RAM and video hardware play movies stored on `.vhs` tapes.

> **Status: Phase 1 of 7 (CPU).** The VCR-1 CPU, the assembler and the firmware
> are working. The tape picture is a colour-bar placeholder until the GPU
> arrives in Phase 2, and the on-screen display is shown in the window title.

## Building

Requirements: CMake 3.21 or newer, a C++17 compiler and SDL2.

```sh
# Native (Linux) build with tests
cmake -S . -B build && cmake --build build -j
./build/rt90_tests

# Windows x64 .exe, cross-compiled with MinGW-w64
./scripts/fetch-deps-win64.sh
cmake -S . -B build-win64 -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake
cmake --build build-win64 -j
```

## Controls (so far)

| Key | Action |
|---|---|
| Space | play / pause |
| S | stop |
| ← / → | rewind / fast-forward (press again to return to play) |
| T | tracking |
| Esc | eject |
| F | fullscreen |
| P | power |
| L | insert a blank test tape (developer shortcut until MP4 import in Phase 5) |

Command-line flags: `--test-tape` starts with a tape inserted, `--play` also
presses PLAY, and `--frames N` quits after N video frames.

## Layout

| Path | Contents |
|---|---|
| `src/core/` | VCR-1 CPU, system bus, assembler |
| `src/vcr/` | tape deck, I/O port map, the machine that wires everything together |
| `src/gpu/` | GPU register file (screen mode, OSD layer) |
| `firmware/vcr1.asm` | VCR firmware, written in VCR-1 assembly |
| `tools/vcrasm.cpp` | command-line assembler |
| `tests/` | unit and integration tests |
| `docs/VCR1.md` | CPU and assembler reference |
