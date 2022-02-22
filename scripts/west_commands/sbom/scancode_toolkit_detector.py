#
# Copyright (c) 2022 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

import subprocess
import json
from data_structure import Data


def detect(data: Data, optional: bool):
    for file in data.files:
        if optional and file.licenses:
            continue
        result = subprocess.run(['scancode', '-cl',
                                '--json-pp', '-',
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
                licenses.add(i['name'])
            file.licenses = file.licenses.union(licenses)
            file.detectors.add('scancode_toolkit')

