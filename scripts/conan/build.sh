#!/bin/bash -el
# Build project and 3rd party libs using Type, Compiler and Profiles in Dir

Root=$(realpath $(dirname $(realpath $0))/../..)

Type=${1:-Release}
Compiler=${2:-g++}
Opts=(`echo ${3} | tr ',' ' '`)
# Major compiler version
Version=$($Compiler --version | sed -nE "s/^.*(clang version|g\+\+.*)\s([0-9]+)\..*$/\2/p")
# Convert C++ compiler to relevant C compiler
CCompiler=$(echo $Compiler | sed -E "s/clang\+\+/clang/; s/g\+\+/gcc/")
# Just clang or gcc for Conan
ConanCompiler=$(echo $Compiler | sed -E "s/^.*clang\+\+.*$/clang/; s/^.*g\+\+.*$/gcc/")

Names=$(printf -- "-%s" ${Type} ${ConanCompiler} "${Opts[@]}")
ProfilesBuild=$(printf -- "--profile:build ${Root}/scripts/conan/%s " common "${Opts[@]}")
ProfilesHost=$(printf  -- "--profile:host  ${Root}/scripts/conan/%s " common "${Opts[@]}")
OSID=$(grep -oP '(?<=^ID=).+' /etc/os-release)
Dir=${4:-$(echo build-${OSID}${Names} | tr '[:upper:]' '[:lower:]')}
Command=${5:-build} # conan install|build

echo $Type CC=$CCompiler CXX=$Compiler Conan:$ConanCompiler $Version Build:$(pwd)/$Dir

export CC=$CCompiler
export CXX=$Compiler

export CONAN_HOME=$(pwd)/$Dir/.conan2 # place for .conan2 with caches, profiles and packages

conan config install ${Root}/scripts/conan/settings_user.yml

conan $Command $Root                \
    -of $Dir                        \
    --build=missing                 \
    --update                        \
    -s build_type=$Type             \
    -s compiler=$ConanCompiler      \
    -s compiler.version=$Version    \
    -s compiler.cppstd=20           \
    -s compiler.libcxx=libstdc++11  \
    $ProfilesHost $ProfilesBuild    \
