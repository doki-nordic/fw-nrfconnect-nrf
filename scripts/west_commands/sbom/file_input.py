#
# Copyright (c) 2019 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

from args import args

from pathlib import Path
from data_structure import Data, FileInfo


def resolve_globs(path: Path, globs: 'list[str]') -> 'set(Path)':
    result = set()
    for glob in globs:
        if glob.startswith('!'):
            for file in path.glob(glob[1:]):
                result.discard(file)
        else:
            for file in path.glob(glob):
                if file.is_file():
                    result.add(file)
    return result


def generate_input(data: Data):
    full_set = set()

    if args.input_files is not None:
        cwd = Path('.').resolve()
        for globs in args.input_files:
            data.inputs.append(f'files: {", ".join(globs)} (relative to {cwd})')
            r = resolve_globs(cwd, globs)
            full_set = full_set.union(r)

    if args.input_list_file is not None:
        for file in args.input_list_file:
            file_path = Path(file).resolve()
            data.inputs.append(f'list of files read from: {file_path}')
            globs = list()
            with open(file_path, 'r') as fd:
                for line in fd:
                    line = line.strip()
                    if line == '' or line[0] == '#':
                        continue
                    globs.append(line)
            r = resolve_globs(file_path.parent, globs)
            full_set = full_set.union(r)

    for file_path in full_set:
        file = FileInfo()
        file.file_path = file_path
        data.files.append(file)
