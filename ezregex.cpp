#include "ezregex.h"
#include <climits>

// ── Character predicates ──────────────────────────────────────────────────────

static bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static bool is_alpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool is_word(char c)
{
    return is_alpha(c) || is_digit(c) || c == '_';
}

static bool is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

// ── Single-atom matching ──────────────────────────────────────────────────────

// Match character c against the pattern atom starting at pat.
// Sets pat_advance to the number of pattern bytes consumed (1 or 2).
static bool match_one(char c, const char* pat, int& pat_advance)
{
    if (pat[0] == '\\') {
        pat_advance = 2;
        switch (pat[1]) {
            case 'd': return is_digit(c);
            case 'D': return !is_digit(c);
            case 'w': return is_word(c);
            case 'W': return !is_word(c);
            case 's': return is_space(c);
            case 'S': return !is_space(c);
            case 't': return c == '\t';
            case 'n': return c == '\n';
            case 'r': return c == '\r';
            case '.':
            case '(':
            case ')':
            case '[':
            case ']':
            case '*':
            case '+':
            case '?':
            case '{':
            case '}':
            case '\\': return c == pat[1];
            default: return false;
        }
    }

    pat_advance = 1;
    if (pat[0] == '.') {
        return c != '\0';
    }
    return c == pat[0];
}

// ── Character class matching ──────────────────────────────────────────────────

// Match character c against a bracket expression starting at pat[0] == '['.
// Sets pat_advance to the total bytes consumed (from '[' through the closing ']').
//
// Rules:
//  - ']' immediately after '[' or '[^' is treated as a literal, not a closer.
//  - '-' is a range operator only when between two non-escape, non-']' chars
//    and the right endpoint is neither ']' nor '\\' (which would start an escape).
//  - '\d', '\w', '\s' and their upper-case negations work inside classes.
//  - All literal escapes ('\.', '\(', etc.) work inside classes.
static bool match_class(char c, const char* pat, int& pat_advance)
{
    const char* p = pat + 1; // skip '['
    bool negate = false;
    bool matched = false;
    bool first = true;

    if (*p == '^') {
        negate = true;
        ++p;
    }

    while (*p != '\0') {
        if (*p == ']' && !first) {
            ++p;
            break;
        }

        if (*p == '\\') {
            if (*(p + 1) == '\0')
                break; // trailing backslash — stop safely
            int adv = 0;
            if (match_one(c, p, adv))
                matched = true;
            p += adv;
        } else if (*p != ']' && *(p + 1) == '-' && *(p + 2) != '\0' && *(p + 2) != ']' &&
                   *(p + 2) != '\\') {
            if ((unsigned char)c >= (unsigned char)*p &&
                (unsigned char)c <= (unsigned char)*(p + 2))
                matched = true;
            p += 3;
        } else {
            if (c == *p)
                matched = true;
            ++p;
        }

        first = false;
    }

    pat_advance = (int)(p - pat);
    return negate ? !matched : matched;
}

// ── Atom dispatch ─────────────────────────────────────────────────────────────

// Dispatch to match_class for '[...' patterns; match_one for everything else.
static bool match_atom(char c, const char* pat, int& pat_adv)
{
    if (pat[0] == '[')
        return match_class(c, pat, pat_adv);
    return match_one(c, pat, pat_adv);
}

// ── Atom advance (pattern-only, no string consumed) ───────────────────────────

// Return how many pattern bytes the atom at pat[0] consumes.
// Needed to locate the quantifier before the str[0]=='\0' guard.
static int atom_advance(const char* pat)
{
    if (pat[0] == '[') {
        int adv = 0;
        match_class('\0', pat, adv); // dry run: result discarded, adv is what we want
        return adv;
    }
    return pat[0] == '\\' ? 2 : 1;
}

// ── Brace quantifier parser ───────────────────────────────────────────────────

// Parse {n}, {n,}, or {n,m} starting at pat[0] == '{'.
// Sets lo, hi (INT_MAX for unbounded), quant_adv (bytes consumed incl. '{' and '}').
// Returns true if well-formed; false if malformed (caller treats '{' as a literal).
static bool parse_braces(const char* pat, int& lo, int& hi, int& quant_adv)
{
    const char* p = pat + 1; // skip '{'

    if (!is_digit(*p))
        return false;

    lo = 0;
    while (is_digit(*p)) {
        int d = *p - '0';
        if (lo > (INT_MAX - d) / 10)
            return false; // overflow guard
        lo = lo * 10 + d;
        ++p;
    }

    if (*p == '}') {
        hi = lo;
        quant_adv = (int)(p - pat) + 1;
        return true;
    }
    if (*p != ',')
        return false;
    ++p;

    if (*p == '}') {
        hi = INT_MAX;
        quant_adv = (int)(p - pat) + 1;
        return true;
    }
    if (!is_digit(*p))
        return false;

    hi = 0;
    while (is_digit(*p)) {
        int d = *p - '0';
        if (hi > (INT_MAX - d) / 10)
            return false; // overflow guard
        hi = hi * 10 + d;
        ++p;
    }

    if (*p != '}')
        return false;
    if (hi < lo)
        return false;

    quant_adv = (int)(p - pat) + 1;
    return true;
}

