#include "core/assembler.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

#include "core/isa.h"

namespace rt90 {

namespace {

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::string upper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

bool is_ident_start(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '.'; }
bool is_ident_char(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.'; }

// Removes a trailing ';' comment, ignoring ';' inside quotes.
std::string strip_comment(const std::string& line) {
    char quote = 0;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (quote) {
            if (c == '\\' && i + 1 < line.size()) ++i;
            else if (c == quote) quote = 0;
        } else if (c == '"' || c == '\'') {
            quote = c;
        } else if (c == ';') {
            return line.substr(0, i);
        }
    }
    return line;
}

// Splits on top-level commas (not inside quotes or brackets).
std::vector<std::string> split_operands(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    char quote = 0;
    int depth = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (quote) {
            cur += c;
            if (c == '\\' && i + 1 < s.size()) cur += s[++i];
            else if (c == quote) quote = 0;
            continue;
        }
        if (c == '"' || c == '\'') quote = c;
        if (c == '[' || c == '(') ++depth;
        if (c == ']' || c == ')') --depth;
        if (c == ',' && depth == 0) {
            out.push_back(trim(cur));
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!trim(cur).empty() || !out.empty()) out.push_back(trim(cur));
    return out;
}

// Decodes one escape sequence starting after the backslash at s[i].
// Advances i past the sequence. Returns -1 on error.
int decode_escape(const std::string& s, size_t& i) {
    if (i >= s.size()) return -1;
    char c = s[i++];
    switch (c) {
        case 'n': return '\n';
        case 't': return '\t';
        case 'r': return '\r';
        case '0': return 0;
        case '\\': return '\\';
        case '"': return '"';
        case '\'': return '\'';
        case 'x': {
            int v = 0, digits = 0;
            while (digits < 2 && i < s.size() && std::isxdigit(static_cast<unsigned char>(s[i]))) {
                char h = s[i++];
                v = v * 16 + (std::isdigit(static_cast<unsigned char>(h)) ? h - '0' : (std::toupper(h) - 'A' + 10));
                ++digits;
            }
            return digits ? v : -1;
        }
        default: return -1;
    }
}

enum class OpKind { Reg, Imm, Mem, MemX, RegX, Str };

struct Operand {
    OpKind kind;
    int reg = -1;
    std::string expr;  // for Imm / Mem
};

int reg_index(const std::string& s) {
    std::string u = upper(trim(s));
    for (int i = 0; i < kNumRegs; ++i)
        if (u == kRegNames[i]) return i;
    return -1;
}

Operand classify(const std::string& raw) {
    std::string s = trim(raw);
    Operand op{OpKind::Imm, -1, s};
    if (s.size() >= 2 && s.front() == '"') {
        op.kind = OpKind::Str;
        return op;
    }
    int r = reg_index(s);
    if (r >= 0) {
        op.kind = OpKind::Reg;
        op.reg = r;
        return op;
    }
    if (upper(s) == "X") {
        op.kind = OpKind::RegX;
        return op;
    }
    if (s.size() >= 2 && s.front() == '[' && s.back() == ']') {
        std::string inner = trim(s.substr(1, s.size() - 2));
        if (upper(inner) == "X") {
            op.kind = OpKind::MemX;
        } else {
            op.kind = OpKind::Mem;
            op.expr = inner;
        }
    }
    return op;
}

char kind_letter(OpKind k) {
    switch (k) {
        case OpKind::Reg:  return 'R';
        case OpKind::Imm:  return 'I';
        case OpKind::Mem:  return 'M';
        case OpKind::MemX: return 'X';
        case OpKind::RegX: return 'x';
        case OpKind::Str:  return 'S';
    }
    return '?';
}

// Mnemonic + operand pattern -> opcode.
//   R reg, I immediate/label, M [addr], X [X]
struct Form {
    const char* mnemonic;
    const char* pattern;
    uint8_t opcode;
};

constexpr Form kForms[] = {
    {"NOP", "", OP_NOP},     {"HLT", "", OP_HLT},     {"WAI", "", OP_WAI},
    {"LD", "RI", OP_LDI},    {"LD", "RR", OP_MOV},    {"LD", "RM", OP_LDM},   {"LD", "RX", OP_LDXR},
    {"LDI", "RI", OP_LDI},   {"MOV", "RR", OP_MOV},
    {"ST", "MR", OP_STM},    {"ST", "XR", OP_STXR},
    {"LDX", "I", OP_LDXI},   {"INX", "", OP_INX},     {"DEX", "", OP_DEX},
    {"IN", "RI", OP_IN},     {"OUT", "IR", OP_OUT},   {"OUT", "II", OP_OUTI},
    {"ADD", "RR", OP_ADD},   {"ADD", "RI", OP_ADDI},  {"SUB", "RR", OP_SUB},  {"SUB", "RI", OP_SUBI},
    {"AND", "RR", OP_AND},   {"AND", "RI", OP_ANDI},  {"OR", "RR", OP_OR},    {"OR", "RI", OP_ORI},
    {"XOR", "RR", OP_XOR},   {"CMP", "RR", OP_CMP},   {"CMP", "RI", OP_CMPI},
    {"INC", "R", OP_INC},    {"DEC", "R", OP_DEC},    {"SHL", "R", OP_SHL},   {"SHR", "R", OP_SHR},
    {"NOT", "R", OP_NOT},
    {"JMP", "I", OP_JMP},    {"JZ", "I", OP_JZ},      {"JNZ", "I", OP_JNZ},   {"JC", "I", OP_JC},
    {"JNC", "I", OP_JNC},    {"JN", "I", OP_JN},      {"CALL", "I", OP_CALL}, {"RET", "", OP_RET},
    {"PUSH", "R", OP_PUSH},  {"POP", "R", OP_POP},    {"PHX", "", OP_PHX},    {"PLX", "", OP_PLX},
};

bool is_mnemonic(const std::string& m) {
    for (const Form& f : kForms)
        if (m == f.mnemonic) return true;
    return false;
}

struct Stmt {
    int line = 0;
    std::string label;
    std::string op;  // uppercased mnemonic or directive
    std::vector<std::string> args;
    std::vector<Operand> operands;
    uint8_t opcode = 0;
    bool is_insn = false;
    uint16_t addr = 0;
};

class Assembler {
public:
    explicit Assembler(AsmResult& out) : out_(out) { out_.image.assign(0x10000, 0); }

