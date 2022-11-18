FROM fedora:36

ARG UNAME=user
ARG UID=1000
ARG GID=1000

RUN groupadd -g $GID -o $UNAME  &&  \
    useradd  -g $GID -u $UID -m -o -s /bin/bash $UNAME

RUN dnf -y update && dnf -y install \
    cmake                           \
    doxygen                         \
    gcc                             \
    glibc-static libstdc++-static   \
    clang clang-tools-extra         \
    lcov llvm                       \
    python3-pip                     \
    perl                            \
    && dnf clean all

RUN pip install                     \
    conan==1.54.0
