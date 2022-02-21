#
# Copyright (c) 2019 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause


from pathlib import Path


class FileInfo:
    file_path: Path
    licenses: 'set[str]' = set()
    sha1: str
    detectors: 'set[str]' = set()
    errors: 'list[str]' = list()
    warnings: 'list[str]' = list()

class License:
    custom: bool = True
    id: str
    name: 'str|None' = None
    url: 'str|None' = None
    text: 'str|None' = None

class Data:
    files: 'list[FileInfo]' = list()
    licenses: 'dict[License]' = dict()
    files_by_license: 'dict[list[FileInfo]]' = dict()
    licenses_sorted: 'list[str]' = list()
    inputs: 'list[str]' = list()
