#
# Copyright (c) 2022 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

'''
Get input files from an application build directory.
'''

import os
import re
from enum import Enum
from pathlib import Path
from west import log
from args import args
from data_structure import Data, FileInfo
from common import SbomException, command_execute


DEFAULT_TARGET = 'zephyr/zephyr.elf'


class FileType(Enum):
    ARCHIVE = 'archive'
    OBJ = 'obj'
    OTHER = 'other'


def detect_file_type(file: Path) -> FileType:
    with open(file, 'r', encoding='8859') as fd:
        header = fd.read(16)
    if header.startswith('!<arch>\n'):
        return FileType.ARCHIVE
    elif header.startswith('\x7FELF'):
        return FileType.OBJ
    else:
        return FileType.OTHER


class InputBuild:

    data: Data
    build_dir: Path
    deps: 'dict[set[str]]'


    def __init__(self, data: Data, build_dir: Path):
        self.data = data
        self.build_dir = Path(build_dir)
        deps_file_name = command_execute('ninja', '-t', 'deps', cwd=self.build_dir,
                                         return_path=True)
        self.parse_deps_file(deps_file_name)


    def parse_deps_file(self, deps_file_name):
        self.deps = dict()
        TARGET_LINE_RE = re.compile(r'([^\s]+)\s*:\s*(#.*)?')
        DEP_LINE_RE = re.compile(r'\s+(.*?)\s*(#.*)?')
        EMPTY_LINE_RE = re.compile(r'\s*(#.*)?')
        with open(deps_file_name, 'r') as fd:
            line_no = 0
            while True:
                line = fd.readline()
                line_no += 1
                if len(line) == 0:
                    break
                line = line.rstrip()
                m = TARGET_LINE_RE.fullmatch(line)
                if m is not None:
                    dep = set()
                    self.deps[m.group(1)] = dep
                    continue
                m = DEP_LINE_RE.fullmatch(line)
                if m is not None:
                    dep.add(m.group(1))
                    continue
                m = EMPTY_LINE_RE.fullmatch(line)
                assert m is not None


    def query_inputs(self, target: str) -> 'tuple[set[str], set[str], set[str], bool]':
        lines = command_execute('ninja', '-t', 'query', target, cwd=self.build_dir)
        lines = lines.split('\n')
        lines = tuple(filter(lambda line: len(line.strip()) > 0, lines))
        ln = 0
        explicit = set()
        implicit = set()
        order_only = set()
        phony = False
        while ln < len(lines):
            assert re.fullmatch(r'\S.*:', lines[ln]) is not None
            ln += 1
            while ln < len(lines):
                m = re.fullmatch(r'(\s*)(.*):(.*)', lines[ln])
                assert m is not None
                if m.group(1) == '':
                    break
                ln += 1
                ind1 = len(m.group(1))
                dir = m.group(2)
                phony = phony or (re.search(r'(\s|^)phony(\s|$)', m.group(3)) is not None)
                if dir == 'input':
                    inputs = True
                else:
                    assert dir == 'outputs'
                    inputs = False
                while ln < len(lines):
                    m = re.fullmatch(r'(\s*)(\|?\|?)\s*(.*)', lines[ln])
                    assert m is not None
                    if len(m.group(1)) <= ind1:
                        break
                    ln += 1
                    target = str(m.group(3))
                    if inputs:
                        if m.group(2) == '':
                            explicit.add(target)
                        elif m.group(2) == '|':
                            implicit.add(target)
                        else:
                            order_only.add(target)
        return (explicit, implicit, order_only, phony)


    def query_inputs_recursive(self, target: str, done: set = set(),
                               inputs_tuple=None) -> 'set[str]':
        if inputs_tuple is None:
            explicit, implicit, _, _ = self.query_inputs(target)
        else:
            explicit, implicit, _, _ = inputs_tuple
        inputs = explicit.union(implicit)
        result = set()
        for input in inputs:
            file_path = (self.build_dir / input).resolve()
            if input in done:
                continue
            done.add(input)
            if file_path.exists():
                result.add(input)
            else:
                sub_inputs_tuple = self.query_inputs(input)
                phony = sub_inputs_tuple[3]
                assert phony
                sub_result = self.query_inputs_recursive(input, done, sub_inputs_tuple)
                result = result.union(sub_result)
        return result


    def verify_elf_inputs_in_map_file(self, map_file: Path, elf_inputs: 'set[str]'):
        # TODO: read map file and check if all inputs provided by a map file are in elf_inputs
        pass


    @staticmethod
    def verify_archive_inputs(archive_path, inputs):
        arch_files = command_execute(args.ar, '-t', archive_path)
        arch_files = arch_files.split('\n')
        arch_files = (f.strip().replace('/', os.sep).replace('\\', os.sep).strip(os.sep)
                      for f in arch_files)
        arch_files = filter(lambda file: len(file.strip()) > 0, arch_files)
        for arch_file in arch_files:
            for input in inputs:
                if input == arch_file:
                    break
                if input.endswith(os.sep + arch_file):
                    break
            else:
                return False
        return True


    def process_archive(self, archive_path, archive_target):
        archive_inputs = self.query_inputs_recursive(archive_target)
        if not self.verify_archive_inputs(archive_path, archive_inputs):
            return {archive_path}
        leafs = set()
        for input in archive_inputs:
            input_path = (self.build_dir / input).resolve()
            input_type = detect_file_type(input_path)
            if input_type == FileType.OTHER:
                leafs.add(input_path)
            else:
                assert input_type == FileType.OBJ
                leafs = leafs.union(self.process_obj(input_path, input))
        return leafs


    def process_obj(self, input_path, input):
        if input not in self.deps:
            return {input_path}
        deps = self.deps[input]
        deps = deps.union(self.query_inputs_recursive(input))
        return set((self.build_dir / x).resolve() for x in deps)


    def generate_from_target(self, target: str):
        if target.find(':') >= 0:
            pos = target.find(':')
            map_file = self.build_dir / target[(pos + 1):]
            target = target[:pos]
        else:
            map_file = (self.build_dir / target).with_suffix('.map')

        if not map_file.exists():
            raise SbomException(f'Cannot find map file for "{target}" '
                                f'in build directory "{self.build_dir}". '
                                f'Expected location "{map_file}".')
        log.dbg(f'Map file: {map_file}')

        self.data.inputs.append(f'{target} from build directory {self.build_dir.resolve()}')
        elf_inputs = self.query_inputs_recursive(target)
        self.verify_elf_inputs_in_map_file(map_file, elf_inputs)
        leafs = set()
        for input in elf_inputs:
            input_path = (self.build_dir / input).resolve()
            input_type = detect_file_type(input_path)
            if input_type == FileType.ARCHIVE:
                leafs = leafs.union(self.process_archive(input_path, input))
            elif input_type == FileType.OBJ:
                leafs = leafs.union(self.process_obj(input_path, input))
            else:
                leafs.add(input_path)
        for leaf in leafs:
            file = FileInfo()
            file.file_path = leaf
            self.data.files.append(file)


