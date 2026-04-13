# Contributing to Karukatta

Hey, thanks for being interested in contributing. Here's how to get started.

## Getting set up

You just need a C++17 compiler. Clone the repo and run:

```bash
./runner.sh
```

That builds the compiler to `build/karukatta`. To test your changes:

```bash
# compile a .kar program and check the exit code
./build/karukatta example/example.kar -o build/test
./build/test
echo $?  # should be 5
```

## What to work on

Check the issues tab for open tasks. Some areas that could use help:

- **Language features** — functions, strings, arrays, mutable variables
- **Optimization passes** — constant folding, dead code elimination
- **More obfuscation** — opaque predicates, string encryption, anti-disassembly tricks
- **Better register allocation** — linear scan instead of the current naive mapping
- **Testing** — more .kar test programs, automated test runner

If you want to tackle something big, open an issue first so we can discuss the approach.

## How to contribute

1. Fork the repo
2. Create a branch (`git checkout -b my-feature`)
3. Make your changes
4. Test on at least one platform (macOS or Linux)
5. Commit with a clear message explaining what you did
6. Open a PR

## Code style

- Keep it simple. This is a compiler, not enterprise Java.
- Comments should explain *why*, not *what*. If the code is clear, don't comment it.
- No massive abstractions for things that only happen once.
- New passes go in `pkg/passes/`, new backends in `pkg/target/`, binary formats in `pkg/emit/`.

## Testing

Run the test programs in `example/` after making changes:

```bash
# quick smoke test on macOS
for f in example/*.kar; do
    name=$(basename $f .kar)
    ./build/karukatta $f -o build/$name --obf=0 2>/dev/null
    build/$name 2>/dev/null
    echo "$name: $?"
done
```

For x86-64 Linux testing on a Mac:

```bash
./build/karukatta example/example.kar -o build/test --target=x86_64-linux
docker run --rm -v $(pwd)/build:/app ubuntu:22.04 /app/test
```

Test all obfuscation levels too — obf=2 is where most bugs hide.

## Project structure

```
pkg/
├── lexer.hpp          # tokenizer
├── parser.hpp         # AST builder
├── arena.hpp          # memory allocator
├── ir.hpp             # intermediate representation
├── ir_builder.hpp     # AST -> IR
├── target/            # machine code backends
├── emit/              # binary format writers
└── passes/            # IR transformation passes
```

## Questions?

Open an issue or reach out on YouTube [@ir0nbyte](https://youtube.com/@ir0nbyte).
