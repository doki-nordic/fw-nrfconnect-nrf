#
# Copyright (c) 2022 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

'''
Generates report using the Jinja2 templates.
'''

from pathlib import Path
from typing import Any
from jinja2 import Template
from west import log
from data_structure import Data


def data_to_dict(data: Any) -> dict:
    '''Convert object to dict by copying public attributes to a new dictionary.'''
    result = dict()
    for name in dir(data):
        if name.startswith('_'):
            continue
        result[name] = getattr(data, name)
    return result


def generate(data: Data, output_file: 'Path|str', template_file: Path):
    '''Generate output_file from data using template_file.'''
    log.dbg(f'Writing output to "{output_file}" using template "{template_file}"')
    with open(template_file, 'r') as fd:
        template_source = fd.read()
    t = Template(template_source)
    out = t.render(**data_to_dict(data))
    with open(output_file, 'w') as fd:
        fd.write(out)
