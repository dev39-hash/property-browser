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
   - regenerate the API snapshot: `python3 tools/api_snapshot.py --include qpb/include --write tests/api_compat/api-1.0.txt`
     (the check only ever allows additions, so the regenerated file must be a superset of the old one).
4. Commit through a pull request and wait for green CI on all platforms.

## 2. Build the artifacts

```sh
tools/make_release.sh <version>
```

The script refuses to run on a dirty tree, a `qpb/VERSION` that does not match, or a missing CHANGELOG section. It creates

- `dist/qpb-<version>.zip`: the `qpb/` folder only, exactly what consumers copy to `components/qpb/`;
- the local branch `qpb-release`: the history of `qpb/` alone (via `git subtree split`), for consumers using
  `git subtree pull --prefix components/qpb <remote> qpb-release --squash`.

## 3. Publish (maintainer)

The script prints the commands:

```sh
git tag -a v<version> -m "qpb <version>"
git push origin v<version>
git push origin qpb-release
```

Then create a GitHub Release for the tag and attach `dist/qpb-<version>.zip`.

## Rules

- Within 1.x, never publish a release whose `tests/api_compat/*` or `api_snapshot` test fails (docs/SPEC.md section 9).
- `1.0.0` is tagged only after a release candidate passed the RC trial (docs/PLAN.md, RC) without API changes.
