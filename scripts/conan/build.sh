#!/bin/bash -el
# Build project and 3rd party libs using Type, Compiler and Profiles in Dir

Root=$(realpath $(dirname $(realpath $0))/../..)

Type=${1:-Release}
Compiler=${2:-g++}
Opts=(`echo ${3} | tr ',' ' '`)
# Major compiler version
Version=$($Compiler --version | sed -nE "s/^.*(clang version|g\+\+.*)\s([0-9]+).*$/\2/p")
# Convert C++ compiler to relevant C compiler
CCompiler=$(echo $Compiler | sed -E "s/clang\+\+/clang/; s/g\+\+/gcc/")
# Just clang or gcc for Conan
ConanCompiler=$(echo $Compiler | sed -E "s/^.*clang\+\+.*$/clang/; s/^.*g\+\+.*$/gcc/")

Names=$(printf -- "-%s" ${Type} ${ConanCompiler} "${Opts[@]}")
Profiles=$(printf -- "--profile ${Root}/scripts/conan/%s " common "${Opts[@]}")
Dir=${4:-$(echo build${Names} | tr '[:upper:]' '[:lower:]')}

echo $Type CC=$CCompiler CXX=$Compiler Conan:$ConanCompiler $Version Build:$(pwd)/$Dir

export CC=$CCompiler
export CXX=$Compiler

export CONAN_USER_HOME=$(pwd)/$Dir # place for .conan with caches, profiles and packages

conan install $Root                 \
    --install-folder $Dir           \
    --build=missing                 \
    --update                        \
    -s os=Linux                     \
    -s arch=x86_64                  \
    -s build_type=$Type             \
    -s compiler=$ConanCompiler      \
    -s compiler.version=$Version    \
    -s compiler.libcxx=libstdc++11  \
    $Profiles                       \

conan build $Root                   \
    --build-folder $Dir             \
    --configure                     \
    --build                         \
    --test                          \
    --install                       \
