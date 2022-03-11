#
# Copyright (c) 2019 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

import re
from data_structure import Data


def detect(data: Data, optional: bool):
    for file in data.files:
        if optional and len(file.licenses) > 0:
            continue
        try:
            with open(file.file_path, 'r', encoding='8859') as fd:
                content = fd.read()
        except:
            file.errors.append('Cannot read file contents')
            #TODO: log details
            continue
        results = set()
        for m in re.finditer(r'(?:^|[^a-zA-Z0-9\-])SPDX-License-Identifier\s*:\s*([a-zA-Z0-9 :\(\)\.\+\-]+)', content):
            id = m.group(1).strip()
            if id != '':
                results.add(id.upper())
        if len(results) > 0:
            file.licenses = file.licenses.union(results)
            file.detectors.add('spdx-tag')

