## Performance Tests
Performance tests focussed on microbenchmarking using [google/benchmark](https://github.com/google/benchmark/blob/main/docs/user_guide.md).

## Useful Commands
```sh
# Build in dir './build' perf and run Transport
make -C ./build perf && ./build/tests/perf/perf --benchmark_filter=Transport --benchmark_counters_tabular=true

# Run Transport under perf stat
sudo perf stat                 ./build/tests/perf/perf --benchmark_filter=Transport --benchmark_counters_tabular=true
sudo perf stat taskset -c 6-11 ./build/tests/perf/perf --benchmark_filter=Transport --benchmark_counters_tabular=true

# Run SpinLock for L1d cache
sudo perf stat -e L1-dcache-loads,L1-dcache-load-misses,mem_inst_retired.lock_loads ./build/tests/perf/perf --benchmark_filter=TASS
sudo perf stat -e L1-dcache-loads,L1-dcache-load-misses,mem_inst_retired.lock_loads ./build/tests/perf/perf --benchmark_filter=TTSS

# Run 5 repetitions with 1 iteration benchmarking
./build/tests/perf/perf --benchmark_filter=*    \
    --benchmark_repetitions=5                   \
    --benchmark_min_time=1x                     \
    --benchmark_enable_random_interleaving=true \
    --benchmark_counters_tabular=true           \
    --benchmark_display_aggregates_only=true

# Watch memory statistics(may affect performance measurements)
watch -n 0.2 "grep -E Mem\|Huge\|Swap\|Cache\|locked\|Shmem /proc/meminfo && free -hwv"
# Drop all caches
sync && echo 3 | sudo tee /proc/sys/vm/drop_caches
# Virtual Memory statistics measurements interval, default is 1 second
sudo sysctl vm.stat_interval=300
```

## [delays.py](delays.py)
This script reads binary files with `int64` samples(nanoseconds timestamps), and plots them as `master` and `signals` delays relative to `master`.
```sh
# Display Pub.ns as master and delays of Sub1/Sub2/Sub3/Sub4.ns signals relative to Pub.ns
./tests/perf/delays.py ./build/tests/perf/TransportPubSub-mem-*
```
![Disruptor 1P4C Timestamps](../../docs/images/delays-Disruptor-1P4C.png)

## Threads Affinity and CPU Core Isolation
```sh
lscpu # CPU summary
lscpu --all --extended --output-all             # Per-core info
lstopo-no-graphics --no-io --no-legend --of txt # Display layout of available CPUs in physical packages
numactl --hardware # Display NUMA nodes
sudo grubby --update-kernel=ALL --args="isolcpus=6-11" # Isolate CPU #6 - #11 from OS scheduling
sudo grubby --update-kernel=ALL --remove-args="isolcpus=6-11"
cat /proc/cmdline       # Display kernel startup parameters
cat /sys/devices/system/cpu/isolated  # display isolated cores
taskset -c 0,4,6-8 pid  # Setting CPU affinity
```

## HugePages Support
```sh
# Set limit of Huge Pages in system
echo 1000 > /proc/sys/vm/nr_hugepages
# Watch statistics of available Huge Pages
watch -n 1 grep Huge /proc/meminfo
watch -n 1 numastat -cm
cat /sys/kernel/mm/transparent_hugepage/enabled
always [madvise] never
```


## Links and References
 - [Isolating CPUs using tuned-profiles-real-time](https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux_for_real_time/8/html/optimizing_rhel_8_for_real_time_for_low_latency_operation/assembly_isolating-cpus-using-tuned-profiles-realtime_optimizing-rhel8-for-real-time-for-low-latency-operation)
 - [Configuring HugeTLB Huge Pages](https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux/7/html/performance_tuning_guide/sect-red_hat_enterprise_linux-performance_tuning_guide-memory-configuring-huge-pages)