// ── Capture state ─────────────────────────────────────────────────────────────

struct CaptureGroup {
    const char* start;
    const char* end;
};

struct CaptureState {
    CaptureGroup groups[EZ_REGEX_MAX_CAPTURES];
    int count;
};

// ── Forward declaration (match_here ↔ match_quantifier mutual recursion) ──────

static bool match_here(const char* pat, const char* str, const char* str_start,
                       CaptureState& state);

// ── Greedy quantifier engine ──────────────────────────────────────────────────

// Match the atom at pat[0..atom_adv-1] between lo and hi times (greedy),
// then continue with match_here on the rest of the pattern.
// quant_adv is the byte length of the quantifier token (1 for ?*+, variable for {}).
static bool match_quantifier(const char* pat, int atom_adv, int lo, int hi, int quant_adv,
                             const char* str, const char* str_start, CaptureState& state)
{
    // Greedily count the maximum number of consecutive atom matches.
    int max_count = 0;
    {
        int dummy = 0;
        while (max_count < hi) {
            if (str[max_count] == '\0')
                break;
            dummy = 0;
            if (!match_atom(str[max_count], pat, dummy))
                break;
            ++max_count;
        }
    }

    // Try from longest to shortest (greedy), saving/restoring capture state.
    const char* pat_rest = pat + atom_adv + quant_adv;
    for (int n = max_count; n >= lo; --n) {
        CaptureState save = state;
        if (match_here(pat_rest, str + n, str_start, save)) {
            state = save;
            return true;
        }
    }
    return false;
}

// ── Core recursive matcher ────────────────────────────────────────────────────

static bool match_here(const char* pat, const char* str, const char* str_start, CaptureState& state)
{
    if (pat[0] == '\0')
        return true;

    // Zero-width assertions — do not advance str.
    if (pat[0] == '^')
        return (str == str_start) && match_here(pat + 1, str, str_start, state);

    if (pat[0] == '$')
        return (str[0] == '\0') && match_here(pat + 1, str, str_start, state);

    if (pat[0] == '\\' && (pat[1] == 'b' || pat[1] == 'B')) {
        char prev = (str > str_start) ? str[-1] : '\0';
        bool at_boundary = is_word(prev) != is_word(str[0]);
        return ((pat[1] == 'b') ? at_boundary : !at_boundary) &&
               match_here(pat + 2, str, str_start, state);
    }

    // Group markers — do not consume a character from str.
    if (pat[0] == '(') {
        if (state.count >= EZ_REGEX_MAX_CAPTURES)
            return false;
        int idx = state.count++;
        state.groups[idx].start = str;
        state.groups[idx].end = nullptr;
        return match_here(pat + 1, str, str_start, state);
    }

    if (pat[0] == ')') {
        for (int i = state.count - 1; i >= 0; --i) {
            if (state.groups[i].end == nullptr) {
                state.groups[i].end = str;
                return match_here(pat + 1, str, str_start, state);
            }
        }
        return false; // unmatched ')' — error detection added in Step 6
    }

    // Compute atom advance so we can peek at the quantifier BEFORE the str check.
    // This allows a*  a?  a{0,3} etc. to match zero times on an empty string.
    int adv = atom_advance(pat);

    // Dispatch to quantifier handler when one follows the atom.
    {
        int lo, hi, qadv;
        switch (pat[adv]) {
            case '?': return match_quantifier(pat, adv, 0, 1, 1, str, str_start, state);
            case '*': return match_quantifier(pat, adv, 0, INT_MAX, 1, str, str_start, state);
            case '+': return match_quantifier(pat, adv, 1, INT_MAX, 1, str, str_start, state);
            case '{':
                if (parse_braces(pat + adv, lo, hi, qadv))
                    return match_quantifier(pat, adv, lo, hi, qadv, str, str_start, state);
                break; // malformed braces — fall through and treat '{' on next call
            default: break;
        }
    }

    // No quantifier: consume exactly one character.
    if (str[0] == '\0')
        return false;

    if (!match_atom(str[0], pat, adv))
        return false;
    return match_here(pat + adv, str + 1, str_start, state);
}

