#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "ezregex.h"

// ── White-box access to internal primitives ───────────────────────────────────
bool _test_is_digit(char c);
bool _test_is_alpha(char c);
bool _test_is_word(char c);
bool _test_is_space(char c);
bool _test_match_one(char c, const char* pat, int& adv);
bool _test_match_class(char c, const char* pat, int& adv);
bool _test_match_atom(char c, const char* pat, int& adv);
int _test_atom_advance(const char* pat);
bool _test_parse_braces(const char* pat, int& lo, int& hi, int& qadv);
int _test_validate_pattern(const char* pat);

// ─────────────────────────────────────────────────────────────────────────────
// Step 1: public-API stub contract
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("captures are cleared on entry even when no match")
{
    std::vector<std::string_view> caps;
    caps.emplace_back("stale");
    int r = ez::regex_match("a", "b", &caps);
    CHECK(r == EZ_REGEX_NO_MATCH);
    CHECK(caps.empty());
}

TEST_CASE("nullptr captures: simple match succeeds without a vector")
{
    CHECK(ez::regex_match("a", "a") == EZ_REGEX_MATCH);
    CHECK(ez::regex_match("a", "b") == EZ_REGEX_NO_MATCH);
    CHECK(ez::regex_match("^\\d+$", "42") == EZ_REGEX_MATCH);
    CHECK(ez::regex_match("^\\d+$", "abc") == EZ_REGEX_NO_MATCH);
}

TEST_CASE("nullptr captures: pattern with groups still matches correctly")
{
    // Groups participate in matching; results are simply not returned.
    CHECK(ez::regex_match("^(\\w+)=(\\w+)$", "key=val") == EZ_REGEX_MATCH);
    CHECK(ez::regex_match("^(\\w+)=(\\w+)$", "bad") == EZ_REGEX_NO_MATCH);
}

TEST_CASE("nullptr captures: error codes are still returned")
{
    CHECK(ez::regex_match("(bad[pat", "x") == EZ_REGEX_ERR_BRACKET);
    CHECK(ez::regex_match("\\q", "x") == EZ_REGEX_ERR_ESCAPE);
}

// ─────────────────────────────────────────────────────────────────────────────
// Step 2: character predicates
// ─────────────────────────────────────────────────────────────────────────────

TEST_SUITE("is_digit")
{

    TEST_CASE("digit boundaries and mid-range")
    {
        CHECK(_test_is_digit('0'));
        CHECK(_test_is_digit('9'));
        CHECK(_test_is_digit('5'));
    }

    TEST_CASE("chars just outside digit range")
    {
        CHECK_FALSE(_test_is_digit('/')); // ASCII 47, one below '0'
        CHECK_FALSE(_test_is_digit(':')); // ASCII 58, one above '9'
    }

    TEST_CASE("non-digit chars")
    {
        CHECK_FALSE(_test_is_digit('a'));
        CHECK_FALSE(_test_is_digit('Z'));
        CHECK_FALSE(_test_is_digit(' '));
        CHECK_FALSE(_test_is_digit('\0'));
    }

} // TEST_SUITE("is_digit")

TEST_SUITE("is_alpha")
{

    TEST_CASE("lowercase boundaries and mid-range")
    {
        CHECK(_test_is_alpha('a'));
        CHECK(_test_is_alpha('z'));
        CHECK(_test_is_alpha('m'));
    }

    TEST_CASE("uppercase boundaries and mid-range")
    {
        CHECK(_test_is_alpha('A'));
        CHECK(_test_is_alpha('Z'));
        CHECK(_test_is_alpha('M'));
    }

    TEST_CASE("chars just outside lowercase range")
    {
        CHECK_FALSE(_test_is_alpha('`')); // ASCII 96, one below 'a'
        CHECK_FALSE(_test_is_alpha('{')); // ASCII 123, one above 'z'
    }

    TEST_CASE("chars just outside uppercase range")
    {
        CHECK_FALSE(_test_is_alpha('@')); // ASCII 64, one below 'A'
        CHECK_FALSE(_test_is_alpha('[')); // ASCII 91, one above 'Z'
    }

    TEST_CASE("non-alpha chars")
    {
        CHECK_FALSE(_test_is_alpha('0'));
        CHECK_FALSE(_test_is_alpha('_'));
        CHECK_FALSE(_test_is_alpha(' '));
        CHECK_FALSE(_test_is_alpha('\0'));
    }

} // TEST_SUITE("is_alpha")

TEST_SUITE("is_word")
{

    TEST_CASE("letters are word chars")
    {
        CHECK(_test_is_word('a'));
        CHECK(_test_is_word('z'));
        CHECK(_test_is_word('A'));
        CHECK(_test_is_word('Z'));
    }

    TEST_CASE("digits are word chars")
    {
        CHECK(_test_is_word('0'));
        CHECK(_test_is_word('9'));
    }

    TEST_CASE("underscore is a word char")
    {
        CHECK(_test_is_word('_'));
    }

    TEST_CASE("non-word chars")
    {
        CHECK_FALSE(_test_is_word(' '));
        CHECK_FALSE(_test_is_word('!'));
        CHECK_FALSE(_test_is_word('-'));
        CHECK_FALSE(_test_is_word('.'));
        CHECK_FALSE(_test_is_word('\0'));
    }

} // TEST_SUITE("is_word")

TEST_SUITE("is_space")
{

    TEST_CASE("all six whitespace characters")
    {
        CHECK(_test_is_space(' '));
        CHECK(_test_is_space('\t'));
        CHECK(_test_is_space('\n'));
        CHECK(_test_is_space('\r'));
        CHECK(_test_is_space('\f'));
        CHECK(_test_is_space('\v'));
    }

    TEST_CASE("non-space chars")
    {
        CHECK_FALSE(_test_is_space('a'));
        CHECK_FALSE(_test_is_space('0'));
        CHECK_FALSE(_test_is_space('!'));
        CHECK_FALSE(_test_is_space('\0'));
    }

} // TEST_SUITE("is_space")

