#
# Copyright (c) 2019 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

from pathlib import Path
import yaml

from data_structure import License


spdx_licenses: 'dict(License)'


def is_spdx_license(id: str):
    return id in spdx_licenses


def get_spdx_license(id: str) -> 'License|None':
    if id in spdx_licenses:
        return spdx_licenses[id]
    return None


def load_data():
    global spdx_licenses
    spdx_licenses = dict()
    with open(Path(__file__).parent / 'data/spdx-licenses.yaml', 'r') as fd:
        data = yaml.safe_load(fd)
    for id, value in data.items():
        if id.startswith('_'):
            continue
        lic = License()
        lic.custom = False
        lic.id = id
        lic.name = value['name']
        lic.url = value['url'] if 'url' in value else f'https://spdx.org/licenses/{id}.html'
        spdx_licenses[id] = lic


load_data()
