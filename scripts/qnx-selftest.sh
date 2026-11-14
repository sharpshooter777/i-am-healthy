#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
#
# Copies a QNX build to a target over SSH and runs the unit tests,
# vm-core --selftest and a synthetic-recording heart-rate check there.
#
# usage: scripts/qnx-selftest.sh [preset] [ssh-target]
#   preset      qnx-x86_64 (default) or qnx-aarch64le
#   ssh-target  user@host or an ~/.ssh/config alias; defaults to
#               $VITALS_QNX_TARGET, then "qnx-qemu"
#
# Needs key-based SSH login to the target.
set -eu

preset=${1:-qnx-x86_64}
target=${2:-${VITALS_QNX_TARGET:-qnx-qemu}}
root=$(cd "$(dirname "$0")/.." && pwd)
bin="$root/build/$preset/bin"
remote_dir=vitalmon/$preset

for exe in vm-core vm-offline core_tests platform_tests sources_tests; do
    if [ ! -x "$bin/$exe" ]; then
        echo "missing $bin/$exe; build first: cmake --build --preset $preset" >&2
        exit 1
    fi
done

ssh "$target" "mkdir -p $remote_dir"
scp -q "$bin/vm-core" "$bin/vm-offline" "$bin/core_tests" "$bin/platform_tests" "$bin/sources_tests" \
    "$target:$remote_dir/"

ssh "$target" "cd $remote_dir && ./core_tests && ./platform_tests && ./sources_tests && ./vm-core --selftest \
    && ./vm-offline synth offline-test.vmrec --seconds 40 --hr 75 --slope 3 \
    && ./vm-offline analyse offline-test.vmrec --max-error 3 --limit 100 --quiet \
    && rm -f offline-test.vmrec"
