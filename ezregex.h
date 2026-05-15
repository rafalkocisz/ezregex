#pragma once

#include <string_view>
#include <vector>

#ifndef EZ_REGEX_MAX_CAPTURES
#define EZ_REGEX_MAX_CAPTURES 16
#endif

#define EZ_REGEX_MATCH 0
#define EZ_REGEX_NO_MATCH 1
#define EZ_REGEX_ERR_SYNTAX -1  // generic syntax error (catch-all; not returned by current code)
#define EZ_REGEX_ERR_DEPTH -2   // too many capture groups (> EZ_REGEX_MAX_CAPTURES)
#define EZ_REGEX_ERR_ESCAPE -3  // trailing backslash, or unknown \x escape sequence
#define EZ_REGEX_ERR_BRACKET -4 // unclosed '[', or backslash at end of bracket content
#define EZ_REGEX_ERR_PAREN -5   // unmatched '(' or ')'
#define EZ_REGEX_ERR_NESTING -6 // nested capture groups (not supported)

namespace ez
{

// regex     — null-terminated regex pattern
// str       — null-terminated string to match against
// captures  — if non-null, filled with one string_view per capture group on
//             success and cleared on every call; pass nullptr (or omit) when
//             capture results are not needed
//
// Returns EZ_REGEX_MATCH, EZ_REGEX_NO_MATCH, or a negative error code.
int regex_match(const char* regex, const char* str,
                std::vector<std::string_view>* captures = nullptr);

} // namespace ez
