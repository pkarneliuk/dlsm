#include <barrier>
#include <format>
#include <iostream>
#include <mutex>
#include <span>
#include <thread>
#include <utility>
#include <vector>

#include "impl/Allocator.hpp"
#include "impl/Clock.hpp"
#include "impl/Disruptor.hpp"
#include "perf/Perf.hpp"
#include "perf/Timestamps.hpp"

using namespace std::literals;

namespace {
#ifdef NDEBUG
const auto repeats = 5;
const auto multiplier = 1;  // for real perf measurement: 1'000;
const auto latency_items = 100'000 * multiplier;
const auto throughput_items = 500'000 * multiplier;
#else
const int repeats = 1;
const auto latency_items = 1'000;
const auto throughput_items = 1'000;
#endif  // NDEBUG

using Timestamps = Tests::Perf::Timestamps<dlsm::Clock::Steady, dlsm::MAdviseAllocator>;

struct alignas(64) Event {
    std::chrono::nanoseconds timestamp;
    std::uint64_t seqnumber;
};

std::mutex state_mutex;  // NOLINT
void synchronized(auto action) {
    std::lock_guard<std::mutex> guard(state_mutex);
    action();
};

void checkSequence(auto& state, const auto& name, auto expected, auto seq) {
    if (expected != seq)
        synchronized([&] {
            state.SkipWithError(
                std::format("{} got gap on #{} received #{}, missing: {}", name, expected, seq, seq - expected));
        });
}

template <typename Impl>
void DisruptorLatency(benchmark::State& state) {
    // clang-format off
    const auto producers = static_cast<std::uint64_t>(state.range(0));
    const auto consumers = static_cast<std::uint64_t>(state.range(1));
    const auto tosend    = static_cast<std::uint64_t>(state.range(2));
    const auto batch     = static_cast<std::uint64_t>(state.range(3));
    // clang-format on

    static auto repeat = repeats;
    if (repeat == 0) repeat = repeats;
    --repeat;

    constexpr auto capacity = std::size_t(1) << 10;  // 1k events in buffer

    auto buffer = std::vector<Event, dlsm::MAdviseAllocator<Event>>{capacity, Event{}};
    auto ring = dlsm::Disruptor::Ring<Event>{buffer};
    auto wait = typename Impl::WaitStrategy{};
    typename Impl::BarrierType barrier;

    using Ty = dlsm::Disruptor::Sequence::Atomic;
    auto external = std::vector<Ty, dlsm::MAdviseAllocator<Ty>>(capacity);

    auto disruptor = Impl{barrier, ring.size(), wait, external};

    struct CollectTimestamps : dlsm::Disruptor::Processor::DefaultHandler<Event> {
        Timestamps::List& ts_;
        benchmark::State& state_;
        const std::string& name_;
        const std::size_t expected_;
        std::size_t count_ = 0ULL;

        CollectTimestamps(Timestamps::List& ts, benchmark::State& state, const std::string& name, std::size_t expected)
            : ts_{ts}, state_{state}, name_{name}, expected_{expected} {}

        Consumed onConsume(Event& data, dlsm::Disruptor::Sequence::Value, std::size_t) {
            ts_[data.seqnumber] = Timestamps::ts();

            const auto expected = count_;
            ++count_;
            checkSequence(state_, name_, expected, data.seqnumber);

            return count_ >= expected_ ? Consumed::Exit : Consumed::Release;
        }

        void report() {
            if (const auto lost = expected_ - count_) state_.counters[name_ + "Lost"] = static_cast<double>(lost);
        }
    };

    for (auto _ : state) {
        auto cpus = dlsm::Thread::AllCPU;
        dlsm::Thread::affinity(cpus.extract());
        std::vector<std::jthread> threads{producers + consumers};
        std::barrier sync(std::ssize(threads));
        auto begin = Timestamps::ts(), end = begin;

        state.counters["Pub x" + std::to_string(producers)] =
            static_cast<double>(tosend) / static_cast<double>(producers);

        auto timestamps = Timestamps{1 + consumers, tosend};

        for (std::size_t i = 0; auto& t : threads) {
            t = std::jthread([&, i]() {
                const auto affinity = cpus.count() > i ? cpus.at(i) : cpus;
                dlsm::Thread::affinity(affinity);

                const auto name =
                    (i < producers) ? "Pub"s + std::to_string(i + 1) : "Sub"s + std::to_string(i + 1 - producers);
                dlsm::Thread::name(name);

                std::uint64_t count = 0;
                if (i < producers) {
                    // First #0 producer will send remaining events to disruptor
                    const auto tosendP = tosend / producers + ((i == 0) ? (tosend % producers) : 0);
                    auto& ts = timestamps[0];
                    sync.arrive_and_wait();
                    if (i == 0) {
                        begin = Timestamps::ts();
                    }

                    if (batch > 0) {
                        while (count < tosendP) {
                            const auto n = std::min(batch, tosendP - count);
                            const auto hi = disruptor.claim(n);
                            const auto lo = hi - static_cast<dlsm::Disruptor::Sequence::Value>(n);
                            for (auto seq = lo; seq < hi; ++seq, ++count) {
                                auto& event = ring[seq];
                                auto useq = static_cast<std::size_t>(seq);

                                event.timestamp = ts[useq] = timestamps.ts();
                                event.seqnumber = useq;

                                disruptor.publish(seq);
                            }
                            // disruptor.publish(lo, hi);
                        }
                    } else {
                        while (count < tosendP) {
                            const auto seq = disruptor.claim() - 1;
                            auto useq = static_cast<std::size_t>(seq);

                            auto& event = ring[seq];
                            event.timestamp = ts[useq] = timestamps.ts();
                            event.seqnumber = useq;

                            disruptor.publish(seq);
                            ++count;
                        }
                    }

                    sync.arrive_and_wait();
                    if (i == 0) {
                        end = Timestamps::ts();

                        if (repeat == 0)  // Write timestamps to file on last repeat
                            timestamps.write(ts, state.name() + "-Pub");
                    }
                } else {
                    const auto id = i - producers;
                    auto& ts = timestamps[1 + id];

                    typename Impl::BarrierType barrier;
                    auto sub = typename Impl::Consumer{barrier, disruptor};
                    disruptor.add(sub.cursor());

                    CollectTimestamps handler(ts, state, name, tosend);
                    dlsm::Disruptor::Processor::Batch p{Event{}, sub, ring, handler};

                    sync.arrive_and_wait();

                    p.run();

                    sync.arrive_and_wait();
                    disruptor.del(sub.cursor());

                    synchronized([&] { handler.report(); });

                    if (repeat == 0)  // Write timestamps to files on last repeat
                        timestamps.write(ts, state.name() + "-" + name);
                }
            });
            ++i;
        }
        threads.clear();
        dlsm::Thread::affinity(dlsm::Thread::AllCPU);

        // Calculate delays and percentiles, Warming: skip first 10% of samples from statistics
        for (const auto& p : timestamps.percentiles(10.0)) {
            state.counters[p.label] = std::chrono::duration<double>(p.value).count();
        }

        const std::chrono::duration<double> seconds = end - begin;
        state.SetIterationTime(seconds.count());
        state.counters["per_item(avg)"] = (seconds / tosend).count();
    }
}

template <typename Impl>
void DisruptorThroughput(benchmark::State& state) {
    // clang-format off
    const auto producers = static_cast<std::uint64_t>(state.range(0));
    const auto consumers = static_cast<std::uint64_t>(state.range(1));
    const auto tosend    = static_cast<std::uint64_t>(state.range(2));
    const auto batch     = static_cast<std::uint64_t>(state.range(3));
    // clang-format on

    const std::size_t capacity =
        std::min(dlsm::Disruptor::ceilingNextPowerOfTwo(tosend / 2), (std::size_t(1) << 20 /*1Mb*/));

    auto buffer = std::vector<Event, dlsm::MAdviseAllocator<Event>>{capacity, Event{}};
    auto ring = dlsm::Disruptor::Ring<Event>{buffer};
    auto wait = typename Impl::WaitStrategy{};
    typename Impl::BarrierType barrier;

    auto disruptor = Impl{barrier, ring.size(), wait};

    auto bar = std::vector<typename Impl::BarrierType>();
    bar.reserve(consumers);
    auto barriers = std::vector<typename Impl::Consumer>();
    barriers.reserve(consumers);
    for (std::size_t i = 0; i < consumers; ++i) {
        auto& b = bar.emplace_back();
        const auto& c = barriers.emplace_back(b, disruptor);
        disruptor.add(c.cursor());
    }

    struct ReaderBatches {
        using AmountSize = std::pair<std::size_t, std::size_t>;
        std::vector<std::atomic<std::size_t>> amounts;
        ReaderBatches(std::size_t capacity) : amounts(capacity) {
            for (auto& v : amounts) {
                std::atomic_init(&v, 0ULL);
            }
        }

        void add(std::size_t size) { amounts.at(size - 1).fetch_add(1); }

        std::vector<AmountSize> sorted() const {
            std::vector<AmountSize> result;
            for (size_t i = 0; i < amounts.size(); ++i) {
                if (amounts[i] != 0) {
                    result.emplace_back(amounts[i] * (i + 1), (i + 1));
                }
            }
            std::ranges::sort(std::rbegin(result), std::rend(result));
            return result;
        }

        void top(std::size_t n) const {
            if (n == 0) return;
            auto ordered = sorted();

            std::size_t total = 0;
            for (const AmountSize& i : ordered) total += i.first;

            std::cout << std::format("Reader batch sizes, top {} of {}:\n", n, ordered.size());
            for (std::size_t i = 0; i < n && i < ordered.size(); ++i) {
                std::cout << std::format("{}): {} item batch,{:>8.4f}%, {} times\n", i + 1, ordered[i].second,
                                         (double)(100 * ordered[i].first) / ((double)total),
                                         (ordered[i].first / ordered[i].second));
            }
            std::cout << std::flush;
        }
    };

    struct CollectBatches : dlsm::Disruptor::Processor::DefaultHandler<Event> {
        ReaderBatches& batches_;
        benchmark::State& state_;
        const std::string& name_;
        const std::size_t expected_;
        std::size_t count_ = 0ULL;

        CollectBatches(ReaderBatches& batches, benchmark::State& state, const std::string& name, std::size_t expected)
            : batches_{batches}, state_{state}, name_{name}, expected_{expected} {}

        void onBatch(dlsm::Disruptor::Sequence::Value, std::size_t amount) { batches_.add(amount); }

        Consumed onConsume(Event& data, dlsm::Disruptor::Sequence::Value, std::size_t) {
            const auto expected = count_;
            ++count_;
            checkSequence(state_, name_, expected, data.seqnumber);

            return count_ >= expected_ ? Consumed::Exit : Consumed::Keep;
        }

        void report() {
            if (const auto lost = expected_ - count_) state_.counters[name_ + "Lost"] = static_cast<double>(lost);
        }
    };

    for (auto _ : state) {
        auto cpus = dlsm::Thread::AllCPU;
        dlsm::Thread::affinity(cpus.extract());
        std::vector<std::jthread> threads{producers + consumers};
        ReaderBatches batches{ring.size()};
        std::barrier sync(std::ssize(threads));

        auto begin = Timestamps::ts(), end = begin;

        state.counters["Pub x" + std::to_string(producers)] =
            static_cast<double>(tosend) / static_cast<double>(producers);
        state.counters["Sub x" + std::to_string(consumers)] = static_cast<double>(tosend);

        for (std::size_t i = 0; auto& t : threads) {
            t = std::jthread([&, i]() {
                const auto affinity = cpus.count() > i ? cpus.at(i) : cpus;
                dlsm::Thread::affinity(affinity);

                const auto name =
                    (i < producers) ? "Pub"s + std::to_string(i + 1) : "Sub"s + std::to_string(i + 1 - producers);
                dlsm::Thread::name(name);

                std::uint64_t count = 0;
                if (i < producers) {
                    // First #0 producer will send remaining events to disruptor
                    const auto tosendP = tosend / producers + ((i == 0) ? (tosend % producers) : 0);
                    sync.arrive_and_wait();
                    if (i == 0) {
                        begin = Timestamps::ts();
                    }

                    if (batch > 0) {
                        while (count < tosendP) {
                            const auto n = std::min(batch, tosendP - count);
                            const dlsm::Disruptor::Sequence::Value hi = disruptor.claim(n);
                            const dlsm::Disruptor::Sequence::Value lo =
                                hi - static_cast<dlsm::Disruptor::Sequence::Value>(n);
                            for (auto seq = lo; seq < hi; ++seq, ++count) {
                                auto& event = ring[seq];

                                event.seqnumber = static_cast<std::size_t>(seq);

                                // disruptor.publish(seq);
                            }
                            disruptor.publish(lo, hi);
                        }
                    } else {
                        while (count < tosendP) {
                            const auto seq = disruptor.claim() - 1;

                            auto& event = ring[seq];
                            event.seqnumber = static_cast<std::size_t>(seq);

                            disruptor.publish(seq);
                            ++count;
                        }
                    }

                    sync.arrive_and_wait();
                    if (i == 0) {
                        end = Timestamps::ts();
                    }
                } else {
                    auto& sub = barriers[i - producers];

                    CollectBatches handler(batches, state, name, tosend);
                    dlsm::Disruptor::Processor::Batch p{Event{}, sub, ring, handler};

                    sync.arrive_and_wait();

                    p.run();

                    sync.arrive_and_wait();
                    disruptor.del(sub.cursor());

                    synchronized([&] { handler.report(); });
                }
            });
            ++i;
        }
        threads.clear();

        const std::chrono::duration<double> seconds = end - begin;
        state.SetIterationTime(seconds.count());
        state.SetItemsProcessed(static_cast<int64_t>(tosend));
        state.SetBytesProcessed(static_cast<int64_t>(tosend * sizeof(Event)));
        state.counters["per_item(avg)"] = (seconds / tosend).count();

        dlsm::Thread::affinity(dlsm::Thread::AllCPU);

        batches.top(8);
    }
}

void DisruptorSpinner(benchmark::State& state) {
    const auto iteration = static_cast<std::uint32_t>(state.range(0));
    dlsm::Disruptor::Waits::SpinsStrategy::Spinner spinner;
    for (auto _ : state) {  // NOLINT
        spinner.iteration_ = iteration;
        spinner.once();
    }
}

void CustomArguments(benchmark::internal::Benchmark* b) {
    b->MeasureProcessCPUTime()->UseManualTime()->Unit(benchmark::kSecond)->Iterations(1)->Repetitions(repeats);
}
}  // namespace

