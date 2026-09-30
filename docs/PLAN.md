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
round 3 with wider API coverage and static/shared/Clang builds) needed no API or behaviour change, so RC.2 is done. RC.3: `1.0.0` has the
content of `1.0.0-rc2` with only `VERSION` and `CHANGELOG.md` changed; see [`rc-trial.md`](rc-trial.md).
The trial application lives in `rc-trial/` and is rerun against every release candidate.

| ID    | Task                                                                                         | Done when |
|-------|----------------------------------------------------------------------------------------------|-----------|
| RC.1  | Build the three reference scenarios (M0.1) as a standalone application outside this repo, embedding `1.0.0-rc1` via `components/qpb/`; also any real project that appears by then | All scenarios work |
| RC.2  | Record every API pain point. If the API must change → change it, ship `rc2`, repeat RC        | One RC round needs no API change |
| RC.3  | Tag `1.0.0` (same content as the last RC, only `VERSION` changes)                              | Tag + release zip |

### M5 — 1.1 (additive only, re-planned after 1.0)

M5.1 `PropertyFormView` (SPEC §5.6) · M5.2 two-way form ↔ model sync · M5.3 `PropertyFilterProxyModel` + search box ·
M5.4 `multiline` attribute · M5.5 `tests/api_compat/v1_1.cpp` + API snapshot diff (additions only) · M5.6 release 1.1.0.

**Status:** M5.1–M5.5 done. `PropertyFormView` (`qpb/src/widgets/PropertyFormView.cpp`, 17 tests in
`tests/widgets/tst_propertyformview.cpp`), `PropertyFilterProxyModel` in `qpb::core` (7 tests), the `multiline` attribute
(tree view and form), `tests/api_compat/v1_1.cpp` and the snapshot `api-1.1.txt` (a strict superset of `api-1.0.txt`, which
is still checked). The search box is the application's `QLineEdit` wired to `setFilterFixedString()`, shown in
`examples/form_view`. Decisions D36–D39 in SPEC. M5.6: released as `1.1.0`.

### M6 — 1.2 (additive only)

M6.1 `QObjectPropertySource` · M6.2 JSON/`QSettings` serialization · M6.3 `Types::Int64` · M6.4 `QUndoStack` example ·
M6.5 `v1_2.cpp` + snapshot diff · M6.6 release 1.2.0.

**Status:** M6.1–M6.5 done. `QObjectPropertySource` (SPEC §4.9, 8 tests), `qpb::serialization` (§4.8, 10 tests incl. INI
and native `QSettings`), `Types::Int64` with an internal 64-bit spin box, `examples/object_editor` (QObject source +
`QUndoStack` + JSON), `tests/api_compat/v1_2.cpp` and `api-1.2.txt` (superset of 1.1). The compile check caught that free
functions in `qpb` break unqualified application calls through ADL, hence the nested namespace (D40). Decisions D40–D42.
M6.6: released as `1.2.0`, after RC trial round 4 (docs/rc-trial.md) found and fixed F7.

### M7 — 1.3 (additive only): `QObjectPropertySource` ergonomics (F8, F9)

Planned after RC trial round 4 ([`rc-trial.md`](rc-trial.md)). Both findings are solved with additions; code written
for 1.0–1.2 builds and behaves the same.

**F8 — group titles.** A group made from a `QObject` is titled with its id (the object name, e.g. `studio_mic`). Ids stay
as they are (paths must be stable for serialization and application code); only the **display name** of the group changes:

- `Q_CLASSINFO("qpb:title", "name")`: the group's display name is the value of that Q_PROPERTY, updated through its
  NOTIFY signal. The class decides.
- `QObjectPropertySource::setTitleProperty(const QString& name)`: the same for objects whose class has no such class
  info, for classes the application cannot change (e.g. from a library). Applies to objects added afterwards; the class
  info wins.
- Neither set: unchanged (the id), as in 1.2.

**F9 — values maintained by the application.** Some properties are not settings but live values (a counter, a status,
the space used). They should never look "modified", be reset or be saved.

- New flag `Property::Flag::Live` (appended, `0x8`), with `isLive()` / `setLive()` and `PropertyBuilderBase::live()`.
  A live property: `isModified()` is always `false` (so views never show it in bold and `IsModifiedRole` is `false`);
  "Reset to default" and `resetToDefault()` leave it alone; `qpb::serialization` neither writes nor reads it. It stays
  editable unless it is also read-only.
