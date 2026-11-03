#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>
"""Isolated CLI ownership, projection, failure and concurrency tests."""
import concurrent.futures
import json
import os
from pathlib import Path
import subprocess
import shutil
import select
import sys
import tempfile
import xml.etree.ElementTree as ET

adapter = sys.argv[1]
with tempfile.TemporaryDirectory() as temporary:
    root = Path(temporary)
    env = dict(os.environ, HOME=str(root), XDG_CONFIG_HOME=str(root / 'config'),
               XDG_DATA_HOME=str(root / 'data'), XDG_STATE_HOME=str(root / 'state'),
               XDG_CONFIG_DIRS=str(root / 'system'), GSETTINGS_BACKEND='keyfile', LABWC_PID='', HOLONIGHT_LABWC_CONFIG_DIR=str(root / 'custom'))
    appearance = root / 'appearance.toml'
    fixture = (Path(__file__).with_name('adapter_cli_tests.sh').read_text().split("<<'EOF'\n", 1)[1].split('\nEOF', 1)[0])
    appearance.write_text(fixture)
    config = root / 'custom'
    config.mkdir()
    rc = config / 'rc.xml'
    original = '<labwc_config><!--keep--><theme><name>HoloNight</name><font place="ActiveWindow"><name>Old</name><size>9</size><weight>bold</weight><slant>italic</slant></font><font place="MenuItem"><name>Menu</name><weight>bold</weight><slant>italic</slant></font></theme><keyboard/><snapping><overlay><enabled>no</enabled></overlay></snapping></labwc_config>'
    rc.write_text(original)
    theme = root / 'data/themes/HoloNight/labwc'

    def run(operation='apply', extra=(), check=True, environment=None):
        result = subprocess.run([adapter, operation, '--appearance', str(appearance), '--json', *extra], env=environment or env, capture_output=True, text=True)
        assert (result.returncode == 0) == check, result.stderr + result.stdout
        value = json.loads(result.stdout)
        assert value['protocol_version'] == 1
        assert all(x['status'] in {'applied', 'unchanged', 'restored', 'unavailable', 'delegated', 'application-owned', 'conflict', 'error'} and x['apply_mode'] in {'live', 'relaunch', 'delegated', 'session-restart'} for x in value['outputs'])
        return {x['name']: x for x in value['outputs']}

    menu = config / 'menu.xml'
    menu.write_text('<openbox_menu/>')
    result = run()
    assert result['labwc/theme']['status'] == 'applied'
    assert result['labwc/reload']['status'] == 'unavailable'
    assert len(list(theme.glob('*.svg'))) == 36
    for svg in theme.glob('*.svg'):
        element = ET.fromstring(svg.read_text())
        assert element.attrib['width'] == '30'
        assert 'currentColor' not in svg.read_text()
    doc = ET.fromstring(rc.read_text())
    assert doc.find('./snapping/overlay/enabled').text == 'no'
    assert doc.find('./theme/font[@place="ActiveWindow"]/size').text == '16'
    assert doc.find('./theme/font[@place="ActiveWindow"]/name').text == 'Inter'
    assert doc.find('./theme/font[@place="ActiveWindow"]/weight').text == 'bold'
    assert doc.find('./theme/font[@place="ActiveWindow"]/slant').text == 'italic'
    for place in ['ActiveWindow', 'InactiveWindow', 'MenuItem', 'MenuHeader']:
        assert doc.find(f'./theme/font[@place="{place}"]/name').text == 'Inter'
        assert doc.find(f'./theme/font[@place="{place}"]/size').text == '16'
    assert doc.find('./theme/font[@place="InactiveWindow"]/weight') is None
    assert doc.find('./theme/font[@place="InactiveWindow"]/slant') is None
    assert doc.find('./theme/font[@place="MenuItem"]/weight').text == 'bold'
    assert doc.find('./theme/font[@place="MenuItem"]/slant').text == 'italic'
    assert '<!--keep-->' in rc.read_text()
    first = (theme / 'themerc').read_bytes()
    themed_keys = [f'snapping.overlay.{kind}.{role}.color'
                   for kind in ('region', 'edge') for role in ('bg', 'border')]
    themed_keys += [f'osd.window-switcher.style-thumbnail.item.active.{role}.color'
                    for role in ('bg', 'border')]
    def colors(data):
        return dict(line.split(': ', 1) for line in data.decode().splitlines() if ': ' in line)
    assert all(key in colors(first) for key in themed_keys)
    assert not any('.enabled:' in line for line in first.decode().splitlines())
    assert menu.read_text() == '<openbox_menu/>'
    applied_xml = rc.read_bytes()
    assert run()['labwc/theme']['status'] == 'unchanged'
    assert rc.read_bytes() == applied_xml
    assert run('status')['labwc/file/themerc']['status'] == 'applied'
    assert run('status', environment=dict(env, XDG_DATA_HOME=str(root / 'different-data')))['labwc/file/themerc']['status'] == 'conflict'
    appearance.write_text(fixture.replace('title_family = "Audiowide"', 'title_family = "Rajdhani"').replace('title_size = 10', 'title_size = 24'))
    assert run()['labwc/theme']['status'] == 'unchanged'
    assert run('status')['labwc/font/ActiveWindow/name']['status'] == 'applied'
    appearance.write_text(fixture.replace('holonight-dark', 'holonight-light').replace('accent = "blue"', 'accent = "violet"').replace('ui_size = 12', 'ui_size = 18').replace('ui_family = "Inter"', 'ui_family = "DejaVu Sans"'))
    assert run('status')['labwc/file/themerc']['status'] == 'conflict'
    run()
    regenerated = (theme / 'themerc').read_bytes()
    assert all(colors(regenerated)[key] != colors(first)[key] for key in themed_keys)
    doc = ET.fromstring(rc.read_text())
    for place in ['ActiveWindow', 'InactiveWindow', 'MenuItem', 'MenuHeader']:
        assert doc.find(f'./theme/font[@place="{place}"]/name').text == 'DejaVu Sans'
        assert doc.find(f'./theme/font[@place="{place}"]/size').text == '24'
    override = config / 'themerc-override'
    enablement = 'snapping.overlay.*.bg.enabled: no\nsnapping.overlay.*.border.enabled: yes\n'
    override.write_text(enablement)
    assert 'labwc/overrides' not in run()
    assert override.read_text() == enablement
    for key in themed_keys:
        override.write_text(f'{key}: #12345678\n')
        assert run()['labwc/overrides']['status'] == 'conflict'
        assert run('status')['labwc/overrides']['status'] == 'conflict'
        assert override.read_text() == f'{key}: #12345678\n'
    override.write_text('window.*.title.bg.color: #123456\nborder.width: 4\n')
    assert run()['labwc/overrides']['status'] == 'conflict'
    assert run('status')['labwc/overrides']['status'] == 'conflict'
    assert '#123456' in override.read_text()
    rc.write_text(rc.read_text().replace('HoloNight', 'Other'))
    appearance.write_text(fixture)
    assert run()['labwc/selection']['status'] == 'delegated'
    assert (theme / 'themerc').read_bytes() != first
    rc.write_text(rc.read_text().replace('Other', 'HoloNight'))
    run()
    (theme / 'close-active.svg').write_text('external')
    assert run()['labwc/file/close-active.svg']['status'] == 'conflict'
    assert run('revert')['labwc/file/close-active.svg']['status'] == 'conflict'
    assert (theme / 'close-active.svg').read_text() == 'external'
    assert not (theme / 'themerc').exists()
    assert ET.fromstring(rc.read_text()).find('./theme/font[@place="ActiveWindow"]/name').text == 'Old'
    restored = ET.fromstring(rc.read_text())
    assert restored.find('./theme/font[@place="MenuItem"]/name').text == 'Menu'
    assert restored.find('./theme/font[@place="MenuItem"]/size') is None
    assert restored.find('./theme/font[@place="MenuHeader"]') is None
    assert restored.find('./theme/font[@place="InactiveWindow"]') is None
    # Restore the owned bytes, then complete revert and apply concurrently.
    state = json.loads((root / 'state/holonight/appearance-adapters.json').read_text())
    (theme / 'close-active.svg').write_text(state['entries']['labwc/file/close-active.svg']['last'])
    run('revert')
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as executor:
        results = list(executor.map(lambda _: run(), range(4)))
    assert sum(x['labwc/theme']['status'] == 'applied' for x in results) == 1
    # Successive font changes compare current output to the last write, not the new request.
    for family, points, pixels in [('DejaVu Sans', 14, '19'), ('Inter', 12, '16')]:
        appearance.write_text(fixture.replace('ui_size = 12', f'ui_size = {points}').replace('ui_family = "Inter"', f'ui_family = "{family}"'))
        assert run()['labwc/theme']['status'] == 'applied'
        doc = ET.fromstring(rc.read_text())
        assert doc.find('./theme/font[@place="ActiveWindow"]/name').text == family
        assert doc.find('./theme/font[@place="ActiveWindow"]/size').text == pixels
    # External menu font edits block apply and survive revert.
    edited = ET.fromstring(rc.read_text())
    edited.find('./theme/font[@place="MenuItem"]/size').text = '31'
    rc.write_text(ET.tostring(edited, encoding='unicode'))
    appearance.write_text(fixture.replace('holonight-dark', 'holonight-mocha').replace('accent = "blue"', 'accent = "violet"'))
    before_files = {p.name: p.read_bytes() for p in theme.iterdir()}
    before_xml = rc.read_bytes()
    conflict = run()['labwc/font/MenuItem/size']
    assert conflict['status'] == 'conflict'
    assert all(text in conflict['diagnostic'] for text in ['Whole labwc update skipped', 'MenuItem/size', str(rc), '[16]', '[31]'])
    assert rc.read_bytes() == before_xml
    assert {p.name: p.read_bytes() for p in theme.iterdir()} == before_files
    appearance.write_text(fixture)
    assert run('revert')['labwc/font/MenuItem/size']['status'] == 'conflict'
    assert ET.fromstring(rc.read_text()).find('./theme/font[@place="MenuItem"]/size').text == '31'
    edited = ET.fromstring(rc.read_text())
    edited.find('./theme/font[@place="MenuItem"]/size').text = '16'
    rc.write_text(ET.tostring(edited, encoding='unicode'))
    run('revert')
    run()
    # Reconciled output applies the latest colors and reloads exactly once.
    with subprocess.Popen([sys.argv[2]], stdout=subprocess.PIPE, text=True) as compositor:
        try:
            assert compositor.stdout.readline().strip() == 'ready'
            live_env = dict(env, LABWC_PID=str(compositor.pid))
            appearance.write_text(fixture.replace('holonight-dark', 'holonight-mocha').replace('accent = "blue"', 'accent = "violet"'))
            result = run(environment=live_env)
            assert result['labwc/theme']['status'] == 'applied'
            assert 'labwc/reload' not in result
            assert select.select([compositor.stdout], [], [], 2)[0]
            assert compositor.stdout.readline().strip() == 'reload'
            assert run(environment=live_env)['labwc/theme']['status'] == 'unchanged'
            assert not select.select([compositor.stdout], [], [], 0.1)[0]
            assert (theme / 'themerc').read_bytes() != first
        finally:
            compositor.terminate()
            compositor.wait(timeout=5)
    appearance.write_text(fixture)
    run()
    # A changed title font is preserved and blocks a complete labwc update.
    rc.write_text(rc.read_text().replace('Inter', 'External'))
    assert run()['labwc/font/ActiveWindow/name']['status'] == 'conflict'
    assert run('revert')['labwc/font/ActiveWindow/name']['status'] == 'conflict'
    assert 'External' in rc.read_text()
    # Malformed XML does not prevent other adapter outputs from working.
    rc.write_text('<broken')
    assert run()['labwc/selection']['status'] == 'conflict'
    assert rc.read_text() == '<broken'
    # Unowned generated paths are never adopted automatically.
    rc.write_text(original)
    (theme / 'themerc').write_text('unowned')
    assert run()['labwc/file/themerc']['status'] == 'conflict'
    # Failed stage write rolls back GTK/KDE projections too.
    broken = root / 'blocked'
    broken.write_text('file')
    failed_env = dict(env, XDG_DATA_HOME=str(broken), XDG_STATE_HOME=str(root / 'failure-state'))
    before = (root / 'config/gtk-3.0/settings.ini').read_bytes()
    run(check=False, environment=failed_env)
    assert (root / 'config/gtk-3.0/settings.ini').read_bytes() == before
    # A font write failure after theme replacement rolls all owned outputs back.
    locked_config = root / 'font-failure'
    locked_config.mkdir()
    locked_rc = locked_config / 'rc.xml'
    locked_rc.write_text(original)
    locked_config.chmod(0o555)
    font_env = dict(env, XDG_DATA_HOME=str(root / 'font-failure-data'), XDG_STATE_HOME=str(root / 'font-failure-state'))
    try:
        result = subprocess.run([adapter, 'apply', '--appearance', str(appearance), '--labwc-config', str(locked_config), '--json'], env=font_env, capture_output=True, text=True)
        assert result.returncode == 1 and 'transaction rolled back' in result.stdout
        assert locked_rc.read_text() == original
        assert not (root / 'font-failure-data/themes/HoloNight/labwc/themerc').exists()
        assert (root / 'config/gtk-3.0/settings.ini').read_bytes() == before
    finally:
        locked_config.chmod(0o755)
    # Replay an interrupted update before status; then preserve external edits during recovery.
    recovery_state = root / 'recovery-state/holonight/appearance-adapters.json'
    recovery_state.parent.mkdir(parents=True)
    recovery_state.write_text('{"entries":{}}')
    target = root / 'recovery-file'
    target.write_text('interrupted')
    entry = {'kind': 'labwc-file', 'target': str(target), 'last': 'interrupted', 'original': 'before'}
    journal = Path(str(recovery_state) + '.labwc-recovery.json')
    journal.write_text(json.dumps({'entries': {'labwc/file/test': entry}, 'before': {'labwc/file/test': entry}}))
    recovery_env = dict(env, XDG_STATE_HOME=str(root / 'recovery-state'))
    run('status', environment=recovery_env)
    assert target.read_text() == 'before' and not journal.exists()
    journal.write_text(json.dumps({'entries': {'labwc/file/test': entry}, 'before': {'labwc/file/test': entry}}))
    target.write_text('external')
    assert run('status', check=False, environment=recovery_env)['labwc/recovery']['status'] == 'error'
    assert target.read_text() == 'external' and journal.exists()
    # Standard XDG search can select a system rc.xml, without creating user rc.xml.
    system = root / 'system/labwc'
    system.mkdir(parents=True)
    (system / 'rc.xml').write_text('<labwc_config><theme><name>Other</name></theme></labwc_config>')
    result = subprocess.run([adapter, 'status', '--json'], env=dict(env, HOLONIGHT_LABWC_CONFIG_DIR=''), capture_output=True, text=True, check=True)
    assert 'HoloNight is not selected' in result.stdout
    assert not (root / 'config/labwc/rc.xml').exists()
    # Recover the interruption between the two directory renames, preserving unknown files.
    gap_state = root / 'gap-state/holonight/appearance-adapters.json'
    gap_state.parent.mkdir(parents=True)
    gap_state.write_text('{"entries":{}}')
    directory = root / 'gap-theme'
    backup = root / 'gap-backup'
    backup.mkdir()
    (backup / 'themerc').write_text('before')
    (backup / 'unmanaged').write_text('keep')
    gap_entry = {'kind': 'labwc-file', 'target': str(directory / 'themerc'), 'last': 'interrupted', 'original': 'before'}
    gap_journal = Path(str(gap_state) + '.labwc-recovery.json')
    gap_journal.write_text(json.dumps({'entries': {'labwc/file/themerc': gap_entry}, 'before': {'labwc/file/themerc': gap_entry}, 'directory': str(directory), 'backup': str(backup)}))
    run('status', environment=dict(env, XDG_STATE_HOME=str(root / 'gap-state')))
    assert (directory / 'themerc').read_text() == 'before'
    assert (directory / 'unmanaged').read_text() == 'keep'
    assert not gap_journal.exists() and not backup.exists()
    # Unset XDG paths resolve below HOME, while a normal user rc.xml selects the theme.
    home = root / 'home-fallback'
    default_rc = home / '.config/labwc/rc.xml'
    default_rc.parent.mkdir(parents=True)
    default_rc.write_text(original)
    home_env = dict(env, HOME=str(home))
    for key in ['XDG_CONFIG_HOME', 'XDG_DATA_HOME', 'XDG_STATE_HOME', 'HOLONIGHT_LABWC_CONFIG_DIR']:
        home_env.pop(key)
    # A fallback installed under ~/.local is recognized only when its entire payload is unchanged.
    home_theme = home / '.local/share/themes/HoloNight/labwc'
    shutil.copytree(Path(adapter).parent / 'labwc-fallback', home_theme)
    fallback_bytes = (home_theme / 'themerc').read_bytes()
    subprocess.run([adapter, 'apply', '--appearance', str(appearance), '--json'], env=home_env, check=True, capture_output=True)
    assert (home / '.local/share/themes/HoloNight/labwc/themerc').exists()
    subprocess.run([adapter, 'revert', '--json'], env=home_env, check=True, capture_output=True)
    assert (home_theme / 'themerc').read_bytes() == fallback_bytes
print('labwc CLI checks passed')
