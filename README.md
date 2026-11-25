# MyCppContainer

## Overview

MyCppContainer is a Modern C++20 Container Lab focused on container design,
memory management, exception safety, and future performance research. It is an
educational, header-only library in the `mycppcontainer` namespace.

The Vector v0.1 educational core is complete and the project is preparing for
initial publication. It implements a focused subset of `std::vector` behavior.
No benchmark results or performance advantages over `std::vector` are claimed.

## Features

`mycppcontainer::Vector<T>` provides:

- Default construction, destruction, constrained copy construction/assignment,
  and `noexcept` move construction/assignment.
- `size()`, `capacity()`, `empty()`, and `reserve()` for empty or nonempty vectors.
- `push_back(const T&)`, `push_back(T&&)`, and forwarding `emplace_back()`
  with automatic growth; `emplace_back()` returns the new element by reference.
- Const-correct `operator[]`, `front()`, `back()`, and `data()`.
- `clear()` and `pop_back()` that retain allocated capacity.
- Pointer `iterator`/`const_iterator` aliases and
  `begin/end/cbegin/cend`, including range-for and standard algorithm use.
- Support for non-default-constructible, move-only, and over-aligned elements,
  subject to each operation's construction requirements.

## Design

- **Storage and lifetime:** `std::allocator<T>` and `std::allocator_traits`
  allocate raw storage separately from constructing elements. Only
  `[0, size())` contains live objects; `size() <= capacity()` is maintained.
- **Ownership:** the Rule of Five gives copies independent storage and transfers
  storage on moves. Moved-from vectors become empty with null storage and zero
  capacity. Self-copy and self-move assignment preserve the object.
- **Growth:** automatic append starts at capacity one, then doubles with an
  overflow guard and saturation at the allocator limit. `reserve(n)` never
  shrinks and leaves size unchanged.
- **Forwarding and aliasing:** emplacement forwards arguments directly into
  storage. During growth, the new tail is constructed before relocating old
  elements so arguments such as `v[0]` are consumed while still valid.
- **Exception safety:** relocation uses `std::move_if_noexcept`; failure cleans
  up completed objects and the new allocation before propagating the exception.
  Copy assignment prepares a complete copy before replacing the target.
  The limits of these guarantees are described below.

Copying allocates capacity equal to the source size; copying an empty vector
produces null storage and zero capacity. Copy assignment does not reuse spare
target capacity. These are deliberate policies of this implementation.

## Project Structure

```text
.
├── AGENTS.md
├── CMakeLists.txt
├── README.md
├── .gitignore
├── include/mycppcontainer/
│   ├── config.hpp
│   └── vector.hpp
├── examples/
│   ├── CMakeLists.txt
│   └── main.cpp
└── tests/
    ├── CMakeLists.txt
    ├── test_smoke.cpp
    └── test_vector.cpp
```

`mycppcontainer` is a CMake INTERFACE target.
`container_lab_demo` currently prints the library's major version;
`container_lab_tests` contains the Catch2 suite. Generated build files are ignored.

## Requirements

| Component | Requirement | Verified environment |
| --- | --- | --- |
| Compiler and standard library | C++20 support | GCC 16.2.1 |
| CMake / CTest | CMake 3.20 or newer, as declared by the project | 4.5.1 |
| Build tool | Ninja for the commands below; no project minimum is specified | 1.13.2 |
| Test framework | Catch2 3 with its CMake package | 3.16.1 |
| Platform | Current validation covers Linux | Linux |

Catch2 is required when `BUILD_TESTING=ON`, which is the default. Dependencies
must already be installed; CMake does not download them. For a nonstandard
installation prefix, set `CMAKE_PREFIX_PATH` during configuration. CTest is
provided with CMake.

The versions above record the validation environment, not minimum supported
compiler or Ninja versions. Other toolchains and the declared minimum CMake
version have not been separately validated; dependencies are not version-locked.

## Build and Test

Run from the repository root. These commands target the verified Linux/GNU
environment and enable the warning flags used during acceptance.

