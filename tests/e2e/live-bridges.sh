#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
#
# The live path end to end on one machine: hr-bridge and grid-bridge replay a
# made-up session, four times faster than life, and vm-core reads both and
# must find strap and camera in agreement.
#
# usage: live-bridges.sh <bin dir> <strap log> <grid file>
set -eu
bin=$1
strap=$2
grid=$3
hr_port=56011
grid_port=56012

"$bin/hr-bridge" replay "$strap" --port "$hr_port" --speed 4 >/dev/null 2>&1 &
hr_pid=$!
"$bin/grid-bridge" replay "$grid" --port "$grid_port" --speed 4 >/dev/null 2>&1 &
grid_pid=$!
trap 'kill "$hr_pid" "$grid_pid" 2>/dev/null || true' EXIT
sleep 1

"$bin/vm-core" --hr-client "127.0.0.1:$hr_port" --grid "127.0.0.1:$grid_port" --quiet --require-corroborated 15
