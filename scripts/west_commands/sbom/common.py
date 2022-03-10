#
# Copyright (c) 2019 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause


import json
from os import unlink
from pathlib import Path
from west import log
import subprocess
from tempfile import mktemp


class SbomException(Exception):
    pass


def command_execute(*cmd_args: 'tuple[str|Path]', cwd: 'str|Path|None'=None,
                    return_path: bool=False, allow_stderr: bool=False) -> 'Path|str':
    cmd_args = tuple(str(x) for x in cmd_args)
    if cwd is not None:
        cwd = str(cwd)
    ninja_out_name = mktemp('.txt', 'licgen_stdout_')
    with open(ninja_out_name, 'w') as ninja_out_fd:
        ninja_err_name = mktemp('.txt', 'licgen_stderr_')
        with open(ninja_err_name, 'w') as ninja_err_fd:
            try:
                cp = subprocess.run(cmd_args, stdout=ninja_out_fd,
                                    stderr=ninja_err_fd, cwd=cwd)
            except Exception as e:
                log.err(f'Running command "{cmd_args[0]}" failed!')
                log.err(f'Arguments: { json.dumps(cmd_args) }, cwd: "{cwd}"')
                log.err(f'Details: {e}')
                raise SbomException('Command execution error.')
    with open(ninja_err_name, 'r') as ninja_err_fd:
        err = ninja_err_fd.read()
        if len(err.strip()) > 0:
            if allow_stderr:
                log.wrn(f'Command "{cmd_args[0]}" reported errors:\n{err}')
            else:
                log.err(f'Command "{cmd_args[0]}" reported errors:\n{err}')
                if cp.returncode == 0:
                    log.err(f'Arguments: { json.dumps(cmd_args) }, cwd: "{cwd}"')
                    raise SbomException('Command execution error.')
    unlink(ninja_err_name)
    if cp.returncode != 0:
        log.err(f'Command "{cmd_args[0]}" exited with error code {cp.returncode}')
        log.err(f'Arguments: { json.dumps(cmd_args) }, cwd: "{cwd}"')
        raise SbomException('Command execution error.')
    if return_path:
        return Path(ninja_out_name)
    else:
        with open(ninja_out_name, 'r') as fd:
            return fd.read()
