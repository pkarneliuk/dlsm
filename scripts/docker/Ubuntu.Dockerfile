FROM ubuntu:22.04

ARG UNAME=user
ARG UID=1000
ARG GID=1000

RUN groupadd -g $GID -o $UNAME  &&  \
    useradd  -g $GID -u $UID -m -o -s /bin/bash $UNAME

RUN apt-get -y update && apt-get -y install sudo && \
    echo "$UNAME ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

RUN apt-get -y update && apt-get -y install \
    cmake                           \
    doxygen graphviz                \
    gcc                             \
    clang clang-format clang-tidy   \
    lcov llvm                       \
    python3-pip                     \
    && apt-get clean

RUN pip install                     \
    conan==1.59.0
