#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>
"""Exercise CMake prefix/DESTDIR installation and manifest-based staged uninstall."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

build = Path(sys.argv[1])
with tempfile.TemporaryDirectory() as temporary:
    root = Path(temporary)
    for index, prefix in enumerate(['/usr', str(root / 'user-prefix')]):
        dest = root / f'stage-{index}'
        env = dict(os.environ, DESTDIR=str(dest), HOME=str(root / 'home'), XDG_CONFIG_HOME=str(root / 'config'), XDG_DATA_HOME=str(root / 'data'), XDG_STATE_HOME=str(root / 'state'))
        subprocess.run(['cmake', '--install', str(build), '--prefix', prefix], env=env, check=True, capture_output=True)
        theme = dest / prefix.lstrip('/') / 'share/themes/HoloNight/labwc'
        assert (theme / 'themerc').is_file()
        assert 'window.active.title.bg.color: #0c1118ff' in (theme / 'themerc').read_text()
        assert len(list(theme.glob('*.svg'))) == 36
        for file in theme.glob('*.svg'):
            ET.parse(file)
        ET.parse(dest / prefix.lstrip('/') / 'share/doc/HoloNightAppearanceAdapters/labwc-theme.xml')
        assert not (root / 'config').exists() and not (root / 'data').exists() and not (root / 'state').exists()
        manifest = [Path(line) for line in (build / 'install_manifest.txt').read_text().splitlines()]
        assert len([x for x in manifest if '/themes/HoloNight/labwc/' in str(x)]) == 37
        for file in manifest:
            staged = dest / str(file).lstrip('/')
            assert staged.is_file()
            staged.unlink()
        assert not any(x.is_file() for x in dest.rglob('*'))
print('labwc staged install/uninstall checks passed')
