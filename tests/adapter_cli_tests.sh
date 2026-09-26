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
theme = "HoloNight-Dark"
fallback = "Papirus"
cursor = "HoloNight"
[layout]
scale = 1.0
[shape]
style = "inherit"
scale = 1.0
EOF
printf '%s\n' '[Settings]' 'unrelated-key=keep' >"$root/config/gtk-3.0/settings.ini"
printf '%s\n' '[General]' 'unrelated=keep' >"$root/config/kdeglobals"
env XDG_CONFIG_HOME="$root/config" XDG_STATE_HOME="$root/state" GSETTINGS_BACKEND=keyfile \
  "$adapter" apply --appearance "$appearance" --json >"$root/apply.json"
grep -q '"operation":"apply"' "$root/apply.json"
grep -q 'unrelated-key=keep' "$root/config/gtk-3.0/settings.ini"
grep -q 'gtk-icon-theme-name=HoloNight-Dark' "$root/config/gtk-3.0/settings.ini"
grep -q 'gtk-icon-theme-name=HoloNight-Dark' "$root/config/gtk-4.0/settings.ini"
grep -q 'ColorScheme=holonight-dark' "$root/config/kdeglobals"
grep -q 'Theme=HoloNight-Dark' "$root/config/kdeglobals"
grep -q 'unrelated=keep' "$root/config/kdeglobals"
grep -q 'font=Inter,12,' "$root/config/kdeglobals"
grep -q 'fixed=JetBrains Mono,12,' "$root/config/kdeglobals"
grep -q 'gtk-application-prefer-dark-theme=true' "$root/config/gtk-4.0/settings.ini"
env XDG_CONFIG_HOME="$root/config" GSETTINGS_BACKEND=keyfile \
  gsettings get org.gnome.desktop.interface color-scheme | grep -qx "'prefer-dark'"
env XDG_CONFIG_HOME="$root/config" GSETTINGS_BACKEND=keyfile \
  gsettings get org.gnome.desktop.interface icon-theme | grep -qx "'HoloNight-Dark'"
env XDG_CONFIG_HOME="$root/config" XDG_STATE_HOME="$root/state" GSETTINGS_BACKEND=keyfile \
  "$adapter" apply --appearance "$appearance" --json >"$root/repeated.json"
grep -q '"name":"kde/General/ColorScheme","status":"unchanged"' "$root/repeated.json"
grep -q '"name":"kde/General/font","status":"unchanged"' "$root/repeated.json"
sed -i 's/holonight-dark/holonight-light/' "$appearance"
sed -i 's/theme = "HoloNight-Dark"/theme = "HoloNight"/' "$appearance"
env XDG_CONFIG_HOME="$root/config" XDG_STATE_HOME="$root/state" GSETTINGS_BACKEND=keyfile \
  "$adapter" status --appearance "$appearance" --json >"$root/stale.json"
grep -q 'canonical appearance changed since last apply' "$root/stale.json"
env XDG_CONFIG_HOME="$root/config" XDG_STATE_HOME="$root/state" GSETTINGS_BACKEND=keyfile \
  "$adapter" apply --appearance "$appearance" --json >"$root/light.json"
grep -q 'ColorScheme=holonight-light' "$root/config/kdeglobals"
grep -q 'gtk-application-prefer-dark-theme=false' "$root/config/gtk-3.0/settings.ini"
grep -q 'gtk-application-prefer-dark-theme=false' "$root/config/gtk-4.0/settings.ini"
env XDG_CONFIG_HOME="$root/config" GSETTINGS_BACKEND=keyfile \
  gsettings get org.gnome.desktop.interface color-scheme | grep -qx "'prefer-light'"
env XDG_CONFIG_HOME="$root/config" XDG_STATE_HOME="$root/state" GSETTINGS_BACKEND=keyfile \
  "$adapter" query --appearance "$appearance" --field cursor-theme | grep -qx HoloNight
env XDG_CONFIG_HOME="$root/config" XDG_STATE_HOME="$root/state" GSETTINGS_BACKEND=keyfile \
  "$adapter" status --json | grep -q '"operation":"status"'
env XDG_CONFIG_HOME="$root/config" XDG_STATE_HOME="$root/state" GSETTINGS_BACKEND=keyfile \
  "$adapter" revert --json >"$root/revert.json"
grep -q '"operation":"revert"' "$root/revert.json"
grep -q 'unrelated-key=keep' "$root/config/gtk-3.0/settings.ini"
grep -q 'unrelated=keep' "$root/config/kdeglobals"
if grep -q 'ColorScheme=' "$root/config/kdeglobals"; then exit 1; fi
if grep -q 'gtk-icon-theme-name=' "$root/config/gtk-3.0/settings.ini"; then exit 1; fi

# An unwritable state destination must restore the outputs changed in this apply.
printf '%s\n' blocked >"$root/block"
if env XDG_CONFIG_HOME="$root/rollback" XDG_STATE_HOME="$root/block" GSETTINGS_BACKEND=keyfile \
  "$adapter" apply --appearance "$appearance" --json >"$root/failed.json"; then exit 1; fi
grep -q '"name":"state","status":"error"' "$root/failed.json"
if grep -q 'ColorScheme=' "$root/rollback/kdeglobals"; then exit 1; fi
if grep -q 'gtk-icon-theme-name=' "$root/rollback/gtk-3.0/settings.ini"; then exit 1; fi

env XDG_CONFIG_HOME="$root/config" XDG_STATE_HOME="$root/state" GSETTINGS_BACKEND=keyfile \
  "$adapter" apply --appearance "$appearance" --json >"$root/again.json"
sed -i 's/ColorScheme=holonight-light/ColorScheme=External/' "$root/config/kdeglobals"
sed -i 's/gtk-icon-theme-name=HoloNight/gtk-icon-theme-name=External/' "$root/config/gtk-4.0/settings.ini"
env XDG_CONFIG_HOME="$root/config" XDG_STATE_HOME="$root/state" GSETTINGS_BACKEND=keyfile \
  "$adapter" revert --json >"$root/conflict.json"
grep -q 'ColorScheme=External' "$root/config/kdeglobals"
grep -q 'gtk-icon-theme-name=External' "$root/config/gtk-4.0/settings.ini"
grep -q '"name":"kde/General/ColorScheme","status":"conflict"' "$root/conflict.json"
