FROM ubuntu:24.04

ARG UNAME=user
ARG UID=1000
ARG GID=1000

RUN userdel --remove ubuntu
RUN groupadd -g $GID -o $UNAME  &&  \
    useradd  -g $GID -u $UID -m -o -s /bin/bash $UNAME

RUN apt-get -y update && apt-get -y install sudo && \
    echo "$UNAME ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

RUN apt-get -y update && apt-get -y install --no-install-recommends \
    make                                    \
    doxygen graphviz                        \
    g++-14                                  \
    clang-18 clang-format-18 clang-tidy-18  \
    lcov llvm-18 libclang-rt-dev            \
    python3-pip                             \
    && apt-get clean

RUN    ln -s $(which g++-14) /usr/local/bin/g++         \
    && ln -s $(which gcc-14) /usr/local/bin/gcc         \
    && ln -s $(which gcov-14) /usr/local/bin/gcov       \
    && ln -s $(which clang++-18) /usr/local/bin/clang++ \
    && ln -s $(which clang-18  ) /usr/local/bin/clang   \
    && ln -s $(which llvm-cov-18) /usr/local/bin/llvm-cov \
    && ln -s $(which clang-tidy-18) /usr/local/bin/clang-tidy

USER $UID:$GID
RUN pip install --break-system-packages --user  \
  conan==2.2.3 cmake==3.29.2
ENV PATH "$PATH:/home/$UNAME/.local/bin"
