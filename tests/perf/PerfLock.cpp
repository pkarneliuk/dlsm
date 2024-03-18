#include "impl/Lock.hpp"
#include "perf/Perf.hpp"

namespace {
template <typename Lock>
void Measure(benchmark::State& state) {
    static Lock m;
    for (auto _ : state) {  // NOLINT
        auto lock = std::unique_lock(m);
        // Dummy operations
        for (int i = 0, x = 0; i < 64; ++i) {
            benchmark::DoNotOptimize(x += i);
        }
    }
}

constexpr benchmark::IterationCount count = 100'000;
}  // namespace

BENCHMARK(Measure<dlsm::Lock::None>)->ThreadRange(1, 8)->Iterations(count);
BENCHMARK(Measure<dlsm::Lock::TASS>)->ThreadRange(1, 8)->Iterations(count);
BENCHMARK(Measure<dlsm::Lock::TTSS>)->ThreadRange(1, 8)->Iterations(count);
BENCHMARK(Measure<dlsm::Lock::StdMutex>)->ThreadRange(1, 8)->Iterations(count);
BENCHMARK(Measure<dlsm::Lock::StdRutex>)->ThreadRange(1, 8)->Iterations(count);
