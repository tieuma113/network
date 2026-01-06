# Network Knowledge (TCP & UDP) — Theory Notes

This document is a theory-first summary of TCP and UDP, written to support requirements analysis and high-level design for a Linux networking library. It is intentionally **not** project-specific code documentation.

## 1) Where TCP/UDP sit in the stack

### 1.1 TCP/IP vs OSI (practical view)
- **Link layer**: Ethernet/Wi‑Fi framing on a local link.
- **Internet layer**: **IP** routes packets across networks (best-effort delivery).
- **Transport layer**: **TCP** and **UDP** multiplex application traffic using **ports** and define end-to-end semantics.
- **Application layer**: HTTP, DNS, custom protocols, etc.

### 1.2 The core addressing concepts
- **IP address** identifies a host/interface.
- **Port** identifies an application endpoint on that host.
- A TCP connection is identified by a **4‑tuple**: `(src_ip, src_port, dst_ip, dst_port)`.
- A UDP “conversation” is often modeled by the same 4‑tuple, even though UDP itself is connectionless.

### 1.3 “Socket” terminology (important for design discussions)
- **Socket (API object)**: an OS handle (a file descriptor on Linux) that represents one endpoint for communication.
- **Socket address**: `(ip, port)` in a `sockaddr` structure.
- **Socket options**: kernel settings attached to the socket (timeouts, buffers, reuseaddr, etc.).

## 2) TCP — Transmission Control Protocol (stream transport)

### 2.1 What TCP guarantees (and what it does not)
TCP provides (between two endpoints in a connection):
- **Reliable delivery**: data is retransmitted if lost (until the connection fails).
- **In-order delivery**: bytes are presented to the application in sequence order.
- **Duplicate suppression**: duplicates are handled internally.
- **Flow control**: receiver can slow down sender using the advertised receive window.

TCP does **not** provide:
- Message boundaries (“records”). TCP is a **byte stream**.
- Delivery within a fixed deadline. Timeouts are policy decisions above TCP.

### 2.2 TCP is a byte stream: the most common misunderstanding
When an application calls `send()` with N bytes:
- TCP may split them into multiple segments, coalesce multiple sends, delay, retransmit, etc.
When the peer calls `recv()`:
- It may return **any number of bytes** from `1..N` (or more if previous sends were buffered).

Key implications:
- You must handle **partial reads** and **partial writes**.
- Protocols on top of TCP need explicit **framing**:
  - delimiter-based (e.g., `\r\n\r\n` for HTTP headers),
  - length-based (e.g., `Content-Length`),
  - chunked framing (e.g., HTTP chunked transfer),
  - or application-specific message formats.

### 2.3 Connection establishment: the 3‑way handshake
Classic TCP handshake:
1. Client → Server: `SYN` (proposes initial sequence number).
2. Server → Client: `SYN+ACK`.
3. Client → Server: `ACK`.

Why three steps?
- Both sides must agree the other side can receive.
- Sequence numbers must be synchronized in both directions.

Important notes:
- `connect()` completes when the handshake succeeds (or fails).
- There are edge cases (simultaneous open, SYN retransmits, SYN cookies), but the key point is: **TCP connection has state** on both ends.

### 2.4 Data transfer: sequence numbers, ACKs, and sliding windows
TCP numbers bytes with **sequence numbers**.
- The receiver ACKs the next expected sequence number, meaning “I have received all bytes up to N−1”.

Two windows matter:
- **Receive window (rwnd)**: flow control; how much buffer the receiver is willing to accept.
- **Congestion window (cwnd)**: congestion control; how much the sender believes the network can handle.

Effective send rate is limited by roughly `min(rwnd, cwnd)`.

### 2.5 Flow control vs congestion control (do not mix them)
- **Flow control (rwnd)** prevents the sender from overwhelming the receiver.
- **Congestion control (cwnd)** prevents the sender from overwhelming the network.

