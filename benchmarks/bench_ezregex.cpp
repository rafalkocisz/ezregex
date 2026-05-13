#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include "ezregex.h"

#include <regex>
#include <string>
#include <vector>
#include <string_view>
#include <cstdint>

namespace nb = ankerl::nanobench;

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::string repeat(char c, size_t n)
{
    return std::string(n, c);
}

// Run one three-way benchmark group:
//   - ezregex one-shot  (validate_pattern + match every call, no pre-compilation)
//   - std::regex hot    (regex pre-compiled once outside the loop)
//   - std::regex cold   (regex constructed inside the loop, models one-shot usage)
//
// min_iters: tune per scenario — short patterns need ~25 000 to suppress MSVC
// std::regex heap-allocation jitter; long-string/no-match scenarios are already
// stable at 500–2 000.
template <class StdMatchFn>
static void bench_group(const char*        title,
                        const char*        pat,
                        const std::string& input,
                        StdMatchFn         std_match,
                        uint64_t           min_iters = 25000)
{
    std::vector<std::string_view> caps;
    std::smatch                   m;
    const std::regex              re_hot(pat);   // pre-compiled once

    nb::Bench()
        .title(title)
        .unit("match")
        .relative(true)           // first entry = 1.00x; others shown as multiples
        .warmup(200)
        .minEpochIterations(min_iters)
        .run("ezregex   one-shot", [&] {
            nb::doNotOptimizeAway(ezregex_match(pat, input.c_str(), &caps));
        })
        .run("std::regex hot    ", [&] {
            nb::doNotOptimizeAway(std_match(re_hot, m, input));
        })
        .run("std::regex cold   ", [&] {
            // Construct every iteration: cost paid when the regex object
            // cannot be cached (e.g. dynamic or one-shot patterns).
            std::regex re(pat);
            nb::doNotOptimizeAway(std_match(re, m, input));
        });
}

// ── main ──────────────────────────────────────────────────────────────────────

int main()
{
    // ── Input strings ─────────────────────────────────────────────────────────
    const std::string s_date    = "2024-01-15";
    const std::string s_kv      = "timeout=30";
    const std::string s_digits  = "price: 42 USD";
    const std::string s_email   = "user@example.com";
    const std::string s_long    = repeat('x', 9990) + "needle";  // match near end
    const std::string s_nomatch = repeat('a', 1000);              // no digits anywhere

    // Uniform adapters so bench_group can call either algorithm.
    auto do_match = [](const std::regex& re, std::smatch& m, const std::string& s) {
        return std::regex_match(s, m, re);
    };
    auto do_search = [](const std::regex& re, std::smatch& m, const std::string& s) {
        return std::regex_search(s, m, re);
    };

    // ── 1. ISO date — anchored full-string match, no captures ─────────────────
    // Short input, fast pattern: use high min_iters to suppress MSVC heap jitter.
    bench_group(
        "1. ISO date  ^\\d{4}-\\d{2}-\\d{2}$",
        R"(^\d{4}-\d{2}-\d{2}$)",
        s_date, do_match);

    // ── 2. Key=value — anchored full match, 2 captures ────────────────────────
    bench_group(
        "2. Key=value  ^(\\w+)=(\\w+)$",
        R"(^(\w+)=(\w+)$)",
        s_kv, do_match);

    // ── 3. Digit extraction — substring search, 1 capture ─────────────────────
    bench_group(
        "3. Digit extraction  \\d+  in short string",
        R"(\d+)",
        s_digits, do_search);

    // ── 4. Email — anchored full match, 3 captures ────────────────────────────
    bench_group(
        "4. Email  ^([\\w]+)@([\\w]+)\\.([a-z]+)$",
        R"(^([\w]+)@([\w]+)\.([a-z]+)$)",
        s_email, do_match);

    // ── 5. Long string — substring search, match near end (10 000 chars) ──────
    // Each iteration is ~150-200 µs; keep min_iters low to avoid multi-minute runs.
    bench_group(
        "5. Long string (10 000 chars)  needle near end",
        "needle",
        s_long, do_search,
        /*min_iters=*/500);

    // ── 6. No-match exhaustion — full scan, no match (1 000 chars) ────────────
    // std::regex takes ~430 µs each; 200 iters keeps total runtime reasonable.
    bench_group(
        "6. No-match exhaustion  \\d{6}  on 1000-char alpha string",
        R"(\d{6})",
        s_nomatch, do_search,
        /*min_iters=*/200);

    // ── 7. std::regex construction cost (no matching) ─────────────────────────
    // ezregex has no compile step; this shows the surcharge embedded in the
    // "cold" numbers above and the reason one-shot usage heavily favours ezregex.
    nb::Bench()
        .title("7. std::regex construction cost  (no matching)")
        .unit("construct")
        .warmup(200)
        .minEpochIterations(25000)
        .run("simple   \\d+",                    [&] {
            std::regex re(R"(\d+)");
            nb::doNotOptimizeAway(&re);
        })
        .run("date     ^\\d{4}-\\d{2}-\\d{2}$",  [&] {
            std::regex re(R"(^\d{4}-\d{2}-\d{2}$)");
            nb::doNotOptimizeAway(&re);
        })
        .run("email    ^([\\w]+)@...$",            [&] {
            std::regex re(R"(^([\w]+)@([\w]+)\.([a-z]+)$)");
            nb::doNotOptimizeAway(&re);
        });
}
