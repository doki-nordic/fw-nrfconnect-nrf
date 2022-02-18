#
# Copyright (c) 2019 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause


import spdx_tag_detector
import file_input

from args import args, init_args
from data_structure import Data


detectors = {
    'spdx-tag': spdx_tag_detector.detect,
    #'scancode-toolkit': scancode_toolkit_detector.detect,
}

generators = {
    #'html': 'templates/report.html.jinja'
    #'other': function if output generation is not trivial
}


def generate_from_template(data, output_file, template_file):
    pass #TODO: use jinja


def main():
    init_args(detectors)

    data = Data()

    file_input.generate_input(data)
    #build_input.generate_input(data) # TODO: process application build inputs

    #input_post_process.post_process(data) # TODO: remove/merge duplicates, calculate sha1

    for f in data.files:
        print(f.file_path)

    for detector_name in args.license_detectors:
        func = detectors[detector_name]
        optional = detector_name in args.optional_license_detectors
        func(data, optional)

    #output_pre_process.pre_process(data) # TODO: Supply additional data needed by the generators, e.g. group files by license, sort files

    for generator_name, generator in generators:
        if f'output_{generator_name}' in args.__dict__:
            output_file = args.__dict__[f'output_{generator_name}']
            if type(generator) is str:
                generate_from_template(data, output_file, generator)
            else:
                generator(data, output_file)


if __name__ == '__main__':
    main()
