## Performance Tests
Performance tests focussed on microbenchmarking using [google/benchmark](https://github.com/google/benchmark/blob/main/docs/user_guide.md).

## Useful Commands
```sh
# Build in dir './build' perf and run Transport
make -C ./build perf && ./build/tests/perf/perf --benchmark_filter=Transport --benchmark_counters_tabular=true

# Run 5 repetitions with 1 iteration benchmarking
./build/tests/perf/perf --benchmark_filter=*    \
    --benchmark_repetitions=5                   \
    --benchmark_min_time=1x                     \
    --benchmark_enable_random_interleaving=true \
    --benchmark_counters_tabular=true           \
    --benchmark_display_aggregates_only=true
```

## [delays.py](delays.py)
This script reads binary files with `int64` samples(nanoseconds timestamps), and plots them as `master` and `signals` delays relative to `master`.
```sh
# Display Pub1.ns as master and delays of Sub1/Sub2/Sub3/Sub4.ns signals relative to Pub1
./tests/perf/delays.py ./build/tests/perf/TransportPubSub-mem-*
```
