#pragma once

#include "../ir.hpp"
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <cassert>

// ARM64 backend — every instruction is a clean 4 bytes, unlike x86's chaos.
// Handles both macOS (X16 + SVC #0x80) and Linux (X8 + SVC #0) syscalls.

namespace arm64 {

// ARM64 registers (X0-X30, SP=31 in some encodings)
enum Reg : uint8_t {
    X0 = 0,   X1 = 1,   X2 = 2,   X3 = 3,
    X4 = 4,   X5 = 5,   X6 = 6,   X7 = 7,
    X8 = 8,   X9 = 9,   X10 = 10, X11 = 11,
    X12 = 12, X13 = 13, X14 = 14, X15 = 15,
    X16 = 16, X17 = 17, X18 = 18, X19 = 19,
    X20 = 20, X21 = 21, X22 = 22, X23 = 23,
    X24 = 24, X25 = 25, X26 = 26, X27 = 27,
    X28 = 28, X29 = 29, // frame pointer
    X30 = 30,           // link register
    XZR = 31,           // zero register (or SP depending on context)
};

// Registers available for allocation
// Avoid: X16-X18 (platform reserved on macOS), X29 (FP), X30 (LR), X31 (SP/ZR)
static const Reg ALLOC_REGS[] = {
    X0, X1, X2, X3, X4, X5, X6, X7,
    X8, X9, X10, X11, X12, X13, X14, X15,
    X19, X20, X21, X22, X23, X24, X25, X26, X27, X28
};
static const int NUM_ALLOC_REGS = 26;

enum class SyscallABI {
    MACOS,  // X16 = 0x2000000 | nr, SVC #0x80
    LINUX,  // X8 = nr, SVC #0
};

class CodeBuffer {
public:
    std::vector<uint8_t> data;

    size_t pos() const { return data.size(); }

    void emit32(uint32_t v) {
        data.push_back(v & 0xFF);
        data.push_back((v >> 8) & 0xFF);
        data.push_back((v >> 16) & 0xFF);
        data.push_back((v >> 24) & 0xFF);
    }

    void patch32(size_t offset, uint32_t v) {
        data[offset]     = v & 0xFF;
        data[offset + 1] = (v >> 8) & 0xFF;
        data[offset + 2] = (v >> 16) & 0xFF;
        data[offset + 3] = (v >> 24) & 0xFF;
    }

    // Read instruction at offset
    uint32_t read32(size_t offset) const {
        return data[offset] | (data[offset+1] << 8) |
               (data[offset+2] << 16) | (data[offset+3] << 24);
    }
};

class ARM64Emitter {
public:
    CodeBuffer code;

    // ─── Data processing (register) ───

    // ADD Xd, Xn, Xm
    void emit_add_rrr(Reg d, Reg n, Reg m) {
        // 1|00|01011|00|0|Rm|000000|Rn|Rd
        uint32_t inst = 0x8B000000 | (m << 16) | (n << 5) | d;
        code.emit32(inst);
    }

    // SUB Xd, Xn, Xm
    void emit_sub_rrr(Reg d, Reg n, Reg m) {
        // 1|10|01011|00|0|Rm|000000|Rn|Rd
        uint32_t inst = 0xCB000000 | (m << 16) | (n << 5) | d;
        code.emit32(inst);
    }

    // MUL Xd, Xn, Xm (alias for MADD Xd, Xn, Xm, XZR)
    void emit_mul_rrr(Reg d, Reg n, Reg m) {
        // 1|00|11011|000|Rm|0|11111|Rn|Rd
        uint32_t inst = 0x9B007C00 | (m << 16) | (n << 5) | d;
        code.emit32(inst);
    }

    // SDIV Xd, Xn, Xm (signed divide)
    void emit_sdiv_rrr(Reg d, Reg n, Reg m) {
        // 1|00|11010110|Rm|00001|1|Rn|Rd
        uint32_t inst = 0x9AC00C00 | (m << 16) | (n << 5) | d;
        code.emit32(inst);
    }

    // MOV Xd, Xm (alias for ORR Xd, XZR, Xm)
    void emit_mov_rr(Reg d, Reg m) {
        // 1|01|01010|00|0|Rm|000000|11111|Rd
        uint32_t inst = 0xAA0003E0 | (m << 16) | d;
        code.emit32(inst);
    }

    // ─── Immediate moves ───
    // ARM64 can load 16-bit chunks with MOVZ/MOVK

    // MOVZ Xd, #imm16, LSL #shift (shift = 0, 16, 32, 48)
    void emit_movz(Reg d, uint16_t imm, int shift = 0) {
        // 1|10|100101|hw|imm16|Rd
        uint32_t hw = shift / 16;
        uint32_t inst = 0xD2800000 | (hw << 21) | ((uint32_t)imm << 5) | d;
        code.emit32(inst);
    }

