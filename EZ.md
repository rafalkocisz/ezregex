# EZ Library Style Guide

Shared conventions for the **ez** family of minimal, dependency-free C++ libraries.
Each member follows these rules unless the domain gives a specific reason not to.

---

## Philosophy

- **One job, done simply.** An ez library solves a single, well-scoped problem. No
  scope creep; the out-of-scope list is as important as the feature list.
- **Zero dependencies.** The library ships as a single header + source pair. No third-party
  runtime dependencies; test/bench/fuzz tooling is kept strictly outside the public API.
- **Trusted callers.** Ez libraries target embedded use in larger applications with
  well-formed inputs. They do not defend against adversarial patterns or pathological
  inputs unless stated explicitly.
- **Readable over clever.** Code should be understandable without context. Prefer flat
  functions, concrete types, and explicit control flow over abstractions.

---

## Code Style — Orthodox C++

| Rule | Detail |
|------|--------|
| Standard | C++17, no extensions (`CMAKE_CXX_EXTENSIONS OFF`) |
| Exceptions | Disabled — no `throw`, no `try`/`catch` |
| RTTI | Disabled — no `dynamic_cast`, no `typeid` |
| Heap | No `new`/`delete`, no `malloc`/`free` inside the library core |
| STL containers | Not used inside the implementation; `std::string_view` as a thin view type is acceptable |
| Internal state | Fixed-size stack arrays bounded by a compile-time constant (e.g. `EZ<NAME>_MAX_ITEMS`) |
| Inheritance | No multiple inheritance; no virtual functions in hot paths |
| Functions | Short, flat, named after what they do; no multi-level nesting |
| Comments | Only where the *why* is non-obvious — hidden constraint, workaround, subtle invariant |

---

## API Conventions

### Shape
- **Single public function** (or a small, cohesive set) in the `ez` namespace.
- Function names include their domain to avoid ambiguity when multiple ez libraries
  are used together: `ez::regex_match`, `ez::json_parse`.
- Input strings are null-terminated `const char*` for C compatibility.
- Output is returned via an **optional nullable pointer** with a default of `nullptr`:
  the caller passes a pointer when they need the result, or omits it for a simple
  match/test call.
- Caller-owned containers — the library never allocates the output container.

### Return value
```
 0   — success / match
 1   — defined negative result (no match, not found, …)
<0   — invalid input / error (use EZ_<NAME>_ERR_* codes)
```
Always test `result < 0` to catch all error variants at once.

### Error codes
Define as macros in the public header:

```cpp
#define EZ_<NAME>_MATCH       0
#define EZ_<NAME>_NO_MATCH    1
#define EZ_<NAME>_ERR_SYNTAX -1   // generic catch-all (reserved; not returned by current code)
// ... specific errors at -2, -3, … with one code per class of error
```

Prefer **specific error codes** over a single generic one — callers benefit from
being able to distinguish e.g. bad escape from unclosed bracket.

### Compile-time limits
Expose tuneable constants as macros that callers can override before `#include`:

```cpp
#ifndef EZ_<NAME>_MAX_ITEMS
#define EZ_<NAME>_MAX_ITEMS 16
#endif
```

---

## Testing

- Framework: **doctest** — vendored as a single header (`tests/doctest.h`); no download required.
- Coverage: every public-API behaviour, every error code, every edge case in internal
  helpers (white-box).
- White-box access: compile the library with `EZ_<NAME>_TESTING` defined (set via CMake
  when building tests). Wrap each internal `static` function in a thin `_test_*` forwarder
  inside `#ifdef EZ_<NAME>_TESTING` blocks.
- Test structure: one `TEST_CASE` per behaviour; group related cases with `TEST_SUITE`.
  Names are declarative sentences: `"empty pattern matches anywhere"`.
- No mocks, no fakes — test against the real implementation.

---

## Build System

```cmake
cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(EZ<NAME>_BUILD_TESTS      "Build unit tests"                      ON)
option(EZ<NAME>_BUILD_BENCHMARKS "Build benchmarks"                      OFF)
option(EZ<NAME>_SANITIZE         "Build with ASan (+ UBSan on GCC/Clang)" OFF)
option(EZ<NAME>_BUILD_FUZZ       "Build libFuzzer target (Clang only)"   OFF)
```

- Tests are **on by default**; benchmarks, sanitizers, and fuzzing are opt-in.
- The `SANITIZE` and `BUILD_FUZZ` flags apply `add_compile_options` / `add_link_options`
  **before** `add_library` so every target in the tree picks them up.
- `BUILD_FUZZ` implies ASan + UBSan; `SANITIZE` is not required as a prerequisite.
- `BUILD_FUZZ` guards against non-Clang compilers with `message(FATAL_ERROR)`.

### Sanitizer flags

| Compiler | Flags |
|----------|-------|
| MSVC | `/fsanitize=address /Zi`; strip `/RTC1` and `/INCREMENTAL` |
| GCC / Clang | `-fsanitize=address,undefined -fno-omit-frame-pointer -g` |

Use a **separate build directory** (`build-san`, `build-fuzz`) to keep sanitised
builds isolated from the normal one.

---

## Benchmarking

- Framework: **nanobench** v4.3.11 — fetched via `FetchContent` at configure time
  (requires internet + git; only when `BUILD_BENCHMARKS=ON`).
- Always benchmark against a **standard-library or OS equivalent** in at least two
  modes: *hot* (pre-compiled/cached object) and *cold* (constructed inside the loop).
- Scenarios must cover: short inputs (fast path), long inputs (algorithmic boundary),
  no-match exhaustion, and — if applicable — construction/compile cost.
