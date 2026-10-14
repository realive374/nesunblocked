// 8x8 bitmap font for the OSD: public-domain VGA glyphs for ASCII plus the
// VCR symbols (see io::Glyph).
#pragma once

#include <cstdint>

namespace rt90 {

// Extra glyph codes used by host-side screens (beyond io::Glyph 0x10-0x14).
enum ExtraGlyph : uint8_t {
    GLYPH_BLOCK = 0x15,  // full block (progress bars)
    GLYPH_DOT   = 0x16,  // ● (REC)
};

// Returns 8 row bytes for `ch`; bit 0 is the leftmost pixel.
const uint8_t* font_glyph(uint8_t ch);

}  // namespace rt90
