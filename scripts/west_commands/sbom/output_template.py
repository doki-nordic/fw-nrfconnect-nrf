#
# Copyright (c) 2019 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause


from pathlib import Path
from typing import Any
from jinja2 import Template

from data_structure import Data


def data_to_dict(data: Any) -> dict:
    result = dict()
    for name in dir(data):
        if name.startswith('_'):
            continue
        result[name] = getattr(data, name)
    return result


def generate(data: Data, output_file: 'Path|str', template_file: Path):
    with open(template_file, 'r') as fd:
        template_source = fd.read()
    t = Template(template_source)
    out = t.render(**data_to_dict(data))
    with open(output_file, 'w') as fd:
        fd.write(out)