// ── Test-accessible wrappers (compiled only with EZREGEX_TESTING) ─────────────
#ifdef EZ_REGEX_TESTING
bool _test_is_digit(char c)
{
    return is_digit(c);
}
bool _test_is_alpha(char c)
{
    return is_alpha(c);
}
bool _test_is_word(char c)
{
    return is_word(c);
}
bool _test_is_space(char c)
{
    return is_space(c);
}
bool _test_match_one(char c, const char* pat, int& adv)
{
    return match_one(c, pat, adv);
}
bool _test_match_class(char c, const char* pat, int& adv)
{
    return match_class(c, pat, adv);
}
bool _test_match_atom(char c, const char* pat, int& adv)
{
    return match_atom(c, pat, adv);
}
int _test_atom_advance(const char* pat)
{
    return atom_advance(pat);
}
bool _test_parse_braces(const char* pat, int& lo, int& hi, int& qadv)
{
    return parse_braces(pat, lo, hi, qadv);
}
#endif

// ── Public API (namespace ez) ─────────────────────────────────────────────────

// ── Pattern validator ─────────────────────────────────────────────────────────

// Walk the pattern once, checking structural validity.
// Returns EZ_REGEX_MATCH (0) if valid, or a negative error code.
static int validate_pattern(const char* pat)
{
    int depth = 0;  // 0 = outside a group, 1 = inside; >1 is an error (no nesting)
    int groups = 0; // total capture groups opened

    for (const char* p = pat; *p != '\0';) {
        if (*p == '\\') {
            if (*(p + 1) == '\0')
                return EZ_REGEX_ERR_ESCAPE; // trailing backslash
            switch (*(p + 1)) {
                case 'd':
                case 'D':
                case 'w':
                case 'W':
                case 's':
                case 'S':
                case 't':
                case 'n':
                case 'r':
                case 'b':
                case 'B':
                case '.':
                case '(':
                case ')':
                case '[':
                case ']':
                case '*':
                case '+':
                case '?':
                case '{':
                case '}':
                case '\\': break;
                default: return EZ_REGEX_ERR_ESCAPE; // unknown escape sequence
            }
            p += 2;
            continue;
        }

        if (*p == '[') {
            // Scan to matching ']', respecting escapes and the first-char rule.
            const char* q = p + 1;
            if (*q == '^')
                ++q;
            bool first = true;
            while (*q != '\0') {
                if (*q == ']' && !first) {
                    break;
                }
                if (*q == '\\') {
                    if (*(q + 1) == '\0')
                        return EZ_REGEX_ERR_BRACKET; // \ at end of content
                    q += 2;
                } else {
                    ++q;
                }
                first = false;
            }
            if (*q != ']')
                return EZ_REGEX_ERR_BRACKET; // unclosed '['
            p = q + 1;
            continue;
        }

        if (*p == '(') {
            if (depth > 0)
                return EZ_REGEX_ERR_NESTING; // nested groups not supported
            ++depth;
            ++groups;
            if (groups > EZ_REGEX_MAX_CAPTURES)
                return EZ_REGEX_ERR_DEPTH;
            ++p;
            continue;
        }

        if (*p == ')') {
            if (depth == 0)
                return EZ_REGEX_ERR_PAREN; // unmatched ')'
            --depth;
            ++p;
            continue;
        }

        ++p;
    }

    if (depth != 0)
        return EZ_REGEX_ERR_PAREN; // unclosed '('
    return EZ_REGEX_MATCH;
}

#ifdef EZ_REGEX_TESTING
int _test_validate_pattern(const char* pat)
{
    return validate_pattern(pat);
}
#endif

namespace ez
{

int regex_match(const char* regex, const char* str, std::vector<std::string_view>* captures)
{
    if (captures)
        captures->clear();

    int err = validate_pattern(regex);
    if (err < 0)
        return err;

    const char* p = str;
    while (true) {
        CaptureState state = {};
        if (match_here(regex, p, str, state)) {
            if (captures) {
                for (int i = 0; i < state.count; ++i) {
                    if (state.groups[i].end != nullptr)
                        captures->emplace_back(
                            state.groups[i].start,
                            (size_t)(state.groups[i].end - state.groups[i].start));
                }
            }
            return EZ_REGEX_MATCH;
        }
        // For anchored patterns only position 0 can succeed.
        if (*p == '\0' || regex[0] == '^')
            break;
        ++p;
    }
    return EZ_REGEX_NO_MATCH;
}

} // namespace ez
