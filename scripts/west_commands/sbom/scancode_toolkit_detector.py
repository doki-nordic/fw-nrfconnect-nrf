#
# Copyright (c) 2022 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

from itertools import count
import subprocess
import json
import multiprocessing
from types import SimpleNamespace
from data_structure import Data
from args import args


def cpu_count():
    '''Retrunt the cpu count'''
    try:
        count = multiprocessing.cpu_count()
    except:
        count = 1
    return count


def detect_cache(data: Data, optional: bool):
    if args.input_scancode_cache == None:
        print('Error input file is missing')
        return
 
    with open(args.input_scancode_cache, 'r') as fd:
        db = json.load(fd, object_hook=lambda d: SimpleNamespace(**d))

    for file in data.files:
        if optional and file.licenses:
            continue
        for f in db.files:
            if f.path == str(file.file_path) and f.sha1 == str(file.sha1):
                file.licenses = file.licenses.union(f.license)
                file.detectors.add('scancode-database')
                continue


def detect(data: Data, optional: bool):
    for file in data.files:
        if optional and file.licenses:
            continue

        result = subprocess.run(['scancode', '-cl',
                                '--json-pp', '-',
                                '--license-text',
                                '--license-text-diagnostics',
                                '-n', str(cpu_count()),
                                file.file_path],
                                capture_output=True,
                                encoding='utf-8')

        try:
            result.check_returncode()
        except:
            file.errors.append(f'scancode_toolkit: {result.stderr}')
        else:
            result_dict = json.loads(result.stdout)
            licenses = set()
            for i in result_dict['files'][0]['licenses']:
                if i['key'] == 'unknown-spdx':
                    licenses.add(i['matched_text'].replace('SPDX-License-Identifier: ', ''))
                else:
                    licenses.add(i['name'])
            file.licenses = file.licenses.union(licenses)
            file.detectors.add('scancode_toolkit')

