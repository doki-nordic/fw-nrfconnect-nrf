#
# Copyright (c) 2019 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause


from data_structure import Data, License
from license_utils import get_license, is_spdx_license


def pre_process(data: Data):
     # TODO: Supply additional data needed by the generators, e.g. group files by license, sort files
    data.files.sort(key=lambda f: f.file_path)
    for file in data.files:
        licenses = file.licenses if len(file.licenses) > 0 else [ '' ]
        for license in licenses:
            if license not in data.files_by_license:
                data.files_by_license[license] = list()
            data.files_by_license[license].append(file)
    def lic_reorder(x: str): #TODO: different order
        if is_spdx_license(x):
            return 'd' + x
        elif x.startswith('LicenseRef-'):
            return 'c' + x
        elif len(x) == 0:
            return 'a' + x
        else:
            return 'b' + x
    data.licenses_sorted = sorted(data.files_by_license.keys(), key=lic_reorder)
    new_licenses = dict()
    for id in data.licenses_sorted:
        if is_spdx_license(id):
            new_licenses[id] = get_license(id)
        elif id in data.licenses:
            new_licenses[id] = data.licenses[id]
        else:
            lic = get_license(id)
            if lic is None:
                lic = License()
                lic.id = id
            new_licenses[id] = lic
    data.licenses = new_licenses