#include "impl/Clock.hpp"
#include "perf/Perf.hpp"

template <dlsm::Clock::Concept Clock>
static void Measure(benchmark::State& state) {
    for (auto _ : state) {
        Clock::timestamp();
    }
    state.counters["Resolution(ns)"] = static_cast<double>(Clock::resolution().count());
}

BENCHMARK(Measure<dlsm::Clock::System>);
BENCHMARK(Measure<dlsm::Clock::Steady>);
BENCHMARK(Measure<dlsm::Clock::HiRes>);
BENCHMARK(Measure<dlsm::Clock::Realtime>);
BENCHMARK(Measure<dlsm::Clock::Monotonic>);
BENCHMARK(Measure<dlsm::Clock::RealCoarse>);
BENCHMARK(Measure<dlsm::Clock::MonoCoarse>);
BENCHMARK(Measure<dlsm::Clock::GetTimeOfDay>);

static void MeasureCPUTicks(benchmark::State& state) {
    for (auto _ : state) {
        dlsm::Clock::cputicks();
    }
}

BENCHMARK(MeasureCPUTicks);
