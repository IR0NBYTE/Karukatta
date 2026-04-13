#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <cassert>

/*
    Karukatta IR - Three-Address Code with Virtual Registers
    Architecture-independent intermediate representation.
*/

enum class IROp {
    // Data movement
    MOV,            // dst = src1 (or imm1)
    LOAD,           // dst = [src1 + imm1]
    STORE,          // [src1 + imm1] = src2

    // Arithmetic
    ADD,            // dst = src1 + src2
    SUB,            // dst = src1 - src2
    MUL,            // dst = src1 * src2
    DIV,            // dst = src1 / src2
    NEG,            // dst = -src1

    // Comparison (result is 0 or 1)
    CMP_EQ,         // dst = (src1 == src2)
    CMP_NE,         // dst = (src1 != src2)
    CMP_LT,         // dst = (src1 < src2)
    CMP_LE,         // dst = (src1 <= src2)
    CMP_GT,         // dst = (src1 > src2)
    CMP_GE,         // dst = (src1 >= src2)

    // Control flow
    LABEL,          // label definition (imm1 = label ID)
    BRANCH,         // unconditional jump to label (imm1 = label ID)
    COND_BRANCH,    // if src1 == 0, jump to label (imm1 = label ID)

    // System
    SYSCALL,        // syscall: src1 = syscall number, src2 = arg0, dst = result
    RET,            // return from function

    // Misc
    NOP,            // no operation
    PUSH,           // push src1 onto stack
    POP,            // pop into dst
};

// Operand: either a virtual register or an immediate value
struct IROperand {
    bool is_imm = false;
    int reg = -1;           // virtual register number (-1 = unused)
    int64_t imm = 0;        // immediate value

    static IROperand Reg(int r) { return {false, r, 0}; }
    static IROperand Imm(int64_t v) { return {true, -1, v}; }
    static IROperand None() { return {false, -1, 0}; }

    bool is_none() const { return !is_imm && reg == -1; }
    bool is_reg() const { return !is_imm && reg >= 0; }
};

struct IRInst {
    IROp op;
    IROperand dst;
    IROperand src1;
    IROperand src2;
    int label_id = -1;      // for LABEL, BRANCH, COND_BRANCH

    // Convenience constructors
    static IRInst MakeMov(int dst, IROperand src) {
        return {IROp::MOV, IROperand::Reg(dst), src, IROperand::None()};
    }
    static IRInst MakeMovImm(int dst, int64_t val) {
        return {IROp::MOV, IROperand::Reg(dst), IROperand::Imm(val), IROperand::None()};
    }
    static IRInst MakeBinOp(IROp op, int dst, IROperand lhs, IROperand rhs) {
        return {op, IROperand::Reg(dst), lhs, rhs};
    }
    static IRInst MakeLabel(int label_id) {
        return {IROp::LABEL, IROperand::None(), IROperand::None(), IROperand::None(), label_id};
    }
    static IRInst MakeBranch(int label_id) {
        return {IROp::BRANCH, IROperand::None(), IROperand::None(), IROperand::None(), label_id};
    }
    static IRInst MakeCondBranch(int reg, int label_id) {
        return {IROp::COND_BRANCH, IROperand::None(), IROperand::Reg(reg), IROperand::None(), label_id};
    }
    static IRInst MakeSyscall(int dst, IROperand syscall_nr, IROperand arg0) {
        return {IROp::SYSCALL, IROperand::Reg(dst), syscall_nr, arg0};
    }
    static IRInst MakeNop() {
        return {IROp::NOP, IROperand::None(), IROperand::None(), IROperand::None()};
    }
};

struct IRModule {
    std::vector<IRInst> instructions;
    int next_vreg = 0;
    int next_label = 0;

    int new_vreg() { return next_vreg++; }
    int new_label() { return next_label++; }

    void emit(IRInst inst) {
        instructions.push_back(inst);
    }

    // Debug: dump IR as text
    std::string dump() const {
        std::stringstream ss;
        for (const auto& inst : instructions) {
            ss << dump_inst(inst) << "\n";
        }
        return ss.str();
    }

    static std::string dump_operand(const IROperand& op) {
        if (op.is_none()) return "_";
        if (op.is_imm) return std::to_string(op.imm);
        return "v" + std::to_string(op.reg);
    }

    static std::string dump_inst(const IRInst& inst) {
        std::stringstream ss;
        switch (inst.op) {
            case IROp::MOV:     ss << "  MOV " << dump_operand(inst.dst) << ", " << dump_operand(inst.src1); break;
            case IROp::ADD:     ss << "  ADD " << dump_operand(inst.dst) << ", " << dump_operand(inst.src1) << ", " << dump_operand(inst.src2); break;
            case IROp::SUB:     ss << "  SUB " << dump_operand(inst.dst) << ", " << dump_operand(inst.src1) << ", " << dump_operand(inst.src2); break;
            case IROp::MUL:     ss << "  MUL " << dump_operand(inst.dst) << ", " << dump_operand(inst.src1) << ", " << dump_operand(inst.src2); break;
            case IROp::DIV:     ss << "  DIV " << dump_operand(inst.dst) << ", " << dump_operand(inst.src1) << ", " << dump_operand(inst.src2); break;
            case IROp::NEG:     ss << "  NEG " << dump_operand(inst.dst) << ", " << dump_operand(inst.src1); break;
            case IROp::CMP_EQ:  ss << "  CMP_EQ " << dump_operand(inst.dst) << ", " << dump_operand(inst.src1) << ", " << dump_operand(inst.src2); break;
            case IROp::CMP_NE:  ss << "  CMP_NE " << dump_operand(inst.dst) << ", " << dump_operand(inst.src1) << ", " << dump_operand(inst.src2); break;
            case IROp::CMP_LT:  ss << "  CMP_LT " << dump_operand(inst.dst) << ", " << dump_operand(inst.src1) << ", " << dump_operand(inst.src2); break;
            case IROp::CMP_LE:  ss << "  CMP_LE " << dump_operand(inst.dst) << ", " << dump_operand(inst.src1) << ", " << dump_operand(inst.src2); break;
            case IROp::CMP_GT:  ss << "  CMP_GT " << dump_operand(inst.dst) << ", " << dump_operand(inst.src1) << ", " << dump_operand(inst.src2); break;
            case IROp::CMP_GE:  ss << "  CMP_GE " << dump_operand(inst.dst) << ", " << dump_operand(inst.src1) << ", " << dump_operand(inst.src2); break;
            case IROp::LABEL:   ss << "L" << inst.label_id << ":"; break;
            case IROp::BRANCH:  ss << "  BR L" << inst.label_id; break;
            case IROp::COND_BRANCH: ss << "  BRZ " << dump_operand(inst.src1) << ", L" << inst.label_id; break;
            case IROp::SYSCALL: ss << "  SYSCALL " << dump_operand(inst.dst) << ", " << dump_operand(inst.src1) << ", " << dump_operand(inst.src2); break;
            case IROp::RET:     ss << "  RET"; break;
            case IROp::NOP:     ss << "  NOP"; break;
            case IROp::PUSH:    ss << "  PUSH " << dump_operand(inst.src1); break;
            case IROp::POP:     ss << "  POP " << dump_operand(inst.dst); break;
            default:            ss << "  ???"; break;
        }
        return ss.str();
    }
};
