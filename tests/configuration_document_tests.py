#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>
"""Sparse appearance compatibility and deterministic external-edit isolation."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

adapter, interposer = sys.argv[1:]
with tempfile.TemporaryDirectory() as temporary:
    root = Path(temporary)
    appearance = root / 'appearance.toml'
    env = dict(os.environ, HOME=str(root), XDG_CONFIG_HOME=str(root / 'config'),
               XDG_STATE_HOME=str(root / 'state'), XDG_DATA_HOME=str(root / 'data'),
               XDG_CONFIG_DIRS=str(root / 'system'), GSETTINGS_BACKEND='keyfile', LABWC_PID='')

    def run(operation='apply', success=True, environment=None):
        result = subprocess.run([adapter, operation, '--appearance', str(appearance), '--json'],
                                env=environment or env, capture_output=True, text=True)
        if environment and environment.get("CA_TEST_STATE_PATH"):
            if environment.get("CA_TEST_RETARGET"):
                assert appearance.resolve() == Path(environment["CA_TEST_RETARGET"]), "retarget hook did not execute"
            else:
                assert "# external writer" in appearance.read_text(), "mutation hook did not execute"
        assert (result.returncode == 0) == success, result.stdout + result.stderr
        return json.loads(result.stdout)

    labwc = root / 'config/labwc'
    labwc.mkdir(parents=True)
    (labwc / 'rc.xml').write_text('<labwc_config><theme><name>HoloNight</name></theme></labwc_config>')
    original = "# sparse v2\nversion=2\nfuture='preserved'\n[theme]\nscheme='holonight-light'\n"
    appearance.write_text(original)
    run()
    assert appearance.read_text() == original
    assert 'ColorScheme=holonight-light' in (root / 'config/kdeglobals').read_text()
    # Removing the explicit override applies the default without recreating it.
    appearance.write_text("# reset\nversion=2\n")
    run()
    assert appearance.read_text() == "# reset\nversion=2\n"
    assert 'ColorScheme=holonight-dark' in (root / 'config/kdeglobals').read_text()
    before = (root / 'config/gtk-3.0/settings.ini').read_bytes()
    for invalid in ["version=9\n", "version=2\n[typography]\nui_size=1\n"]:
        appearance.write_text(invalid)
        run(success=False)
        assert appearance.read_text() == invalid
        assert (root / 'config/gtk-3.0/settings.ini').read_bytes() == before
    state = root / 'state/holonight/appearance-adapters.json'
    old_state = state.read_bytes()
    theme = root / 'data/themes/HoloNight/labwc/themerc'
    old_theme = theme.read_bytes()
    old_xml = (labwc / 'rc.xml').read_bytes()
    # Exercise changes both before state publication and immediately after it.
    for trigger in [root / 'config/gtk-3.0/settings.ini', state]:
        appearance.write_text("version=2\n[theme]\nscheme='holonight-storm'\n[typography]\nui_size=18\n")
        response = run(success=False, environment=dict(env, LD_PRELOAD=interposer,
                       CA_TEST_STATE_PATH=str(trigger), CA_TEST_APPEARANCE_PATH=str(appearance)))
        assert any(item['name'] == 'canonical' and item['status'] == 'error' for item in response['outputs'])
        assert '# external writer' in appearance.read_text()
        assert state.read_bytes() == old_state
        assert theme.read_bytes() == old_theme
        assert (labwc / 'rc.xml').read_bytes() == old_xml
        assert (root / 'config/gtk-3.0/settings.ini').read_bytes() == before
    # Identical bytes at a different symlink target still invalidate this apply.
    first = root / 'first.toml'
    second = root / 'second.toml'
    first.write_text("version=2\n[theme]\nscheme='holonight-storm'\n[typography]\nui_size=18\n")
    second.write_bytes(first.read_bytes())
    appearance.unlink()
    appearance.symlink_to(first)
    response = run(success=False, environment=dict(env, LD_PRELOAD=interposer,
                   CA_TEST_STATE_PATH=str(state), CA_TEST_APPEARANCE_PATH=str(appearance),
                   CA_TEST_RETARGET=str(second)))
    assert appearance.resolve() == second
    assert second.read_bytes() == first.read_bytes()
    assert state.read_bytes() == old_state
    assert theme.read_bytes() == old_theme
    assert (labwc / 'rc.xml').read_bytes() == old_xml
    assert (root / 'config/gtk-3.0/settings.ini').read_bytes() == before
