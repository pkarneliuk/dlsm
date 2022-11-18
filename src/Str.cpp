#include "impl/Str.hpp"

#include <regex>

namespace dlsm::Str {
ParseOpts::ParseOpts(const std::string& opts) {
    static const std::regex re("([^=]*)=([^,]*),?");
    std::for_each(std::sregex_iterator(opts.begin(), opts.end(), re), std::sregex_iterator{}, [&](const auto& match) {
        const auto it = this->find(match[1]);
        if (it != this->end()) {
            throw std::invalid_argument("Duplicate key:" + it->first);
        }
        (*this)[match[1]] = match[2];
    });
}

const std::string& ParseOpts::get(const std::string& key, const std::string& _default) const {
    if (const auto it = find(key); it != end()) return it->second;
    if (!_default.empty()) return _default;
    throw std::invalid_argument("Missing required key=value for key:" + key);
}
}  // namespace dlsm::Str