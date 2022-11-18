#!/bin/bash -el
# Run args or open interactive terminal inside Docker container

Root=$(realpath $(dirname $(realpath $0))/../..)

Dockerfile=$Root/scripts/docker/Ubuntu.Dockerfile
Tag=dls:$(basename $Dockerfile)
[[ $# == 0 ]] && Interactive="--interactive --tty"

docker build --tag $Tag --quiet \
    --build-arg UID=$(id -u)    \
    --build-arg GID=$(id -g)    \
    - < $Dockerfile
docker run --rm $Interactive    \
    --user $(id -u):$(id -g)    \
    --volume  $Root:/target     \
    --workdir /target           \
    $Tag $@