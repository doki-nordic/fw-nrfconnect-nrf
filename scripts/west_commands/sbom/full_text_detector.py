#
# Copyright (c) 2019 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

import re
from data_structure import Data
from license_utils import get_license_texts

COMMENTS_STRIP_RE = re.compile(r'^\s*(?:\/?\*|\/\/|#)?\s*(.*?)\s*(?:\*\/?|\/\/|#)?\s*$')
WHITESPACE_COLLAPSE_RE = re.compile(r'\s+')


normalized_texts: 'dict(str)' = dict()
detector_patterns: 'list(tuple(re.Pattern, str))' = list()


def normalize_text(text: str, strip_comments: bool = False):
    if strip_comments:
        out = ''
        for line in text.split('\n'):
            out += COMMENTS_STRIP_RE.sub(r'\1', line) + '\n'
        text = out
    return WHITESPACE_COLLAPSE_RE.sub(' ', text).strip()


def init():
    if len(normalized_texts) == 0:
        for license in get_license_texts():
            if license.detector is None:
                normalized_texts[normalize_text(license.text)] = license.id
            else:
                pattern = ''
                for part in license.detector.split('</regex>'):
                    plain, *regex = part.split('<regex>') + ['']
                    pattern += re.escape(normalize_text(plain)) + ''.join(regex)
                detector_patterns.append((re.compile(pattern), license.id))


def detect(data: Data, optional: bool):
    init()
    for file in data.files:
        if optional and len(file.licenses) > 0:
            continue
        try:
            with open(file.file_path, 'r', encoding='8859') as fd:
                content = fd.read()
        except:
            file.errors.append('Cannot read file contents')
            # TODO: log details
            continue
        results = set()
        content = normalize_text(content, True)
        for text, id in normalized_texts.items():
            pos = content.find(text)
            if pos >= 0:
                results.add(id.upper())
        for pattern, id in detector_patterns:
            if pattern.search(content) is not None:
                results.add(id.upper())
        if len(results) > 0:
            file.licenses = file.licenses.union(results)
            file.detectors.add('full-text')
