#
# Copyright (c) 2019 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause


import spdx_tag_detector

from args import args, init_args


detectors = {
    'spdx-tag': spdx_tag_detector.detect,
    #'scancode-toolkit': scancode_toolkit_detector.detect,
}


def main():
    init_args(detectors)

    data = [] # TODO: get from input

    for detector_name in args.license_detectors:
        func = detectors[detector_name]
        func(data, detector_name in args.optional_license_detectors)

    #TODO: send data to output


if __name__ == '__main__':
    main()
