#!/usr/bin/env bash
# Regenerates the sample media used by the tests (needs the ffmpeg CLI).
# sample_av.mp4: 10 s, 640x360 (16:9), 24 fps H.264 + stereo AAC 44.1 kHz.
# Every second starts with one white video frame and a 50 ms 1 kHz beep, so
# tests can check that sound and picture stay aligned after conversion.
set -euo pipefail
cd "$(dirname "$0")"
ffmpeg -y -hide_banner -loglevel error \
  -f lavfi -i "testsrc2=s=640x360:r=24:d=10" \
  -f lavfi -i "aevalsrc='if(lt(mod(t\,1)\,0.05)\,0.5*sin(2*PI*1000*t)\,0)|if(lt(mod(t\,1)\,0.05)\,0.5*sin(2*PI*1000*t)\,0)':s=44100:d=10" \
  -vf "drawbox=x=0:y=0:w=iw:h=ih:color=white:t=fill:enable='lt(mod(t\,1)\,0.040)'" \
  -c:v libx264 -preset veryslow -crf 30 -pix_fmt yuv420p -c:a aac -b:a 64k -movflags +faststart \
  -metadata title="RetroTube Sample" sample_av.mp4
ls -l sample_av.mp4
