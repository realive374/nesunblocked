#!/usr/bin/env bash
# Cross-compiles a minimal, static, LGPL-only FFmpeg for Windows x64 into
# third_party/win64. Only what MP4 import needs: the file protocol, common
# demuxers and decoders, swscale and swresample.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$ROOT/third_party/win64"
DL="$ROOT/third_party/downloads"
VER=7.1.1
mkdir -p "$DEST" "$DL"
[ -f "$DEST/lib/libavformat.a" ] && { echo "FFmpeg already built"; exit 0; }

if [ ! -d "$DL/FFmpeg-n$VER" ]; then
  git clone --depth 1 --branch "n$VER" https://github.com/FFmpeg/FFmpeg.git "$DL/FFmpeg-n$VER"
fi
cd "$DL/FFmpeg-n$VER"
./configure \
  --prefix="$DEST" --target-os=mingw32 --arch=x86_64 --cross-prefix=x86_64-w64-mingw32- \
  --enable-static --disable-shared --disable-programs --disable-doc --disable-debug \
  --disable-autodetect --enable-w32threads --disable-network --disable-everything \
  --enable-avcodec --enable-avformat --enable-swscale --enable-swresample \
  --disable-avdevice --disable-avfilter --disable-postproc \
  --enable-protocol=file \
  --enable-demuxer=mov,matroska,avi,mpegts,mpegps,flv \
  --enable-decoder=h264,hevc,mpeg4,mpeg2video,mpeg1video,h263,vp8,vp9,mjpeg,prores \
  --enable-decoder=aac,aac_latm,mp3,mp3float,mp2,ac3,eac3,opus,vorbis,flac,alac \
  --enable-decoder=pcm_s16le,pcm_s16be,pcm_s24le,pcm_f32le,pcm_u8 \
  --enable-parser=h264,hevc,mpeg4video,mpegvideo,vp8,vp9,aac,aac_latm,mpegaudio,ac3,opus,vorbis,flac \
  --enable-bsf=h264_mp4toannexb,hevc_mp4toannexb,aac_adtstoasc,vp9_superframe_split
make -j"$(nproc)"
make install
echo "FFmpeg $VER installed to $DEST"