def check_gnu_ar_command():
    '''
    Checks if "ar --version" works correctly. If not, raises exception with information
    for user.
    '''
    try:
        if args.ar is not None:
            command_execute(args.ar, '--version', allow_stderr=True)
            return
    except Exception as ex:
        raise SbomException(f'Cannot execute command "{args.ar}".\n'
            f'Make sure that you have PATH pointing to that file.') from ex
    other_names = ['arm-zephyr-eabi-ar', 'arm-none-eabi-ar', 'ar']
    for name in other_names:
        try:
            command_execute(name, '--version', allow_stderr=True)
            args.ar = name
            return
        except Exception:
            pass
    if args.ar is None:
        raise SbomException(f'Cannot execute command "ar".\n'
            f'Make sure that you have PATH pointing to that file.\n'
            f'You can user "--ar=path/to/ar" to pass specific path to "ar".')
    log.dbg(f'"ar" command detected: {args.ar}')


def generate_input(data: Data):
    if args.build_dir is not None:
        log.wrn('Fetching input files from a build directory is experimental for now.')
        check_gnu_ar_command()
        for build_dir, *targets in args.build_dir:
            if len(targets) == 0:
                targets = [DEFAULT_TARGET]
            log.dbg(f'INPUT: build directory: {build_dir}, targets: {targets}')
            b = InputBuild(data, build_dir)
            for target in targets:
                b.generate_from_target(target)