using namespace dlsm::Disruptor::Waits;
using namespace dlsm::Disruptor::Barriers;
using namespace dlsm::Disruptor::Sequencers;
static constexpr std::size_t Ndeps = 4;  // Max number of dependencies of Barrier
using SPMCSpinsAtomics = SPMC<SpinsStrategy, AtomicsBarrier<Ndeps>>;
using MPMCSpinsAtomics = MPMC<SpinsStrategy, AtomicsBarrier<Ndeps>>;
// Inter-process communications through shared memory requires placement Barriers
// in external shared memory and using offsets instead of pointers for dependencies
using SPMCSpinsSharing = SPMC<SpinsStrategy, OffsetsBarrier<Ndeps>&>;
using MPMCSpinsSharing = MPMC<SpinsStrategy, OffsetsBarrier<Ndeps>&>;

BENCHMARK(DisruptorLatency<SPMCSpinsAtomics>)
    ->Apply(CustomArguments)
    ->Args({1, 4, latency_items, 0})
    ->Args({1, 4, latency_items, 32});
BENCHMARK(DisruptorLatency<SPMCSpinsSharing>)
    ->Apply(CustomArguments)
    ->Args({1, 4, latency_items, 0})
    ->Args({1, 4, latency_items, 32});

BENCHMARK(DisruptorLatency<MPMCSpinsAtomics>)
    ->Apply(CustomArguments)
    ->Args({1, 4, latency_items, 0})
    ->Args({1, 4, latency_items, 32});