- Use `relative(true)` so the first entry is the 1.00× baseline.
- Tune `minEpochIterations` per scenario: ~25 000 for fast patterns; lower for slow
  scenarios to keep total runtime reasonable.
- Document results in the README with a table and key observations, including any
  known instability sources (e.g. MSVC heap-allocation variance).

---

## Sanitizers

### What to check
- **ASan**: out-of-bounds reads/writes, use-after-free, stack overflows.
- **UBSan**: signed integer overflow, null-pointer dereference, misaligned access.
  Pay particular attention to arithmetic in parser/accumulator loops (overflow guard
  pattern: `if (x > (INT_MAX - d) / 10) return false`).

### MSVC notes
- ASan runtime DLL must be on `PATH` before running the instrumented executable.
- Use `RelWithDebInfo` (not `Debug`) for the most actionable ASan stack traces.

### WSL2 / UBSan on Windows
Run under WSL2 with Clang for full ASan + UBSan coverage.

---

## Fuzzing

- Framework: **libFuzzer** (Clang built-in).
- Input format: `<primary-input> '\x00' <secondary-input>` — split on the first null
  byte. If absent, treat the whole input as the primary with an empty secondary.
- The fuzz target should assert **output invariants** (e.g. returned spans stay within
  the input buffer) so logic bugs become hard crashes, not silent wrong answers.
- Seed corpus in `fuzz/corpus/` — one file per feature area, derived from the test suite.
- Use `-runs=N` to bound run length; `-max_total_time=N` is unreliable on WSL2.
- Document any **slow units** or crashes found, and record whether they were fixed or
  accepted as known limitations.

---

## Documentation

### README.md (user-facing)
Sections in order:
1. One-line description
2. Features (bullet list) + **Not supported** line + **Performance / known limitations** note
3. Quick start (code example showing the simplest call first, then captures)
4. API reference (function signature, parameter table, return-value table)
5. Supported syntax / behaviour table (domain-specific)
6. Build instructions (Linux/macOS and Windows)
7. Sanitizers (build + run, MSVC notes)
8. Fuzzing (build + run, input format, seed corpus, findings)
9. Running tests
10. Benchmarks (build + run, scenarios table, indicative results, observations)
11. Repository layout

### CLAUDE.md (AI assistant constitution)
Sections in order:
1. Purpose (one paragraph)
2. Public API (signature + contract comment)
3. Supported behaviour (domain-specific table)
4. Matching / processing semantics
5. Return / error codes
6. Coding Standards (pointer to EZ.md + domain-specific additions)
7. Architecture (key design decisions)
8. Repository layout
9. **Commands** (configure, build, test, sanitize, fuzz — exact shell commands)
10. Out of Scope (explicit list)

---

## Repository Layout

```
ez<name>/
  CMakeLists.txt
  EZ.md                  ← this file (or a reference to a shared copy)
  ez<name>.h             public API + error codes
  ez<name>.cpp           implementation
  CLAUDE.md              AI assistant project constitution
  tests/
    CMakeLists.txt
    doctest.h            vendored single-header test framework
    test_ez<name>.cpp    unit + integration tests
  benchmarks/
    CMakeLists.txt
    bench_ez<name>.cpp   library vs std equivalent
  fuzz/
    CMakeLists.txt
    fuzz_ez<name>.cpp    libFuzzer target
    corpus/              seed inputs (one file per feature area)
```

---

## Naming Conventions

| Element | Convention | Example |
|---------|------------|---------|
| Namespace | `ez` — flat, one level, shared by all ez libraries | `ez::regex_match`, `ez::json_parse` |
| Public free functions | `snake_case` inside `ez::` | `ez::regex_match` |
| Public classes / structs | `PascalCase` inside `ez::` | `ez::Span`, `ez::Regex` |
| Public macros (return codes) | `EZ_<LIBRARY>_<CONDITION>` | `EZ_REGEX_MATCH`, `EZ_REGEX_ERR_ESCAPE` |
| Compile-time limits | `EZ_<LIBRARY>_MAX_<THING>` | `EZ_REGEX_MAX_CAPTURES` |
| Internal functions | `snake_case`, verb-first, `static` linkage | `match_here`, `parse_braces` |
| Internal structs | `PascalCase`, no `ez::` prefix | `CaptureState`, `CaptureGroup` |
| White-box test hooks | `_test_<name>`, compiled under `EZ_<LIBRARY>_TESTING` | `_test_parse_braces` |
| CMake options | `EZ<LIBRARY>_<OPTION>` (library name concatenated) | `EZREGEX_BUILD_TESTS`, `EZREGEX_SANITIZE` |

### Rationale

**Shared namespace, domain-prefixed functions.** All ez libraries live in `ez::`. Function
names include their domain (`regex_match`, `json_parse`) to avoid ambiguity when multiple ez
libraries are used together in the same translation unit.

**`PascalCase` classes, `snake_case` free functions.** Mirrors the C++ standard library
(`std::string_view`, `std::regex_match`) and makes the distinction between types and
callables visually immediate.

**`EZ_<LIBRARY>_` macro prefix.** Macros cannot be namespaced, so a per-library prefix with
explicit separators (`EZ_REGEX_`, `EZ_JSON_`) prevents collisions across the ez family even
when multiple headers are included in the same translation unit.

**CMake options use the concatenated library name** (`EZREGEX_`, `EZJSON_`) rather than the
underscore-separated macro prefix. This is a deliberate distinction: CMake cache variables
are a build-system interface, not a C++ identifier, and the shorter prefix is conventional
in CMake projects.