Consequences in practice:
- A `send()` call can block even if the peer is alive (backpressure).
- Throughput and latency depend on network conditions (RTT, loss), not just CPU.

### 2.6 Retransmission and loss recovery (high-level)
TCP detects loss via:
- **Timeout (RTO)**: retransmit after no ACK for some time.
- **Duplicate ACKs / fast retransmit**: infer loss earlier when later data is acknowledged out of order.

Loss recovery interacts with congestion control (cwnd reduction), affecting throughput.

### 2.7 Segmentation, MSS, MTU, and fragmentation
Definitions:
- **MTU**: maximum IP packet size on a link (commonly 1500 bytes on Ethernet).
- **MSS**: maximum TCP payload size in a segment (≈ MTU minus headers).

Why it matters:
- TCP will segment application data to fit MSS.
- IP fragmentation is undesirable; if any fragment is lost, the whole packet is effectively lost.
  - Modern stacks try to avoid fragmentation using **Path MTU Discovery (PMTUD)**.

Design implication:
- A library should not assume “one write equals one packet”.

### 2.8 Latency behaviors: Nagle’s algorithm and delayed ACK
Two classic mechanisms:
- **Nagle’s algorithm** (`TCP_NODELAY` disables it): tries to reduce many tiny packets by coalescing small writes.
- **Delayed ACK**: receiver may delay ACK briefly to piggyback or ACK multiple segments.

Their interaction can increase latency for small request/response patterns.
Rule of thumb (theory):
- If your protocol sends tiny messages and cares about latency, you may consider `TCP_NODELAY`.
- Otherwise, default settings are typically fine.

### 2.9 Connection close, half-close, and TIME_WAIT
Closing in TCP is also a stateful handshake:
- Each direction (send/receive) can be closed independently (**half-close**).
- A normal close uses `FIN` and ACKs; data in flight can still arrive after FIN until all bytes are delivered.

`TIME_WAIT` exists to:
- ensure late segments from an old connection do not get mistaken for a new one with the same 4‑tuple,
- allow proper ACK of the final FIN.

Design implications:
- A graceful shutdown is different from an abrupt reset.
- You must treat `recv() == 0` as “peer performed an orderly shutdown” (EOF).

### 2.10 Reset (RST) and “broken pipe”
RST indicates an abnormal termination or a response to an invalid state, e.g.:
- connecting to a port with no listener (often appears as `ECONNREFUSED`),
- sending on a connection that peer has already reset.

“Broken pipe” (`EPIPE`) commonly occurs when writing after the peer closed; it may also generate **SIGPIPE** on Unix.

### 2.11 TCP head-of-line blocking (HOL)
Because TCP delivers bytes **in order**, if a segment is lost:
- later data cannot be delivered to the application until missing bytes are retransmitted.

This is not a bug—it's part of TCP’s semantics. It influences application-level protocol choices.

## 3) UDP — User Datagram Protocol (datagram transport)

### 3.1 What UDP is (and is not)
UDP is a minimal transport protocol:
- Message-oriented: each send corresponds to one **datagram** (a message boundary).
- Best-effort: datagrams can be **lost**, **duplicated**, **reordered**, or **delayed**.
- No built-in congestion control or retransmission.
- No connection establishment handshake.

UDP *does* provide:
- Port multiplexing.
- Optional checksum (IPv4 may allow zero; IPv6 requires checksum).

### 3.2 Datagram semantics and boundaries
With UDP:
- `sendto()` sends one datagram.
- `recvfrom()` receives one datagram (up to the buffer size).

If the receive buffer is smaller than the datagram:
- the datagram may be **truncated** and the remainder discarded (OS-dependent details, but truncation is a real design concern).

Design implications:
- You must decide what “message size” limits exist and how to report truncation.
- Unlike TCP, “read until you get the rest later” is not a valid assumption.

