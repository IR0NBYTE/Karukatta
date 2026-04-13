#pragma once

#include "pass.hpp"
#include <vector>
#include <unordered_map>
#include <unordered_set>

/*
    Control Flow Flattening (CFF) Pass

    Transforms structured control flow into a dispatcher-based switch.
    Every basic block gets a random state ID. All transitions go through
    a central dispatcher that selects the next block based on a state variable.

    Before CFF:
        B0 → B1 → B2 → B3

    After CFF:
        entry: state = STATE_B0; goto dispatcher
        dispatcher: switch(state) { STATE_B0→B0, STATE_B1→B1, ... }
        B0: ...; state = STATE_B1; goto dispatcher
        B1: ...; state = STATE_B2; goto dispatcher
        ...

    This makes control flow analysis extremely difficult for disassemblers
    and decompilers (IDA, Ghidra, Binary Ninja).
*/

class ControlFlowFlatteningPass : public Pass {
public:
    std::string name() const override { return "control-flow-flattening"; }

    void run(IRModule& module) override {
        // Step 1: Split IR into basic blocks
        auto blocks = split_basic_blocks(module);
        if (blocks.size() < 2) return; // nothing to flatten

        // Step 2: Assign random state IDs to each block
        std::unordered_map<int, int64_t> block_state_ids;
        for (size_t i = 0; i < blocks.size(); i++) {
            block_state_ids[i] = g_obf_rng.rand_val();
        }

        // Step 3: Map old label IDs to block indices
        std::unordered_map<int, int> label_to_block;
        for (size_t i = 0; i < blocks.size(); i++) {
            if (!blocks[i].empty() && blocks[i][0].op == IROp::LABEL) {
                label_to_block[blocks[i][0].label_id] = (int)i;
            }
        }

        // Step 4: Build the flattened IR
        IRModule flat;
        flat.next_vreg = module.next_vreg;
        flat.next_label = module.next_label;

        // State variable (virtual register)
        int state_vreg = flat.new_vreg();

        // Dispatcher label
        int dispatcher_label = flat.new_label();
        // Exit label (for the end)
        int exit_label = flat.new_label();

        // Entry: set initial state and jump to dispatcher
        flat.emit(IRInst::MakeMovImm(state_vreg, block_state_ids[0]));
        flat.emit(IRInst::MakeBranch(dispatcher_label));

        // Dispatcher: compare state against each block's ID and branch
        flat.emit(IRInst::MakeLabel(dispatcher_label));

        // For each block, emit: if (state == BLOCK_STATE) goto block_label
        std::vector<int> block_labels;
        for (size_t i = 0; i < blocks.size(); i++) {
            int blk_label = flat.new_label();
            block_labels.push_back(blk_label);

            int cmp_result = flat.new_vreg();
            int state_val = flat.new_vreg();
            flat.emit(IRInst::MakeMovImm(state_val, block_state_ids[i]));

            // Compare: cmp_result = (state == block_state_id)
            flat.emit(IRInst::MakeBinOp(IROp::CMP_EQ, cmp_result,
                                         IROperand::Reg(state_vreg),
                                         IROperand::Reg(state_val)));

            // Invert: we use COND_BRANCH which branches on zero
            // So we need to branch to "skip" if NOT equal, or branch to block if equal
            int skip_label = flat.new_label();
            flat.emit(IRInst::MakeCondBranch(cmp_result, skip_label));
            // If we get here, cmp_result was non-zero (match!)
            flat.emit(IRInst::MakeBranch(blk_label));
            flat.emit(IRInst::MakeLabel(skip_label));
        }

        // If no match (shouldn't happen), jump to exit
        flat.emit(IRInst::MakeBranch(exit_label));

        // Emit each block with state transitions
        for (size_t i = 0; i < blocks.size(); i++) {
            flat.emit(IRInst::MakeLabel(block_labels[i]));

            for (const auto& inst : blocks[i]) {
                if (inst.op == IROp::LABEL) {
                    continue; // skip original labels, we have our own
                }

                if (inst.op == IROp::BRANCH) {
                    // Replace: set state to target block's state, jump to dispatcher
                    auto it = label_to_block.find(inst.label_id);
                    if (it != label_to_block.end()) {
                        int target_block = it->second;
                        // Use arithmetic to compute next state (harder to pattern-match)
                        int64_t target_state = block_state_ids[target_block];
                        int64_t xor_key = g_obf_rng.rand_val();
                        int tmp = flat.new_vreg();
                        flat.emit(IRInst::MakeMovImm(tmp, target_state ^ xor_key));
                        int key_vreg = flat.new_vreg();
                        flat.emit(IRInst::MakeMovImm(key_vreg, xor_key));
                        // state = tmp XOR key  (we simulate XOR with (a+b) - 2*(a AND b)... or just use direct for now)
                        // Simplified: just set the state directly with obfuscated value
                        flat.emit(IRInst::MakeMovImm(state_vreg, target_state));
                        flat.emit(IRInst::MakeBranch(dispatcher_label));
                    } else {
                        flat.emit(inst);
                    }
                    continue;
                }

                if (inst.op == IROp::COND_BRANCH) {
                    // Conditional: if zero, set state to target block; else set state to next block
                    auto it = label_to_block.find(inst.label_id);
                    if (it != label_to_block.end()) {
                        int target_block = it->second;
                        int next_block = (int)i + 1;
                        if (next_block >= (int)blocks.size()) next_block = (int)i;

                        int skip = flat.new_label();
                        flat.emit(IRInst::MakeCondBranch(inst.src1.reg, skip));
                        // Not zero: fall through to next block
                        flat.emit(IRInst::MakeMovImm(state_vreg, block_state_ids[next_block]));
                        flat.emit(IRInst::MakeBranch(dispatcher_label));
                        flat.emit(IRInst::MakeLabel(skip));
                        // Zero: go to target block
                        flat.emit(IRInst::MakeMovImm(state_vreg, block_state_ids[target_block]));
                        flat.emit(IRInst::MakeBranch(dispatcher_label));
                    } else {
                        flat.emit(inst);
                    }
                    continue;
                }

                // All other instructions pass through unchanged
                flat.emit(inst);
            }

            // If the block didn't end with a branch/syscall, add transition to next block
            if (!blocks[i].empty()) {
                const auto& last = blocks[i].back();
                if (last.op != IROp::BRANCH && last.op != IROp::COND_BRANCH &&
                    last.op != IROp::SYSCALL && last.op != IROp::RET) {
                    int next = (int)i + 1;
                    if (next < (int)blocks.size()) {
                        flat.emit(IRInst::MakeMovImm(state_vreg, block_state_ids[next]));
                        flat.emit(IRInst::MakeBranch(dispatcher_label));
                    }
                }
            }
        }

        flat.emit(IRInst::MakeLabel(exit_label));

        module = flat;
    }

private:
    struct BasicBlock {
        std::vector<IRInst> instructions;
        bool empty() const { return instructions.empty(); }
        size_t size() const { return instructions.size(); }
        const IRInst& operator[](size_t i) const { return instructions[i]; }
        const IRInst& back() const { return instructions.back(); }
        std::vector<IRInst>::const_iterator begin() const { return instructions.begin(); }
        std::vector<IRInst>::const_iterator end() const { return instructions.end(); }
    };

    std::vector<BasicBlock> split_basic_blocks(const IRModule& module) {
        std::vector<BasicBlock> blocks;
        BasicBlock current;

        for (const auto& inst : module.instructions) {
            if (inst.op == IROp::LABEL && !current.empty()) {
                blocks.push_back(current);
                current = BasicBlock();
            }

            current.instructions.push_back(inst);

            if (inst.op == IROp::BRANCH || inst.op == IROp::COND_BRANCH ||
                inst.op == IROp::SYSCALL || inst.op == IROp::RET) {
                blocks.push_back(current);
                current = BasicBlock();
            }
        }

        if (!current.empty()) {
            blocks.push_back(current);
        }

        return blocks;
    }
};
