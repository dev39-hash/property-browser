# qpb — Reference use cases

No real consuming project is available yet, so API design (PLAN M1.2) and the pre-1.0 trial (PLAN RC.1)
use the three reference scenarios below. Each one becomes an example application; together they cover every
basic type, groups, custom types and all three views.

"Custom property table" in the original brainstorm means: **users build their own property tables with the
public API of the library** (declare properties with the builder, register custom types and editors) — goals G1 and G3
in the spec. It does not mean extra columns or a separate table widget.

## 1. Scene object inspector (tree view)

A 3D editor shows the properties of the selected object. Nested groups, frequent edits, custom types.

| Path                 | Type      | Default          | Constraints                          |
|----------------------|-----------|------------------|--------------------------------------|
| `name`               | string    | `"Main camera"`  | `maxLength` 64, regex `^[A-Za-z_][A-Za-z0-9_ ]*$` |
| `visible`            | bool      | true             |                                      |
| `Transform/x`        | double    | 0.0              | −100 … 100, step 0.5, suffix `" m"`  |
| `Transform/y`        | double    | 0.0              | −100 … 100, step 0.5, suffix `" m"`  |
| `Transform/z`        | double    | 0.0              | −100 … 100, step 0.5, suffix `" m"`  |
| `Camera/fov`         | int       | 60               | 10 … 170, suffix `"°"`               |
| `Camera/projection`  | enum      | Perspective      | Perspective, Orthographic            |
| `Camera/lut`         | file path | empty            | filter `"LUT files (*.cube)"`, must exist |
| `Render/tint`        | custom `app.color` | white   | registered outside the library (S2)  |

## 2. Application settings dialog (form view, 1.1; tree/list view in 1.0)

A settings dialog backed by `QSettings`. Flat groups, read-only entries, paths.

| Path                     | Type      | Default     | Constraints                        |
|--------------------------|-----------|-------------|------------------------------------|
| `General/language`       | enum      | `"en"`      | options with string values `en`, `vi` |
| `General/autosave`       | bool      | true        |                                    |
| `General/autosaveMinutes`| int       | 5           | 1 … 60; disabled when `autosave` is off (application callback) |
| `Paths/projectDir`       | directory | home dir    | must exist                         |
| `Paths/cacheDir`         | directory | empty       |                                    |
| `About/version`          | string    | app version | read-only                          |

## 3. Plugin configuration built at runtime (list view)

Properties are defined from data (e.g. a plugin manifest) while the application runs; properties are added and
removed on a live model.

| Path                 | Type    | Default | Constraints                                 |
|----------------------|---------|---------|---------------------------------------------|
| `<plugin>/enabled`   | bool    | false   | one group per loaded plugin                 |
| `<plugin>/threshold` | double  | 0.5     | 0 … 1, 2 decimals                           |
| `<plugin>/mode`      | enum    | 0       | options read from the manifest              |
| `<plugin>/output`    | file    | empty   | mode `save`                                 |

## Criteria derived from these scenarios

- Scenario 1 is `examples/inspector` and the S1 quickstart panel (≤ 30 lines).
- Scenario 2 exercises read-only state, enabled/disabled dependencies driven by the application, and string-valued enums.
- Scenario 3 exercises runtime `add`/`remove` on a live model (`TreeObserver`, PLAN M2.7).
- RC.1 builds scenarios 1–3 as a standalone application outside this repo's build, embedding `qpb/` as a copied component.
