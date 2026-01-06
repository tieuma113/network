# High-Level Design (HLD)

This document describes the high-level design for this project. It is a living document: keep it consistent with `doc/REQUIREMENTS.md` and evolve it as the implementation grows.

Related docs:
- `doc/REQUIREMENTS.md` (what we are building)
- `doc/network_knowledge.md` (TCP/UDP/HTTP theory notes)

---

## 0) Design Decisions (Locked for v1)

- Target platform: Linux (POSIX sockets)
- C++ standard: C++20 (per `CMakeLists.txt`)
- I/O model (v1): single-thread, blocking sockets
- Timeout strategy (v1): `poll(2)`-based per operation (explicit timeout/deadline parameter)
- IP version (v1): IPv4-only
- DNS resolution (v1): not included (caller supplies IP/port)
- Error model: `std::expected<T, std::error_code>` (no exceptions)
- Dependency direction: `http` depends on `net`; `net` must not depend on `http`
- No third-party dependencies (standard library + Linux/POSIX syscalls only)

---

## 1) Overview

### 1.1 Project goal
Build a small learning-focused Linux networking library:
- `net`: TCP/UDP transport primitives with explicit timeouts and clear error handling.
- `http`: HTTP/1.1 message building/parsing on top of `net` (later sections).

### 1.2 Non-goals (v1)
- Asynchronous I/O (`epoll`), coroutines, thread pools.
- TLS, HTTP/2, performance tuning, advanced socket features.

---

## 2) Architecture Summary

### 2.1 Layering
- `net` (transport): owns file descriptors, provides connect/listen/accept and send/recv primitives.
- `http` (application): uses `net` to read/write bytes and implements HTTP framing and parsing rules.

Dependency direction:
- `http` → `net` (allowed)
- `net` → `http` (not allowed)

### 2.2 Key principles
- RAII for all OS resources (especially FDs).
- Move-only ownership for socket handles.
- Explicit handling of partial read/write and EOF.
- Deterministic, debuggable control flow (blocking + timeouts).

### 2.3 Source layout (current + intended)
Current build setup (can evolve):
- `inc/` public headers
- `src/` implementations
- CMake target `net` (library)

Intended evolution:
- Keep `net` and `http` as separate logical modules (may be separate CMake targets later).

---

## 3) `net` Module (Transport Layer) — v1 Design

### 3.1 Responsibilities
`net` is responsible for:
- Socket lifecycle: create, configure, close.
- Address representation (IPv4 in v1).
- TCP client/server primitives: connect, listen, accept.
- UDP send/receive primitives.
- Timeouts via `poll(2)` for connect/accept/read/write readiness.
- Error propagation using `std::error_code` + `std::expected`.

`net` is explicitly *not* responsible for:
- HTTP parsing/serialization.
- Application-level framing beyond “send/recv bytes”.

### 3.2 Public types and responsibilities (stable intent)

Names are placeholders; choose your naming, but keep responsibilities clear and non-overlapping.

#### `net::Fd`
- Purpose: minimal RAII wrapper for a Linux file descriptor.
- Invariants: owns a valid fd (`>= 0`) or “no fd” (`-1`).
- Ownership: move-only; destructor closes when owning.
- Notes: prefer `accept4(..., SOCK_CLOEXEC)` and/or `fcntl(FD_CLOEXEC)` for safety.

#### `net::SocketAddressV4`
- Purpose: represent an IPv4 endpoint `(ip, port)` to pass to syscalls.
- Representation: wraps `sockaddr_in` plus a length accessor.
- Conversions: construct from dotted quad + port; format to string for diagnostics.

#### `net::TcpSocket` (connected stream)
- Purpose: represent an established TCP connection.
- State: connected (owns fd) or empty/closed.
- Core operations (v1):
  - Connect: `connect(remote, timeout)`
  - Write: `send_some(span<byte>, timeout)` and optional `send_all(...)`
  - Read: `recv_some(span<byte>, timeout)` and optional `recv_exact(...)`
  - Close: `shutdown(how)` and/or `close()`
- Options (v1): keep minimal; consider internal defaults + explicit toggles later.

#### `net::TcpListener` (listening socket)
- Purpose: bind/listen and accept incoming connections.
- Core operations (v1):
  - `bind(local)`
  - `listen(backlog)`
  - `accept(timeout)` → returns `net::TcpSocket`
- Options (v1): `SO_REUSEADDR` for quick restarts; optionally expose backlog.

#### `net::UdpSocket` (datagram)
- Purpose: UDP datagram I/O, optionally “connected UDP”.
- Core operations (v1):
  - `bind(local)` (optional)
  - `send_to(remote, bytes, timeout)`
  - `recv_from(buffer, timeout)` → returns `(remote, bytes_received)`
  - Optional `connect(remote)` so `send/recv` target a fixed peer
- Truncation policy (v1): if buffer is too small, return an error (`message_size`) and discard remainder.

#### Timeouts / Deadlines
- Representation: `std::chrono::steady_clock`-based deadline/duration.
- Timeout result (v1): timeout returns `std::errc::timed_out` via `std::error_code`.

### 3.3 API contracts (caller-visible semantics)

