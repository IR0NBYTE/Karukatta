#pragma once

#include "../ir.hpp"
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <cstring>
#include <cassert>
#include <algorithm>

/*
    Karukatta x86-64 Backend
    Translates IR → raw x86-64 machine code bytes.
    No NASM. No external assembler. Direct byte emission.
*/

namespace x86 {

// x86-64 register encoding (3-bit values, REX.B extends to 4-bit)
enum Reg : uint8_t {
    RAX = 0, RCX = 1, RDX = 2, RBX = 3,
    RSP = 4, RBP = 5, RSI = 6, RDI = 7,
    R8  = 8, R9  = 9, R10 = 10, R11 = 11,
    R12 = 12, R13 = 13, R14 = 14, R15 = 15,
};

// Registers available for allocation (caller-saved first, then callee-saved)
// Excludes: RSP, RBP (stack), R10, R11 (scratch for codegen)
static const Reg ALLOC_REGS[] = {
    RAX, RCX, RDX, RSI, RDI, R8, R9,
    RBX, R12, R13, R14, R15
};
static const int NUM_ALLOC_REGS = 12;

// Condition codes for Jcc/SETcc instructions
enum CondCode : uint8_t {
    CC_E  = 0x04, // equal (ZF=1)
    CC_NE = 0x05, // not equal (ZF=0)
    CC_L  = 0x0C, // less (SF!=OF)
    CC_LE = 0x0E, // less or equal (ZF=1 or SF!=OF)
    CC_G  = 0x0F, // greater (ZF=0 and SF==OF)
    CC_GE = 0x0D, // greater or equal (SF==OF)
};

class CodeBuffer {
public:
    std::vector<uint8_t> data;

    size_t pos() const { return data.size(); }

    void emit8(uint8_t b) { data.push_back(b); }

    void emit16(uint16_t v) {
        emit8(v & 0xFF);
        emit8((v >> 8) & 0xFF);
    }

    void emit32(uint32_t v) {
        emit8(v & 0xFF);
        emit8((v >> 8) & 0xFF);
        emit8((v >> 16) & 0xFF);
        emit8((v >> 24) & 0xFF);
    }

    void emit64(uint64_t v) {
        emit32(v & 0xFFFFFFFF);
        emit32((v >> 32) & 0xFFFFFFFF);
    }

    // Patch a 32-bit value at a given offset
    void patch32(size_t offset, uint32_t v) {
        data[offset]     = v & 0xFF;
        data[offset + 1] = (v >> 8) & 0xFF;
        data[offset + 2] = (v >> 16) & 0xFF;
        data[offset + 3] = (v >> 24) & 0xFF;
    }
};

class X86_64Emitter {
public:
    CodeBuffer code;

    // ─── REX prefix helpers ───
    // REX byte: 0100WRXB
    // W=1 for 64-bit operand size
    // R extends ModR/M reg field
    // X extends SIB index field
    // B extends ModR/M r/m field or SIB base

    uint8_t rex(bool w, bool r, bool x, bool b) {
        return 0x40 | (w ? 8 : 0) | (r ? 4 : 0) | (x ? 2 : 0) | (b ? 1 : 0);
    }

    void emit_rex_rr(Reg dst, Reg src) {
        code.emit8(rex(true, dst >= R8, false, src >= R8));
    }

    void emit_rex_r(Reg r) {
        code.emit8(rex(true, false, false, r >= R8));
    }

    // ModR/M byte: mod(2) | reg(3) | r/m(3)
    uint8_t modrm(uint8_t mod, Reg reg, Reg rm) {
        return (mod << 6) | ((reg & 7) << 3) | (rm & 7);
    }

    // ─── Instruction emission ───

    // MOV reg, reg (64-bit)
    void emit_mov_rr(Reg dst, Reg src) {
        emit_rex_rr(src, dst);
        code.emit8(0x89);
        code.emit8(modrm(0b11, src, dst));
    }

    // MOV reg, imm64
    void emit_mov_ri(Reg dst, int64_t imm) {
        if (imm >= 0 && imm <= 0xFFFFFFFF) {
            // MOV r32, imm32 (zero-extends to 64-bit)
            if (dst >= R8) code.emit8(rex(false, false, false, true));
            code.emit8(0xB8 + (dst & 7));
            code.emit32((uint32_t)imm);
        } else {
            // MOV r64, imm64
            emit_rex_r(dst);
            code.emit8(0xB8 + (dst & 7));
            code.emit64((uint64_t)imm);
        }
    }

