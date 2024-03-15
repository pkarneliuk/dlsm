#include "impl/SpinLock.hpp"
#include "perf/Perf.hpp"

namespace {
template <typename SpinLock>
void Measure(benchmark::State& state) {
    static SpinLock m;
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

BENCHMARK(Measure<dlsm::SpinLock::TASSLock>)->ThreadRange(1, 8)->Iterations(count);
BENCHMARK(Measure<dlsm::SpinLock::TTSSLock>)->ThreadRange(1, 8)->Iterations(count);
BENCHMARK(Measure<dlsm::SpinLock::StdMutex>)->ThreadRange(1, 8)->Iterations(count);
