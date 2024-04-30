#include <atomic>
#include <barrier>
#include <iostream>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "impl/Allocator.hpp"
#include "impl/Clock.hpp"
#include "impl/Thread.hpp"
#include "impl/Transport.hpp"
#include "perf/Perf.hpp"
#include "perf/Timestamps.hpp"

using namespace std::literals;

namespace {
struct Event {
    std::chrono::nanoseconds timestamp;
    std::uint64_t seqnumber;
};

template <class... Args>
void TransportPubSub(benchmark::State& state, Args&&... args) {
    auto args_tuple = std::make_tuple(std::forward<Args>(args)...);
    // clang-format off
    const auto type             = std::get<0>(args_tuple);
    const auto ropts            = std::get<1>(args_tuple);
    const auto popts            = std::get<2>(args_tuple);
    const auto sopts            = std::get<3>(args_tuple);
    const auto subscribers      = static_cast<std::uint64_t>(state.range(0));
    const auto tosend           = static_cast<std::uint64_t>(state.range(1));
    const auto send_delay_after = static_cast<std::uint64_t>(state.range(2));
    const auto send_delay_us    = static_cast<std::uint64_t>(state.range(3));
    // clang-format on

    auto runtime = dlsm::Transport<std::remove_pointer_t<decltype(type)>>(ropts);
    std::mutex state_mutex;
    const auto synchronized = [&](auto action) {
        std::lock_guard<std::mutex> guard(state_mutex);
        action();
    };

    for (auto _ : state) {
        const auto affinity = dlsm::Thread::AllCPU;
        dlsm::Thread::affinity(affinity.at(0));
        std::atomic_uint64_t last_sent = 0, send_failed = 0;
        std::vector<std::jthread> threads{1 + subscribers};
        std::barrier sync(std::ssize(threads));

        auto timestamps =
            Tests::Perf::Timestamps<dlsm::Clock::Monotonic, dlsm::MAdviseAllocator>{std::size(threads), tosend};

        auto begin = timestamps.ts(), end = begin;

        for (std::size_t i = 0; auto& t : threads) {
            t = std::jthread([&, i]() {
                if (affinity.count() > i + 1) dlsm::Thread::affinity(affinity.at(i + 1));

                const auto name = (i == 0) ? "Pub"s : "Sub" + std::to_string(i);
                dlsm::Thread::name(name);

                auto& ts = timestamps[i];
                std::uint64_t count = 0;
                if (i == 0) {
                    auto pub = runtime.pub(popts);

                    sync.arrive_and_wait();
                    begin = timestamps.ts();

                    Event e{0ns, 0};
                    while (count < tosend) {
                        ts[e.seqnumber] = e.timestamp = timestamps.ts();
                        e.seqnumber += 1;

                        if (!pub.send(e)) {
                            std::cerr << "Failed to Send " << e.seqnumber << '\n';
                            send_failed = 1;
                            break;
                        }
                        if (send_delay_after != 0 && (count % send_delay_after == 0)) {
                            if (send_delay_us) {
                                if (send_delay_us == 1)
                                    dlsm::Thread::BusyPause::pause();
                                else if (send_delay_us == 2)
                                    dlsm::Thread::NanoSleep::pause();
                                else if (send_delay_us == 3)
                                    std::this_thread::yield();
                                else
                                    std::this_thread::sleep_for(std::chrono::microseconds{send_delay_us});
                            }
                        }
                        count += 1;
                    }
                    last_sent = count;
                    sync.arrive_and_wait();
                    end = timestamps.ts();

                    synchronized([&] { state.counters[name] = static_cast<double>(count); });
                } else {
                    auto sub = runtime.sub(sopts);
                    std::size_t timeouts = 0;

                    sync.arrive_and_wait();
                    Event e{};
                    while (e.seqnumber < tosend) {
                        const auto expected = e.seqnumber + 1;
                        if (sub.recv(e)) {
                            ts[e.seqnumber - 1] = timestamps.ts();
                            count += 1;
                            if (expected != e.seqnumber) {
                                std::cerr << "Gap in Recv #" << expected << " != #" << e.seqnumber
                                          << " missing:" << (e.seqnumber - expected) << '\n';
                                break;
                            }
                        } else {
                            ++timeouts;
                            if (last_sent == tosend) {
                                std::cerr << "Sending completed. Break recv after Timeout on #" << e.seqnumber
                                          << " lost:" << (tosend - e.seqnumber) << '\n';
                                break;
                            }
                            if (send_failed) {
                                std::cerr << "Break after Send failure on #" << e.seqnumber
                                          << " lost:" << (tosend - e.seqnumber) << '\n';
                                break;
                            }
                        }
                    }
                    sync.arrive_and_wait();

                    synchronized([&] {
                        if (const auto lost = tosend - count) state.counters[name + "Lost"] = static_cast<double>(lost);
                        if (timeouts) state.counters[name + "TO"] = static_cast<double>(timeouts);
                    });
                }

                // Write timestamps to files
                timestamps.write(ts, state.name() + "-" + name);
            });
            ++i;
        }
        threads.clear();
        dlsm::Thread::affinity(dlsm::Thread::AllCPU);

        // Calculate delays for all samples and percentiles
        for (const auto& p : timestamps.percentiles()) {
            state.counters[p.label] = std::chrono::duration<double>(p.value).count();
        }

        const std::chrono::duration<double> seconds = end - begin;
        state.SetIterationTime(seconds.count());
        state.counters["per_item(avg)"] = (seconds / tosend).count();
    }
}
}  // namespace