    // ADD reg, reg
    void emit_add_rr(Reg dst, Reg src) {
        emit_rex_rr(src, dst);
        code.emit8(0x01);
        code.emit8(modrm(0b11, src, dst));
    }

    // SUB reg, reg
    void emit_sub_rr(Reg dst, Reg src) {
        emit_rex_rr(src, dst);
        code.emit8(0x29);
        code.emit8(modrm(0b11, src, dst));
    }

    // IMUL reg, reg (signed multiply, result in dst)
    void emit_imul_rr(Reg dst, Reg src) {
        emit_rex_rr(dst, src);
        code.emit8(0x0F);
        code.emit8(0xAF);
        code.emit8(modrm(0b11, dst, src));
    }

    // CQO (sign-extend RAX into RDX:RAX for division)
    void emit_cqo() {
        code.emit8(rex(true, false, false, false));
        code.emit8(0x99);
    }

    // IDIV reg (signed divide RDX:RAX by reg, quotient in RAX)
    void emit_idiv_r(Reg src) {
        emit_rex_r(src);
        code.emit8(0xF7);
        code.emit8(modrm(0b11, (Reg)7, src)); // /7 opcode extension
    }

    // CMP reg, reg
    void emit_cmp_rr(Reg a, Reg b) {
        emit_rex_rr(b, a);
        code.emit8(0x39);
        code.emit8(modrm(0b11, b, a));
    }

    // SETcc reg (set byte based on condition)
    void emit_setcc(CondCode cc, Reg dst) {
        if (dst >= R8) code.emit8(rex(false, false, false, true));
        code.emit8(0x0F);
        code.emit8(0x90 + cc);
        code.emit8(modrm(0b11, (Reg)0, dst));
    }

    // MOVZX reg64, reg8 (zero-extend byte to 64-bit)
    void emit_movzx_r8(Reg dst, Reg src) {
        emit_rex_rr(dst, src);
        code.emit8(0x0F);
        code.emit8(0xB6);
        code.emit8(modrm(0b11, dst, src));
    }

    // TEST reg, reg
    void emit_test_rr(Reg a, Reg b) {
        emit_rex_rr(b, a);
        code.emit8(0x85);
        code.emit8(modrm(0b11, b, a));
    }

    // JMP rel32 (returns offset of the 32-bit displacement for patching)
    size_t emit_jmp_rel32() {
        code.emit8(0xE9);
        size_t patch_pos = code.pos();
        code.emit32(0); // placeholder
        return patch_pos;
    }

    // Jcc rel32 (conditional jump, returns patch position)
    size_t emit_jcc_rel32(CondCode cc) {
        code.emit8(0x0F);
        code.emit8(0x80 + cc);
        size_t patch_pos = code.pos();
        code.emit32(0); // placeholder
        return patch_pos;
    }

    // JZ rel32 (jump if zero)
    size_t emit_jz_rel32() {
        return emit_jcc_rel32(CC_E);
    }

    // PUSH reg
    void emit_push(Reg r) {
        if (r >= R8) code.emit8(rex(false, false, false, true));
        code.emit8(0x50 + (r & 7));
    }

    // POP reg
    void emit_pop(Reg r) {
        if (r >= R8) code.emit8(rex(false, false, false, true));
        code.emit8(0x58 + (r & 7));
    }

    // SYSCALL
    void emit_syscall() {
        code.emit8(0x0F);
        code.emit8(0x05);
    }

    // XOR reg, reg (useful for zeroing)
    void emit_xor_rr(Reg dst, Reg src) {
        emit_rex_rr(src, dst);
        code.emit8(0x31);
        code.emit8(modrm(0b11, src, dst));
    }

    // RET
    void emit_ret() {
        code.emit8(0xC3);
    }

    // NOP
    void emit_nop() {
        code.emit8(0x90);
    }

    // Patch a relative jump at patch_pos to jump to current position
    void patch_jump(size_t patch_pos) {
        int32_t rel = (int32_t)(code.pos() - (patch_pos + 4));
        code.patch32(patch_pos, (uint32_t)rel);
    }
};

// ─── IR → x86-64 lowering ───

class X86_64Backend {
public:
    X86_64Emitter emitter;

