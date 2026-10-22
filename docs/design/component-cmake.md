# The component folder and its CMake files

`qpb/` is the unit of distribution: a project copies the folder as it is (for example to `components/qpb/`), adds two
lines of CMake and links `qpb::widgets` or `qpb::core`. Everything outside `qpb/` (the root `CMakeLists.txt`, `tests/`,
`examples/`, `tools/`, `docs/`, `rc-trial/`) exists only to develop the library, and is not in the release zip.

Because `qpb/CMakeLists.txt` runs inside someone else's project, it must not change that project. These are the rules
for it and for the helper files in `qpb/cmake/`; the leak check in `tests/consumer` enforces the ones it can.

## Rules

- **Paths.** Use `CMAKE_CURRENT_SOURCE_DIR`, `CMAKE_CURRENT_BINARY_DIR` and `CMAKE_CURRENT_LIST_DIR`, or the
  `PROJECT_*` variables of qpb's own `project()`. Never `CMAKE_SOURCE_DIR` or `CMAKE_BINARY_DIR`: in a host project they
  point at the host.
- **No global state.** Do not set `CMAKE_CXX_STANDARD`, `CMAKE_CXX_FLAGS`, `CMAKE_AUTOMOC`, `CMAKE_AUTORCC`, output
  directories, `BUILD_SHARED_LIBS` or similar, and do not call `add_compile_options`, `include_directories`,
  `add_definitions` or `link_libraries`. Everything goes through `target_*` commands and `set_target_properties()` on
  qpb's own targets (for example `AUTOMOC ON` on `qpb_core`). Reason: a global setting changes how the host builds its
  own code.
- **The C++ standard is a minimum.** `target_compile_features(<target> PUBLIC cxx_std_17)` (done once in
  `qpb_add_library`). A consumer on C++20 still works; the consumer test builds with both.
- **Warnings and Qt hygiene are private.** Warning flags (`/W4` or `-Wall -Wextra -Wpedantic`) and the defines
  `QT_NO_CAST_FROM_ASCII`, `QT_NO_CAST_TO_ASCII`, `QT_NO_URL_CAST_FROM_STRING` and `QT_DISABLE_DEPRECATED_*` are
  `PRIVATE`. They apply to qpb's sources and never to the consumer. In turn, the public headers have to be warning-free
  under the consumer's own strict flags.
- **Reuse the host's Qt.** If the target `Qt6::Widgets` exists, use Qt 6; else if `Qt5::Widgets` exists, use Qt 5.
  Otherwise find Qt 6, or Qt 5 when there is no Qt 6 (`find_package(QT NAMES Qt6 Qt5)`). Then check the minimum
  version: 6.5, or 5.15. Reason: one project must not load two different Qt builds.
- **Names.** Targets are `qpb_*` with the aliases `qpb::core` and `qpb::widgets`; options and cache variables are
  `QPB_*`. These names are API: renaming one is a breaking change (see
  [public-headers.md](public-headers.md)). The consumer test also allows new cache entries only with these prefixes.
- **Nothing but the libraries.** No tests or examples inside `qpb/`. No `install()` rules by default; if they are ever
  added, they sit behind an option `QPB_INSTALL` that is off.
- **No Qt resources, with one exception.** A `.qrc` in a static library would need `Q_INIT_RESOURCE` in the consumer's
  code, which would break "two lines of CMake". The one exception is `translations/qpb_translations.qrc` (the embedded
  `.qm` files); `loadTranslation()` registers that resource itself, once, so consumers do nothing. Icons come from
  `QStyle::standardIcon` or are painted in code.
- **No static initializers.** Types and editors must not register themselves from a static object: when linking
  statically the linker may drop that object file. The basic types are registered lazily in `TypeRegistry::global()`
  and `EditorFactory::global()`.
- **The version.** `qpb/VERSION` holds `MAJOR.MINOR.PATCH` with an optional suffix such as `-rc1`; it is the single source
  of the version. `project(qpb LANGUAGES CXX)` is called **without** `VERSION`: in a subdirectory,
  `project(VERSION)` sets the host's `CMAKE_PROJECT_VERSION` when the host has none. The file is added to
  `CMAKE_CONFIGURE_DEPENDS`, so replacing the folder with a newer one re-runs CMake and refreshes the generated version
  header.
- **Libraries.** Static by default, independent of the host's `BUILD_SHARED_LIBS`; `QPB_BUILD_SHARED=ON` builds shared
  libraries, and the consumer then deploys them next to the executable. Symbols are hidden by default and exported with
  `QPB_CORE_EXPORT` / `QPB_WIDGETS_EXPORT`; the static libraries are position independent so they can be linked into the
  consumer's shared libraries.
- **ASCII only (R6).** The check covers `CMakeLists.txt`, `.cmake` files and the `.in` templates under `qpb/`.

## The consumer check

`tests/consumer` builds throwaway applications from a project in `tests/consumer/project` that embeds a copy of `qpb/`
in `components/qpb/`, exactly as a consumer would. The project records its own CMake settings before
`add_subdirectory(components/qpb)` and fails to configure if qpb changed any of them (standard, flags, AUTOMOC, AUTOUIC,
AUTORCC, `BUILD_SHARED_LIBS`, output directories, `CMAKE_MODULE_PATH`, ...) or added a cache entry that does not belong
to qpb. Its own compile flags are strict (`/W4 /WX` or `-Wall -Wextra -Wpedantic -Werror`), so a warning in a public
header fails the build. The scenarios are static C++17 with the host finding Qt first, static C++20 with qpb finding Qt
itself, shared, and an in-place upgrade (replace `components/qpb` with a newer copy and rebuild in the same build tree).

Run it after every change in `qpb/CMakeLists.txt` or `qpb/cmake/`; see
[building-and-testing.md](../development/building-and-testing.md).