#ifdef NDEBUG
const auto repeats = 5;
const auto num_msgs = 1'000'000;
#else
const auto repeats = 1;
const auto num_msgs = 1'000;
#endif  // NDEBUG

const auto hwm_msgs = 1'000'000;
const auto buf_size = sizeof(Event) * num_msgs;

const auto popts = ",delay_ms=100,send_timeout_ms=10,send_buf_size="s + std::to_string(buf_size) + ",send_hwm_msgs="s +
                   std::to_string(hwm_msgs);
const auto sopts = ",delay_ms=500,recv_timeout_ms=100,recv_buf_size="s + std::to_string(buf_size) + ",recv_hwm_msgs="s +
                   std::to_string(hwm_msgs);

// Use BENCHMARK_TEMPLATE1_CAPTURE after 1.8.4+ release
BENCHMARK_CAPTURE(TransportPubSub, mem, (dlsm::ZMQ*)nullptr, "io_threads=0"s,
                  "endpoint=inproc://mem-pub-sub-perf"s + popts, "endpoint=inproc://mem-pub-sub-perf"s + sopts)
    ->MeasureProcessCPUTime()
    ->UseManualTime()
    ->Unit(benchmark::kSecond)
    ->Iterations(1)
    ->Repetitions(repeats)
    ->Args({4, num_msgs, 0, 1});
BENCHMARK_CAPTURE(TransportPubSub, ipc, (dlsm::ZMQ*)nullptr, "io_threads=2"s,
                  "endpoint=ipc://@ipc-pub-sub-perf"s + popts, "endpoint=ipc://@ipc-pub-sub-perf"s + sopts)
    ->MeasureProcessCPUTime()
    ->UseManualTime()
    ->Unit(benchmark::kSecond)
    ->Iterations(1)
    ->Repetitions(repeats)
    ->Args({4, num_msgs, 0, 1});
BENCHMARK_CAPTURE(TransportPubSub, tcp, (dlsm::ZMQ*)nullptr, "io_threads=2"s, "endpoint=tcp://127.0.0.1:5551"s + popts,
                  "endpoint=tcp://127.0.0.1:5551"s + sopts)
    ->MeasureProcessCPUTime()
    ->UseManualTime()
    ->Unit(benchmark::kSecond)
    ->Iterations(1)
    ->Repetitions(repeats)
    ->Args({4, num_msgs, 0, 1});
