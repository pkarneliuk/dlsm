#!/bin/bash -el

Root=$(realpath $(dirname $(realpath $0))/..)

find $Root/build-fedora* -mindepth 1 ! -regex '^.*/.conan2\(/.*\)?' -delete
find $Root/build-ubuntu* -mindepth 1 ! -regex '^.*/.conan2\(/.*\)?' -delete
