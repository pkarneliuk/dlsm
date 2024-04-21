#include "impl/Str.hpp"

#include <format>
#include <regex>

namespace dlsm::Str {
ParseOpts::ParseOpts(const std::string_view& opts) {
    static const std::regex re("([^=]*)=([^,]*),?");
    std::for_each(std::cregex_iterator(opts.begin(), opts.end(), re), std::cregex_iterator{}, [&](const auto& match) {
        const auto it = this->find(match[1]);
        if (it != this->end()) {
            throw std::invalid_argument(std::format("Duplicate key:{}", it->first));
        }
        (*this)[match[1]] = match[2];
    });
}

const std::string& ParseOpts::required(const std::string& key) const {
    if (const auto it = find(key); it != end()) return it->second;
    throw std::invalid_argument(std::format("Missing required key=value for key:{}", key));
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

XPair xpair(const std::string_view& axb) try {
    auto pair = dlsm::Str::split(axb, "x");
    if (pair.size() != 2) throw std::invalid_argument("Must be 2 unsigned desimal values");
    const auto parse = [](const auto& str) {
        std::size_t pos = 0;
        auto v = std::stoll(str, &pos, 10);
        if (v < 0 || pos != str.size()) throw std::invalid_argument("Value is negative");
        return v;
    };
    return {static_cast<std::uint32_t>(parse(pair[0])), static_cast<std::uint32_t>(parse(pair[1]))};
} catch (...) {
    throw std::invalid_argument(std::format("Invalid value:'{}' for xpair()", axb));
}

void copy(std::string_view src, std::span<char> dst) {
    if (std::size(dst) <= std::size(src)) {
        throw std::invalid_argument{
            std::format("Str::copy(): dst has not enough capacity: {} to store: {}", dst.size(), src)};
    }

    auto len = std::min(std::size(dst) - 1, std::size(src));
    auto end = std::copy(std::begin(src), std::begin(src) + len, std::begin(dst));
    std::fill(end, std::end(dst), '\0');
}
}  // namespace dlsm::Str