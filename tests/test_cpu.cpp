// Unit tests for the VCR-1 CPU: every opcode, flags, stack and faults.
#include <vector>

#include "core/assembler.h"
#include "core/bus.h"
#include "core/cpu.h"
#include "test.h"

using namespace rt90;

namespace {

struct Rig {
    Bus bus;
    Cpu cpu{bus};

    // Loads raw bytes at 0x0000 and runs until HLT (or a step limit).
    void load(const std::vector<uint8_t>& code) {
        bus.load_rom(code);
        bus.clear_ram();
        cpu.reset();
    }
    void run(int max_steps = 10000) {
        for (int i = 0; i < max_steps && !cpu.halted() && !cpu.waiting(); ++i) cpu.step();
    }
    void asm_run(const std::string& src) {
        AsmResult r = assemble(src);
        CHECK(r.ok());
        if (!r.ok()) std::fprintf(stderr, "%s", r.error_text().c_str());
        load(r.slice(0));
        run();
    }
    uint8_t A() const { return cpu.regs().r[0]; }
    uint8_t B() const { return cpu.regs().r[1]; }
    uint8_t C() const { return cpu.regs().r[2]; }
    uint8_t D() const { return cpu.regs().r[3]; }
    bool Z() const { return cpu.regs().flags & FLAG_Z; }
    bool Cf() const { return cpu.regs().flags & FLAG_C; }
    bool N() const { return cpu.regs().flags & FLAG_N; }
};

// I/O device that records writes and returns port number + 1 on reads.
struct EchoIo : IoDevice {
    uint8_t last_port = 0, last_value = 0;
    int writes = 0;
    uint8_t io_read(uint8_t port) override { return static_cast<uint8_t>(port + 1); }
    void io_write(uint8_t port, uint8_t value) override {
        last_port = port;
        last_value = value;
        ++writes;
    }
};

}  // namespace

TEST(cpu_opcode_table_is_consistent) {
    CHECK(kOpCount >= 30);
    for (const OpInfo& op : kOpTable) {
        CHECK(op_info(op.code) == &op);
        CHECK(op.cycles >= 1);
    }
    int defined = 0;
    for (int c = 0; c < 256; ++c)
        if (op_info(static_cast<uint8_t>(c))) ++defined;
    CHECK_EQ(defined, kOpCount);
}

TEST(cpu_raw_ldi_mov_hlt) {
    Rig t;
    t.load({OP_LDI, 0, 0x42, OP_MOV, 0x10 /*B<-A*/, OP_HLT});
    t.run();
    CHECK_EQ(t.A(), 0x42);
    CHECK_EQ(t.B(), 0x42);
    CHECK(t.cpu.halted());
    CHECK(t.cpu.fault() == CpuFault::None);
    CHECK_EQ(t.cpu.regs().pc, 6);
    CHECK_EQ(t.cpu.total_cycles(), 2u + 1u + 1u);
}

TEST(cpu_nop_and_cycles) {
    Rig t;
    t.load({OP_NOP, OP_NOP, OP_HLT});
    CHECK_EQ(t.cpu.step(), 1);
    CHECK_EQ(t.cpu.step(), 1);
    t.run();
    CHECK_EQ(t.cpu.instructions(), 3u);
}

TEST(cpu_load_sets_zero_and_negative) {
    Rig t;
    t.asm_run("LD A, 0\nHLT");
    CHECK(t.Z());
    CHECK(!t.N());
    t.asm_run("LD A, $80\nHLT");
    CHECK(!t.Z());
    CHECK(t.N());
}

TEST(cpu_memory_load_store) {
    Rig t;
    t.asm_run(
        "LD A, $5A\n"
        "ST [$8100], A\n"
        "LD B, [$8100]\n"
        "HLT");
    CHECK_EQ(t.B(), 0x5A);
    CHECK_EQ(t.bus.peek(0x8100), 0x5A);
}

TEST(cpu_rom_is_write_protected) {
    Rig t;
    t.asm_run("LD A, $99\nST [$0100], A\nLD B, [$0100]\nHLT");
    CHECK_EQ(t.B(), 0);
    CHECK_EQ(t.bus.rom_write_violations(), 1u);
}

TEST(cpu_indexed_load_store_inx_dex) {
    Rig t;
    t.asm_run(
        "LDX $8200\n"
        "LD A, 7\n"
        "ST [X], A\n"
        "INX\n"
        "LD A, 9\n"
        "ST [X], A\n"
        "DEX\n"
        "LD C, [X]\n"
        "INX\n"
        "LD D, [X]\n"
        "HLT");
    CHECK_EQ(t.C(), 7);
    CHECK_EQ(t.D(), 9);
    CHECK_EQ(t.cpu.regs().x, 0x8201);
}

