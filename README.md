# property-browser (qpb)

A property browser library for Qt 6 Widgets (namespace `qpb`): declare properties once, display them as
Tree / List / Form views on top of a single `QAbstractItemModel`, and extend data types without modifying the library.

Status: pre-1.0, under development. The public API is frozen starting with 1.0 (see the stability policy in the spec).

## Using qpb in a project

qpb is distributed as a self-contained component folder. Copy `qpb/` into your project (for example to
`components/qpb/`) and add:

```cmake
find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
add_subdirectory(components/qpb)
target_link_libraries(my_app PRIVATE qpb::widgets)
```

To update, replace the folder and rebuild. Requirements: C++17, Qt ≥ 6.5, CMake ≥ 3.21.
Static libraries are built by default; set `QPB_BUILD_SHARED=ON` for shared ones.

## Developing qpb

```sh
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/path/to/Qt/6.5.3/gcc_64
cmake --build build
ctest --test-dir build --output-on-failure
```

`tests/consumer` builds a throwaway application that embeds a copy of `qpb/` exactly like a consuming project.

## Documentation

- [Specification](docs/SPEC.md) ([Tiếng Việt](docs/SPEC-vi.md))
- [Implementation plan](docs/PLAN.md) ([Tiếng Việt](docs/PLAN-vi.md))
- [Reference use cases](docs/use-cases.md)
- [Original brainstorm (Vietnamese)](docs/brainstorm-vi.md)