- `QObjectPropertySource`: metadata key `live` (`Q_CLASSINFO("qpb:used", "live")`).
- Open decision (D44): should a Q_PROPERTY **without WRITE but with NOTIFY** become live automatically? It is what the
  RC trial wanted, but it changes 1.2 behaviour (such properties are bold after a change today). Proposal: keep 1.2
  behaviour by default and add `QObjectPropertySource::setLiveReadOnlyProperties(bool)` (default `false`), so an
  application opts in with one call. Alternative: treat it as a bug fix (SPEC §9.1 allows fixing behaviour that
  contradicts the documentation) and call it out in the CHANGELOG.

| ID   | Task                                                                                              | Est. (h) | Done when |
|------|---------------------------------------------------------------------------------------------------|----------|-----------|
| M7.1 | SPEC: §4.2 (Live flag), §4.8 (serialization skips live), §4.9 (title, `live`), D43 (titles), D44 (Live); `-vi` | 1 | Decisions recorded |
| M7.2 | `Flag::Live`, `isLive()`/`setLive()`, builder `live()`; `isModified()`, reset and serialization honour it | 2 | Core tests: modified, reset (single and group), JSON and QSettings skip it |
| M7.3 | Views: no code change expected (they use `IsModifiedRole` and the shared reset helpers); tests that a live property is never bold and the reset menu ignores it, in tree and form | 1 | Widget tests pass |
| M7.4 | `QObjectPropertySource`: `qpb:title`, `setTitleProperty()`, `live` key, `setLiveReadOnlyProperties()` (if D44 is accepted); title follows NOTIFY | 3 | Source tests: title set/updated/removed with the object, live metadata |
| M7.5 | `tests/api_compat/v1_3.cpp` + `api-1.3.txt` (superset of 1.2); `examples/object_editor` shows titles and a live value | 1 | Compat and snapshot tests pass |
| M7.6 | RC trial round 5: the Devices page uses `qpb:title` and `live`; F8 and F9 closed, no new behaviour change | 1 | Trial tests pass, report updated |
| M7.7 | Release 1.3.0 (PR, green CI, zip, `qpb-release`)                                                    | 0.5 | Release zip |

Total ≈ 9.5 h.

**Status:** M7.1–M7.6 done with the proposed D44 (opt-in `setLiveReadOnlyProperties()`). `Flag::Live` (core, 4 test
functions across property, model, serialization), titles and live values in `QObjectPropertySource` (3 tests), tree and
form views unchanged (2 tests), `tests/api_compat/v1_3.cpp` and `api-1.3.txt` (superset of 1.2), `examples/object_editor`
with titles, a live counter and undo that skips it, SPEC §4.2, §4.8, §4.9, D43, D44. RC round 5 closed F8 and F9.
M7.7: released as `1.3.0`.

### M8 — 1.4 (additive only): reacting to values (F3, F10)

Planned after the RC trial rounds 1 and 4 ([`rc-trial.md`](rc-trial.md)). In every scenario the application listens to
`PropertyModel::valueChanged` and compares `path` strings: once to copy edited values back into its own data (F3, an
`if (path == ...)` chain) and once to enable or show a property depending on another one (F10). 1.4 adds a direct way
for both; code written for 1.0–1.3 builds and behaves the same.

**F3 — a callback per property.** On the model, by path, the way Qt connections work:

```cpp
QMetaObject::Connection PropertyModel::onValueChanged(const QString& path, const QObject* context,
    std::function<void(const QVariant& value)> handler);

model.onValueChanged("Transform/x", this, [this](const QVariant& v) { m_object.x = v.toDouble(); });
```

- Called after `valueChanged` for that exact path, from user edits and application writes alike. `context` ends the
  connection when it is destroyed; the returned connection can be disconnected.
- By path, not by `Property*`, so it survives `setRoot()` and properties being removed and added again (the inspector
  replaces its tree for every selected object).
- A path of a group also reports changes of its descendants, with a second overload taking
  `std::function<void(const QString& path, const QVariant& value)>`.
- Open decision **D45:** on the model (proposed; the model already owns change notification and paths) or on
  `Property` (works without a model, but `Property` is not a QObject, so lifetime and disconnection need a handle type of
  their own). Proposal: the model.