    std::vector<uint8_t> compile(const IRModule& module) {
        m_vreg_count = module.next_vreg;
        m_label_positions.clear();
        m_label_patches.clear();

        // Set up stack space if we need spill slots
        int spill_count = (m_vreg_count > NUM_ALLOC_REGS) ? (m_vreg_count - NUM_ALLOC_REGS) : 0;
        m_spill_size = 0;
        if (spill_count > 0) {
            m_spill_size = ((spill_count * 8) + 15) & ~15;
            // SUB RSP, spill_size (allocate stack space)
            emitter.code.emit8(emitter.rex(true, false, false, false));
            emitter.code.emit8(0x81);
            emitter.code.emit8(emitter.modrm(0b11, (Reg)5, RSP)); // /5 = SUB
            emitter.code.emit32((uint32_t)m_spill_size);
        }

        for (size_t i = 0; i < module.instructions.size(); i++) {
            emit_instruction(module.instructions[i]);
        }

        // Second pass: patch all label references
        for (auto& [patch_pos, label_id] : m_label_patches) {
            auto it = m_label_positions.find(label_id);
            assert(it != m_label_positions.end());
            int32_t rel = (int32_t)(it->second - (patch_pos + 4));
            emitter.code.patch32(patch_pos, (uint32_t)rel);
        }

        return emitter.code.data;
    }

private:
    int m_vreg_count = 0;
    int m_stack_slots = 0;
    std::unordered_map<int, size_t> m_label_positions; // label_id → code offset
    std::vector<std::pair<size_t, int>> m_label_patches; // (patch_pos, label_id)

    // Virtual register → physical register mapping
    // Simple strategy: first 14 vregs get physical regs, rest spill to stack
    // For the MVP this is sufficient. Linear scan allocator comes in Phase 3.

    bool vreg_in_reg(int vreg) const {
        return vreg >= 0 && vreg < NUM_ALLOC_REGS;
    }

    Reg vreg_to_phys(int vreg) const {
        assert(vreg >= 0 && vreg < NUM_ALLOC_REGS);
        return ALLOC_REGS[vreg];
    }

    int m_spill_size = 0;

    // For spilled vregs, compute stack offset from RSP (positive offsets)
    int spill_offset(int vreg) const {
        return (vreg - NUM_ALLOC_REGS) * 8;
    }

    // Load a virtual register into a physical register (RAX as scratch if spilled)
    Reg load_vreg(int vreg, Reg scratch = RAX) {
        if (vreg < 0) return scratch;
        if (vreg_in_reg(vreg)) {
            return vreg_to_phys(vreg);
        }
        // Spilled: load from stack
        // MOV scratch, [RBP + offset]
        emit_load_stack(scratch, spill_offset(vreg));
        return scratch;
    }

    // Store a physical register to a virtual register's location
    void store_vreg(int vreg, Reg src) {
        if (vreg < 0) return;
        if (vreg_in_reg(vreg)) {
            Reg dst = vreg_to_phys(vreg);
            if (dst != src) {
                emitter.emit_mov_rr(dst, src);
            }
        } else {
            emit_store_stack(spill_offset(vreg), src);
        }
    }

    // MOV reg, [RSP + disp32]
    // RSP encoding requires SIB byte (RSP=4 in ModR/M r/m means SIB follows)
    void emit_load_stack(Reg dst, int32_t disp) {
        emitter.code.emit8(emitter.rex(true, dst >= R8, false, false));
        emitter.code.emit8(0x8B);
        emitter.code.emit8(emitter.modrm(0b10, dst, RSP)); // mod=10 (disp32), r/m=RSP(4) → SIB
        emitter.code.emit8(0x24); // SIB: scale=00, index=RSP(none), base=RSP
        emitter.code.emit32((uint32_t)disp);
    }

    // MOV [RSP + disp32], reg
    void emit_store_stack(int32_t disp, Reg src) {
        emitter.code.emit8(emitter.rex(true, src >= R8, false, false));
        emitter.code.emit8(0x89);
        emitter.code.emit8(emitter.modrm(0b10, src, RSP));
        emitter.code.emit8(0x24); // SIB byte for RSP base
        emitter.code.emit32((uint32_t)disp);
    }

