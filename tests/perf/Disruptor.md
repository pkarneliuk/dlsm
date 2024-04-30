## Latency & Throughput tests for [dlsm::Disruptor](../../include/impl/Disruptor.hpp) are in [PerfDisruptor.cpp](PerfDisruptor.cpp)

It was tested in Virtual Box on Windows 10 host, so 99% percentiles and jitter are not representative. Latency tests skip first `10%` of measurements samples during calculation `50%`, `90%`, `99%` metrics(just treat them as warming memory and caches).

### Tests Environment
```bash
user@fedora:~/dlsm$ uname -a
Linux fedora 6.5.10-300.fc39.x86_64 #1 SMP PREEMPT_DYNAMIC Thu Nov  2 20:01:06 UTC 2023 x86_64 GNU/Linux
user@fedora:~/dlsm$ lscpu
Architecture:            x86_64
  CPU op-mode(s):        32-bit, 64-bit
  Address sizes:         39 bits physical, 48 bits virtual
  Byte Order:            Little Endian
CPU(s):                  12
  On-line CPU(s) list:   0-11
Vendor ID:               GenuineIntel
  Model name:            12th Gen Intel(R) Core(TM) i7-12700H
    CPU family:          6
    Model:               154
    Thread(s) per core:  1
    Core(s) per socket:  12
    Socket(s):           1
    Stepping:            3
    BogoMIPS:            5376.00
    Flags:               fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx rdtscp lm constant_tsc rep_good nopl xtopology nonstop_tsc cpuid tsc_known_freq pni pclmulqdq ssse3 c
                         x16 sse4_1 sse4_2 x2apic movbe popcnt aes xsave avx rdrand hypervisor lahf_lm abm 3dnowprefetch fsgsbase bmi1 avx2 bmi2 invpcid rdseed clflushopt arat md_clear flush_l1d arch_capabilities
Virtualization features:
  Hypervisor vendor:     KVM
  Virtualization type:   full
Caches (sum of all):
  L1d:                   576 KiB (12 instances)
  L1i:                   384 KiB (12 instances)
  L2:                    15 MiB (12 instances)
  L3:                    288 MiB (12 instances)
NUMA:
  NUMA node(s):          1
  NUMA node0 CPU(s):     0-11
Vulnerabilities:
  Gather data sampling:  Not affected
  Itlb multihit:         Not affected
  L1tf:                  Not affected
  Mds:                   Not affected
  Meltdown:              Not affected
  Mmio stale data:       Not affected
  Retbleed:              Not affected
  Spec rstack overflow:  Not affected
  Spec store bypass:     Vulnerable
  Spectre v1:            Mitigation; usercopy/swapgs barriers and __user pointer sanitization
  Spectre v2:            Mitigation; Retpolines, STIBP disabled, RSB filling, PBRSB-eIBRS Not affected
  Srbds:                 Not affected
  Tsx async abort:       Not affected
user@fedora:~/dlsm$ cat /proc/cmdline
BOOT_IMAGE=(hd0,gpt2)/vmlinuz-6.5.10-300.fc39.x86_64 root=UUID=3ce67a5d-34a5-4187-a149-a042b4e03660 ro rootflags=subvol=root rhgb quiet isolcpus=6-11
user@fedora:~/dlsm$ cat /sys/kernel/mm/transparent_hugepage/enabled
always [madvise] never
user@fedora:~/dlsm$ numactl --hardware
available: 1 nodes (0)
node 0 cpus: 0 1 2 3 4 5 6 7 8 9 10 11
node 0 size: 15977 MB
node 0 free: 3666 MB
node distances:
node   0
  0:  10
```

### Tests Results for SPMC/MPMC busy-wait In-process/Inter-process
Run by:
```user@fedora:~/dlsm$ time taskset -c 6-11  ./build/tests/perf/perf --benchmark_filter=Disruptor --benchmark_counters_tabular=true```

Scroll it horizontally.
<details><summary>2024-04-28T03:06:58+03:00</summary>

