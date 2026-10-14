// Unit tests for the VCR-1 assembler.
#include "core/assembler.h"
#include "core/isa.h"
#include "test.h"

using namespace rt90;

namespace {
std::vector<uint8_t> bytes(const std::string& src) {
    AsmResult r = assemble(src);
    if (!r.ok()) std::fprintf(stderr, "%s", r.error_text().c_str());
    return r.ok() ? r.slice(0) : std::vector<uint8_t>{};
}
bool has_error(const std::string& src, const std::string& needle, int line = -1) {
    AsmResult r = assemble(src);
    for (const AsmError& e : r.errors)
        if (e.message.find(needle) != std::string::npos && (line < 0 || e.line == line)) return true;
    std::fprintf(stderr, "  expected error '%s', got: %s\n", needle.c_str(), r.error_text().c_str());
    return false;
}
}  // namespace

TEST(asm_encodes_every_form) {
    const std::vector<uint8_t> expect = {
        OP_NOP, OP_HLT, OP_WAI,
        OP_LDI, 0, 5,
        OP_MOV, 0x12,
        OP_LDM, 3, 0x34, 0x12,
        OP_LDXR, 1,
        OP_STM, 2, 0x00, 0x80,
        OP_STXR, 0,
        OP_LDXI, 0xCD, 0xAB,
        OP_INX, OP_DEX,
        OP_IN, 0, 0x10,
        OP_OUT, 1, 0x11,
        OP_OUTI, 0x24, 0,
        OP_ADD, 0x01, OP_ADDI, 0, 1,
        OP_SUB, 0x23, OP_SUBI, 3, 2,
        OP_AND, 0x01, OP_ANDI, 0, 0x0F,
        OP_OR, 0x01, OP_ORI, 0, 0xF0,
        OP_XOR, 0x00,
        OP_CMP, 0x01, OP_CMPI, 0, 10,
        OP_INC, 0, OP_DEC, 1, OP_SHL, 2, OP_SHR, 3, OP_NOT, 0,
        OP_JMP, 0x00, 0x00, OP_JZ, 0x00, 0x00, OP_JNZ, 0x00, 0x00, OP_JC, 0x00, 0x00,
        OP_JNC, 0x00, 0x00, OP_JN, 0x00, 0x00, OP_CALL, 0x00, 0x00, OP_RET,
        OP_PUSH, 0, OP_POP, 1, OP_PHX, OP_PLX,
    };
    const std::string src =
        "start: nop\n hlt\n wai\n"
        " ld a, 5\n"
        " ld b, c\n"
        " ld d, [$1234]\n"
        " ld b, [x]\n"
        " st [$8000], c\n"
        " st [X], A\n"
        " ldx $ABCD\n"
        " inx\n dex\n"
        " in a, $10\n"
        " out $11, b\n"
        " out $24, 0\n"
        " add a, b\n add a, 1\n"
        " sub c, d\n sub d, 2\n"
        " and a, b\n and a, $0F\n"
        " or a, b\n or a, $F0\n"
        " xor a, a\n"
        " cmp a, b\n cmp a, 10\n"
        " inc a\n dec b\n shl c\n shr d\n not a\n"
        " jmp start\n jz start\n jnz start\n jc start\n jnc start\n jn start\n call start\n ret\n"
        " push a\n pop b\n phx\n plx\n";
    CHECK(bytes(src) == expect);
}

TEST(asm_number_formats_and_expressions) {
    CHECK(bytes(".db 42, $2A, 0x2A, %101010, 'A', '\\n'") ==
          (std::vector<uint8_t>{42, 42, 42, 42, 'A', '\n'}));
    CHECK(bytes(".equ BASE, $8000\n.dw BASE + 2, BASE - 1\n.db <$1234, >$1234, -1") ==
          (std::vector<uint8_t>{0x02, 0x80, 0xFF, 0x7F, 0x34, 0x12, 0xFF}));
    CHECK(bytes(".db (2 + 3) - 1") == (std::vector<uint8_t>{4}));
}

TEST(asm_strings_and_escapes) {
    CHECK(bytes(".db \"PLAY \\x10\", 0") == (std::vector<uint8_t>{'P', 'L', 'A', 'Y', ' ', 0x10, 0}));
    CHECK(bytes(".db \"a;b,c\"  ; comment with \"quotes\"") == (std::vector<uint8_t>{'a', ';', 'b', ',', 'c'}));
    CHECK(bytes("OUT $23, ':'") == (std::vector<uint8_t>{OP_OUTI, 0x23, ':'}));
    CHECK(bytes("OUT $23, ';'") == (std::vector<uint8_t>{OP_OUTI, 0x23, ';'}));
}

TEST(asm_forward_and_backward_labels) {
    const std::vector<uint8_t> b = bytes(
        "    JMP end\n"     // 0: 50 07 00
        "mid: NOP\n"        // 3
        "    JMP mid\n"     // 4: 50 03 00
        "end: HLT\n");      // 7
    CHECK(b == (std::vector<uint8_t>{OP_JMP, 7, 0, OP_NOP, OP_JMP, 3, 0, OP_HLT}));
}

TEST(asm_org_and_symbols) {
    AsmResult r = assemble(".org $100\nentry: NOP\n.org $200\ndata: .db 1\n");
    CHECK(r.ok());
    CHECK_EQ(r.symbols["entry"], 0x100);
    CHECK_EQ(r.symbols["data"], 0x200);
    CHECK_EQ(r.min_addr, 0x100);
    CHECK_EQ(r.max_addr, 0x201u);
    CHECK_EQ(r.image[0x200], 1);
}

TEST(asm_label_on_own_line_and_case) {
    AsmResult r = assemble("Loop:\n  inc A\n  JMP Loop\n");
    CHECK(r.ok());
    CHECK_EQ(r.symbols["Loop"], 0);
}

TEST(asm_errors) {
    CHECK(has_error("FOO A", "unknown instruction", 1));
    CHECK(has_error("NOP\nLD A, [X], B", "invalid operands", 2));
    CHECK(has_error("ST A, 5", "invalid operands"));
    CHECK(has_error("JMP nowhere", "undefined symbol 'nowhere'"));
    CHECK(has_error("LD A, 300", "does not fit in 8 bits"));
    CHECK(has_error("x1: NOP\nx1: NOP", "duplicate symbol", 2));
    CHECK(has_error(".equ A, 5", "register name"));
    CHECK(has_error(".org later\nlater: NOP", "undefined symbol"));
    CHECK(has_error(".db \"abc", "unterminated string"));
    CHECK(has_error(".db \"\\q\"", "bad escape"));
    CHECK(has_error(".bogus 1", "unknown directive"));
    CHECK(has_error("LD A, 5 5", "trailing text"));
    CHECK(has_error("LD A, $", "malformed number"));
}

TEST(asm_reports_all_errors_with_lines) {
    AsmResult r = assemble("FOO\nNOP\nBAR\n");
    CHECK_EQ(r.errors.size(), 2u);
    if (r.errors.size() == 2) {
        CHECK_EQ(r.errors[0].line, 1);
        CHECK_EQ(r.errors[1].line, 3);
    }
    CHECK(r.error_text().find("line 3:") != std::string::npos);
}
