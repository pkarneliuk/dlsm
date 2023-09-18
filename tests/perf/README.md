## Performance Tests
Performance tests focussed on microbenchmarking using [google/benchmark](https://github.com/google/benchmark/blob/main/docs/user_guide.md).

## Useful Commands
```sh
# Run 5 repetitions with 1 second benchmarking
./build/tests/perf/perf                         \
    --benchmark_repetitions=5                   \
    --benchmark_min_time=1.0s                   \
    --benchmark_enable_random_interleaving=true \
    --benchmark_counters_tabular=true           \
    --benchmark_display_aggregates_only=true
```