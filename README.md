# RetroTube 90

A Windows desktop app that emulates a 1990s TV and VCR. It works like a small
virtual machine: an emulated 8-bit CPU runs the VCR firmware, and emulated
GPU, RAM and video hardware play movies stored on `.vhs` tapes, with CRT
static and subtle VHS noise.

> **Status: Phases 1–5 of 7 are done.** Real movies play. Still to come:
> Phase 6 (the F1 debug overlay and more tests) and Phase 7 (icon, version
> info, `.vhs` file association and the `dist/` package).

## Playing a movie

Drag an `.mp4` onto the TV window, open one with **File → Open** (Ctrl+O), or
drop it onto `RetroTube90.exe`. The VCR shows **RECORDING...** while it
converts the movie to a `.vhs` tape, which it saves next to the original. It
then inserts the tape and presses PLAY.

`.vhs` tapes load straight away. Opening the same MP4 again reuses its tape.
Files are recognized by their content, so a renamed MP4 still works. Anything
else gets a friendly **TAPE NOT RECOGNIZED** screen.

## Controls

| Key | Action |
|---|---|
| Space | play / pause |
| S | stop |
| ← / → | rewind / fast-forward (press again to return to play) |
| T | tracking (briefly cleans up the picture) |
| N | noise slider; ← / → adjust it while it is open (also +/- or the mouse wheel) |
| F | fullscreen |
| Esc | eject (cancels a recording in progress) |
| Ctrl+O | open a video or tape |
| I | interlaced look |
| P | power |

The default noise level is 15%. Tape hiss sits about 30 dB below the movie's
sound.

## Building

Requirements: CMake 3.21 or newer, a C++17 compiler, SDL2 and the FFmpeg
libraries (libavformat, libavcodec, libswscale, libswresample).

```sh
# Native (Linux) build with tests
cmake -S . -B build && cmake --build build -j
./build/rt90_tests

# Windows x64 .exe, cross-compiled with MinGW-w64; SDL2 and FFmpeg are linked statically
./scripts/fetch-deps-win64.sh      # SDL2 development package
./scripts/build-ffmpeg-win64.sh    # minimal LGPL FFmpeg, decoders only
cmake -S . -B build-win64 -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake
cmake --build build-win64 -j
```

Developer flags: `--test-tape` (built-in colour-bar tape), `--play`,
`--noise PERCENT`, `--frames N` (quit after N frames) and
`--shot FRAME:file.png` (save a screenshot). The log is written to
`%LOCALAPPDATA%\RetroTube90\retrotube90.log`.

## How it works

| Path | Contents |
|---|---|
| `src/core/` | VCR-1 CPU, 64 KB bounds-checked RAM, system bus, assembler, ring buffer |
| `src/vcr/` | tape deck, I/O port map, the machine that wires everything to a 1 MHz clock |
| `firmware/vcr1.asm` | VCR firmware, written in VCR-1 assembly |
| `src/gpu/` | GPU: tape + OSD layers at 320×240, double-buffered VRAM, CRT filter |
| `src/av/` | NTSC timing, A/V sync, TV snow and hiss, VHS picture and audio noise, interlacing |
| `src/tape/` | `.vhs` format, tape streaming, MP4 detection and import (FFmpeg) |
| `src/app/` | `TvSet` (everything except the window) and on-screen screens |
| `src/platform/` | Windows menu and Open dialog |
| `tests/` | unit and integration tests |
| `docs/VCR1.md` | CPU and assembler reference |

The `.vhs` format is documented at the top of `src/tape/vhs_format.h`.

Video decoding uses [FFmpeg](https://ffmpeg.org) (LGPL 2.1), built without any
GPL components. Windowing and sound use [SDL2](https://libsdl.org) (zlib
licence). JPEG and PNG come from [stb](https://github.com/nothings/stb)
(public domain). The 8×8 font is the public-domain IBM VGA font.
