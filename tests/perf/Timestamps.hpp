#pragma once

#include <algorithm>
#include <fstream>
#include <future>
#include <string>
#include <vector>

#include "impl/Clock.hpp"

namespace Tests::Perf {

template <dlsm::Clock::Concept Clock = dlsm::Clock::Steady, template <typename> typename Allocator = std::allocator>
struct Timestamps {
    static constexpr std::chrono::nanoseconds Initial{0}, Max{100'000'000};
    static constexpr std::chrono::nanoseconds delta(const std::chrono::nanoseconds& begin,
                                                    const std::chrono::nanoseconds& end) {
        // Write Max value if no data in end sample
        return end == Timestamps::Initial ? Timestamps::Max : (end - begin);
    }
    static std::chrono::nanoseconds ts() { return Clock::timestamp(); };

    using List = std::vector<std::chrono::nanoseconds, Allocator<std::chrono::nanoseconds>>;
    std::vector<List> lists_;

    Timestamps(std::size_t lists, std::size_t samples) : lists_{lists, List(samples, Initial)} {}
    List& operator[](std::size_t i) { return lists_.at(i); }

    Timestamps::List delays(double skipFirst = 0.0, const std::size_t master = 0) const {
        if (lists_.size() <= 1) return {};

        const auto subscribers = lists_.size() - 1;  // - master
        const auto parallel = lists_[0].size() > 100'000;
        const auto skip = static_cast<std::size_t>(double(lists_[0].size()) * (skipFirst / 100.0));
        Timestamps::List deltas(subscribers * (lists_[0].size() - skip), Initial);
        auto dst = std::begin(deltas);

        std::vector<std::future<void>> handles;
        handles.reserve(subscribers);

        const auto& pub = lists_[master];  // start samples
        for (std::size_t s = 0; s < lists_.size(); ++s) {
            if (s == master) continue;
            const auto& sub = lists_[s];  // end samples
            auto i1 = std::cbegin(pub) + static_cast<std::ptrdiff_t>(skip);
            auto i2 = std::cend(pub);
            auto i3 = std::cbegin(sub) + static_cast<std::ptrdiff_t>(skip);
            if (parallel) {
                handles.emplace_back(std::async(std::launch::async, [=]() { std::transform(i1, i2, i3, dst, delta); }));
            } else {
                std::transform(i1, i2, i3, dst, delta);
            }
            dst += i2 - i1;
        }
        handles.clear();
        return deltas;
    }

    struct Percentile {
        std::string label = "unknown";
        std::chrono::nanoseconds value{0};
    };

    // Calculate percentiles of delays for all samples
    std::vector<Percentile> percentiles(double skipFirst = 0.0, std::size_t master = 0) const {
        return percentiles(delays(skipFirst, master));
    }

    static std::vector<Percentile> percentiles(List deltas) {
        if (deltas.empty()) return {};

        const auto percentile = [&](double p) {
            const auto index = static_cast<std::ptrdiff_t>(static_cast<double>(deltas.size() - 1) * p);
            const auto it = std::begin(deltas) + index;
            std::nth_element(std::begin(deltas), it, std::end(deltas));
            return *it;
        };
        auto minmax = std::minmax_element(std::begin(deltas), std::end(deltas));
        return {
            {"Min ", *minmax.first},
            {"Max ", *minmax.second},
            // {"25% ", percentile(0.25)},
            {"50% ", percentile(0.50)},
            // {"75% ", percentile(0.75)},
            {"90% ", percentile(0.90)},
            {"99% ", percentile(0.99)},
            // {"99.9% ", percentile(0.999)},
        };
    }

    void write(const std::size_t index, const std::string& name) const { write(lists_.at(index), name); }

    static void write(const Timestamps::List& ts, const std::string& name) {
        auto replaceAll = [](auto s, std::string_view o, std::string_view n) {
            while (s.find(o) != std::string::npos) s.replace(s.find(o), o.size(), n);
            return s;
        };
        const auto path = replaceAll(name, "/", "-") + ".ns";

        std::ofstream s{path, s.trunc | s.binary | s.out};
        if (!s) throw std::runtime_error{"Failed to open " + path + " for writing"};

        s.write(reinterpret_cast<const char*>(std::data(ts)), std::ssize(ts) * static_cast<long>(sizeof(ts[0])));
    }
};

}  // namespace Tests::Perf
