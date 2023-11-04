#!/bin/bash -el

Root=$(realpath $(dirname $(realpath $0))/..)

$Root/scripts/conan/build.sh Debug g++ "coverage asan"
$Root/scripts/conan/build.sh Debug clang++ usan
$Root/scripts/conan/build.sh Debug clang++ tsan

$Root/scripts/conan/build.sh Debug g++ "coverage asan"
$Root/scripts/conan/build.sh Debug g++ usan
$Root/scripts/conan/build.sh Debug g++ tsan

$Root/scripts/conan/build.sh Release clang++
$Root/scripts/conan/build.sh Release g++
