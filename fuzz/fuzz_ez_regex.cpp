#include "ez_regex.h"
#include <cassert>
#include <cstring>
#include <string>
#include <vector>
#include <string_view>

// Input format: <pattern bytes> '\x00' <subject bytes>
// If no null separator is present, the whole input is treated as the pattern
// with an empty subject — this exercises the validator on arbitrary bytes.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    const char* raw = reinterpret_cast<const char*>(data);
    const char* sep = static_cast<const char*>(std::memchr(raw, '\0', size));

    std::string pattern, subject;
    if (sep) {
        pattern.assign(raw, static_cast<size_t>(sep - raw));
        subject.assign(sep + 1, size - static_cast<size_t>(sep - raw) - 1);
    } else {
        pattern.assign(raw, size);
    }

    std::vector<std::string_view> caps;
    int r = ez::regex_match(pattern.c_str(), subject.c_str(), &caps);

    // Every returned capture must point inside the subject buffer.
    if (r == EZ_REGEX_MATCH) {
        const char* s = subject.c_str();
        const char* e = s + subject.size();
        for (const auto& sv : caps) {
            assert(sv.data() >= s);
            assert(sv.data() + sv.size() <= e);
        }
    }

    return 0;
}