### 3.3 “Connected UDP” (a common confusion)
Calling `connect()` on a UDP socket:
- does **not** create a reliable connection or perform a handshake,
- sets a **default peer address** so you can use `send()`/`recv()` without specifying addresses,
- filters incoming datagrams so the socket only receives from that peer,
- may allow some network errors (e.g., ICMP unreachable) to be reported as socket errors on subsequent operations.

Connected UDP is often used for clients that speak to exactly one server.

### 3.4 MTU, IP fragmentation, and why large UDP datagrams are risky
If a UDP datagram exceeds the path MTU:
- IP may fragment it into multiple packets.
- Loss of any fragment typically means the entire datagram is lost.

Practical rule:
- Keep UDP payloads comfortably under common MTUs (e.g., under ~1200 bytes if you want to be conservative across the Internet).

### 3.5 Broadcast and multicast (conceptual overview)
- **Broadcast**: one-to-all on a local network (requires enabling broadcast on the socket).
- **Multicast**: one-to-many group communication.
  - Receivers join a group; routers may forward multicast depending on configuration.
  - Requires additional socket options and group management (IGMP/MLD concepts).

These are powerful but add complexity to API design; they are often deferred to later phases.

### 3.6 What UDP is good for
UDP works well when:
- your app can tolerate loss or implements its own reliability,
- low latency is more important than perfect reliability,
- you need message boundaries,
- you want to build protocols like QUIC (which implements reliability and congestion control in user space).

## 4) Common cross-cutting topics (TCP and UDP)

### 4.1 Blocking vs non-blocking I/O
- **Blocking** calls may sleep until they can make progress (data available, buffer space, connection established).
- **Non-blocking** calls return immediately with `EAGAIN/EWOULDBLOCK` if they would block.

Even in blocking designs, you should understand non-blocking semantics because:
- readiness APIs (`poll`) are fundamentally about “would block?”,
- some race conditions can still produce `EAGAIN` after a readiness notification.

### 4.2 Timeouts: where they come from
There are multiple “timeouts” in networking:
- **Application timeouts**: how long your app is willing to wait.
- **Socket-level timeouts**: e.g., receive/send timeouts configured on the socket.
- **Protocol timeouts**: TCP retransmission behavior (RTO), keepalive, etc.

Using readiness waiting (e.g., `poll`) is conceptually:
- Wait until an operation is likely not to block, up to a deadline.
- Then attempt the operation and handle errors/EOF normally.

### 4.3 Signals and EINTR
On Unix-like systems, blocking syscalls may return early with `errno = EINTR` if interrupted by a signal.
Theory-level policy choices:
- **Retry internally** (common for low-level wrappers).
- Propagate to the caller (useful if you want cancellation semantics).

### 4.4 SIGPIPE
When writing to a closed connection, Unix may raise **SIGPIPE**; default action is process termination.
Library-level implication:
- You must choose a strategy to prevent unexpected termination (often by suppressing SIGPIPE per send call).

### 4.5 Buffering and backpressure
There are buffers at multiple layers:
- application buffers,
- kernel send/receive buffers,
- NIC/driver queues.

Backpressure occurs when these fill up:
- In TCP, `send()` can block because the receiver/network cannot keep up.
- In UDP, `sendto()` may fail (e.g., no buffer space), and you must decide whether to drop, retry, or apply your own rate control.

## 5) How the theory maps to Linux/POSIX syscalls

This section is not an API design; it is a conceptual mapping from transport concepts to OS primitives.

### 5.1 Core socket lifecycle
- Create a socket: `socket(2)`
- Close it: `close(2)` (or `shutdown(2)` for half-close semantics in TCP)

### 5.2 TCP client
- Connect: `connect(2)`
- Send/receive: `send(2)`, `recv(2)` (or `write(2)`, `read(2)`)
- Socket options: `getsockopt(2)`, `setsockopt(2)`

### 5.3 TCP server
- Bind: `bind(2)`
- Listen: `listen(2)`
- Accept: `accept(2)` / `accept4(2)`
- I/O: `send(2)`, `recv(2)`

