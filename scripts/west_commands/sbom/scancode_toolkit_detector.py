#
# Copyright (c) 2022 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

'''
Implementation of a detector based on an external tool - scancode-toolkit.
For more details see: https://scancode-toolkit.readthedocs.io/en/stable/
'''

import json
from tempfile import NamedTemporaryFile
from west import log
from data_structure import Data, FileInfo
from args import args
from common import SbomException, command_execute, concurrent_pool_iter


def check_scancode():
    '''Checks if "scancode --version" works correctly. If not, raises exception with information
    for user.'''
    try:
        command_execute(args.scancode, '--version', allow_stderr=True)
    except Exception as ex:
        raise SbomException(f'Cannot execute scancode command "{args.scancode}".\n'
            f'Make sure that you have scancode-toolkit installed.\n'
            f'Pass "--scancode=/path/to/scancode" if the scancode executable is '
            f'not available on PATH.') from ex


def detect_file(file: FileInfo) -> 'set(str)':
    '''Execute scancode and get license identifier from its results.'''
    with NamedTemporaryFile(mode="w+") as output_file:
        command_execute(args.scancode, '-cl',
                        '--json', output_file.name,
                        '--license-text',
                        '--license-text-diagnostics',
                        '--quiet',
                        file.file_path, allow_stderr=True)
        output_file.seek(0)
        result = json.loads(output_file.read())
        licenses = set()
        for i in result['files'][0]['licenses']:
            if i['key'] != 'unknown-spdx':
                licenses.add(i['key'])
            else:
                log.wrn(f'Unknown spdx tag, file: {file.file_path}')
        return licenses


def detect(data: Data, optional: bool):
    '''License detection using scancode-toolkit.'''

    if optional:
        filtered = tuple(filter(lambda file: len(file.licenses) == 0, data.files))
    else:
        filtered = data.files

    if len(filtered) > 0:
        check_scancode()

    for results, file, _ in concurrent_pool_iter(detect_file, filtered):
        if len(results) > 0:
            file.licenses = file.licenses.union(results)
            file.detectors.add('scancode-toolkit')
