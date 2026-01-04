# Karukatta Language Specification

## Overview

Karukatta is a statically-structured, imperative programming language designed for educational purposes and systems programming. The language compiles to x86-64 assembly and produces native executables for Unix-based systems.

## Lexical Structure

### Keywords

The language reserves the following keywords:

```
exit    let    if
```

### Operators

Arithmetic operators (in order of precedence):
- Multiplicative: `*` `/`
- Additive: `+` `-`

Assignment operator: `=`

### Delimiters

```
(  )    Parentheses (grouping, function calls)
{  }    Braces (scope blocks)
;       Semicolon (statement terminator)
```

### Literals

**Integer Literals**: Sequence of decimal digits (0-9)
```
0  42  1234
```

### Identifiers

Identifiers must start with a letter (a-z, A-Z) and may contain letters and digits.

```
x  counter  value123
```

### Whitespace

Spaces, tabs, and newlines are ignored except as token separators.

## Grammar

The language grammar in Extended Backus-Naur Form (EBNF):

```ebnf
program     ::= statement*

statement   ::= exit_stmt
              | let_stmt
              | scope
              | if_stmt

exit_stmt   ::= "exit" "(" expression ")" ";"

let_stmt    ::= "let" identifier "=" expression ";"

scope       ::= "{" statement* "}"

if_stmt     ::= "if" "(" expression ")" scope

expression  ::= term (("+"|"-"|"*"|"/") term)*

term        ::= integer_literal
              | identifier
              | "(" expression ")"
```

## Semantics

### Program Structure

A Karukatta program consists of zero or more statements executed in sequence. Execution begins at the first statement and proceeds sequentially unless altered by control flow.

### Variable Declaration

Variables are declared using the `let` keyword and must be initialized:

```kar
let x = 42;
let result = 10 + 5;
```

Variables are immutable after declaration. Attempting to redeclare a variable in the same scope results in a compilation error.

### Scoping

Variables are lexically scoped. A scope is created by:
- The program body (global scope)
- Brace-delimited blocks `{ }`
- Conditional blocks

Variables declared in an inner scope shadow outer scope variables of the same name. Variables are destroyed when their scope ends.

```kar
let x = 1;
{
    let x = 2;    // shadows outer x
    let y = 3;
}
// y no longer exists here
```

### Expressions

Expressions evaluate to integer values. Operators follow standard mathematical precedence:
- Multiplication and division have higher precedence than addition and subtraction
- Operators of equal precedence are left-associative
- Parentheses override default precedence

```kar
let a = 2 + 3 * 4;      // evaluates to 14
let b = (2 + 3) * 4;    // evaluates to 20
```

### Control Flow

**Conditional Execution**

The `if` statement executes a scope only if the condition expression evaluates to a non-zero value:

```kar
if (x) {
    exit(1);
}
```

Zero is considered false; all other values are true.

### Program Termination

The `exit` statement terminates the program with the given exit code:

```kar
exit(0);    // successful termination
exit(1);    // error termination
```

Exit codes are passed to the operating system and follow Unix conventions (0 for success, non-zero for errors).

If program execution reaches the end without an explicit `exit()`, the program exits with code 0.

## Type System

Currently, Karukatta is unityped - all values are 64-bit signed integers. Type checking ensures:
- Variables are declared before use
- Variables are not redeclared in the same scope
- Expressions contain valid operators and operands

## Memory Model

- Variables are allocated on the runtime stack
- Variable lifetime is bound to their lexical scope
- Automatic deallocation occurs when scope ends
- Stack grows downward in memory (x86-64 convention)

## Implementation Notes

### Target Platform

- Architecture: x86-64 (AMD64)
- Operating System: Linux
- Assembly: NASM syntax
- Linking: GNU LD

### Calling Convention

Currently, the language does not support functions. The runtime uses Linux system calls:
- System call 60 (`sys_exit`) for program termination

### Integer Representation

Integers are 64-bit signed values (two's complement representation) stored in registers or on the stack.

## Examples

### Basic Arithmetic

```kar
let x = 10;
let y = 20;
let sum = x + y;
exit(sum);
```

### Conditional Execution

```kar
let condition = 1;
if (condition) {
    exit(42);
}
exit(0);
```

### Complex Expressions

```kar
let a = 2;
let b = 3;
let c = 4;
let result = a * b + c;    // result = 10
exit(result);
```

### Nested Scopes

```kar
let x = 5;
{
    let y = 10;
    {
        let z = x + y;
        exit(z);    // exits with 15
    }
}
```
