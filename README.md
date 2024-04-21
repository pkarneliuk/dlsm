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
