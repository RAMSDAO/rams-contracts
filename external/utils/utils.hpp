#pragma once

#include <vector>
#include <string>

namespace rams {
    namespace utils {
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
    }  // namespace utils
}  // namespace rams