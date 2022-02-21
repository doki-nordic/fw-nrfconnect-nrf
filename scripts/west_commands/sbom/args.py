#
# Copyright (c) 2019 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause


import argparse
from pathlib import Path

default_report_name = 'sbom_report.html'

command_description = 'Create license report for application'

command_help = '''
Create license report for application
TODO: longer help
'''

detectors_help = '''
spdx-tag
  Search for the SPDX-License-Identifier in the source code or the binary file.
  For guidelines, see:
  https://spdx.github.io/spdx-spec/using-SPDX-short-identifiers-in-source-files

scancode-toolkit
  scancode-toolkit description
'''


class ArgsClass:
    ''' Lists all command line arguments for better type hinting. '''
    _initialized: bool = False
    # arguments added by west
    help: 'bool|None'
    zephyr_base: 'str|None'
    verbose: int
    command: str
    # command arguments
    build_dir: 'list[list[str]]|None'
    input_files: 'list[list[str]]|None'
    input_list_file: 'list[str]|None'
    license_detectors: 'list[str]'
    optional_license_detectors: 'set[str]'
    output_html: 'str|None'
    help_detectors: bool


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


def add_arguments(parser: argparse.ArgumentParser):
    parser.add_argument('-d', '--build-dir', nargs='+', action='append',
                        help='Build input directory. You can provide this option more than once.')
    parser.add_argument('--input-files', nargs='+', action='append',
                        help='Input files. You can use globs (?, *, **) to provide more files. '
                             'You can start argument with the exclamation mark to exclude file '
                             'that were already found starting from the last "--input-files".'
                             'You can provide this option more than once.')
    parser.add_argument('--input-list-file', action='append',
                        help='Reads list of files from a file. Works the same as "--input-files". '
                             'with arguments from each line of the file.'
                             'You can provide this option more than once.')
    parser.add_argument('--license-detectors', default='spdx-tag',
                        help='Comma separated list of enabled license detectors.')
    parser.add_argument('--optional-license-detectors', default='', # TODO: default scancode-toolkit
                        help='Comma separated list of optional license detectors. Optional license '
                             'detector is skipped if any of the previous detectors has already '
                             'detected any license.')
    parser.add_argument('--output-html', default='',
                        help='Gererage output HTML report.')
    parser.add_argument('--help-detectors', action='store_true',
                        help='Show help for each available detector and exit.')


def copy_arguments(source):
    global args
    for name in source.__dict__:
        args.__dict__[name] = source.__dict__[name]
    args._initialized = True


def init_args(allowed_detectors: dict):
    '''Parse, validate and postprocess arguments'''
    global args, command_description

    if not args._initialized:
        # Parse command line arguments if running outside west
        parser = argparse.ArgumentParser(description=command_description,
                                        formatter_class=argparse.ArgumentDefaultsHelpFormatter)
        add_arguments(parser)
        copy_arguments(parser.parse_args())

    if args.help_detectors:
        print(detectors_help)
        exit()

    # Validate and postprocess arguments
    args.license_detectors = split_detectors_list(allowed_detectors, args.license_detectors)
    args.optional_license_detectors = set(split_detectors_list(allowed_detectors,
                                                               args.optional_license_detectors))

    if args.output_html == '':
        if args.build_dir != None:
            args.output_html = Path(args.build_dir[0][0]) / default_report_name
        else:
            args.output_html = None


args: 'ArgsClass' = ArgsClass()
