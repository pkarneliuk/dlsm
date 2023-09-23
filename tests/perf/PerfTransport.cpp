#include <algorithm>
#include <atomic>
#include <iostream>
#include <latch>
#include <mutex>
#include <thread>
#include <vector>

#include "impl/Clock.hpp"
#include "impl/Thread.hpp"
#include "impl/Transport.hpp"
#include "perf/Perf.hpp"

using namespace std::literals;

struct Event {
    std::chrono::nanoseconds timestamp;
    std::uint64_t seqnumber;
};

template <class... Args>
static void TransportZMQPubSub(benchmark::State& state, Args&&... args) {
    auto args_tuple = std::make_tuple(std::move(args)...);
    // clang-format off
    const auto endpoint         = std::get<0>(args_tuple);
    const auto buf_size         = std::get<1>(args_tuple);
    const auto hwm_msgs         = std::get<2>(args_tuple);
    const auto subscribers      = static_cast<std::uint64_t>(state.range(0));
    const auto tosend           = static_cast<std::uint64_t>(state.range(1));
    const auto send_delay_after = static_cast<std::uint64_t>(state.range(2));
    const auto send_delay_us    = static_cast<std::uint64_t>(state.range(3));
    // clang-format on

    // inproc:// does not need I/O threads, priority=99,sched_policy=2,
    auto p = dlsm::Transport::create("io_threads=" + (endpoint.starts_with("inproc://") ? "0"s : "4"s));
    std::atomic_uint64_t last_sent = 0, send_failed = 0;
    std::mutex state_mutex;
    const auto synchronized = [&](auto action) {
        std::lock_guard<std::mutex> guard(state_mutex);
        action();
    };

    const auto timestamp = []() { return dlsm::Clock::Monotonic::timestamp(); };

    for (auto _ : state) {
        std::vector<std::chrono::nanoseconds> deltas;

        std::vector<std::jthread> threads{1 + subscribers};
        std::latch enter{std::ssize(threads)};
        std::latch exit{std::ssize(threads)};

        for (std::size_t i = 0; auto& t : threads) {
            t = std::jthread([&, i]() {
                std::uint64_t count = 0;
                if (i == 0) {
                    auto pub = p->endpoint("type=pub,endpoint=" + endpoint +
                                           ",send_timeout_ms=10"
                                           ",send_buf_size=" +
                                           std::to_string(buf_size) + ",send_hwm_msgs=" + std::to_string(hwm_msgs));

                    enter.arrive_and_wait();

                    Event e{};
                    while (count < tosend) {
                        e.timestamp = timestamp();
                        e.seqnumber += 1;

                        if (!pub->send(e)) {
                            std::cerr << "Failed to Send " << e.seqnumber << std::endl;
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
                    synchronized([&] { state.counters["Pub"] = static_cast<double>(count); });
                    last_sent = count;
                    exit.arrive_and_wait();
                } else {
                    std::vector<std::chrono::nanoseconds> ts(tosend);
                    std::size_t timeouts = 0;

                    auto sub = p->endpoint("type=sub,endpoint=" + endpoint +
                                           ",delay_ms=500"
                                           ",recv_timeout_ms=100"
                                           ",recv_buf_size=" +
                                           std::to_string(buf_size) + ",recv_hwm_msgs=" + std::to_string(hwm_msgs));

                    enter.arrive_and_wait();
                    Event e{};
                    while (e.seqnumber < tosend) {
                        const auto expected = e.seqnumber + 1;
                        if (sub->recv(e)) {
                            ts[e.seqnumber - 1] = timestamp() - e.timestamp;
                            count += 1;
                            if (expected != e.seqnumber) {
                                //    std::cerr << "Gap in Recv #" << expected << " != #" << e.seqnumber
                                //              << " missing:" << (e.seqnumber - expected) << std::endl;
                            }
                        } else {
                            ++timeouts;
                            if (send_failed && last_sent >= tosend) {
                                std::cerr << "Break after Gap in Recv #" << last_sent << " != #" << e.seqnumber
                                          << " missing:" << (last_sent - e.seqnumber) << std::endl;
                                break;
                            }
                        }
                    }
                    synchronized([&] {
                        deltas.insert(deltas.end(), ts.begin(), ts.end());

                        if (const auto lost = tosend - count; lost)
                            state.counters["Sub" + std::to_string(i) + "Lost"] = static_cast<double>(lost);
                        if (timeouts) state.counters["Sub" + std::to_string(i) + "TO"] = static_cast<double>(timeouts);
                    });
                    exit.arrive_and_wait();
                }
            });
            ++i;
        }
        threads.clear();

        std::sort(deltas.begin(), deltas.end());

        const auto percentile = [&](double p) {
            return deltas.at(static_cast<std::size_t>(static_cast<double>(deltas.size()) * p));
        };
        const auto duration = [&](std::chrono::nanoseconds ns) { return std::chrono::duration<double>(ns).count(); };

        state.counters["Min"] = duration(deltas.front());
        state.counters["Max"] = duration(deltas.back());
        state.counters["50%"] = duration(percentile(0.50));
        state.counters["99%"] = duration(percentile(0.99));
        state.counters["99.9%"] = duration(percentile(0.999));
    }
}

#ifdef NDEBUG
const auto repeats = 5;
const auto num_msgs = 1'000'000;
#else
const auto repeats = 1;
const auto num_msgs = 1'000;
#endif  // NDEBUG

const auto hwm_msgs = 1'000'000;
const auto buf_size = sizeof(Event) * num_msgs;

BENCHMARK_CAPTURE(TransportZMQPubSub, inproc, "inproc://inproc-pub-sub-perf"s, buf_size, hwm_msgs)
    ->Unit(benchmark::kSecond)
    ->Iterations(1)
    ->Repetitions(repeats)
    ->Args({4, num_msgs, 0, 1});
BENCHMARK_CAPTURE(TransportZMQPubSub, ipc, "ipc://@ipc-pub-sub-perf"s, buf_size, hwm_msgs)
    ->Unit(benchmark::kSecond)
    ->Iterations(1)
    ->Repetitions(repeats)
    ->Args({4, num_msgs, 0, 1});
BENCHMARK_CAPTURE(TransportZMQPubSub, tcp, "tcp://127.0.0.1:5551"s, buf_size, hwm_msgs)
    ->Unit(benchmark::kSecond)
    ->Iterations(1)
    ->Repetitions(repeats)
    ->Args({4, num_msgs, 0, 1});
