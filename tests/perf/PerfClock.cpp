#include "impl/Clock.hpp"
#include "perf/Perf.hpp"

namespace {
template <dlsm::Clock::Concept Clock>
void Measure(benchmark::State& state) {
    for (auto _ : state) {  // NOLINT
        Clock::timestamp();
    }
    state.counters["Resolution(ns)"] = static_cast<double>(Clock::resolution().count());
}

void MeasureCPUTicks(benchmark::State& state) {
    for (auto _ : state) {  // NOLINT
        dlsm::Clock::cputicks();
    }
}
}  // namespace

BENCHMARK(Measure<dlsm::Clock::System>);
BENCHMARK(Measure<dlsm::Clock::Steady>);
BENCHMARK(Measure<dlsm::Clock::HiRes>);
BENCHMARK(Measure<dlsm::Clock::Realtime>);
BENCHMARK(Measure<dlsm::Clock::Monotonic>);
BENCHMARK(Measure<dlsm::Clock::RealCoarse>);
BENCHMARK(Measure<dlsm::Clock::MonoCoarse>);
BENCHMARK(Measure<dlsm::Clock::GetTimeOfDay>);

BENCHMARK(MeasureCPUTicks);
