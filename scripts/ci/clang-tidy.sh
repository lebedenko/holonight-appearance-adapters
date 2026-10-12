#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Preserve diagnostics in the owned labwc implementation fragment as well as headers.
exec clang-tidy '--header-filter=/(include|src|tests|probes)/.*\.(h|hpp|inc)$' "$@"
