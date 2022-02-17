#
# Copyright (c) 2019 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause


import argparse

detectors_help = '''
License detectors:

spdx-tag
  Search for the SPDX-License-Identifier in the source code or the binary file.
  For guidelines, see:
  https://spdx.github.io/spdx-spec/using-SPDX-short-identifiers-in-source-files

scancode-toolkit
  scancode-toolkit description
'''


class ArgsClass:
    ''' Lists all command line arguments for better type hinting. '''
    help: bool
    license_detectors: 'list[str]'
    optional_license_detectors: 'set[str]'
    pass


def split_arg_list(text: str) -> 'list[str]':
    '''Split comma separated list removing whitespaces and empty items'''
    arr = text.split(',')
    arr = [x.strip() for x in arr]
    arr = list(filter(lambda x: len(x) > 0, arr))
    return arr


def split_detectors_list(allowed_detectors: dict, text: str) -> 'list[str]':
    '''Split comma separated list of detectors removing whitespaces, empty items and validating.'''
    arr = split_arg_list(text)
    for name in arr:
        if name not in allowed_detectors:
            raise Exception(f'Detector not found: {name}') #TODO: create our exception class for this tool
    return arr


def init_args(allowed_detectors: dict):
    '''Parse, validate and postprocess arguments'''
    global args

    # Parse command line arguments
    parser = argparse.ArgumentParser(description='Create license report for application',
                                     add_help=False, formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('--license-detectors', default='spdx-tag',
                        help='Comma separated list of enabled license detectors.')
    parser.add_argument('--optional-license-detectors', default='', # TODO: default scancode-toolkit
                        help='Comma separated list of optional license detectors. Optional license '
                             'detector is skipped if any of the previous detectors has already '
                             'detected any license.')
    parser.add_argument('--help', action='store_true',
                        help='Show this help message and exit')
    args = parser.parse_args(namespace=args)

    # Show help with list of detectors
    if args.help:
        parser.print_help()
        print(detectors_help)
        exit()

    # Validate and postprocess arguments
    args.license_detectors = split_detectors_list(allowed_detectors, args.license_detectors)
    args.optional_license_detectors = set(split_detectors_list(allowed_detectors,
                                                               args.optional_license_detectors))


args: 'ArgsClass' = ArgsClass()
