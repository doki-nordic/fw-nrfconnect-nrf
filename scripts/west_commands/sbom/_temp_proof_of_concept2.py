#
# Copyright (c) 2019 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

import json
import os
from pathlib import Path
import re
import sys
from tokenize import group
from turtle import Turtle
from build_files_extract import tool_execute
from generator_error import GeneratorError

'''

1. get all dependencies with ninja -t deps
2. get inputs of ninja "zephyr/zephyr.elf" target
3. for each input
    if object file:
        find related source file
    if archive:
        get list of contained object files: ar -t
        find ninja targets and dependencies for each file
        if something goes wrong tread archive as a external library
    if other:
        tread as source file
'''


os.chdir('/home/doki/work/ncs2/zephyr/samples/application_development/external_lib/build_nrf52840dk_nrf52840')

build_dir = Path('.').resolve()


def query_inputs(build_dir, target) -> 'tuple[set[str], set[str], set[str], bool]':
    lines = tool_execute(build_dir, f'ninja -t query {target}', True)
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
                raise GeneratorError(f'Cannot parse output from "ninja -t query {target}" command!')
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

def list_archive(file) -> 'set[str]':
    files = tool_execute(build_dir, f'ar -t "{file}"', True)
    files = files.split('\n')
    files = (f.strip().replace('/', os.sep).replace('\\', os.sep).strip(os.sep) for f in files)
    files = filter(lambda file: len(file.strip()) > 0, files)
    return set(files)

def parse_deps_file(build_dir, deps_file_name):
    result = dict()
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
                result[m.group(1)] = dep
                continue
            m = DEP_LINE_RE.fullmatch(line)
            if m is not None:
                dep.add(m.group(1))
                continue
            m = EMPTY_LINE_RE.fullmatch(line)
            if m is not None:
                continue
            raise GeneratorError(f'{deps_file_name}:{line_no}: Cannot parse dependency file')
    return result

deps_file_name = tool_execute(build_dir, 'ninja -t deps')
deps = parse_deps_file(build_dir, deps_file_name)

explicit, implicit, _, _ = query_inputs(build_dir, 'zephyr/zephyr.elf')
inputs = explicit.union(implicit)

objs = dict()
sources = set()

def query_inputs_recursive(build_dir, target, ind='', done=set()):
    explicit, implicit, order_only, _ = query_inputs(build_dir, target)
    inputs = explicit.union(implicit)
    for input in inputs:
        file_path = (build_dir / input).resolve()
        if input in done:
            print(f'{ind}DONE: {input}')
            continue
        done.add(input)
        if not file_path.exists():
            _, _, _, phony = query_inputs(build_dir, input)
            if not phony:
                print(f'{ind}ERROR: {input}')
            else:
                print(f'{ind}{input} phony ->')
                query_inputs_recursive(build_dir, input, ind + '    ', done)
        else:
            explicit, implicit, order_only, _ = query_inputs(build_dir, input)
            if len(explicit) + len(implicit) > 0:
                print(f'{ind}{input} ->')
            else:
                print(f'{ind}{input}')
            query_inputs_recursive(build_dir, input, ind + '    ', done)


query_inputs_recursive(build_dir, 'zephyr/zephyr.elf')

exit()

for input in inputs:
    file_path = (build_dir / input).resolve()
    if not file_path.exists():
        _, _, _, phony = query_inputs(build_dir, input)
        if not phony:
            raise GeneratorError(f'Input {input} does not exists on path {file_path}')
        continue
    with open(file_path, 'r', encoding='8859') as fd:
        header = fd.read(16)
    if header.startswith('!<arch>\n'):
        try:
            arch_files = list_archive(file_path)
            explicit, implicit, order_only, _ = query_inputs(build_dir, input)
            arch_inputs = explicit.union(implicit, order_only)
            arch_objs = dict()
            for arch_input in arch_inputs:
                arch_input_path = build_dir / arch_input
                matched = False
                if arch_input_path.name in arch_files:
                    arch_files.remove(arch_input_path.name)
                    matched = True
                else:
                    for arch_file in arch_files:
                        if arch_input.endswith(os.sep + arch_file):
                            arch_files.remove(arch_file)
                            matched = True
                            break
                if not matched:
                    continue
                if arch_input not in deps:
                    raise 0
                for dep in deps[arch_input]:
                    sources.add(str(build_dir / dep))
            if len(arch_files) > 0:
                raise 0
        except:
            sources.add(str(file_path))
    elif header.startswith('\x7FELF'):
        objs[str(file_path)] = input
    else:
        sources.add(str(file_path))

for name, dep in deps.items():
    print(f'{name} -> {" ".join(dep)}')

print('------------ OBJS')
print('\n'.join([f'{x[1]}   ->  {x[0]}' for x in objs.items()]))
print('------------ SOURCES')
sources = list(sources)
sources.sort()
print('\n'.join(sources))


#print(json.dumps([list(inputs), list(outputs), phony], indent=1))
