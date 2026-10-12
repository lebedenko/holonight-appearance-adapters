#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Reject incompatible installed providers before building a consumer."""
import json
import os
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tooling'))
import external
import workflow

prepared = Path('/prepared')
consumer = prepared / 'consumer'
state = json.loads((prepared / 'provider-state.json').read_text())
config = json.loads((consumer / 'tooling/module.json').read_text())
# Compare in the original preparation context: paths are part of the identities.
workflow.ROOT = consumer
for name in ('config', 'system-services', 'qt'):
    os.environ['HOLONIGHT_' + name.upper().replace('-', '_') + '_SOURCE'] = str(prepared / name)
context = workflow.build_context('test')
resolved = external.check(config, features=workflow.selected_features(config, 'test'), cmake_options=context)
toolchain = workflow.toolchain_signature(context)
for dependency in config['dependencies']:
    source = workflow.dependency_source(dependency)
    expected = {**workflow.source_signature(source), 'prefix': str(consumer / 'build/deps/prefix'),
                'options': [*workflow.provider_options(source, config), *dependency.get('options', [])],
                'cmake_subdirectory': dependency.get('cmake_subdirectory', ''), 'wayland': config.get('wayland', False),
                'build_type': 'Debug', 'toolchain': toolchain, 'external': resolved}
    # HOLONIGHT_DEPENDENCY_PREFIX is intentionally absent from canonical toolchain identities.
    if state[dependency['module']] != expected:
        raise RuntimeError('Incompatible prepared provider: ' + dependency['module'])
workflow.packages(config, consumer / 'build/deps/prefix')
packages = subprocess.check_output(['pacman', '--config', '/dev/null', '-Q'], text=True)
if packages != (prepared / 'packages.txt').read_text():
    raise RuntimeError('Prepared package identity mismatch')
