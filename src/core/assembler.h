// Two-pass assembler for VCR-1 assembly language.
//
// Syntax summary (see docs/VCR1.md for the full reference):
//
//   ; comment
//   .org  $0000                  set location counter
//   .equ  NAME, expr             define a constant (must be defined before use
//                                in .org/.equ; anywhere else forward refs are OK)
//   .db   1, $FF, 'A', "text\x10", 0
//   .dw   label, $1234
//   label:  LD A, 10             immediate
//           LD A, B              register
//           LD A, [var]          absolute memory
//           LD A, [X]            indexed by X
//           ST [var], A
//           OUT PORT, A / OUT PORT, 5 / IN A, PORT
//
// Numbers: 42, $2A, 0x2A, %101010, 'c'. Expressions support + and - and
// unary minus, plus <expr (low byte) and >expr (high byte).
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace rt90 {

struct AsmError {
    int line;
    std::string message;
};

struct AsmResult {
    // Flat 64 KB image; only [min_addr, max_addr) is meaningful.
    std::vector<uint8_t> image;
    uint16_t min_addr = 0xFFFF;
    uint32_t max_addr = 0;  // one past the last emitted byte
    std::map<std::string, uint16_t> symbols;
    std::vector<AsmError> errors;

    bool ok() const { return errors.empty(); }
    size_t size() const { return max_addr > min_addr ? max_addr - min_addr : 0; }
    // Bytes from `origin` up to the last emitted byte.
    std::vector<uint8_t> slice(uint16_t origin = 0) const;
    // All errors formatted as "line N: message" lines.
    std::string error_text() const;
};

AsmResult assemble(const std::string& source);

}  // namespace rt90
