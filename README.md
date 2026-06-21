# PyRite

A lightweight interpreted scripting language with built-in arbitrary-precision arithmetic.

## Features

- **BigNumber** — all numbers are arbitrary-precision by default
- **C-like syntax** — if/elif/else/end, while/end, fn/end
- **OOP** — classes (`ins`) and structs
- **Modules** — lazy-loading `require` with single-file and directory support
- **Lambdas** — `fn(x) -> x * 2`
- **High-order functions**, closures, try/catch, and more

## Build

```bash
make release    # optimized build
make debug      # debug build with verbose output
make clean      # remove build artifacts
```

Requires a C++11 compiler (g++ or clang++).

## Run

```bash
./PyRite script.pr        # run a file
./PyRite                  # start REPL
```

## Example

```python
fn fib(dec n) do
  if n <= 1 then return n endif
  return fib(n - 1) + fib(n - 2)
end

say(fib(20))  // 6765
```

## REPL Commands

| Command | Description |
|---------|-------------|
| `run()` | Execute buffered code |
| `halt()` | Exit |
| `help()` | List built-in functions |
| `$ code` | Execute and add to buffer |
