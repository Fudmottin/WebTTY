# WebTTY

WebTTY is a browser-based terminal emulator written with standard web
technologies on the frontend and a small C++20 bridge on the backend.

The long-term goal is a lightweight terminal that does not depend on large
JavaScript frameworks or heavyweight native GUI toolkits.

## Design Goals

- HTML, CSS and modern JavaScript only
- C++20/23 backend
- Minimal dependencies
- Portable between macOS and Linux
- Standards-based (HTTPS + WebSocket)
- Low memory footprint
- Clean separation of concerns

## Architecture

```
Browser
    │
    │ HTTPS + WSS
    ▼
C++ Bridge
    │
    ▼
PTY
    │
    ▼
User Shell
```

The bridge intentionally knows nothing about terminal semantics.

The browser owns:

- keyboard encoding
- VT parser
- screen model
- rendering

The bridge owns:

- HTTPS
- WebSocket
- PTY lifecycle
- byte transport

## Current Status

The project currently supports:

- HTTPS static file server
- secure WebSocket endpoint
- browser connection status
- WebSocket echo test

The next milestone replaces the echo test with a local PTY-backed shell.

## Building

Requirements:

- CMake 3.20+
- C++20 compiler
- Boost
- OpenSSL
- Ninja (recommended)

```
cmake -S . -B build/debug -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DOPENSSL_ROOT_DIR="$(brew --prefix openssl@3)"

cmake --build build/debug
```

## License

AGPL-3.0

