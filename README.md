# WebTTY

WebTTY is a browser-based terminal emulator.

The user interface runs in a standards-compliant web browser using HTML, CSS, and JavaScript. A small C++20 bridge serves the browser application over HTTPS, establishes a secure WebSocket connection, and mediates communication with the host.

## Current Status

The current implementation provides:

* HTTPS static file server
* Secure WebSocket (WSS) endpoint
* Browser-based terminal page
* Browser-to-bridge communication
* WebSocket echo test

The echo test establishes the browser-to-bridge transport that will be used by a PTY-backed terminal session.

## Project Layout

```text
WebTTY/
├── CMakeLists.txt
├── src/
├── tests/
└── www/
```

* `src/` contains the C++ bridge.
* `www/` contains the browser application.
* `tests/` contains test targets.

## Requirements

* CMake 3.20 or newer
* C++20 compiler
* Boost 1.90 or newer
* OpenSSL 3
* Ninja (recommended)

On macOS these dependencies may be installed using Homebrew.

## Configure

```sh
cmake \
  -S . \
  -B build/debug \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DOPENSSL_ROOT_DIR="$(brew --prefix openssl@3)"
```

## Build

```sh
cmake --build build/debug
```

## Development Certificate

Generate a self-signed certificate for local development.

```sh
./keygen.sh
```

or generate one manually with OpenSSL.

## Run

From the repository root:

```sh
./build/debug/webtty
```

Open:

```text
https://127.0.0.1:8443/
```

Your browser will prompt you to trust the self-signed certificate the first time you connect.

## License

WebTTY is licensed under the GNU Affero General Public License v3.0 (AGPL-3.0).