BENCHMARK(DisruptorLatency<MPMCSpinsSharing>)
    ->Apply(CustomArguments)
    ->Args({1, 4, latency_items, 0})
    ->Args({1, 4, latency_items, 32});
BENCHMARK(DisruptorLatency<MPMCSpinsAtomics>)
    ->Apply(CustomArguments)
    ->Args({4, 1, latency_items, 0})
    ->Args({4, 1, latency_items, 32});
BENCHMARK(DisruptorLatency<MPMCSpinsSharing>)
    ->Apply(CustomArguments)
    ->Args({4, 1, latency_items, 0})
    ->Args({4, 1, latency_items, 32});

BENCHMARK(DisruptorThroughput<SPMCSpinsAtomics>)->Apply(CustomArguments)->Args({1, 4, throughput_items, 32});
BENCHMARK(DisruptorThroughput<SPMCSpinsSharing>)->Apply(CustomArguments)->Args({1, 4, throughput_items, 32});

BENCHMARK(DisruptorThroughput<MPMCSpinsAtomics>)
    ->Apply(CustomArguments)
    ->Args({1, 4, throughput_items, 32})
    ->Args({4, 1, throughput_items, 32});
BENCHMARK(DisruptorThroughput<MPMCSpinsSharing>)
    ->Apply(CustomArguments)
    ->Args({1, 4, throughput_items, 32})
    ->Args({4, 1, throughput_items, 32});

BENCHMARK(DisruptorSpinner)->DenseRange(SpinsStrategy::Spinner::Initial, SpinsStrategy::Spinner::Sleep, 1);
