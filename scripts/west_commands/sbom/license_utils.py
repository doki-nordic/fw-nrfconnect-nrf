#
# Copyright (c) 2019 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

from pathlib import Path
import yaml

from data_structure import License


spdx_licenses: 'dict(License)' = dict()
license_texts: 'list(License)' = list()
license_by_id: 'dict(License)' = dict()


def is_spdx_license(id: str):
    return id in spdx_licenses


def get_license(id: str) -> 'License|None':
    if id in license_by_id:
        return license_by_id[id]
    return None


def get_license_texts() -> 'list(License)':
    return license_texts


def load_data():

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
        license_by_id[id] = lic

    with open(Path(__file__).parent / 'data/license-texts.yaml', 'r') as fd:
        data = yaml.safe_load(fd)
    for value in data:
        lic = License()
        lic.id = value['id']
        lic.custom = not is_spdx_license(lic.id)
        lic.name = value['name'] if 'name' in value else None
        lic.url = value['url'] if 'url' in value else None
        lic.text = value['text']
        license_texts.append(lic)
        if lic.id not in license_by_id:
            license_by_id[lic.id] = lic

load_data()
