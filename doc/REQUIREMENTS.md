# Requirements

## Goals
- The purpose of this project is creating fully function tcp/udp (transport layer) and http (application layer) support library with cpp23. 
- This project is my study project so I can practice my networking skill 

## Non-goals
- TLS
- HTTP/2
- performance tuning
- TLS and HTTP/2 will be adding later

## Functional requirements
- TCP client: connect, send, receive, close.
- TCP server: bind, listen, accept, send, receive, close.
- UDP client: send, receive, close.
- UDP server: bind, send, receive, close. 
- Timeout handling (poll-based).
- Error reporting model (std::error_code + std::expected).
- IPv4-only in the first iteration.
- HTTP/1.1 requests/responses with Content-Length.
- design with multithread and async

## Non-functional requirements
- Simplicity and debuggability.
- Minimal allocations and clear ownership.
- Linux-only portability, C++23.

## Constraints
- No third-party dependencies.
- Only C++ standard library + Linux/POSIX syscalls.

## Public API surface (high-level)
- [ ] TODO: Summarize the main public types/modules (`net`, `http`).
- 'net' is the transport layer lib
- net::tcp_client will fully create a tcp client socket
- net::tcp_server will create a tcp server socket
- net::udp_client will create a udp client socket
- net::udp_server will create a udp server socket
- 'http' is the application layer lib
- http requirement will be write later

## Error model
- std::expected<expedted value, std::error_code>

## Compatibility
- Linux-only notes (syscalls, headers).