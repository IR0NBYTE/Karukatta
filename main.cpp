#include <iostream>
#include <fstream>
#include <optional>
#include <vector>
#include <sstream>
#include <cstring>
#include <random>
#include <sys/stat.h>

#include "./pkg/ir_builder.hpp"
#include "./pkg/target/x86_64.hpp"
#include "./pkg/target/arm64.hpp"
#include "./pkg/emit/elf.hpp"
#include "./pkg/emit/macho.hpp"
#include "./pkg/passes/pass.hpp"
#include "./pkg/passes/cff.hpp"
#include "./pkg/passes/insn_sub.hpp"
#include "./pkg/passes/dead_insert.hpp"

/*
    Karukatta Compiler v2.0
    @ir0nbyte

    Compilation pipeline:
      Source (.kar) → Lexer → Parser → AST → IR → x86-64/ARM64 → ELF/Mach-O

    No NASM. No external assembler. No linker.
    Direct machine code emission.
*/

struct CompilerOptions {
    std::string input_file;
    std::string output_file;
    int obf_level = 0;       // 0=none, 1=basic, 2=medium, 3=full
    std::string target = "auto";
    uint64_t seed = 0;
    bool random_seed = false;
    bool dump_ir = false;
    bool dump_asm = false;
};

void usage() {
    std::cout << "Karukatta Compiler v2.0\n\n"
              << "Usage: karukatta <file.kar> -o <output> [options]\n\n"
              << "Options:\n"
              << "  -o <name>          Output binary name\n"
              << "  --obf=<0-3>        Obfuscation level (default: 0)\n"
              << "  --target=<target>  Target triple (default: auto)\n"
              << "                     x86_64-linux, arm64-linux, arm64-macos\n"
              << "  --dump-ir          Print IR to stderr\n"
              << "  --dump-asm         Print disassembly-like output to stderr\n"
              << "\n";
}

CompilerOptions parse_args(int argc, const char* argv[]) {
    CompilerOptions opts;

    if (argc < 4) {
        usage();
        exit(EXIT_FAILURE);
    }

    opts.input_file = argv[1];

    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-o" && i + 1 < argc) {
            opts.output_file = argv[++i];
        } else if (arg.rfind("--obf=", 0) == 0) {
            opts.obf_level = std::stoi(arg.substr(6));
        } else if (arg.rfind("--target=", 0) == 0) {
            opts.target = arg.substr(9);
        } else if (arg.rfind("--seed=", 0) == 0) {
            std::string val = arg.substr(7);
            if (val == "random") {
                opts.random_seed = true;
            } else {
                opts.seed = std::stoull(val);
            }
        } else if (arg == "--dump-ir") {
            opts.dump_ir = true;
        } else if (arg == "--dump-asm") {
            opts.dump_asm = true;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            usage();
            exit(EXIT_FAILURE);
        }
    }

    if (opts.output_file.empty()) {
        std::cerr << "Error: -o <output> is required\n";
        usage();
        exit(EXIT_FAILURE);
    }

    return opts;
}

std::string detect_target() {
#if defined(__x86_64__) || defined(_M_X64)
    #if defined(__linux__)
        return "x86_64-linux";
    #elif defined(__APPLE__)
        return "x86_64-macos";
    #else
        return "x86_64-linux";
    #endif
#elif defined(__aarch64__) || defined(_M_ARM64)
    #if defined(__linux__)
        return "arm64-linux";
    #elif defined(__APPLE__)
        return "arm64-macos";
    #else
        return "arm64-linux";
    #endif
#else
    return "x86_64-linux";
#endif
}

