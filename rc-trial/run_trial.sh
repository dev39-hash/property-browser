#!/usr/bin/env bash
# Builds and tests the RC trial against a released qpb zip (docs/PLAN.md RC.1).
#
#   rc-trial/run_trial.sh path/to/qpb-1.0.0-rc1.zip [Qt prefix]
#
# Screenshots of the three pages go to rc-trial/build/screenshots/.

set -euo pipefail
zip="$(realpath "$1")"
qt_prefix="${2:-}"
here="$(cd "$(dirname "$0")" && pwd)"

rm -rf "$here/components"
mkdir -p "$here/components"
unzip -q "$zip" -d "$here/components"

cmake -S "$here" -B "$here/build" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    ${qt_prefix:+-DCMAKE_PREFIX_PATH="$qt_prefix"}
cmake --build "$here/build"
mkdir -p "$here/build/screenshots"
QPB_TRIAL_SCREENSHOTS="$here/build/screenshots" ctest --test-dir "$here/build" --output-on-failure
