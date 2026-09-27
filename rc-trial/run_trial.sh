#!/usr/bin/env bash
# Builds and tests the RC trial against a released qpb zip (docs/PLAN.md RC.1).
#
#   rc-trial/run_trial.sh path/to/qpb-1.0.0-rc1.zip [Qt prefix [extra CMake arguments...]]
#
# Examples of extra arguments: -DQPB_BUILD_SHARED=ON, -DCMAKE_CXX_COMPILER=clang++.
# Set TRIAL_BUILD_DIR to build somewhere other than rc-trial/build.
#
# Screenshots of the three pages go to <build>/screenshots/.

set -euo pipefail
zip="$(realpath "$1")"
qt_prefix="${2:-}"
shift $(( $# < 2 ? $# : 2 ))
here="$(cd "$(dirname "$0")" && pwd)"
build="${TRIAL_BUILD_DIR:-$here/build}"

rm -rf "$here/components"
mkdir -p "$here/components"
unzip -q "$zip" -d "$here/components"

cmake -S "$here" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    ${qt_prefix:+-DCMAKE_PREFIX_PATH="$qt_prefix"} "$@"
cmake --build "$build"
mkdir -p "$build/screenshots"
QPB_TRIAL_SCREENSHOTS="$build/screenshots" ctest --test-dir "$build" --output-on-failure
