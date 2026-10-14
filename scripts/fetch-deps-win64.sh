#!/usr/bin/env bash
# Downloads the Windows x64 (MinGW) development packages into third_party/win64.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$ROOT/third_party/win64"
DL="$ROOT/third_party/downloads"
SDL_VER=2.30.9
mkdir -p "$DEST" "$DL"

if [ ! -f "$DEST/lib/libSDL2.a" ]; then
  curl -fsSL -o "$DL/SDL2-mingw.tar.gz" \
    "https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VER/SDL2-devel-$SDL_VER-mingw.tar.gz"
  tar -xzf "$DL/SDL2-mingw.tar.gz" -C "$DL"
  cp -r "$DL/SDL2-$SDL_VER/x86_64-w64-mingw32/." "$DEST/"
fi
echo "SDL2 $SDL_VER ready in $DEST"
