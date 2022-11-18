#!/bin/bash -el

readelf -W --dyn-syms --demangle $1 | grep dlsm
ldd --version
ldd $1
