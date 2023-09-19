#pragma once

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace dlsm::Str {
// Parse opt1=value1,opt2=value2 string
struct ParseOpts : public std::map<std::string, std::string> {
    ParseOpts(const std::string_view& opts);
    const std::string& required(const std::string& key) const;
    const std::string& get(const std::string& key, const std::string& _default = Default) const;
    static const std::string Default;
};

std::vector<std::string> split(const std::string_view& s, const std::string_view& delimiter);
}  // namespace dlsm::Str