### 5.4 UDP
- Bind (optional, for receiving or fixed port): `bind(2)`
- Send datagrams: `sendto(2)`
- Receive datagrams: `recvfrom(2)`
- Connected UDP: `connect(2)` then `send(2)`/`recv(2)` are allowed

### 5.5 Readiness waiting and timeouts
- Wait for read/write readiness: `poll(2)` (or `select(2)`)
- Adjust blocking mode if needed: `fcntl(2)` with `O_NONBLOCK`

### 5.6 Useful socket options to know (theory-level)
- `SO_REUSEADDR`: re-bind quickly after restart (server sockets).
- `SO_RCVBUF` / `SO_SNDBUF`: kernel buffer sizes.
- `SO_RCVTIMEO` / `SO_SNDTIMEO`: socket-level timeouts (an alternative to `poll`).
- TCP-specific: `TCP_NODELAY`, `SO_KEEPALIVE` (with TCP keepalive tunables).
- UDP-specific: `SO_BROADCAST`, multicast group options (later).

## 6) Common pitfalls (what designs must not assume)
- **TCP message boundaries do not exist**: never assume 1 `recv` == 1 application message.
- **Partial writes happen**: even in blocking mode, `send` may write fewer bytes than requested.
- **EOF is not an error**: `recv == 0` is a state (“peer closed send direction”), not “timeout”.
- **Readiness is not success**: `poll` says “likely to not block”, not “operation will succeed”.
- **UDP truncation is real**: receiving into too-small buffers loses data; decide on a policy.
- **SIGPIPE can kill the process**: you must prevent it or document how callers must handle it.
- **Large UDP payloads risk fragmentation**: keep datagrams small when possible.

## 7) HTTP/1.1 — Application layer on top of TCP

HTTP/1.1 is an **application-layer** protocol that uses TCP as a byte stream transport. Most practical complexity comes from mapping a structured message protocol onto a stream where reads/writes are partial and boundaries do not exist.

### 7.1 The HTTP message model (bytes, lines, and CRLF)
HTTP/1.1 messages are sequences of bytes with a line-oriented syntax:
- Lines are terminated by **CRLF** (`\r\n`), not just `\n`.
- A message is: **start-line** + **headers** + **empty line** + optional **body**.
- The header section ends at the first **CRLF CRLF** sequence (`\r\n\r\n`).

Practical implication over TCP:
- You must parse incrementally: “read until you have enough bytes to decide what comes next”.
- You must be able to handle headers split across multiple `recv()` calls.

### 7.2 Requests vs responses (start-line)
**Request line**:
- Form: `METHOD SP request-target SP HTTP-version CRLF`
- Examples:
  - `GET /index.html HTTP/1.1`
  - `POST /submit HTTP/1.1`

Notes:
- `METHOD` is an uppercase token in common practice, but the spec treats it as a token.
- `request-target` is most often **origin-form** (`/path?query`).
- For proxies you may see **absolute-form** (`http://example.com/path`) and for `CONNECT` you see **authority-form** (`host:port`).

**Status line**:
- Form: `HTTP-version SP status-code SP reason-phrase CRLF`
- Example: `HTTP/1.1 200 OK`

Notes:
- `reason-phrase` is optional/meaningless for program logic; treat it as informational.

### 7.3 Headers: field syntax and important rules
Each header line is:
- `field-name ":" OWS field-value OWS CRLF`

Key rules:
- Header **field names are case-insensitive** (e.g., `Content-Length` == `content-length`).
- There may be optional whitespace (OWS) around values; be tolerant in parsing.
- **Obsolete line folding** (`obs-fold`, a header value continuing on the next line starting with SP/HTAB) exists historically but is obsolete and risky; a safe baseline is to **reject** it or treat it strictly.

HTTP/1.1 requires:
- `Host` header in requests (for origin servers), except in some edge forms; practically: require it for origin-form requests.

