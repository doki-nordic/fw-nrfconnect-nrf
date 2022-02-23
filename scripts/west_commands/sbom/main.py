#
# Copyright (c) 2019 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause


import json
from pathlib import Path
import spdx_tag_detector
import full_text_detector
import scancode_toolkit_detector
import file_input
import input_build
import input_post_process
import output_pre_process

from args import args, init_args
from data_structure import Data
import output_template


detectors = {
    'spdx-tag': spdx_tag_detector.detect,
    'full-text': full_text_detector.detect,
    'scancode-toolkit': scancode_toolkit_detector.detect,
}

generators = {
    'html': 'templates/report.html.jinja'
    #'other': function if output generation is not trivial
}


def main():
    init_args(detectors)

    data = Data()

    input_build.generate_input(data)
    file_input.generate_input(data)

    input_post_process.post_process(data)

    for detector_name in args.license_detectors:
        func = detectors[detector_name]
        optional = detector_name in args.optional_license_detectors
        func(data, optional)

    output_pre_process.pre_process(data)

    if 0:
        for f in data.files:
            print(f.file_path)
            for name in dir(f):
                if name == 'file_path' or name.startswith('_'):
                    continue
                value = getattr(f, name)
                print(f'        {name}: {value}')

    for generator_name, generator in generators.items():
        output_file = args.__dict__[f'output_{generator_name}']
        if output_file is None:
            pass # Generator is unused
        elif type(generator) is str:
            output_template.generate(data, output_file, Path(__file__).parent / generator)
        else:
            generator(data, output_file)


if __name__ == '__main__':
    main()
