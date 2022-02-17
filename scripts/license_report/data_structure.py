#
# Copyright (c) 2019 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause


from pathlib import Path


class FileInfo:
    file_path: Path
    licenses: 'set[str]'
    sha1: str

class CustomLicense:
    id: str
    text: str
    name: str

class Data:
    files: 'list[FileInfo]'
    custom_licenses: 'dict[CustomLicense]'
    files_by_license: 'dict[list[FileInfo]]'
