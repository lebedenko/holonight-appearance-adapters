#!/bin/sh
set -eu
# Xvfb invokes its keyboard compiler at this fixed path.
cd /input
python3 scripts/ci/prepare-tools.py
ln -s /work/tools/usr/bin/xkbcomp /usr/bin/xkbcomp
exec setpriv --reuid="$CI_UID" --regid="$CI_GID" --clear-groups \
  /bin/sh /input/scripts/ci/lane.sh verification