    void run(const std::string& source) {
        parse(source);
        pass1();
        if (!out_.errors.empty()) return;
        pass2();
    }

private:
    void error(int line, const std::string& msg) { out_.errors.push_back({line, msg}); }

    void parse(const std::string& source) {
        std::istringstream in(source);
        std::string raw;
        int line_no = 0;
        while (std::getline(in, raw)) {
            ++line_no;
            std::string line = trim(strip_comment(raw));
            if (line.empty()) continue;
            Stmt st;
            st.line = line_no;
            // Optional "label:".
            size_t i = 0;
            if (is_ident_start(line[0]) && line[0] != '.') {
                while (i < line.size() && is_ident_char(line[i])) ++i;
                if (i < line.size() && line[i] == ':') {
                    st.label = line.substr(0, i);
                    line = trim(line.substr(i + 1));
                }
            }
            if (!line.empty()) {
                size_t j = 0;
                while (j < line.size() && !std::isspace(static_cast<unsigned char>(line[j]))) ++j;
                st.op = upper(line.substr(0, j));
                std::string rest = trim(line.substr(j));
                if (!rest.empty()) st.args = split_operands(rest);
            }
            stmts_.push_back(std::move(st));
        }
    }

    void define(const std::string& name, int64_t value, int line) {
        if (reg_index(name) >= 0 || upper(name) == "X") {
            error(line, "'" + name + "' is a register name and cannot be a symbol");
            return;
        }
        if (out_.symbols.count(name)) {
            error(line, "duplicate symbol '" + name + "'");
            return;
        }
        out_.symbols[name] = static_cast<uint16_t>(value & 0xFFFF);
    }