TEST(cpu_add_sets_carry) {
    Rig t;
    t.asm_run("LD A, 200\nLD B, 100\nADD A, B\nHLT");
    CHECK_EQ(t.A(), 44);
    CHECK(t.Cf());
    t.asm_run("LD A, 1\nADD A, 2\nHLT");
    CHECK_EQ(t.A(), 3);
    CHECK(!t.Cf());
    t.asm_run("LD A, 255\nADD A, 1\nHLT");
    CHECK_EQ(t.A(), 0);
    CHECK(t.Z());
    CHECK(t.Cf());
}

TEST(cpu_sub_sets_borrow) {
    Rig t;
    t.asm_run("LD A, 5\nLD B, 7\nSUB A, B\nHLT");
    CHECK_EQ(t.A(), 254);
    CHECK(t.Cf());
    CHECK(t.N());
    t.asm_run("LD A, 7\nSUB A, 7\nHLT");
    CHECK_EQ(t.A(), 0);
    CHECK(t.Z());
    CHECK(!t.Cf());
}

TEST(cpu_logic_ops) {
    Rig t;
    t.asm_run("LD A, %11001100\nLD B, %10101010\nAND A, B\nHLT");
    CHECK_EQ(t.A(), 0x88);
    t.asm_run("LD A, %11001100\nLD B, %10101010\nOR A, B\nHLT");
    CHECK_EQ(t.A(), 0xEE);
    t.asm_run("LD A, %11001100\nLD B, %10101010\nXOR A, B\nHLT");
    CHECK_EQ(t.A(), 0x66);
    t.asm_run("LD A, $F0\nAND A, $3C\nHLT");
    CHECK_EQ(t.A(), 0x30);
    t.asm_run("LD A, $F0\nOR A, $0F\nHLT");
    CHECK_EQ(t.A(), 0xFF);
    t.asm_run("LD A, $0F\nNOT A\nHLT");
    CHECK_EQ(t.A(), 0xF0);
    CHECK(t.N());
}

TEST(cpu_cmp_does_not_modify) {
    Rig t;
    t.asm_run("LD A, 10\nLD B, 10\nCMP A, B\nHLT");
    CHECK_EQ(t.A(), 10);
    CHECK(t.Z());
    CHECK(!t.Cf());
    t.asm_run("LD A, 3\nCMP A, 10\nHLT");
    CHECK_EQ(t.A(), 3);
    CHECK(!t.Z());
    CHECK(t.Cf());
}

TEST(cpu_inc_dec_shift) {
    Rig t;
    t.asm_run("LD A, 255\nINC A\nHLT");
    CHECK_EQ(t.A(), 0);
    CHECK(t.Z());
    t.asm_run("LD A, 0\nDEC A\nHLT");
    CHECK_EQ(t.A(), 255);
    CHECK(t.N());
    t.asm_run("LD A, $81\nSHL A\nHLT");
    CHECK_EQ(t.A(), 0x02);
    CHECK(t.Cf());
    t.asm_run("LD A, $81\nSHR A\nHLT");
    CHECK_EQ(t.A(), 0x40);
    CHECK(t.Cf());
}

TEST(cpu_branches) {
    Rig t;
    // Count down from 5, summing into B: 5+4+3+2+1 = 15.
    t.asm_run(
        "    LD A, 5\n"
        "    LD B, 0\n"
        "loop:\n"
        "    ADD B, A\n"
        "    DEC A\n"
        "    JNZ loop\n"
        "    HLT");
    CHECK_EQ(t.B(), 15);

    t.asm_run("LD A, 1\nCMP A, 2\nJC yes\nLD D, 1\nHLT\nyes: LD D, 2\nHLT");
    CHECK_EQ(t.D(), 2);
    t.asm_run("LD A, 3\nCMP A, 2\nJNC yes\nLD D, 1\nHLT\nyes: LD D, 2\nHLT");
    CHECK_EQ(t.D(), 2);
    t.asm_run("LD A, 1\nJZ no\nLD D, 3\nHLT\nno: LD D, 4\nHLT");
    CHECK_EQ(t.D(), 3);
    t.asm_run("LD A, $90\nJN neg\nLD D, 1\nHLT\nneg: LD D, 5\nHLT");
    CHECK_EQ(t.D(), 5);
    t.asm_run("JMP over\nLD D, 1\nover: LD D, 6\nHLT");
    CHECK_EQ(t.D(), 6);
}

