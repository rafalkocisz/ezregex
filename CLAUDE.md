# ezregex — Project Constitution

> Non-domain-specific style rules (Orthodox C++, code formatting, testing, build options,
> benchmarking, sanitizers, fuzzing, naming, repository layout) live in **[EZ.md](EZ.md)**
> and apply to this project in full. This file contains only ezregex-specific decisions.

## Purpose
A minimal, dependency-free C++ regex matching library focused on simplicity and correctness.
Single header+source pair, Orthodox C++ style, ASCII-only, no Unicode.

## Public API

```cpp
namespace ez {

// regex    — null-terminated regex pattern
// str      — null-terminated string to match
// captures — if non-null: filled with string_views into str for each capture
//            group on success, and cleared on every call regardless of outcome
//            if nullptr (default): match proceeds normally, captures discarded
//
// Returns:  0  — successful match
//          >0  — no match (use EZ_REGEX_NO_MATCH)
//          <0  — invalid regex (use EZ_REGEX_ERR_* codes)
int regex_match(const char* regex,
                const char* str,
                std::vector<std::string_view>* captures = nullptr);

} // namespace ez
```

## Supported Regex Syntax

| Syntax       | Meaning                              |
|--------------|--------------------------------------|
| `.`          | Any character (wildcard)             |
| `?`          | Zero or one of the preceding element |
| `*`          | Zero or more (greedy)                |
| `+`          | One or more (greedy)                 |
| `{n}`        | Exactly n repetitions                |
| `{n,}`       | At least n repetitions               |
| `{n,m}`      | Between n and m repetitions          |
| `(...)`      | Capturing group (no nesting)         |
| `[abc]`      | Character class                      |
| `[a-z]`      | Character range                      |
| `[^abc]`     | Negated character class              |
| `\d \D`      | Digit / non-digit                    |
| `\w \W`      | Word char / non-word char            |
| `\s \S`      | Whitespace / non-whitespace          |
| `\t \n \r`   | Tab / newline / carriage-return      |
| `\b \B`      | Word boundary / non-word boundary    |
| `^`          | Anchor — start of string             |
| `$`          | Anchor — end of string               |
| `\.`         | Literal dot (escape metacharacter)   |
| `\( \)`      | Literal parentheses                  |
| `\[ \]`      | Literal brackets                     |
| `\* \+ \?`   | Literal quantifier characters        |
| `\{ \}`      | Literal braces                       |
| `\\`         | Literal backslash                    |

**Not supported:** alternation (`|`), nested groups, Unicode, lookahead/lookbehind.

## Matching semantics
- By default, matching is a **substring search**: the pattern may match anywhere in `str`.
- Anchor with `^` and/or `$` to require start/end of string.
- `captures` is cleared on every call. Views are valid as long as `str` is alive.

## Return / Error codes

```cpp
#define EZ_REGEX_MAX_CAPTURES  16   // max number of capture groups; override before including

#define EZ_REGEX_MATCH         0    // successful match
#define EZ_REGEX_NO_MATCH      1    // pattern did not match
#define EZ_REGEX_ERR_SYNTAX   -1    // generic catch-all (not returned by current code)
#define EZ_REGEX_ERR_DEPTH    -2    // too many capture groups (> EZ_REGEX_MAX_CAPTURES)
#define EZ_REGEX_ERR_ESCAPE   -3    // trailing backslash, or unknown \x escape sequence
#define EZ_REGEX_ERR_BRACKET  -4    // unclosed '[', or backslash at end of bracket content
#define EZ_REGEX_ERR_PAREN    -5    // unmatched '(' or ')'
#define EZ_REGEX_ERR_NESTING  -6    // nested capture groups (not supported)
```

`captures` is **always cleared** on entry regardless of outcome. Views into `str` are only valid while `str` is alive.

## Coding Standards
See [EZ.md § Code Style](EZ.md#code-style--orthodox-c). Domain-specific addition:
internal capture state is bounded by `EZ_REGEX_MAX_CAPTURES` (default 16, overridable
before `#include`).

## Naming Conventions
See [EZ.md § Naming Conventions](EZ.md#naming-conventions) for the full table and
rationale. ezregex-specific instantiation:

| Element | Name |
|---------|------|
| Namespace | `ez` |
| Public function | `ez::regex_match` |
| Return-code macros | `EZ_REGEX_MATCH`, `EZ_REGEX_NO_MATCH`, `EZ_REGEX_ERR_*` |
| Compile-time limit | `EZ_REGEX_MAX_CAPTURES` |
| White-box testing flag | `EZ_REGEX_TESTING` |
| CMake options | `EZREGEX_BUILD_TESTS`, `EZREGEX_BUILD_BENCHMARKS`, `EZREGEX_SANITIZE`, `EZREGEX_BUILD_FUZZ` |

## Architecture
- **No separate compilation step.** The engine is a recursive-descent matcher that
  parses and matches the regex in a single pass. Quantifiers are handled by bounded
  recursion, not by building an NFA/DFA graph.
- Source files: `ez_regex.h`, `ez_regex.cpp`.
- Unit tests: `tests/test_ez_regex.cpp` using the **doctest** framework.
- Build system: CMake ≥ 3.20.

## Repository Layout
Follows the canonical ez layout from [EZ.md § Repository Layout](EZ.md#repository-layout)
in full, including `benchmarks/` and `fuzz/`.

## Commands

```sh
# Configure + build (normal)
cmake -B build && cmake --build build

# Run tests
ctest --test-dir build

# Sanitizer build (Clang, WSL2/Linux)
cmake -B build-san -DEZREGEX_SANITIZE=ON -DCMAKE_CXX_COMPILER=clang++
cmake --build build-san && ./build-san/tests/test_ez_regex

# Fuzz build (Clang, WSL2/Linux)
cmake -B build-fuzz -DEZREGEX_BUILD_FUZZ=ON -DCMAKE_CXX_COMPILER=clang++
cmake --build build-fuzz
./build-fuzz/fuzz/fuzz_ez_regex fuzz/corpus/ -runs=500000
```

## Out of Scope
- NFA/DFA compilation, JIT, or any regex IR.
- Unicode / multibyte encodings.
- Alternation (`|`).
- Named captures, backreferences, lookahead/lookbehind.
- Streaming or incremental matching.
- Catastrophic backtracking protection (match step budget / complexity limit). The engine
  is a backtracking recursive-descent matcher and intentionally makes no guarantees about
  worst-case matching time. It is designed for trusted patterns only.
