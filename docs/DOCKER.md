# Linux dev container

## Why

The sender cannot be built or run natively on macOS. Two hard blockers:

| Needed | On Darwin |
|---|---|
| `AF_UNIX` + `SOCK_SEQPACKET` | Not supported. `socket()` fails at runtime — Darwin offers only `STREAM` and `DGRAM` for `AF_UNIX`. The IPC contract in the brief is unimplementable natively. |
| `sendmmsg(2)` | Does not exist. One syscall per packet: ~17,000/s instead of ~266/s. |

The container is also where protobuf gets pinned, so the 7.36.0-vs-7.36.1
mismatch cannot happen again.

## Use it

```bash
./dev.sh test      # configure + build + ctest, all inside Linux
./dev.sh           # interactive shell, repo mounted at /workspace
./dev.sh bash -lc "tc qdisc show dev eth0"
```

First run pulls the base image and takes a few minutes. After that it is cached.

## Build directories

Use **`build-linux/`** inside the container, not `cmake-build-debug/`.

A CMake cache records absolute paths to the compiler, the SDK and the protobuf
install. A cache written by macOS clang and one written by container gcc cannot
share a directory — you get confusing "compiler has changed" errors and stale
object files. One build directory per platform, both gitignored.

## CLion

Keep using the same run button:

1. **Settings → Build, Execution, Deployment → Toolchains → + → Docker**
2. Image: `nexus-sender-dev:latest` (run `./dev.sh test` once first so it exists)
3. **Settings → CMake →** add a profile with Toolchain `Docker`, build directory
   `build-linux`
4. Keep the macOS profile if you like — editing and indexing still work there,
   it just cannot run the sender.

Docker Desktop has to be running.

## About the protobuf version

This image ships Ubuntu's protobuf 3.21.12, not the 7.36.x on your Mac. That is
deliberate and costs nothing:

- The protobuf **wire format is stable across versions**. A sender built here
  interoperates perfectly with a receiver built against any other version.
- Only *generated C++* is version-specific, and CMake now generates it locally
  from the `.proto` sources, so it always matches whatever runtime is present.

If the team later wants one exact version everywhere, this Dockerfile is the
single place to pin it — that is the point of having it.

## Architecture

On Apple Silicon the image builds `arm64` natively, which is fast. If the lab
machines are x86 and you want to sanity-check before a demo:

```bash
docker build --platform=linux/amd64 -t nexus-sender-dev:amd64 -f Dockerfile .
```

That runs under emulation and is slow — fine for a correctness check, useless
for measuring throughput. Anything you intend to quote as a performance number
has to be measured on the real target hardware.

## What is in the image

| Package | Why |
|---|---|
| `build-essential` `cmake` `ninja-build` `gdb` `git` `pkg-config` | toolchain |
| `protobuf-compiler` `libprotobuf-dev` | schema codegen, generated locally |
| `libisal-dev` | Reed–Solomon. Already here so days 6–8 need no image change. |
| `iproute2` | `tc netem`, for injecting the loss you are designing against |
| `tcpdump` | to confirm frames never fragment |
| `python3` `python3-protobuf` | the fake `file_monitor` / receiver harnesses |

Verified on this image's package set: ISA-L encodes K=200 → N=255 over
1400-byte symbols, and recovers all 200 source symbols after 55 are erased.
