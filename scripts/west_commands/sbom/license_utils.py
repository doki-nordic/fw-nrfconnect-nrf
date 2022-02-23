#
# Copyright (c) 2019 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

from pathlib import Path
import re
import yaml

from data_structure import DataBaseClass, License, LicenseExpr


spdx_licenses: 'dict(License)' = dict()
license_texts: 'list(License)' = list()
license_by_id: 'dict(License)' = dict()


def is_spdx_license(id: str):
    return id in spdx_licenses


def get_license(id: str) -> 'License|None':
    if id in license_by_id:
        return license_by_id[id]
    return None


def get_license_texts() -> 'list[License]':
    return license_texts


LICENSE_EXPR_RE = re.compile(r'\s*([a-z0-9\.\-\:]+|\+|\)|\()\s*', re.IGNORECASE)


def tokenize_license_expr(expr: str) -> 'list[str]|None':
    result = list()
    expr = expr.strip()
    while len(expr) > 0:
        m = LICENSE_EXPR_RE.match(expr)
        if m is None:
            return None
        result.append(m.group(1))
        expr = expr[m.end():]
    return result


class SPDXLicenseExprInfo(DataBaseClass):
    expr: str
    valid: bool
    is_id_only: bool = False
    licenses: 'set[str]' = set()
    or_present: bool = False


def get_spdx_license_expr_info(expr: str) -> SPDXLicenseExprInfo:
    result = SPDXLicenseExprInfo()
    result.expr = expr.strip()
    tokens = tokenize_license_expr(expr)
    result.valid = tokens is not None
    if not result.valid:
        return result
    ignore_next = False
    if len(tokens) == 0:
        result.licenses.add('')
        result.is_id_only = True
        return result
    for token in tokens:
        if token == 'OR':
            result.or_present = True
        elif ignore_next or token in ('WITH', 'AND', '(', ')', '+'):
            pass
        else:
            result.licenses.add(token)
        ignore_next = (token == 'WITH')
    result.is_id_only = len(tokens) == 1 and len(result.licenses) == 1
    return result


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
