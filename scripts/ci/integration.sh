#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
task build:release
DESTDIR="$PWD/build/stage" cmake --install build/release
cmake -S . -B build/minimal -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DBUILD_KCONFIG=OFF -DBUILD_GTK_PROBES=OFF
cmake --build build/minimal
ctest --test-dir build/minimal --output-on-failure --no-tests=error
