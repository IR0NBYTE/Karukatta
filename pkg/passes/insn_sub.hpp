#pragma once

#include "pass.hpp"

// Instruction substitution — replaces simple ops with equivalent junk.
// ADD a, b might become SUB a, NEG(b) or a + b + noise - noise.
// Different seed = different substitutions = polymorphic output.

class InstructionSubstitutionPass : public Pass {
public:
    std::string name() const override { return "instruction-substitution"; }

    void run(IRModule& module) override {
        std::vector<IRInst> new_insts;
        new_insts.reserve(module.instructions.size() * 2);

        for (const auto& inst : module.instructions) {
            switch (inst.op) {
                case IROp::ADD:
                    substitute_add(new_insts, inst, module);
                    break;
                case IROp::SUB:
                    substitute_sub(new_insts, inst, module);
                    break;
                case IROp::MOV:
                    substitute_mov(new_insts, inst, module);
                    break;
                default:
                    new_insts.push_back(inst);
                    break;
            }
        }

        module.instructions = std::move(new_insts);
    }

private:
    void substitute_add(std::vector<IRInst>& out, const IRInst& inst, IRModule& mod) {
        int choice = g_obf_rng.rand_choice(3);

        if (choice == 0) {
            // ADD a, b → tmp = NEG(b); SUB a, tmp (i.e., a - (-b))
            int neg_tmp = mod.new_vreg();
            // neg_tmp = 0 - src2
            int zero = mod.new_vreg();
            out.push_back(IRInst::MakeMovImm(zero, 0));
            out.push_back(IRInst::MakeBinOp(IROp::SUB, neg_tmp,
                                             IROperand::Reg(zero), inst.src2));
            out.push_back(IRInst::MakeBinOp(IROp::SUB, inst.dst.reg,
                                             inst.src1, IROperand::Reg(neg_tmp)));
        } else if (choice == 1) {
            // ADD a, b → result = a + b + c - c (add random noise)
            int64_t noise = g_obf_rng.rand_int(1, 1000);
            int noise_vreg = mod.new_vreg();
            int tmp1 = mod.new_vreg();
            out.push_back(IRInst::MakeMovImm(noise_vreg, noise));
            out.push_back(IRInst::MakeBinOp(IROp::ADD, tmp1,
                                             inst.src1, inst.src2));
            out.push_back(IRInst::MakeBinOp(IROp::ADD, inst.dst.reg,
                                             IROperand::Reg(tmp1),
                                             IROperand::Reg(noise_vreg)));
            out.push_back(IRInst::MakeBinOp(IROp::SUB, inst.dst.reg,
                                             IROperand::Reg(inst.dst.reg),
                                             IROperand::Reg(noise_vreg)));
        } else {
            // No substitution
            out.push_back(inst);
        }
    }

    void substitute_sub(std::vector<IRInst>& out, const IRInst& inst, IRModule& mod) {
        int choice = g_obf_rng.rand_choice(3);

        if (choice == 0) {
            // SUB a, b → tmp = NEG(b); ADD a, tmp
            int neg_tmp = mod.new_vreg();
            int zero = mod.new_vreg();
            out.push_back(IRInst::MakeMovImm(zero, 0));
            out.push_back(IRInst::MakeBinOp(IROp::SUB, neg_tmp,
                                             IROperand::Reg(zero), inst.src2));
            out.push_back(IRInst::MakeBinOp(IROp::ADD, inst.dst.reg,
                                             inst.src1, IROperand::Reg(neg_tmp)));
        } else if (choice == 1) {
            // SUB a, b → result = a - b + c - c
            int64_t noise = g_obf_rng.rand_int(1, 1000);
            int noise_vreg = mod.new_vreg();
            int tmp1 = mod.new_vreg();
            out.push_back(IRInst::MakeMovImm(noise_vreg, noise));
            out.push_back(IRInst::MakeBinOp(IROp::SUB, tmp1,
                                             inst.src1, inst.src2));
            out.push_back(IRInst::MakeBinOp(IROp::ADD, inst.dst.reg,
                                             IROperand::Reg(tmp1),
                                             IROperand::Reg(noise_vreg)));
            out.push_back(IRInst::MakeBinOp(IROp::SUB, inst.dst.reg,
                                             IROperand::Reg(inst.dst.reg),
                                             IROperand::Reg(noise_vreg)));
        } else {
            out.push_back(inst);
        }
    }

    void substitute_mov(std::vector<IRInst>& out, const IRInst& inst, IRModule& mod) {
        // Only substitute MOV reg, 0 (zeroing)
        if (inst.src1.is_imm && inst.src1.imm == 0 && g_obf_rng.rand_bool(0.5)) {
            // MOV a, 0 → tmp = rand; SUB a, tmp, tmp
            int64_t val = g_obf_rng.rand_int(1, 999999);
            int tmp = mod.new_vreg();
            out.push_back(IRInst::MakeMovImm(tmp, val));
            out.push_back(IRInst::MakeBinOp(IROp::SUB, inst.dst.reg,
                                             IROperand::Reg(tmp),
                                             IROperand::Reg(tmp)));
        } else {
            out.push_back(inst);
        }
    }
};
