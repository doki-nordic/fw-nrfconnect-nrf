#
# Copyright (c) 2019 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause


import copy
from pathlib import Path

class DataBaseClass:
    '''Class that do shallow copy of class variables to instance variables.'''
    def __init__(self) -> None:
        for name in tuple(dir(self)):
            if name.startswith('_'):
                continue
            setattr(self, name, copy.copy(getattr(self, name)))

class FileInfo(DataBaseClass):
    file_path: Path
    licenses: 'set[str]' = set()
    license_expr: str
    sha1: str
    detectors: 'set[str]' = set()
    errors: 'list[str]' = list()
    warnings: 'list[str]' = list()

class License(DataBaseClass):
    is_expr: bool = False
    id: str
    custom: bool = True
    name: 'str|None' = None
    url: 'str|None' = None
    text: 'str|None' = None

class LicenseExpr(DataBaseClass):
    is_expr: bool = True
    expr: str
    valid: bool
    custom: bool = True
    licenses: 'list[str]' = list()

class Data(DataBaseClass):
    files: 'list[FileInfo]' = list()
    licenses: 'dict[License|LicenseExpr]' = dict()
    files_by_license: 'dict[list[FileInfo]]' = dict()
    licenses_sorted: 'list[str]' = list()
    inputs: 'list[str]' = list()
