#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
lane=$1
mkdir /work/holonight-appearance-adapters
cp -a /input/. /work/holonight-appearance-adapters/
cd /work/holonight-appearance-adapters
export HOME=/work/build/home LC_ALL=C.UTF-8 TZ=UTC
export XDG_RUNTIME_DIR=/work/runtime QT_FORCE_STDERR_LOGGING=1 QT_QPA_PLATFORM=offscreen
export GDK_BACKEND=x11 GSK_RENDERER=cairo GTK_A11Y=none
export QT_QPA_PLATFORMTHEME= QT_STYLE_OVERRIDE= JOBS=${JOBS:-2}
mkdir -p "$HOME"
mkdir -m 700 "$XDG_RUNTIME_DIR"
if [ "$lane" = licensing ]; then
  reuse --version
  reuse lint
  exit
fi
python3 --version
cmake --version
ninja --version
c++ --version
pkg-config --modversion Qt6Core Qt6Gui Qt6Xml json-glib-1.0 gio-2.0
if [ "$lane" = prepare ]; then
  mkdir -p /prepared/consumer
  cp -a /work/holonight-appearance-adapters/. /prepared/consumer/
  cd /prepared/consumer
  export HOLONIGHT_CONFIG_SOURCE=/prepared/config
  export HOLONIGHT_SYSTEM_SERVICES_SOURCE=/prepared/system-services
  export HOLONIGHT_QT_SOURCE=/prepared/qt
  task deps PRESET=test
  cp build/deps/provider-state.json /prepared/provider-state.json
  pacman --config /dev/null -Q > /prepared/packages.txt
  exit
fi
export HOLONIGHT_DEPENDENCY_PREFIX=/prepared/consumer/build/deps/prefix
export LD_LIBRARY_PATH="$HOLONIGHT_DEPENDENCY_PREFIX/lib"
python3 scripts/ci/validate-prepared.py
python3 scripts/ci/test_launcher.py
python3 scripts/ci/test_dependencies.py
case "$lane" in
  full|static)
    export HOLONIGHT_BUILD_FEATURES='BUILD_KCONFIG=ON BUILD_GTK_PROBES=ON'
    task build PRESET=test
    if [ "$lane" = full ]; then
      ctest --test-dir build/test --output-on-failure --no-tests=error
      cat build/test/Testing/Temporary/LastTest.log
    else
      git init -q
      git add .
      task format-check
      clang-format --dry-run --Werror src/labwc_adapter.inc
      CLANG_TIDY="$PWD/scripts/ci/clang-tidy.sh" task tidy
    fi
    ;;
  minimal)
    export HOLONIGHT_BUILD_FEATURES='BUILD_KCONFIG=OFF BUILD_GTK_PROBES=OFF'
    task build PRESET=minimal
    ctest --test-dir build/minimal --output-on-failure --no-tests=error
    cat build/minimal/Testing/Temporary/LastTest.log
    ;;
  release)
    task build PRESET=release
    cmake --install build/release --prefix /work/install
    ;;
  *) echo "Unknown lane: $lane" >&2; exit 2 ;;
esac