Write these as MUST/SHOULD statements; implementation should be validated against them with tests.

#### Partial I/O
- TCP is a byte stream: `recv_some` MUST be allowed to return any number of bytes including fewer than requested.
- `recv_some == 0` MUST mean EOF (orderly shutdown by peer).
- `send_some` MUST be allowed to write fewer bytes than requested.
- Convenience helpers:
  - `send_all` MUST loop until all bytes written, timeout, or error.
  - `recv_exact` MUST loop until N bytes read, EOF, timeout, or error.

#### EINTR / EAGAIN policy
- EINTR:
  - v1 policy: retry internally where safe; do not surface `EINTR` to callers.
- EAGAIN/EWOULDBLOCK:
  - Even after `poll`, syscalls may still return EAGAIN due to races; v1 policy is to re-`poll` until deadline or progress.

#### SIGPIPE policy (Linux)
- Writes to a closed socket may raise SIGPIPE; policy:
  - v1: use `send(..., MSG_NOSIGNAL)` for TCP writes to prevent process termination.

#### EOF vs timeout vs error (must be distinguishable)
- EOF is not an error; it is a successful read of 0 bytes.
- Timeout is represented by `std::errc::timed_out` error_code.
- Other failures propagate the underlying `errno` as `std::error_code`.

### 3.4 Timeout strategy (v1: `poll(2)`-based)

Define which operations use `poll` and how:
- Connect with timeout:
  - Initiate connect, `poll` for writability, then confirm success via `getsockopt(SO_ERROR)`.
- Accept with timeout:
  - Poll listener for readability, then `accept4`.
- Recv/Send with timeout:
  - Poll for readability/writability, then attempt `recv`/`send`.

Define timeout semantics precisely:
- If deadline passes before progress: return timeout error.
- If some bytes were transferred and then deadline passes:
  - v1 policy:
    - primitives (`send_some`/`recv_some`): return the partial bytes transferred; timeout is only returned when no progress was made before the deadline.
    - “all/exact” helpers (`send_all`/`recv_exact`): return timeout if the operation could not complete before deadline.

### 3.5 Syscall mapping (cheat sheet)

Provide a mapping table for implementers:
- create: `socket(2)`
- configure: `setsockopt(2)` / `fcntl(2)` / `getsockopt(2)`
- connect: `connect(2)` + `poll(2)` + `getsockopt(SO_ERROR)`
- server: `bind(2)`, `listen(2)`, `accept4(2)`
- I/O: `send(2)`, `recv(2)` (or `read(2)`, `write(2)`)
- shutdown/close: `shutdown(2)`, `close(2)`

### 3.6 Socket options (v1 defaults)
The initial goal is to keep this minimal and explicit.

Recommended defaults:
- Listener: `SO_REUSEADDR=1`
- Accepted sockets: `SOCK_CLOEXEC` (via `accept4`) if available
- TCP send: use `MSG_NOSIGNAL` to suppress SIGPIPE

Deferred options (document as potential follow-ups):
- `TCP_NODELAY` (latency vs small packets tradeoff)
- `SO_KEEPALIVE` (keepalive policy/tuning)
- Buffer sizing (`SO_RCVBUF`, `SO_SNDBUF`)

### 3.7 Extension points (later)
- IPv6 support (`sockaddr_storage`).
- DNS resolution wrapper (`getaddrinfo`).
- Async/event-driven (`epoll`) and/or multi-threaded server model.
- Higher-level helpers (buffered reader, line reader) for HTTP.

---

## 4) `http` Module (Application Layer) — v1 Intent (Not Implemented Yet)

`http` will build message framing on top of TCP stream semantics.

v1 constraints (from requirements):
- HTTP/1.1 only, no TLS.
- Support messages with `Content-Length`.
- Defer `Transfer-Encoding: chunked` to a later iteration.

Planned responsibilities:
- Types: `http::Request`, `http::Response`, header container.
- Serialization: CRLF line endings; always emit `Host` for requests; emit `Content-Length` when a body exists.
- Parsing: incremental parsing with size limits:
  - parse start-line and headers until `\r\n\r\n`
  - determine body length from `Content-Length`
  - read exactly N bytes for the body
- Connection handling (v1):
  - no pipelining
  - allow one request/response per connection initially; keep-alive can be added once parsing is robust

---

## 5) Testing Strategy (v1)

No third-party test framework: small test executables + CTest.

### 5.1 Unit-ish tests
- Address parsing/formatting.
- Error mapping from `errno` to `std::error_code`.

### 5.2 Integration tests (localhost)
- TCP: start listener, connect to `127.0.0.1`, send/recv, close handling (EOF).
- Timeout: accept timeout, recv timeout (avoid flaky tests; use generous margins).
- UDP: sendto/recvfrom on localhost; truncation policy test.

---

## 6) Open Questions (track here)

Keep this list short and actively maintained.
- [ ] Do we expose `send_all` / `recv_exact` publicly, or keep only “some” primitives in v1?
- [ ] UDP truncation: return error vs return (n, truncated=true)?
- [ ] HTTP next step after Content-Length: add chunked, or focus on robustness + limits first?
