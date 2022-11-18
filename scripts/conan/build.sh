#!/bin/bash -el

Root=$(realpath $(dirname $(realpath $0))/../..)

Type=${1:-Release}
Opts=(`echo ${2:-gcc} | tr ',' ' '`)
Names=$(printf -- "-%s" "${Opts[@]}")
Profiles=$(printf -- "--profile ${Root}/scripts/conan/%s " common "${Opts[@]}")
Dir=${3:-$(echo build-${Type}${Names} | tr '[:upper:]' '[:lower:]')}

export CONAN_USER_HOME=$(pwd)/$Dir # place for .conan

conan install $Root                 \
    --install-folder $Dir           \
    --build=missing                 \
    --update                        \
    -s os=Linux                     \
    -s arch=x86_64                  \
    -s build_type=$Type             \
    $Profiles                       \

conan build $Root                   \
    --build-folder $Dir             \
    --configure                     \
    --build                         \
    --test                          \
    --install                       \
