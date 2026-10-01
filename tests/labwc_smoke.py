#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>
"""Optional labwc 0.20.2 headless visual smoke test; requires GTK3 GI, grim, wlr-randr."""
import json
import os
from pathlib import Path
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import time

if len(sys.argv) > 1 and sys.argv[1] == '--client':
    import gi
    gi.require_version('Gtk', '3.0')
    from gi.repository import Gtk
    windows = []
    for title in ['Inactive HoloNight window', 'Active HoloNight window']:
        window = Gtk.Window(title=title)
        window.set_default_size(420, 220)
        window.add(Gtk.Label(label='Server-side decoration smoke test'))
        window.show_all()
        windows.append(window)
    Gtk.main()
    sys.exit()

class Pointer:
    """Minimal wire client for wlr-virtual-pointer v1; no host input is used."""
    def __init__(self, env):
        self.socket = socket.socket(socket.AF_UNIX)
        self.socket.connect(str(Path(env['XDG_RUNTIME_DIR']) / env['WAYLAND_DISPLAY']))
        self.socket.settimeout(3)
        self.send(1, 1, struct.pack('I', 2))  # get_registry
        self.send(1, 0, struct.pack('I', 3))  # sync
        globals_ = {}
        while True:
            obj, opcode, data = self.receive()
            if obj == 2 and opcode == 0:
                name, length = struct.unpack_from('II', data)
                interface = data[8:8+length-1].decode()
                globals_[interface] = name
            if obj == 3 and opcode == 0:
                break
        interface = b'zwlr_virtual_pointer_manager_v1\0'
        string = struct.pack('I', len(interface)) + interface + b'\0' * (-len(interface) % 4)
        self.send(2, 0, struct.pack('I', globals_['zwlr_virtual_pointer_manager_v1']) + string + struct.pack('II', 1, 4))
        self.send(4, 0, struct.pack('II', 0, 5))
    def send(self, obj, opcode, data=b''):
        self.socket.sendall(struct.pack('II', obj, ((len(data)+8) << 16) | opcode) + data)
    def receive(self):
        def exact(size):
            result = b''
            while len(result) < size:
                chunk = self.socket.recv(size - len(result))
                if not chunk: raise RuntimeError('Wayland connection closed')
                result += chunk
            return result
        obj, header = struct.unpack('II', exact(8))
        return obj, header & 0xffff, exact((header >> 16) - 8)
    def move(self, x, y, width, height):
        self.send(5, 1, struct.pack('IIIII', int(time.monotonic()*1000) & 0xffffffff, x, y, width, height))
        self.send(5, 4)
        time.sleep(.15)
    def click(self):
        for state in [1, 0]:
            self.send(5, 2, struct.pack('III', int(time.monotonic()*1000) & 0xffffffff, 272, state))
            self.send(5, 4)
            time.sleep(.1)

