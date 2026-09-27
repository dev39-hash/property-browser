# qpb — RC trial report

The release-candidate trial of docs/PLAN.md (RC.1, RC.2): the three reference scenarios of
[`use-cases.md`](use-cases.md) built as one standalone application, [`rc-trial/`](../rc-trial), that embeds a released
component folder exactly as a consuming project does (`rc-trial/run_trial.sh <qpb-x.y.z.zip>`). Nothing else from this
repository is used. `rc-trial/tests/tst_trial.cpp` drives the application through its UI (offscreen) and saves
screenshots of each page.

## Round 1 — `1.0.0-rc1`

Integration: the zip extracted into `components/qpb/`, two lines of CMake, no warnings. 9 of 10 trial tests passed.

| # | Finding | Kind | Outcome |
|---|---------|------|---------|
| F1 | `Property::setValue()` / `PropertyModel::setValue()`, the application-code API, rejected read-only and disabled properties. The settings page could not update its read-only "last saved" field, and "Reset all" only worked because `autosave` happened to be reset before the dependent `autosaveMinutes`. | Documented behaviour | **Changed in rc2** (SPEC D34): read-only/disabled only block user edits through views (`setData`). The view's reset menu is a user action and leaves read-only/disabled properties alone. |
| F2 | The name column kept Qt's default width; names such as "language", "projectDir" and "Field of view" were cut off. | UI behaviour | **Changed in rc2** (SPEC D35): the column fits its contents until a width is set. |
| F3 | Writing edited values back into application data means matching `path` strings in `valueChanged` (an `if (path == ...)` chain). | Ergonomics | Kept for 1.0. A per-property change callback can be added in 1.x without breaking anything. |
| F4 | Resetting a whole tree goes through `model.root()->resetToDefault()`; `PropertyModel::resetToDefault()` needs an index and the root has none. | Ergonomics | Kept; `root()->resetToDefault()` is short and documented. |

No public signature had to change; F1 changes documented behaviour, so a second release candidate is required.

## Round 2 — `1.0.0-rc2`

All 10 trial tests pass against the rc2 zip, including the read-only status update and "Reset all" in any order;
screenshots show the full property names. Exit criterion for `1.0.0`: a round without API or behaviour changes.

## Round 3 — `1.0.0-rc2`, wider coverage

Same zip as round 2; the trial application was extended to use the parts of the public API rounds 1 and 2 did not touch:

- **Inspector:** a property validator that needs application data (object names are unique in the scene), an alternative
  editor for a built-in type chosen per property (`IntBuilder::editor()` with an application `QSlider` editor), and a
  group hidden per object (lights have no camera settings).
- **Settings:** keyboard navigation (Tab, arrow keys, Space), a `mustExist` directory typed by hand, and "Reset group" from
  the view's context menu next to a read-only group.
- **Plugins:** bulk changes in one batch (`beginBatch()` / `endBatch()`, `batchValueChanged`) seen through the filter proxy.

`run_trial.sh` now passes extra CMake arguments through, so the trial was built three ways: static with GCC, shared
(`-DQPB_BUILD_SHARED=ON`) and with Clang (`-Wall -Wextra`). 15 of 15 trial tests pass in every build, with no
warnings from qpb.

| # | Finding | Kind | Outcome |
|---|---------|------|---------|
| F5 | Tab / Shift+Tab chain the editors and skip check boxes (Bool properties), so keyboard users reach them with the arrow keys and toggle them with Space. | Documented behaviour (SPEC 5.5) | Kept for 1.0. Adding check boxes to the Tab chain later changes no code in consuming projects. |
| F6 | Rejected values (duplicate name, missing directory) keep the old value, emit `validationFailed` with the validator's message, and the view shows it as a tool tip. | Confirmation | Works as specified; nothing to change. |

No API or behaviour change was needed: the exit criterion is met, and `1.0.0` is released with the content of
`1.0.0-rc2` (RC.3).

## Round 4 — `1.2.0` before release

The trial application moved to the API of 1.1 and 1.2 the way a consuming project would upgrade: the settings page stores
its values with `qpb::serialization` instead of its own loops, can switch to a `PropertyFormView`, and gained a multi-line
field, a 64-bit cache size and JSON export/import; the plugins page uses `PropertyFilterProxyModel` instead of a
configured `QSortFilterProxyModel`; a new **Devices** page (scenario 4) shows application `QObject`s through
`QObjectPropertySource` in a form. The unchanged round-3 application was also built against the 1.2.0 zip first:
15 of 15 tests pass, so upgrading needs no code change.

22 trial tests pass (static, GCC) after the fix below.

| # | Finding | Kind | Outcome |
|---|---------|------|---------|
| F7 | `serialization::toJson()` wrote `"About": {}` for a group whose properties are all read-only, while `save()` writes no key for it. | Behaviour | **Fixed before 1.2.0 was released:** groups with nothing to store are left out of the JSON too. |
| F8 | A `QObjectPropertySource` group is titled with the object name (`studio_mic`); showing the device's own name means calling `setDisplayName()` on the group and keeping it up to date by hand. | Ergonomics | Kept for 1.2. A display-name option can be added later without breaking anything. |
| F9 | Live read-only values coming from an object (`used`) are bold ("modified") as soon as they change, which reads as a user edit. | Ergonomics | Kept; the application can call `setDefaultValue()`. To reconsider with F8. |
| F10 | Dependencies between properties (autosave enables autosaveMinutes) are still wired by hand through `valueChanged`, in both views. | Ergonomics | Kept, as F3: no API change needed for 1.x. |

Otherwise the new API behaved as specified: the form view follows the dependency and the multi-line field, the filter
shows all plugin settings when the plugin name is searched, device edits reach the objects, values a device refuses
(recording while disabled) are replaced by the device's value, and a deleted device disappears from the form.