int main(int argc, const char* argv[]) {
    CompilerOptions opts = parse_args(argc, argv);

    std::string target = opts.target;
    if (target == "auto") {
        target = detect_target();
    }

    // ─── Read source file ───
    std::string src_code;
    {
        std::ifstream input(opts.input_file);
        if (!input.is_open()) {
            std::cerr << "Error: Cannot open file '" << opts.input_file << "'\n";
            return EXIT_FAILURE;
        }
        std::stringstream ss;
        ss << input.rdbuf();
        src_code = ss.str();
    }

    // ─── Lex ───
    Lexer lexer(std::move(src_code), opts.input_file);
    std::vector<Token> tokens = lexer.lexerize();

    // ─── Parse ───
    Parser parser(std::move(tokens));
    std::optional<NodeProg> prog = parser.parse_prog();
    if (!prog.has_value()) {
        std::cerr << "Error: Failed to parse program\n";
        return EXIT_FAILURE;
    }

    // ─── Lower to IR ───
    TargetOS target_os = TargetOS::LINUX;
    if (target.find("macos") != std::string::npos) {
        target_os = TargetOS::MACOS;
    }
    IRBuilder ir_builder(prog.value(), target_os);
    IRModule ir = ir_builder.build();

    // ─── Obfuscation seed ───
    if (opts.random_seed) {
        std::random_device rd;
        opts.seed = rd();
    }
    g_obf_rng.set_seed(opts.seed);

    // ─── Run obfuscation passes ───
    if (opts.obf_level >= 1) {
        InstructionSubstitutionPass().run(ir);
    }
    if (opts.obf_level >= 2) {
        ControlFlowFlatteningPass().run(ir);
        DeadCodeInsertionPass().run(ir);
    }

    if (opts.dump_ir) {
        std::cerr << "=== IR (" << ir.instructions.size() << " instructions) ===\n"
                  << ir.dump() << "=== END IR ===\n";
    }

    // ─── Code generation ───
    std::vector<uint8_t> machine_code;

    if (target == "x86_64-linux" || target == "x86_64-macos") {
        x86::X86_64Backend backend;
        machine_code = backend.compile(ir);
    }
    else if (target == "arm64-linux" || target == "arm64-macos") {
        arm64::ARM64Backend backend;
        backend.syscall_abi = (target == "arm64-macos")
            ? arm64::SyscallABI::MACOS
            : arm64::SyscallABI::LINUX;
        machine_code = backend.compile(ir);
    }
    else {
        std::cerr << "Error: Unknown target '" << target << "'\n";
        return EXIT_FAILURE;
    }

    // ─── Emit binary ───
    if (target == "x86_64-linux" || target == "arm64-linux") {
        elf::ELFEmitter emitter;
        uint16_t machine = (target == "x86_64-linux") ? elf::EM_X86_64 : elf::EM_AARCH64;
        if (!emitter.emit(opts.output_file, machine_code, machine)) {
            std::cerr << "Error: Failed to write ELF binary\n";
            return EXIT_FAILURE;
        }
    }
    else if (target == "x86_64-macos" || target == "arm64-macos") {
        // On macOS, we write a minimal .s wrapper and use the system linker.
        // The machine code is emitted as .byte directives.
        // This is what Go and Zig do on macOS — you can't avoid libSystem.
        std::string asm_file = opts.output_file + ".s";
        std::string obj_file = opts.output_file + ".o";
        {
            std::ofstream out(asm_file);
            out << ".global _main\n.align 4\n_main:\n";
            for (size_t i = 0; i < machine_code.size(); i += 4) {
                out << "    .byte ";
                for (size_t j = i; j < i + 4 && j < machine_code.size(); j++) {
                    if (j > i) out << ", ";
                    out << "0x" << std::hex << (int)machine_code[j];
                }
                out << "\n";
            }
            out.close();
        }
        std::string arch = (target == "arm64-macos") ? "arm64" : "x86_64";
        std::string sdk;
        {
            FILE* f = popen("xcrun --show-sdk-path 2>/dev/null", "r");
            char buf[512];
            if (f && fgets(buf, sizeof(buf), f)) {
                sdk = buf;
                if (!sdk.empty() && sdk.back() == '\n') sdk.pop_back();
            }
            if (f) pclose(f);
        }
        std::string cmd = "as -o " + obj_file + " " + asm_file + " 2>&1";
        if (system(cmd.c_str()) != 0) {
            std::cerr << "Error: Assembly failed\n";
            return EXIT_FAILURE;
        }
        cmd = "ld -o " + opts.output_file + " -lSystem -syslibroot " + sdk
            + " -e _main -arch " + arch + " " + obj_file + " 2>&1";
        if (system(cmd.c_str()) != 0) {
            std::cerr << "Error: Linking failed\n";
            return EXIT_FAILURE;
        }
        // Clean up intermediate files
        remove(asm_file.c_str());
        remove(obj_file.c_str());
    }

    // Make executable
    #if !defined(_WIN32)
    chmod(opts.output_file.c_str(), 0755);
    #endif

    std::cerr << "Compiled: " << opts.input_file << " → " << opts.output_file
              << " [" << target << ", " << machine_code.size() << " bytes]\n";

    return 0;
}
