# qpb — Implementation Plan

> Specification: [`SPEC.md`](SPEC.md). Original idea: [`brainstorm-vi.md`](brainstorm-vi.md).
> Vietnamese translation: [`PLAN-vi.md`](PLAN-vi.md). This English document is authoritative.
> Resource assumption: **part-time (~15 hours/week)**, one person.

## 0. Direction

Three goals drive the whole plan (SPEC §1.1):

1. **Stable API (G5):** projects already using qpb upgrade within a major version **without changing code**.
   → Design the API *before* implementing it (API-first); the first release for real projects is **1.0**, and the API is frozen from then on.
   0.x is internal to this repo.
2. **Distributed as a component folder (G6):** copy `qpb/` into a project's `components/qpb/`, add two lines of CMake.
   Update = replace the folder + rebuild. → The repo is organised around the `qpb/` folder from day one;
   tests and examples use it exactly like a consumer.
3. **Written from scratch (G7):** no wrapping/forking, no code copied from QtPropertyBrowser/QtnProperty.

Consequence: ~18 extra hours for API design, compatibility testing and packaging, plus a **trial (RC)** phase in a real project
before tagging 1.0.

---

## 1. Milestone overview

| Milestone | Weeks (estimate) | Verifiable outcome                                                                     |
|-----------|------------------|------------------------------------------------------------------------------------------|
| **M0** Component skeleton        | 1        | `qpb/` builds as a component; `tests/consumer` uses it via `add_subdirectory`            |
| **M1** 1.0 API design            | 1–2      | All 1.0 public headers written and reviewed; sample code compiles against them           |
| **M2** Core + Model              | 2–3      | `qpb::core` fully implemented, tests pass including `QAbstractItemModelTester`           |
| **M3** Tree view + editors       | 4–5      | Inspector edits all 7 types with full keyboard support → internal tag `0.1.0`            |
| **M4** Hardening & API freeze    | 6        | List mode, reset, compatibility tests, release packaging → `1.0.0-rc1`                   |
| **RC** Trial                     | 7–8      | rc1 runs the reference scenarios in a standalone app with no API change → tag `1.0.0`    |
| **M5** 1.1                       | after 1.0| Form view + filter (additive only)                                                        |
| **M6** 1.2                       | after 1.1| QObject adapter, serialization, Int64 (additive only)                                    |

M0–M4 ≈ 86 hours (≈ 6 part-time weeks). M5–M6 will be re-planned after 1.0 is used for real.

---

## 2. Work breakdown

Every task has an estimate (focused hours) and a **Done when** criterion.

### M0 — Component skeleton (≈ 8h)

**Status:** done. No real consuming project is available, so M0.1 uses the reference scenarios in `docs/use-cases.md`.
The optional `QPB_INSTALL` option from SPEC §6.3 is not added yet: there are no install rules to switch on (R4).

| ID    | Task                                                                                                  | h | Done when |
|-------|-------------------------------------------------------------------------------------------------------|---|-----------|
| M0.1  | Write `docs/use-cases.md`: 3 reference scenarios (no real project is available) with full property lists | 1 | File exists; used as input for M1 |
| M0.2  | Create the repo layout from SPEC §6.1: `qpb/{CMakeLists.txt,VERSION,LICENSE,CHANGELOG.md,include/qpb,src}`, dev root CMake | 2 | `cmake -S . -B build && cmake --build build` works |
| M0.3  | `qpb/CMakeLists.txt` follows **all** of SPEC §6.3 (no globals, static by default, conditional `find_package`, no `.qrc`) | 2 | §6.3 checklist reviewed line by line |
| M0.4  | `qpbglobal.h` driven by `VERSION`: `QPB_VERSION*`, `QPB_VERSION_CHECK`, export macros, `QPB_DEPRECATED_X`, `QPB_DISABLE_DEPRECATED` | 1 | Test checks `qpb::version()` |
| M0.5  | `tests/consumer/`: standalone CMake project; CTest copies `qpb/` into `components/qpb/` and builds a minimal app | 1 | CTest passes for both C++17 and C++20 |
| M0.6  | `.clang-format`, `.gitignore`, README update; (optional) GitHub Actions Ubuntu + Qt 6.5/6.8            | 1 | In the repo |

