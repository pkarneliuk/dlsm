# DLSM

[![build](https://github.com/pkarneliuk/dlsm/actions/workflows/ci.yml/badge.svg)](https://github.com/pkarneliuk/dlsm/actions/workflows/ci.yml)
[![coverage](https://codecov.io/gh/pkarneliuk/dlsm/branch/master/graph/badge.svg)](https://codecov.io/gh/pkarneliuk/dlsm)

## Introduction
This project contains scripts and snippets in C++.

## Features
 - Build in [Docker](https://www.docker.com/resources/what-container/) containers(Fedora, Ubuntu) or in current environment
 - Build by GCC and Clang C++ compilers
 - Build 3rd-party libraries by [Conan](https://docs.conan.io/en/latest/introduction.html)
 - Build C++ code coverage HTML report by [LCOV](https://github.com/linux-test-project/lcov) with lines/functions/branches metrics
 - Build with Thread/Address/UndefinedBehavior [Sanitizer](https://github.com/google/sanitizers)
 - Integration with [clang-format](https://clang.llvm.org/docs/ClangFormat.html) and [clang-tidy](https://clang.llvm.org/extra/clang-tidy/)
 - Integration with [Doxygen](https://github.com/doxygen/doxygen)

## Disruptor
Yet another implementation of [LMAX Disruptor](https://lmax-exchange.github.io/disruptor/disruptor.html) in `C++20`.
<details><summary>Details</summary>

#### Other known implementations
 - Original [LMAX Disruptor](https://github.com/LMAX-Exchange/disruptor) in `Java`
 - [Abc-Arbitrage/Disruptor-cpp](https://github.com/Abc-Arbitrage/Disruptor-cpp)
 - [lewissbaker/disruptorplus](https://github.com/lewissbaker/disruptorplus)
 - [Vallest/Disruptor-CPP](https://github.com/Vallest/Disruptor-CPP)
 - [jeremyko/disruptorCpp-IPC](https://github.com/jeremyko/disruptorCpp-IPC)

#### Features of [dlsm::Disruptor](include/impl/Disruptor.hpp)
 - Template-based implementation with different components for customization:
   - Barriers::
     - PointerBarrier - minimal container for dependencies of a sequence
     - AtomicsBarrier - `std::atomic` pointers to dependencies
     - OffsetsBarrier - `std::atomic` offsets to dependencies for placing in shared memory
   - Waits::
     - SpinsStrategy - busy-wait based on exponential `__x86_64__` `_mm_pause()` intrinsic
     - YieldStrategy - busy-wait based on `std::this_thread::yield()`
     - BlockStrategy - blocking strategy based on `std::std::condition_variable_any`
     - ShareStrategy - blocking strategy based on `pthreads` for placing in shared memory
   - Sequencers::
     - `SPMC` - Single Producer Multiple Consumers pattern
     - `MPMC` - Multiple Producers Multiple Consumers pattern
   - Ring - adapter for external random-access container(`std::array/vector`) for ring-access to Events
 - External memory injection(optional) for sequencers, useful for placement in HugePages/SharedMemory
 - [Unit](tests/unit/TestDisruptor.cpp) and [Performance(latency&throughput)](tests/perf/PerfDisruptor.cpp) tests
 - [dlsm::Disruptor::Graph](include/impl/DisruptorGraph.hpp) - high-level API

#### Known defects and limitations
 - Implementation of lock-free operations in not portable to `Weak Memory Model` platforms(ARM, PowerPC)
 - `Claim-Timeout`/`Consume-Timeout` operations are not implemented in Sequencers(Publishers and Consumers)
 - `SPSC` - Single Producer Single Consumer pattern is not implemented
 - dlsm::Disruptor::Graph has high overhead caused by indirections and virtual calls
 - dlsm::Disruptor::Graph is incomplete and unstable

#### Latency & Throughput tests
Results of performance tests are in separate [tests/perf/Disruptor.md](tests/perf/Disruptor.md).

</details>

## Useful Scripts
```sh
./scripts/format.sh   # Apply .clang-format rules on hpp/cpp files
./scripts/build.sh    # Perform all types of builds in current environment
./scripts/conan/build.sh Debug g++ coverage    # Build Debug by g++ with code coverage
./scripts/conan/build.sh Release clang++ tsan  # Build Release by clang++ with ThreadSanitizer
./scripts/docker/run.sh Ubuntu                     # Start Ubuntu.Dockerfile container in interactive mode
./scripts/docker/run.sh Fedora ./scripts/build.sh  # Perform all types of builds in Docker container
./scripts/docker/run.sh Fedora cat /etc/os-release # Perform command in Fedora.Dockerfile container
./scripts/docker/run.sh Ubuntu ./scripts/conan/build.sh Release g++ tsan ./build-ubuntu-tsan
./scripts/docker/run.sh Ubuntu ./scripts/conan/build.sh RelWithDebInfo g++ common ./build
```

## Links and References
 - [Measuring Latency in Linux](http://btorpey.github.io/blog/2014/02/18/clock-sources-in-linux/)
 - [Weak vs. Strong Memory Models](https://preshing.com/20120930/weak-vs-strong-memory-models/)
 - [Fast and/or Large Memory – Cache and Memory Hierarchy](https://cw.fel.cvut.cz/b192/_media/courses/b35apo/en/lectures/04/b35apo_lecture04-cache-en.pdf)
