#
# Copyright (c) 2022 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

'''
Implementation of a detector based on an external tool - scancode-toolkit.
For more details see: https://scancode-toolkit.readthedocs.io/en/stable/
'''

import json
import multiprocessing
from threading import Thread
from tempfile import NamedTemporaryFile
from queue import Queue
from west import log
from data_structure import Data, FileInfo
from args import args
from common import command_execute


class _FileQueue(Queue):
    SENTINEL = object()

    def close(self):
        self.put(self.SENTINEL)

    def __iter__(self):
        while True:
            item = self.get()
            try:
                if item is self.SENTINEL:
                    return
                yield item
            finally:
                self.task_done()


class _Worker(Thread):
    def __init__(self, func, in_queue):
        super().__init__()
        self.func = func
        self.in_queue = in_queue

    def run(self):
        for item in self.in_queue:
            self.func(item)


def _cpu_count():
    '''Retrunt the cpu count'''
    try:
        count = multiprocessing.cpu_count()
    except:
        count = 1
    return count


def _run_scancode(file: FileInfo):
    with NamedTemporaryFile() as output_file:
        command_execute('scancode', '-cl',
                        '--json', output_file.name,
                        '--license-text',
                        '--license-text-diagnostics',
                        '--quiet',
                        file.file_path, allow_stderr=True)
        output_file.seek(0)
        result = json.loads(output_file.read())
        licenses = set()
        for i in result['files'][0]['licenses']:
            if i['key'] != 'unknown-spdx':
                licenses.add(i['key'])
            else:
                log.wrn(f'Unknown spdx tag, file: {file.file_path}')
        file.licenses = file.licenses.union(licenses)
        file.detectors.add('scancode_toolkit')


def detect(data: Data, optional: bool):
    '''License detection using scancode-toolkit.'''
    file_queue: FileInfo = _FileQueue()

    for file in data.files:
        if optional and file.licenses:
            continue
        file_queue.put(file)
    cores = args.processes if args.processes > 0 else _cpu_count()
    threads = [_Worker(_run_scancode, file_queue) for _ in range(cores)]
    for thread in threads:
        thread.start()

    for _ in threads:
        file_queue.close()
    file_queue.join()

    for thread in threads:
        thread.join()