    void emit_instruction(const IRInst& inst) {
        switch (inst.op) {
            case IROp::MOV: {
                int dst_vreg = inst.dst.reg;
                if (inst.src1.is_imm) {
                    // Use R10 as scratch to avoid clobbering any live allocatable reg
                    Reg tmp = (vreg_in_reg(dst_vreg) && dst_vreg >= 0) ? vreg_to_phys(dst_vreg) : R10;
                    emitter.emit_mov_ri(tmp, inst.src1.imm);
                    store_vreg(dst_vreg, tmp);
                } else {
                    Reg src = load_vreg(inst.src1.reg, R11);
                    store_vreg(dst_vreg, src);
                }
                break;
            }

            case IROp::ADD: case IROp::SUB: case IROp::MUL: {
                // Use R10/R11 as scratch to avoid clobbering live vregs in RAX/RCX/etc
                Reg lhs = load_vreg(inst.src1.reg, R10);
                Reg rhs = load_vreg(inst.src2.reg, R11);

                if (lhs != R10) emitter.emit_mov_rr(R10, lhs);

                if (inst.op == IROp::ADD) emitter.emit_add_rr(R10, rhs);
                else if (inst.op == IROp::SUB) emitter.emit_sub_rr(R10, rhs);
                else emitter.emit_imul_rr(R10, rhs);

                store_vreg(inst.dst.reg, R10);
                break;
            }

            case IROp::DIV: {
                // IDIV uses RAX:RDX, so we must use those. Save/restore if needed.
                Reg lhs = load_vreg(inst.src1.reg, R10);
                Reg rhs = load_vreg(inst.src2.reg, R11);

                emitter.emit_mov_rr(RAX, (lhs != R10) ? lhs : R10);
                if (rhs == RAX) {
                    emitter.emit_mov_rr(R11, rhs);
                    rhs = R11;
                }
                if (rhs == RDX) {
                    emitter.emit_mov_rr(R11, rhs);
                    rhs = R11;
                }
                emitter.emit_cqo();
                emitter.emit_idiv_r(rhs);
                store_vreg(inst.dst.reg, RAX);
                break;
            }

            case IROp::CMP_EQ: case IROp::CMP_NE:
            case IROp::CMP_LT: case IROp::CMP_LE:
            case IROp::CMP_GT: case IROp::CMP_GE: {
                // Use R10/R11 as scratch to avoid clobbering src operands
                Reg lhs = load_vreg(inst.src1.reg, R10);
                Reg rhs = load_vreg(inst.src2.reg, R11);
                // Copy to scratch if lhs is a physical reg we might clobber
                if (lhs != R10) {
                    emitter.emit_mov_rr(R10, lhs);
                }
                emitter.emit_cmp_rr(R10, rhs);

                CondCode cc;
                switch (inst.op) {
                    case IROp::CMP_EQ: cc = CC_E;  break;
                    case IROp::CMP_NE: cc = CC_NE; break;
                    case IROp::CMP_LT: cc = CC_L;  break;
                    case IROp::CMP_LE: cc = CC_LE;  break;
                    case IROp::CMP_GT: cc = CC_G;   break;
                    case IROp::CMP_GE: cc = CC_GE;  break;
                    default: cc = CC_E; break;
                }
                // Use R10 for result to avoid clobbering RAX (which might hold a live vreg)
                emitter.emit_setcc(cc, R10);
                emitter.emit_movzx_r8(R10, R10);
                store_vreg(inst.dst.reg, R10);
                break;
            }

            case IROp::LABEL: {
                m_label_positions[inst.label_id] = emitter.code.pos();
                break;
            }

            case IROp::BRANCH: {
                size_t patch = emitter.emit_jmp_rel32();
                m_label_patches.push_back({patch, inst.label_id});
                break;
            }

            case IROp::COND_BRANCH: {
                // Branch if src1 == 0. Use R10 as scratch to avoid clobbering live regs.
                Reg cond = load_vreg(inst.src1.reg, R10);
                emitter.emit_test_rr(cond, cond);
                size_t patch = emitter.emit_jz_rel32();
                m_label_patches.push_back({patch, inst.label_id});
                break;
            }

            case IROp::SYSCALL: {
                // Linux x86-64 syscall: RAX=number, RDI=arg0
                // Must handle aliasing: load both into scratch first
                Reg nr = load_vreg(inst.src1.reg, R10);
                Reg arg = load_vreg(inst.src2.reg, R11);
                // Copy to scratch if they're in physical regs that might conflict
                if (nr != R10) emitter.emit_mov_rr(R10, nr);
                if (arg != R11) emitter.emit_mov_rr(R11, arg);
                // Now safely move from scratch to target regs
                emitter.emit_mov_rr(RAX, R10);
                emitter.emit_mov_rr(RDI, R11);
                emitter.emit_syscall();
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

} // namespace x86
