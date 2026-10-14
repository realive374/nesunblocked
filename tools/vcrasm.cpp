// vcrasm - command-line assembler for VCR-1 assembly.
//
//   vcrasm input.asm [-o output.bin] [--org ADDR] [--symbols]
//
// Writes a flat binary from ORG (default $0000) to the last emitted byte.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>

#include "core/assembler.h"

int main(int argc, char** argv) {
    std::string in_path, out_path;
    unsigned long org = 0;
    bool symbols = false;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "-o") && i + 1 < argc) out_path = argv[++i];
        else if (!std::strcmp(argv[i], "--org") && i + 1 < argc) org = std::strtoul(argv[++i], nullptr, 0);
        else if (!std::strcmp(argv[i], "--symbols")) symbols = true;
        else if (argv[i][0] != '-' && in_path.empty()) in_path = argv[i];
        else {
            std::fprintf(stderr, "usage: vcrasm input.asm [-o output.bin] [--org ADDR] [--symbols]\n");
            return 2;
        }
    }
    if (in_path.empty()) {
        std::fprintf(stderr, "usage: vcrasm input.asm [-o output.bin] [--org ADDR] [--symbols]\n");
        return 2;
    }
    std::ifstream in(in_path, std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "vcrasm: cannot open %s\n", in_path.c_str());
        return 1;
    }
    std::stringstream ss;
    ss << in.rdbuf();

    rt90::AsmResult r = rt90::assemble(ss.str());
    if (!r.ok()) {
        for (const rt90::AsmError& e : r.errors)
            std::fprintf(stderr, "%s:%d: error: %s\n", in_path.c_str(), e.line, e.message.c_str());
        return 1;
    }
    if (out_path.empty()) {
        out_path = in_path;
        size_t dot = out_path.find_last_of('.');
        if (dot != std::string::npos && out_path.find_first_of("/\\", dot) == std::string::npos) out_path.resize(dot);
        out_path += ".bin";
    }
    const auto image = r.slice(static_cast<uint16_t>(org & 0xFFFF));
    std::ofstream out(out_path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(image.data()), static_cast<std::streamsize>(image.size()));
    if (!out) {
        std::fprintf(stderr, "vcrasm: cannot write %s\n", out_path.c_str());
        return 1;
    }
    std::printf("vcrasm: %s -> %s (%zu bytes, $%04lX-$%04X)\n", in_path.c_str(), out_path.c_str(), image.size(),
                org, r.max_addr ? static_cast<unsigned>(r.max_addr - 1) : 0u);
    if (symbols)
        for (const auto& [name, value] : r.symbols) std::printf("  %-20s $%04X\n", name.c_str(), value);
    return 0;
}
