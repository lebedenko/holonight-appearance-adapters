#!/bin/sh
set -eu
gtk3_probe=$1
gtk4_probe=$2
gtk3_links=$(ldd "$gtk3_probe")
gtk4_links=$(ldd "$gtk4_probe")
printf '%s\n' "$gtk3_links" | grep -q 'libgtk-3'
if printf '%s\n' "$gtk3_links" | grep -q 'libgtk-4'; then
  echo "GTK 3 probe unexpectedly links GTK 4" >&2
  exit 1
fi
printf '%s\n' "$gtk4_links" | grep -q 'libgtk-4'
if printf '%s\n' "$gtk4_links" | grep -q 'libgtk-3'; then
  echo "GTK 4 probe unexpectedly links GTK 3" >&2
  exit 1
fi
