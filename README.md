# Daric

**Daric** is a modern C++ utility library.
It provides **thread-safe logging, thread pool management, and networking modules** out of the box.
Daric is modular, so you can enable only the features you need.

## Features

- **Logger**: Thread-safe singleton logging system with `Info`, `Warning`, and `Error` levels.
- **ThreadPool**: Flexible thread pool for parallel task execution.
- **Networking**: TCP server and client for asynchronous communication.
- **Modular CMake**: Select which modules to build: Logger, ThreadPool, Networking.
- **Expandable**: Easily add new modules (database, serialization, etc.) without breaking existing code.



## Directory Structure

```bash
Daric/
├── CMakeLists.txt
├── include/daric/ # Public headers
├── src/ # Source files
├── examples/ # Usage examples
└── tests/ # Unit tests
```

## Build Instructions

1. **Clone the repository**

```bash
git clone <your-repo-url> Daric
cd Daric
```

2. **Configure the build**

- Build all modules (default):

```bash
cmake -S . -B build
```

- Build specific modules only:

```bash
cmake -S . -B build \
  -DBUILD_LOGGER=ON \
  -DBUILD_THREADPOOL=OFF \
  -DBUILD_NETWORKING=ON
```

3. **Build**

```bash
cmake --build build
```

4. **Run examples**

```bash
./build/examples/example_logger
./build/examples/example_network
```

## Install (Optional)

To install headers and library system-wide:

```bash
cmake --install build --prefix /usr/local
```

This will install:

- libDaric.a → /usr/local/lib
- include/daric/* → /usr/local/include/daric

## Usage Example

```cpp
#include "daric/daric.h"

int main() {
#ifdef BUILD_LOGGER
    auto &logger = daric::Logger::instance();
    logger.log(daric::LogLevel::Info, "Daric Logger is working!");
#endif

#ifdef BUILD_THREADPOOL
    daric::ThreadPool pool(4);
    pool.enqueue([]{ /* your task */ });
#endif

#ifdef BUILD_NETWORKING
    daric::TCPClient client("127.0.0.1", 8080);
    client.connectToServer();
#endif

    return 0;
}
```

## Dependencies

- C++17 or newer
- POSIX-compliant system (Linux/macOS) for Networking module (Windows requires Winsock adjustments)
- CMake >= 3.16