**F10 — conditions between properties.** Declared once, evaluated by the model:

```cpp
general.addInt("autosaveMinutes", 5).enabledWhen("General/autosave");          // bool true / non-empty value
camera.addDouble("orthoScale", 1.0).visibleWhen("Camera/projection", 1);        // equals a value
limits.addInt64("quota", 0).enabledWhen("Limits/mode", [](const QVariant& v) { return v != "unlimited"; });
```

- `Property::setEnabledWhen()` / `setVisibleWhen()` (+ builder methods), each with three forms: source is truthy,
  source equals a value, or a predicate on the source value. One condition of each kind per property; `clear...()`
  removes it.
- The condition is **combined** with the property's own flags (`isEnabled()` = own flag and condition and ancestors),
  so it never overwrites what the application set with `setEnabled()` / `setVisible()`. Views need no change: they
  already follow the effective state.
- The model evaluates conditions when a source value changes, when the tree changes (`setRoot()`, rows added or
  removed) and when a condition is set. A property that is not in a model, or whose source path does not exist, keeps
  the condition "true" (nothing is hidden or disabled by a typo; a warning is logged once).
- `QObjectPropertySource`: metadata keys `enabledWhen=<id>` and `visibleWhen=<id>`, relative to the object's group.
- Open decision **D46:** combine with the own flags (proposed) or have the model write the flags `Disabled` / `Hidden`
  (simpler, but it overwrites the application's own `setEnabled(false)`).
- Out of scope: conditions on several sources, computed values, read-only conditions. The predicate form covers most
  cases (it may read other properties through the model it captures); a general rule API can come later.

| ID   | Task                                                                                              | Est. (h) | Done when |
|------|---------------------------------------------------------------------------------------------------|----------|-----------|
| M8.1 | SPEC: §4.2 (conditions), §4.6 (`onValueChanged`), §4.9 (metadata keys), D45, D46; `-vi`             | 1 | Decisions recorded |
| M8.2 | `PropertyModel::onValueChanged()` (exact path, group path, context lifetime, disconnect, `setRoot()`) | 2 | Model tests pass |
| M8.3 | Conditions in core: storage in `PropertyPrivate`, effective `isEnabled()` / `isVisible()`, evaluation in the model, change notifications, missing sources | 4 | Core tests: all three forms, flags combined, nested groups, structure changes, batches |
| M8.4 | Builders, `QObjectPropertySource` metadata; views: tests that tree and form follow conditions (no view code expected) | 2 | Widget and source tests pass |
| M8.5 | `tests/api_compat/v1_4.cpp` + `api-1.4.txt` (superset of 1.3); examples `settings_dialog` and `inspector` without `valueChanged` if-chains | 1.5 | Compat, snapshot and example builds pass |
| M8.6 | RC trial round 6: the trial replaces its `valueChanged` code with `onValueChanged()` and `enabledWhen()`; F3 and F10 closed | 1.5 | Trial tests pass, report updated |
| M8.7 | Release 1.4.0 (PR, green CI, zip, `qpb-release`)                                                    | 0.5 | Release zip |

Total ≈ 12.5 h.

**Status:** M8.1–M8.6 done with D45 (on the model) and D46 (combined with the own flags). `onValueChanged()` (2 test
functions), conditions (`tests/core/tst_conditions.cpp`, 13 test functions), `QObjectPropertySource` metadata, tree and
form views unchanged (2 tests), `tests/api_compat/v1_4.cpp` and `api-1.4.txt` (superset of 1.3); examples
`settings_dialog` (no `valueChanged` code left) and `inspector` (`visibleWhen`, `onValueChanged`); SPEC §4.2, §4.6, §4.9,
D45, D46. Found on the way: a literal `0` value was ambiguous with the predicate overload (`int` overloads added), and
clearing an unmet condition did not notify views (fixed). RC round 6 closed F3 and F10. M8.7: released as `1.4.0`.

### M9 — 1.5 (additive only): resetting everything and check boxes in the Tab chain (F4, F5)

Planned after RC trial rounds 1 and 3 ([`rc-trial.md`](rc-trial.md)): the two findings that were kept for 1.0 because
they could be solved later without breaking anything. Code written for 1.0–1.4 builds and behaves the same. 1.5 also
ships the fix already listed under *Unreleased* in the CHANGELOG (MSVC warning C4458 when building `qpb/`).

**F4 — reset the whole tree from the model.** `PropertyModel::resetToDefault()` takes an index and the root has none,
so applications write `model.root()->resetToDefault()`. 1.5 adds the call one looks for on the model:

```cpp
bool PropertyModel::resetAllToDefault();

connect(resetButton, &QPushButton::clicked, &model, &PropertyModel::resetAllToDefault);
```

- Same effect as `root()->resetToDefault()`: application code, so read-only and disabled properties are reset too
  (D34) and live ones are left alone (D44); one `batchValueChanged` for the whole tree. Returns false if any reset was
  rejected; true for a model without a root (nothing to reset).
- `resetToDefault(QModelIndex())` keeps returning false: making it reset everything would change documented behaviour
  (§9.2).

**F5 — check boxes in the Tab chain.** In `PropertyTreeView`, Tab / Shift+Tab chain the editors and skip Bool
properties (SPEC §5.5), which are check boxes without an editor; keyboard users reach them with the arrow keys.

```cpp
view.setTabStopsOnCheckBoxes(true);   // also a Q_PROPERTY
```

- Off by default: the 1.4 chain is unchanged.
- On: Tab / Shift+Tab also stop on the value cell of a Bool property that the user can change (enabled, not read-only,
  visible). No editor opens there; Space toggles it (as today), and the next Tab / Shift+Tab goes on along the chain,
  leaving the view at either end as before.
- Tree view only: in `PropertyFormView` check boxes are widgets and already take part in the Tab chain.
- Open decision **D49:** opt-in (proposed) or the new default. The default would change what keyboard users of every
  consuming application get after an upgrade, which §9.2 forbids; the opt-in follows D44.
- Decision **D48** records the F4 choice: a new method instead of giving `resetToDefault(QModelIndex())` a meaning.

| ID   | Task                                                                                              | Est. (h) | Done when |
|------|---------------------------------------------------------------------------------------------------|----------|-----------|
| M9.1 | SPEC: §4.6 (`resetAllToDefault`), §5.5 (`tabStopsOnCheckBoxes`), §8 (1.5), D48, D49; `-vi`           | 1 | Decisions recorded |
| M9.2 | `PropertyModel::resetAllToDefault()`                                                              | 1 | Model tests: nested groups, read-only reset, live skipped, one `batchValueChanged`, rejected reset → false, no root → true |
| M9.3 | `PropertyTreeView::setTabStopsOnCheckBoxes()`: `moveCursor()` and Tab / Shift+Tab from a check box row | 2.5 | Widget tests: off keeps the 1.4 chain; on stops on check boxes both ways, Space toggles, the next Tab opens the next editor, disabled / read-only / hidden check boxes are skipped, through a proxy, at the end of the view |
| M9.4 | `tests/api_compat/v1_5.cpp` + `api-1.5.txt` (superset of 1.4); `examples/settings_dialog` uses both additions | 1 | Compat, snapshot and example builds pass |
| M9.5 | RC trial round 7: the settings page uses `resetAllToDefault()` and `setTabStopsOnCheckBoxes(true)`; F4 and F5 closed | 1.5 | Trial tests pass, report updated |
| M9.6 | Release 1.5.0 (PR, zip, tags `v1.5.0` and `qpb-v1.5.0`, `qpb-release`, GitHub Release)               | 0.5 | Release zip |

Total ≈ 7.5 h. GitHub Actions is disabled for now (the account cannot start jobs), so "green CI" means the full
`ctest` run locally (Windows, MSVC, Qt 6.11) until it is enabled again; Linux and macOS are not covered meanwhile.

**Status:** M9.1–M9.5 done with D48 and D49 as proposed (a new method; opt-in). `resetAllToDefault()` (1 test
function), `setTabStopsOnCheckBoxes()` with a `focusNextPrevChild()` override so Tab on a check box goes on along the
chain (4 test functions: both directions, skipped check boxes, end of the view, proxy), `tests/api_compat/v1_5.cpp` and
`api-1.5.txt` (superset of 1.4), `examples/settings_dialog` with "Restore Defaults" and check boxes in the Tab chain,
SPEC §4.6, §5.5, §8, D48, D49. `ctest` passes 30/30 locally. RC round 7 closed F4 and F5. M9.6: released as
`1.5.0`.

### M10 — 1.6 (additive only): theming with style sheets (QSS)

Requested by the maintainer: applications theme their whole UI with a style sheet and want the property browser to
follow it. A probe with an application-wide dark sheet (like the ones real applications use: rules for `QWidget`,
`QLabel`, `QToolButton:checked`, `QTreeView::item:hover`, editors) showed that the standard Qt selectors already cover
most of qpb (tree view, header, item hover/selection, alternate rows, scroll bars, tool tips, menus and every editor, in
cells and in the form), but not four things qpb draws or sets up itself:

1. **Tree view, group rows:** their background is the palette's `Button`, which a sheet's `background-color` sets to
   the colour of every other row, so group rows no longer stand out; and no selector can address group rows (items are
   not widgets).
2. **Tree view, emphasis colours:** names of modified properties are bold and read-only values dimmed
   (`PlaceholderText`), but a sheet can change neither colour.
3. **Form view, modified labels lose their bold font** under any sheet with a rule for `QLabel` (Qt resets the fonts of
   styled child widgets) — a bug: the "modified" cue disappears.
4. **Form view, group titles** are plain checkable `QToolButton`s: they pick up the application's generic
   `QToolButton:checked` rule while expanded, and nothing identifies them for a rule of their own.

1.6 adds hooks for these; code written for 1.0–1.5 builds and behaves the same, with or without a style sheet.

**Tree view: colour properties, settable from a sheet with `qproperty-`.**

```cpp
Q_PROPERTY(QBrush groupBackground ...)     // default: palette Button (as before)
Q_PROPERTY(QColor groupForeground ...)     // default: the item text colour
Q_PROPERTY(QColor modifiedForeground ...)  // default: the item text colour (names stay bold)
Q_PROPERTY(QColor readOnlyForeground ...)  // default: palette PlaceholderText (as before)
```

```css
qpb--PropertyTreeView {
    qproperty-groupBackground: #2c3038;
    qproperty-modifiedForeground: #e87c00;
}
```

- An invalid colour / `Qt::NoBrush` means the default. The delegate passes them to the style in the item's
  `backgroundBrush` and `Text` colour, which the style sheet style keeps (checked in Qt 6.5 and 6.11 sources); a
  matching `::item` rule with a `color` or `background` still wins, as in any Qt view.
- Colours only: `qproperty-` cannot set fonts, and bold (groups, modified names) is the structural cue.

**Form view: stable selectors on its widgets (dynamic properties).**

| Widget | Selector |
|---|---|
| a group's section / title button / body | `QWidget[qpbPart="group"]`, `QToolButton[qpbPart="groupTitle"]`, `QWidget[qpbPart="groupBody"]` |
| a property's label | `QLabel[qpbPart="label"]`, and `[qpbModified="true"]` while it is modified |
| the read-only text of a type without editor | `QLabel[qpbPart="value"]` |
| the browse button of path editors (both views) | `QToolButton[qpbPart="browse"]` |

- `qpbModified` is updated with the value and the label is re-polished, so rules for it apply at once.
- Bold labels and titles are re-applied after a style sheet has reset their font (fixes 3.).
- The C++ class names of internal widgets (`qpb--detail--PathEdit`, ...) are not part of the API; only these properties
  are.

**Example `examples/custom_theme`:** one sheet template with `@{token}` placeholders and a dark and a light token table
(graphite and orange), switched at runtime; a tree view and a form view on the same model; it styles standard widgets
with standard selectors and qpb's parts with the hooks above.

Decisions: **D50** colour properties on the tree view (items are not widgets; `qproperty-` is Qt's way to reach
painted parts from a sheet); **D51** dynamic properties `qpbPart` / `qpbModified` as the form view's style sheet API,
class names of internal widgets excluded; **D52** emphasis fonts are kept under style sheets (re-applied after a sheet
resets them).

| ID    | Task                                                                                           | Est. (h) | Done when |
|-------|------------------------------------------------------------------------------------------------|----------|-----------|
| M10.1 | SPEC: §5.5 (colour properties), §5.6 (selectors, bold kept), new §5.8 (style sheets), §8 (1.6), D50–D52; `-vi` | 1.5 | Decisions recorded |
| M10.2 | Tree view: the four properties, used by the delegate for group rows, modified names, read-only values | 3 | Widget tests: defaults, set by `qproperty-` from a sheet, pixels of group rows and emphasis colours under a sheet with `::item` rules |
| M10.3 | Form view and path editor: `qpbPart`, `qpbModified` with re-polish, bold kept under sheets       | 2.5 | Widget tests: selectors match, `qpbModified` follows edits and resets, labels stay bold with a `QLabel` rule |
| M10.4 | `tests/api_compat/v1_6.cpp` + `api-1.6.txt`; `examples/custom_theme` (+ Qt Creator projects)       | 2 | Compat, snapshot and example builds pass; screenshots checked in both themes |
| M10.5 | RC trial round 8: the trial application runs under a dark sheet using the hooks; new findings recorded | 1.5 | Trial tests pass, report updated |
| M10.6 | Release 1.6.0                                                                                    | 0.5 | Release zip |

Total ≈ 11 h.

**Status:** M10.1–M10.5 done with D50–D52 as proposed. The probe also corrected two first guesses: group rows did not
lose their background to `::item` rules, a sheet's `background-color` gives the palette's `Button` the rows' colour; and
the style sheet style keeps an item's `Text` colour (Qt 6.5 and 6.11 sources), so no `WindowText` workaround was needed.
Colour properties on `PropertyTreeView` (1 test function with pixel checks, also for a winning `::item` rule),
`qpbPart` / `qpbModified` on the form's widgets and the browse button, bold re-applied after a sheet resets it (3 test
functions; the regression test fails without the fix), `tests/api_compat/v1_6.cpp` and `api-1.6.txt` (superset of 1.5),
`examples/custom_theme` (screenshots checked: dark, light, no sheet), SPEC §5.5, §5.6, §5.8, §8, D50–D52. `ctest`
passes 33/33 locally. RC round 8 ran the trial under an application sheet with the hooks; F11 and F12 are Qt behaviour,
documented. M10.6 (release 1.6.0) waits for the maintainer.