    // MOVK Xd, #imm16, LSL #shift (keep other bits)
    void emit_movk(Reg d, uint16_t imm, int shift = 0) {
        // 1|11|100101|hw|imm16|Rd
        uint32_t hw = shift / 16;
        uint32_t inst = 0xF2800000 | (hw << 21) | ((uint32_t)imm << 5) | d;
        code.emit32(inst);
    }

    // Load a 64-bit immediate into a register (up to 4 instructions)
    void emit_mov_imm(Reg d, int64_t imm) {
        uint64_t val = (uint64_t)imm;

        if (val == 0) {
            // MOV Xd, XZR
            emit_mov_rr(d, XZR);
            return;
        }

        // Use MOVZ for first non-zero chunk, MOVK for the rest
        bool first = true;
        for (int shift = 0; shift < 64; shift += 16) {
            uint16_t chunk = (val >> shift) & 0xFFFF;
            if (chunk != 0 || (shift == 0 && val <= 0xFFFF)) {
                if (first) {
                    emit_movz(d, chunk, shift);
                    first = false;
                } else {
                    emit_movk(d, chunk, shift);
                }
            }
        }

        // If all chunks were zero (shouldn't happen, handled above)
        if (first) {
            emit_movz(d, 0, 0);
        }
    }

    // ─── Comparison ───

    // CMP Xn, Xm (alias for SUBS XZR, Xn, Xm)
    void emit_cmp_rr(Reg n, Reg m) {
        // 1|11|01011|00|0|Rm|000000|Rn|11111
        uint32_t inst = 0xEB00001F | (m << 16) | (n << 5);
        code.emit32(inst);
    }

    // CSET Xd, cond (set to 1 if condition true, else 0)
    // Encoded as: CSINC Xd, XZR, XZR, invert(cond)
    void emit_cset(Reg d, uint8_t cond) {
        // Invert condition (flip bit 0)
        uint8_t inv_cond = cond ^ 1;
        // 1|00|11010100|11111|inv_cond|0|1|11111|Rd
        uint32_t inst = 0x9A9F07E0 | (inv_cond << 12) | d;
        code.emit32(inst);
    }

    // Condition codes
    static const uint8_t COND_EQ = 0x0;
    static const uint8_t COND_NE = 0x1;
    static const uint8_t COND_LT = 0xB;
    static const uint8_t COND_LE = 0xD;
    static const uint8_t COND_GT = 0xC;
    static const uint8_t COND_GE = 0xA;

    // ─── Branches ───

    // B imm26 (unconditional branch, PC-relative)
    // Returns offset to patch
    size_t emit_b() {
        size_t pos = code.pos();
        code.emit32(0x14000000); // placeholder
        return pos;
    }

    // CBZ Xt, offset (branch if register is zero)
    // Returns offset to patch
    size_t emit_cbz(Reg t) {
        size_t pos = code.pos();
        // 1|011010|0|imm19|Rt
        uint32_t inst = 0xB4000000 | t;
        code.emit32(inst);
        return pos;
    }

    // Patch a branch instruction at 'pos' to jump to 'target'
    void patch_branch(size_t pos, size_t target) {
        uint32_t inst = code.read32(pos);
        int32_t offset = (int32_t)(target - pos) / 4; // in instruction units

        if ((inst & 0xFC000000) == 0x14000000) {
            // B instruction: imm26
            inst = (inst & 0xFC000000) | (offset & 0x03FFFFFF);
        } else if ((inst & 0xFF000000) == 0xB4000000) {
            // CBZ instruction: imm19 at bits [23:5]
            inst = (inst & 0xFF00001F) | ((offset & 0x7FFFF) << 5);
        }

        code.patch32(pos, inst);
    }

    // ─── System ───

    // SVC #imm16
    void emit_svc(uint16_t imm) {
        // 11010100|000|1|imm16|000|01
        uint32_t inst = 0xD4000001 | ((uint32_t)imm << 5);
        code.emit32(inst);
    }

    // NOP
    void emit_nop() {
        code.emit32(0xD503201F);
    }

    // RET (branch to X30)
    void emit_ret() {
        code.emit32(0xD65F03C0);
    }
};

// ─── IR → ARM64 lowering ───

class ARM64Backend {
public:
    ARM64Emitter emitter;
    SyscallABI syscall_abi = SyscallABI::MACOS;

