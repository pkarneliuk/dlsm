#pragma once

#include <algorithm>
#include <fstream>
#include <future>
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

        const auto subscribers = lists_.size() - 1;  // - master
        const auto parallel = lists_[0].size() > 100'000;
        Timestamps::List deltas(subscribers * lists_[0].size(), Initial);
        auto dst = std::begin(deltas);

        const auto delta = [](const auto& pub, const auto& sub) {
            // Write Max value if no data in subscriber sample
            return sub == Timestamps::Initial ? Timestamps::Max : (sub - pub);
        };

        std::vector<std::future<void>> handles;
        handles.reserve(subscribers);

        const auto& pub = lists_[master];  // start samples
        for (std::size_t s = 0; s < lists_.size(); ++s) {
            if (s == master) continue;
            const auto& sub = lists_[s];  // end samples
            auto i1 = std::cbegin(pub);
            auto i2 = std::cend(pub);
            auto i3 = std::cbegin(sub);
            if (parallel) {
                handles.emplace_back(std::async(std::launch::async, [=]() { std::transform(i1, i2, i3, dst, delta); }));
            } else {
                std::transform(i1, i2, i3, dst, delta);
            }
            dst += static_cast<long>(pub.size());
        }
        handles.clear();

        std::sort(std::begin(deltas), std::end(deltas));

        const auto percentile = [&](double p) {
            return deltas.at(static_cast<std::size_t>(static_cast<double>(deltas.size()) * p));
        };
        return {
            {"Min ", deltas.front()},
            {"Max ", deltas.back()},
            // {"25% ", percentile(0.25)},
            {"50% ", percentile(0.50)},
            // {"75% ", percentile(0.75)},
            {"90% ", percentile(0.90)},
            {"99% ", percentile(0.99)},
            {"99.9% ", percentile(0.999)},
        };
    }

    void write(const std::size_t index, const std::string& name) const {
        const auto& ts = lists_.at(index);

        auto replaceAll = [](auto s, std::string_view o, std::string_view n) {
            while (s.find(o) != std::string::npos) s.replace(s.find(o), o.size(), n);
            return s;
        };
        const auto path = replaceAll(name, "/", "-") + ".ns";

        std::fstream s{path, s.trunc | s.binary | s.out};
        if (!s) throw std::runtime_error{"Failed to open " + path + " for writing"};

        s.write(reinterpret_cast<const char*>(std::data(ts)), std::ssize(ts) * static_cast<long>(sizeof(ts[0])));
    }
};

}  // namespace Tests::Perf
