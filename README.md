### MY NETWORK LIBRARY ###
<This project is for learning purpose>

# Summary
- This library is for TCP/IP wrapper and http for c++ in the linux system

# Requirement
- This project can only use std, linux systemcall and build in functions of c++

# Design docs
- `doc/HIGH_LEVEL_DESIGN.md`
- `doc/TCP_IP_TRANSPORT_LAYER.md`

# Style / Lint
- Format: `clang-format -i main.cpp` (config: `.clang-format`)
- Lint: `clang-tidy main.cpp --` (config: `.clang-tidy`)

# Build / Compile checklist (Linux)
- [ ] Install a C++ compiler (GCC or Clang)
- [ ] Install CMake (needed: `>=3.10`; if using presets: `>=3.19`)
- [ ] Install Ninja (recommended; required by the provided preset)
- [ ] Configure
  - With preset (uses Ninja): `cmake --preset "Configure preset using toolchain file"`
  - Without presets:
    - `cmake -S . -B out/build/local -G Ninja -DCMAKE_BUILD_TYPE=Debug`
- [ ] Build
  - With preset build dir: `cmake --build out/build/"Configure preset using toolchain file" -j`
  - Without presets: `cmake --build out/build/local -j`
- [ ] Run
  - `./out/build/<build-dir>/network_dev`
- [ ] (Optional) Install
  - `cmake --install <build-dir>`
- [ ] (Optional) Tests
  - `ctest --test-dir <build-dir>`

# Knowledge to work on this project
- C++: compilation model, headers vs sources, RAII, `std::string`/`std::vector`, error handling
- C++ concurrency: `std::thread`, `std::mutex`, `std::condition_variable`, atomics/memory order basics, thread pools
- Linux/POSIX: file descriptors, `read`/`write`, `close`, signals, timeouts, non-blocking I/O
- Linux scalability: `epoll`, `eventfd`, `timerfd`, `SO_REUSEPORT` (later), process/thread scheduling basics
- Networking: TCP/IP basics, sockets (`socket`, `bind`, `listen`, `accept`, `connect`), byte order, ports
- HTTP: request/response format, headers, status codes, parsing/serialization basics
- Tooling: CMake targets, build types (Debug/Release), Ninja, `gdb`/`lldb`, sanitizers (ASan/UBSan)
- Performance: profiling with `perf`, benchmarking/load testing (e.g., `wrk`/`hey`), latency percentiles, avoiding allocations
- Debugging network apps: `curl`, `nc`, `ss`, `lsof`, `tcpdump` (optional but useful)
