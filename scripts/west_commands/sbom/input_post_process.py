#
# Copyright (c) 2019 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause


import hashlib
from data_structure import Data, FileInfo


def remove_duplicates(data: Data):
    def is_not_visited(file: FileInfo):
        if file.file_path in visited:
            return False
        visited.add(file.file_path)
        return True
    visited = set()
    data.files = list(filter(is_not_visited, data.files))


def calculate_hashes(data: Data):
    for file in data.files:
        sha1 = hashlib.sha1()
        try:
            with open(file.file_path, 'rb') as fd:
                while True:
                    data = fd.read(65536)
                    if len(data) == 0 or data is None:
                        break
                    sha1.update(data)
        except:
            file.errors.append('Cannot calculate SHA-1 of the file')
            file.sha1 = ''
            #TODO: log detailed information
            continue
        file.sha1 = sha1.hexdigest()


def post_process(data: Data):
    remove_duplicates(data)
    calculate_hashes(data)