### 7.4 When does a message have a body?
Whether a message has a body depends on method/status and on framing headers:
- Requests: many methods can have a body, but some common ones typically do not (e.g., `GET`).
- Responses:
  - `1xx`, `204`, and `304` responses **do not** have a body.
  - Responses to `HEAD` **do not** have a body (even if headers imply one).

### 7.5 Body framing: how the receiver knows “where the body ends”
Because TCP is a stream, HTTP/1.1 needs explicit framing rules. The receiver decides the body length in this order:

1) If `Transfer-Encoding` is present and the final encoding is `chunked`:
   - The body is a sequence of chunks: `<hex-size>\r\n<bytes>\r\n ... 0\r\n<trailers>\r\n`.
   - Trailers are additional headers after the terminating `0` chunk (optional).

2) Else if `Content-Length` is present:
   - The body is exactly that many bytes.
   - Multiple `Content-Length` headers are ambiguous and dangerous; robust parsers typically **reject** conflicting values.

3) Else if neither applies and the message is allowed to have a body:
   - The body ends at **connection close** (EOF). This is common in HTTP/1.0 style responses and some error cases.

Security/design note:
- Conflicting `Transfer-Encoding` and `Content-Length` can lead to request smuggling issues; simplest safe rule is: if `Transfer-Encoding` is present, ignore `Content-Length` (or reject), and handle only supported encodings.

### 7.6 Connection semantics (HTTP/1.1 keep-alive)
HTTP/1.1 connections are **persistent by default**:
- Multiple requests/responses can be sent over one TCP connection (sequentially).
- To close: either side can include `Connection: close`, then close after finishing the current message.

Pipelining (sending multiple requests without waiting for responses) exists in the spec but is widely discouraged in practice. A simple first implementation can assume:
- At most one in-flight request per connection (no pipelining).

### 7.7 Practical parsing pitfalls (what your library must not assume)
- `\r\n\r\n` can appear split across reads; delimiter search must work across buffer boundaries.
- A header section can be large; you need a **maximum header size** to avoid unbounded memory usage.
- A single `recv()` can contain:
  - only part of headers,
  - headers + part of body,
  - a full message + start of the next message (persistent connections).
- For `Content-Length`, you must read exactly N bytes even if `recv()` returns fewer.
- For chunked encoding, chunk boundaries do not align to TCP reads; parsing must be stateful.

### 7.8 Minimal “on the wire” examples
GET request (no body):
```
GET /hello HTTP/1.1\r
\nHost: example.com\r
\nUser-Agent: my-client\r
\n\r
\n
```

Response with `Content-Length`:
```
HTTP/1.1 200 OK\r
\nContent-Type: text/plain\r
\nContent-Length: 5\r
\n\r
\nhello
```

Chunked response (conceptual):
```
HTTP/1.1 200 OK\r
\nTransfer-Encoding: chunked\r
\n\r
\n5\r
\nhello\r
\n0\r
\n\r
\n
```

## 8) Recommended reading (offline-friendly)

### man pages (Linux)
- `man 2 socket`, `connect`, `bind`, `listen`, `accept`, `accept4`
- `man 2 send`, `recv`, `sendto`, `recvfrom`, `shutdown`, `close`
- `man 2 poll`, `select`, `fcntl`
- `man 2 getsockopt`, `setsockopt`
- `man 7 tcp`, `udp`, `ip`, `socket`, `signal`

### RFCs (core)
- TCP: RFC 793 (original TCP specification)
- UDP: RFC 768
- Host requirements: RFC 1122 (TCP/IP behavior notes)
- HTTP/1.1 messaging: RFC 9112 (obsoletes RFC 7230)
- HTTP semantics: RFC 9110

### RFCs / docs (deeper, optional)
- TCP congestion control: RFC 5681
- TCP retransmission timeout (RTO): RFC 6298
- TCP window scaling: RFC 7323
- Path MTU Discovery: RFC 1191 (IPv4), RFC 8201 (IPv6)
- HTTP/1.1 message syntax (older but still widely referenced): RFC 7230