    // --- expression evaluation -------------------------------------------
    struct Eval {
        const std::map<std::string, uint16_t>& syms;
        const std::string& s;
        size_t pos = 0;
        bool undefined = false;
        std::string err;

        void ws() {
            while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) ++pos;
        }
        int64_t expr() {
            int64_t v = term();
            for (;;) {
                ws();
                if (pos < s.size() && (s[pos] == '+' || s[pos] == '-')) {
                    char o = s[pos++];
                    int64_t r = term();
                    v = (o == '+') ? v + r : v - r;
                } else {
                    return v;
                }
            }
        }
        int64_t term() {
            ws();
            if (pos >= s.size()) {
                if (err.empty()) err = "expected a value";
                return 0;
            }
            char c = s[pos];
            if (c == '-') { ++pos; return -term(); }
            if (c == '<') { ++pos; return term() & 0xFF; }
            if (c == '>') { ++pos; return (term() >> 8) & 0xFF; }
            if (c == '(') {
                ++pos;
                int64_t v = expr();
                ws();
                if (pos < s.size() && s[pos] == ')') ++pos;
                else if (err.empty()) err = "missing ')'";
                return v;
            }
            if (c == '\'') {
                ++pos;
                int v;
                if (pos < s.size() && s[pos] == '\\') {
                    ++pos;
                    v = decode_escape(s, pos);
                } else if (pos < s.size()) {
                    v = static_cast<unsigned char>(s[pos++]);
                } else {
                    v = -1;
                }
                if (v < 0 || pos >= s.size() || s[pos] != '\'') {
                    if (err.empty()) err = "bad character literal";
                    return 0;
                }
                ++pos;
                return v;
            }
            if (c == '$' || c == '%' || std::isdigit(static_cast<unsigned char>(c))) return number();
            if (is_ident_start(c)) {
                size_t start = pos;
                while (pos < s.size() && is_ident_char(s[pos])) ++pos;
                std::string name = s.substr(start, pos - start);
                auto it = syms.find(name);
                if (it == syms.end()) {
                    undefined = true;
                    if (err.empty()) err = "undefined symbol '" + name + "'";
                    return 0;
                }
                return it->second;
            }
            if (err.empty()) err = std::string("unexpected '") + c + "'";
            ++pos;
            return 0;
        }
        int64_t number() {
            int base = 10;
            if (s[pos] == '$') { base = 16; ++pos; }
            else if (s[pos] == '%') { base = 2; ++pos; }
            else if (s[pos] == '0' && pos + 1 < s.size() && (s[pos + 1] == 'x' || s[pos + 1] == 'X')) {
                base = 16;
                pos += 2;
            }
            size_t start = pos;
            int64_t v = 0;
            while (pos < s.size()) {
                char c = static_cast<char>(std::toupper(static_cast<unsigned char>(s[pos])));
                int d;
                if (c >= '0' && c <= '9') d = c - '0';
                else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
                else break;
                if (d >= base) break;
                v = v * base + d;
                ++pos;
                if (v > 0xFFFFFF) break;
            }
            if (pos == start && err.empty()) err = "malformed number";
            return v;
        }
    };

    // Evaluates `text`. If `must_resolve` is false, undefined symbols are
    // tolerated (returns 0, ok=true) - used for sizing in pass 1.
    bool eval(const std::string& text, int line, bool must_resolve, int64_t& value) {
        Eval ev{out_.symbols, text, 0, false, {}};
        value = ev.expr();
        ev.ws();
        if (ev.err.empty() && ev.pos != text.size()) ev.err = "unexpected trailing text in '" + text + "'";
        if (!ev.err.empty()) {
            if (ev.undefined && !must_resolve) return true;
            error(line, ev.err);
            return false;
        }
        return true;
    }

    // Parses a "..." string literal into bytes.
    bool parse_string(const std::string& s, int line, std::vector<uint8_t>& bytes) {
        if (s.size() < 2 || s.front() != '"' || s.back() != '"') {
            error(line, "unterminated string");
            return false;
        }
        for (size_t i = 1; i + 1 < s.size();) {
            if (s[i] == '\\') {
                ++i;
                int v = decode_escape(s, i);
                if (v < 0) {
                    error(line, "bad escape sequence in string");
                    return false;
                }
                bytes.push_back(static_cast<uint8_t>(v));
            } else {
                bytes.push_back(static_cast<uint8_t>(s[i++]));
            }
        }
        return true;
    }

