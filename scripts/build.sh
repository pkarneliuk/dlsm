#!/bin/bash -el

Root=$(realpath $(dirname $(realpath $0))/..)

$Root/scripts/conan/build.sh Debug clang,coverage
$Root/scripts/conan/build.sh Debug   gcc,coverage

#$Root/scripts/conan/build.sh Release clang
#$Root/scripts/conan/build.sh Release gcc
