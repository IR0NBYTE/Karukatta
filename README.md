# Karukatta
<div style="text-align:center;">
    <img src="logo-white.png" alt="Alt text" width="500" height="500">
</div>

Karukatta is a simple compiler written in C++ that uses NASM (Netwide Assembler) for assembly and the GNU LD (Linker) for linking. It's designed to compile Karukatta source code and produce executable files on Unix-based systems.

## Features

- Karukatta source code compilation.
- Assembly using NASM.
- Linking using the GNU LD linker.
- Generates executable files.

## Installation
Clone this repository to your local machine:
```bash
git clone https://github.com/IR0NBYTE/Karukatta.git
cd Karukatta
```
### Prerequisites

Before installing and using the compiler, ensure you have the following prerequisites installed on your system:

- NASM (Netwide Assembler)
- GNU LD (Linker)

### Usage

```bash 
cd Karukatta
./build/karukatta <srcCode.kar> -o nameOfTheoutputBin
```

### Example
``` bash
# Open the example folder
cd example
# compile the karukatta code
../build/karukatta example.kar -o code
# Run the code
./code
# Check the exit returned
echo $?
```

