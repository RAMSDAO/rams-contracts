#pragma once

#include <vector>
#include <string>

namespace rams {
namespace utils {
static bool is_digit(const string str) {
    if (!str.size()) return false;
    for (const auto c : str) {
        if (!isdigit(c)) return false;
    }
    return true;
}
static std::vector<std::string> split(const std::string str, const std::string delim) {
    std::vector<std::string> tokens;
    if (str.size() == 0) return tokens;

    size_t prev = 0, pos = 0;
    do {
        pos = str.find(delim, prev);
        if (pos == std::string::npos) pos = str.length();
        std::string token = str.substr(prev, pos - prev);
        if (token.length() > 0) tokens.push_back(token);
        prev = pos + delim.length();
    } while (pos < str.length() && prev < str.length());
    return tokens;
}

static name parse_name(const string& str) {
    if (str.length() == 0 || str.length() > 13) return {};
    int i = -1;
    for (const auto c : str) {
        i++;
        if (islower(c) || (isdigit(c) && c <= '6') || c == '.') {
            if (i == 0 && !islower(c)) return {};
            if (i == 12 && (c < 'a' || c > 'j')) return {};
        } else
            return {};
    }
    return name{str};
}

}  // namespace utils
}  // namespace rams