### M1 — 1.0 API design (≈ 10h) — *API-first*

**Status:** done. Headers in `qpb/include/qpb/`, compile-only examples in `examples/`, future-proofing sketches and
self-contained header checks in `tests/api/`, review in [`api-review.md`](api-review.md). Examples link once M2/M3 implement the API.

Goal: settle the shape of the API **before** any implementation depends on it, because it cannot change after 1.0.

| ID    | Task                                                                                                  | h | Done when |
|-------|-------------------------------------------------------------------------------------------------------|---|-----------|
| M1.1  | Write all 1.0 public headers (declarations + doc comments only): `Property`, `PropertyGroup`, builders, `Attr`, `Types`, `TypeRegistry`, `ValidationResult`, `PropertyModel`, `EditorFactory`, `PropertyDelegate`, `PropertyTreeView`, `qpb.h` | 4 | Every header follows SPEC §9.3 (d-pointer, no public data members, no inline logic) |
| M1.2  | Write against the headers (compile only, no linking): `examples/quickstart` (≤ 30 lines), `examples/custom_type` (QColor), and the reference scenarios from M0.1 | 2 | Compiles to object files; ergonomics self-assessed |
| M1.3  | **Future-proofing check:** sketch (do not implement) headers for `PropertyFormView`, `PropertyFilterProxyModel`, `QObjectPropertySource`, serialization and `Int64` using only the 1.0 public API | 2 | Nothing in the 1.0 API needs adding/changing; if it does → change the API **now** |
| M1.4  | API review against SPEC §9.2–9.3 checklists; record new decisions in SPEC Appendix B                  | 1 | Checklist signed off; SPEC updated |
| M1.5  | "Self-contained header" test (each public header included on its own) added to CTest                  | 1 | CTest passes |

After M1, every public header change must be justified in Appendix B (still allowed before 1.0, but with a reason).

### M2 — Core + Model (≈ 22h)

