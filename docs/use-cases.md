# qpb — Use cases

Input for API design (docs/PLAN.md M0.1, M1.2) and the RC trial (RC.1).
List at least three real projects/panels that will use qpb, and the full property list of one panel.

> **Status: to be filled in by the maintainer.** The entries below are placeholders.

## Projects

| # | Project | Panel(s) that would use qpb | Data source today (QObject / struct / JSON / settings) | Preferred view (tree / list / form) |
|---|---------|-----------------------------|--------------------------------------------------------|-------------------------------------|
| 1 | TODO    | TODO                        | TODO                                                   | TODO                                |
| 2 | TODO    | TODO                        | TODO                                                   | TODO                                |
| 3 | TODO    | TODO                        | TODO                                                   | TODO                                |

## Reference panel (used for M1.2 and RC.1)

Project: TODO — Panel: TODO

| Path (group/id) | Type (bool/int/double/string/enum/file/dir/custom) | Default | Constraints (range, regex, filter, options...) | Notes |
|-----------------|-----------------------------------------------------|---------|-------------------------------------------------|-------|
| TODO            |                                                     |         |                                                 |       |

## Open questions answered by these use cases

- Meaning of "custom property table" (SPEC Appendix B, PLAN §7).
- Primary data source: explicit builder vs. `QObject` (decides whether `QObjectPropertySource` moves into 1.0).
- Custom types needed beyond the 7 basic ones.
