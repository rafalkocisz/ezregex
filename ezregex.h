#pragma once

#include <string_view>
#include <vector>

#ifndef EZREGEX_MAX_CAPTURES
#define EZREGEX_MAX_CAPTURES 16
#endif

#define EZREGEX_MATCH         0
#define EZREGEX_NO_MATCH      1
#define EZREGEX_ERR_SYNTAX   -1   // generic syntax error (catch-all; not returned by current code)
#define EZREGEX_ERR_DEPTH    -2   // too many capture groups (> EZREGEX_MAX_CAPTURES)
#define EZREGEX_ERR_ESCAPE   -3   // trailing backslash, or unknown \x escape sequence
#define EZREGEX_ERR_BRACKET  -4   // unclosed '[', or backslash at end of bracket content
#define EZREGEX_ERR_PAREN    -5   // unmatched '(' or ')'
#define EZREGEX_ERR_NESTING  -6   // nested capture groups (not supported)

// regex     — null-terminated regex pattern
// str       — null-terminated string to match against
// captures  — if non-null, filled with one string_view per capture group on
//             success and cleared on every call; pass nullptr (or omit) when
//             capture results are not needed
//
// Returns EZREGEX_MATCH, EZREGEX_NO_MATCH, or a negative error code.
int ezregex_match(const char* regex,
                  const char* str,
                  std::vector<std::string_view>* captures = nullptr);
