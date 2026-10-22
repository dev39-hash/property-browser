#!/usr/bin/env bash
# Prepares a qpb release locally (docs/RELEASING.md). Creates nothing outside
# the repository and dist/: tagging and handing the release over are printed.
#
#   tools/make_release.sh 1.0.0-rc1
#
# Produces:
#   dist/qpb-<version>.zip   the qpb/ component folder only, as consumers copy it
#   branch qpb-release       history of qpb/ alone, for git subtree / submodule
#                            consumers (tagged qpb-v<version> by hand)

set -euo pipefail

version="${1:-}"
if [[ ! "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z.]+)?$ ]]; then
    echo "usage: $0 MAJOR.MINOR.PATCH[-suffix]" >&2
    exit 2
fi

root="$(git rev-parse --show-toplevel)"
cd "$root"

if [[ -n "$(git status --porcelain)" ]]; then
    echo "error: the working tree has uncommitted changes" >&2
    exit 1
fi
if [[ "$(tr -d '[:space:]' < qpb/VERSION)" != "$version" ]]; then
    echo "error: qpb/VERSION is '$(tr -d '[:space:]' < qpb/VERSION)', expected '$version'" >&2
    echo "       update qpb/VERSION and qpb/CHANGELOG.md, commit, then run this again" >&2
    exit 1
fi
if ! grep -q "^## ${version//./\\.}" qpb/CHANGELOG.md; then
    echo "error: qpb/CHANGELOG.md has no '## $version' section" >&2
    exit 1
fi

mkdir -p dist
archive="dist/qpb-$version.zip"
git archive --format=zip --prefix=qpb/ -o "$archive" HEAD:qpb
echo "created $archive"

git subtree split --prefix=qpb --branch=qpb-release >/dev/null 2>&1
if [[ "$(git rev-parse HEAD:qpb)" != "$(git rev-parse 'qpb-release^{tree}')" ]]; then
    echo "error: qpb-release does not end with the qpb/ folder of HEAD" >&2
    exit 1
fi
echo "updated local branch qpb-release ($(git rev-parse --short qpb-release))"

cat <<MSG

Release artifacts are ready. If the verification of docs/RELEASING.md step 2
(ctest with Qt 6 and Qt 5.15, 25 Release runs of the widget suites) passed on
this commit, tag:

  git tag -a v$version -m "qpb $version"
  git tag -a qpb-v$version qpb-release -m "qpb $version component folder"

Hand main, qpb-release, v$version and qpb-v$version to the other machine in the
next git bundle, and give $archive to consumers.
MSG