TEST_SUITE("match_one")
{

    TEST_CASE("wildcard '.' matches any non-null char")
    {
        int adv;
        adv = 0;
        CHECK(_test_match_one('a', ".", adv));
        CHECK(adv == 1);
        adv = 0;
        CHECK(_test_match_one('0', ".", adv));
        CHECK(adv == 1);
        adv = 0;
        CHECK(_test_match_one('!', ".", adv));
        CHECK(adv == 1);
        adv = 0;
        CHECK(_test_match_one(' ', ".", adv));
        CHECK(adv == 1);
        adv = 0;
        CHECK(_test_match_one('\t', ".", adv));
        CHECK(adv == 1);
        adv = 0;
        CHECK(_test_match_one('\n', ".", adv));
        CHECK(adv == 1);
    }

    TEST_CASE("wildcard '.' does not match null")
    {
        int adv = 0;
        CHECK_FALSE(_test_match_one('\0', ".", adv));
        CHECK(adv == 1);
    }

    TEST_CASE("\\d matches digits, not others")
    {
        int adv;
        adv = 0;
        CHECK(_test_match_one('0', "\\d", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK(_test_match_one('9', "\\d", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK_FALSE(_test_match_one('a', "\\d", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK_FALSE(_test_match_one(' ', "\\d", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK_FALSE(_test_match_one('_', "\\d", adv));
        CHECK(adv == 2);
    }

    TEST_CASE("\\D matches non-digits, not digits")
    {
        int adv;
        adv = 0;
        CHECK(_test_match_one('a', "\\D", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK(_test_match_one('_', "\\D", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK(_test_match_one(' ', "\\D", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK_FALSE(_test_match_one('0', "\\D", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK_FALSE(_test_match_one('9', "\\D", adv));
        CHECK(adv == 2);
    }

    TEST_CASE("\\w matches word chars, not others")
    {
        int adv;
        adv = 0;
        CHECK(_test_match_one('a', "\\w", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK(_test_match_one('Z', "\\w", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK(_test_match_one('5', "\\w", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK(_test_match_one('_', "\\w", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK_FALSE(_test_match_one('!', "\\w", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK_FALSE(_test_match_one(' ', "\\w", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK_FALSE(_test_match_one('-', "\\w", adv));
        CHECK(adv == 2);
    }

    TEST_CASE("\\W matches non-word chars, not word chars")
    {
        int adv;
        adv = 0;
        CHECK(_test_match_one('!', "\\W", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK(_test_match_one(' ', "\\W", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK(_test_match_one('-', "\\W", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK_FALSE(_test_match_one('a', "\\W", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK_FALSE(_test_match_one('0', "\\W", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK_FALSE(_test_match_one('_', "\\W", adv));
        CHECK(adv == 2);
    }

    TEST_CASE("\\s matches all six whitespace chars, not others")
    {
        int adv;
        adv = 0;
        CHECK(_test_match_one(' ', "\\s", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK(_test_match_one('\t', "\\s", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK(_test_match_one('\n', "\\s", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK(_test_match_one('\r', "\\s", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK(_test_match_one('\f', "\\s", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK(_test_match_one('\v', "\\s", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK_FALSE(_test_match_one('a', "\\s", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK_FALSE(_test_match_one('0', "\\s", adv));
        CHECK(adv == 2);
    }

    TEST_CASE("\\S matches non-space, not whitespace")
    {
        int adv;
        adv = 0;
        CHECK(_test_match_one('a', "\\S", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK(_test_match_one('0', "\\S", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK(_test_match_one('_', "\\S", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK_FALSE(_test_match_one(' ', "\\S", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK_FALSE(_test_match_one('\t', "\\S", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK_FALSE(_test_match_one('\n', "\\S", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK_FALSE(_test_match_one('\r', "\\S", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK_FALSE(_test_match_one('\f', "\\S", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK_FALSE(_test_match_one('\v', "\\S", adv));
        CHECK(adv == 2);
    }

    TEST_CASE("literal escape sequences: each matches its own char, adv == 2")
    {
        struct {
            const char* pat;
            char ch;
        } cases[] = {
            {"\\.", '.'}, {"\\(", '('}, {"\\)", ')'}, {"\\[", '['}, {"\\]", ']'},   {"\\*", '*'},
            {"\\+", '+'}, {"\\?", '?'}, {"\\{", '{'}, {"\\}", '}'}, {"\\\\", '\\'},
        };
        for (auto& tc : cases) {
            int adv = 0;
            CHECK(_test_match_one(tc.ch, tc.pat, adv));
            CHECK(adv == 2);
            adv = 0;
            CHECK_FALSE(_test_match_one('x', tc.pat, adv));
            CHECK(adv == 2);
        }
    }

    TEST_CASE("\\t \\n \\r match the corresponding control characters, adv == 2")
    {
        int adv;
        adv = 0;
        CHECK(_test_match_one('\t', "\\t", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK_FALSE(_test_match_one('t', "\\t", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK(_test_match_one('\n', "\\n", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK_FALSE(_test_match_one('n', "\\n", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK(_test_match_one('\r', "\\r", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK_FALSE(_test_match_one('r', "\\r", adv));
        CHECK(adv == 2);
    }

    TEST_CASE("unknown escape returns false, adv == 2")
    {
        int adv;
        adv = 0;
        CHECK_FALSE(_test_match_one('x', "\\x", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK_FALSE(_test_match_one('e', "\\e", adv));
        CHECK(adv == 2);
    }

    TEST_CASE("literal character: matches itself, not others, adv == 1")
    {
        int adv;
        adv = 0;
        CHECK(_test_match_one('a', "a", adv));
        CHECK(adv == 1);
        adv = 0;
        CHECK(_test_match_one('Z', "Z", adv));
        CHECK(adv == 1);
        adv = 0;
        CHECK(_test_match_one('0', "0", adv));
        CHECK(adv == 1);
        adv = 0;
        CHECK_FALSE(_test_match_one('b', "a", adv));
        CHECK(adv == 1);
        adv = 0;
        CHECK_FALSE(_test_match_one('a', "b", adv));
        CHECK(adv == 1);
        adv = 0;
        CHECK_FALSE(_test_match_one('1', "0", adv));
        CHECK(adv == 1);
    }

} // TEST_SUITE("match_one")

// ─────────────────────────────────────────────────────────────────────────────
// Step 3: bracket expression matching
// ─────────────────────────────────────────────────────────────────────────────

TEST_SUITE("match_class")
{

    TEST_CASE("single char [a]")
    {
        int adv;
        adv = 0;
        CHECK(_test_match_class('a', "[a]", adv));
        CHECK(adv == 3);
        adv = 0;
        CHECK_FALSE(_test_match_class('b', "[a]", adv));
        CHECK(adv == 3);
    }

    TEST_CASE("literal chars [abc]")
    {
        int adv;
        adv = 0;
        CHECK(_test_match_class('a', "[abc]", adv));
        CHECK(adv == 5);
        adv = 0;
        CHECK(_test_match_class('b', "[abc]", adv));
        CHECK(adv == 5);
        adv = 0;
        CHECK(_test_match_class('c', "[abc]", adv));
        CHECK(adv == 5);
        adv = 0;
        CHECK_FALSE(_test_match_class('d', "[abc]", adv));
        CHECK(adv == 5);
        adv = 0;
        CHECK_FALSE(_test_match_class('0', "[abc]", adv));
        CHECK(adv == 5);
    }

    TEST_CASE("range [a-z]")
    {
        int adv;
        adv = 0;
        CHECK(_test_match_class('a', "[a-z]", adv));
        CHECK(adv == 5);
        adv = 0;
        CHECK(_test_match_class('m', "[a-z]", adv));
        CHECK(adv == 5);
        adv = 0;
        CHECK(_test_match_class('z', "[a-z]", adv));
        CHECK(adv == 5);
        adv = 0;
        CHECK_FALSE(_test_match_class('`', "[a-z]", adv));
        CHECK(adv == 5); // one below 'a'
        adv = 0;
        CHECK_FALSE(_test_match_class('{', "[a-z]", adv));
        CHECK(adv == 5); // one above 'z'
        adv = 0;
        CHECK_FALSE(_test_match_class('A', "[a-z]", adv));
        CHECK(adv == 5);
        adv = 0;
        CHECK_FALSE(_test_match_class('0', "[a-z]", adv));
        CHECK(adv == 5);
    }

    TEST_CASE("digit range [0-9]")
    {
        int adv;
        adv = 0;
        CHECK(_test_match_class('0', "[0-9]", adv));
        CHECK(adv == 5);
        adv = 0;
        CHECK(_test_match_class('9', "[0-9]", adv));
        CHECK(adv == 5);
        adv = 0;
        CHECK_FALSE(_test_match_class('/', "[0-9]", adv));
        CHECK(adv == 5); // one below '0'
        adv = 0;
        CHECK_FALSE(_test_match_class(':', "[0-9]", adv));
        CHECK(adv == 5); // one above '9'
    }

    TEST_CASE("multiple ranges [a-zA-Z0-9]")
    {
        int adv;
        adv = 0;
        CHECK(_test_match_class('a', "[a-zA-Z0-9]", adv));
        CHECK(adv == 11);
        adv = 0;
        CHECK(_test_match_class('Z', "[a-zA-Z0-9]", adv));
        CHECK(adv == 11);
        adv = 0;
        CHECK(_test_match_class('5', "[a-zA-Z0-9]", adv));
        CHECK(adv == 11);
        adv = 0;
        CHECK_FALSE(_test_match_class('_', "[a-zA-Z0-9]", adv));
        CHECK(adv == 11);
        adv = 0;
        CHECK_FALSE(_test_match_class(' ', "[a-zA-Z0-9]", adv));
        CHECK(adv == 11);
    }

    TEST_CASE("negated class [^abc]")
    {
        int adv;
        adv = 0;
        CHECK_FALSE(_test_match_class('a', "[^abc]", adv));
        CHECK(adv == 6);
        adv = 0;
        CHECK_FALSE(_test_match_class('b', "[^abc]", adv));
        CHECK(adv == 6);
        adv = 0;
        CHECK(_test_match_class('d', "[^abc]", adv));
        CHECK(adv == 6);
        adv = 0;
        CHECK(_test_match_class('0', "[^abc]", adv));
        CHECK(adv == 6);
    }

    TEST_CASE("negated range [^a-z]")
    {
        int adv;
        adv = 0;
        CHECK_FALSE(_test_match_class('m', "[^a-z]", adv));
        CHECK(adv == 6);
        adv = 0;
        CHECK(_test_match_class('A', "[^a-z]", adv));
        CHECK(adv == 6);
        adv = 0;
        CHECK(_test_match_class('0', "[^a-z]", adv));
        CHECK(adv == 6);
        adv = 0;
        CHECK(_test_match_class('!', "[^a-z]", adv));
        CHECK(adv == 6);
    }

    TEST_CASE("']' as first char []abc]")
    {
        int adv;
        adv = 0;
        CHECK(_test_match_class(']', "[]abc]", adv));
        CHECK(adv == 6);
        adv = 0;
        CHECK(_test_match_class('a', "[]abc]", adv));
        CHECK(adv == 6);
        adv = 0;
        CHECK_FALSE(_test_match_class('d', "[]abc]", adv));
        CHECK(adv == 6);
    }

    TEST_CASE("'^' with ']' as first char [^]abc]")
    {
        int adv;
        adv = 0;
        CHECK_FALSE(_test_match_class(']', "[^]abc]", adv));
        CHECK(adv == 7);
        adv = 0;
        CHECK_FALSE(_test_match_class('a', "[^]abc]", adv));
        CHECK(adv == 7);
        adv = 0;
        CHECK(_test_match_class('d', "[^]abc]", adv));
        CHECK(adv == 7);
    }

    TEST_CASE("'-' as first char [-abc]")
    {
        int adv;
        adv = 0;
        CHECK(_test_match_class('-', "[-abc]", adv));
        CHECK(adv == 6);
        adv = 0;
        CHECK(_test_match_class('a', "[-abc]", adv));
        CHECK(adv == 6);
        adv = 0;
        CHECK_FALSE(_test_match_class('d', "[-abc]", adv));
        CHECK(adv == 6);
    }

    TEST_CASE("'-' as last char [abc-]")
    {
        int adv;
        adv = 0;
        CHECK(_test_match_class('a', "[abc-]", adv));
        CHECK(adv == 6);
        adv = 0;
        CHECK(_test_match_class('-', "[abc-]", adv));
        CHECK(adv == 6);
        adv = 0;
        CHECK_FALSE(_test_match_class('d', "[abc-]", adv));
        CHECK(adv == 6);
    }

    TEST_CASE("'-' as only char [-]")
    {
        int adv;
        adv = 0;
        CHECK(_test_match_class('-', "[-]", adv));
        CHECK(adv == 3);
        adv = 0;
        CHECK_FALSE(_test_match_class('a', "[-]", adv));
        CHECK(adv == 3);
    }

    TEST_CASE("reverse range [z-a] — empty, nothing matches")
    {
        int adv;
        adv = 0;
        CHECK_FALSE(_test_match_class('a', "[z-a]", adv));
        CHECK(adv == 5);
        adv = 0;
        CHECK_FALSE(_test_match_class('m', "[z-a]", adv));
        CHECK(adv == 5);
        adv = 0;
        CHECK_FALSE(_test_match_class('z', "[z-a]", adv));
        CHECK(adv == 5);
    }

    TEST_CASE("range end blocked by '\\': '-' becomes literal [a-\\d]")
    {
        // *(p+2)=='\\' prevents 'a'-'\\d' being parsed as a range;
        // 'a', '-', \d (digit shorthand) are treated independently.
        int adv;
        adv = 0;
        CHECK(_test_match_class('a', "[a-\\d]", adv));
        CHECK(adv == 6);
        adv = 0;
        CHECK(_test_match_class('-', "[a-\\d]", adv));
        CHECK(adv == 6);
        adv = 0;
        CHECK(_test_match_class('5', "[a-\\d]", adv));
        CHECK(adv == 6); // \d matched
        adv = 0;
        CHECK_FALSE(_test_match_class('b', "[a-\\d]", adv));
        CHECK(adv == 6);
    }

    TEST_CASE("\\d inside class [\\d]")
    {
        int adv;
        adv = 0;
        CHECK(_test_match_class('0', "[\\d]", adv));
        CHECK(adv == 4);
        adv = 0;
        CHECK(_test_match_class('9', "[\\d]", adv));
        CHECK(adv == 4);
        adv = 0;
        CHECK_FALSE(_test_match_class('a', "[\\d]", adv));
        CHECK(adv == 4);
    }

    TEST_CASE("\\w inside class [\\w]")
    {
        int adv;
        adv = 0;
        CHECK(_test_match_class('a', "[\\w]", adv));
        CHECK(adv == 4);
        adv = 0;
        CHECK(_test_match_class('_', "[\\w]", adv));
        CHECK(adv == 4);
        adv = 0;
        CHECK_FALSE(_test_match_class(' ', "[\\w]", adv));
        CHECK(adv == 4);
    }

    TEST_CASE("\\s inside class [\\s]")
    {
        int adv;
        adv = 0;
        CHECK(_test_match_class(' ', "[\\s]", adv));
        CHECK(adv == 4);
        adv = 0;
        CHECK(_test_match_class('\t', "[\\s]", adv));
        CHECK(adv == 4);
        adv = 0;
        CHECK_FALSE(_test_match_class('a', "[\\s]", adv));
        CHECK(adv == 4);
    }

    TEST_CASE("\\D inside class [\\D]")
    {
        int adv;
        adv = 0;
        CHECK(_test_match_class('a', "[\\D]", adv));
        CHECK(adv == 4);
        adv = 0;
        CHECK_FALSE(_test_match_class('0', "[\\D]", adv));
        CHECK(adv == 4);
    }

    TEST_CASE("\\W inside class [\\W]")
    {
        int adv;
        adv = 0;
        CHECK(_test_match_class('!', "[\\W]", adv));
        CHECK(adv == 4);
        adv = 0;
        CHECK_FALSE(_test_match_class('a', "[\\W]", adv));
        CHECK(adv == 4);
    }

    TEST_CASE("\\S inside class [\\S]")
    {
        int adv;
        adv = 0;
        CHECK(_test_match_class('a', "[\\S]", adv));
        CHECK(adv == 4);
        adv = 0;
        CHECK_FALSE(_test_match_class(' ', "[\\S]", adv));
        CHECK(adv == 4);
    }

    TEST_CASE("literal backslash inside class [\\\\]")
    {
        int adv;
        adv = 0;
        CHECK(_test_match_class('\\', "[\\\\]", adv));
        CHECK(adv == 4);
        adv = 0;
        CHECK_FALSE(_test_match_class('a', "[\\\\]", adv));
        CHECK(adv == 4);
    }

    TEST_CASE("literal dot inside class [\\.]")
    {
        int adv;
        adv = 0;
        CHECK(_test_match_class('.', "[\\.]", adv));
        CHECK(adv == 4);
        adv = 0;
        CHECK_FALSE(_test_match_class('a', "[\\.]", adv));
        CHECK(adv == 4);
    }

    TEST_CASE("mixed range and escape [a-z\\d_]")
    {
        int adv;
        adv = 0;
        CHECK(_test_match_class('m', "[a-z\\d_]", adv));
        CHECK(adv == 8);
        adv = 0;
        CHECK(_test_match_class('5', "[a-z\\d_]", adv));
        CHECK(adv == 8);
        adv = 0;
        CHECK(_test_match_class('_', "[a-z\\d_]", adv));
        CHECK(adv == 8);
        adv = 0;
        CHECK_FALSE(_test_match_class(' ', "[a-z\\d_]", adv));
        CHECK(adv == 8);
        adv = 0;
        CHECK_FALSE(_test_match_class('!', "[a-z\\d_]", adv));
        CHECK(adv == 8);
    }

    TEST_CASE("negated class with escape [^\\d]")
    {
        int adv;
        adv = 0;
        CHECK(_test_match_class('a', "[^\\d]", adv));
        CHECK(adv == 5);
        adv = 0;
        CHECK_FALSE(_test_match_class('0', "[^\\d]", adv));
        CHECK(adv == 5);
    }

    TEST_CASE("unclosed class does not crash")
    {
        int adv = 0;
        (void)_test_match_class('a', "[abc", adv);
    }

    TEST_CASE("trailing backslash in class does not crash")
    {
        int adv = 0;
        (void)_test_match_class('a', "[a\\", adv);
    }

} // TEST_SUITE("match_class")

// ─────────────────────────────────────────────────────────────────────────────
// Step 4: match_atom dispatch + core matcher (no quantifiers)
// ─────────────────────────────────────────────────────────────────────────────

TEST_SUITE("match_atom")
{

    TEST_CASE("dispatches to match_class for '[' patterns")
    {
        int adv = 0;
        CHECK(_test_match_atom('a', "[abc]", adv));
        CHECK(adv == 5);
        adv = 0;
        CHECK_FALSE(_test_match_atom('d', "[abc]", adv));
        CHECK(adv == 5);
    }

    TEST_CASE("dispatches to match_one for non-'[' patterns")
    {
        int adv;
        adv = 0;
        CHECK(_test_match_atom('a', "a", adv));
        CHECK(adv == 1);
        adv = 0;
        CHECK(_test_match_atom('5', "\\d", adv));
        CHECK(adv == 2);
        adv = 0;
        CHECK(_test_match_atom('x', ".", adv));
        CHECK(adv == 1);
        adv = 0;
        CHECK_FALSE(_test_match_atom('b', "a", adv));
        CHECK(adv == 1);
        adv = 0;
        CHECK_FALSE(_test_match_atom('a', "\\d", adv));
        CHECK(adv == 2);
    }

} // TEST_SUITE("match_atom")

TEST_SUITE("core_matcher")
{

    // ── Empty pattern ─────────────────────────────────────────────────────────────

    TEST_CASE("empty pattern matches empty string")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("", "", &caps) == EZ_REGEX_MATCH);
        CHECK(caps.empty());
    }

    TEST_CASE("empty pattern matches non-empty string at position 0")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("", "abc", &caps) == EZ_REGEX_MATCH);
    }

    // ── Literal sequences ─────────────────────────────────────────────────────────

    TEST_CASE("exact literal match")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("abc", "abc", &caps) == EZ_REGEX_MATCH);
    }

    TEST_CASE("literal found as substring")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("abc", "xabcx", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("abc", "xabc", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("abc", "abcx", &caps) == EZ_REGEX_MATCH);
    }

    TEST_CASE("literal no match")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("abc", "xyz", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("abc", "ab", &caps) == EZ_REGEX_NO_MATCH); // pattern longer
        CHECK(ez::regex_match("abc", "", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("abc", "ABC", &caps) == EZ_REGEX_NO_MATCH); // case sensitive
    }

    TEST_CASE("single character match and no-match")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("a", "a", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("a", "b", &caps) == EZ_REGEX_NO_MATCH);
    }

    // ── Wildcard ─────────────────────────────────────────────────────────────────

    TEST_CASE("wildcard matches any char in sequence")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("h.llo", "hello", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("h.llo", "hXllo", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("h.llo", "h5llo", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("h.llo", "h llo", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("...", "abc", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("...", "xyz", &caps) == EZ_REGEX_MATCH);
    }

    TEST_CASE("wildcard no match when string too short")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("...", "ab", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("..", "a", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match(".", "", &caps) == EZ_REGEX_NO_MATCH);
    }

    // ── Escape sequences ──────────────────────────────────────────────────────────

    TEST_CASE("digit escape \\d")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("\\d", "5", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("\\d", "a", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("\\d\\d\\d", "123", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("\\d\\d\\d", "12a", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("word escape \\w")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("\\w", "a", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("\\w", "_", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("\\w", "5", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("\\w", " ", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("\\w", "!", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("space escape \\s")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("\\s", " ", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("\\s", "\t", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("\\s", "a", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("negated escapes \\D \\W \\S")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("\\D", "a", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("\\D", "5", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("\\W", "!", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("\\W", "a", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("\\S", "a", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("\\S", " ", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("literal dot escape matches only '.'")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("a\\.b", "a.b", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("a\\.b", "axb", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("\\.", ".", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("\\.", "a", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("other literal escapes")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("\\(", "(", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("\\)", ")", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("\\[", "[", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("\\]", "]", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("\\*", "*", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("\\+", "+", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("\\?", "?", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("\\\\", "\\", &caps) == EZ_REGEX_MATCH);
    }

    // ── Character classes ─────────────────────────────────────────────────────────

    TEST_CASE("character class [aeiou]")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("[aeiou]", "a", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("[aeiou]", "e", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("[aeiou]", "b", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("[aeiou]", "c", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("character range [a-z]")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("[a-z]", "m", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("[a-z]", "A", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("[0-9]", "5", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("[0-9]", "a", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("negated class [^abc]")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("[^abc]", "d", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("[^abc]", "a", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("class sequence [a-z][0-9]")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("[a-z][0-9]", "m5", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("[a-z][0-9]", "5m", &caps) == EZ_REGEX_NO_MATCH);
    }

    // ── Anchors ───────────────────────────────────────────────────────────────────

    TEST_CASE("'^' anchors to start of string")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^abc", "abc", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^abc", "xabc", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^abc", "abc extra", &caps) == EZ_REGEX_MATCH); // suffix ok
    }

    TEST_CASE("'$' anchors to end of string")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("abc$", "abc", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("abc$", "abcx", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("abc$", "xabc", &caps) == EZ_REGEX_MATCH); // prefix ok
    }

    TEST_CASE("'^...$' requires full string match")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^abc$", "abc", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^abc$", "xabc", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^abc$", "abcx", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^abc$", "xabcx", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^abc$", "", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("'^' and '$' on empty string")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^", "", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("$", "", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^$", "", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^$", "a", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("'^' inside pattern acts as mid-string assertion")
    {
        std::vector<std::string_view> caps;
        // 'a' then '^' (not at start) then 'b': can only match if 'a' IS at start
        // i.e. str == str_start when '^' is reached, which means pattern starts matching at 0
        // "a^b" on "ab": at pos 0, match 'a', then '^' checks str=="ab"[1]!="ab"[0] → fail
        CHECK(ez::regex_match("a^b", "ab", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("'$' inside pattern is a zero-width end assertion")
    {
        std::vector<std::string_view> caps;
        // "abc$xyz" can never match: after 'abc', we need end-of-string, but 'x' follows
        CHECK(ez::regex_match("abc$xyz", "abcxyz", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("abc$xyz", "abc", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("'^' inside pattern is a zero-width start assertion")
    {
        std::vector<std::string_view> caps;
        // "abc^xyz" can never match: after consuming 'abc' we are no longer at str_start
        CHECK(ez::regex_match("abc^xyz", "abcxyz", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("abc^xyz", "abc", &caps) == EZ_REGEX_NO_MATCH);
    }

    // ── Mixed atoms ───────────────────────────────────────────────────────────────

    TEST_CASE("mixed sequence: anchor + class + escape + literal")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^[A-Z]\\d[a-z]$", "A5m", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^[A-Z]\\d[a-z]$", "a5m", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^[A-Z]\\d[a-z]$", "A5M", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^[A-Z]\\d[a-z]$", "A5", &caps) == EZ_REGEX_NO_MATCH);
    }

    // ── Capturing groups ──────────────────────────────────────────────────────────

    TEST_CASE("single group captures the matched span")
    {
        std::vector<std::string_view> caps;
        REQUIRE(ez::regex_match("(abc)", "abc", &caps) == EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 1);
        CHECK(caps[0] == "abc");
    }

    TEST_CASE("group found as substring")
    {
        std::vector<std::string_view> caps;
        REQUIRE(ez::regex_match("(bc)", "abcd", &caps) == EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 1);
        CHECK(caps[0] == "bc");
    }

    TEST_CASE("multiple sequential groups")
    {
        std::vector<std::string_view> caps;
        REQUIRE(ez::regex_match("(a)(b)(c)", "abc", &caps) == EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 3);
        CHECK(caps[0] == "a");
        CHECK(caps[1] == "b");
        CHECK(caps[2] == "c");
    }

    TEST_CASE("group with non-captured prefix and suffix")
    {
        std::vector<std::string_view> caps;
        REQUIRE(ez::regex_match("x(y)z", "xyz", &caps) == EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 1);
        CHECK(caps[0] == "y");
    }

    TEST_CASE("group capturing a wildcard match")
    {
        std::vector<std::string_view> caps;
        REQUIRE(ez::regex_match("(.)", "x", &caps) == EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 1);
        CHECK(caps[0] == "x");
    }

    TEST_CASE("group capturing a class match")
    {
        std::vector<std::string_view> caps;
        REQUIRE(ez::regex_match("([a-z])", "m", &caps) == EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 1);
        CHECK(caps[0] == "m");
    }

    TEST_CASE("group capturing an escape match")
    {
        std::vector<std::string_view> caps;
        REQUIRE(ez::regex_match("(\\d)", "7", &caps) == EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 1);
        CHECK(caps[0] == "7");
    }

    TEST_CASE("empty group captures empty string view")
    {
        std::vector<std::string_view> caps;
        REQUIRE(ez::regex_match("()", "abc", &caps) == EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 1);
        CHECK(caps[0].empty());
    }

    TEST_CASE("group with anchors")
    {
        std::vector<std::string_view> caps;
        REQUIRE(ez::regex_match("^(abc)$", "abc", &caps) == EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 1);
        CHECK(caps[0] == "abc");

        CHECK(ez::regex_match("^(abc)$", "xabc", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(caps.empty());
    }

    TEST_CASE("anchor inside group")
    {
        std::vector<std::string_view> caps;
        REQUIRE(ez::regex_match("(abc$)", "abc", &caps) == EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 1);
        CHECK(caps[0] == "abc");

        CHECK(ez::regex_match("(abc$)", "abcx", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("no match clears captures")
    {
        std::vector<std::string_view> caps;
        caps.emplace_back("stale");
        CHECK(ez::regex_match("(abc)", "xyz", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(caps.empty());
    }

    TEST_CASE("captures are views into the original string, not copies")
    {
        const char str[] = "hello world";
        std::vector<std::string_view> caps;
        REQUIRE(ez::regex_match("(world)", str, &caps) == EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 1);
        CHECK(caps[0] == "world");
        CHECK(caps[0].data() == str + 6); // "world" starts at byte offset 6
    }

    TEST_CASE("exactly EZ_REGEX_MAX_CAPTURES groups all captured")
    {
        std::string pat, str;
        for (int i = 0; i < EZ_REGEX_MAX_CAPTURES; ++i) {
            pat += "(a)";
            str += "a";
        }
        std::vector<std::string_view> caps;
        REQUIRE(ez::regex_match(pat.c_str(), str.c_str(), &caps) == EZ_REGEX_MATCH);
        REQUIRE((int)caps.size() == EZ_REGEX_MAX_CAPTURES);
        for (auto& c : caps)
            CHECK(c == "a");
    }

    TEST_CASE("exceeding EZ_REGEX_MAX_CAPTURES returns EZ_REGEX_ERR_DEPTH")
    {
        std::string pat, str;
        for (int i = 0; i < EZ_REGEX_MAX_CAPTURES + 1; ++i) {
            pat += "(a)";
            str += "a";
        }
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match(pat.c_str(), str.c_str(), &caps) == EZ_REGEX_ERR_DEPTH);
    }

    // ── Unmatched group markers ───────────────────────────────────────────────────

    TEST_CASE("unmatched ')' in pattern returns EZ_REGEX_ERR_PAREN")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("abc)", "abc)", &caps) == EZ_REGEX_ERR_PAREN);
        CHECK(ez::regex_match(")", ")", &caps) == EZ_REGEX_ERR_PAREN);
    }

} // TEST_SUITE("core_matcher")

// ─────────────────────────────────────────────────────────────────────────────
// Step 5: atom_advance, parse_braces, quantifiers
// ─────────────────────────────────────────────────────────────────────────────

TEST_SUITE("atom_advance")
{

    TEST_CASE("literal char consumes 1 byte")
    {
        CHECK(_test_atom_advance("a") == 1);
        CHECK(_test_atom_advance("0") == 1);
        CHECK(_test_atom_advance(".") == 1);
        CHECK(_test_atom_advance("*") == 1); // unquantified * is just a char here
    }

    TEST_CASE("escape sequence consumes 2 bytes")
    {
        CHECK(_test_atom_advance("\\d") == 2);
        CHECK(_test_atom_advance("\\w") == 2);
        CHECK(_test_atom_advance("\\s") == 2);
        CHECK(_test_atom_advance("\\.") == 2);
        CHECK(_test_atom_advance("\\\\") == 2);
    }

    TEST_CASE("bracket class consumes full '[...]' width")
    {
        CHECK(_test_atom_advance("[a]") == 3);
        CHECK(_test_atom_advance("[abc]") == 5);
        CHECK(_test_atom_advance("[a-z]") == 5);
        CHECK(_test_atom_advance("[^\\d]") == 5);
        CHECK(_test_atom_advance("[a-zA-Z]") == 8);
    }

} // TEST_SUITE("atom_advance")

TEST_SUITE("parse_braces")
{

    TEST_CASE("{n} — exactly n repetitions")
    {
        int lo, hi, qadv;
        CHECK(_test_parse_braces("{3}", lo, hi, qadv));
        CHECK(lo == 3);
        CHECK(hi == 3);
        CHECK(qadv == 3);
        CHECK(_test_parse_braces("{0}", lo, hi, qadv));
        CHECK(lo == 0);
        CHECK(hi == 0);
        CHECK(qadv == 3);
        CHECK(_test_parse_braces("{1}", lo, hi, qadv));
        CHECK(lo == 1);
        CHECK(hi == 1);
        CHECK(qadv == 3);
    }

    TEST_CASE("{n} multi-digit")
    {
        int lo, hi, qadv;
        CHECK(_test_parse_braces("{12}", lo, hi, qadv));
        CHECK(lo == 12);
        CHECK(hi == 12);
        CHECK(qadv == 4);
    }

    TEST_CASE("{n,} — at least n")
    {
        int lo, hi, qadv;
        CHECK(_test_parse_braces("{3,}", lo, hi, qadv));
        CHECK(lo == 3);
        CHECK(hi == INT_MAX);
        CHECK(qadv == 4);
        CHECK(_test_parse_braces("{0,}", lo, hi, qadv));
        CHECK(lo == 0);
        CHECK(hi == INT_MAX);
        CHECK(qadv == 4);
        CHECK(_test_parse_braces("{1,}", lo, hi, qadv));
        CHECK(lo == 1);
        CHECK(hi == INT_MAX);
        CHECK(qadv == 4);
    }

    TEST_CASE("{n,m} — between n and m")
    {
        int lo, hi, qadv;
        CHECK(_test_parse_braces("{3,5}", lo, hi, qadv));
        CHECK(lo == 3);
        CHECK(hi == 5);
        CHECK(qadv == 5);
        CHECK(_test_parse_braces("{0,1}", lo, hi, qadv));
        CHECK(lo == 0);
        CHECK(hi == 1);
        CHECK(qadv == 5);
        CHECK(_test_parse_braces("{3,3}", lo, hi, qadv));
        CHECK(lo == 3);
        CHECK(hi == 3);
        CHECK(qadv == 5); // lo==hi
        CHECK(_test_parse_braces("{10,20}", lo, hi, qadv));
        CHECK(lo == 10);
        CHECK(hi == 20);
        CHECK(qadv == 7);
    }

    TEST_CASE("malformed: no digit after '{'")
    {
        int lo, hi, qadv;
        CHECK_FALSE(_test_parse_braces("{abc}", lo, hi, qadv));
        CHECK_FALSE(_test_parse_braces("{,5}", lo, hi, qadv));
        CHECK_FALSE(_test_parse_braces("{}", lo, hi, qadv));
    }

    TEST_CASE("malformed: hi < lo")
    {
        int lo, hi, qadv;
        CHECK_FALSE(_test_parse_braces("{5,3}", lo, hi, qadv));
        CHECK_FALSE(_test_parse_braces("{5,4}", lo, hi, qadv));
    }

    TEST_CASE("malformed: non-digit after comma")
    {
        int lo, hi, qadv;
        CHECK_FALSE(_test_parse_braces("{3,abc}", lo, hi, qadv));
    }

    TEST_CASE("valid {n,} with trailing chars")
    {
        int lo, hi, qadv;
        CHECK(_test_parse_braces("{3,}x", lo, hi, qadv));
        CHECK(lo == 3);
        CHECK(hi == INT_MAX);
        CHECK(qadv == 4);
    }

    TEST_CASE("malformed: unclosed braces")
    {
        int lo, hi, qadv;
        CHECK_FALSE(_test_parse_braces("{3", lo, hi, qadv));
        CHECK_FALSE(_test_parse_braces("{3,", lo, hi, qadv));
        CHECK_FALSE(_test_parse_braces("{3,5", lo, hi, qadv));
    }

    TEST_CASE("malformed: garbage after lo digits")
    {
        int lo, hi, qadv;
        CHECK_FALSE(_test_parse_braces("{3x}", lo, hi, qadv));   // not ',' or '}'
        CHECK_FALSE(_test_parse_braces("{3,5x}", lo, hi, qadv)); // not '}'
    }

    // ── Overflow guard ────────────────────────────────────────────────────────────
    // Values that would overflow int must be rejected rather than produce UB.

    TEST_CASE("overflow guard: n exceeding INT_MAX is rejected")
    {
        int lo, hi, qadv;
        CHECK_FALSE(_test_parse_braces("{2147483648}", lo, hi, qadv));  // INT_MAX + 1
        CHECK_FALSE(_test_parse_braces("{9999999999}", lo, hi, qadv));  // far above INT_MAX
        CHECK_FALSE(_test_parse_braces("{21474836470}", lo, hi, qadv)); // 10× INT_MAX
    }

    TEST_CASE("overflow guard: hi exceeding INT_MAX is rejected")
    {
        int lo, hi, qadv;
        CHECK_FALSE(_test_parse_braces("{3,2147483648}", lo, hi, qadv)); // hi = INT_MAX + 1
        CHECK_FALSE(_test_parse_braces("{1,9999999999}", lo, hi, qadv)); // hi far above INT_MAX
    }

    TEST_CASE("overflow guard: INT_MAX itself is accepted")
    {
        int lo, hi, qadv;
        // 2147483647 == INT_MAX; must not trip the overflow guard.
        CHECK(_test_parse_braces("{2147483647}", lo, hi, qadv));
        CHECK(lo == INT_MAX);
        CHECK(hi == INT_MAX);
        CHECK(qadv == 12); // '{' + 10 digits + '}' = 12 bytes
    }

    TEST_CASE("overflow guard: hi == INT_MAX is accepted")
    {
        int lo, hi, qadv;
        CHECK(_test_parse_braces("{1,2147483647}", lo, hi, qadv));
        CHECK(lo == 1);
        CHECK(hi == INT_MAX);
        CHECK(qadv == 14); // '{' + '1' + ',' + 10 digits + '}' = 14 bytes
    }

} // TEST_SUITE("parse_braces")

TEST_SUITE("quantifiers")
{

    // ── '?' — zero or one ────────────────────────────────────────────────────────

    TEST_CASE("? matches one or zero occurrences")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("colou?r", "colour", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("colou?r", "color", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("colou?r", "colouur", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("? on anchored pattern")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^a?$", "", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^a?$", "a", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^a?$", "aa", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("? with escape atom")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^\\d?$", "", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^\\d?$", "5", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^\\d?$", "a", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^\\d?$", "55", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("? with class atom")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^[abc]?$", "", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^[abc]?$", "a", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^[abc]?$", "d", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^[abc]?$", "aa", &caps) == EZ_REGEX_NO_MATCH);
    }

    // ── '*' — zero or more ───────────────────────────────────────────────────────

    TEST_CASE("* matches zero or more occurrences")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^a*$", "", &caps) == EZ_REGEX_MATCH); // zero
        CHECK(ez::regex_match("^a*$", "a", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^a*$", "aaa", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^a*$", "b", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^a*$", "ab", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("* matches zero times when atom doesn't match at all")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("a*b", "b", &caps) == EZ_REGEX_MATCH); // zero a's
        CHECK(ez::regex_match("a*b", "aab", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("a*b", "aaa", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("* with escape atom")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^\\d*$", "", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^\\d*$", "123", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^\\d*$", "12a", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("* with class atom")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^[a-z]*$", "", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^[a-z]*$", "hello", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^[a-z]*$", "Hello", &caps) == EZ_REGEX_NO_MATCH);
    }

    // ── '+' — one or more ────────────────────────────────────────────────────────

    TEST_CASE("+ requires at least one occurrence")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^a+$", "", &caps) == EZ_REGEX_NO_MATCH); // zero fails
        CHECK(ez::regex_match("^a+$", "a", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^a+$", "aaa", &caps) == EZ_REGEX_MATCH);
    }

    TEST_CASE("+ fails when atom never matches")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("a+b", "b", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("a+b", "ab", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("a+b", "aaab", &caps) == EZ_REGEX_MATCH);
    }

    TEST_CASE("+ with escape atom")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^\\d+$", "", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^\\d+$", "5", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^\\d+$", "123", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^\\w+$", "hello_42", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^\\w+$", "hello 42", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("+ with class atom")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^[a-z]+$", "hello", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^[a-z]+$", "", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^[a-z]+$", "Hello", &caps) == EZ_REGEX_NO_MATCH);
    }

    // ── '{n}' — exactly n ────────────────────────────────────────────────────────

    TEST_CASE("{n}: exactly n matches")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^a{3}$", "aa", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^a{3}$", "aaa", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^a{3}$", "aaaa", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("{0}: zero matches (atom skipped entirely)")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^a{0}b$", "b", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^a{0}b$", "ab", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("{1}: exactly one match — same as no quantifier")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^a{1}$", "a", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^a{1}$", "", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^a{1}$", "aa", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("{n} with class atom")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^[0-9]{3}$", "123", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^[0-9]{3}$", "12", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^[0-9]{3}$", "1234", &caps) == EZ_REGEX_NO_MATCH);
    }

    // ── '{n,}' — at least n ──────────────────────────────────────────────────────

    TEST_CASE("{n,}: at least n matches")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^a{2,}$", "a", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^a{2,}$", "aa", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^a{2,}$", "aaaa", &caps) == EZ_REGEX_MATCH);
    }

    TEST_CASE("{0,}: equivalent to *")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^a{0,}$", "", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^a{0,}$", "aaa", &caps) == EZ_REGEX_MATCH);
    }

    TEST_CASE("{1,}: equivalent to +")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^a{1,}$", "", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^a{1,}$", "a", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^a{1,}$", "aaa", &caps) == EZ_REGEX_MATCH);
    }

    // ── '{n,m}' — between n and m ────────────────────────────────────────────────

    TEST_CASE("{n,m}: matches lo..hi times")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^a{2,4}$", "a", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^a{2,4}$", "aa", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^a{2,4}$", "aaa", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^a{2,4}$", "aaaa", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^a{2,4}$", "aaaaa", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("{n,n}: exactly n (degenerate range)")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^a{3,3}$", "aaa", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^a{3,3}$", "aa", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^a{3,3}$", "aaaa", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("{n,m} with class atom")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^[a-z]{2,4}$", "ab", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^[a-z]{2,4}$", "abcd", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^[a-z]{2,4}$", "a", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^[a-z]{2,4}$", "abcde", &caps) == EZ_REGEX_NO_MATCH);
    }

    // ── malformed braces treated as literal chars ─────────────────────────────────

    TEST_CASE("unclosed '{' is literal")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^a\\{3\\}$", "a{3}", &caps) == EZ_REGEX_MATCH); // escaped
        CHECK(ez::regex_match("a{3", "a{3", &caps) == EZ_REGEX_MATCH); // unclosed → literals
        CHECK(ez::regex_match("a{3", "aaa", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("'{n,m}' with hi<lo is literal")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("a{5,3}", "a{5,3}", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("a{5,3}", "aaaaa", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("overflowing {n} is treated as literal braces, not UB")
    {
        std::vector<std::string_view> caps;
        // parse_braces returns false on overflow → {n} becomes a literal '{'.
        // So \d{9999999999} = match one \d, then literal "{9999999999}".
        CHECK(ez::regex_match("^\\d{9999999999}$", "5{9999999999}", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^\\d{9999999999}$", "55", &caps) == EZ_REGEX_NO_MATCH);
        // Same for overflow in hi.
        CHECK(ez::regex_match("^a{1,9999999999}$", "a{1,9999999999}", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^a{1,9999999999}$", "aaa", &caps) == EZ_REGEX_NO_MATCH);
    }

    // ── quantifiers with capturing groups ─────────────────────────────────────────

    TEST_CASE("group capturing a + match")
    {
        std::vector<std::string_view> caps;
        REQUIRE(ez::regex_match("(\\w+)", "hello", &caps) == EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 1);
        CHECK(caps[0] == "hello");
    }

    TEST_CASE("group capturing a * match (zero)")
    {
        std::vector<std::string_view> caps;
        REQUIRE(ez::regex_match("(a*)", "", &caps) == EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 1);
        CHECK(caps[0].empty());
    }

    TEST_CASE("group capturing a * match (several)")
    {
        std::vector<std::string_view> caps;
        REQUIRE(ez::regex_match("^(a*)$", "aaa", &caps) == EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 1);
        CHECK(caps[0] == "aaa");
    }

    TEST_CASE("group capturing a ? match")
    {
        std::vector<std::string_view> caps;
        REQUIRE(ez::regex_match("^(a?)$", "a", &caps) == EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 1);
        CHECK(caps[0] == "a");

        REQUIRE(ez::regex_match("^(a?)$", "", &caps) == EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 1);
        CHECK(caps[0].empty());
    }

    TEST_CASE("group capturing a {n,m} match")
    {
        std::vector<std::string_view> caps;
        REQUIRE(ez::regex_match("^(a{2,3})$", "aa", &caps) == EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 1);
        CHECK(caps[0] == "aa");

        REQUIRE(ez::regex_match("^(a{2,3})$", "aaa", &caps) == EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 1);
        CHECK(caps[0] == "aaa");
    }

    TEST_CASE("digit run captured as substring")
    {
        std::vector<std::string_view> caps;
        REQUIRE(ez::regex_match("(\\d+)", "abc123def", &caps) == EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 1);
        CHECK(caps[0] == "123");
    }

    TEST_CASE("two groups with quantifiers")
    {
        std::vector<std::string_view> caps;
        REQUIRE(ez::regex_match("^([a-z]+)\\s(\\d+)$", "hello 42", &caps) == EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 2);
        CHECK(caps[0] == "hello");
        CHECK(caps[1] == "42");
    }

    // ── greedy backtracking ───────────────────────────────────────────────────────

    TEST_CASE("greedy .+ backtracks to let suffix match")
    {
        std::vector<std::string_view> caps;
        // (.+)foo: .+ greedy → tries to consume all, backs off until "foo" fits
        REQUIRE(ez::regex_match("(.+)foo", "barbarfoofoo", &caps) == EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 1);
        CHECK(caps[0] == "barbarfoo"); // greedy: longest prefix that still allows "foo"
    }

    TEST_CASE("greedy .* captures entire string")
    {
        std::vector<std::string_view> caps;
        REQUIRE(ez::regex_match("^(.*)$", "hello", &caps) == EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 1);
        CHECK(caps[0] == "hello");
    }

    TEST_CASE("greedy .* on empty string captures empty")
    {
        std::vector<std::string_view> caps;
        REQUIRE(ez::regex_match("^(.*)$", "", &caps) == EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 1);
        CHECK(caps[0].empty());
    }

    TEST_CASE("greedy backtrack restores capture state correctly")
    {
        // (a+) before a fixed suffix: capture should reflect the final matched length
        std::vector<std::string_view> caps;
        REQUIRE(ez::regex_match("^(a+)b$", "aaab", &caps) == EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 1);
        CHECK(caps[0] == "aaa"); // greedy: all 'a's, then 'b' matches
    }

} // TEST_SUITE("quantifiers")

// ─────────────────────────────────────────────────────────────────────────────
// Step 6: error detection — validate_pattern + ez::regex_match error returns
// ─────────────────────────────────────────────────────────────────────────────

#include <climits>

TEST_SUITE("validate_pattern")
{

    // ── Valid patterns ────────────────────────────────────────────────────────────

    TEST_CASE("valid: plain literals")
    {
        CHECK(_test_validate_pattern("hello") == EZ_REGEX_MATCH);
        CHECK(_test_validate_pattern("") == EZ_REGEX_MATCH);
        CHECK(_test_validate_pattern("a") == EZ_REGEX_MATCH);
    }

    TEST_CASE("valid: wildcard and anchors")
    {
        CHECK(_test_validate_pattern(".") == EZ_REGEX_MATCH);
        CHECK(_test_validate_pattern("^$") == EZ_REGEX_MATCH);
        CHECK(_test_validate_pattern("^.+$") == EZ_REGEX_MATCH);
    }

    TEST_CASE("valid: quantifiers on literals")
    {
        CHECK(_test_validate_pattern("a*") == EZ_REGEX_MATCH);
        CHECK(_test_validate_pattern("a+") == EZ_REGEX_MATCH);
        CHECK(_test_validate_pattern("a?") == EZ_REGEX_MATCH);
        CHECK(_test_validate_pattern("a{3}") == EZ_REGEX_MATCH);
        CHECK(_test_validate_pattern("a{3,}") == EZ_REGEX_MATCH);
        CHECK(_test_validate_pattern("a{2,5}") == EZ_REGEX_MATCH);
    }

    TEST_CASE("valid: recognized escapes")
    {
        CHECK(_test_validate_pattern("\\d") == EZ_REGEX_MATCH);
        CHECK(_test_validate_pattern("\\D") == EZ_REGEX_MATCH);
        CHECK(_test_validate_pattern("\\w") == EZ_REGEX_MATCH);
        CHECK(_test_validate_pattern("\\W") == EZ_REGEX_MATCH);
        CHECK(_test_validate_pattern("\\s") == EZ_REGEX_MATCH);
        CHECK(_test_validate_pattern("\\S") == EZ_REGEX_MATCH);
        CHECK(_test_validate_pattern("\\t") == EZ_REGEX_MATCH);
        CHECK(_test_validate_pattern("\\n") == EZ_REGEX_MATCH);
        CHECK(_test_validate_pattern("\\r") == EZ_REGEX_MATCH);
        CHECK(_test_validate_pattern("\\b") == EZ_REGEX_MATCH);
        CHECK(_test_validate_pattern("\\B") == EZ_REGEX_MATCH);
        CHECK(_test_validate_pattern("\\.") == EZ_REGEX_MATCH);
        CHECK(_test_validate_pattern("\\(") == EZ_REGEX_MATCH);
        CHECK(_test_validate_pattern("\\)") == EZ_REGEX_MATCH);
        CHECK(_test_validate_pattern("\\[") == EZ_REGEX_MATCH);
        CHECK(_test_validate_pattern("\\]") == EZ_REGEX_MATCH);
        CHECK(_test_validate_pattern("\\*") == EZ_REGEX_MATCH);
        CHECK(_test_validate_pattern("\\+") == EZ_REGEX_MATCH);
        CHECK(_test_validate_pattern("\\?") == EZ_REGEX_MATCH);
        CHECK(_test_validate_pattern("\\{") == EZ_REGEX_MATCH);
        CHECK(_test_validate_pattern("\\}") == EZ_REGEX_MATCH);
        CHECK(_test_validate_pattern("\\\\") == EZ_REGEX_MATCH);
    }

    TEST_CASE("valid: balanced capture groups")
    {
        CHECK(_test_validate_pattern("(a)") == EZ_REGEX_MATCH);
        CHECK(_test_validate_pattern("(a)(b)") == EZ_REGEX_MATCH);
        CHECK(_test_validate_pattern("(a+)b(c)") == EZ_REGEX_MATCH);
    }

    TEST_CASE("valid: bracket expressions")
    {
        CHECK(_test_validate_pattern("[abc]") == EZ_REGEX_MATCH);
        CHECK(_test_validate_pattern("[^abc]") == EZ_REGEX_MATCH);
        CHECK(_test_validate_pattern("[a-z]") == EZ_REGEX_MATCH);
        CHECK(_test_validate_pattern("[]]") == EZ_REGEX_MATCH);  // ']' as first char
        CHECK(_test_validate_pattern("[^]]") == EZ_REGEX_MATCH); // ']' as first char after '^'
        CHECK(_test_validate_pattern("[\\d]") == EZ_REGEX_MATCH);
        CHECK(_test_validate_pattern("[\\w+]") == EZ_REGEX_MATCH);
    }

    // ── Syntax errors ─────────────────────────────────────────────────────────────

    TEST_CASE("error: trailing backslash returns EZ_REGEX_ERR_ESCAPE")
    {
        CHECK(_test_validate_pattern("a\\") == EZ_REGEX_ERR_ESCAPE);
        CHECK(_test_validate_pattern("\\") == EZ_REGEX_ERR_ESCAPE);
    }

    TEST_CASE("error: unknown escape returns EZ_REGEX_ERR_ESCAPE")
    {
        CHECK(_test_validate_pattern("\\a") == EZ_REGEX_ERR_ESCAPE);
        CHECK(_test_validate_pattern("\\1") == EZ_REGEX_ERR_ESCAPE);
        CHECK(_test_validate_pattern("\\x") == EZ_REGEX_ERR_ESCAPE);
        CHECK(_test_validate_pattern("\\z") == EZ_REGEX_ERR_ESCAPE);
    }

    TEST_CASE("error: unmatched open paren returns EZ_REGEX_ERR_PAREN")
    {
        CHECK(_test_validate_pattern("(a") == EZ_REGEX_ERR_PAREN);
        CHECK(_test_validate_pattern("a(b") == EZ_REGEX_ERR_PAREN);
        CHECK(_test_validate_pattern("(a)(b") == EZ_REGEX_ERR_PAREN);
    }

    TEST_CASE("error: unmatched close paren returns EZ_REGEX_ERR_PAREN")
    {
        CHECK(_test_validate_pattern(")") == EZ_REGEX_ERR_PAREN);
        CHECK(_test_validate_pattern("a)") == EZ_REGEX_ERR_PAREN);
        CHECK(_test_validate_pattern("(a))") == EZ_REGEX_ERR_PAREN);
    }

    TEST_CASE("error: nested groups returns EZ_REGEX_ERR_NESTING")
    {
        CHECK(_test_validate_pattern("(a(b)c)") == EZ_REGEX_ERR_NESTING); // group inside group
        CHECK(_test_validate_pattern("((a))") == EZ_REGEX_ERR_NESTING);   // double-wrapped
        CHECK(_test_validate_pattern("(a)(b(c))") ==
              EZ_REGEX_ERR_NESTING);                                 // nested in second group
        CHECK(_test_validate_pattern("((") == EZ_REGEX_ERR_NESTING); // nested + unclosed
    }

    TEST_CASE("error: unclosed bracket returns EZ_REGEX_ERR_BRACKET")
    {
        CHECK(_test_validate_pattern("[abc") == EZ_REGEX_ERR_BRACKET);
        CHECK(_test_validate_pattern("[^abc") == EZ_REGEX_ERR_BRACKET);
        CHECK(_test_validate_pattern("[") == EZ_REGEX_ERR_BRACKET);
        CHECK(_test_validate_pattern("[^") == EZ_REGEX_ERR_BRACKET);
    }

    TEST_CASE("error: backslash at end of bracket content returns EZ_REGEX_ERR_BRACKET")
    {
        CHECK(_test_validate_pattern("[a\\") == EZ_REGEX_ERR_BRACKET);
    }

    // ── Depth limit ───────────────────────────────────────────────────────────────

    TEST_CASE("error: too many capture groups returns EZ_REGEX_ERR_DEPTH")
    {
        // Build a pattern with EZ_REGEX_MAX_CAPTURES+1 groups: "(a)(a)...(a)"
        std::string pat;
        for (int i = 0; i <= EZ_REGEX_MAX_CAPTURES; ++i)
            pat += "(a)";
        CHECK(_test_validate_pattern(pat.c_str()) == EZ_REGEX_ERR_DEPTH);
    }

    TEST_CASE("valid: exactly EZ_REGEX_MAX_CAPTURES groups is allowed")
    {
        std::string pat;
        for (int i = 0; i < EZ_REGEX_MAX_CAPTURES; ++i)
            pat += "(a)";
        CHECK(_test_validate_pattern(pat.c_str()) == EZ_REGEX_MATCH);
    }

} // TEST_SUITE("validate_pattern")

// ─────────────────────────────────────────────────────────────────────────────
// Step 6: ez::regex_match propagates error codes
// ─────────────────────────────────────────────────────────────────────────────

TEST_SUITE("ez::regex_match error propagation")
{

    TEST_CASE("unclosed paren returns EZ_REGEX_ERR_PAREN, captures cleared")
    {
        std::vector<std::string_view> caps;
        caps.emplace_back("stale");

        int r = ez::regex_match("(unclosed", "anything", &caps);
        CHECK(r == EZ_REGEX_ERR_PAREN);
        CHECK(caps.empty()); // captures always cleared on entry
    }

    TEST_CASE("trailing backslash returns EZ_REGEX_ERR_ESCAPE")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("a\\", "a", &caps) == EZ_REGEX_ERR_ESCAPE);
        CHECK(caps.empty());
    }

    TEST_CASE("unknown escape returns EZ_REGEX_ERR_ESCAPE")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("\\a", "a", &caps) == EZ_REGEX_ERR_ESCAPE);
        CHECK(caps.empty());
    }

    TEST_CASE("returns EZ_REGEX_ERR_DEPTH for too many groups")
    {
        std::string pat;
        for (int i = 0; i <= EZ_REGEX_MAX_CAPTURES; ++i)
            pat += "(a)";
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match(pat.c_str(), "a", &caps) == EZ_REGEX_ERR_DEPTH);
        CHECK(caps.empty());
    }

    TEST_CASE("unmatched close paren returns EZ_REGEX_ERR_PAREN")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("a)", "a", &caps) == EZ_REGEX_ERR_PAREN);
    }

    TEST_CASE("nested groups return EZ_REGEX_ERR_NESTING")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("(a(b)c)", "abc", &caps) == EZ_REGEX_ERR_NESTING);
        CHECK(ez::regex_match("((a))", "a", &caps) == EZ_REGEX_ERR_NESTING);
        CHECK(ez::regex_match("(a)(b(c))", "abc", &caps) == EZ_REGEX_ERR_NESTING);
        CHECK(caps.empty()); // captures always cleared on entry
    }

    TEST_CASE("unclosed bracket returns EZ_REGEX_ERR_BRACKET")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("[abc", "a", &caps) == EZ_REGEX_ERR_BRACKET);
    }

    TEST_CASE("valid pattern still matches correctly after validation passes")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("(\\d+)", "abc123def", &caps) == EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 1);
        CHECK(caps[0] == "123");
    }

} // TEST_SUITE("ez::regex_match error propagation")

// ─────────────────────────────────────────────────────────────────────────────
// Step 7: cross-feature integration
// ─────────────────────────────────────────────────────────────────────────────

TEST_SUITE("integration")
{

    // ── Substring search semantics ────────────────────────────────────────────────

    TEST_CASE("substring search returns leftmost match")
    {
        // Both "aa" and "aaa" are in "xaaabaa"; leftmost match starts at offset 1
        const char* str = "xaaabaa";
        std::vector<std::string_view> caps;
        REQUIRE(ez::regex_match("(a+)", str, &caps) == EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 1);
        CHECK(caps[0] == "aaa");          // greedy, starting from leftmost position
        CHECK(caps[0].data() == str + 1); // pointer confirms offset 1
    }

    TEST_CASE("suffix anchor finds match at end of longer string")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("foo$", "xyzfoo", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("\\d+$", "abc123", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("foo$", "foobar", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("caret anchor defeats substring search")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^foo", "xfoo", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^foo", "foo", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^foo", "foobar", &caps) == EZ_REGEX_MATCH);
    }

    TEST_CASE("both anchors require full string match")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^foo$", "foo", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^foo$", "xfoo", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^foo$", "foox", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^foo$", "xfoox", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("capture pointer points into original string at correct offset")
    {
        const char* str = "hello 42 world";
        std::vector<std::string_view> caps;
        REQUIRE(ez::regex_match("(\\d+)", str, &caps) == EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 1);
        CHECK(caps[0] == "42");
        CHECK(caps[0].data() == str + 6); // "42" starts at index 6
    }

    // ── Empty string edge cases ───────────────────────────────────────────────────

    TEST_CASE("^$ matches empty string only")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^$", "", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^$", "a", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("^.+$ does not match empty string")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^.+$", "", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^.+$", "x", &caps) == EZ_REGEX_MATCH);
    }

    TEST_CASE("^.$  matches exactly one character")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^.$", "a", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^.$", "", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^.$", "ab", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("a* on empty string matches (zero repetitions)")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^a*$", "", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^a*$", "aaa", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^a*$", "b", &caps) == EZ_REGEX_NO_MATCH);
    }

    // ── Zero-lower-bound quantifiers ─────────────────────────────────────────────

    TEST_CASE("{0,n} allows zero repetitions")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^a{0,3}b$", "b", &caps) == EZ_REGEX_MATCH);        // 0 a's
        CHECK(ez::regex_match("^a{0,3}b$", "ab", &caps) == EZ_REGEX_MATCH);       // 1
        CHECK(ez::regex_match("^a{0,3}b$", "aaab", &caps) == EZ_REGEX_MATCH);     // 3
        CHECK(ez::regex_match("^a{0,3}b$", "aaaab", &caps) == EZ_REGEX_NO_MATCH); // 4 — too many
    }

    TEST_CASE("a? before required suffix")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^a?b$", "b", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^a?b$", "ab", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^a?b$", "aab", &caps) == EZ_REGEX_NO_MATCH);
    }

    // ── Class + quantifier combinations ──────────────────────────────────────────

    TEST_CASE("[a-z]+ matches a lowercase word")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^[a-z]+$", "hello", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^[a-z]+$", "Hello", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^[a-z]+$", "", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("[0-9]{4} matches exactly four digits")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^[0-9]{4}$", "1234", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^[0-9]{4}$", "123", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^[0-9]{4}$", "12345", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("[^\\s]+ extracts a non-whitespace token")
    {
        std::vector<std::string_view> caps;
        REQUIRE(ez::regex_match("([^\\s]+)", "  hello  ", &caps) == EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 1);
        CHECK(caps[0] == "hello");
    }

    TEST_CASE("[^0-9]+ prefix before first digit")
    {
        std::vector<std::string_view> caps;
        REQUIRE(ez::regex_match("^([^0-9]+)", "abc123", &caps) == EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 1);
        CHECK(caps[0] == "abc");
    }

    TEST_CASE("[\\d]+ is equivalent to \\d+")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^[\\d]+$", "42", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^[\\d]+$", "abc", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("[A-Z][a-z]+ matches a capitalized word")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^[A-Z][a-z]+$", "Hello", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^[A-Z][a-z]+$", "hello", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^[A-Z][a-z]+$", "HELLO", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^[A-Z][a-z]+$", "H", &caps) == EZ_REGEX_NO_MATCH); // no lower chars
    }

    TEST_CASE("[0-9A-Fa-f]+ matches hex string")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^[0-9A-Fa-f]+$", "deadBEEF", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^[0-9A-Fa-f]+$", "xyz", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^[0-9A-Fa-f]+$", "g", &caps) == EZ_REGEX_NO_MATCH);
    }

    // ── Literal escapes in realistic patterns ────────────────────────────────────

    TEST_CASE("literal parens: \\(\\d+\\) matches parenthesized number")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^\\(\\d+\\)$", "(42)", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^\\(\\d+\\)$", "42", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^\\(\\d+\\)$", "(42", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("literal dot: \\d+\\.\\d+ matches a decimal number")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^\\d+\\.\\d+$", "3.14", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^\\d+\\.\\d+$", "3x14", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^\\d+\\.\\d+$", "314", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("literal quantifier chars: \\* \\+ \\?")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^a\\*b$", "a*b", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^a\\+b$", "a+b", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^a\\?b$", "a?b", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^a\\*b$", "aab", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("literal backslash: a\\\\b matches a\\b")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^a\\\\b$", "a\\b", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^a\\\\b$", "ab", &caps) == EZ_REGEX_NO_MATCH);
    }

    // ── Realistic multi-feature patterns ─────────────────────────────────────────

    TEST_CASE("ISO date: ^\\d{4}-\\d{2}-\\d{2}$")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^\\d{4}-\\d{2}-\\d{2}$", "2024-01-15", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^\\d{4}-\\d{2}-\\d{2}$", "24-01-15", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^\\d{4}-\\d{2}-\\d{2}$", "2024-1-15", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^\\d{4}-\\d{2}-\\d{2}$", "2024/01/15", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("ISO date with captures: year, month, day extracted")
    {
        std::vector<std::string_view> caps;
        REQUIRE(ez::regex_match("^(\\d{4})-(\\d{2})-(\\d{2})$", "2024-01-15", &caps) ==
                EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 3);
        CHECK(caps[0] == "2024");
        CHECK(caps[1] == "01");
        CHECK(caps[2] == "15");
    }

    TEST_CASE("time pattern: \\d{2}:\\d{2}:\\d{2}")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^\\d{2}:\\d{2}:\\d{2}$", "12:34:56", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^\\d{2}:\\d{2}:\\d{2}$", "1:34:56", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^\\d{2}:\\d{2}:\\d{2}$", "12:34", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("key=value extraction")
    {
        std::vector<std::string_view> caps;
        REQUIRE(ez::regex_match("(\\w+)=(\\w+)", "timeout=30", &caps) == EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 2);
        CHECK(caps[0] == "timeout");
        CHECK(caps[1] == "30");
    }

    TEST_CASE("key=value found inside a larger string")
    {
        std::vector<std::string_view> caps;
        REQUIRE(ez::regex_match("(\\w+)=(\\w+)", "config: timeout=30 retries=3", &caps) ==
                EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 2);
        CHECK(caps[0] == "timeout");
        CHECK(caps[1] == "30");
    }

    TEST_CASE("email-like pattern extracts user and domain")
    {
        std::vector<std::string_view> caps;
        REQUIRE(ez::regex_match("^([\\w]+)@([\\w]+)\\.([a-z]+)$", "alice@example.com", &caps) ==
                EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 3);
        CHECK(caps[0] == "alice");
        CHECK(caps[1] == "example");
        CHECK(caps[2] == "com");
    }

    TEST_CASE("decimal number: captured integer and fractional parts")
    {
        std::vector<std::string_view> caps;
        REQUIRE(ez::regex_match("(\\d+)\\.(\\d+)", "price: 19.99 USD", &caps) == EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 2);
        CHECK(caps[0] == "19");
        CHECK(caps[1] == "99");
    }

    TEST_CASE("optional sign before integer")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^[+-]?\\d+$", "42", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^[+-]?\\d+$", "+42", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^[+-]?\\d+$", "-42", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^[+-]?\\d+$", "++42", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^[+-]?\\d+$", "4x", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("whitespace-separated word pair")
    {
        std::vector<std::string_view> caps;
        REQUIRE(ez::regex_match("^(\\w+)\\s+(\\w+)$", "hello world", &caps) == EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 2);
        CHECK(caps[0] == "hello");
        CHECK(caps[1] == "world");
    }

    TEST_CASE("multiple whitespace between tokens")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^\\w+\\s+\\w+$", "hello   world", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^\\w+\\s+\\w+$", "hello world", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^\\w+\\s+\\w+$", "helloworld", &caps) == EZ_REGEX_NO_MATCH);
    }

    // ── Negated class integration ─────────────────────────────────────────────────

    TEST_CASE("^[^a]+$ rejects strings containing 'a'")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^[^a]+$", "bcde", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^[^a]+$", "bcae", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^[^a]+$", "", &caps) == EZ_REGEX_NO_MATCH); // + needs >= 1
    }

    TEST_CASE("[^\\d]+ captures non-digit prefix")
    {
        std::vector<std::string_view> caps;
        REQUIRE(ez::regex_match("^([^\\d]+)", "abc123", &caps) == EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 1);
        CHECK(caps[0] == "abc");
    }

    // ── Escapes inside character classes integration ──────────────────────────────

    TEST_CASE("[\\d\\s]+ matches mixed digits and whitespace")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^[\\d\\s]+$", "1 2 3", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^[\\d\\s]+$", "1a2", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("[\\w-]+ matches identifiers with hyphens")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^[\\w-]+$", "foo-bar", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^[\\w-]+$", "foo_bar", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^[\\w-]+$", "foo bar", &caps) == EZ_REGEX_NO_MATCH);
    }

    // ── Quantifier on complex atoms ───────────────────────────────────────────────

    TEST_CASE("(\\w+) repeated pattern: captures leftmost greedy token")
    {
        // Pattern has no quantifier on the group itself — group captures one + run
        std::vector<std::string_view> caps;
        REQUIRE(ez::regex_match("^(\\w+)\\s(\\w+)\\s(\\w+)$", "one two three", &caps) ==
                EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 3);
        CHECK(caps[0] == "one");
        CHECK(caps[1] == "two");
        CHECK(caps[2] == "three");
    }

    TEST_CASE("[a-z]{2,4} with anchors")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^[a-z]{2,4}$", "ab", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^[a-z]{2,4}$", "abcd", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^[a-z]{2,4}$", "a", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^[a-z]{2,4}$", "abcde", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("\\d{2}:\\d{2} found as substring in timestamp string")
    {
        std::vector<std::string_view> caps;
        REQUIRE(ez::regex_match("(\\d{2}):(\\d{2})", "[12:45] event", &caps) == EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 2);
        CHECK(caps[0] == "12");
        CHECK(caps[1] == "45");
    }

    // ── Greedy vs. required suffix: capture correctness ──────────────────────────

    TEST_CASE("(.+) before literal suffix backtracks to shortest fit")
    {
        std::vector<std::string_view> caps;
        REQUIRE(ez::regex_match("^(.+)b$", "aaab", &caps) == EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 1);
        CHECK(caps[0] == "aaa");
    }

    TEST_CASE("(\\d+) greedy captures longest run before non-digit")
    {
        std::vector<std::string_view> caps;
        REQUIRE(ez::regex_match("(\\d+)x", "12345x", &caps) == EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 1);
        CHECK(caps[0] == "12345");
    }

    TEST_CASE("two adjacent greedy groups: first takes all it can")
    {
        std::vector<std::string_view> caps;
        // (a*)(a+) on "aaa": first group is greedy, leaves minimum 1 'a' for second
        REQUIRE(ez::regex_match("^(a*)(a+)$", "aaa", &caps) == EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 2);
        // greedy: first takes "aa", second takes "a"
        CHECK(caps[0] == "aa");
        CHECK(caps[1] == "a");
    }

    // ── Case sensitivity ──────────────────────────────────────────────────────────

    TEST_CASE("matching is case-sensitive")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^hello$", "hello", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^hello$", "Hello", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^hello$", "HELLO", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("[a-z] does not match uppercase")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^[a-z]+$", "abc", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^[a-z]+$", "Abc", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^[a-z]+$", "ABC", &caps) == EZ_REGEX_NO_MATCH);
    }

} // TEST_SUITE("integration")

// ─────────────────────────────────────────────────────────────────────────────
// Command-line option parsing
// ─────────────────────────────────────────────────────────────────────────────

TEST_SUITE("cmdline options")
{

    // Pattern: ^-([a-zA-Z])$
    // Exactly one letter after a single dash; rejects aggregated, long, and digit flags.
    TEST_CASE("short option: -w")
    {
        std::vector<std::string_view> caps;

        REQUIRE(ez::regex_match("^-([a-zA-Z])$", "-w", &caps) == EZ_REGEX_MATCH);
        CHECK(caps[0] == "w");

        REQUIRE(ez::regex_match("^-([a-zA-Z])$", "-Q", &caps) == EZ_REGEX_MATCH);
        CHECK(caps[0] == "Q");

        CHECK(ez::regex_match("^-([a-zA-Z])$", "-wQl", &caps) == EZ_REGEX_NO_MATCH); // aggregated
        CHECK(ez::regex_match("^-([a-zA-Z])$", "--w", &caps) == EZ_REGEX_NO_MATCH);  // long-style
        CHECK(ez::regex_match("^-([a-zA-Z])$", "-1", &caps) == EZ_REGEX_NO_MATCH);   // digit flag
        CHECK(ez::regex_match("^-([a-zA-Z])$", "w", &caps) == EZ_REGEX_NO_MATCH);    // no dash
        CHECK(ez::regex_match("^-([a-zA-Z])$", "-", &caps) == EZ_REGEX_NO_MATCH);    // bare dash
    }

    // Pattern: ^-([a-zA-Z]+)$
    // One or more letters after a single dash; single -w is a degenerate case of this.
    TEST_CASE("aggregated short options: -wQl")
    {
        std::vector<std::string_view> caps;

        REQUIRE(ez::regex_match("^-([a-zA-Z]+)$", "-w", &caps) == EZ_REGEX_MATCH);
        CHECK(caps[0] == "w");

        REQUIRE(ez::regex_match("^-([a-zA-Z]+)$", "-wQl", &caps) == EZ_REGEX_MATCH);
        CHECK(caps[0] == "wQl");

        REQUIRE(ez::regex_match("^-([a-zA-Z]+)$", "-abc", &caps) == EZ_REGEX_MATCH);
        CHECK(caps[0] == "abc");

        CHECK(ez::regex_match("^-([a-zA-Z]+)$", "-", &caps) == EZ_REGEX_NO_MATCH);    // bare dash
        CHECK(ez::regex_match("^-([a-zA-Z]+)$", "--w", &caps) == EZ_REGEX_NO_MATCH);  // long-style
        CHECK(ez::regex_match("^-([a-zA-Z]+)$", "-123", &caps) == EZ_REGEX_NO_MATCH); // digits
        CHECK(ez::regex_match("^-([a-zA-Z]+)$", "-w=x", &caps) == EZ_REGEX_NO_MATCH); // has value
    }

    // Pattern: ^--([a-zA-Z][a-zA-Z0-9-]*)$
    // Two dashes, a letter start, then letters/digits/hyphens. The trailing '-' in
    // [a-zA-Z0-9-] is a literal because ']' immediately follows it.
    TEST_CASE("long option: --write")
    {
        std::vector<std::string_view> caps;

        REQUIRE(ez::regex_match("^--([a-zA-Z][a-zA-Z0-9-]*)$", "--write", &caps) == EZ_REGEX_MATCH);
        CHECK(caps[0] == "write");

        REQUIRE(ez::regex_match("^--([a-zA-Z][a-zA-Z0-9-]*)$", "--verbose", &caps) ==
                EZ_REGEX_MATCH);
        CHECK(caps[0] == "verbose");

        REQUIRE(ez::regex_match("^--([a-zA-Z][a-zA-Z0-9-]*)$", "--output-file", &caps) ==
                EZ_REGEX_MATCH);
        CHECK(caps[0] == "output-file");

        REQUIRE(ez::regex_match("^--([a-zA-Z][a-zA-Z0-9-]*)$", "--opt123", &caps) ==
                EZ_REGEX_MATCH);
        CHECK(caps[0] == "opt123");

        // single-char long option is valid
        REQUIRE(ez::regex_match("^--([a-zA-Z][a-zA-Z0-9-]*)$", "--v", &caps) == EZ_REGEX_MATCH);
        CHECK(caps[0] == "v");

        CHECK(ez::regex_match("^--([a-zA-Z][a-zA-Z0-9-]*)$", "-write", &caps) ==
              EZ_REGEX_NO_MATCH); // single dash
        CHECK(ez::regex_match("^--([a-zA-Z][a-zA-Z0-9-]*)$", "--", &caps) ==
              EZ_REGEX_NO_MATCH); // no name
        CHECK(ez::regex_match("^--([a-zA-Z][a-zA-Z0-9-]*)$", "--123", &caps) ==
              EZ_REGEX_NO_MATCH); // starts with digit
        CHECK(ez::regex_match("^--([a-zA-Z][a-zA-Z0-9-]*)$", "--write=val", &caps) ==
              EZ_REGEX_NO_MATCH); // has value
        CHECK(ez::regex_match("^--([a-zA-Z][a-zA-Z0-9-]*)$", "write", &caps) ==
              EZ_REGEX_NO_MATCH); // no dashes
    }

    // Pattern: ^--([a-zA-Z][a-zA-Z0-9-]*)=(\S+)$
    // Same name rules as above, then '=', then a non-whitespace value (\S+).
    TEST_CASE("long option with value: --user=name")
    {
        std::vector<std::string_view> caps;

        REQUIRE(ez::regex_match("^--([a-zA-Z][a-zA-Z0-9-]*)=(\\S+)$", "--user=alice", &caps) ==
                EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 2);
        CHECK(caps[0] == "user");
        CHECK(caps[1] == "alice");

        REQUIRE(ez::regex_match("^--([a-zA-Z][a-zA-Z0-9-]*)=(\\S+)$", "--count=42", &caps) ==
                EZ_REGEX_MATCH);
        CHECK(caps[0] == "count");
        CHECK(caps[1] == "42");

        REQUIRE(ez::regex_match("^--([a-zA-Z][a-zA-Z0-9-]*)=(\\S+)$", "--output-file=/tmp/out",
                                &caps) == EZ_REGEX_MATCH);
        CHECK(caps[0] == "output-file");
        CHECK(caps[1] == "/tmp/out");

        REQUIRE(ez::regex_match("^--([a-zA-Z][a-zA-Z0-9-]*)=(\\S+)$", "--level=debug", &caps) ==
                EZ_REGEX_MATCH);
        CHECK(caps[0] == "level");
        CHECK(caps[1] == "debug");

        CHECK(ez::regex_match("^--([a-zA-Z][a-zA-Z0-9-]*)=(\\S+)$", "--user=", &caps) ==
              EZ_REGEX_NO_MATCH); // empty value
        CHECK(ez::regex_match("^--([a-zA-Z][a-zA-Z0-9-]*)=(\\S+)$", "--user alice", &caps) ==
              EZ_REGEX_NO_MATCH); // space not =
        CHECK(ez::regex_match("^--([a-zA-Z][a-zA-Z0-9-]*)=(\\S+)$", "-user=alice", &caps) ==
              EZ_REGEX_NO_MATCH); // single dash
        CHECK(ez::regex_match("^--([a-zA-Z][a-zA-Z0-9-]*)=(\\S+)$", "--=alice", &caps) ==
              EZ_REGEX_NO_MATCH); // missing name
    }

} // TEST_SUITE("cmdline options")

// ─────────────────────────────────────────────────────────────────────────────
// Step 8: literal whitespace escapes \t \n \r
// ─────────────────────────────────────────────────────────────────────────────

TEST_SUITE("whitespace_escapes")
{

    TEST_CASE("\\t matches tab, not other chars")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^\\t$", "\t", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^\\t$", "t", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^\\t$", " ", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^\\t$", "\n", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("\\n matches newline, not other chars")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^\\n$", "\n", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^\\n$", "n", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^\\n$", " ", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^\\n$", "\t", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("\\r matches carriage-return, not other chars")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^\\r$", "\r", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^\\r$", "r", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("^\\r$", "\n", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("\\t\\n inside a pattern between literals")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^a\\tb$", "a\tb", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^a\\nb$", "a\nb", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^a\\rb$", "a\rb", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^a\\tb$", "a b", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("\\t inside a character class")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("^[\\t\\n]+$", "\t\n\t", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^[\\t\\n]+$", "a", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("\\t+ captures a tab run")
    {
        std::vector<std::string_view> caps;
        REQUIRE(ez::regex_match("(\\t+)", "x\t\ty", &caps) == EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 1);
        CHECK(caps[0] == "\t\t");
    }

} // TEST_SUITE("whitespace_escapes")

// ─────────────────────────────────────────────────────────────────────────────
// Step 8: word boundary assertions \b and \B
// ─────────────────────────────────────────────────────────────────────────────

TEST_SUITE("word_boundary")
{

    TEST_CASE("\\b matches at word/non-word transitions")
    {
        std::vector<std::string_view> caps;
        // word boundary before "cat" in "a cat"
        CHECK(ez::regex_match("\\bcat\\b", "a cat b", &caps) == EZ_REGEX_MATCH);
        // whole-word match — "cat" surrounded by spaces
        CHECK(ez::regex_match("\\bcat\\b", "cat", &caps) == EZ_REGEX_MATCH);
        // "cat" at start of string
        CHECK(ez::regex_match("\\bcat\\b", "cat food", &caps) == EZ_REGEX_MATCH);
        // "cat" at end of string
        CHECK(ez::regex_match("\\bcat\\b", "my cat", &caps) == EZ_REGEX_MATCH);
    }

    TEST_CASE("\\b does not match inside a word")
    {
        std::vector<std::string_view> caps;
        // "cat" embedded in "concatenate" — no word boundary around it
        CHECK(ez::regex_match("^\\bcat\\b$", "concatenate", &caps) == EZ_REGEX_NO_MATCH);
        // anchored whole-word: "cats" has no boundary after "cat"
        CHECK(ez::regex_match("\\bcat\\b", "cats", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("\\bcat\\b", "scat", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("\\bcat\\b", "scatter", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("\\b at start of string — boundary when first char is word char")
    {
        std::vector<std::string_view> caps;
        // "^\\b" means: start of string AND a word boundary (first char is \w)
        CHECK(ez::regex_match("^\\bfoo", "foobar", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("^\\bfoo", " foobar", &caps) == EZ_REGEX_NO_MATCH); // ^ fails
    }

    TEST_CASE("\\b at end of string — boundary when last char is word char")
    {
        std::vector<std::string_view> caps;
        CHECK(ez::regex_match("foo\\b$", "foo", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("foo\\b$", "foo ", &caps) == EZ_REGEX_NO_MATCH); // $ fails
    }

    TEST_CASE("\\B matches inside a word, not at boundary")
    {
        std::vector<std::string_view> caps;
        // "ca\\Bt" matches "cat" where there is no boundary between 'a' and 't'
        CHECK(ez::regex_match("ca\\Bt", "cat", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("ca\\Bt", "ca t", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("\\B does not match at a word boundary")
    {
        std::vector<std::string_view> caps;
        // "\\Bcat\\B" requires non-boundary on both sides → must be inside a longer word
        CHECK(ez::regex_match("\\Bcat\\B", "concatenate", &caps) == EZ_REGEX_MATCH);
        CHECK(ez::regex_match("\\Bcat\\B", "cat", &caps) == EZ_REGEX_NO_MATCH);
        CHECK(ez::regex_match("\\Bcat\\B", "a cat b", &caps) == EZ_REGEX_NO_MATCH);
    }

    TEST_CASE("\\b with quantifiers: \\b\\w+\\b extracts whole word")
    {
        std::vector<std::string_view> caps;
        REQUIRE(ez::regex_match("\\b(\\w+)\\b", "hello world", &caps) == EZ_REGEX_MATCH);
        REQUIRE(caps.size() == 1);
        CHECK(caps[0] == "hello");
    }

    TEST_CASE("\\b on non-word-only string: no match")
    {
        std::vector<std::string_view> caps;
        // String is all spaces — no word chars, so no word boundaries
        CHECK(ez::regex_match("\\b", "   ", &caps) == EZ_REGEX_NO_MATCH);
    }

} // TEST_SUITE("word_boundary")
