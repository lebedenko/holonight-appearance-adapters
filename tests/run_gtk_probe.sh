#!/bin/sh
set -eu
dbus_run=$1
xvfb_run=$2
probe=$3
fixture=$(mktemp -d)
trap 'rm -rf "$fixture"' EXIT
mkdir -p "$fixture/config/gtk-3.0" "$fixture/config/gtk-4.0"
printf '[Settings]\ngtk-icon-theme-name=HoloNightFixture\ngtk-application-prefer-dark-theme=true\n' > "$fixture/config/gtk-3.0/settings.ini"
cp "$fixture/config/gtk-3.0/settings.ini" "$fixture/config/gtk-4.0/settings.ini"
HOME="$fixture" XDG_CONFIG_HOME="$fixture/config" GSETTINGS_BACKEND=keyfile \
  "$dbus_run" -- "$xvfb_run" -a "$probe"
