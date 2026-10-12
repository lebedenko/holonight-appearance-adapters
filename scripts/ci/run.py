#!/usr/bin/env python3
"""Run push validation against isolated current working-tree inputs."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
LANES = ('full', 'minimal', 'release', 'static', 'licensing')
PROVIDERS = {
    'config': '0ccd25f323662f50a47bb048be636160f2b7c135',
    'system-services': '70614eb70329af7014853c2973e5624acf0b251e',
    'qt': '7c3ce074a8c2e02e4ee8cad8cd19eb8627aa4f68',
}


def preparation_identity(images):
    return {'providers': PROVIDERS, 'image': images['build'], 'build_type': 'Debug',
            'bundle': (ROOT / 'tooling/VERSION').read_text().strip(),
            'module': json.loads((ROOT / 'tooling/module.json').read_text()),
            'preset': json.loads((ROOT / 'CMakePresets.json').read_text())['configurePresets'][1]}


def source_identity(root):
    return {str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in sorted(root.rglob('*')) if path.is_file()}


def git(*args):
    return subprocess.check_output(['git', '-C', str(ROOT), *args])


def snapshot(destination):
    tracked = git('ls-files', '-z').split(b'\0')
    new = git('ls-files', '--others', '--exclude-standard', '-z').split(b'\0')
    for raw in dict.fromkeys(tracked + new):
        if not raw:
            continue
        name = os.fsdecode(raw)
        source = ROOT / name
        if source.is_symlink() or source.is_file():
            target = destination / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target, follow_symlinks=False)
        elif source.exists():
            raise RuntimeError(f'Unsupported snapshot input: {name}')
    return [os.fsdecode(raw) for raw in new if raw]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--lane', choices=LANES, action='append')
    parser.add_argument('--acquire-images', action='store_true', help='Explicitly pull required images before offline acceptance')
    parser.add_argument('--prepare', action='store_true', help='Acquire exact inputs and prepare installed providers explicitly')
    for provider in PROVIDERS:
        parser.add_argument('--' + provider + '-source', type=Path, help='Supply a checkout containing the exact provider revision')
    args = parser.parse_args()
    output = ROOT / 'build' / 'ci'
    output.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix=datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ-'), dir=output))
    print(f'CI logs: {output}', flush=True)
    report = {'revision': git('rev-parse', 'HEAD').decode().strip(),
              'status': git('status', '--porcelain=v1', '-z').decode(errors='surrogateescape'),
              'platform': 'linux/amd64', 'lanes': {}}
    report['dirty'] = bool(report['status'])
    failed = False
    try:
        runtime = shutil.which('docker') or shutil.which('podman')
        if not runtime:
            raise RuntimeError('Neither Docker nor Podman is available; install a container runtime.')
        report['runtime'] = subprocess.check_output([runtime, '--version'], text=True).strip()
        images = json.loads((ROOT / 'scripts/ci/images.json').read_text())
        prepared = ROOT / 'build/ci-prepared'
        identity = preparation_identity(images)
        report['preparation_identity'] = identity
        if args.acquire_images or args.prepare:
            for image in dict.fromkeys(images['licensing' if lane == 'licensing' else 'build'] for lane in args.lane or LANES):
                subprocess.run([runtime, 'pull', '--platform', report['platform'], image], check=True)
        if args.prepare:
            if prepared.exists():
                previous = prepared / 'inputs.json'
                if not previous.is_file() or json.loads(previous.read_text()) != identity:
                    number = 1
                    while prepared.with_name('ci-prepared.previous-' + str(number)).exists():
                        number += 1
                    prepared.rename(prepared.with_name('ci-prepared.previous-' + str(number)))
            prepared.mkdir(parents=True, exist_ok=True)
            (prepared / 'inputs.json').write_text(json.dumps(identity, indent=2) + '\n')
            for provider, revision in PROVIDERS.items():
                supplied = getattr(args, provider.replace('-', '_') + '_source')
                checkout = supplied or prepared / ('acquired-' + provider)
                if not supplied and not checkout.exists():
                    subprocess.run(['git', 'clone', '--no-checkout',
                                    'https://github.com/lebedenko/holonight-' + provider + '.git', str(checkout)], check=True)
                archive = subprocess.check_output(['git', '-C', str(checkout), 'archive', revision])
                destination = prepared / provider
                with tempfile.TemporaryDirectory(prefix='provider-export-') as export:
                    exported = Path(export)
                    subprocess.run(['tar', '-x', '-C', str(exported)], input=archive, check=True)
                    if destination.exists() and source_identity(destination) == source_identity(exported):
                        continue
                    if destination.exists():
                        number = 1
                        while destination.with_name(provider + '.previous-' + str(number)).exists():
                            number += 1
                        destination.rename(destination.with_name(provider + '.previous-' + str(number)))
                    shutil.copytree(exported, destination, symlinks=True)
        elif any(lane != 'licensing' for lane in args.lane or LANES):
            if not (prepared / 'manifest.json').is_file():
                raise RuntimeError('Missing prepared provider artifacts; run task ci:prepare explicitly.')
            manifest = json.loads((prepared / 'manifest.json').read_text())
            if manifest != identity or not (prepared / 'provider-state.json').is_file():
                raise RuntimeError('Prepared provider identity mismatch; run explicit CI preparation.')
        with tempfile.TemporaryDirectory(prefix='holonight-appearance-adapters-ci-') as temporary:
            source = Path(temporary) / 'source'
            source.mkdir()
            account_options = []
            if os.getuid() != 0:
                passwd = Path(temporary) / 'passwd'
                group = Path(temporary) / 'group'
                passwd.write_text('root:x:0:0:root:/root:/bin/sh\n'
                                  f'ci:x:{os.getuid()}:{os.getgid()}:CI:/work/build/home:/bin/sh\n'
                                  'nobody:x:65534:65534:nobody:/nonexistent:/bin/sh\n')
                group.write_text('root:x:0:\n' + f'ci:x:{os.getgid()}:\n' + 'nobody:x:65534:\n')
                account_options = ['--mount', f'type=bind,src={passwd},dst=/etc/passwd,readonly',
                                   '--mount', f'type=bind,src={group},dst=/etc/group,readonly']
            report['untracked_inputs'] = snapshot(source)
            if report['untracked_inputs']:
                print('Untracked inputs included; add before pushing:')
                for name in report['untracked_inputs']:
                    print(f'  {name!r}')
            for lane in ('prepare',) if args.prepare else args.lane or LANES:
                image = images['licensing' if lane == 'licensing' else 'build']
                command = [runtime, 'run', '--rm', '--pull=never', '--network=none', '--platform', report['platform'], '--user', f'{os.getuid()}:{os.getgid()}',
                           '--mount', f'type=bind,src={source},dst=/input,readonly',
                           '--tmpfs', '/work:rw,exec,mode=1777', '--workdir', '/work',
                           '--env', 'JOBS=' + os.environ.get('JOBS', '2'),
                           '--entrypoint', '/bin/sh', image, '/input/scripts/ci/lane.sh', lane]
                if Path(runtime).name == 'podman' and os.getuid() != 0:
                    command.insert(2, '--userns=keep-id')
                if lane != 'licensing':
                    prepared.mkdir(parents=True, exist_ok=True)
                    index = command.index(image)
                    command[index:index] = ['--mount', f'type=bind,src={prepared},dst=/prepared' + ('' if lane == 'prepare' else ',readonly')]
                image_index = command.index(image)
                command[image_index:image_index] = account_options
                print(f'Running {lane}: {image}', flush=True)
                with (output / f'{lane}.log').open('w') as log:
                    result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
                report['lanes'][lane] = {'image': image, 'exit_code': result.returncode}
                print(f'{lane}: {"PASS" if result.returncode == 0 else "FAIL"} (see {output / (lane + ".log")})', flush=True)
                if result.returncode != 0:
                    print((output / f'{lane}.log').read_text(errors='replace'), flush=True)
                failed |= result.returncode != 0
                if lane == 'prepare' and result.returncode == 0:
                    (prepared / 'manifest.json').write_text(json.dumps(identity, indent=2) + '\n')
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        report['error'] = str(error)
        print(f'CI failed: {error}', file=sys.stderr)
        failed = True
    finally:
        (output / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
    return int(failed)


if __name__ == '__main__':
    sys.exit(main())
