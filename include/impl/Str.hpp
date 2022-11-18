#pragma once

#include <map>
#include <string>

namespace dlsm::Str {
// Parse opt1=value1,opt2=value2 string
struct ParseOpts : public std::map<std::string, std::string> {
    ParseOpts(const std::string& opts);
    const std::string& get(const std::string& key, const std::string& _default = {}) const;
};
}  // namespace dlsm::Str