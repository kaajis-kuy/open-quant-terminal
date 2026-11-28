# Building Open Quant Terminal

## Requirements

- CMake 3.27 or newer
- A C++20-capable compiler
- Qt 6.8.x with the Qt Widgets module
- Qt Test module when building tests
- Ninja is recommended but not required

CI currently verifies Linux with Qt 6.8.3. Other operating systems and Qt patch versions have not yet been verified.

## Configure

~~~~bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="/path/to/Qt/6.8.x"
~~~~

If Qt is already discoverable through the environment or system package manager, `CMAKE_PREFIX_PATH` can be omitted.

## Build

~~~~bash
cmake --build build --parallel 2
~~~~

Use a low parallelism value on memory-constrained machines.

## Build and run tests

~~~~bash
cmake -S . -B build -DOQT_BUILD_TESTS=ON -DCMAKE_PREFIX_PATH="/path/to/Qt/6.8.x"
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
~~~~

The initial smoke test uses Qt's offscreen platform and does not require a display server.

## Current scope

The current targets include the Qt application shell, canonical data-contract validation, and provider-response envelope decoding. Tests use deterministic JSON fixtures and do not require live provider credentials or network access. The first complete provider-to-domain vertical slice remains pending.