    // --- pass 1: addresses, sizes, opcode selection ---------------------
    void pass1() {
        uint32_t pc = 0;
        for (Stmt& st : stmts_) {
            if (!st.label.empty()) define(st.label, pc, st.line);
            st.addr = static_cast<uint16_t>(pc);
            if (st.op.empty()) continue;

            if (st.op == ".ORG") {
                int64_t v;
                if (st.args.size() != 1) error(st.line, ".org takes one argument");
                else if (eval(st.args[0], st.line, true, v)) {
                    if (v < 0 || v > 0xFFFF) error(st.line, ".org address out of range");
                    else pc = static_cast<uint32_t>(v);
                }
                if (!st.label.empty()) out_.symbols[st.label] = static_cast<uint16_t>(pc);
                st.addr = static_cast<uint16_t>(pc);
                continue;
            }
            if (st.op == ".EQU") {
                int64_t v;
                if (!st.label.empty()) error(st.line, ".equ cannot have a label");
                if (st.args.size() != 2) error(st.line, ".equ takes NAME, value");
                else if (eval(st.args[1], st.line, true, v)) define(st.args[0], v, st.line);
                continue;
            }
            if (st.op == ".DB") {
                if (st.args.empty()) error(st.line, ".db needs at least one value");
                for (const std::string& a : st.args) {
                    if (!a.empty() && a.front() == '"') {
                        std::vector<uint8_t> b;
                        if (parse_string(a, st.line, b)) pc += static_cast<uint32_t>(b.size());
                    } else {
                        pc += 1;
                    }
                }
                continue;
            }
            if (st.op == ".DW") {
                if (st.args.empty()) error(st.line, ".dw needs at least one value");
                pc += 2 * static_cast<uint32_t>(st.args.size());
                continue;
            }
            if (!st.op.empty() && st.op[0] == '.') {
                error(st.line, "unknown directive '" + st.op + "'");
                continue;
            }
            if (!is_mnemonic(st.op)) {
                error(st.line, "unknown instruction '" + st.op + "'");
                continue;
            }

            std::string pattern;
            for (const std::string& a : st.args) {
                Operand o = classify(a);
                st.operands.push_back(o);
                pattern += kind_letter(o.kind);
            }
            bool found = false;
            for (const Form& f : kForms) {
                if (st.op == f.mnemonic && pattern == f.pattern) {
                    st.opcode = f.opcode;
                    found = true;
                    break;
                }
            }
            if (!found) {
                error(st.line, "invalid operands for " + st.op);
                continue;
            }
            st.is_insn = true;
            pc += 1 + static_cast<uint32_t>(fmt_size(op_info(st.opcode)->fmt));
            if (pc > 0x10000) error(st.line, "program runs past end of memory");
        }
    }

    // --- pass 2: emit ---------------------------------------------------
    void emit(uint32_t& pc, uint8_t b) {
        if (pc > 0xFFFF) return;
        out_.image[pc] = b;
        out_.min_addr = std::min<uint16_t>(out_.min_addr, static_cast<uint16_t>(pc));
        ++pc;
        out_.max_addr = std::max(out_.max_addr, pc);
    }

    bool value8(const std::string& e, int line, uint8_t& out, const char* what) {
        int64_t v;
        if (!eval(e, line, true, v)) return false;
        if (v < -128 || v > 255) {
            error(line, std::string(what) + " value " + std::to_string(v) + " does not fit in 8 bits");
            return false;
        }
        out = static_cast<uint8_t>(v & 0xFF);
        return true;
    }

    bool value16(const std::string& e, int line, uint16_t& out) {
        int64_t v;
        if (!eval(e, line, true, v)) return false;
        if (v < -32768 || v > 0xFFFF) {
            error(line, "value " + std::to_string(v) + " does not fit in 16 bits");
            return false;
        }
        out = static_cast<uint16_t>(v & 0xFFFF);
        return true;
    }