---

## 3. Task dependencies

```text
M0.2 ─► M0.3 ─► M0.4 ─► M0.5
M0.* ─► M1.1 ─► M1.2, M1.3 ─► M1.4 ─► M1.5
M1.* ─► M2.1 ─► M2.2 ─► M2.3
        M2.4 ─► M2.5 ─► M2.6 ─► M2.7 ─► M2.8
M2.4 ─► M3.1 ─► M3.2 ─► M3.3 ─► M3.4, M3.5 ─► M3.6 ─► M3.7 ─► M3.8
M3.* ─► M4.* ─► RC ─► 1.0.0 ─► M5 ─► M6 ─► M7 ─► M8 ─► M9 ─► M10
```

---

## 4. Release process (every 1.x release)

Step-by-step guide and tooling: [`RELEASING.md`](RELEASING.md), `tools/make_release.sh`.

1. Update `qpb/VERSION` and add an entry to `qpb/CHANGELOG.md` (Added / Changed / Deprecated / Fixed / **Upgrade notes**).
2. Green CI: unit tests, `api_compat/*` of **all** previous releases, `tests/consumer`, `check_arch`, API snapshot diff has additions only.
3. Add `tests/api_compat/v<x_y>.cpp` for the new (minor) release and update the snapshot.
4. Tag `vX.Y.Z` on `main`.
5. Produce artifacts: `qpb-X.Y.Z.zip` (the `qpb/` folder only) attached to the GitHub Release; update the `qpb-release` branch
   (`git subtree split --prefix qpb -b qpb-release`) and tag its tip `qpb-vX.Y.Z`, for consumers using `git subtree` or
   `git submodule` (SPEC §6.2, D47).

**Consumer side when upgrading:** replace `components/qpb/` (zip, `git subtree pull` or submodule checkout of
`qpb-vX.Y.Z`) → rebuild → read the *Upgrade notes*. For 1.x the expectation is "nothing to do".

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
| How consumers sync the component folder   | **Settled:** release zip (default), `git subtree` or `git submodule` from `qpb-release`, pinned by `qpb-vX.Y.Z` tags (SPEC §6.2, D47) |
| License                                   | Open — assumed MIT                              |
| Meaning of "custom property table"        | **Settled:** users build their own property tables with the library's public API (G1, G3); no extra concept |
| Primary data source                       | Open — assumed explicit builder; settle before M1.3 |