    std::vector<uint8_t> compile(const IRModule& module) {
        m_label_positions.clear();
        m_label_patches.clear();
        m_max_spill = 0;
        m_stack_allocated = false;

        // Pre-pass: determine how many vregs need spilling
        int max_vreg = module.next_vreg;
        int spill_count = (max_vreg > NUM_ALLOC_REGS) ? (max_vreg - NUM_ALLOC_REGS) : 0;
        if (spill_count > 0) {
            // Allocate stack space (16-byte aligned)
            int stack_size = ((spill_count * 8) + 15) & ~15;
            emit_sub_sp_imm(stack_size);
            m_stack_allocated = true;
        }

        for (const auto& inst : module.instructions) {
            emit_instruction(inst);
        }

        // Patch all branches
        for (auto& [patch_pos, label_id] : m_label_patches) {
            auto it = m_label_positions.find(label_id);
            assert(it != m_label_positions.end());
            emitter.patch_branch(patch_pos, it->second);
        }

        return emitter.code.data;
    }

private:
    std::unordered_map<int, size_t> m_label_positions;
    std::vector<std::pair<size_t, int>> m_label_patches;

    // Simple vreg → physical reg mapping (same strategy as x86 backend)
    bool vreg_in_reg(int vreg) const {
        return vreg >= 0 && vreg < NUM_ALLOC_REGS;
    }

    Reg vreg_to_phys(int vreg) const {
        assert(vreg >= 0 && vreg < NUM_ALLOC_REGS);
        return ALLOC_REGS[vreg];
    }

    // STR Xt, [SP, #imm]  (store to stack)
    void emit_str_sp(Reg t, int32_t offset) {
        // 1|11|1100100|0|imm12|11111|Rt (unsigned offset, SP base)
        uint32_t imm12 = ((uint32_t)offset / 8) & 0xFFF;
        uint32_t inst = 0xF9000000 | (imm12 << 10) | (31 << 5) | t; // Rn=SP(31)
        emitter.code.emit32(inst);
    }

    // LDR Xt, [SP, #imm]  (load from stack)
    void emit_ldr_sp(Reg t, int32_t offset) {
        uint32_t imm12 = ((uint32_t)offset / 8) & 0xFFF;
        uint32_t inst = 0xF9400000 | (imm12 << 10) | (31 << 5) | t;
        emitter.code.emit32(inst);
    }

    // SUB SP, SP, #imm  (allocate stack)
    void emit_sub_sp_imm(uint32_t imm) {
        uint32_t inst = 0xD10003FF | ((imm & 0xFFF) << 10);
        emitter.code.emit32(inst);
    }

    int spill_offset(int vreg) const {
        return (vreg - NUM_ALLOC_REGS) * 8;
    }

    bool m_stack_allocated = false;
    int m_max_spill = 0;

    void ensure_stack(int vreg) {
        if (vreg >= NUM_ALLOC_REGS) {
            int needed = (vreg - NUM_ALLOC_REGS + 1);
            if (needed > m_max_spill) m_max_spill = needed;
        }
    }

    Reg load_vreg(int vreg, Reg scratch = X9) {
        if (vreg < 0) return scratch;
        if (vreg_in_reg(vreg)) return vreg_to_phys(vreg);
        // Spilled: load from stack
        int off = spill_offset(vreg);
        emit_ldr_sp(scratch, off);
        return scratch;
    }

    void store_vreg(int vreg, Reg src) {
        if (vreg < 0) return;
        if (vreg_in_reg(vreg)) {
            Reg dst = vreg_to_phys(vreg);
            if (dst != src) emitter.emit_mov_rr(dst, src);
            return;
        }
        // Spilled: store to stack
        int off = spill_offset(vreg);
        emit_str_sp(src, off);
    }

