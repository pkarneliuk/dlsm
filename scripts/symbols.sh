#!/bin/bash -el

readelf -W --dyn-syms $1 | grep @GLIBC_ | tr '@' ' ' | sort -k 9 
ldd --version
ldd $1
