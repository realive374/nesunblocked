#!/usr/bin/env python3
"""Builds web/RetroTube90.html: injects the VCR-1 firmware and the 8x8 font
into web/retrotube90.src.html so the phone version runs the same firmware."""
import json, pathlib, re

root = pathlib.Path(__file__).resolve().parent.parent
src = (root / "web/retrotube90.src.html").read_text()
firmware = (root / "firmware/vcr1.asm").read_text()
rows = re.findall(r"\{([^}]*)\}", (root / "src/gpu/font8x8_data.inc").read_text())
font = [[int(v, 16) for v in r.split(",")] for r in rows]
assert len(font) == 128
font[0x10] = [0x01, 0x07, 0x1F, 0x7F, 0x1F, 0x07, 0x01, 0x00]  # play
font[0x11] = [0x80, 0xE0, 0xF8, 0xFE, 0xF8, 0xE0, 0x80, 0x00]  # reverse
font[0x12] = [0x00, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x00]  # stop
font[0x13] = [0x66] * 7 + [0x00]                                  # pause
font[0x14] = [0x18, 0x3C, 0x7E, 0xFF, 0x00, 0xFF, 0xFF, 0x00]  # eject
font[0x15] = [0xFF] * 8                                           # block
font[0x16] = [0x00, 0x3C, 0x7E, 0x7E, 0x7E, 0x7E, 0x3C, 0x00]  # dot
fw = json.dumps(firmware).replace("</", "<\\/")
out = src.replace("__FIRMWARE__", fw).replace("__FONT__", json.dumps(font, separators=(",", ":")))
(root / "web/RetroTube90.html").write_text(out)
print(f"web/RetroTube90.html: {len(out)} bytes")
