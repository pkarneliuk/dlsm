#include "impl/Str.h"

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

const std::string& ParseOpts::Get(const std::string& key, const std::string& _default) const {
    const auto it = find(key);
    if (it == end()) {
        return _default;
    }
    return it->second;
}
}  // namespace dlsm::Str