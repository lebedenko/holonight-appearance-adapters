#!/bin/sh
set -eu
lane=$1
mkdir /work/source
cp -a /input/. /work/source/
cd /work/source
export HOME=/work/build/home LC_ALL=C.UTF-8 TZ=UTC
mkdir -p "$HOME"
if [ "$lane" = licensing ]; then
  reuse --version
  reuse lint
  exit
fi
[ "$lane" = verification ] || exit 2
python3 --version
git --version
cmake --version
ninja --version
c++ --version
clang-format --version
clang-tidy --version
pkg-config --modversion Qt6Core Qt6Gui Qt6Xml
python3 scripts/ci/test_launcher.py
export PATH="/work/source/scripts/ci:/work/tools/usr/bin:$PATH"
export LD_LIBRARY_PATH=/work/tools/usr/lib
export PKG_CONFIG_PATH=/work/tools/usr/lib/pkgconfig:/work/tools/usr/share/pkgconfig
export GI_TYPELIB_PATH=/work/tools/usr/lib/girepository-1.0
export XDG_DATA_DIRS=/work/tools/usr/share:/usr/share
export XDG_RUNTIME_DIR=/work/runtime GDK_BACKEND=x11 GSK_RENDERER=cairo GTK_A11Y=none
export QT_QPA_PLATFORM=offscreen QT_FORCE_STDERR_LOGGING=1
mkdir -m 700 "$XDG_RUNTIME_DIR"
export GDK_PIXBUF_MODULE_FILE="$XDG_RUNTIME_DIR/loaders.cache"
/work/tools/usr/bin/gdk-pixbuf-query-loaders /work/tools/usr/lib/gdk-pixbuf-2.0/2.10.0/loaders/*.so > "$GDK_PIXBUF_MODULE_FILE"
pkg-config --modversion json-glib-1.0 gio-2.0 gtk+-3.0 gtk4
pkg-config --cflags --libs json-glib-1.0 gio-2.0 gtk+-3.0 gtk4
GSETTINGS_BACKEND=memory gsettings get org.gnome.desktop.interface color-scheme
dbus-run-session --version
xvfb-run -a /bin/true
mkdir -p /work/providers
fetch_provider() {
  name=$1
  revision=$2
  git init -q "/work/providers/$name"
  git -C "/work/providers/$name" fetch --depth 1 "https://github.com/lebedenko/$name.git" "$revision"
  git -C "/work/providers/$name" checkout --detach FETCH_HEAD
  [ "$(git -C "/work/providers/$name" rev-parse HEAD)" = "$revision" ]
}
fetch_provider holonight-config 5cd36ec9986801c4b831527a38e7b7414b9a1312
fetch_provider holonight-qt fb0acc6520802d8f9c014bc9042b71b79d8c6d9a
prefix=/work/providers/prefix
cmake -S /work/providers/holonight-config -B /work/providers/config-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build /work/providers/config-build --parallel 2
cmake --install /work/providers/config-build --prefix "$prefix"
cmake -S /work/providers/holonight-qt -B /work/providers/qt-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF -DCMAKE_PREFIX_PATH="$prefix"
cmake --build /work/providers/qt-build --parallel 2
cmake --install /work/providers/qt-build --prefix "$prefix"
export LD_LIBRARY_PATH="$prefix/lib:/work/tools/usr/lib"
cmake -S . -B build/verification -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DBUILD_TESTING=ON -DBUILD_GTK_PROBES=ON \
  -DCMAKE_PREFIX_PATH="$prefix;/work/tools/usr" -DHOLONIGHT_QT_DIR="$prefix" -DHOLONIGHT_CONFIG_DIR="$prefix"
cmake --build build/verification --parallel 2
build/verification/palette_tests
ctest --test-dir build/verification --output-on-failure --no-tests=error
cmake --build build/verification --target format-check
cmake --build build/verification --target tidy
cp -a build/verification/Testing /output/
