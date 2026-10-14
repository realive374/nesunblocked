// VCR-1: the 8-bit CPU that runs the VCR firmware.
#pragma once

#include <cstdint>
#include <string>

#include "core/bus.h"
#include "core/isa.h"

namespace rt90 {

struct CpuRegs {
    uint8_t r[kNumRegs] = {0, 0, 0, 0};  // A, B, C, D
    uint16_t pc = 0;
    uint16_t sp = memmap::STACK_RESET;
    uint16_t x = 0;  // 16-bit index register
    uint8_t flags = 0;
};

enum class CpuFault : uint8_t {
    None,
    IllegalOpcode,
    BadRegister,
    StackOverflow,
    StackUnderflow,
    ExecFromIo,
};

const char* fault_name(CpuFault f);

class Cpu {
public:
    explicit Cpu(Bus& bus) : bus_(bus) {}

    void reset(uint16_t pc = memmap::ROM_START);

    // Executes one instruction and returns the cycles it took. Returns 1 idle
    // cycle while halted, faulted or waiting for vsync.
    int step();

    // Runs for (at least) `cycles` cycles. Time spent in WAI is counted as idle
    // cycles, so this always consumes the full budget. Returns cycles consumed.
    uint64_t run(uint64_t cycles);

    // Vertical sync from the video hardware: releases a pending WAI.
    void vsync() { waiting_ = false; }

    const CpuRegs& regs() const { return regs_; }
    CpuRegs& regs() { return regs_; }
    bool halted() const { return halted_; }
    bool waiting() const { return waiting_; }
    CpuFault fault() const { return fault_; }
    uint16_t fault_pc() const { return fault_pc_; }
    uint64_t total_cycles() const { return total_cycles_; }
    uint64_t busy_cycles() const { return busy_cycles_; }
    uint64_t instructions() const { return instructions_; }

    // One-line register dump, e.g. for the debug overlay and logs.
    std::string describe() const;

private:
    uint8_t fetch8();
    uint16_t fetch16();
    void push8(uint8_t v);
    uint8_t pop8();
    void set_zn(uint8_t v);
    bool reg_ok(int r);
    void raise(CpuFault f);

    Bus& bus_;
    CpuRegs regs_;
    bool halted_ = false;
    bool waiting_ = false;
    CpuFault fault_ = CpuFault::None;
    uint16_t fault_pc_ = 0;
    uint16_t insn_pc_ = 0;
    uint64_t total_cycles_ = 0;
    uint64_t busy_cycles_ = 0;
    uint64_t instructions_ = 0;
};

}  // namespace rt90
