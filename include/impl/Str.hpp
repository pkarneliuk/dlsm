#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <vector>

struct SignaturePlaceholder;  // Must be in global namespace

namespace dlsm::Str {
// Reference implementation by geekfolk:
// https://www.reddit.com/r/cpp/comments/lfi6jt/finally_a_possibly_portable_way_to_convert_types/
template <typename T>
consteval auto signature() {
    return std::string_view{std::source_location::current().function_name()};
}

template <typename T>
consteval auto typenameOf() noexcept {
    // Find compiler-specific position of typename
    auto text = signature<::SignaturePlaceholder>();
    auto head = text.find("SignaturePlaceholder");
    auto tail = text.size() - head - 20;
    // Find typename at compiler-specific position
    auto type = signature<T>();
    auto count = type.size() - head - tail;
    return type.substr(head, count);
}

// Parse opt1=value1,opt2=value2 string
struct ParseOpts : public std::map<std::string, std::string> {
    ParseOpts(const std::string_view& opts);
    const std::string& required(const std::string& key) const;
    const std::string& get(const std::string& key, const std::string& _default = Default) const;
    static const std::string Default;
};

std::vector<std::string> split(const std::string_view& s, const std::string_view& delimiter);

using XPair = std::pair<std::uint32_t, std::uint32_t>;
XPair xpair(const std::string_view& axb);
void copy(std::string_view src, std::span<char> dst);

template <std::size_t Size = 16>
struct Flat : public std::array<char, Size> {
    Flat() : std::array<char, Size>{'\0'} {};
    Flat(std::string_view v) : Flat{} { *this = v; }

    std::string_view str() const { return {this->data()}; }
    operator std::string_view() const { return str(); }
    Flat& operator=(std::string_view v) {
        copy(v, *this);
        return *this;
    }
};

}  // namespace dlsm::Str