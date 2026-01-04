# Development Setup

This document explains how to set up your development environment for Karukatta.

## macOS Setup

### Install NASM

```bash
brew install nasm
```

### Verify Installation

```bash
nasm -version
```

## Linux Setup

### Debian/Ubuntu

```bash
sudo apt-get update
sudo apt-get install nasm build-essential
```

### Fedora/RHEL

```bash
sudo dnf install nasm gcc-c++
```

## Building the Compiler

```bash
./runner.sh
```

The compiler will be built to `build/karukatta`.

## Running Tests

```bash
cd tests
./test_runner.sh
```

## Note

The compiler currently generates x86-64 assembly for Linux. On macOS, you can compile and run the compiler itself, but the generated programs will require Linux (or use Docker/VM for testing).

For macOS native support, we would need to:
1. Generate macOS-compatible assembly (Mach-O format)
2. Use macOS system calls instead of Linux syscalls
3. Adjust the calling convention

This is planned for future releases.