```bash
cmake -S . -B build/debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
  -DMYCPPCONTAINER_ENABLE_SANITIZERS=OFF \
  -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic"
cmake --build build/debug --parallel
ctest --test-dir build/debug --output-on-failure

cmake -S . -B build/release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
  -DMYCPPCONTAINER_ENABLE_SANITIZERS=OFF \
  -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic"
cmake --build build/release --parallel
ctest --test-dir build/release --output-on-failure
```

To build without the test dependency, configure a separate directory with
`-DBUILD_TESTING=OFF`. The example target is still built.

## Sanitizers

`MYCPPCONTAINER_ENABLE_SANITIZERS` defaults to `OFF`. When enabled, ASan/UBSan
compile and link flags are PRIVATE to `container_lab_tests`; they do not
propagate through the interface library or instrument the example.
The current flags are validated on Linux/GCC; other configurations are unverified.

```bash
cmake -S . -B build/sanitized -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
  -DMYCPPCONTAINER_ENABLE_SANITIZERS=ON \
  -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic"

ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
  cmake --build build/sanitized --parallel --verbose

ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
  ctest --test-dir build/sanitized --output-on-failure
```

The build also runs Catch2 test discovery. Its verbose output should show
`-fsanitize=address,undefined` on test compilation and linking, and
`-fno-omit-frame-pointer` on test compilation.

## Validation

Acceptance review on **2026-11-25** validated the implementation and tests now
recorded in commit `9f6ced7`, using separate, freshly configured build directories.

| Configuration | Passed / total | Failed | Compiler warnings | Sanitizer diagnostics |
| --- | --- | --- | --- | --- |
| Debug | 89 / 89 | 0 | 0 | Not enabled |
| Release (`-O3 -DNDEBUG`) | 89 / 89 | 0 | 0 | Not enabled |
| Debug + ASan/UBSan, leak detection enabled | 89 / 89 | 0 | 0 | None reported |

The suite contains 88 Vector test cases and one smoke test. Catch2 sections are
not separate CTest cases. All three builds used `-Wall -Wextra -Wpedantic`.
A standalone header compilation check with `-std=c++20` also passed.

Tests cover normal and boundary operations, copy/move ownership, self-reference,
constructor exceptions, partial-construction cleanup, lifetime counters,
throwing move-only relocation, and 64-byte-aligned elements. Source review
complements these checks.

Deterministic allocation-failure injection, extreme actual allocations,
cross-platform validation, and benchmarks remain unverified. Passing tests and
clean sanitizer runs do not prove the absence of all lifetime errors or UB.

## Limitations

- This is a subset of the STL interface: no `at()`, `insert()`, `erase()`,
  `resize()`, range/initializer-list constructors, or reverse iterators.
  `at()` is intentionally outside the current core scope.
- Allocation is fixed to `std::allocator<T>`; allocator customization and
  propagation policies are not implemented. Element destructors must not throw.
- `operator[](i)` requires `i < size()`. `front()`, `back()`, and
  `pop_back()` require a nonempty vector. These operations perform no checks.
- A non-null `data()` does not imply live elements. Spare capacity must not be
  accessed as constructed objects, and `end()` must not be dereferenced.
- Reallocation invalidates element pointers, references, and iterators.
  Capacity-preserving append keeps existing element addresses, but changes the
  end boundary. Removal invalidates references to destroyed elements; reacquire
  endpoints after size changes.
- Strong rollback for `reserve()` and growth relies on copying being available
  or moving being nonthrowing. If a move-only element's move throws, old values
  may change; cleanup and storage/lifetime validity are retained, assuming the
  element remains safely destructible. Constructor side effects on arguments,
  including an internal element passed as a move source, are not rolled back.
- There is no CI pipeline or benchmark suite yet.

## Roadmap

All items below are **planned, not implemented**:

- `UniquePtr<T>` ownership utilities.
- Allocator concepts and configurable allocation.
- `SmallVector<T, N>` with small-buffer optimization.
- Benchmarks for allocation behavior, cache locality, and latency.

## References

The repository sources define the behavior documented here:

- [Vector implementation](include/mycppcontainer/vector.hpp)
- [Behavioral specifications and regression tests](tests/test_vector.cpp)
- [CMake target configuration](CMakeLists.txt)
- [Catch2 and CTest integration](tests/CMakeLists.txt)
