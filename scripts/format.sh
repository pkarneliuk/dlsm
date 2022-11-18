#!/bin/bash -el

Root=$(realpath $(dirname $(realpath $0))/..)

cd $Root && find . -type f | grep -E "^./(include|src|tests).*.(hpp|cpp)$" | xargs clang-format -i