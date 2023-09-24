#include "impl/Str.hpp"

#include <regex>

namespace dlsm::Str {
ParseOpts::ParseOpts(const std::string_view& opts) {
    static const std::regex re("([^=]*)=([^,]*),?");
    std::for_each(std::cregex_iterator(opts.begin(), opts.end(), re), std::cregex_iterator{}, [&](const auto& match) {
        const auto it = this->find(match[1]);
        if (it != this->end()) {
            throw std::invalid_argument("Duplicate key:" + it->first);
        }
        (*this)[match[1]] = match[2];
    });
}

const std::string& ParseOpts::required(const std::string& key) const {
    if (const auto it = find(key); it != end()) return it->second;
    throw std::invalid_argument("Missing required key=value for key:" + key);
}
const std::string ParseOpts::Default{""};
const std::string& ParseOpts::get(const std::string& key, const std::string& _default) const {
    if (const auto it = find(key); it != end()) return it->second;
    return _default;
}

std::vector<std::string> split(const std::string_view& s, const std::string_view& delimiter) {
    if (delimiter.empty()) return std::vector<std::string>{std::string{s}};
    std::size_t pos_start = 0, pos_end = 0, delim_len = delimiter.length();
    std::string token;
    std::vector<std::string> res;

    while ((pos_end = s.find(delimiter, pos_start)) != std::string::npos) {
        token = s.substr(pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        res.emplace_back(token);
    }

    res.emplace_back(s.substr(pos_start));
    return res;
}

}  // namespace dlsm::Str