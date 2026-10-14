#include "core/cpu.h"

#include <array>
#include <cstdio>

namespace rt90 {

namespace {

// 256-entry decode table built from kOpTable at startup.
struct DecodeTable {
    std::array<const OpInfo*, 256> ops{};
    DecodeTable() {
        ops.fill(nullptr);
        for (const OpInfo& op : kOpTable) ops[op.code] = &op;
    }
};

const DecodeTable& decode_table() {
    static const DecodeTable table;
    return table;
}

}  // namespace

const OpInfo* op_info(uint8_t code) { return decode_table().ops[code]; }

const char* fault_name(CpuFault f) {
    switch (f) {
        case CpuFault::None:           return "none";
        case CpuFault::IllegalOpcode:  return "illegal opcode";
        case CpuFault::BadRegister:    return "bad register";
        case CpuFault::StackOverflow:  return "stack overflow";
        case CpuFault::StackUnderflow: return "stack underflow";
        case CpuFault::ExecFromIo:     return "exec from I/O page";
    }
    return "?";
}

void Cpu::reset(uint16_t pc) {
    regs_ = CpuRegs{};
    regs_.pc = pc;
    halted_ = false;
    waiting_ = false;
    fault_ = CpuFault::None;
    fault_pc_ = 0;
}

uint8_t Cpu::fetch8() { return bus_.read(regs_.pc++); }

uint16_t Cpu::fetch16() {
    uint16_t lo = fetch8();
    uint16_t hi = fetch8();
    return static_cast<uint16_t>(lo | (hi << 8));
}

void Cpu::push8(uint8_t v) {
    if (regs_.sp <= memmap::STACK_START) {
        raise(CpuFault::StackOverflow);
        return;
    }
    --regs_.sp;
    bus_.write(regs_.sp, v);
}

uint8_t Cpu::pop8() {
    if (regs_.sp >= memmap::STACK_RESET) {
        raise(CpuFault::StackUnderflow);
        return 0;
    }
    return bus_.read(regs_.sp++);
}

void Cpu::set_zn(uint8_t v) {
    regs_.flags &= static_cast<uint8_t>(~(FLAG_Z | FLAG_N));
    if (v == 0) regs_.flags |= FLAG_Z;
    if (v & 0x80) regs_.flags |= FLAG_N;
}

bool Cpu::reg_ok(int r) {
    if (r >= 0 && r < kNumRegs) return true;
    raise(CpuFault::BadRegister);
    return false;
}

void Cpu::raise(CpuFault f) {
    if (fault_ != CpuFault::None) return;
    fault_ = f;
    fault_pc_ = insn_pc_;
    halted_ = true;
}

int Cpu::step() {
    if (halted_ || waiting_) {
        ++total_cycles_;
        return 1;
    }
    insn_pc_ = regs_.pc;
    if (regs_.pc >= memmap::IO_START) {
        raise(CpuFault::ExecFromIo);
        ++total_cycles_;
        return 1;
    }

    const uint8_t code = fetch8();
    const OpInfo* info = op_info(code);
    if (!info) {
        raise(CpuFault::IllegalOpcode);
        ++total_cycles_;
        return 1;
    }

    uint8_t* R = regs_.r;
    uint8_t& F = regs_.flags;

    auto set_c = [&F](bool c) {
        if (c) F |= FLAG_C;
        else F &= static_cast<uint8_t>(~FLAG_C);
    };
    auto alu = [&](uint8_t op, uint8_t& dst, uint8_t src) {
        switch (op) {
            case OP_ADD: {
                unsigned sum = unsigned(dst) + src;
                dst = static_cast<uint8_t>(sum);
                set_c(sum > 0xFF);
                set_zn(dst);
                break;
            }
            case OP_SUB:
                set_c(dst < src);
                dst = static_cast<uint8_t>(dst - src);
                set_zn(dst);
                break;
            case OP_CMP:
                set_c(dst < src);
                set_zn(static_cast<uint8_t>(dst - src));
                break;
            case OP_AND: dst &= src; set_zn(dst); break;
            case OP_OR:  dst |= src; set_zn(dst); break;
            case OP_XOR: dst ^= src; set_zn(dst); break;
            default: break;
        }
    };
    auto branch = [&](bool cond) {
        uint16_t target = fetch16();
        if (cond) regs_.pc = target;
    };

    switch (code) {
        case OP_NOP: break;
        case OP_HLT: halted_ = true; break;
        case OP_WAI: waiting_ = true; break;

        case OP_LDI: {
            int r = fetch8();
            uint8_t imm = fetch8();
            if (!reg_ok(r)) break;
            R[r] = imm;
            set_zn(imm);
            break;
        }
        case OP_MOV: {
            uint8_t rr = fetch8();
            int d = rr >> 4, s = rr & 0x0F;
            if (!reg_ok(d) || !reg_ok(s)) break;
            R[d] = R[s];
            set_zn(R[d]);
            break;
        }
        case OP_LDM: {
            int r = fetch8();
            uint16_t addr = fetch16();
            if (!reg_ok(r)) break;
            R[r] = bus_.read(addr);
            set_zn(R[r]);
            break;
        }
        case OP_STM: {
            int r = fetch8();
            uint16_t addr = fetch16();
            if (!reg_ok(r)) break;
            bus_.write(addr, R[r]);
            break;
        }
        case OP_LDXR: {
            int r = fetch8();
            if (!reg_ok(r)) break;
            R[r] = bus_.read(regs_.x);
            set_zn(R[r]);
            break;
        }
        case OP_STXR: {
            int r = fetch8();
            if (!reg_ok(r)) break;
            bus_.write(regs_.x, R[r]);
            break;
        }
        case OP_LDXI: regs_.x = fetch16(); break;
        case OP_INX: ++regs_.x; break;
        case OP_DEX: --regs_.x; break;

        case OP_IN: {
            int r = fetch8();
            uint8_t port = fetch8();
            if (!reg_ok(r)) break;
            R[r] = bus_.read(static_cast<uint16_t>(memmap::IO_START | port));
            set_zn(R[r]);
            break;
        }
        case OP_OUT: {
            int r = fetch8();
            uint8_t port = fetch8();
            if (!reg_ok(r)) break;
            bus_.write(static_cast<uint16_t>(memmap::IO_START | port), R[r]);
            break;
        }
        case OP_OUTI: {
            uint8_t port = fetch8();
            uint8_t imm = fetch8();
            bus_.write(static_cast<uint16_t>(memmap::IO_START | port), imm);
            break;
        }

        case OP_ADD: case OP_SUB: case OP_AND: case OP_OR: case OP_XOR: case OP_CMP: {
            uint8_t rr = fetch8();
            int d = rr >> 4, s = rr & 0x0F;
            if (!reg_ok(d) || !reg_ok(s)) break;
            alu(code, R[d], R[s]);
            break;
        }
        case OP_ADDI: case OP_SUBI: case OP_ANDI: case OP_ORI: case OP_CMPI: {
            int r = fetch8();
            uint8_t imm = fetch8();
            if (!reg_ok(r)) break;
            static constexpr uint8_t kRegForm[] = {OP_ADD, OP_SUB, OP_AND, OP_OR, OP_CMP};
            alu(kRegForm[code - OP_ADDI], R[r], imm);
            break;
        }

        case OP_INC: case OP_DEC: case OP_SHL: case OP_SHR: case OP_NOT: {
            int r = fetch8();
            if (!reg_ok(r)) break;
            uint8_t& v = R[r];
            switch (code) {
                case OP_INC: ++v; break;
                case OP_DEC: --v; break;
                case OP_SHL: set_c(v & 0x80); v = static_cast<uint8_t>(v << 1); break;
                case OP_SHR: set_c(v & 0x01); v = static_cast<uint8_t>(v >> 1); break;
                case OP_NOT: v = static_cast<uint8_t>(~v); break;
                default: break;
            }
            set_zn(v);
            break;
        }

        case OP_JMP: branch(true); break;
        case OP_JZ:  branch(F & FLAG_Z); break;
        case OP_JNZ: branch(!(F & FLAG_Z)); break;
        case OP_JC:  branch(F & FLAG_C); break;
        case OP_JNC: branch(!(F & FLAG_C)); break;
        case OP_JN:  branch(F & FLAG_N); break;

        case OP_CALL: {
            uint16_t target = fetch16();
            push8(static_cast<uint8_t>(regs_.pc >> 8));
            push8(static_cast<uint8_t>(regs_.pc & 0xFF));
            if (fault_ == CpuFault::None) regs_.pc = target;
            break;
        }
        case OP_RET: {
            uint16_t lo = pop8();
            uint16_t hi = pop8();
            if (fault_ == CpuFault::None) regs_.pc = static_cast<uint16_t>(lo | (hi << 8));
            break;
        }
        case OP_PUSH: {
            int r = fetch8();
            if (reg_ok(r)) push8(R[r]);
            break;
        }
        case OP_POP: {
            int r = fetch8();
            if (reg_ok(r)) {
                uint8_t v = pop8();
                if (fault_ == CpuFault::None) R[r] = v;
            }
            break;
        }
        case OP_PHX:
            push8(static_cast<uint8_t>(regs_.x >> 8));
            push8(static_cast<uint8_t>(regs_.x & 0xFF));
            break;
        case OP_PLX: {
            uint16_t lo = pop8();
            uint16_t hi = pop8();
            if (fault_ == CpuFault::None) regs_.x = static_cast<uint16_t>(lo | (hi << 8));
            break;
        }
        default:
            raise(CpuFault::IllegalOpcode);
            break;
    }

    ++instructions_;
    total_cycles_ += info->cycles;
    busy_cycles_ += info->cycles;
    return info->cycles;
}

uint64_t Cpu::run(uint64_t cycles) {
    uint64_t used = 0;
    while (used < cycles) {
        if (halted_ || waiting_) {
            // Idle until the end of the budget (the next vsync will wake us).
            uint64_t idle = cycles - used;
            total_cycles_ += idle;
            used += idle;
            break;
        }
        used += static_cast<uint64_t>(step());
    }
    return used;
}

std::string Cpu::describe() const {
    char buf[160];
    std::snprintf(buf, sizeof buf, "PC=%04X SP=%04X X=%04X A=%02X B=%02X C=%02X D=%02X F=%c%c%c%s%s",
                  regs_.pc, regs_.sp, regs_.x, regs_.r[0], regs_.r[1], regs_.r[2], regs_.r[3],
                  (regs_.flags & FLAG_N) ? 'N' : '-', (regs_.flags & FLAG_C) ? 'C' : '-',
                  (regs_.flags & FLAG_Z) ? 'Z' : '-', waiting_ ? " WAI" : "",
                  fault_ != CpuFault::None ? " FAULT" : (halted_ ? " HLT" : ""));
    return buf;
}

}  // namespace rt90
