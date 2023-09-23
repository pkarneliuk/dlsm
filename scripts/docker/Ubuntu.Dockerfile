FROM ubuntu:22.04

ARG UNAME=user
ARG UID=1000
ARG GID=1000

RUN groupadd -g $GID -o $UNAME  &&  \
    useradd  -g $GID -u $UID -m -o -s /bin/bash $UNAME

RUN apt-get -y update && apt-get -y install sudo && \
    echo "$UNAME ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

RUN apt-get -y install software-properties-common   \
    && add-apt-repository -y ppa:ubuntu-toolchain-r/test

RUN apt-get -y update && apt-get -y install --no-install-recommends \
    make                                    \
    doxygen graphviz                        \
    g++-13                                  \
    clang-15 clang-format clang-tidy        \
    lcov llvm                               \
    python3-pip                             \
    && apt-get clean

RUN    ln -s $(which g++-13) /usr/local/bin/g++         \
    && ln -s $(which gcc-13) /usr/local/bin/gcc         \
    && ln -s $(which gcov-13) /usr/local/bin/gcov       \
    && ln -s $(which clang++-15) /usr/local/bin/clang++ \
    && ln -s $(which clang-15  ) /usr/local/bin/clang

RUN pip install                     \
    conan==2.0.11 cmake==3.27.5
