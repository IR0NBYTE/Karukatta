#pragma once

#include "parser.hpp"
#include "ir.hpp"
#include <unordered_map>
#include <string>
#include <algorithm>

// Walks the AST and spits out IR instructions.
// Handles variable scoping and syscall number mapping per target OS.

enum class TargetOS { LINUX, MACOS };

class IRBuilder {
public:
    explicit IRBuilder(const NodeProg& prog, TargetOS os = TargetOS::LINUX)
        : m_prog(prog), m_os(os) {}

    IRModule build() {
        for (const NodeStmt* stmt : m_prog.stmts) {
            lower_stmt(stmt);
        }
        // Default exit(0) at end of program
        int zero = m_module.new_vreg();
        m_module.emit(IRInst::MakeMovImm(zero, 0));
        int syscall_nr = m_module.new_vreg();
        m_module.emit(IRInst::MakeMovImm(syscall_nr, syscall_exit()));
        m_module.emit(IRInst::MakeSyscall(-1, IROperand::Reg(syscall_nr), IROperand::Reg(zero)));
        return m_module;
    }

private:
    // Returns the virtual register holding the expression result
    int lower_expr(const NodeExpr* expr) {
        struct ExprVisitor {
            IRBuilder& b;
            int result;

            void operator()(const NodeTerm* term) {
                result = b.lower_term(term);
            }
            void operator()(const NodeBinExpr* bin_expr) {
                result = b.lower_bin_expr(bin_expr);
            }
        };
        ExprVisitor v{*this, -1};
        std::visit(v, expr->var);
        return v.result;
    }

    int lower_term(const NodeTerm* term) {
        struct TermVisitor {
            IRBuilder& b;
            int result;

            void operator()(const NodeTermIntLit* lit) {
                int dst = b.m_module.new_vreg();
                int64_t val = std::stoll(lit->int_lit.value.value());
                b.m_module.emit(IRInst::MakeMovImm(dst, val));
                result = dst;
            }
            void operator()(const NodeTermIdent* ident) {
                const std::string& name = ident->ident.value.value();
                auto it = b.find_var(name);
                if (it == b.m_vars.end()) {
                    std::cerr << "Undeclared identifier: " << name << std::endl;
                    exit(EXIT_FAILURE);
                }
                result = it->vreg;
            }
            void operator()(const NodeTermParen* paren) {
                result = b.lower_expr(paren->expr);
            }
        };
        TermVisitor v{*this, -1};
        std::visit(v, term->var);
        return v.result;
    }

    int lower_bin_expr(const NodeBinExpr* bin_expr) {
        struct BinVisitor {
            IRBuilder& b;
            int result;

            void handle(IROp op, const NodeExpr* lhs, const NodeExpr* rhs) {
                int l = b.lower_expr(lhs);
                int r = b.lower_expr(rhs);
                int dst = b.m_module.new_vreg();
                b.m_module.emit(IRInst::MakeBinOp(op, dst, IROperand::Reg(l), IROperand::Reg(r)));
                result = dst;
            }

            void operator()(const NodeBinExprAdd* e) { handle(IROp::ADD, e->lhs, e->rhs); }
            void operator()(const NodeBinExprSub* e) { handle(IROp::SUB, e->lhs, e->rhs); }
            void operator()(const NodeBinExprMulti* e) { handle(IROp::MUL, e->lhs, e->rhs); }
            void operator()(const NodeBinExprDiv* e) { handle(IROp::DIV, e->lhs, e->rhs); }
            void operator()(const NodeBinExprCmpEq* e) { handle(IROp::CMP_EQ, e->lhs, e->rhs); }
            void operator()(const NodeBinExprCmpNotEq* e) { handle(IROp::CMP_NE, e->lhs, e->rhs); }
            void operator()(const NodeBinExprCmpLess* e) { handle(IROp::CMP_LT, e->lhs, e->rhs); }
            void operator()(const NodeBinExprCmpLessEq* e) { handle(IROp::CMP_LE, e->lhs, e->rhs); }
            void operator()(const NodeBinExprCmpGreater* e) { handle(IROp::CMP_GT, e->lhs, e->rhs); }
            void operator()(const NodeBinExprCmpGreaterEq* e) { handle(IROp::CMP_GE, e->lhs, e->rhs); }
        };
        BinVisitor v{*this, -1};
        std::visit(v, bin_expr->var);
        return v.result;
    }

