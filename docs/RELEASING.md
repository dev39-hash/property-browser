# Releasing qpb

The release process of docs/PLAN.md section 4, step by step. Only the maintainer publishes; the tooling prepares
everything locally.

## 1. Prepare the release commit

1. Update `qpb/VERSION`, e.g. `1.0.0-rc1` or `1.0.0` (`MAJOR.MINOR.PATCH` with an optional `-suffix`).
2. Rename `## Unreleased` in `qpb/CHANGELOG.md` to `## <version>`; fill in *Added / Changed / Deprecated / Fixed*
   and **Upgrade notes** (for 1.x usually "nothing to do").
3. For a new minor release (1.1, 1.2, ...):
   - add `tests/api_compat/v<major>_<minor>.cpp` exercising the new API, and list it in
     `tests/api_compat/CMakeLists.txt`;
   - add the API snapshot of the new version:
     `python3 tools/api_snapshot.py --include qpb/include --write tests/api_compat/api-<major>.<minor>.txt` and list it in
     `tests/api_compat/CMakeLists.txt`. Earlier snapshots are never edited; each is still checked, and the new one must be
     a superset of the previous one;
   - since 1.7, the same for the Qt 5 configuration (docs/SPEC.md section 9.6): `--qt-major 5 --write
     tests/api_compat/api-qt5-<major>.<minor>.txt`, listed with the Qt 5 snapshots, and a `qt5_v<major>_<minor>.cpp`
     when the Qt 5 API gains something of its own.
4. Commit through a pull request and wait for green CI on all platforms, with Qt 6 and with Qt 5.15 (MSVC 2019 / 2022,
   GCC or Clang; Qt 5.15 headers do not compile with MSVC 2026).

## 2. Build the artifacts

```sh
tools/make_release.sh <version>
```

The script refuses to run on a dirty tree, a `qpb/VERSION` that does not match, or a missing CHANGELOG section. It creates

- `dist/qpb-<version>.zip`: the `qpb/` folder only, exactly what consumers copy to `components/qpb/`;
- the local branch `qpb-release`: the history of `qpb/` alone (via `git subtree split`), whose tip is now this release.

These are the three ways consumers get the folder (docs/SPEC.md section 6.2, D47): the zip, `git subtree` and
`git submodule`. The last two need the tag `qpb-v<version>` on `qpb-release`, created below; `v<version>` tags the whole
repository and cannot be used by them.

## 3. Publish (maintainer)

The script prints the commands:

```sh
git tag -a v<version> -m "qpb <version>"
git tag -a qpb-v<version> qpb-release -m "qpb <version> component folder"
git push origin v<version> qpb-v<version>
git push origin qpb-release
```

Then create a GitHub Release for the tag `v<version>` and attach `dist/qpb-<version>.zip`.

## Rules

- Within 1.x, never publish a release whose `tests/api_compat/*` or `api_snapshot` test fails (docs/SPEC.md section 9).
- `1.0.0` is tagged only after a release candidate passed the RC trial (docs/PLAN.md, RC) without API changes.
- Every release has both tags, and `qpb-v<version>` contains exactly the `qpb/` folder of `v<version>`
  (`git rev-parse v<version>:qpb` equals `git rev-parse qpb-v<version>^{tree}`). Tags are never moved once pushed.
