#
# Copyright (c) 2022 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

from concurrent.futures import thread
from itertools import count
import subprocess
import json
import multiprocessing
from threading import Thread
from types import SimpleNamespace
from queue import Queue
from unittest import result
from data_structure import Data, FileInfo
from args import args
from common import SbomException, command_execute

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
            result = self.func(item)


def _cpu_count():
    '''Retrunt the cpu count'''
    try:
        count = multiprocessing.cpu_count()
    except:
        count = 1
    return count


def _run_scancode(file: FileInfo):
    result = command_execute('scancode', '-cl',
                             '--json-pp', '-',
                             '--license-text',
                             '--license-text-diagnostics',
                             file.file_path, allow_stderr=True)
    result_dict = json.loads(result)
    licenses = set()
    for i in result_dict['files'][0]['licenses']:
        if i['key'] != 'unknown-spdx':
            licenses.add(i['key'])
    file.licenses = file.licenses.union(licenses)
    file.detectors.add('scancode_toolkit')


def detect(data: Data, optional: bool):
    file_queue:FileInfo = _FileQueue()

    for file in data.files:
        if optional and file.licenses:
            continue
        file_queue.put(file)
    n = args.processes if args.processes > 0 else _cpu_count()
    threads = [_Worker(_run_scancode, file_queue) for _ in range(n)]
    for thread in threads:
        thread.start()

    for _ in threads:
        file_queue.close()
    file_queue.join()

    for thread in threads:
        thread.join()