TEST(cpu_call_ret_push_pop) {
    Rig t;
    t.asm_run(
        "    LD A, 1\n"
        "    LD B, 2\n"
        "    PUSH A\n"
        "    PUSH B\n"
        "    CALL sub\n"
        "    POP C\n"
        "    POP D\n"
        "    HLT\n"
        "sub:\n"
        "    LD A, 9\n"
        "    RET\n");
    CHECK_EQ(t.A(), 9);
    CHECK_EQ(t.C(), 2);
    CHECK_EQ(t.D(), 1);
    CHECK_EQ(t.cpu.regs().sp, memmap::STACK_RESET);
    CHECK(t.cpu.fault() == CpuFault::None);
}

TEST(cpu_phx_plx) {
    Rig t;
    t.asm_run("LDX $1234\nPHX\nLDX 0\nPLX\nHLT");
    CHECK_EQ(t.cpu.regs().x, 0x1234);
}

TEST(cpu_io_in_out_outi) {
    Rig t;
    EchoIo dev;
    t.bus.map_io(0x40, 0x4F, &dev);
    t.asm_run("IN A, $41\nLD B, 7\nOUT $42, B\nHLT");
    CHECK_EQ(t.A(), 0x42);
    CHECK_EQ(dev.last_port, 0x42);
    CHECK_EQ(dev.last_value, 7);
    t.asm_run("OUT $43, 99\nHLT");
    CHECK_EQ(dev.last_port, 0x43);
    CHECK_EQ(dev.last_value, 99);
    // Memory-mapped: plain LD/ST on $FF4x reach the same device.
    t.asm_run("LD A, 11\nST [$FF44], A\nLD B, [$FF45]\nHLT");
    CHECK_EQ(dev.last_port, 0x44);
    CHECK_EQ(dev.last_value, 11);
    CHECK_EQ(t.B(), 0x46);
    // Unmapped ports read as open bus.
    t.asm_run("IN A, $90\nHLT");
    CHECK_EQ(t.A(), 0xFF);
}

TEST(cpu_wai_waits_for_vsync) {
    Rig t;
    t.asm_run("LD A, 1\nWAI\nLD A, 2\nHLT");
    CHECK(t.cpu.waiting());
    CHECK_EQ(t.A(), 1);
    const uint64_t used = t.cpu.run(1000);  // idles through the whole budget
    CHECK_EQ(used, 1000u);
    CHECK_EQ(t.A(), 1);
    t.cpu.vsync();
    t.run();
    CHECK_EQ(t.A(), 2);
}

TEST(cpu_run_consumes_budget) {
    Rig t;
    t.asm_run("loop: INC A\nJMP loop");  // never halts; asm_run stops after 10000 steps
    const uint64_t before = t.cpu.total_cycles();
    const uint64_t used = t.cpu.run(5000);
    CHECK(used >= 5000u);
    CHECK(used < 5000u + 8u);
    CHECK_EQ(t.cpu.total_cycles() - before, used);
}

TEST(cpu_fault_illegal_opcode) {
    Rig t;
    t.load({OP_NOP, 0xEE});
    t.run();
    CHECK(t.cpu.fault() == CpuFault::IllegalOpcode);
    CHECK_EQ(t.cpu.fault_pc(), 1);
    CHECK(t.cpu.halted());
}

TEST(cpu_fault_bad_register) {
    Rig t;
    t.load({OP_LDI, 7, 1});
    t.run();
    CHECK(t.cpu.fault() == CpuFault::BadRegister);
}

TEST(cpu_fault_stack_overflow_and_underflow) {
    Rig t;
    t.asm_run("loop: PUSH A\nJMP loop");
    CHECK(t.cpu.fault() == CpuFault::StackOverflow);
    CHECK_EQ(t.cpu.regs().sp, memmap::STACK_START);
    t.asm_run("POP A\nHLT");
    CHECK(t.cpu.fault() == CpuFault::StackUnderflow);
    t.asm_run("RET");
    CHECK(t.cpu.fault() == CpuFault::StackUnderflow);
}

TEST(cpu_fault_exec_from_io) {
    Rig t;
    t.asm_run("JMP $FF00");
    CHECK(t.cpu.fault() == CpuFault::ExecFromIo);
}

TEST(cpu_describe) {
    Rig t;
    t.asm_run("LD A, $AB\nHLT");
    const std::string d = t.cpu.describe();
    CHECK(d.find("A=AB") != std::string::npos);
    CHECK(d.find("HLT") != std::string::npos);
}
