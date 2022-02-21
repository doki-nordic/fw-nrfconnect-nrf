#
# Copyright (c) 2019 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

from enum import Enum
from os import unlink
import os
import re
import subprocess
from tempfile import mktemp
from args import args

from pathlib import Path
from data_structure import Data, FileInfo


default_target = 'zephyr/zephyr.elf'


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
        deps_file_name = self.tool_execute('ninja -t deps')
        self.parse_deps_file(deps_file_name)


    def parse_deps_file(self, deps_file_name): # TODO: check if deps are valid
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
                if m is not None:
                    continue
                raise Exception(f'{deps_file_name}:{line_no}: Cannot parse dependency file')


    def tool_execute(self, command: str, return_str: bool=False) -> 'Path|str':
        ninja_out_name = mktemp('.txt', 'licgen_stdout_')
        with open(ninja_out_name, 'w') as ninja_out_fd:
            ninja_err_name = mktemp('.txt', 'licgen_stderr_')
            with open(ninja_err_name, 'w') as ninja_err_fd:
                try:
                    cp = subprocess.run(command, shell=True, stdout=ninja_out_fd,
                                        stderr=ninja_err_fd, cwd=self.build_dir)
                except Exception as e:
                    # TODO: log details
                    raise # TODO: raise out wxception that can be shown to user
        with open(ninja_err_name, 'r') as ninja_err_fd:
            err = ninja_err_fd.read().strip()
            if len(err) > 0:
                # TODO: log err
                if cp.returncode == 0:
                    raise Exception(f'"{command}" command reported some errors.') #TODO: our exception
        unlink(ninja_err_name)
        if cp.returncode != 0:
            #TODO: our exception
            raise Exception(f'"{command}" command exited with error code {cp.returncode}')
        if return_str:
            with open(ninja_out_name, 'r') as fd:
                return fd.read()
        else:
            return Path(ninja_out_name)


    def query_inputs(self, target: str) -> 'tuple[set[str], set[str], set[str], bool]':
        lines = self.tool_execute(f'ninja -t query {target}', True)
        lines = lines.split('\n')
        lines = tuple(filter(lambda line: len(line.strip()) > 0, lines))
        ln = 0
        explicit = set()
        implicit = set()
        order_only = set()
        phony = False
        while ln < len(lines):
            assert(re.fullmatch(r'\S.*:', lines[ln]) != None)
            ln += 1
            while ln < len(lines):
                m = re.fullmatch(r'(\s*)(.*):(.*)', lines[ln])
                assert(m != None)
                if m.group(1) == '':
                    break
                ln += 1
                ind1 = len(m.group(1))
                dir = m.group(2)
                phony = phony or (re.search(r'(\s|^)phony(\s|$)', m.group(3)) != None)
                if dir == 'input':
                    inputs = True
                elif dir == 'outputs':
                    inputs = False
                else:
                    #TODO our exception
                    raise Exception(f'Cannot parse output from "ninja -t query {target}" command!')
                while ln < len(lines):
                    m = re.fullmatch(r'(\s*)(\|?\|?)\s*(.*)', lines[ln])
                    assert(m != None)
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


    def query_inputs_recursive(self, target: str, done: set=set(), inputs_tuple=None) -> 'set[str]':
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
                if phony:
                    sub_result = self.query_inputs_recursive(input, done, sub_inputs_tuple)
                    result = result.union(sub_result)
                else:
                    #TODO: our exception
                    raise Exception(f'Non-phony target {input} does not exist on disk.')
        return result


    def verify_elf_inputs_in_map_file(self, map_file: Path, elf_inputs: 'set[str]'):
        #TODO: read map file and check if all inputs provided by a map file are available in elf_inputs
        pass


    def verify_archive_inputs(self, archive_path, inputs):
        arch_files = self.tool_execute(f'ar -t "{archive_path}"', True) #TODO: implement 'ar' tool detection or input parameter
        arch_files = arch_files.split('\n')
        arch_files = (f.strip().replace('/', os.sep).replace('\\', os.sep).strip(os.sep) for f in arch_files)
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
            return { archive_path }
        leafs = set()
        for input in archive_inputs:
            input_path = self.build_dir / input
            input_type = detect_file_type(input_path)
            if input_type == FileType.OTHER:
                leafs.add(input_path)
            elif input_type == FileType.OBJ:
                leafs = leafs.union(self.process_obj(input_path, input))
            else:
                raise Exception(f'One library depends on another') #TODO our exception
        return leafs


    def process_obj(self, input_path, input):
        if input not in self.deps:
            return { input_path }
        deps = self.deps[input]
        deps = deps.union(self.query_inputs_recursive(input))
        return set(self.build_dir / x for x in deps)


    def generate_from_target(self, target: str):
        if target.find(':') >= 0:
            pos = target.find(':')
            map_file = self.build_dir / target[(pos + 1):]
            target = target[:pos]
        else:
            map_file = (self.build_dir / target).with_suffix('.map')

        if not map_file.exists():
            # TODO: our exception
            raise Exception(f'Cannot find map file for "{target}" '
                            f'in build directory "{self.build_dir}". '
                            f'Expected location "{map_file}".')

        self.data.inputs.append(f'{target} from build directory {self.build_dir.resolve()}')
        elf_inputs = self.query_inputs_recursive(target)
        self.verify_elf_inputs_in_map_file(map_file, elf_inputs)
        leafs = set()
        for input in elf_inputs:
            input_path = self.build_dir / input
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


def generate_input(data: Data):
    if args.build_dir is not None:
        for build_dir, *targets in args.build_dir:
            if len(targets) == 0:
                targets = [ default_target ]
            b = InputBuild(data, build_dir)
            for target in targets:
                b.generate_from_target(target)
