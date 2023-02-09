'''
TODO: Comment
'''

from pathlib import Path
from shutil import rmtree
import subprocess
from time import time
import sys
from types import SimpleNamespace
import yaml
import traceback
import binascii
import pickle
from jinja2 import Template

ROOT_DIR = Path(__file__).parent.parent.parent.parent.parent.parent.resolve()
GIT_DIR = ROOT_DIR / 'nrf'
#GLOB = '**/sample.yaml'
GLOB = 'nrf/samples/bluetooth/**/sample.yaml'
#GLOB = 'nrf/**/sample.yaml'
#GLOB = 'zephyr/samples/basic/minimal/**/sample.yaml'
#GLOB = 'zephyr/samples/basic/**/sample.yaml'

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

REBUILD = set([
    #'HKDF example',
    #'Machine learning',
    #'ECDH example',
    #'AES CBC example',
    #'HMAC example',
    #'Matter Weather Station',
    #'BLE UART service',
    #'BLE MDS service',
])


class TestException(Exception):
    def __init__(self, type, message) -> None:
        super().__init__(f'{type}: {message}')
        self.type = type
        self.message = message
    def get_result(self):
        return SimpleNamespace(type=self.type, message=self.message)


def find_platform(name: str, sample_dir: Path, info: dict):
    def get_list(data):
        if data is None:
            return []
        elif isinstance(data, str):
            return data.split()
        else:
            return data
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
    return platform


def build_sample(name: str, sample_dir: Path, platform: str):
    print(f'Building {name} ({sample_dir}) with {platform}...')
    proc = subprocess.run(['west', 'build', '-b', platform, '-d', 'build_sbom_test_cmp'], cwd=sample_dir, check=False)
    print('Done')
    if proc.returncode != 0:
        raise TestException('error', f'Build failed with code: {proc.returncode}')


def checkout_branch(branch):
    subprocess.run(['git', 'checkout', branch], cwd=GIT_DIR, check=True)


def process_sample(name: str, sample_dir: Path):
    print(f'Processing {name} ({sample_dir})...')
    proc = subprocess.run([
            'west', 'ncs-sbom',
            '-d', 'build_sbom_test_cmp',
            '--license-detectors', 'spdx-tag,full-text,external-file'
        ],
        cwd=sample_dir,
        stderr=subprocess.STDOUT,
        stdout=subprocess.PIPE,
        check=False)
    print('Done')
    if proc.returncode != 0:
        raise TestException('error', f'SBOM failed with {proc.returncode}: {proc.stdout.decode()}')


def check_sample(name: str, sample_dir: Path, info: dict, test: SimpleNamespace):

    test.platform = find_platform(name, sample_dir, info)

    try:
        build_sample(name, sample_dir, test.platform)
    except TestException as _:
        try:
            rmtree(sample_dir / 'build_sbom_test_cmp', ignore_errors=True)
            test.platform = DEFAULT_PLATFORMS[0]
            build_sample(name, sample_dir, test.platform)
        except TestException as ex:
            test.build = ex.get_result()
            test.test1 = TestException('-', 'N/A').get_result()
            test.test2 = TestException('-', 'N/A').get_result()
            return

    checkout_branch('test-1')

    start_time = time()
    try:
        process_sample(name, sample_dir)
    except TestException as ex:
        test.test1 = ex.get_result()
        test.test2 = TestException('-', 'N/A').get_result()
        return
    test.test1 = TestException(str(round((time() - start_time) * 10) / 10) + 's', 'OK').get_result()

    checkout_branch('test-2')

    start_time = time()
    try:
        process_sample(name, sample_dir)
    except TestException as ex:
        test.test2 = ex.get_result()
        return
    test.test2 = TestException(str(round((time() - start_time) * 10) / 10) + 's', 'OK').get_result()


    # TODO interpret results

    rmtree(sample_dir / 'build_sbom_test_cmp', ignore_errors=True)


def write_report(state: 'dict[str, SimpleNamespace]'):
    output_file = ROOT_DIR / 'sbom-test-cmp.html'
    with open(Path(__file__).parent / 'sbom-test-cmp.html.jinja2', 'r', encoding='utf-8') as fd:
        template_source = fd.read()
    template = Template(template_source)
    out = template.render(state=state)
    with open(output_file, 'w', encoding='utf-8') as fd:
        fd.write(out)


def main():
    state = dict()
    print('Root dir: ', ROOT_DIR)
    state_file = ROOT_DIR / 'sbom-test-compare.pickle'
    if state_file.exists():
        with state_file.open('rb') as fd:
            state = pickle.load(fd)
            write_report(state)
    result_ok = TestException('ok', 'OK').get_result()
    for yaml_file in ROOT_DIR.glob(GLOB):
        sample_dir = yaml_file.parent
        info = yaml.full_load(yaml_file.open())
        name = (info.get('sample') or {}).get('name') or (sample_dir.parent.name + '/' + sample_dir.name)
        if (str(sample_dir) in state) and (name not in REBUILD):
            continue
        skip = False
        try:
            test = SimpleNamespace(
                name=name,
                dir=sample_dir,
                reldir=sample_dir.relative_to(ROOT_DIR),
                build=result_ok,
                test1=result_ok,
                test2=result_ok,
                unknown=None,
                )
            check_sample(name, sample_dir, info, test)
        except KeyboardInterrupt as _:
            skip = True
            exit(1)
        except BaseException as _:
            test.unknown = TestException('exception', traceback.format_exc()).get_result()
        finally:
            if skip:
                exit(1)
            state[str(sample_dir)] = test
            with state_file.open('wb') as fd:
                pickle.dump(state, fd)
            write_report(state)


if __name__ == "__main__":
    main()
