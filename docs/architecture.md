# Karukatta Compiler Architecture

## Overview

Karukatta is a single-pass compiler implementing a traditional three-phase architecture: lexical analysis, syntax analysis, and code generation. The compiler translates Karukatta source code into x86-64 assembly, which is then assembled and linked to produce native executables.

## Compilation Pipeline

```
Source Code (.kar)
    |
    v
[Lexer] ---------> Token Stream
    |
    v
[Parser] --------> Abstract Syntax Tree (AST)
    |
    v
[Generator] -----> Assembly Code (.asm)
    |
    v
[NASM] ----------> Object File (.o)
    |
    v
[GNU LD] --------> Executable Binary
```

## Component Architecture

### 1. Lexical Analyzer (Lexer)

**Location**: `pkg/lexer.hpp`

**Responsibility**: Convert raw source text into a sequence of tokens.

**Implementation Details**:

The lexer performs a single left-to-right scan of the input, using a finite automaton to recognize tokens. It maintains an internal pointer (`m_index`) into the source string.

**Key Methods**:
- `peek(offset)`: Lookahead without consuming characters
- `consume()`: Consume and return the current character
- `lexerize()`: Main tokenization loop

**Token Recognition Strategy**:
```
1. Check first character:
   - Letter → scan identifier/keyword
   - Digit → scan integer literal
   - Special character → emit single-character token
   - Whitespace → skip

2. For identifiers:
   - Consume [a-zA-Z][a-zA-Z0-9]*
   - Check against keyword table
   - Emit either keyword or identifier token

3. For integers:
   - Consume [0-9]+
   - Emit integer literal token
```

**Token Types**:
```cpp
enum class TokenType {
    exit, int_lit, semi, open_paren, close_paren,
    ident, let, eq, plus, star, minus, fslash,
    open_curly, close_curly, if_
};
```

### 2. Syntax Analyzer (Parser)

**Location**: `pkg/parser.hpp`

**Responsibility**: Build an Abstract Syntax Tree from the token stream and validate syntax.

**Implementation Details**:

The parser uses a recursive descent approach with operator precedence climbing (Pratt parsing) for expressions. This technique elegantly handles operator precedence without requiring separate grammar rules for each precedence level.

**Parsing Strategy**:

**Statements** (recursive descent):
```
parse_stmt()
  ├─> parse_exit_stmt()   // exit(expr);
  ├─> parse_let_stmt()    // let id = expr;
  ├─> parse_scope()       // { stmt* }
  └─> parse_if_stmt()     // if (expr) scope
```

**Expressions** (precedence climbing):
```
parse_expr(min_prec):
  1. Parse left term
  2. While next operator >= min_prec:
     a. Consume operator
     b. Parse right expression with higher precedence
     c. Build binary expression node
  3. Return expression
```

**Precedence Table**:
```cpp
Operator    Precedence
--------    ----------
* /         1 (higher)
+ -         0 (lower)
```

**Key Methods**:
- `parse_prog()`: Entry point, parses entire program
- `parse_stmt()`: Parses a single statement
- `parse_expr(min_prec)`: Pratt parser for expressions
- `parse_term()`: Parses expression terminals
- `peek(offset)`: Lookahead into token stream
- `consume()`: Advance parser position
- `try_consume(type)`: Conditionally consume expected token

**AST Node Types**:

The AST uses C++ `std::variant` to represent sum types (union of types):

```
NodeProg
  └─> NodeStmt*
       ├─> NodeStmtExit*
       ├─> NodeStmtLet*
       ├─> NodeScope*
       └─> NodeStmtIf*

NodeExpr
  ├─> NodeTerm*
  │    ├─> NodeTermIntLit*
  │    ├─> NodeTermIdent*
  │    └─> NodeTermParen*
  └─> NodeBinExpr*
       ├─> NodeBinExprAdd*
       ├─> NodeBinExprSub*
       ├─> NodeBinExprMulti*
       └─> NodeBinExprDiv*
```

### 3. Memory Management (Arena Allocator)

**Location**: `pkg/arena.hpp`

**Responsibility**: Fast, efficient memory allocation for AST nodes.

**Design Rationale**:

AST nodes have uniform lifetime - they're all needed during code generation, then all discarded. Arena allocation provides:
- O(1) allocation (simple pointer bump)
- O(1) deallocation (single free of entire arena)
- Excellent cache locality
- No fragmentation

**Implementation**:
```cpp
class ArenaAllocator {
    byte* m_buffer;     // start of allocation region
    byte* m_offset;     // current allocation point
    size_t m_size;      // total arena size (4MB)

    template <typename T>
    T* alloc() {
        void* ptr = m_offset;
        m_offset += sizeof(T);
        return static_cast<T*>(ptr);
    }
};
```

**Memory Layout**:
```
m_buffer ────────────────────────────────── m_buffer + m_size
    │                                              │
    │◄──── allocated ────►│◄──── free ────────────►│
                          │
                      m_offset
```

### 4. Code Generator

**Location**: `pkg/gen.hpp`

**Responsibility**: Translate AST to x86-64 assembly code.

**Code Generation Strategy**:

The generator uses the **visitor pattern** to traverse the AST. Each node type has a corresponding visitor method that emits assembly code.

**Stack-Based Evaluation**:

Expressions are evaluated using a stack machine model:
```
Expression: 2 + 3
Generated code:
    mov rax, 2
    push rax        ; stack: [2]
    mov rax, 3
    push rax        ; stack: [2, 3]
    pop rbx         ; rbx = 3, stack: [2]
    pop rax         ; rax = 2, stack: []
    add rax, rbx    ; rax = 5
    push rax        ; stack: [5]
```

