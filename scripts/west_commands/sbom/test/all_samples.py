

from pathlib import Path
import subprocess
from tempfile import NamedTemporaryFile
import traceback
import yaml

dirs = '''
    nrf/samples
    zephyr/samples
    '''.split()

supported_platforms = '''
    nrf52840dk_nrf52840 nrf52dk_nrf52832 nrf5340dk_nrf5340_cpuapp nrf52840dk_nrf52811
    nrf52840dongle_nrf52840 nrf52dk_nrf52805 nrf52dk_nrf52810 nrf52833dk_nrf52833
    nrf52833dk_nrf52820 nrf9160dk_nrf9160_ns
    '''.split()

yamls = []

for samples_dir in dirs:
    files = (Path(__file__).parents[5] / samples_dir).glob('**/sample.yaml')
    yamls = yamls + list(files)

for yaml_file in yamls:
    sample_dir = yaml_file.parent
    print(f'Sample: {sample_dir}')
    with open(yaml_file, 'r') as fd:
        data = yaml.safe_load(fd)
    try:
        name = data['sample']['name']
    except KeyError:
        name = 'Unnamed'
    print(f'  name: {name}')
    platforms = set()
    try:
        for test_name, test_data in data['tests'].items():
            def collect_platforms(key):
                if key in test_data:
                    value = test_data[key]
                    if isinstance(value, str):
                        value = value.split()
                    return set(value)
                return set()
            platforms.update(collect_platforms('platform_allow'))
            platforms.update(collect_platforms('integration_platforms'))
    except KeyError:
        pass
    board = supported_platforms[0]
    recommended = False
    for p in supported_platforms:
        if p in platforms:
            board = p
            recommended = True
            break
    print(f'  board: {board} {"not recommended" if not recommended else ""}')

    with NamedTemporaryFile(delete=False, mode='w+', prefix='buildlog_', suffix='.txt') as out_file:
        print(f'  build log: {out_file.name}')
        try:
            subprocess.run(['west', 'build', '-d', 'build_sbom_test', '-b', board],
                           cwd=str(sample_dir), stderr=out_file, stdout=out_file, check=True)
            print('  build: OK')
        except Exception as ex:
            out_file.write('\n\n\n\n\n')
            out_file.write(traceback.format_exc())
            print('  build: ERROR')
            continue

    with NamedTemporaryFile(delete=False, mode='w+', prefix='buildlog_', suffix='.txt') as out_file:
        print(f'  ncs-sbom log: {out_file.name}')
        try:
            subprocess.run(['west', 'ncs-sbom', '-d', 'build_sbom_test', '--license-detectors', 'spdx-tag,full-text'],
                           cwd=str(sample_dir), stderr=out_file, stdout=out_file, check=True)
            print('  west ncs-sbom: \x1b[32mOK\x1b[0m')
            print(f'  report: {sample_dir}/build_sbom_test/sbom_report.html')
        except Exception as ex:
            out_file.write('\n\n\n\n\n')
            out_file.write(traceback.format_exc())
            print('\x1b[31m  west ncs-sbom: ERROR\x1b[0m')
            continue
    