```bash
2024-04-28T03:06:58+03:00
Running ./build/tests/perf/perf
Run on (12 X 2687.99 MHz CPU s)
CPU Caches:
  L1 Data 48 KiB (x12)
  L1 Instruction 32 KiB (x12)
  L2 Unified 1280 KiB (x12)
  L3 Unified 24576 KiB (x12)
Load Average: 2.26, 3.71, 3.72
----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
Benchmark                                                                                                              Time             CPU   Iterations       50%        90%        99%        Max        Min      Pub x1 per_item(avg)
----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
DisruptorLatency<SPMCSpinsAtomics>/1/4/100000000/0/iterations:1/repeats:5/process_time/manual_time                  9.33 s          50.7 s             1       171n       596n    10.393u  0.0195771        41n       100M      93.2754n
DisruptorLatency<SPMCSpinsAtomics>/1/4/100000000/0/iterations:1/repeats:5/process_time/manual_time                  9.32 s          51.7 s             1       168n       624n     8.825u  0.0157792        49n       100M      93.1781n
DisruptorLatency<SPMCSpinsAtomics>/1/4/100000000/0/iterations:1/repeats:5/process_time/manual_time                  9.27 s          51.0 s             1       166n       584n     9.442u  0.0132266        52n       100M      92.7215n
DisruptorLatency<SPMCSpinsAtomics>/1/4/100000000/0/iterations:1/repeats:5/process_time/manual_time                  9.41 s          52.0 s             1       164n       587n     8.473u   0.035434        53n       100M      94.0744n
DisruptorLatency<SPMCSpinsAtomics>/1/4/100000000/0/iterations:1/repeats:5/process_time/manual_time                  9.48 s          54.5 s             1       169n       378n     9.518u  0.0144451        54n       100M       94.793n
DisruptorLatency<SPMCSpinsAtomics>/1/4/100000000/0/iterations:1/repeats:5/process_time/manual_time_mean             9.36 s          52.0 s             5     167.6n     553.8n    9.3302u  0.0196924      49.8n       100M      93.6085n
DisruptorLatency<SPMCSpinsAtomics>/1/4/100000000/0/iterations:1/repeats:5/process_time/manual_time_median           9.33 s          51.7 s             5       168n       587n     9.442u  0.0157792        52n       100M      93.2754n
DisruptorLatency<SPMCSpinsAtomics>/1/4/100000000/0/iterations:1/repeats:5/process_time/manual_time_stddev          0.082 s          1.48 s             5   2.70185n   99.5349n   736.093n   9.11682m   5.26308n      0.833      822.033p
DisruptorLatency<SPMCSpinsAtomics>/1/4/100000000/0/iterations:1/repeats:5/process_time/manual_time_cv               0.88 %          2.86 %             5      1.61%     17.97%      7.89%     46.30%     10.57%      0.00%         0.88%
DisruptorLatency<SPMCSpinsAtomics>/1/4/100000000/32/iterations:1/repeats:5/process_time/manual_time                 3.41 s          20.8 s             1     1.152u    16.203u    29.877u  0.0144117        64n       100M      34.0952n
DisruptorLatency<SPMCSpinsAtomics>/1/4/100000000/32/iterations:1/repeats:5/process_time/manual_time                 3.28 s          19.1 s             1     1.179u     8.561u    29.671u   9.01935m        62n       100M      32.7974n
DisruptorLatency<SPMCSpinsAtomics>/1/4/100000000/32/iterations:1/repeats:5/process_time/manual_time                 3.38 s          19.8 s             1     1.174u    14.384u    29.853u    0.01529        61n       100M      33.7833n
DisruptorLatency<SPMCSpinsAtomics>/1/4/100000000/32/iterations:1/repeats:5/process_time/manual_time                 3.19 s          18.6 s             1     1.228u    12.517u    30.047u   4.49388m        53n       100M      31.8845n
DisruptorLatency<SPMCSpinsAtomics>/1/4/100000000/32/iterations:1/repeats:5/process_time/manual_time                 2.47 s          18.0 s             1     1.362u     5.897u    26.551u   5.23252m        64n       100M      24.7149n
DisruptorLatency<SPMCSpinsAtomics>/1/4/100000000/32/iterations:1/repeats:5/process_time/manual_time_mean            3.15 s          19.2 s             5     1.219u   11.5124u   29.1998u    9.6895m      60.8n       100M       31.455n
DisruptorLatency<SPMCSpinsAtomics>/1/4/100000000/32/iterations:1/repeats:5/process_time/manual_time_median          3.28 s          19.1 s             5     1.179u    12.517u    29.853u   9.01935m        62n       100M      32.7974n
DisruptorLatency<SPMCSpinsAtomics>/1/4/100000000/32/iterations:1/repeats:5/process_time/manual_time_stddev         0.387 s          1.07 s             5   84.6227n    4.2279u   1.48671u   5.02428m   4.54973n      0.833      3.86675n
DisruptorLatency<SPMCSpinsAtomics>/1/4/100000000/32/iterations:1/repeats:5/process_time/manual_time_cv             12.29 %          5.54 %             5      6.94%     36.72%      5.09%     51.85%      7.48%      0.00%        12.29%
DisruptorLatency<SPMCSpinsSharing>/1/4/100000000/0/iterations:1/repeats:5/process_time/manual_time                  12.0 s          64.6 s             1       231n       899n    10.268u  0.0172506        22n       100M      120.157n
DisruptorLatency<SPMCSpinsSharing>/1/4/100000000/0/iterations:1/repeats:5/process_time/manual_time                  11.9 s          64.5 s             1       232n       896n     10.14u  0.0169999        67n       100M      119.421n
DisruptorLatency<SPMCSpinsSharing>/1/4/100000000/0/iterations:1/repeats:5/process_time/manual_time                  12.2 s          65.5 s             1       232n       902n    11.206u  0.0262233        61n       100M      122.285n
DisruptorLatency<SPMCSpinsSharing>/1/4/100000000/0/iterations:1/repeats:5/process_time/manual_time                  12.1 s          65.1 s             1       236n       894n    10.291u   0.024601        58n       100M       120.56n
DisruptorLatency<SPMCSpinsSharing>/1/4/100000000/0/iterations:1/repeats:5/process_time/manual_time                  11.2 s          63.5 s             1       186n       340n    10.372u   8.19814m        57n       100M      111.998n
DisruptorLatency<SPMCSpinsSharing>/1/4/100000000/0/iterations:1/repeats:5/process_time/manual_time_mean             11.9 s          64.6 s             5     223.4n     786.2n   10.4554u  0.0186546        53n       100M      118.884n
DisruptorLatency<SPMCSpinsSharing>/1/4/100000000/0/iterations:1/repeats:5/process_time/manual_time_median           12.0 s          64.6 s             5       232n       896n    10.291u  0.0172506        58n       100M      120.157n
DisruptorLatency<SPMCSpinsSharing>/1/4/100000000/0/iterations:1/repeats:5/process_time/manual_time_stddev          0.399 s         0.745 s             5   20.9952n   249.452n   427.779n   7.18841m   17.7623n      0.833      3.99075n
DisruptorLatency<SPMCSpinsSharing>/1/4/100000000/0/iterations:1/repeats:5/process_time/manual_time_cv               3.36 %          1.15 %             5      9.40%     31.73%      4.09%     38.53%     33.51%      0.00%         3.36%
DisruptorLatency<SPMCSpinsSharing>/1/4/100000000/32/iterations:1/repeats:5/process_time/manual_time                 3.66 s          22.7 s             1       695n    12.187u    30.443u   2.83701m        62n       100M      36.5854n
DisruptorLatency<SPMCSpinsSharing>/1/4/100000000/32/iterations:1/repeats:5/process_time/manual_time                 3.62 s          21.5 s             1       730n    19.688u    29.874u   8.96693m        67n       100M      36.1616n
DisruptorLatency<SPMCSpinsSharing>/1/4/100000000/32/iterations:1/repeats:5/process_time/manual_time                 3.45 s          20.7 s             1       707n    13.619u     44.48u  0.0146715        67n       100M      34.5485n
DisruptorLatency<SPMCSpinsSharing>/1/4/100000000/32/iterations:1/repeats:5/process_time/manual_time                 3.61 s          21.7 s             1       720n     21.18u    43.759u   2.78296m        67n       100M      36.0637n
DisruptorLatency<SPMCSpinsSharing>/1/4/100000000/32/iterations:1/repeats:5/process_time/manual_time                 3.18 s          21.5 s             1       686n     8.492u     29.67u    1.0919m        67n       100M      31.8346n
DisruptorLatency<SPMCSpinsSharing>/1/4/100000000/32/iterations:1/repeats:5/process_time/manual_time_mean            3.50 s          21.6 s             5     707.6n   15.0332u   35.6452u   6.07005m        66n       100M      35.0387n
DisruptorLatency<SPMCSpinsSharing>/1/4/100000000/32/iterations:1/repeats:5/process_time/manual_time_median          3.61 s          21.5 s             5       707n    13.619u    30.443u   2.83701m        67n       100M      36.0637n
DisruptorLatency<SPMCSpinsSharing>/1/4/100000000/32/iterations:1/repeats:5/process_time/manual_time_stddev         0.195 s         0.714 s             5   17.8969n   5.29949u   7.74532u   5.66604m   2.23607n      0.833      1.95003n
DisruptorLatency<SPMCSpinsSharing>/1/4/100000000/32/iterations:1/repeats:5/process_time/manual_time_cv              5.57 %          3.30 %             5      2.53%     35.25%     21.73%     93.34%      3.39%      0.00%         5.57%
DisruptorLatency<MPMCSpinsAtomics>/1/4/100000000/0/iterations:1/repeats:5/process_time/manual_time                  14.4 s          77.3 s             1       141n       183n     8.958u  0.0144793        46n       100M      144.301n
DisruptorLatency<MPMCSpinsAtomics>/1/4/100000000/0/iterations:1/repeats:5/process_time/manual_time                  12.4 s          66.5 s             1       125n       175n      7.09u  0.0305681        45n       100M      124.479n
DisruptorLatency<MPMCSpinsAtomics>/1/4/100000000/0/iterations:1/repeats:5/process_time/manual_time                  12.5 s          66.8 s             1       126n       174n     7.567u  0.0130309        43n       100M      124.727n
DisruptorLatency<MPMCSpinsAtomics>/1/4/100000000/0/iterations:1/repeats:5/process_time/manual_time                  12.5 s          67.1 s             1       125n       173n     6.906u   0.016571        42n       100M      124.585n
DisruptorLatency<MPMCSpinsAtomics>/1/4/100000000/0/iterations:1/repeats:5/process_time/manual_time                  12.5 s          69.2 s             1       125n       173n     7.977u   0.017144        47n       100M      124.665n
DisruptorLatency<MPMCSpinsAtomics>/1/4/100000000/0/iterations:1/repeats:5/process_time/manual_time_mean             12.9 s          69.4 s             5     128.4n     175.6n    7.6996u  0.0183587      44.6n       100M      128.551n
DisruptorLatency<MPMCSpinsAtomics>/1/4/100000000/0/iterations:1/repeats:5/process_time/manual_time_median           12.5 s          67.1 s             5       125n       174n     7.567u   0.016571        45n       100M      124.665n
DisruptorLatency<MPMCSpinsAtomics>/1/4/100000000/0/iterations:1/repeats:5/process_time/manual_time_stddev          0.880 s          4.54 s             5   7.05691n     4.219n   818.462n   7.02098m   2.07364n      0.833      8.80475n
DisruptorLatency<MPMCSpinsAtomics>/1/4/100000000/0/iterations:1/repeats:5/process_time/manual_time_cv               6.85 %          6.55 %             5      5.50%      2.40%     10.63%     38.24%      4.65%      0.00%         6.85%
DisruptorLatency<MPMCSpinsAtomics>/1/4/100000000/32/iterations:1/repeats:5/process_time/manual_time                 8.87 s          51.0 s             1       179n       291n    10.571u  0.0204653        53n       100M      88.7241n
DisruptorLatency<MPMCSpinsAtomics>/1/4/100000000/32/iterations:1/repeats:5/process_time/manual_time                 8.82 s          48.9 s             1       179n       292n     9.546u  0.0117131        52n       100M       88.165n
DisruptorLatency<MPMCSpinsAtomics>/1/4/100000000/32/iterations:1/repeats:5/process_time/manual_time                 9.00 s          50.3 s             1       180n       293n     9.493u   0.015888        61n       100M      89.9522n
DisruptorLatency<MPMCSpinsAtomics>/1/4/100000000/32/iterations:1/repeats:5/process_time/manual_time                 8.80 s          49.3 s             1       178n       283n     7.967u   7.53523m        61n       100M      87.9993n
DisruptorLatency<MPMCSpinsAtomics>/1/4/100000000/32/iterations:1/repeats:5/process_time/manual_time                 8.76 s          51.4 s             1       179n       283n     8.341u  0.0258682        43n       100M      87.6462n
DisruptorLatency<MPMCSpinsAtomics>/1/4/100000000/32/iterations:1/repeats:5/process_time/manual_time_mean            8.85 s          50.2 s             5       179n     288.4n    9.1836u   0.016294        54n       100M      88.4974n
DisruptorLatency<MPMCSpinsAtomics>/1/4/100000000/32/iterations:1/repeats:5/process_time/manual_time_median          8.82 s          50.3 s             5       179n       291n     9.493u   0.015888        53n       100M       88.165n
DisruptorLatency<MPMCSpinsAtomics>/1/4/100000000/32/iterations:1/repeats:5/process_time/manual_time_stddev         0.090 s          1.06 s             5   707.107p   4.97996n   1.04188u   7.19253m   7.48331n      0.833      901.552p
DisruptorLatency<MPMCSpinsAtomics>/1/4/100000000/32/iterations:1/repeats:5/process_time/manual_time_cv              1.02 %          2.12 %             5      0.40%      1.73%     11.34%     44.14%     13.86%      0.00%         1.02%
DisruptorLatency<MPMCSpinsSharing>/1/4/100000000/0/iterations:1/repeats:5/process_time/manual_time                  12.4 s          67.3 s             1       125n       177n     8.354u   7.56218m        49n       100M      124.325n
DisruptorLatency<MPMCSpinsSharing>/1/4/100000000/0/iterations:1/repeats:5/process_time/manual_time                  12.4 s          66.7 s             1       124n       175n     6.644u   8.37152m        49n       100M      124.132n
DisruptorLatency<MPMCSpinsSharing>/1/4/100000000/0/iterations:1/repeats:5/process_time/manual_time                  12.3 s          66.3 s             1       123n       170n     7.489u  0.0170156        46n       100M      122.869n
DisruptorLatency<MPMCSpinsSharing>/1/4/100000000/0/iterations:1/repeats:5/process_time/manual_time                  12.5 s          67.6 s             1       125n       179n      7.67u  0.0152955        46n       100M      125.142n
DisruptorLatency<MPMCSpinsSharing>/1/4/100000000/0/iterations:1/repeats:5/process_time/manual_time                  12.3 s          68.2 s             1       124n       173n      7.06u   0.011449        45n       100M      122.939n
DisruptorLatency<MPMCSpinsSharing>/1/4/100000000/0/iterations:1/repeats:5/process_time/manual_time_mean             12.4 s          67.2 s             5     124.2n     174.8n    7.4434u  0.0119388        47n       100M      123.881n
DisruptorLatency<MPMCSpinsSharing>/1/4/100000000/0/iterations:1/repeats:5/process_time/manual_time_median           12.4 s          67.3 s             5       124n       175n     7.489u   0.011449        46n       100M      124.132n
DisruptorLatency<MPMCSpinsSharing>/1/4/100000000/0/iterations:1/repeats:5/process_time/manual_time_stddev          0.097 s         0.764 s             5    836.66p   3.49285n   645.882n   4.15815m   1.87083n      0.833       969.57p
DisruptorLatency<MPMCSpinsSharing>/1/4/100000000/0/iterations:1/repeats:5/process_time/manual_time_cv               0.78 %          1.14 %             5      0.67%      2.00%      8.68%     34.83%      3.98%      0.00%         0.78%
DisruptorLatency<MPMCSpinsSharing>/1/4/100000000/32/iterations:1/repeats:5/process_time/manual_time                 8.81 s          49.4 s             1       181n       290n     9.616u  0.0144294        42n       100M       88.061n
DisruptorLatency<MPMCSpinsSharing>/1/4/100000000/32/iterations:1/repeats:5/process_time/manual_time                 8.75 s          49.1 s             1       181n       288n     8.711u  0.0261344        61n       100M      87.4536n
DisruptorLatency<MPMCSpinsSharing>/1/4/100000000/32/iterations:1/repeats:5/process_time/manual_time                 8.74 s          48.8 s             1       181n       289n     9.085u  0.0121636        60n       100M      87.3647n
DisruptorLatency<MPMCSpinsSharing>/1/4/100000000/32/iterations:1/repeats:5/process_time/manual_time                 10.0 s          55.1 s             1       191n       308n    11.045u  0.0147897        57n       100M      100.367n
DisruptorLatency<MPMCSpinsSharing>/1/4/100000000/32/iterations:1/repeats:5/process_time/manual_time                 8.81 s          51.1 s             1       182n       296n    10.421u  0.0138417        61n       100M      88.1306n
DisruptorLatency<MPMCSpinsSharing>/1/4/100000000/32/iterations:1/repeats:5/process_time/manual_time_mean            9.03 s          50.7 s             5     183.2n     294.2n    9.7756u  0.0162718      56.2n       100M      90.2754n
DisruptorLatency<MPMCSpinsSharing>/1/4/100000000/32/iterations:1/repeats:5/process_time/manual_time_median          8.81 s          49.4 s             5       181n       290n     9.616u  0.0144294        60n       100M       88.061n
DisruptorLatency<MPMCSpinsSharing>/1/4/100000000/32/iterations:1/repeats:5/process_time/manual_time_stddev         0.565 s          2.63 s             5   4.38178n   8.31865n   957.039n   5.60456m   8.10555n      0.833      5.65208n
DisruptorLatency<MPMCSpinsSharing>/1/4/100000000/32/iterations:1/repeats:5/process_time/manual_time_cv              6.26 %          5.19 %             5      2.39%      2.83%      9.79%     34.44%     14.42%      0.00%         6.26%
----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
Benchmark                                                                                                              Time             CPU   Iterations       50%        90%        99%        Max        Min      Pub x4 per_item(avg)
----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
DisruptorLatency<MPMCSpinsAtomics>/4/1/100000000/0/iterations:1/repeats:5/process_time/manual_time                  5.22 s          27.0 s             1       395n    47.021u    54.884u   8.69645m        65n        25M      52.2439n
DisruptorLatency<MPMCSpinsAtomics>/4/1/100000000/0/iterations:1/repeats:5/process_time/manual_time                  5.38 s          27.9 s             1       360n     3.065u    52.267u    8.5814m        42n        25M      53.7544n
DisruptorLatency<MPMCSpinsAtomics>/4/1/100000000/0/iterations:1/repeats:5/process_time/manual_time                  5.23 s          27.2 s             1       372n     32.64u    52.497u  0.0100931        65n        25M      52.3347n
DisruptorLatency<MPMCSpinsAtomics>/4/1/100000000/0/iterations:1/repeats:5/process_time/manual_time                  5.36 s          27.8 s             1       379n    35.132u    52.384u  0.0307617        68n        25M      53.5996n
DisruptorLatency<MPMCSpinsAtomics>/4/1/100000000/0/iterations:1/repeats:5/process_time/manual_time                  5.31 s          28.0 s             1       361n       566n    52.353u  0.0140403        57n        25M      53.1311n
DisruptorLatency<MPMCSpinsAtomics>/4/1/100000000/0/iterations:1/repeats:5/process_time/manual_time_mean             5.30 s          27.6 s             5     373.4n   23.6848u    52.877u  0.0144346      59.4n        25M      53.0127n
DisruptorLatency<MPMCSpinsAtomics>/4/1/100000000/0/iterations:1/repeats:5/process_time/manual_time_median           5.31 s          27.8 s             5       372n     32.64u    52.384u  0.0100931        65n        25M      53.1311n
DisruptorLatency<MPMCSpinsAtomics>/4/1/100000000/0/iterations:1/repeats:5/process_time/manual_time_stddev          0.070 s         0.466 s             5   14.4326n   20.7091u   1.12496u   9.39103m   10.5499n    0.20825      699.868p
DisruptorLatency<MPMCSpinsAtomics>/4/1/100000000/0/iterations:1/repeats:5/process_time/manual_time_cv               1.32 %          1.69 %             5      3.87%     87.44%      2.13%     65.06%     17.76%      0.00%         1.32%
DisruptorLatency<MPMCSpinsAtomics>/4/1/100000000/32/iterations:1/repeats:5/process_time/manual_time                 2.36 s          12.4 s             1    17.976u    20.155u    51.061u  0.0160589        71n        25M      23.6266n
DisruptorLatency<MPMCSpinsAtomics>/4/1/100000000/32/iterations:1/repeats:5/process_time/manual_time                 2.51 s          13.3 s             1    17.951u    48.674u    55.345u   5.88402m        66n        25M      25.1401n
DisruptorLatency<MPMCSpinsAtomics>/4/1/100000000/32/iterations:1/repeats:5/process_time/manual_time                 2.41 s          13.0 s             1    17.963u    19.337u    51.256u   1.50153m        74n        25M      24.0937n
DisruptorLatency<MPMCSpinsAtomics>/4/1/100000000/32/iterations:1/repeats:5/process_time/manual_time                 2.74 s          14.5 s             1    17.941u    49.475u     56.75u   8.56021m        70n        25M      27.3809n
DisruptorLatency<MPMCSpinsAtomics>/4/1/100000000/32/iterations:1/repeats:5/process_time/manual_time                 2.50 s          13.9 s             1    17.969u     48.96u    56.638u   6.76251m        62n        25M      25.0487n
DisruptorLatency<MPMCSpinsAtomics>/4/1/100000000/32/iterations:1/repeats:5/process_time/manual_time_mean            2.51 s          13.4 s             5     17.96u   37.3202u     54.21u   7.75343m      68.6n        25M       25.058n
DisruptorLatency<MPMCSpinsAtomics>/4/1/100000000/32/iterations:1/repeats:5/process_time/manual_time_median          2.50 s          13.3 s             5    17.963u    48.674u    55.345u   6.76251m        70n        25M      25.0487n
DisruptorLatency<MPMCSpinsAtomics>/4/1/100000000/32/iterations:1/repeats:5/process_time/manual_time_stddev         0.145 s         0.814 s             5   14.0357n   16.0481u   2.84066u   5.31963m   4.66905n    0.20825      1.44753n
DisruptorLatency<MPMCSpinsAtomics>/4/1/100000000/32/iterations:1/repeats:5/process_time/manual_time_cv              5.78 %          6.06 %             5      0.08%     43.00%      5.24%     68.61%      6.81%      0.00%         5.78%
DisruptorLatency<MPMCSpinsSharing>/4/1/100000000/0/iterations:1/repeats:5/process_time/manual_time                  5.22 s          27.2 s             1       365n     4.512u    52.461u  0.0345443        44n        25M       52.186n
DisruptorLatency<MPMCSpinsSharing>/4/1/100000000/0/iterations:1/repeats:5/process_time/manual_time                  5.25 s          26.9 s             1       395n    42.742u    56.332u  0.0157147        35n        25M      52.4871n
DisruptorLatency<MPMCSpinsSharing>/4/1/100000000/0/iterations:1/repeats:5/process_time/manual_time                  5.25 s          27.2 s             1       394n    50.714u    55.603u  0.0111909        59n        25M      52.5139n
DisruptorLatency<MPMCSpinsSharing>/4/1/100000000/0/iterations:1/repeats:5/process_time/manual_time                  5.21 s          26.9 s             1       391n    45.953u    53.378u   2.36218m        56n        25M      52.1453n
DisruptorLatency<MPMCSpinsSharing>/4/1/100000000/0/iterations:1/repeats:5/process_time/manual_time                  5.16 s          27.1 s             1       391n    50.341u    55.672u    7.1939m        58n        25M      51.5701n
DisruptorLatency<MPMCSpinsSharing>/4/1/100000000/0/iterations:1/repeats:5/process_time/manual_time_mean             5.22 s          27.0 s             5     387.2n   38.8524u   54.6892u  0.0142012      50.4n        25M      52.1805n
DisruptorLatency<MPMCSpinsSharing>/4/1/100000000/0/iterations:1/repeats:5/process_time/manual_time_median           5.22 s          27.1 s             5       391n    45.953u    55.603u  0.0111909        56n        25M       52.186n
DisruptorLatency<MPMCSpinsSharing>/4/1/100000000/0/iterations:1/repeats:5/process_time/manual_time_stddev          0.038 s         0.143 s             5   12.5379n   19.4775u   1.67211u  0.0123942   10.5024n    0.20825      380.453p
DisruptorLatency<MPMCSpinsSharing>/4/1/100000000/0/iterations:1/repeats:5/process_time/manual_time_cv               0.73 %          0.53 %             5      3.24%     50.13%      3.06%     87.28%     20.84%      0.00%         0.73%
DisruptorLatency<MPMCSpinsSharing>/4/1/100000000/32/iterations:1/repeats:5/process_time/manual_time                 2.61 s          13.9 s             1    18.151u    28.802u    52.341u   4.02694m        71n        25M      26.1356n
DisruptorLatency<MPMCSpinsSharing>/4/1/100000000/32/iterations:1/repeats:5/process_time/manual_time                 2.62 s          13.9 s             1    18.264u    24.878u    53.455u  0.0150808        69n        25M      26.2062n
DisruptorLatency<MPMCSpinsSharing>/4/1/100000000/32/iterations:1/repeats:5/process_time/manual_time                 2.27 s          12.1 s             1      18.1u    20.084u    52.087u   3.29571m        68n        25M      22.7498n
DisruptorLatency<MPMCSpinsSharing>/4/1/100000000/32/iterations:1/repeats:5/process_time/manual_time                 2.95 s          15.5 s             1    18.374u    50.574u    60.596u  0.0109504        69n        25M      29.4771n
DisruptorLatency<MPMCSpinsSharing>/4/1/100000000/32/iterations:1/repeats:5/process_time/manual_time                 2.58 s          14.3 s             1    18.157u     48.42u    54.874u   1.77409m        72n        25M      25.7666n
DisruptorLatency<MPMCSpinsSharing>/4/1/100000000/32/iterations:1/repeats:5/process_time/manual_time_mean            2.61 s          14.0 s             5   18.2092u   34.5516u   54.6706u   7.02559m      69.8n        25M      26.0671n
DisruptorLatency<MPMCSpinsSharing>/4/1/100000000/32/iterations:1/repeats:5/process_time/manual_time_median          2.61 s          13.9 s             5    18.157u    28.802u    53.455u   4.02694m        69n        25M      26.1356n
DisruptorLatency<MPMCSpinsSharing>/4/1/100000000/32/iterations:1/repeats:5/process_time/manual_time_stddev         0.238 s          1.24 s             5   109.771n   14.0089u    3.4904u    5.7178m   1.64317n    0.20825      2.38469n
DisruptorLatency<MPMCSpinsSharing>/4/1/100000000/32/iterations:1/repeats:5/process_time/manual_time_cv              9.15 %          8.85 %             5      0.60%     40.54%      6.38%     81.39%      2.35%      0.00%         9.15%
Reader batch sizes, top 8 of 4526:
1): 32 item batch,  4.1767%, 2610418 times
2): 736 item batch,  4.0764%, 110771 times
3): 64 item batch,  3.6406%, 1137703 times
4): 768 item batch,  2.8739%, 74841 times
5): 704 item batch,  2.8422%, 80745 times
6): 96 item batch,  0.9476%, 197407 times
7): 672 item batch,  0.8873%, 26407 times
8): 530176 item batch,  0.8218%, 31 times
Reader batch sizes, top 8 of 4454:
1): 736 item batch,  8.2478%, 224125 times
2): 704 item batch,  5.9748%, 169738 times
3): 768 item batch,  5.6427%, 146945 times
4): 32 item batch,  2.7101%, 1693799 times
5): 64 item batch,  2.3350%, 729686 times
6): 672 item batch,  1.9245%, 57276 times
7): 800 item batch,  1.3206%, 33014 times
8): 640 item batch,  0.8431%, 26348 times
Reader batch sizes, top 8 of 4428:
1): 736 item batch,  6.9030%, 187581 times
2): 768 item batch,  5.3083%, 138238 times
3): 704 item batch,  4.4576%, 126635 times
4): 672 item batch,  1.3684%, 40726 times
5): 800 item batch,  1.2582%, 31456 times
6): 640 item batch,  0.5642%, 17632 times
7): 545536 item batch,  0.5455%, 20 times
8): 540000 item batch,  0.5130%, 19 times
Reader batch sizes, top 8 of 4459:
1): 736 item batch,  8.4155%, 228682 times
2): 704 item batch,  6.0134%, 170835 times
3): 768 item batch,  5.6323%, 146675 times
4): 32 item batch,  1.7521%, 1095076 times
5): 672 item batch,  1.6568%, 49309 times
6): 64 item batch,  1.4526%, 453927 times
7): 800 item batch,  1.2814%, 32035 times
8): 511040 item batch,  0.7155%, 28 times
Reader batch sizes, top 8 of 4537:
1): 736 item batch,  9.3446%, 253930 times
2): 704 item batch,  6.3247%, 179679 times
3): 768 item batch,  5.9360%, 154583 times
4): 32 item batch,  4.7501%, 2968817 times
5): 64 item batch,  4.2997%, 1343649 times
6): 672 item batch,  1.6975%, 50522 times
7): 800 item batch,  1.2138%, 30346 times
8): 96 item batch,  0.9079%, 189153 times
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
Benchmark                                                                                                              Time             CPU   Iterations     Pub x1     Sub x4 bytes_per_second items_per_second per_item(avg)
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
DisruptorThroughput<SPMCSpinsAtomics>/1/4/500000000/32/iterations:1/repeats:5/process_time/manual_time              2.81 s          13.7 s             1       500M       500M      10.6214Gi/s       178.198M/s      5.61173n
DisruptorThroughput<SPMCSpinsAtomics>/1/4/500000000/32/iterations:1/repeats:5/process_time/manual_time              2.64 s          13.0 s             1       500M       500M      11.2828Gi/s       189.293M/s      5.28281n
DisruptorThroughput<SPMCSpinsAtomics>/1/4/500000000/32/iterations:1/repeats:5/process_time/manual_time              2.65 s          13.0 s             1       500M       500M      11.2509Gi/s       188.759M/s      5.29777n
DisruptorThroughput<SPMCSpinsAtomics>/1/4/500000000/32/iterations:1/repeats:5/process_time/manual_time              2.63 s          12.9 s             1       500M       500M      11.3158Gi/s       189.847M/s      5.26739n
DisruptorThroughput<SPMCSpinsAtomics>/1/4/500000000/32/iterations:1/repeats:5/process_time/manual_time              2.69 s          13.2 s             1       500M       500M      11.0834Gi/s       185.949M/s      5.37782n
DisruptorThroughput<SPMCSpinsAtomics>/1/4/500000000/32/iterations:1/repeats:5/process_time/manual_time_mean         2.68 s          13.2 s             5       500M       500M      11.1109Gi/s       186.409M/s       5.3675n
DisruptorThroughput<SPMCSpinsAtomics>/1/4/500000000/32/iterations:1/repeats:5/process_time/manual_time_median       2.65 s          13.0 s             5       500M       500M      11.2509Gi/s       188.759M/s      5.29777n
DisruptorThroughput<SPMCSpinsAtomics>/1/4/500000000/32/iterations:1/repeats:5/process_time/manual_time_stddev      0.072 s         0.297 s             5      4.165      4.165      294.763Mi/s        4.8294M/s      143.015p
DisruptorThroughput<SPMCSpinsAtomics>/1/4/500000000/32/iterations:1/repeats:5/process_time/manual_time_cv           2.66 %          2.25 %             5      0.00%      0.00%            2.59%            2.59%         2.66%
Reader batch sizes, top 8 of 3841:
1): 768 item batch,  8.4588%, 220281 times
2): 736 item batch,  7.0356%, 191184 times
3): 800 item batch,  3.8331%, 95827 times
4): 704 item batch,  2.8809%, 81845 times
5): 534176 item batch,  1.1752%, 44 times
6): 527328 item batch,  1.1074%, 42 times
7): 514368 item batch,  1.1059%, 43 times
8): 521216 item batch,  1.0424%, 40 times
Reader batch sizes, top 8 of 4513:
1): 768 item batch,  6.7796%, 176552 times
2): 736 item batch,  5.8031%, 157693 times
3): 800 item batch,  2.9445%, 73612 times
4): 704 item batch,  2.2984%, 65295 times
5): 64 item batch,  0.7613%, 237899 times
6): 525088 item batch,  0.6564%, 25 times
7): 523456 item batch,  0.6543%, 25 times
8): 672 item batch,  0.6327%, 18829 times
Reader batch sizes, top 8 of 4024:
1): 768 item batch,  8.1923%, 213340 times
2): 736 item batch,  7.6941%, 209078 times
3): 800 item batch,  3.4410%, 86025 times
4): 704 item batch,  3.3273%, 94525 times
5): 64 item batch,  1.3713%, 428530 times
6): 128 item batch,  1.1205%, 175076 times
7): 96 item batch,  0.9783%, 203821 times
8): 672 item batch,  0.9392%, 27952 times
Reader batch sizes, top 8 of 4644:
1): 768 item batch,  5.9593%, 155190 times
2): 736 item batch,  5.3490%, 145353 times
3): 800 item batch,  2.5952%, 64881 times
4): 704 item batch,  2.2451%, 63781 times
5): 528736 item batch,  0.8724%, 33 times
6): 519808 item batch,  0.8317%, 32 times
7): 518592 item batch,  0.7001%, 27 times
8): 529952 item batch,  0.6889%, 26 times
Reader batch sizes, top 8 of 4376:
1): 768 item batch,  7.5147%, 195696 times
2): 736 item batch,  6.6186%, 179854 times
3): 800 item batch,  3.1532%, 78829 times
4): 704 item batch,  2.4945%, 70867 times
5): 64 item batch,  0.9346%, 292047 times
6): 672 item batch,  0.6039%, 17973 times
7): 96 item batch,  0.5489%, 114358 times
8): 128 item batch,  0.5350%, 83592 times
DisruptorThroughput<SPMCSpinsSharing>/1/4/500000000/32/iterations:1/repeats:5/process_time/manual_time              2.64 s          13.0 s             1       500M       500M       11.272Gi/s       189.113M/s      5.28784n
DisruptorThroughput<SPMCSpinsSharing>/1/4/500000000/32/iterations:1/repeats:5/process_time/manual_time              2.66 s          13.1 s             1       500M       500M      11.2054Gi/s       187.996M/s      5.31927n
DisruptorThroughput<SPMCSpinsSharing>/1/4/500000000/32/iterations:1/repeats:5/process_time/manual_time              2.65 s          13.0 s             1       500M       500M      11.2454Gi/s       188.666M/s      5.30038n
DisruptorThroughput<SPMCSpinsSharing>/1/4/500000000/32/iterations:1/repeats:5/process_time/manual_time              2.66 s          13.1 s             1       500M       500M      11.2032Gi/s       187.958M/s      5.32034n
DisruptorThroughput<SPMCSpinsSharing>/1/4/500000000/32/iterations:1/repeats:5/process_time/manual_time              2.64 s          13.0 s             1       500M       500M      11.2945Gi/s       189.491M/s       5.2773n
DisruptorThroughput<SPMCSpinsSharing>/1/4/500000000/32/iterations:1/repeats:5/process_time/manual_time_mean         2.65 s          13.0 s             5       500M       500M      11.2441Gi/s       188.645M/s      5.30103n
DisruptorThroughput<SPMCSpinsSharing>/1/4/500000000/32/iterations:1/repeats:5/process_time/manual_time_median       2.65 s          13.0 s             5       500M       500M      11.2454Gi/s       188.666M/s      5.30038n
DisruptorThroughput<SPMCSpinsSharing>/1/4/500000000/32/iterations:1/repeats:5/process_time/manual_time_stddev      0.009 s         0.051 s             5      4.165      4.165      41.2658Mi/s       676.099k/s      18.9928p
DisruptorThroughput<SPMCSpinsSharing>/1/4/500000000/32/iterations:1/repeats:5/process_time/manual_time_cv           0.36 %          0.39 %             5      0.00%      0.00%            0.36%            0.36%         0.36%
Reader batch sizes, top 8 of 8:
1): 8 item batch, 99.6571%, 249142688 times
2): 4 item batch,  0.1030%, 515203 times
3): 3 item batch,  0.0788%, 525619 times
4): 5 item batch,  0.0746%, 298235 times
5): 7 item batch,  0.0273%, 78133 times
6): 2 item batch,  0.0263%, 262774 times
7): 1 item batch,  0.0206%, 411707 times
8): 6 item batch,  0.0123%, 40911 times
Reader batch sizes, top 8 of 8:
1): 8 item batch, 99.3445%, 248361146 times
2): 5 item batch,  0.1519%, 607447 times
3): 7 item batch,  0.1389%, 396881 times
4): 6 item batch,  0.1144%, 381176 times
5): 3 item batch,  0.1039%, 692630 times
6): 2 item batch,  0.0567%, 567007 times
7): 4 item batch,  0.0531%, 265704 times
8): 1 item batch,  0.0367%, 733654 times
Reader batch sizes, top 8 of 8:
1): 8 item batch, 99.5314%, 248828614 times
2): 5 item batch,  0.1043%, 417362 times
3): 7 item batch,  0.0918%, 262307 times
4): 3 item batch,  0.0783%, 521694 times
5): 6 item batch,  0.0734%, 244748 times
6): 4 item batch,  0.0451%, 225571 times
7): 2 item batch,  0.0444%, 444123 times
8): 1 item batch,  0.0312%, 624029 times
Reader batch sizes, top 8 of 8:
1): 8 item batch, 99.3000%, 248249881 times
2): 5 item batch,  0.1587%, 634666 times
3): 7 item batch,  0.1471%, 420403 times
4): 6 item batch,  0.1202%, 400517 times
5): 3 item batch,  0.1108%, 738713 times
6): 2 item batch,  0.0624%, 624300 times
7): 4 item batch,  0.0600%, 299803 times
8): 1 item batch,  0.0409%, 817748 times
Reader batch sizes, top 8 of 8:
1): 8 item batch, 99.3922%, 248480436 times
2): 5 item batch,  0.1356%, 542261 times
3): 7 item batch,  0.1238%, 353742 times
4): 6 item batch,  0.1020%, 340105 times
5): 3 item batch,  0.0968%, 645056 times
6): 2 item batch,  0.0573%, 573121 times
7): 4 item batch,  0.0539%, 269657 times
8): 1 item batch,  0.0384%, 768345 times
DisruptorThroughput<MPMCSpinsAtomics>/1/4/500000000/32/iterations:1/repeats:5/process_time/manual_time              7.52 s          37.0 s             1       500M       500M      3.96497Gi/s       66.5212M/s      15.0328n
DisruptorThroughput<MPMCSpinsAtomics>/1/4/500000000/32/iterations:1/repeats:5/process_time/manual_time              7.05 s          34.7 s             1       500M       500M      4.22896Gi/s       70.9502M/s      14.0944n
DisruptorThroughput<MPMCSpinsAtomics>/1/4/500000000/32/iterations:1/repeats:5/process_time/manual_time              7.05 s          34.7 s             1       500M       500M      4.22704Gi/s        70.918M/s      14.1008n
DisruptorThroughput<MPMCSpinsAtomics>/1/4/500000000/32/iterations:1/repeats:5/process_time/manual_time              7.04 s          34.6 s             1       500M       500M      4.23587Gi/s       71.0661M/s      14.0714n
DisruptorThroughput<MPMCSpinsAtomics>/1/4/500000000/32/iterations:1/repeats:5/process_time/manual_time              7.09 s          34.8 s             1       500M       500M      4.20548Gi/s       70.5562M/s      14.1731n
DisruptorThroughput<MPMCSpinsAtomics>/1/4/500000000/32/iterations:1/repeats:5/process_time/manual_time_mean         7.15 s          35.1 s             5       500M       500M      4.17247Gi/s       70.0024M/s      14.2945n
DisruptorThroughput<MPMCSpinsAtomics>/1/4/500000000/32/iterations:1/repeats:5/process_time/manual_time_median       7.05 s          34.7 s             5       500M       500M      4.22704Gi/s        70.918M/s      14.1008n
DisruptorThroughput<MPMCSpinsAtomics>/1/4/500000000/32/iterations:1/repeats:5/process_time/manual_time_stddev      0.207 s          1.02 s             5      4.165      4.165      119.344Mi/s       1.95534M/s      414.474p
DisruptorThroughput<MPMCSpinsAtomics>/1/4/500000000/32/iterations:1/repeats:5/process_time/manual_time_cv           2.90 %          2.92 %             5      0.00%      0.00%            2.79%            2.79%         2.90%
Reader batch sizes, top 8 of 8:
1): 8 item batch, 99.9919%, 62494920 times
2): 5 item batch,  0.0018%, 1799 times
3): 7 item batch,  0.0017%, 1226 times
4): 6 item batch,  0.0016%, 1324 times
5): 3 item batch,  0.0013%, 2125 times
6): 2 item batch,  0.0008%, 1878 times
7): 1 item batch,  0.0006%, 2912 times
8): 4 item batch,  0.0004%, 519 times
Reader batch sizes, top 8 of 8:
1): 8 item batch, 99.9960%, 62497487 times
2): 5 item batch,  0.0008%, 834 times
3): 7 item batch,  0.0007%, 526 times
4): 3 item batch,  0.0007%, 1138 times
5): 6 item batch,  0.0006%, 522 times
6): 2 item batch,  0.0004%, 1038 times
7): 4 item batch,  0.0004%, 467 times
8): 1 item batch,  0.0004%, 1762 times
Reader batch sizes, top 8 of 8:
1): 8 item batch, 99.9954%, 62497109 times
2): 5 item batch,  0.0011%, 1108 times
3): 7 item batch,  0.0010%, 691 times
4): 6 item batch,  0.0009%, 735 times
5): 3 item batch,  0.0008%, 1283 times
6): 2 item batch,  0.0004%, 978 times
7): 1 item batch,  0.0003%, 1428 times
8): 4 item batch,  0.0002%, 277 times
Reader batch sizes, top 8 of 8:
1): 8 item batch, 99.9990%, 62499360 times
2): 5 item batch,  0.0002%, 243 times
3): 7 item batch,  0.0002%, 158 times
4): 6 item batch,  0.0002%, 161 times
5): 3 item batch,  0.0002%, 282 times
6): 2 item batch,  0.0001%, 196 times
7): 1 item batch,  0.0001%, 347 times
8): 4 item batch,  0.0000%, 62 times
Reader batch sizes, top 8 of 8:
1): 8 item batch, 99.9985%, 62499066 times
2): 5 item batch,  0.0003%, 316 times
3): 7 item batch,  0.0003%, 207 times
4): 3 item batch,  0.0002%, 408 times
5): 6 item batch,  0.0002%, 200 times
6): 1 item batch,  0.0002%, 773 times
7): 2 item batch,  0.0001%, 345 times
8): 4 item batch,  0.0001%, 139 times
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
Benchmark                                                                                                              Time             CPU   Iterations     Pub x4     Sub x1 bytes_per_second items_per_second per_item(avg)
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
DisruptorThroughput<MPMCSpinsAtomics>/4/1/500000000/32/iterations:1/repeats:5/process_time/manual_time              2.98 s          14.6 s             1       125M       500M      10.0149Gi/s       168.023M/s      5.95157n
DisruptorThroughput<MPMCSpinsAtomics>/4/1/500000000/32/iterations:1/repeats:5/process_time/manual_time              3.02 s          14.8 s             1       125M       500M      9.86776Gi/s       165.553M/s      6.04034n
DisruptorThroughput<MPMCSpinsAtomics>/4/1/500000000/32/iterations:1/repeats:5/process_time/manual_time              3.01 s          14.8 s             1       125M       500M      9.89201Gi/s        165.96M/s      6.02553n
DisruptorThroughput<MPMCSpinsAtomics>/4/1/500000000/32/iterations:1/repeats:5/process_time/manual_time              3.06 s          15.0 s             1       125M       500M      9.73692Gi/s       163.358M/s      6.12151n
DisruptorThroughput<MPMCSpinsAtomics>/4/1/500000000/32/iterations:1/repeats:5/process_time/manual_time              2.99 s          14.7 s             1       125M       500M      9.95568Gi/s       167.029M/s        5.987n
DisruptorThroughput<MPMCSpinsAtomics>/4/1/500000000/32/iterations:1/repeats:5/process_time/manual_time_mean         3.01 s          14.8 s             5       125M       500M      9.89346Gi/s       165.985M/s      6.02519n
DisruptorThroughput<MPMCSpinsAtomics>/4/1/500000000/32/iterations:1/repeats:5/process_time/manual_time_median       3.01 s          14.8 s             5       125M       500M      9.89201Gi/s        165.96M/s      6.02553n
DisruptorThroughput<MPMCSpinsAtomics>/4/1/500000000/32/iterations:1/repeats:5/process_time/manual_time_stddev      0.032 s         0.157 s             5    1.04125      4.165      107.148Mi/s       1.75551M/s      64.0014p
DisruptorThroughput<MPMCSpinsAtomics>/4/1/500000000/32/iterations:1/repeats:5/process_time/manual_time_cv           1.06 %          1.06 %             5      0.00%      0.00%            1.06%            1.06%         1.06%
Reader batch sizes, top 8 of 8:
1): 8 item batch, 99.5906%, 248976466 times
2): 5 item batch,  0.0983%, 393270 times
3): 7 item batch,  0.0865%, 247088 times
4): 6 item batch,  0.0696%, 232153 times
5): 3 item batch,  0.0667%, 444594 times
6): 2 item batch,  0.0345%, 345466 times
7): 4 item batch,  0.0309%, 154438 times
8): 1 item batch,  0.0228%, 456922 times
Reader batch sizes, top 8 of 8:
1): 8 item batch, 99.4713%, 248678300 times
2): 5 item batch,  0.1225%, 490057 times
3): 7 item batch,  0.1063%, 303651 times
4): 6 item batch,  0.0883%, 294226 times
5): 3 item batch,  0.0870%, 579796 times
6): 2 item batch,  0.0481%, 481169 times
7): 4 item batch,  0.0442%, 221047 times
8): 1 item batch,  0.0323%, 646488 times
Reader batch sizes, top 8 of 8:
1): 8 item batch, 99.2057%, 248014249 times
2): 5 item batch,  0.1984%, 793634 times
3): 7 item batch,  0.1623%, 463687 times
4): 3 item batch,  0.1328%, 885197 times
5): 6 item batch,  0.1300%, 433381 times
6): 2 item batch,  0.0652%, 651736 times
7): 4 item batch,  0.0615%, 307388 times
8): 1 item batch,  0.0442%, 883128 times
Reader batch sizes, top 8 of 8:
1): 8 item batch, 99.5104%, 248776010 times
2): 5 item batch,  0.1093%, 437196 times
3): 7 item batch,  0.0966%, 276033 times
4): 3 item batch,  0.0802%, 534577 times
5): 6 item batch,  0.0799%, 266423 times
6): 4 item batch,  0.0465%, 232290 times
7): 2 item batch,  0.0464%, 463807 times
8): 1 item batch,  0.0307%, 614666 times
Reader batch sizes, top 8 of 8:
1): 8 item batch, 99.5235%, 248808669 times
2): 5 item batch,  0.1107%, 442871 times
3): 7 item batch,  0.0941%, 268717 times
4): 3 item batch,  0.0791%, 527140 times
5): 6 item batch,  0.0767%, 255698 times
6): 4 item batch,  0.0444%, 221973 times
7): 2 item batch,  0.0433%, 432919 times
8): 1 item batch,  0.0283%, 565936 times
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
Benchmark                                                                                                              Time             CPU   Iterations     Pub x1     Sub x4 bytes_per_second items_per_second per_item(avg)
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
DisruptorThroughput<MPMCSpinsSharing>/1/4/500000000/32/iterations:1/repeats:5/process_time/manual_time              7.38 s          36.3 s             1       500M       500M      4.03807Gi/s       67.7477M/s      14.7607n
DisruptorThroughput<MPMCSpinsSharing>/1/4/500000000/32/iterations:1/repeats:5/process_time/manual_time              7.14 s          35.1 s             1       500M       500M      4.17344Gi/s       70.0187M/s      14.2819n
DisruptorThroughput<MPMCSpinsSharing>/1/4/500000000/32/iterations:1/repeats:5/process_time/manual_time              7.10 s          34.9 s             1       500M       500M      4.19871Gi/s       70.4427M/s      14.1959n
DisruptorThroughput<MPMCSpinsSharing>/1/4/500000000/32/iterations:1/repeats:5/process_time/manual_time              7.42 s          36.5 s             1       500M       500M      4.01902Gi/s       67.4279M/s      14.8307n
DisruptorThroughput<MPMCSpinsSharing>/1/4/500000000/32/iterations:1/repeats:5/process_time/manual_time              7.38 s          36.4 s             1       500M       500M        4.039Gi/s       67.7631M/s      14.7573n
DisruptorThroughput<MPMCSpinsSharing>/1/4/500000000/32/iterations:1/repeats:5/process_time/manual_time_mean         7.28 s          35.8 s             5       500M       500M      4.09365Gi/s         68.68M/s      14.5653n
DisruptorThroughput<MPMCSpinsSharing>/1/4/500000000/32/iterations:1/repeats:5/process_time/manual_time_median       7.38 s          36.3 s             5       500M       500M        4.039Gi/s       67.7631M/s      14.7573n
DisruptorThroughput<MPMCSpinsSharing>/1/4/500000000/32/iterations:1/repeats:5/process_time/manual_time_stddev      0.150 s         0.756 s             5      4.165      4.165      87.2656Mi/s       1.42976M/s      300.908p
DisruptorThroughput<MPMCSpinsSharing>/1/4/500000000/32/iterations:1/repeats:5/process_time/manual_time_cv           2.07 %          2.11 %             5      0.00%      0.00%            2.08%            2.08%         2.07%
Reader batch sizes, top 8 of 8:
1): 8 item batch, 99.9986%, 62499095 times
2): 7 item batch,  0.0003%, 236 times
3): 5 item batch,  0.0003%, 260 times
4): 3 item batch,  0.0002%, 353 times
5): 6 item batch,  0.0002%, 165 times
6): 1 item batch,  0.0002%, 815 times
7): 2 item batch,  0.0002%, 388 times
8): 4 item batch,  0.0001%, 162 times
Reader batch sizes, top 8 of 8:
1): 8 item batch, 99.9569%, 62473081 times
2): 5 item batch,  0.0093%, 9317 times
3): 7 item batch,  0.0093%, 6609 times
4): 6 item batch,  0.0080%, 6706 times
5): 3 item batch,  0.0067%, 11140 times
6): 2 item batch,  0.0040%, 9925 times
7): 1 item batch,  0.0033%, 16366 times
8): 4 item batch,  0.0025%, 3158 times
Reader batch sizes, top 8 of 8:
1): 8 item batch, 99.9978%, 62498625 times
2): 7 item batch,  0.0005%, 345 times
3): 5 item batch,  0.0005%, 479 times
4): 6 item batch,  0.0004%, 372 times
5): 3 item batch,  0.0003%, 575 times
6): 2 item batch,  0.0002%, 499 times
7): 1 item batch,  0.0001%, 691 times
8): 4 item batch,  0.0001%, 136 times
Reader batch sizes, top 8 of 8:
1): 8 item batch, 99.9768%, 62485506 times
2): 5 item batch,  0.0044%, 4430 times
3): 7 item batch,  0.0042%, 2993 times
4): 6 item batch,  0.0041%, 3403 times
5): 3 item batch,  0.0034%, 5688 times
6): 2 item batch,  0.0027%, 6750 times
7): 1 item batch,  0.0023%, 11477 times
8): 4 item batch,  0.0021%, 2598 times
Reader batch sizes, top 8 of 8:
1): 8 item batch, 99.9884%, 62492748 times
2): 7 item batch,  0.0022%, 1557 times
3): 5 item batch,  0.0021%, 2075 times
4): 6 item batch,  0.0020%, 1676 times
5): 3 item batch,  0.0017%, 2798 times
6): 2 item batch,  0.0014%, 3514 times
7): 1 item batch,  0.0012%, 5992 times
8): 4 item batch,  0.0011%, 1318 times
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
Benchmark                                                                                                              Time             CPU   Iterations     Pub x4     Sub x1 bytes_per_second items_per_second per_item(avg)
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
DisruptorThroughput<MPMCSpinsSharing>/4/1/500000000/32/iterations:1/repeats:5/process_time/manual_time              3.09 s          15.2 s             1       125M       500M      9.63559Gi/s       161.658M/s      6.18588n
DisruptorThroughput<MPMCSpinsSharing>/4/1/500000000/32/iterations:1/repeats:5/process_time/manual_time              3.18 s          15.6 s             1       125M       500M      9.37906Gi/s       157.354M/s      6.35508n
DisruptorThroughput<MPMCSpinsSharing>/4/1/500000000/32/iterations:1/repeats:5/process_time/manual_time              3.16 s          15.5 s             1       125M       500M      9.43276Gi/s       158.255M/s       6.3189n
DisruptorThroughput<MPMCSpinsSharing>/4/1/500000000/32/iterations:1/repeats:5/process_time/manual_time              3.16 s          15.5 s             1       125M       500M      9.43433Gi/s       158.282M/s      6.31785n
DisruptorThroughput<MPMCSpinsSharing>/4/1/500000000/32/iterations:1/repeats:5/process_time/manual_time              3.21 s          15.8 s             1       125M       500M      9.29507Gi/s       155.945M/s       6.4125n
DisruptorThroughput<MPMCSpinsSharing>/4/1/500000000/32/iterations:1/repeats:5/process_time/manual_time_mean         3.16 s          15.5 s             5       125M       500M      9.43536Gi/s       158.299M/s      6.31804n
DisruptorThroughput<MPMCSpinsSharing>/4/1/500000000/32/iterations:1/repeats:5/process_time/manual_time_median       3.16 s          15.5 s             5       125M       500M      9.43276Gi/s       158.255M/s       6.3189n
DisruptorThroughput<MPMCSpinsSharing>/4/1/500000000/32/iterations:1/repeats:5/process_time/manual_time_stddev      0.042 s         0.227 s             5    1.04125      4.165      128.463Mi/s       2.10474M/s      83.3087p
DisruptorThroughput<MPMCSpinsSharing>/4/1/500000000/32/iterations:1/repeats:5/process_time/manual_time_cv           1.32 %          1.46 %             5      0.00%      0.00%            1.33%            1.33%         1.32%

real	13m48.142s
user	49m19.298s
sys	1m3.041s
```

</details>

</details>