    void pass2() {
        for (const Stmt& st : stmts_) {
            uint32_t pc = st.addr;
            if (st.op == ".DB") {
                for (const std::string& a : st.args) {
                    if (!a.empty() && a.front() == '"') {
                        std::vector<uint8_t> b;
                        if (parse_string(a, st.line, b))
                            for (uint8_t x : b) emit(pc, x);
                    } else {
                        uint8_t v = 0;
                        value8(a, st.line, v, "byte");
                        emit(pc, v);
                    }
                }
                continue;
            }
            if (st.op == ".DW") {
                for (const std::string& a : st.args) {
                    uint16_t v = 0;
                    value16(a, st.line, v);
                    emit(pc, static_cast<uint8_t>(v & 0xFF));
                    emit(pc, static_cast<uint8_t>(v >> 8));
                }
                continue;
            }
            if (!st.is_insn) continue;

            const OpInfo* info = op_info(st.opcode);
            const std::vector<Operand>& o = st.operands;
            emit(pc, st.opcode);
            switch (info->fmt) {
                case Fmt::None: break;
                case Fmt::R: {
                    // The register may be either operand: LD A,[X] / ST [X],A.
                    const Operand& reg = (o.size() > 1 && o[0].kind != OpKind::Reg) ? o[1] : o[0];
                    emit(pc, static_cast<uint8_t>(reg.reg));
                    break;
                }
                case Fmt::RR: emit(pc, static_cast<uint8_t>((o[0].reg << 4) | o[1].reg)); break;
                case Fmt::RI: {
                    // OUT port, reg is encoded as [reg][port].
                    const bool out = st.opcode == OP_OUT;
                    const Operand& reg = out ? o[1] : o[0];
                    const Operand& imm = out ? o[0] : o[1];
                    uint8_t v = 0;
                    value8(imm.expr, st.line, v, out || st.opcode == OP_IN ? "port" : "immediate");
                    emit(pc, static_cast<uint8_t>(reg.reg));
                    emit(pc, v);
                    break;
                }
                case Fmt::RM: {
                    // LD r,[addr] / ST [addr],r are both encoded as [reg][lo][hi].
                    const bool st_form = st.opcode == OP_STM;
                    const Operand& reg = st_form ? o[1] : o[0];
                    const Operand& mem = st_form ? o[0] : o[1];
                    uint16_t a = 0;
                    value16(mem.expr, st.line, a);
                    emit(pc, static_cast<uint8_t>(reg.reg));
                    emit(pc, static_cast<uint8_t>(a & 0xFF));
                    emit(pc, static_cast<uint8_t>(a >> 8));
                    break;
                }
                case Fmt::I16: {
                    uint16_t a = 0;
                    value16(o[0].expr, st.line, a);
                    emit(pc, static_cast<uint8_t>(a & 0xFF));
                    emit(pc, static_cast<uint8_t>(a >> 8));
                    break;
                }
                case Fmt::PI: {
                    uint8_t port = 0, imm = 0;
                    value8(o[0].expr, st.line, port, "port");
                    value8(o[1].expr, st.line, imm, "immediate");
                    emit(pc, port);
                    emit(pc, imm);
                    break;
                }
            }
        }
    }

    AsmResult& out_;
    std::vector<Stmt> stmts_;
};

}  // namespace

std::vector<uint8_t> AsmResult::slice(uint16_t origin) const {
    if (max_addr <= origin || image.empty()) return {};
    return std::vector<uint8_t>(image.begin() + origin, image.begin() + max_addr);
}

std::string AsmResult::error_text() const {
    std::string s;
    for (const AsmError& e : errors) s += "line " + std::to_string(e.line) + ": " + e.message + "\n";
    return s;
}

AsmResult assemble(const std::string& source) {
    AsmResult result;
    Assembler as(result);
    as.run(source);
    if (result.min_addr > result.max_addr) result.min_addr = 0;
    return result;
}

}  // namespace rt90