    void lower_stmt(const NodeStmt* stmt) {
        struct StmtVisitor {
            IRBuilder& b;

            void operator()(const NodeStmtExit* s) {
                int val = b.lower_expr(s->expr);
                int syscall_nr = b.m_module.new_vreg();
                b.m_module.emit(IRInst::MakeMovImm(syscall_nr, b.syscall_exit()));
                b.m_module.emit(IRInst::MakeSyscall(-1, IROperand::Reg(syscall_nr), IROperand::Reg(val)));
            }

            void operator()(const NodeStmtLet* s) {
                const std::string& name = s->ident.value.value();
                // Check for redeclaration in current scope
                for (auto it = b.m_vars.rbegin(); it != b.m_vars.rend(); ++it) {
                    if (it->scope_depth < b.m_scope_depth) break;
                    if (it->name == name) {
                        std::cerr << "Identifier already used: " << name << std::endl;
                        exit(EXIT_FAILURE);
                    }
                }
                int vreg = b.lower_expr(s->expr);
                b.m_vars.push_back({name, vreg, b.m_scope_depth});
            }

            void operator()(const NodeScope* scope) {
                b.lower_scope(scope);
            }

            void operator()(const NodeStmtIf* s) {
                int cond = b.lower_expr(s->expr);
                int label_else = b.m_module.new_label();
                int label_end = b.m_module.new_label();

                b.m_module.emit(IRInst::MakeCondBranch(cond, label_else));
                b.lower_scope(s->scope);

                if (s->else_scope.has_value()) {
                    b.m_module.emit(IRInst::MakeBranch(label_end));
                    b.m_module.emit(IRInst::MakeLabel(label_else));
                    b.lower_scope(s->else_scope.value());
                    b.m_module.emit(IRInst::MakeLabel(label_end));
                } else {
                    b.m_module.emit(IRInst::MakeLabel(label_else));
                }
            }

            void operator()(const NodeStmtWhile* s) {
                int label_start = b.m_module.new_label();
                int label_end = b.m_module.new_label();

                b.m_module.emit(IRInst::MakeLabel(label_start));
                int cond = b.lower_expr(s->expr);
                b.m_module.emit(IRInst::MakeCondBranch(cond, label_end));
                b.lower_scope(s->scope);
                b.m_module.emit(IRInst::MakeBranch(label_start));
                b.m_module.emit(IRInst::MakeLabel(label_end));
            }
        };
        StmtVisitor v{*this};
        std::visit(v, stmt->var);
    }

    void lower_scope(const NodeScope* scope) {
        m_scope_depth++;
        size_t var_count = m_vars.size();
        for (const NodeStmt* stmt : scope->stmts) {
            lower_stmt(stmt);
        }
        // Pop variables from this scope
        while (m_vars.size() > var_count) {
            m_vars.pop_back();
        }
        m_scope_depth--;
    }

    struct VarInfo {
        std::string name;
        int vreg;
        int scope_depth;
    };

    std::vector<VarInfo>::iterator find_var(const std::string& name) {
        // Search from back to front (most recent scope first)
        for (auto it = m_vars.end(); it != m_vars.begin();) {
            --it;
            if (it->name == name) return it;
        }
        return m_vars.end();
    }

    int64_t syscall_exit() const {
        switch (m_os) {
            case TargetOS::MACOS: return 0x2000001; // exit
            case TargetOS::LINUX: return 60;
        }
        return 60;
    }

    const NodeProg& m_prog;
    IRModule m_module;
    std::vector<VarInfo> m_vars;
    int m_scope_depth = 0;
    TargetOS m_os;
};