adapter = str(Path(sys.argv[1]).resolve())
output = Path(sys.argv[2]).resolve()
output.mkdir(parents=True, exist_ok=True)
assert '0.20.2' in subprocess.check_output(['labwc', '--version'], text=True)
with tempfile.TemporaryDirectory(prefix='holonight-labwc-smoke-') as temporary:
    root = Path(temporary)
    runtime = root / 'runtime'
    runtime.mkdir(mode=0o700)
    config = root / 'config/labwc'
    config.mkdir(parents=True)
    env = dict(os.environ, HOME=str(root), XDG_CONFIG_HOME=str(root/'config'), XDG_DATA_HOME=str(root/'data'), XDG_STATE_HOME=str(root/'state'), XDG_RUNTIME_DIR=str(runtime), XDG_CONFIG_DIRS=str(root/'system'), WLR_BACKENDS='headless', WLR_RENDERER='pixman', WLR_SCENE_DISABLE_DIRECT_SCANOUT='1', WLR_HEADLESS_OUTPUTS='1', LABWC_UPDATE_ACTIVATION_ENV='0', GSETTINGS_BACKEND='keyfile', GDK_BACKEND='wayland')
    for key in ['WAYLAND_DISPLAY', 'DISPLAY', 'LABWC_PID', 'DBUS_SESSION_BUS_ADDRESS']:
        env.pop(key, None)
    fixture = Path(__file__).with_name('adapter_cli_tests.sh').read_text().split("<<'EOF'\n", 1)[1].split('\nEOF', 1)[0]
    appearance = root / 'appearance.toml'
    appearance.write_text(fixture)
    # Extra controls intentionally included to test all toggle states.
    (config / 'rc.xml').write_text('''<labwc_config><theme><name>HoloNight</name><titlebar><layout>menu,shade,desk:iconify,max,close</layout></titlebar><dropShadows>yes</dropShadows><cornerRadius>8</cornerRadius><maximizedDecoration>titlebar</maximizedDecoration></theme><windowRules><windowRule title="Inactive*"><action name="MoveTo" x="80" y="100"/></windowRule><windowRule title="Active*"><action name="MoveTo" x="350" y="250"/></windowRule></windowRules></labwc_config>''')
    subprocess.run([adapter, 'apply', '--appearance', str(appearance), '--json'], env=env, check=True, capture_output=True)
    with (output/'labwc.log').open('w') as log:
        compositor = subprocess.Popen(['labwc', '-C', str(config), '-V'], env=env, stdout=log, stderr=log)
        client = None
        try:
            for _ in range(100):
                sockets = list(runtime.glob('wayland-*'))
                sockets = [x for x in sockets if not x.name.endswith('.lock')]
                if sockets: break
                if compositor.poll() is not None: raise RuntimeError((output/'labwc.log').read_text())
                time.sleep(.05)
            env['WAYLAND_DISPLAY'] = sockets[0].name
            env['LABWC_PID'] = str(compositor.pid)
            subprocess.run(['wlr-randr', '--output', 'HEADLESS-1', '--custom-mode', '1920x1080', '--scale', '1'], env=env, check=True)
            client = subprocess.Popen([sys.executable, __file__, '--client'], env=env, stdout=log, stderr=log)
            time.sleep(1)
            pointer = Pointer(env)
            def capture(name):
                time.sleep(.3)
                subprocess.run(['grim', str(output/(name+'.png'))], env=env, check=True)
            capture('dark-1x')
            # MoveTo positions the outer decoration; active titlebar starts at (350,250).
            pointer.move(746, 268, 1920, 1080)
            capture('close-hover-1x')
            pointer.move(714, 268, 1920, 1080)
            capture('maximize-hover-1x')
            pointer.click()
            capture('maximized-1x')
            pointer.move(1862, 17, 1920, 1080)
            pointer.click()
            capture('restored-1x')
            pointer.move(408, 268, 1920, 1080)
            pointer.click()
            capture('shade-1x')
            pointer.click()
            pointer.move(476, 118, 1920, 1080)
            capture('inactive-close-hover-1x')
            pointer.move(106, 118, 1920, 1080)
            pointer.click()
            capture('menu-1x')
            pointer.move(1000, 700, 1920, 1080)
            pointer.click()
            appearance.write_text(fixture.replace('holonight-dark', 'holonight-light').replace('accent = "blue"', 'accent = "violet"'))
            response = subprocess.run([adapter, 'apply', '--appearance', str(appearance), '--json'], env=env, check=True, capture_output=True, text=True)
            (output/'reload-response.json').write_text(response.stdout)
            time.sleep(.3)
            capture('light-reloaded-1x')
            subprocess.run(['wlr-randr', '--output', 'HEADLESS-1', '--scale', '2'], env=env, check=True)
            time.sleep(.4)
            capture('light-2x')
            pointer.socket.close()
        finally:
            if client:
                client.terminate()
                client.wait(timeout=5)
            compositor.terminate()
            compositor.wait(timeout=5)
print(f'Screenshots and log: {output}')
