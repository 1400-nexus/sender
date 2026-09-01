# Uniflow sender — Linux build and run environment.
#
# WHY THIS EXISTS
# ---------------
# The sender needs two things macOS does not have:
#
#   AF_UNIX SOCK_SEQPACKET   Darwin supports only STREAM and DGRAM for AF_UNIX;
#                            socket() fails at runtime. The IPC contract in the
#                            design brief cannot be implemented natively on a Mac.
#   sendmmsg(2)              Batched send. Without it: one syscall per packet,
#                            ~17,000/s instead of ~266/s.
#
# It is also where protobuf gets pinned. Everyone who builds in this image has
# the same protoc and the same libprotobuf, so the 7.36.0-vs-7.36.1 class of
# problem cannot happen again.
#
# NOTE ON PROTOBUF VERSIONS: this image ships Ubuntu's 3.21.12, not the 7.36.x
# on your Mac. That is fine, and not a compromise — the protobuf WIRE FORMAT is
# stable across versions, so a sender built here interoperates perfectly with a
# receiver built anywhere else. Only the generated C++ is version-specific, and
# CMake now generates that locally from the .proto sources.

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        ninja-build \
        pkg-config \
        git \
        gdb \
        protobuf-compiler \
        libprotobuf-dev \
        libisal-dev \
        iproute2 \
        tcpdump \
        iputils-ping \
        python3 \
        python3-protobuf \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# libisal-dev is here already so days 6–8 (Reed–Solomon) need no image change.
# iproute2 gives you `tc netem` for injecting the loss you are designing against.

WORKDIR /workspace

CMD ["/bin/bash"]
