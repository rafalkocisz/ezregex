# ezregex

[![CI](https://github.com/rafalkocisz/ezregex/actions/workflows/ci.yml/badge.svg)](https://github.com/rafalkocisz/ezregex/actions/workflows/ci.yml)

A minimal, dependency-free C++ regex matching library. Single header + source pair,
Orthodox C++ style, ASCII-only, no Unicode.

Part of the **ez** library family — shared design principles and conventions in [EZ.md](ez/EZ.md).

## Features

- Substring search by default; `^` / `$` anchors for full-string matching
- Greedy quantifiers: `?`, `*`, `+`, `{n}`, `{n,}`, `{n,m}`
- Capturing groups `(...)` — up to 16, non-nested
- Character classes `[abc]`, ranges `[a-z]`, negation `[^abc]`
- Shorthand classes: `\d`, `\w`, `\s` and their uppercase negations
- Literal escapes for all metacharacters: `\.`, `\(`, `\\`, etc.
- Specific error codes — distinguish bad escape, unclosed bracket, unmatched paren, etc.
- No heap allocation in the matching core; no exceptions; no RTTI

**Not supported:** alternation (`|`), nested groups, Unicode, lookahead/lookbehind.

**Performance caveat:** ezregex uses a backtracking recursive-descent engine. Like all
backtracking engines (including `std::regex`), it can exhibit exponential-time behaviour
on certain pattern/input combinations. A classic trigger is a pattern of repeated `??`
pairs matched against a string of `?` characters — e.g. `?` repeated 47 times followed
by a literal absent from the subject. The library is intended for use with **trusted,
well-formed patterns**; do not expose it to patterns supplied by untrusted users without
additional validation or complexity limits at the call site.

## Quick start

```cpp
#include "ez_regex.h"
#include <vector>
#include <string_view>
#include <cstdio>

int main()
{
    // Simple match — no captures needed, omit the vector entirely
    if (ez::regex_match("\\d+", "price: 42 USD") == EZ_REGEX_MATCH)
        printf("found a number\n");

    // Full-string match with anchors
    if (ez::regex_match("^\\d{4}-\\d{2}-\\d{2}$", "2024-01-15") == EZ_REGEX_MATCH)
        printf("valid ISO date\n");

    // Capturing groups — pass a vector to receive the captures
    std::vector<std::string_view> caps;
    if (ez::regex_match("^(\\w+)=(\\w+)$", "timeout=30", &caps) == EZ_REGEX_MATCH) {
        printf("key:   %.*s\n", (int)caps[0].size(), caps[0].data());
        printf("value: %.*s\n", (int)caps[1].size(), caps[1].data());
    }

    // Error handling
    int r = ez::regex_match("(bad[pattern", "test");
    if (r < 0)
        printf("invalid pattern: error code %d\n", r);
}
```

## API

```cpp
namespace ez {

int regex_match(const char* regex,
                const char* str,
                std::vector<std::string_view>* captures = nullptr);

} // namespace ez
```

| Parameter  | Description |
|------------|-------------|
| `regex`    | Null-terminated regex pattern |
| `str`      | Null-terminated string to search |
| `captures` | If non-null, filled with one `string_view` per capture group on a successful match and **always cleared** on entry; pass `nullptr` (or omit) when capture results are not needed — groups still participate in matching |

### Return values

| Code                     | Value | Meaning |
|--------------------------|------:|---------|
| `EZ_REGEX_MATCH`         |  `0`  | Pattern matched |
| `EZ_REGEX_NO_MATCH`      |  `1`  | Pattern did not match |
| `EZ_REGEX_ERR_SYNTAX`    | `-1`  | Generic syntax error (reserved catch-all) |
| `EZ_REGEX_ERR_DEPTH`     | `-2`  | Too many capture groups (> `EZ_REGEX_MAX_CAPTURES`) |
| `EZ_REGEX_ERR_ESCAPE`    | `-3`  | Trailing `\`, or unknown escape sequence |
| `EZ_REGEX_ERR_BRACKET`   | `-4`  | Unclosed `[`, or `\` at end of bracket content |
| `EZ_REGEX_ERR_PAREN`     | `-5`  | Unmatched `(` or `)` |
| `EZ_REGEX_ERR_NESTING`   | `-6`  | Nested capture groups (not supported) |

Any negative return value means an invalid pattern; test with `result < 0` to catch all errors.

The default capture limit is 16. Override it before including the header:

```cpp
#define EZ_REGEX_MAX_CAPTURES 32
#include "ez_regex.h"
```

## Supported syntax

| Syntax        | Meaning |
|---------------|---------|
| `.`           | Any character |
| `?`           | Zero or one (greedy) |
| `*`           | Zero or more (greedy) |
| `+`           | One or more (greedy) |
| `{n}`         | Exactly n repetitions |
| `{n,}`        | At least n repetitions |
| `{n,m}`       | Between n and m repetitions |
| `(...)`       | Capturing group (no nesting) |
| `[abc]`       | Character class |
| `[a-z]`       | Character range |
| `[^abc]`      | Negated class |
| `\d` / `\D`   | Digit / non-digit |
| `\w` / `\W`   | Word character (`[A-Za-z0-9_]`) / non-word |
| `\s` / `\S`   | Whitespace / non-whitespace |
| `\t` / `\n` / `\r` | Tab / newline / carriage-return |
| `\b` / `\B`   | Word boundary / non-word boundary (zero-width) |
| `^`           | Anchor — start of string |
| `$`           | Anchor — end of string |
| `\.` `\(` `\)` `\[` `\]` `\*` `\+` `\?` `\{` `\}` `\\` | Literal metacharacter |

## Build

**Requirements:** CMake ≥ 3.20, a C++17 compiler.

### Linux / macOS

```sh
cmake -B build
cmake --build build
```

### Windows (Visual Studio)

```bat
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Debug
```

To build without tests:

```sh
cmake -B build -DEZREGEX_BUILD_TESTS=OFF
cmake --build build
```

## Sanitizers

Running the test suite under sanitizers is the recommended way to check for memory errors
and undefined behaviour.

| Sanitizer | MSVC (VS 2022) | GCC / Clang |
|---|---|---|
| ASan (Address Sanitizer) | `/fsanitize=address` | `-fsanitize=address` |
| UBSan (Undefined Behaviour) | not supported | `-fsanitize=undefined` |

### Build

Use a **separate build directory** to keep the sanitized build isolated from the normal one.

```sh
# Linux / macOS — ASan + UBSan
cmake -B build-san -DEZREGEX_SANITIZE=ON
cmake --build build-san

# Windows (ASan only — MSVC does not support UBSan)
cmake -B build-san -G "Visual Studio 17 2022" -DEZREGEX_SANITIZE=ON
cmake --build build-san --config RelWithDebInfo
```

`RelWithDebInfo` is recommended on MSVC: it gives optimised code with debug symbols, which
produces the most actionable ASan stack traces.

### Run

```sh
# Linux / macOS
./build-san/tests/test_ez_regex

# Windows — the ASan runtime DLL must be on PATH before running.
# Adjust the MSVC version number to match your installation.
$asanDir = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\<version>\bin\Hostx64\x64"
$env:PATH = "$asanDir;$env:PATH"
.\build-san\tests\RelWithDebInfo\test_ez_regex.exe
```

> **MSVC notes:**
> - The `/RTC1` runtime-check flag (added by CMake to Debug builds) is incompatible with
>   ASan; the CMakeLists.txt strips it automatically when `EZREGEX_SANITIZE=ON`.
> - The `/INCREMENTAL` linker option is likewise removed automatically.
> - Use `RelWithDebInfo` rather than `Debug` for the sanitized build.

### For UBSan on Windows

The easiest option is WSL2 with GCC or Clang:

```sh
# Inside WSL2 (Ubuntu)
cmake -B build-san -DEZREGEX_SANITIZE=ON -DCMAKE_CXX_COMPILER=clang++
cmake --build build-san
./build-san/tests/test_ez_regex
```

### What the sanitizers check

UBSan would previously have fired on `parse_braces` when a pattern contained a brace
quantifier with a number exceeding `INT_MAX` (e.g. `\d{9999999999}`), causing signed
integer overflow during digit accumulation. This has been fixed with an overflow guard;
the affected pattern is now treated as literal characters, consistent with other
malformed-brace handling.

## Fuzzing

Coverage-guided fuzzing with [libFuzzer](https://llvm.org/docs/LibFuzzer.html) finds crashes,
assertion failures, and sanitizer violations that unit tests miss — including unexpected
interactions between features and deeply malformed inputs.

**Requirements:** Clang, ASan, UBSan. Supported on Linux and WSL2; not recommended on
Windows-native toolchains.

### Build

Use a separate build directory. `EZREGEX_BUILD_FUZZ=ON` implies ASan + UBSan automatically.

```sh
# Inside WSL2 (Ubuntu) or native Linux
cmake -B build-fuzz -DEZREGEX_BUILD_FUZZ=ON -DCMAKE_CXX_COMPILER=clang++
cmake --build build-fuzz
```

### Run

```sh
# Run for 500 000 iterations using the provided seed corpus.
./build-fuzz/fuzz/fuzz_ez_regex fuzz/corpus/ -runs=500000

# Run indefinitely, saving new interesting inputs to fuzz/corpus/.
./build-fuzz/fuzz/fuzz_ez_regex fuzz/corpus/
```

libFuzzer prints a summary line for each new coverage-increasing input it finds and
exits immediately (with a non-zero code) if an assertion fails or a sanitizer fires.
The offending input is saved to `crash-<hash>` in the current directory.

### Reproducing a crash

```sh
# Feed a saved crash input directly to the fuzzer binary.
./build-fuzz/fuzz/fuzz_ez_regex crash-<hash>
```

### Input format

Each fuzzer input is split on the first `\x00` byte:

```
<pattern bytes> \x00 <subject bytes>
```

If no null byte is present, the whole input is treated as the pattern with an empty
subject, exercising `validate_pattern` on arbitrary byte sequences.

### Seed corpus

`fuzz/corpus/` contains 10 seed inputs derived from the test suite, covering the main
feature areas (anchors, captures, character classes, quantifiers, shorthands).
libFuzzer uses these as starting points for mutation.

### What fuzzing is likely to find

- Edge cases in `match_class` — complex first-char and range rules
- Capture pointer arithmetic errors caught by the bounds assertion in the fuzz target

### Results

A 500 000-iteration run (WSL2, Clang 14, ASan + UBSan) produced **no crashes and no
sanitizer violations**. One slow unit was flagged:

| Finding | Input | Time |
|---------|-------|------|
| Catastrophic backtracking | pattern: `?` × 47 + `_`; subject: `?` × 25 | 46 s |

The pattern `??` is parsed as "match literal `?` zero or one time". With 23 such pairs
and a subject consisting entirely of `?` characters, the backtracking engine explores
~2²³ combinations before determining there is no match. This is a known property of
backtracking engines and is documented in the Performance caveat above; no code change
was made.

> **Note on `-max_total_time`:** On WSL2, libFuzzer's time-limit flag (`-max_total_time=N`)
> is unreliable because `SIGALRM` delivery is inconsistent. Use `-runs=N` instead to
> control run length by iteration count.

## Running tests

Tests use [doctest](https://github.com/doctest/doctest) (vendored single header; no download needed).

### Linux / macOS

```sh
ctest --test-dir build
```

### Windows

```bat
ctest --test-dir build -C Debug
```

For verbose output showing each test case:

```sh
# Linux / macOS
./build/tests/test_ez_regex

# Windows
.\build\tests\Debug\test_ez_regex.exe
```

## Benchmarks

Benchmarks compare ezregex against `std::regex` in two modes:

- **hot** — `std::regex` object pre-compiled once, loop measures matching only
- **cold** — `std::regex` object constructed inside the loop, models one-shot usage

Uses [nanobench](https://github.com/martinus/nanobench) v4.3.11, downloaded automatically
by CMake at configure time (requires internet access and git).

### Build

```sh
# Linux / macOS
cmake -B build -DEZREGEX_BUILD_BENCHMARKS=ON
cmake --build build --config Release

# Windows
cmake -B build -G "Visual Studio 17 2022" -DEZREGEX_BUILD_BENCHMARKS=ON
cmake --build build --config Release
```

### Run

```sh
# Linux / macOS
./build/benchmarks/bench_ez_regex

# Windows
.\build\benchmarks\Release\bench_ez_regex.exe
```

### Benchmark scenarios

| # | Pattern | Input | Notes |
|---|---------|-------|-------|
| 1 | `^\d{4}-\d{2}-\d{2}$` | `"2024-01-15"` | Anchored, no captures |
| 2 | `^(\w+)=(\w+)$` | `"timeout=30"` | Anchored, 2 captures |
| 3 | `\d+` | `"price: 42 USD"` | Substring search, 1 capture |
| 4 | `^([\w]+)@([\w]+)\.([a-z]+)$` | `"user@example.com"` | Anchored, 3 captures |
| 5 | `needle` | 10 000-char string, match at byte 9990 | Long input, late match |
| 6 | `\d{6}` | 1 000-char all-alpha string | No-match exhaustion |
| 7 | construction only | — | `std::regex` compile cost with no matching |

### Indicative results (MSVC, Release, Windows 10)

| Scenario | ezregex | std::regex hot | std::regex cold |
|---|---|---|---|
| ISO date | ~210 ns | ~2 200 ns | ~6 000 ns |
| Key=value | ~160 ns | ~2 400 ns | ~10 800 ns |
| Digit extraction | ~220 ns | ~4 200 ns | ~5 700 ns |
| Email | ~330 ns | ~3 400 ns | ~13 500 ns |
| Long string 10 KB | ~160 µs | ~43 µs | ~48 µs |
| No-match 1 KB | ~21 µs | ~430 µs | ~430 µs |

**Key observations:**

- For short inputs ezregex is **10–15× faster than `std::regex` hot** and **30–65× faster cold**,
  because there is no NFA/DFA compilation step and the recursive-descent matcher has
  very low overhead for patterns measured in tens of characters.
- On **long inputs** `std::regex` wins (~4×): its compiled automaton can scan the string in
  O(n) with a tight inner loop, whereas ezregex's substring-search outer loop retries the
  pattern at every position.
- `std::regex` **construction costs 4–6 µs** per object on MSVC — comparable to the total
  cost of an ezregex one-shot match. Applications that match many different patterns or
  cannot cache compiled regex objects benefit most from ezregex.

> **Note:** `std::regex` on MSVC has significant measurement variance due to internal heap
> allocation patterns. Some benchmark rows may show nanobench "Unstable" markers; the
> ratios remain consistent across runs.

## Repository layout

```
ezregex/
  CMakeLists.txt
  ez_regex.h              public API + error codes
  ez_regex.cpp            implementation
  tests/
    CMakeLists.txt
    doctest.h             vendored single-header test framework
    test_ez_regex.cpp      unit + integration tests
  benchmarks/
    CMakeLists.txt
    bench_ez_regex.cpp     ezregex vs std::regex benchmark suite
  fuzz/
    CMakeLists.txt
    fuzz_ez_regex.cpp      libFuzzer target (Clang + WSL2/Linux only)
    corpus/               seed inputs derived from the test suite
```