**Status:** done. `qpb::core` is implemented (`qpb/src/core/`) with 61 test functions in `tests/core/`
(`tst_property`, `tst_typeregistry`, `tst_propertymodel`, the last one under `QAbstractItemModelTester` in Fatal mode).
M2.3's "quickstart links" needs the widgets module (M3); the core part is proven by `tests/consumer`, which now builds a
model and runs the value pipeline from a statically linked copy of `qpb/` (this also covers M2.4's static-link check).

| ID    | Task                                                                                         | h | Done when |
|-------|----------------------------------------------------------------------------------------------|---|-----------|
| M2.1  | `Property` + d-pointer: data, flags, inherited effective state, `path()` (SPEC §4.2)          | 3 | Tests: readOnly/enabled/visible inherited across 3 levels; correct paths |
| M2.2  | `PropertyGroup`: `add`/`remove`/`find`, duplicate ids (SPEC §4.3)                             | 2 | Tests: nested find, remove, duplicate id creates no new node |
| M2.3  | Builders + `Attr::*` (SPEC §4.3–4.4)                                                          | 3 | The M1.2 quickstart now links and runs |
| M2.4  | `TypeRegistry` + 7 basic types: displayText, normalize, validate; lazy registration (SPEC §4.5, §4.7, §6.3) | 4 | Per-type tests; static-link test does not lose the basic types |
| M2.5  | `PropertyModel` read side: index/parent/data/flags/header, roles (incl. the `UserRole` range) | 3 | `QAbstractItemModelTester` (Fatal) passes on a 3-level tree |
| M2.6  | 6-step write pipeline + `valueChanged`/`validationFailed` (SPEC §4.6)                         | 3 | `QSignalSpy`: 1 signal on change, 0 when equal/read-only; error → `validationFailed` |
| M2.7  | `TreeObserver`: add/remove/metadata on a live model; `setRoot`                                | 3 | Tester passes while adding/removing at runtime |
| M2.8  | Nested batches + recursive `resetToDefault`                                                   | 1 | `batchValueChanged` emitted once by the outermost batch |

### M3 — Tree view + editors (≈ 24h)

**Status:** done, except the internal `0.1.0` tag (M3.8), which is left to the maintainer. `qpb::widgets` is implemented
(`EditorFactory` with the seven built-in editors, internal `PathEdit`, `PropertyDelegate`, `PropertyTreeView`); all five
examples now link and start. `tests/widgets` covers the factory (13 functions) and view/delegate interaction (19 functions:
Enter/Escape/focus-out, Tab/Shift+Tab skipping, enum and path commits, dialog focus protection, check boxes, validation tool
tip, modes, hidden rows, context-menu reset, proxy models). The manual pass used offscreen screenshots (`QPB_SCREENSHOT_DIR`).
The decision point after M3.6 did not trigger: no hacks beyond an event filter and a `moveCursor()` override were needed.

High-risk parts first: Int + String + FilePath (editor UX, focus while a dialog is open).

| ID    | Task                                                                                         | h | Done when |
|-------|----------------------------------------------------------------------------------------------|---|-----------|
| M3.1  | `EditorFactory` + `editorId` → `typeId` lookup + `notifyCommit` (SPEC §5.1–5.2)               | 2 | Headless tests create editors by typeId/editorId |
| M3.2  | `PropertyDelegate`: create/set/commit via the factory; Int + String                           | 3 | Editing Int changes the model; Esc cancels; focus-out commits |
| M3.3  | `PropertyTreeView` Mode::Tree: 2 columns, spanned groups, hidden rows per `IsVisibleRole`     | 3 | `examples/inspector` shows the Appendix A tree |
| M3.4  | `PathEdit` file/dir + FocusOut suppression while the dialog is open + test hook (SPEC §5.3–5.4, D6) | 4 | Fake dialog does not close the editor; selection commits immediately; native dialog checked by hand |
| M3.5  | Double, Enum (commit on selection), Bool (`CheckStateRole`, click + Space)                    | 3 | Per-type tests |
| M3.6  | Keyboard: Enter, Esc, Tab/Shift+Tab skipping groups/read-only                                 | 4 | `QTest::keyClick` across 5 properties with a group in between |
| M3.7  | Validation errors (cell tooltip), middle-elided paths, group painting, bold when modified     | 2 | Bad regex → old value kept + tooltip |
| M3.8  | Manual pass over the whole inspector; fix blockers; internal tag `0.1.0`                      | 3 | UX issue list; blockers fixed |

**Decision point (end of M3.6):** if Tab/FocusOut requires replacing `QAbstractItemView`'s editing machinery
→ consider making the form view (persistent editors) the primary 1.0 view. This decision **must be made before M4**
because it cannot change after 1.0.

### M4 — Hardening & API freeze (≈ 22h)

**Status:** everything that can be done without publishing is done; `1.0.0-rc1` itself (M4.9: tag, GitHub Release,
pushing `qpb-release`) waits for the maintainer. M4.1/M4.2 came with M3. New in M4: `tests/api_compat/v1_0.cpp` (M4.4),
consumer scenarios for shared builds, host-setting leaks and in-place upgrades (M4.5), `tools/check_architecture.cmake`
with a new rule R6 and `tools/count_lines.cmake` (M4.3, M4.6), Windows (MSVC) and macOS CI jobs (M4.7),
`tools/api_snapshot.py` with the baseline `tests/api_compat/api-1.0.txt` (M4.8), `tools/make_release.sh` and
[`RELEASING.md`](RELEASING.md) (M4.9), README integration and upgrade guide plus the final review in
[`api-review.md`](api-review.md) (M4.10). The checks found two real problems in the component's CMake (D31, D32).

| ID    | Task                                                                                         | h | Done when |
|-------|----------------------------------------------------------------------------------------------|---|-----------|
| M4.1  | Mode::List (D4) + switch button in the inspector                                              | 2 | Tree↔List keeps values and selection |
| M4.2  | Context menu Reset to default / Reset group                                                   | 1 | Action disabled when not modified |
| M4.3  | Finish examples; measure S1 (≤ 30 lines) and S2 (≤ 100 lines) with a script                   | 2 | Script runs in CTest |
| M4.4  | `tests/api_compat/v1_0.cpp`: exercises **all** 1.0 public API (build + run)                   | 3 | CTest passes; file frozen from here on |
| M4.5  | Extend `tests/consumer`: static/shared, C++17/C++20, no leaked globals (compare `CMAKE_*` before/after `add_subdirectory`), simulate "replace folder and rebuild" | 3 | CTest passes |
| M4.6  | `tools/check_arch.sh`: core includes no QtWidgets/QtGui; views never branch on typeId; public headers never include `src/` (R1, R2, R5) | 1 | Runs in CTest |
| M4.7  | Windows (MSVC) + macOS builds; public headers warning-free under `-Wall -Wextra -Wpedantic` / `/W4` | 3 | Clean on all 3 OSes |
| M4.8  | Baseline API snapshot (`tools/api_snapshot`) for comparisons from 1.1                          | 2 | Snapshot file in the repo |
| M4.9  | Release process (section 4): `CHANGELOG.md`, `qpb-1.0.0-rc1.zip` containing only `qpb/`, `qpb-release` branch via `git subtree split` | 3 | rc1 released on GitHub |
| M4.10 | Final API review against SPEC §9; README covers component integration + upgrade policy        | 2 | Review done |

### RC — Trial before freezing (1–2 weeks, alongside other work)

**Status:** round 1 (`1.0.0-rc1`) found two behaviour problems, fixed in `1.0.0-rc2`. Rounds 2 and 3 (`1.0.0-rc2`,
round 3 with wider API coverage and static/shared/Clang builds) needed no API or behaviour change, so RC.2 is done and
RC.3 (tag `1.0.0`) is next; see [`rc-trial.md`](rc-trial.md).
The trial application lives in `rc-trial/` and is rerun against every release candidate.

| ID    | Task                                                                                         | Done when |
|-------|----------------------------------------------------------------------------------------------|-----------|
| RC.1  | Build the three reference scenarios (M0.1) as a standalone application outside this repo, embedding `1.0.0-rc1` via `components/qpb/`; also any real project that appears by then | All scenarios work |
| RC.2  | Record every API pain point. If the API must change → change it, ship `rc2`, repeat RC        | One RC round needs no API change |
| RC.3  | Tag `1.0.0` (same content as the last RC, only `VERSION` changes)                              | Tag + release zip |

### M5 — 1.1 (additive only, re-planned after 1.0)

M5.1 `PropertyFormView` (SPEC §5.6) · M5.2 two-way form ↔ model sync · M5.3 `PropertyFilterProxyModel` + search box ·
M5.4 `multiline` attribute · M5.5 `tests/api_compat/v1_1.cpp` + API snapshot diff (additions only) · M5.6 release 1.1.0.

### M6 — 1.2 (additive only)

M6.1 `QObjectPropertySource` · M6.2 JSON/`QSettings` serialization · M6.3 `Types::Int64` · M6.4 `QUndoStack` example ·
M6.5 `v1_2.cpp` + snapshot diff · M6.6 release 1.2.0.

---

## 3. Task dependencies

```text
M0.2 ─► M0.3 ─► M0.4 ─► M0.5
M0.* ─► M1.1 ─► M1.2, M1.3 ─► M1.4 ─► M1.5
M1.* ─► M2.1 ─► M2.2 ─► M2.3
        M2.4 ─► M2.5 ─► M2.6 ─► M2.7 ─► M2.8
M2.4 ─► M3.1 ─► M3.2 ─► M3.3 ─► M3.4, M3.5 ─► M3.6 ─► M3.7 ─► M3.8
M3.* ─► M4.* ─► RC ─► 1.0.0 ─► M5 ─► M6
```

---

## 4. Release process (every 1.x release)

Step-by-step guide and tooling: [`RELEASING.md`](RELEASING.md), `tools/make_release.sh`.

1. Update `qpb/VERSION` and add an entry to `qpb/CHANGELOG.md` (Added / Changed / Deprecated / Fixed / **Upgrade notes**).
2. Green CI: unit tests, `api_compat/*` of **all** previous releases, `tests/consumer`, `check_arch`, API snapshot diff has additions only.
3. Add `tests/api_compat/v<x_y>.cpp` for the new (minor) release and update the snapshot.
4. Tag `vX.Y.Z` on `main`.
5. Produce artifacts: `qpb-X.Y.Z.zip` (the `qpb/` folder only) attached to the GitHub Release; update the `qpb-release` branch
   (`git subtree split --prefix qpb -b qpb-release`) for consumers using `git subtree pull`.

**Consumer side when upgrading:** replace `components/qpb/` → rebuild → read the *Upgrade notes*. For 1.x the expectation is "nothing to do".

---

## 5. Working agreements

- **Branches:** one branch per task/task group, e.g. `feat/M2.4-type-registry`; PR into `main`, squash merge.
- **Definition of Done for every PR:** warning-free build; tests for new behaviour; `ctest` passes (widgets `offscreen`);
  no violation of R1–R5 (SPEC §3); new public API has a user.
- **PRs touching `qpb/include/`:** the description states "API change: none / additive / breaking".
  After 1.0, "breaking" is rejected unless working on 2.0.
- **Commits:** Conventional Commits (`feat(core): ...`, `fix(widgets): ...`).
- **Language:** code, comments, commit messages and docs in English. `-vi` docs are Vietnamese reference translations.
- **No copied code** from QtPropertyBrowser/QtnProperty or differently licensed sources (G7); only their UX behaviour may be referenced.

---

## 6. Risks and scope cuts

| Risk                                          | Signal                                         | Response                                                                    |
|-----------------------------------------------|------------------------------------------------|-----------------------------------------------------------------------------|
| 1.0 API is wrong but already frozen            | Workarounds needed in the real project during RC | Extend RC; do not tag 1.0 while in doubt. After 1.0: add new API + deprecate the old |
| Schedule slip                                  | M3.6 not done by end of week 5                 | Cut from 1.0 (re-adding in 1.x is *additive*, not breaking): DirPath → Tab navigation → shared build. **Never** cut M1, M4.4–M4.6 |
| Editor UX needs deep hacks                     | M3.4/M3.6 exceed 2× estimate                   | Decision point at end of M3.6                                               |
| Unusual consumer CMake setup breaks the build  | RC.1 CMake errors                              | Add the case to `tests/consumer`, fix `qpb/CMakeLists.txt`                  |
| `TreeObserver` causes index bugs               | `QAbstractItemModelTester` fails               | 1.0 only allows structural changes via `setRoot`; runtime add/remove becomes additive in 1.1 (the `add/remove` API already exists; the limitation is documented) |
| Native dialogs differ across OSes              | Editor closes when the dialog opens on Windows/macOS | Fallback: close the editor before opening the dialog; write the result via `model->setData` |

---

## 7. Decisions settled / still open

| Question                                  | Status                                          |
|-------------------------------------------|-------------------------------------------------|
| C++17 or C++20?                           | **Settled: C++17**                              |
| Minimum Qt?                               | **Settled: 6.5**                                |
| Namespace / target names / include prefix | **Settled: `qpb`, `qpb::core`, `qpb::widgets`, `<qpb/...>`** |
| Build system of consuming projects        | **Settled: CMake only**                         |
| Language of code and docs                 | **Settled: English** (`-vi` files for reference)|
| How consumers sync the component folder   | Open — default: manual copy from the release zip |
| License                                   | Open — assumed MIT                              |
| Meaning of "custom property table"        | **Settled:** users build their own property tables with the library's public API (G1, G3); no extra concept |
| Primary data source                       | Open — assumed explicit builder; settle before M1.3 |
