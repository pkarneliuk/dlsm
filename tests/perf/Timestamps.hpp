#pragma once

#include <algorithm>
#include <fstream>
#include <string>
#include <vector>

#include "impl/Clock.hpp"

namespace Tests::Perf {

template <typename Clock = dlsm::Clock::System, template <typename> typename Allocator = std::allocator>
struct Timestamps {
    static constexpr std::chrono::nanoseconds Initial{0}, Max{100'000'000};
    static std::chrono::nanoseconds ts() { return Clock::timestamp(); };

    using List = std::vector<std::chrono::nanoseconds, Allocator<std::chrono::nanoseconds>>;
    std::vector<List> lists_;

    Timestamps(std::size_t lists, std::size_t samples) : lists_{lists, List(samples, Initial)} {}
    List& operator[](std::size_t i) { return lists_.at(i); }

    struct Percentile {
        std::string label = "unknown";
        std::chrono::nanoseconds value{0};
    };

    // Calculate percentiles of delays for all samples
    std::vector<Percentile> percentiles(const std::size_t master = 0) const {
        if (lists_.size() <= 1) return {};

        Timestamps::List deltas;
        deltas.reserve((lists_.size() - 1) * lists_[0].size());

        const auto& pub = lists_[master];  // start samples
        for (std::size_t s = 0; s < lists_.size(); ++s) {
            if (s == master) continue;
            const auto& sub = lists_[s];  // end samples
            for (std::size_t i = 0; i < pub.size(); ++i) {
                // Write Max value if no data in sub sample
                deltas.emplace_back(sub[i] == Timestamps::Initial ? Timestamps::Max : (sub[i] - pub[i]));
            }
        }

        std::sort(deltas.begin(), deltas.end());

        const auto percentile = [&](double p) {
            return deltas.at(static_cast<std::size_t>(static_cast<double>(deltas.size()) * p));
        };
        return {
            {"Min ", deltas.front()},
            {"Max ", deltas.back()},
            // {"25% ", percentile(0.25)},
            {"50% ", percentile(0.50)},
            // {"75% ", percentile(0.75)},
            {"99% ", percentile(0.99)},
            {"99.9% ", percentile(0.999)},
        };
    }

    void write(const std::string& filepath, const std::size_t master = 0) const {
        const auto write = [](const auto filename, const auto& ts) {
            std::fstream s{filename, s.trunc | s.binary | s.out};
            if (s)
                s.write(reinterpret_cast<const char*>(std::data(ts)),
                        std::ssize(ts) * static_cast<long>(sizeof(ts[0])));
            else
                throw std::runtime_error{"Failed to open " + filename + " for writing"};
        };

        for (std::size_t i = 0; const auto& ts : lists_) {
            write(i == master ? (filepath + "-Pub.ns") : (filepath + "-Sub" + std::to_string(i++) + ".ns"), ts);
        }
    }
};

}  // namespace Tests::Perf
