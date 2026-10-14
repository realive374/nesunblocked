// VCR-1 instruction set architecture.
//
// The VCR-1 is a small 8-bit CPU with a 16-bit address space. It is shared by
// the CPU core (decode/execute) and the assembler (encode), so both always
// agree on opcodes, operand formats and cycle counts.
#pragma once

#include <cstdint>

namespace rt90 {

// Memory map (16-bit address space, all of it reachable by LD/ST).
namespace memmap {
constexpr uint16_t ROM_START   = 0x0000;  // firmware, write-protected
constexpr uint16_t ROM_END     = 0x7FFF;
constexpr uint16_t RAM_START   = 0x8000;  // general work RAM
constexpr uint16_t RAM_END     = 0xFDFF;
constexpr uint16_t STACK_START = 0xFE00;  // 256-byte hardware stack
constexpr uint16_t STACK_END   = 0xFEFF;
constexpr uint16_t IO_START    = 0xFF00;  // memory-mapped I/O page
constexpr uint16_t IO_END      = 0xFFFF;
constexpr uint16_t STACK_RESET = 0xFF00;  // SP after reset (first push -> 0xFEFF)
}  // namespace memmap

// Flag bits.
enum Flag : uint8_t {
    FLAG_Z = 0x01,  // zero
    FLAG_C = 0x02,  // carry (ADD) / borrow (SUB, CMP) / shifted-out bit
    FLAG_N = 0x04,  // negative (bit 7 of result)
};

// Operand encodings that follow the opcode byte.
enum class Fmt : uint8_t {
    None,  // (no operand bytes)
    R,     // [reg]
    RR,    // [rd << 4 | rs]
    RI,    // [reg][imm8]          also IN/OUT: [reg][port]
    RM,    // [reg][addr lo][addr hi]
    I16,   // [lo][hi]
    PI,    // [port][imm8]
};

constexpr int fmt_size(Fmt f) {
    switch (f) {
        case Fmt::None: return 0;
        case Fmt::R:    return 1;
        case Fmt::RR:   return 1;
        case Fmt::RI:   return 2;
        case Fmt::RM:   return 3;
        case Fmt::I16:  return 2;
        case Fmt::PI:   return 2;
    }
    return 0;
}

enum Op : uint8_t {
    OP_NOP  = 0x00, OP_HLT  = 0x01, OP_WAI  = 0x02,
    OP_LDI  = 0x10, OP_MOV  = 0x11, OP_LDM  = 0x12, OP_STM  = 0x13,
    OP_LDXR = 0x14, OP_STXR = 0x15, OP_LDXI = 0x16, OP_INX  = 0x17, OP_DEX = 0x18,
    OP_IN   = 0x20, OP_OUT  = 0x21, OP_OUTI = 0x22,
    OP_ADD  = 0x30, OP_SUB  = 0x31, OP_AND  = 0x32, OP_OR   = 0x33, OP_XOR = 0x34, OP_CMP = 0x35,
    OP_ADDI = 0x38, OP_SUBI = 0x39, OP_ANDI = 0x3A, OP_ORI  = 0x3B, OP_CMPI = 0x3C,
    OP_INC  = 0x40, OP_DEC  = 0x41, OP_SHL  = 0x42, OP_SHR  = 0x43, OP_NOT = 0x44,
    OP_JMP  = 0x50, OP_JZ   = 0x51, OP_JNZ  = 0x52, OP_JC   = 0x53, OP_JNC = 0x54, OP_JN = 0x55,
    OP_CALL = 0x58, OP_RET  = 0x59, OP_PUSH = 0x5A, OP_POP  = 0x5B, OP_PHX = 0x5C, OP_PLX = 0x5D,
};

struct OpInfo {
    uint8_t code;
    const char* name;  // canonical name (used by the disassembler / debugger)
    Fmt fmt;
    uint8_t cycles;
};

// Table of every defined opcode. Anything not listed is an illegal opcode and
// faults the CPU.
inline constexpr OpInfo kOpTable[] = {
    {OP_NOP, "NOP", Fmt::None, 1},  {OP_HLT, "HLT", Fmt::None, 1},  {OP_WAI, "WAI", Fmt::None, 1},
    {OP_LDI, "LDI", Fmt::RI, 2},    {OP_MOV, "MOV", Fmt::RR, 1},    {OP_LDM, "LDM", Fmt::RM, 3},
    {OP_STM, "STM", Fmt::RM, 3},    {OP_LDXR, "LDXR", Fmt::R, 2},   {OP_STXR, "STXR", Fmt::R, 2},
    {OP_LDXI, "LDX", Fmt::I16, 3},  {OP_INX, "INX", Fmt::None, 1},  {OP_DEX, "DEX", Fmt::None, 1},
    {OP_IN, "IN", Fmt::RI, 2},      {OP_OUT, "OUT", Fmt::RI, 2},    {OP_OUTI, "OUTI", Fmt::PI, 2},
    {OP_ADD, "ADD", Fmt::RR, 1},    {OP_SUB, "SUB", Fmt::RR, 1},    {OP_AND, "AND", Fmt::RR, 1},
    {OP_OR, "OR", Fmt::RR, 1},      {OP_XOR, "XOR", Fmt::RR, 1},    {OP_CMP, "CMP", Fmt::RR, 1},
    {OP_ADDI, "ADDI", Fmt::RI, 2},  {OP_SUBI, "SUBI", Fmt::RI, 2},  {OP_ANDI, "ANDI", Fmt::RI, 2},
    {OP_ORI, "ORI", Fmt::RI, 2},    {OP_CMPI, "CMPI", Fmt::RI, 2},  {OP_INC, "INC", Fmt::R, 1},
    {OP_DEC, "DEC", Fmt::R, 1},     {OP_SHL, "SHL", Fmt::R, 1},     {OP_SHR, "SHR", Fmt::R, 1},
    {OP_NOT, "NOT", Fmt::R, 1},     {OP_JMP, "JMP", Fmt::I16, 3},   {OP_JZ, "JZ", Fmt::I16, 3},
    {OP_JNZ, "JNZ", Fmt::I16, 3},   {OP_JC, "JC", Fmt::I16, 3},     {OP_JNC, "JNC", Fmt::I16, 3},
    {OP_JN, "JN", Fmt::I16, 3},     {OP_CALL, "CALL", Fmt::I16, 5}, {OP_RET, "RET", Fmt::None, 4},
    {OP_PUSH, "PUSH", Fmt::R, 3},   {OP_POP, "POP", Fmt::R, 3},     {OP_PHX, "PHX", Fmt::None, 4},
    {OP_PLX, "PLX", Fmt::None, 4},
};

inline constexpr int kOpCount = sizeof(kOpTable) / sizeof(kOpTable[0]);

// Returns nullptr for illegal opcodes.
const OpInfo* op_info(uint8_t code);

constexpr int kNumRegs = 4;  // A, B, C, D
inline constexpr const char* kRegNames[kNumRegs] = {"A", "B", "C", "D"};

}  // namespace rt90
