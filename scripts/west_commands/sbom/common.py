#
# Copyright (c) 2019 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause


import json
from pathlib import Path
import subprocess
from tempfile import NamedTemporaryFile
from west import log


class SbomException(Exception):
    pass


def command_execute(*cmd_args: 'tuple[str|Path]', cwd: 'str|Path|None'=None,
                    return_path: bool=False, allow_stderr: bool=False) -> 'Path|str':
    cmd_args = tuple(str(x) for x in cmd_args)
    if cwd is not None:
        cwd = str(cwd)
    out_file = NamedTemporaryFile(delete=False if return_path == True else False)
    err_file = NamedTemporaryFile()
    try:
        cp = subprocess.run(cmd_args, stdout=out_file,
                            stderr=err_file, cwd=cwd)
    except Exception as e:
        log.err(f'Running command "{cmd_args[0]}" failed!')
        log.err(f'Arguments: { json.dumps(cmd_args) }, cwd: "{cwd}"')
        log.err(f'Details: {e}')
        raise SbomException('Command execution error.')
    out_file.seek(0)
    err_file.seek(0)

    err = err_file.read()
    if len(err.strip()) > 0:
        if allow_stderr:
            log.wrn(f'Command "{cmd_args[0]}" reported errors:\n{err}')
        else:
            log.err(f'Command "{cmd_args[0]}" reported errors:\n{err}')
            if cp.returncode == 0:
                log.err(f'Arguments: { json.dumps(cmd_args) }, cwd: "{cwd}"')
                raise SbomException('Command execution error.')
    err_file.close()

    if cp.returncode != 0:
        log.err(f'Command "{cmd_args[0]}" exited with error code {cp.returncode}')
        log.err(f'Arguments: { json.dumps(cmd_args) }, cwd: "{cwd}"')
        raise SbomException('Command execution error.')
    if return_path:
        return Path(out_file)
    else:
        return out_file.read()
