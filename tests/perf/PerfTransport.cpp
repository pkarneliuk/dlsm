#include <algorithm>
#include <atomic>
#include <barrier>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "impl/Clock.hpp"
#include "impl/Thread.hpp"
#include "impl/Transport.hpp"
#include "perf/Perf.hpp"

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

    const auto timestamp = []() { return dlsm::Clock::Monotonic::timestamp(); };

    for (auto _ : state) {
        std::atomic_uint64_t last_sent = 0, send_failed = 0;
        std::vector<std::jthread> threads{1 + subscribers};
        std::barrier sync(std::ssize(threads));

        using NsList = std::vector<std::chrono::nanoseconds>;
        std::vector<NsList> timestamps{std::size(threads), NsList(tosend, 0ns)};

        for (std::size_t i = 0; auto& t : threads) {
            t = std::jthread([&, i]() {
                auto& ts = timestamps[i];
                std::uint64_t count = 0;
                if (i == 0) {
                    auto pub = runtime.pub(popts);

                    sync.arrive_and_wait();

                    Event e{0ns, 0};
                    while (count < tosend) {
                        ts[e.seqnumber] = e.timestamp = timestamp();
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
                    synchronized([&] { state.counters["Pub"] = static_cast<double>(count); });
                    last_sent = count;
                    sync.arrive_and_wait();
                } else {
                    std::size_t timeouts = 0;

                    auto sub = runtime.sub(sopts);

                    sync.arrive_and_wait();
                    Event e{};
                    while (e.seqnumber < tosend) {
                        const auto expected = e.seqnumber + 1;
                        if (sub.recv(e)) {
                            ts[e.seqnumber - 1] = timestamp();
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
                    synchronized([&] {
                        if (const auto lost = tosend - count)
                            state.counters["Sub" + std::to_string(i) + "Lost"] = static_cast<double>(lost);
                        if (timeouts) state.counters["Sub" + std::to_string(i) + "TO"] = static_cast<double>(timeouts);
                    });
                    sync.arrive_and_wait();
                }
            });
            ++i;
        }
        threads.clear();

        {  // Calculate delays for all samples and percentiles
            std::vector<std::chrono::nanoseconds> deltas;
            deltas.reserve(subscribers * tosend);
            for (std::size_t s = 1; s <= subscribers; ++s) {
                for (std::size_t i = 0; i < tosend; ++i) {
                    const auto& pub = timestamps[0];
                    const auto& sub = timestamps[s];
                    deltas.emplace_back(sub[i] == 0ns ? 100s : (sub[i] - pub[i]));
                }
            }

            std::sort(deltas.begin(), deltas.end());

            const auto percentile = [&](double p) {
                return deltas.at(static_cast<std::size_t>(static_cast<double>(deltas.size()) * p));
            };
            const auto duration = [&](std::chrono::nanoseconds ns) {
                return std::chrono::duration<double>(ns).count();
            };

            state.counters["Min"] = duration(deltas.front());
            state.counters["Max"] = duration(deltas.back());
            state.counters["50%"] = duration(percentile(0.50));
            state.counters["99%"] = duration(percentile(0.99));
            state.counters["99.9%"] = duration(percentile(0.999));
        }

        {  // Write timestamps to files
            const auto write = [](const auto filename, const auto& ts) {
                std::fstream s{filename, s.trunc | s.binary | s.out};
                if (s)
                    s.write(reinterpret_cast<const char*>(std::data(ts)),
                            std::ssize(ts) * static_cast<long>(sizeof(ts[0])));
                else
                    std::cerr << "Failed to open " << filename << " for writing" << std::endl;
            };
            auto replaceAll = [](auto s, std::string_view o, std::string_view n) {
                while (s.find(o) != std::string::npos) s.replace(s.find(o), o.size(), n);
                return s;
            };
            const auto testname = replaceAll(state.name(), "/", "-");

            for (std::size_t i = 0; const auto& ts : timestamps) {
                if (i == 0)
                    write(testname + "-Pub.ns"s, ts);
                else
                    write(testname + "-Sub" + std::to_string(i) + ".ns"s, ts);
                ++i;
            }
        }
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
    ->Unit(benchmark::kSecond)
    ->Iterations(1)
    ->Repetitions(repeats)
    ->Args({4, num_msgs, 0, 1});
BENCHMARK_CAPTURE(TransportPubSub, ipc, (dlsm::ZMQ*)nullptr, "io_threads=2"s,
                  "endpoint=ipc://@ipc-pub-sub-perf"s + popts, "endpoint=ipc://@ipc-pub-sub-perf"s + sopts)
    ->Unit(benchmark::kSecond)
    ->Iterations(1)
    ->Repetitions(repeats)
    ->Args({4, num_msgs, 0, 1});
BENCHMARK_CAPTURE(TransportPubSub, tcp, (dlsm::ZMQ*)nullptr, "io_threads=2"s, "endpoint=tcp://127.0.0.1:5551"s + popts,
                  "endpoint=tcp://127.0.0.1:5551"s + sopts)
    ->Unit(benchmark::kSecond)
    ->Iterations(1)
    ->Repetitions(repeats)
    ->Args({4, num_msgs, 0, 1});

// BENCHMARK_CAPTURE(TransportPubSub, iox, (dlsm::IOX*)nullptr, "name=iox,inproc=on,pools=64x10000,log=off"s,
//                   "service=iox/test/perf,onfull=wait"s, "service=iox/test/perf,onfull=wait"s)
//     ->Unit(benchmark::kSecond)
//     ->Iterations(1)
//     ->Repetitions(repeats)
//     ->Args({4, num_msgs, 0, 1});

// BENCHMARK_CAPTURE(TransportPubSub, iox, (dlsm::IOX*)nullptr,
// "name=iox,inproc=on,monitor=on,pools=32x10000000,log=verbose"s,
// "service=iox/test/perf,onfull=discard"s,
// "service=iox/test/perf,onfull=discard"s)
//     ->Unit(benchmark::kSecond)
//     ->Iterations(1)
//     ->Repetitions(repeats)
//     ->Args({1, num_msgs, 0, 1});
