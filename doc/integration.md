# Integration

How to get the qpb folder, add it to a CMake project, choose between static and shared libraries, use it with Qt 5, and
update it. Back to the [documentation index](../README.md#documentation).

Contents: [Getting the folder](#getting-the-folder) - [CMake](#cmake) - [Static or shared](#static-or-shared) -
[Qt 5](#qt-5) - [Platforms](#platforms) - [Upgrading](#upgrading)

## Getting the folder

The `qpb/` folder is the whole library: headers, sources, CMake files, translations, this documentation. Put it in your
project as `components/qpb/` (any path works; the examples use this one):

```text
my-app/
  CMakeLists.txt
  components/
    qpb/          <- this folder
  src/
```

There are three ways to get and update it. All three give the same `components/qpb/` with the content of the release
zip, so the CMake lines below are the same for each.

| Method | Suits | Pinned to a version by |
|---|---|---|
| Release zip (default) | Any project, no git needed | The zip that was extracted (`components/qpb/VERSION`) |
| `git subtree` | Git projects that commit the folder | The tag in the `add` / `pull` command |
| `git submodule` | Git projects already using submodules | The commit recorded for `components/qpb` |

**Release zip.** Each release is a zip, `qpb-X.Y.Z.zip`, that contains the `qpb/` folder and nothing else. Delete the
old folder and extract the new one in its place, first time and every update:

```sh
rm -rf components/qpb && unzip qpb-X.Y.Z.zip -d components     # the zip contains qpb/
```

**git subtree and git submodule.** Both work from a clone of the qpb repository (`<remote>` below is the path or URL of
that clone). Every release publishes the branch `qpb-release`, whose history is the `qpb/` folder alone and whose tip is
always the newest release, and the tag `qpb-vX.Y.Z` on the commit of that release. Use these tags: the plain `vX.Y.Z`
tags mark the whole repository and cannot be used here, because subtree and submodule need the component at the root.

```sh
# git subtree - the folder is committed in your repository
git subtree add  --prefix components/qpb <remote> qpb-vX.Y.Z --squash     # first time
git subtree pull --prefix components/qpb <remote> qpb-vX.Y.Z --squash     # update

# git submodule - your repository records a commit of qpb-release
git submodule add -b qpb-release <remote> components/qpb                  # first time, then pin:
git -C components/qpb fetch --tags && git -C components/qpb checkout qpb-vX.Y.Z
git add components/qpb && git commit -m "Use qpb X.Y.Z"                   # same two lines to update
```

With a submodule, `git submodule update --remote components/qpb` moves to the newest release (the tip of
`qpb-release`). The submodule's clone also downloads the development history, but only the component is checked out.

Do not edit files in the folder. The zip overwrites a local change, subtree reports it as a merge conflict, and a
submodule needs it committed inside the submodule. Changes belong in the qpb repository.

## CMake

```cmake
find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
add_subdirectory(components/qpb)
target_link_libraries(my_app PRIVATE qpb::widgets)   # pulls in qpb::core
```

```cpp
#include <qpb/qpb.h>       // everything; needs qpb::widgets
#include <qpb/qpbcore.h>   // qpb::core only: no QtWidgets or QtGui headers
```

Linking a target brings its include directory, the C++17 requirement and the Qt libraries it needs. Nothing else has
to be set up: there is nothing to initialise, no resource to import and no plug-in to load. Types and editors register
themselves the first time they are used.

| Target | Contains | Depends on |
|---|---|---|
| `qpb::core` | properties, model, types, validation, serialization, QObject source, filter proxy, translations | Qt Core |
| `qpb::widgets` | tree view, form view, delegate, editors | `qpb::core`, Qt Widgets |

Code that uses only the model (a command-line tool, a server, a unit test) links `qpb::core` and needs no GUI libraries.

**Options.** One cache option, set on the command line or before `add_subdirectory`:

| Option | Default | Meaning |
|---|---|---|
| `QPB_BUILD_SHARED` | `OFF` | Build qpb as shared libraries instead of static ones (see below). Independent of your own `BUILD_SHARED_LIBS` |

The folder has no install step and no package configuration file: it is meant to be used with `add_subdirectory`.

**Which Qt is used.** qpb uses the Qt your project has already found. If `Qt6::Widgets` exists when
`add_subdirectory` runs, Qt 6 is used; otherwise if `Qt5::Widgets` exists, Qt 5. If your project has not found Qt, qpb
looks for it itself: Qt 6 first, then Qt 5. qpb checks the minimum version (6.5 or 5.15) and stops with an error
message that names the version it found. To make qpb use Qt 5 on a machine that has both, call `find_package(Qt5 5.15
REQUIRED COMPONENTS Widgets)` before `add_subdirectory`. Telling CMake where Qt is (`CMAKE_PREFIX_PATH`) is your job, as
for any Qt project.

**Your project stays as it is.** The folder changes none of your CMake settings: the C++ standard, compiler flags,
output directories and AUTOMOC are set on qpb's own targets only. C++17 is a minimum, so your project can build with
C++20. The warning flags used to build qpb are private; the public headers compile without warnings under `/W4` and
under `-Wall -Wextra -Wpedantic`.

**Version.** The version of the folder is in the file `VERSION`. CMake runs again when that file changes, for example
after you replace the folder. At compile time the version is available as `QPB_VERSION` and `QPB_VERSION_STR`, at run
time as `qpb::version()`; see [Compatibility](compatibility.md#checking-the-version).

## Static or shared

**Static** is the default. The qpb libraries are linked into your executable; there is nothing to deploy. They are
built position independent, so they can also be linked into your own shared libraries.

**Shared** (`-DQPB_BUILD_SHARED=ON`) builds `qpb::core` and `qpb::widgets` as shared libraries, named `qpbcore` and
`qpbwidgets`. Deploy both next to your executable (or wherever your platform finds them): on Windows the two DLLs, on
Linux the `.so` files with their version links. `qpb::widgets` needs `qpb::core`, so ship both even when you link only
`qpb::widgets`. Build the libraries and your application from the same version of the folder; to catch a mismatch at
run time, compare `qpb::version()` with `QPB_VERSION_STR`:

```cpp
if (std::strcmp(qpb::version(), QPB_VERSION_STR) != 0)
    qWarning("qpb headers and library differ");
```

Compatibility within 1.x is source compatibility, not binary compatibility: a shared library of another version is not
a drop-in replacement. Rebuild your application after each update.

## Qt 5

Since 1.7 the folder also builds with Qt 5.15. The CMake lines are the same, with Qt 5 found first:

```cmake
find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets)
add_subdirectory(components/qpb)
target_link_libraries(my_app PRIVATE qpb::widgets)
```

Everything in this documentation applies to both Qt majors, except two API differences where Qt 5 has no equivalent;
they are listed in [Compatibility](compatibility.md#qt-5-and-qt-6). Use one Qt major for qpb and your application.

Compilers: Qt 5.15's own headers do not compile with the default toolset of MSVC 2026 (they use
`stdext::checked_array_iterator`, which its standard library no longer has). Use the MSVC 2019 (14.29) or 2022 toolset,
GCC or Clang with Qt 5.15; Visual Studio 2026 works with the 14.29 toolset installed and selected
(`vcvarsall x86 -vcvars_ver=14.29`).

## Platforms

- **Windows** (MSVC 2019 or newer; Qt 6 with MSVC 2022 or newer, Qt 5.15 with the MSVC 2019 or 2022 toolset):
  verified for every release by a full test run in both Qt configurations.
- **Linux** (GCC, Clang): supported. The code and the CMake files are portable, but Linux is not built for every release.
- **macOS**: not supported.

Only Qt Core, Gui and Widgets are used (Test for qpb's own tests). There are no other dependencies.

## Upgrading

1. Replace the folder as a whole: delete `components/qpb/` and put the new version in its place (or use the subtree or
   submodule command above). Never extract a new version over the old folder: files that the new version removed would
   stay.
2. Rebuild your project. CMake runs again because `VERSION` changed.
3. Read the *Upgrade notes* of the new version, and of every version in between, in [CHANGELOG.md](../CHANGELOG.md).
   Most of them say "nothing to do".

Within 1.x neither your code nor your CMake needs to change; see [Compatibility](compatibility.md). To find uses of API
that is going away, define `QPB_DISABLE_DEPRECATED` once, as described there.
