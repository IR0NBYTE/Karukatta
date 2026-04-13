#pragma once

#include "pass.hpp"

// Sprinkles fake computations into the IR that run but don't affect anything.
// Uses fresh virtual registers that nobody ever reads. Just noise.

class DeadCodeInsertionPass : public Pass {
public:
    std::string name() const override { return "dead-code-insertion"; }

    void run(IRModule& module) override {
        std::vector<IRInst> new_insts;
        new_insts.reserve(module.instructions.size() * 2);

        for (const auto& inst : module.instructions) {
            // Randomly insert dead code before some instructions
            if (should_insert(inst)) {
                insert_dead_computation(new_insts, module);
            }

            new_insts.push_back(inst);

            // Sometimes insert after too
            if (should_insert(inst) && g_obf_rng.rand_bool(0.3)) {
                insert_dead_computation(new_insts, module);
            }
        }

        module.instructions = std::move(new_insts);
    }

private:
    bool should_insert(const IRInst& inst) {
        // Don't insert around labels, branches, or syscalls
        if (inst.op == IROp::LABEL || inst.op == IROp::BRANCH ||
            inst.op == IROp::COND_BRANCH || inst.op == IROp::SYSCALL ||
            inst.op == IROp::RET || inst.op == IROp::NOP) {
            return false;
        }
        // ~30% chance of insertion
        return g_obf_rng.rand_bool(0.3);
    }

    void insert_dead_computation(std::vector<IRInst>& out, IRModule& mod) {
        int choice = g_obf_rng.rand_choice(4);

        // All dead code uses fresh virtual registers that are never read
        int dead1 = mod.new_vreg();
        int dead2 = mod.new_vreg();
        int dead3 = mod.new_vreg();

        switch (choice) {
            case 0: {
                // Dead arithmetic chain: a = rand; b = rand; c = a + b
                out.push_back(IRInst::MakeMovImm(dead1, g_obf_rng.rand_val()));
                out.push_back(IRInst::MakeMovImm(dead2, g_obf_rng.rand_val()));
                out.push_back(IRInst::MakeBinOp(IROp::ADD, dead3,
                                                 IROperand::Reg(dead1),
                                                 IROperand::Reg(dead2)));
                break;
            }
            case 1: {
                // Dead multiply: a = rand; b = rand; c = a * b
                out.push_back(IRInst::MakeMovImm(dead1, g_obf_rng.rand_int(1, 100)));
                out.push_back(IRInst::MakeMovImm(dead2, g_obf_rng.rand_int(1, 100)));
                out.push_back(IRInst::MakeBinOp(IROp::MUL, dead3,
                                                 IROperand::Reg(dead1),
                                                 IROperand::Reg(dead2)));
                break;
            }
            case 2: {
                // Dead comparison: a = rand; b = rand; c = (a < b)
                out.push_back(IRInst::MakeMovImm(dead1, g_obf_rng.rand_val()));
                out.push_back(IRInst::MakeMovImm(dead2, g_obf_rng.rand_val()));
                out.push_back(IRInst::MakeBinOp(IROp::CMP_LT, dead3,
                                                 IROperand::Reg(dead1),
                                                 IROperand::Reg(dead2)));
                break;
            }
            case 3: {
                // Single dead mov
                out.push_back(IRInst::MakeMovImm(dead1, g_obf_rng.rand_val()));
                break;
            }
        }
    }
};