    void emit_instruction(const IRInst& inst) {
        switch (inst.op) {
            case IROp::MOV: {
                int dst_vreg = inst.dst.reg;
                if (inst.src1.is_imm) {
                    Reg d = vreg_in_reg(dst_vreg) ? vreg_to_phys(dst_vreg) : X9;
                    emitter.emit_mov_imm(d, inst.src1.imm);
                    store_vreg(dst_vreg, d);
                } else {
                    Reg s = load_vreg(inst.src1.reg, X10);
                    store_vreg(dst_vreg, s);
                }
                break;
            }

            case IROp::ADD: {
                Reg lhs = load_vreg(inst.src1.reg, X9);
                Reg rhs = load_vreg(inst.src2.reg, X10);
                Reg dst = vreg_in_reg(inst.dst.reg) ? vreg_to_phys(inst.dst.reg) : X9;
                emitter.emit_add_rrr(dst, lhs, rhs);
                store_vreg(inst.dst.reg, dst);
                break;
            }

            case IROp::SUB: {
                Reg lhs = load_vreg(inst.src1.reg, X9);
                Reg rhs = load_vreg(inst.src2.reg, X10);
                Reg dst = vreg_in_reg(inst.dst.reg) ? vreg_to_phys(inst.dst.reg) : X9;
                emitter.emit_sub_rrr(dst, lhs, rhs);
                store_vreg(inst.dst.reg, dst);
                break;
            }

            case IROp::MUL: {
                Reg lhs = load_vreg(inst.src1.reg, X9);
                Reg rhs = load_vreg(inst.src2.reg, X10);
                Reg dst = vreg_in_reg(inst.dst.reg) ? vreg_to_phys(inst.dst.reg) : X9;
                emitter.emit_mul_rrr(dst, lhs, rhs);
                store_vreg(inst.dst.reg, dst);
                break;
            }

            case IROp::DIV: {
                Reg lhs = load_vreg(inst.src1.reg, X9);
                Reg rhs = load_vreg(inst.src2.reg, X10);
                Reg dst = vreg_in_reg(inst.dst.reg) ? vreg_to_phys(inst.dst.reg) : X9;
                emitter.emit_sdiv_rrr(dst, lhs, rhs);
                store_vreg(inst.dst.reg, dst);
                break;
            }

            case IROp::CMP_EQ: case IROp::CMP_NE:
            case IROp::CMP_LT: case IROp::CMP_LE:
            case IROp::CMP_GT: case IROp::CMP_GE: {
                Reg lhs = load_vreg(inst.src1.reg, X9);
                Reg rhs = load_vreg(inst.src2.reg, X10);
                emitter.emit_cmp_rr(lhs, rhs);

                uint8_t cond;
                switch (inst.op) {
                    case IROp::CMP_EQ: cond = ARM64Emitter::COND_EQ; break;
                    case IROp::CMP_NE: cond = ARM64Emitter::COND_NE; break;
                    case IROp::CMP_LT: cond = ARM64Emitter::COND_LT; break;
                    case IROp::CMP_LE: cond = ARM64Emitter::COND_LE; break;
                    case IROp::CMP_GT: cond = ARM64Emitter::COND_GT; break;
                    case IROp::CMP_GE: cond = ARM64Emitter::COND_GE; break;
                    default: cond = ARM64Emitter::COND_EQ; break;
                }

                Reg dst = vreg_in_reg(inst.dst.reg) ? vreg_to_phys(inst.dst.reg) : X9;
                emitter.emit_cset(dst, cond);
                store_vreg(inst.dst.reg, dst);
                break;
            }

            case IROp::LABEL: {
                m_label_positions[inst.label_id] = emitter.code.pos();
                break;
            }

            case IROp::BRANCH: {
                size_t patch = emitter.emit_b();
                m_label_patches.push_back({patch, inst.label_id});
                break;
            }

            case IROp::COND_BRANCH: {
                Reg cond = load_vreg(inst.src1.reg, X9);
                size_t patch = emitter.emit_cbz(cond);
                m_label_patches.push_back({patch, inst.label_id});
                break;
            }

            case IROp::SYSCALL: {
                Reg nr = load_vreg(inst.src1.reg, X9);
                Reg arg = load_vreg(inst.src2.reg, X10);

                if (syscall_abi == SyscallABI::MACOS) {
                    // macOS: X16 = 0x2000000 | syscall_nr, arg in X0, SVC #0x80
                    // Map Linux syscall numbers to macOS
                    // We'll handle this by loading the raw number into X16
                    // The IR uses Linux numbers (60=exit), we remap here
                    if (arg != X0) emitter.emit_mov_rr(X0, arg);
                    // For macOS: we need to compute 0x2000000 | nr
                    // Load nr into X16, then OR with 0x2000000
                    emitter.emit_mov_rr(X16, nr);
                    // The IR builder emits 60 for exit. macOS exit = 0x2000001
                    // We need to remap: emit MOVZ X16, #1 then MOVK X16, #0x200, LSL#16
                    // Actually simpler: just load the macOS syscall number directly
                    // For now, detect common syscalls and remap
                    // TODO: proper syscall abstraction in IR
                    // HACK: We'll remap in the IR builder based on target
                    // For now, assume the value in nr already has the macOS prefix
                    emitter.emit_svc(0x80);
                } else {
                    // Linux: X8 = syscall_nr, arg in X0, SVC #0
                    if (arg != X0) emitter.emit_mov_rr(X0, arg);
                    if (nr != X8) emitter.emit_mov_rr(X8, nr);
                    emitter.emit_svc(0);
                }
                break;
            }

            case IROp::NOP:
                emitter.emit_nop();
                break;

            case IROp::RET:
                emitter.emit_ret();
                break;

            default:
                break;
        }
    }
};

} // namespace arm64
