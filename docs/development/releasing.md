# Releasing qpb

How a release is made, step by step. The repository is local: there is no hosted remote and no automated build service,
and releases and work travel between two machines as git bundles (step 5). Only the maintainer releases. The tooling
prepares everything locally, and the person who releases verifies the result with the gate in step 2.

## 1. Prepare the release commit

1. Update `qpb/VERSION`, e.g. `1.0.0-rc1` or `1.0.0` (`MAJOR.MINOR.PATCH` with an optional `-suffix`).
2. Rename `## Unreleased` in `qpb/CHANGELOG.md` to `## <version>`; fill in *Added / Changed / Deprecated / Fixed*
   and **Upgrade notes** (for 1.x usually "nothing to do"). Update the "Version history" list in `qpb/README.md`.
3. For a new minor release (1.1, 1.2, ...):
   - when it adds API, add `tests/api_compat/v<major>_<minor>.cpp` exercising the new API, and list it in
     `tests/api_compat/CMakeLists.txt`;
   - add the API snapshot of the new version:
     `python3 tools/api_snapshot.py --include qpb/include --write tests/api_compat/api-<major>.<minor>.txt` and list it in
     `tests/api_compat/CMakeLists.txt`. Earlier snapshots are never edited; each is still checked, and the new one must be
     a superset of the previous one (see [public-headers.md](../design/public-headers.md));
   - since 1.7, the same for the Qt 5 configuration: `--qt-major 5 --write
     tests/api_compat/api-qt5-<major>.<minor>.txt`, listed with the Qt 5 snapshots, and a `qt5_v<major>_<minor>.cpp`
     when the Qt 5 API gains something of its own.
4. Since 1.8, when translatable texts changed: `python3 tools/update_translations.py --qt-bin <Qt prefix>/bin`, translate
   the unfinished texts in `qpb/translations/*.ts`, run it again and commit the `.ts` and `.qm` files (see
   [building-and-testing.md](building-and-testing.md)). Existing contexts and source texts are never changed within 1.x.
   The test `translations_up_to_date` (when `lupdate` is found) and `tst_translations` check this.
5. Since 1.9, bring the translated user documentation in `qpb/doc/vi/` up to date with the English documentation of
   the release: every file of `qpb/doc/` and `qpb/README.md` (as `qpb/doc/vi/README.md`), the same sections, code and
   links, and the version in the note at the top of each file. Between releases only the English documentation
   changes; the English text is the reference.
6. Commit on `main`. The working tree must be clean afterwards.

## 2. Verify

Nothing is built for you, so the release is verified by hand on the release commit, with a Release build unless noted:

- the full `ctest` run with **Qt 6** and with **Qt 5.15**, every test passing. Which compilers are used with each Qt, and
  which platforms are covered, is in [qpb/doc/integration.md](../../qpb/doc/integration.md#platforms) and its
  [Qt 5](../../qpb/doc/integration.md#qt-5) section;
- **25 runs** of each widget suite (`tst_editorfactory`, `tst_propertytreeview`, `tst_propertyformview`,
  `tst_widgettranslations`) on a Release build, for example `ctest -R <suite> --repeat until-fail:25` (see
  [building-and-testing.md](building-and-testing.md)). A crash in any run blocks the release, even when every Debug run
  passes. Reason: a use-after-free in code that removes properties passed every Debug run and several Release runs, and
  crashed roughly a third to a half of Release runs of a widget suite.

If anything fails, fix it in a new commit and verify again from the start.

## 3. Build the artifacts

```sh
tools/make_release.sh <version>
```

The script refuses to run on a dirty tree, a `qpb/VERSION` that does not match, or a missing CHANGELOG section. It creates

- `dist/qpb-<version>.zip`: the `qpb/` folder only, exactly what consumers copy to `components/qpb/`;
- the local branch `qpb-release`: the history of `qpb/` alone (via `git subtree split`), whose tip is now this release.

The zip is the default way to get qpb (see [`qpb/doc/integration.md`](../../qpb/doc/integration.md)). `git subtree` and
`git submodule` work from a clone of this repository and need the tag `qpb-v<version>` on `qpb-release`, created below;
`v<version>` tags the whole repository and cannot be used by them.

For a release candidate, run the trial application against the zip before going on (see
[building-and-testing.md](building-and-testing.md)): `rc-trial/run_trial.sh dist/qpb-<version>.zip <Qt prefix>`.

## 4. Tag

The script prints the commands:

```sh
git tag -a v<version> -m "qpb <version>"
git tag -a qpb-v<version> qpb-release -m "qpb <version> component folder"
```

`v<version>` is on `main`, `qpb-v<version>` on `qpb-release`. Both are annotated.

## 5. Hand the release over

- `main`, `qpb-release` and both tags travel to the other machine in the next git bundle. To keep it small, make a
  range per branch against the last points the other machine confirmed:
  `git bundle create <file>.bundle main qpb-release v<version> qpb-v<version> --not <last confirmed main> <last confirmed qpb-release>`.
  The other machine reads it with
  `git fetch <file>.bundle "refs/heads/*:refs/remotes/bundle/*" "refs/tags/*:refs/tags/*"` and fast-forwards its
  branches to `bundle/main` and `bundle/qpb-release`.
- `dist/qpb-<version>.zip` is given to consumers directly. It is not tracked: `dist/` is ignored, so it does not travel
  in the bundle.

## Rules

- Within 1.x, never release a version whose `tests/api_compat/*` or `api_snapshot` test fails.
- The first release of a major version, such as `1.0.0`, is tagged only after a release candidate passed the trial
  application without API changes.
- Every release has both tags, and `qpb-v<version>` contains exactly the `qpb/` folder of `v<version>`
  (`git rev-parse v<version>:qpb` equals `git rev-parse qpb-v<version>^{tree}`). Tags are never moved once released.
