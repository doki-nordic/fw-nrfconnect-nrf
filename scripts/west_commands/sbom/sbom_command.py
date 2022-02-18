# Copyright (c) 2019 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0

'''The "sbom" extension command.'''

import argparse
import args
import main
from textwrap import dedent
from west.commands import WestCommand


class Sbom(WestCommand):

    def __init__(self):
        super().__init__('sbom', args.command_description, args.command_help)

    def do_add_parser(self, parser_adder):
        parser = parser_adder.add_parser(
            self.name, help=self.help,
            formatter_class=argparse.ArgumentDefaultsHelpFormatter,
            description=self.description)
        args.add_arguments(parser)
        return parser

    def do_run(self, arguments, unknown_arguments):
        args.copy_arguments(arguments)
        main.main()
