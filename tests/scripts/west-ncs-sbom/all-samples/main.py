'''
TODO: Comment
'''

from pathlib import Path
from shutil import rmtree
import subprocess
import sys
from types import SimpleNamespace
import yaml
import traceback
import binascii
import pickle
from jinja2 import Template

ROOT_DIR = Path(__file__).parent.parent.parent.parent.parent.parent.resolve()
#GLOB = '**/sample.yaml'
#GLOB = 'nrf/samples/bluetooth/**/sample.yaml'
GLOB = 'nrf/**/sample.yaml'

DEFAULT_PLATFORMS = [
    'nrf52840dk_nrf52840',
    'nrf5340dk_nrf5340',
    'nrf52832_mdk',
    'nrf52840_mdk',
    'nrf52dk_nrf52832',
    'nrf9160dk_nrf9160',
    'thingy52_nrf52832',
    'thingy53_nrf5340',
]

state = dict()
state_file = Path()
current_test = None

def get_list(data):
    if data is None:
        return []
    elif isinstance(data, str):
        return data.split()
    else:
        return data

def check_sample(name: str, sample_dir: Path, info: dict):
    global current_test
    all_platforms = set()
    preffered_platforms = set()
    exluded_platforms = set()
    hash = [name, str(sample_dir)]
    if ('tests' in info) and isinstance(info['tests'], dict):
        for (test_name, test) in info['tests'].items():
            hash.append(test_name)
            for plat in get_list(test.get('integration_platforms')):
                preffered_platforms.add(plat)
            for plat in get_list(test.get('platform_allow')):
                all_platforms.add(plat)
            for plat in get_list(test.get('platform_exclude')):
                exluded_platforms.add(plat)
    preffered_platforms = set(x for x in preffered_platforms if x.lower().count('nrf'))
    all_platforms = set(x for x in all_platforms if x.lower().count('nrf'))
    if len(preffered_platforms) == 0:
        preffered_platforms = all_platforms
    preffered_platforms = set(x for x in preffered_platforms if x not in exluded_platforms)
    if len(preffered_platforms) == 0:
        preffered_platforms = set(x for x in DEFAULT_PLATFORMS if x not in exluded_platforms)
        if len(preffered_platforms) == 0:
            preffered_platforms = DEFAULT_PLATFORMS[0]
    preffered_platforms = list(preffered_platforms)
    preffered_platforms.sort()
    hash.sort()
    platform = preffered_platforms[binascii.crc32(','.join(hash).encode()) % len(preffered_platforms)]
    current_test.platform = platform
    print(f'Building {name} ({sample_dir}) with {platform}...')
    proc = subprocess.run(['west', 'build', '-b', platform, '-d', 'build_sbom_test_all'], cwd=sample_dir)
    print('Done')
    if proc.returncode != 0:
        results_put('build', f'Build failed with code: {proc.returncode}')
        return
    print(f'Processing {name} ({sample_dir}) with {platform}...')
    proc = subprocess.run(['west', 'ncs-sbom', '-d', 'build_sbom_test_all', '--license-detectors', 'spdx-tag,full-text,external-file'], cwd=sample_dir)
    print('Done')
    if proc.returncode != 0:
        results_put('failed', f'SBOM failed with code: {proc.returncode}')
        return
    results_put('ok', 'OK')
    rmtree(sample_dir / 'build_sbom_test_all', ignore_errors=True)

def write_report():
    output_file = ROOT_DIR / 'sbom-test-all.html'
    with open(Path(__file__).parent / 'template.html.jinja2', 'r') as fd:
        template_source = fd.read()
    template = Template(template_source)
    out = template.render(state=state)
    with open(output_file, 'w') as fd:
        fd.write(out)

def results_start(name: str, sample_dir: Path):
    global current_test, state, state_file
    print(f'---------------{name}----------')
    current_test = SimpleNamespace(
        name=name,
        dir=sample_dir,
        reldir=sample_dir.relative_to(ROOT_DIR),
        result='unknown',
        message='')

def results_end(skip=False):
    global current_test, state, state_file
    print(f'---------------------------------')
    if (current_test is None) or skip:
        current_test = None
        return
    state[str(current_test.dir)] = current_test
    current_test = None
    with state_file.open('wb') as fd:
        pickle.dump(state, fd)
    write_report()

def results_put(result: str, message: str):
    global current_test, state, state_file
    print(message, file=sys.stderr)
    current_test.result = result
    current_test.message += message + '\n'


REBUILD = set([
    #'HKDF example',
    #'Machine learning',
    #'ECDH example',
    #'AES CBC example',
    #'HMAC example',
    #'Matter Weather Station',
])

def main():
    global state, state_file
    print('Root dir: ', ROOT_DIR)
    state_file = ROOT_DIR / 'sbom-test-all.pickle'
    if state_file.exists():
        with state_file.open('rb') as fd:
            state = pickle.load(fd)
            write_report()
    for yaml_file in ROOT_DIR.glob(GLOB):
        sample_dir = yaml_file.parent
        info = yaml.full_load(yaml_file.open())
        name = (info.get('sample') or {}).get('name') or (sample_dir.parent.name + '/' + sample_dir.name)
        if (str(sample_dir) in state) and (name not in REBUILD):
            continue
        try:
            results_start(name, sample_dir)
            check_sample(name, sample_dir, info)
        except KeyboardInterrupt as e:
            results_end(True)
            exit(1)
        except BaseException as e:
            results_put('exception', traceback.format_exc())
        finally:
            results_end()

if __name__ == "__main__":
    main()
