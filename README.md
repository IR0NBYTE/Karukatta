# Karukatta

<div align="center">
    <img src="logo-white.png" alt="Karukatta Logo" width="500" height="500">
</div>

A compiler for the Karukatta programming language, written in C++. Compiles to native x86-64 executables via NASM assembly.

## About

Karukatta is an educational compiler demonstrating classical compiler construction techniques. It implements a complete compilation pipeline from source code to executable binary, including lexical analysis, syntax analysis with operator precedence parsing, and x86-64 code generation.

The compiler is designed to be readable and serve as a learning resource for compiler implementation, while remaining capable of producing functional executables.

## Language Overview

Karukatta is an imperative language with C-like syntax supporting:

- Integer arithmetic with proper operator precedence
- Comparison operators for conditional logic
- Variable declarations and lexical scoping
- Conditional execution with if-else statements
- While loops for iteration
- Single-line comments
- Program exit with status codes

### Example Program

```kar
// Variable declarations and comparisons
let x = 10;
let y = 20;

// Conditional logic with else
if (x < y) {
    exit(1);
} else {
    exit(0);
}
```

For a complete language reference, see [Language Specification](docs/language-specification.md).

## Features

**Compiler Implementation**:
- Single-pass compilation architecture
- Pratt parser for expression precedence
- Professional error reporting with line and column tracking
- Arena-based memory allocation for AST nodes
- Stack-based code generation
- Direct x86-64 assembly output

**Language Features**:
- Arithmetic expressions: `+`, `-`, `*`, `/`
- Comparison operators: `==`, `!=`, `<`, `<=`, `>`, `>=`
- Variable bindings with `let`
- Lexically scoped blocks
- Conditional execution with `if` and `else`
- While loops
- Single-line comments with `//`
- System exit with return codes

## Prerequisites

**Build Requirements**:
- C++17 compatible compiler (GCC 7+, Clang 5+)
- Make or equivalent build system

**Runtime Requirements**:
- NASM (Netwide Assembler)
- GNU LD (Linker)
- Linux x86-64 system

### Installing Prerequisites

**Debian/Ubuntu**:
```bash
sudo apt-get update
sudo apt-get install build-essential nasm
```

**Fedora/RHEL**:
```bash
sudo dnf install gcc-c++ nasm
```

**macOS**:
```bash
brew install nasm
# Xcode Command Line Tools provides g++
xcode-select --install
```

## Installation

Clone the repository:
```bash
git clone https://github.com/IR0NBYTE/Karukatta.git
cd Karukatta
```

Build the compiler:
```bash
./runner.sh
```

The compiled binary will be located at `build/karukatta`.

## Usage

### Compiling a Program

```bash
./build/karukatta <source.kar> -o <output-binary>
```

The compiler generates an executable binary that can be run directly:

```bash
./build/karukatta program.kar -o program
./program
echo $?  # Check exit status
```

### Running the Example

```bash
cd example
../build/karukatta example.kar -o code
./code
echo $?  # Should output: 5
```

### Compilation Process

The compiler performs the following steps:
1. Lexical analysis (tokenization)
2. Syntax analysis (parsing to AST)
3. Code generation (x86-64 assembly)
4. Assembly (NASM produces object file)
5. Linking (LD produces executable)

Intermediate `.asm` and `.o` files are generated alongside the output binary.

## Project Structure

```
karukatta/
├── main.cpp              # Compiler entry point and orchestration
├── pkg/                  # Compiler components
│   ├── lexer.hpp         # Lexical analyzer
│   ├── parser.hpp        # Recursive descent parser
│   ├── gen.hpp           # x86-64 code generator
│   └── arena.hpp         # Arena memory allocator
├── example/              # Example programs
│   └── example.kar       # Sample Karukatta program
├── docs/                 # Documentation
│   ├── language-specification.md
│   └── architecture.md
├── build/                # Build artifacts
│   └── karukatta         # Compiler executable
└── runner.sh             # Build script
```

## Documentation

**[Language Specification](docs/language-specification.md)**: Complete grammar, semantics, and language reference.

**[Architecture Overview](docs/architecture.md)**: Detailed compiler design, implementation strategy, and data flow.

## Language Grammar

```ebnf
program     ::= statement*
statement   ::= exit_stmt | let_stmt | scope | if_stmt | while_stmt
exit_stmt   ::= "exit" "(" expression ")" ";"
let_stmt    ::= "let" identifier "=" expression ";"
scope       ::= "{" statement* "}"
if_stmt     ::= "if" "(" expression ")" scope ("else" scope)?
while_stmt  ::= "while" "(" expression ")" scope
expression  ::= term (operator term)*
operator    ::= "+" | "-" | "*" | "/" | "==" | "!=" | "<" | "<=" | ">" | ">="
term        ::= integer_literal | identifier | "(" expression ")"
comment     ::= "//" [any character except newline]* newline
```

Operator precedence (high to low):
1. Parentheses `( )`
2. Multiplication `*`, Division `/`
3. Addition `+`, Subtraction `-`
4. Comparison `==`, `!=`, `<`, `<=`, `>`, `>=`

## Architecture

The compiler follows a traditional three-phase design:

**Lexer** (`pkg/lexer.hpp`): Converts source text to token stream using finite automaton.

**Parser** (`pkg/parser.hpp`): Builds Abstract Syntax Tree using recursive descent with Pratt parsing for expressions.

**Generator** (`pkg/gen.hpp`): Traverses AST using visitor pattern to emit x86-64 assembly with stack-based evaluation.

See [Architecture Documentation](docs/architecture.md) for detailed design rationale and implementation details.

## Examples

### Variables and Arithmetic

```kar
let a = 5;
let b = 3;
let result = a * 2 + b;
exit(result);  // exits with code 13
```

### Comparisons and If-Else

```kar
let x = 10;
let y = 20;

if (x > y) {
    exit(1);
} else {
    exit(0);
}
```

### While Loops

```kar
let flag = 1;

while (flag) {
    exit(42);
}

exit(0);
```

### Comments and Complex Logic

```kar
// Calculate with comparisons
let a = 10;
let b = 20;
let isLess = a < b;  // evaluates to 1 (true)

if (isLess) {
    exit(100);
} else {
    exit(200);
}
```

## Implementation Details

**Memory Management**: Uses arena allocation for AST nodes, providing O(1) allocation and deallocation with excellent cache locality.

**Expression Parsing**: Implements Pratt parsing (precedence climbing) for elegant handling of operator precedence without grammar ambiguity.

**Code Generation**: Stack-based evaluation model generates straightforward assembly code. Variables are tracked with stack offsets.

**Scope Handling**: Lexical scoping with automatic stack cleanup when blocks exit.

## Limitations

Current implementation constraints:
- Linux x86-64 only (uses Linux syscalls)
- Single type system (64-bit integers)
- No function definitions
- No mutable variables (all variables are immutable)
- No standard library
- No arrays or strings

These are deliberate design choices for educational clarity. The architecture supports future extensions.

## Technical Notes

**Assembly Format**: NASM syntax, ELF64 object format

**Calling Convention**: Uses Linux syscalls directly (no libc)

**Exit Syscall**: syscall number 60 (`sys_exit`)

**Register Usage**: `rax` (accumulator), `rbx` (secondary), `rdi` (syscall argument), `rsp` (stack pointer)

**Stack Growth**: Downward (standard x86-64 convention)

## License

MIT License - See LICENSE file for details.
