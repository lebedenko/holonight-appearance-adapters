#!/bin/sh
set -eu

adapter=$1
root=$(mktemp -d)
trap 'rm -rf "$root"' EXIT
mkdir -p "$root/config/gtk-3.0" "$root/config/gtk-4.0" "$root/state"
appearance="$root/appearance.toml"
cat >"$appearance" <<'EOF'
version = 1
[theme]
scheme = "holonight-dark"
accent = "blue"
[typography]
ui_family = "Inter"
ui_size = 12
monospace_family = "JetBrains Mono"
monospace_size = 12
title_family = "Audiowide"
title_size = 10
display_family = "Rajdhani"
display_size = 24
[icons]
theme = "HoloNight"
fallback = "Papirus"
cursor = "HoloNight"
[layout]
scale = 1.0
[shape]
style = "inherit"
scale = 1.0
EOF
printf '%s\n' '[Settings]' 'unrelated-key=keep' >"$root/config/gtk-3.0/settings.ini"
env XDG_CONFIG_HOME="$root/config" XDG_STATE_HOME="$root/state" GSETTINGS_BACKEND=memory \
  "$adapter" apply --appearance "$appearance" --json >"$root/apply.json"
grep -q '"operation":"apply"' "$root/apply.json"
grep -q 'unrelated-key=keep' "$root/config/gtk-3.0/settings.ini"
grep -q 'gtk-icon-theme-name=HoloNight' "$root/config/gtk-3.0/settings.ini"
env XDG_CONFIG_HOME="$root/config" XDG_STATE_HOME="$root/state" GSETTINGS_BACKEND=memory \
  "$adapter" query --appearance "$appearance" --field cursor-theme | grep -qx HoloNight
env XDG_CONFIG_HOME="$root/config" XDG_STATE_HOME="$root/state" GSETTINGS_BACKEND=memory \
  "$adapter" status --json | grep -q '"operation":"status"'
env XDG_CONFIG_HOME="$root/config" XDG_STATE_HOME="$root/state" GSETTINGS_BACKEND=memory \
  "$adapter" revert --json >"$root/revert.json"
grep -q '"operation":"revert"' "$root/revert.json"
grep -q 'unrelated-key=keep' "$root/config/gtk-3.0/settings.ini"
if grep -q 'gtk-icon-theme-name=' "$root/config/gtk-3.0/settings.ini"; then exit 1; fi