**Variable Management**:

Variables are tracked in a symbol table with stack locations:
```cpp
struct Var {
    string name;
    size_t stack_loc;  // offset from stack base
};
vector<Var> m_vars;
```

**Scope Management**:

When entering a scope, the current variable count is saved. When exiting:
```
1. Calculate variables declared in this scope
2. Emit: add rsp, (count * 8)  ; deallocate stack space
3. Remove variables from symbol table
```

**Control Flow**:

Conditional jumps use generated labels:
```kar
if (x) {
    exit(1);
}
exit(0);
```

Generated assembly:
```asm
    push QWORD [rsp + offset_of_x]
    pop rax
    test rax, rax
    jz label0          ; jump if zero (false)
    ; true branch
    mov rax, 60
    mov rdi, 1
    syscall
label0:
    ; continue
    mov rax, 60
    mov rdi, 0
    syscall
```

**Key Methods**:
- `gen_prog()`: Entry point, emits program preamble
- `gen_stmt()`: Statement visitor dispatcher
- `gen_expr()`: Expression visitor dispatcher
- `gen_term()`: Terminal visitor dispatcher
- `push()/pop()`: Stack operation helpers
- `begin_scope()/end_scope()`: Scope management

### 5. Assembly and Linking

**NASM (Netwide Assembler)**:
- Assembles `.asm` → `.o` object file
- Uses ELF64 format for Linux
- Command: `nasm -felf64 output.asm`

**GNU LD (Linker)**:
- Links object file → executable
- No standard library linked (direct syscalls)
- Command: `ld -o output output.o`

## Data Flow Example

For the program:
```kar
let x = 2 + 3;
exit(x);
```

**Lexer Output**:
```
[let] [ident:"x"] [eq] [int_lit:"2"] [plus] [int_lit:"3"] [semi]
[exit] [open_paren] [ident:"x"] [close_paren] [semi]
```

**Parser Output (AST)**:
```
NodeProg
  └─> NodeStmtLet
       ├─> ident: "x"
       └─> NodeBinExprAdd
            ├─> NodeTermIntLit("2")
            └─> NodeTermIntLit("3")
  └─> NodeStmtExit
       └─> NodeTermIdent("x")
```

**Generator Output (Assembly)**:
```asm
global _start
_start:
    mov rax, 2
    push rax
    mov rax, 3
    push rax
    pop rbx
    pop rax
    add rax, rbx
    push rax                  ; x is at [rsp]
    push QWORD [rsp + 0]      ; load x
    mov rax, 60
    pop rdi
    syscall
```

## Design Decisions

### Why Single Pass?

- Simpler implementation for educational purposes
- Sufficient for current language features
- Faster compilation for small programs

**Tradeoff**: Cannot reference forward declarations (not needed currently).

### Why Arena Allocation?

- AST nodes have uniform lifetime
- Eliminates need for individual `delete` calls
- Prevents memory leaks
- Improves performance (faster than malloc/free)

**Tradeoff**: Memory not reclaimed until compilation completes (acceptable for typical source files).

### Why Stack-Based Code Generation?

- Simple and predictable
- Easy to understand and debug
- Portable across different register sets

**Tradeoff**: Less efficient than register-based allocation (future optimization opportunity).

### Why Visitor Pattern?

- Separation of concerns (traversal vs. operation)
- Easy to add new operations without modifying AST
- Type-safe dispatch using `std::variant` and `std::visit`

**Tradeoff**: More verbose than direct member functions.

## Error Handling

Current error handling strategy:
- Lexer: Fatal error on unrecognized character
- Parser: Fatal error on syntax violations
- Generator: Fatal error on undeclared variables or redeclarations

All errors immediately terminate compilation with `exit(EXIT_FAILURE)`.

## Performance Characteristics

**Lexer**: O(n) where n = source file size
**Parser**: O(n) where n = number of tokens
**Generator**: O(m) where m = number of AST nodes

**Memory Usage**:
- Lexer: O(n) for token array
- Parser: O(m) for AST (4MB arena)
- Generator: O(v + s) where v = variables, s = scope depth

**Typical Performance**:
- Small programs (<100 lines): <100ms total compilation time
- Memory footprint: <10MB

## Extension Points

The architecture supports future enhancements:

1. **Type Checker**: Add pass between parser and generator
2. **Optimizer**: Add IR and optimization passes before code generation
3. **Multiple Backends**: Abstract code generator interface
4. **Error Recovery**: Implement partial parse tree construction
5. **Incremental Compilation**: Cache AST between compilations

## File Organization

```
karukatta/
├── main.cpp              # Entry point, orchestrates compilation
├── pkg/
│   ├── lexer.hpp         # Lexical analysis
│   ├── parser.hpp        # Syntax analysis
│   ├── gen.hpp           # Code generation
│   └── arena.hpp         # Memory management
├── example/
│   └── example.kar       # Sample program
└── build/
    └── karukatta         # Compiler executable
```

## Dependencies

**Build-time**:
- C++17 compiler (GCC 7+, Clang 5+)
- Standard library (no external dependencies)

**Runtime**:
- NASM (assembler)
- GNU LD (linker)
- Linux operating system (for syscalls)

## Limitations

Current architectural limitations:
- Single-threaded compilation
- No intermediate representation (IR)
- No optimization passes
- Platform-specific (x86-64 Linux only)
- No symbol export/import (no modules)

These are deliberate simplifications for educational clarity and can be extended as needed.
