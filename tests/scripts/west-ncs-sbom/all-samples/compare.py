'''
TODO: Comment
'''

import json
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
GLOB = '**/sample.yaml'
#GLOB = 'nrf/samples/bluetooth/**/sample.yaml'
#GLOB = 'nrf/**/sample.yaml'
#GLOB = 'zephyr/samples/basic/minimal/**/sample.yaml'
#GLOB = 'zephyr/samples/basic/**/sample.yaml'
#GLOB = 'nrf/samples/bluetooth/throughput/**/sample.yaml'

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


def process_sample(name: str, sample_dir: Path, output_path: Path):
    print(f'Processing {name} ({sample_dir})...')
    proc = subprocess.run([
            'west', 'ncs-sbom',
            '-d', 'build_sbom_test_cmp',
            '--license-detectors', 'external-file',
            '--output-cache-database', output_path.with_suffix('.json'),
            '--output-html', output_path.with_suffix('.html'),
        ],
        cwd=sample_dir,
        stderr=subprocess.STDOUT,
        stdout=subprocess.PIPE,
        check=False)
    print('Done')
    if proc.returncode != 0:
        raise TestException('error', f'SBOM failed with {proc.returncode}: {proc.stdout.decode()}')


def get_file_list(file: Path) -> 'set[str]':
    return set(json.loads(file.read_text())['files'].keys())


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

    output1 = sample_dir / 'test-1.json'
    output2 = sample_dir / 'test-2.json'

    checkout_branch('test-1')

    start_time = time()
    try:
        process_sample(name, sample_dir, output1)
        test.test1 = TestException('ok ' + str(round((time() - start_time) * 10) / 10) + 's', 'OK').get_result()
        list1 = get_file_list(output1)
    except TestException as ex:
        test.test1 = ex.get_result()
        list1 = set()

    checkout_branch('test-2')

    start_time = time()
    try:
        process_sample(name, sample_dir, output2)
        test.test2 = TestException('ok ' + str(round((time() - start_time) * 10) / 10) + 's', 'OK').get_result()
        list2 = get_file_list(output2)
    except TestException as ex:
        test.test2 = ex.get_result()
        list2 = set()

    if (len(list1) > 0) and (len(list2) > 0):
        only1 = list1.difference(list2)
        only2 = list2.difference(list1)
    else:
        only1 = set()
        only2 = set()

    if len(only2) > 0:
        test.test1.type = test.test1.type.replace('ok', 'miss')
        only2 = list(only2)
        only2.sort()
        list_str = '\n'.join(only2)
        test.test1.message = f'Missing files:\n{list_str}'

    if len(only1) > 0:
        test.test2.type = test.test2.type.replace('ok', 'miss')
        only1 = list(only1)
        only1.sort()
        list_str = '\n'.join(only1)
        test.test2.message = f'Missing files:\n{list_str}'

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
    state_file = ROOT_DIR / 'sbom-test-cmp.pickle'
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
