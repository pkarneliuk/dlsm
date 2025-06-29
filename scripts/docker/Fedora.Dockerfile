FROM fedora:42

ARG UNAME=user
ARG UID=1000
ARG GID=1000

RUN groupadd -g $GID -o $UNAME  &&  \
    useradd  -g $GID -u $UID -m -o -s /bin/bash $UNAME

RUN dnf -y update && dnf -y install sudo && \
    echo "$UNAME ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

RUN dnf -y update && dnf -y install \
    doxygen                         \
    gcc                             \
    glibc-static libstdc++-static   \
    libasan-static                  \
    libtsan-static                  \
    libubsan-static                 \
    libatomic                       \
    clang clang-tools-extra         \
    lcov llvm                       \
    python3-pip                     \
    && dnf clean all

RUN pip install                     \
    conan==2.17.1 cmake==3.29.2
