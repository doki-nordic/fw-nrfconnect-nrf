#!/usr/bin/env python3

import io
import os
import sys
import math
import shutil
import random
import signal
import struct
import binascii
import platform
import argparse
import tempfile
import threading
import traceback
import subprocess
from time import sleep
from enum import IntEnum
from pathlib import Path
from textwrap import dedent
from types import SimpleNamespace


# Update major, minor or patch version for each change in this file.
VERSION = '1.0.0'


#region Extcap Plugin configuration


INTERFACE_NAME = 'bt_hci_rtt'
DISPLAY_NAME = 'Bluetooth HCI monitor packets over RTT'
JLINK_DEVICES_URL = 'https://www.segger.com/supported-devices/nordic-semi/'

EXTCAP_INTERFACES = dedent('''
    extcap {version=''' + VERSION + '''}{help=''' + JLINK_DEVICES_URL +'''}
    interface {value=''' + INTERFACE_NAME + '''}{display=''' + DISPLAY_NAME + '''}
    ''').strip()

EXTCAP_DLTS = dedent('''
    dlt {number=254}{name=DLT_BLUETOOTH_LINUX_MONITOR}{display=Bluetooth Linux Monitor}
    ''').strip()

EXTCAP_CONFIG = dedent('''
    arg {number=0}{call=--device}{display=Device}{tooltip=Device name - press Help for full list}{type=string}{required=false}{group=Main}
    arg {number=1}{call=--iface}{display=Interface}{tooltip=Target interface}{type=selector}{required=false}{group=Main}
    arg {number=2}{call=--speed}{display=Speed (kHz)}{tooltip=Target speed}{type=integer}{range=5,50000}{default=4000}{required=false}{group=Main}
    arg {number=5}{call=--channel}{display=RTT Channel}{tooltip=RTT channel that monitor uses}{type=integer}{range=1,99}{default=1}{required=false}{group=Main}
    arg {number=3}{call=--snr}{display=Serial Number}{tooltip=Fill if you have more devices connected}{type=string}{required=false}{group=Optional}
    arg {number=4}{call=--addr}{display=RTT Address}{tooltip=Single address or ranges <Rangestart> <RangeSize>[, <Range1Start> <Range1Size>, ...]}{type=string}{required=false}{group=Optional}
    arg {number=6}{call=--logger}{display=JLinkRTTLogger Executable}{tooltip=Select your executable if you do not have in your PATH}{type=fileselect}{mustexist=true}{group=Optional}
    arg {number=7}{call=--debug}{display=Debug output}{tooltip=This is only for debuging this extcap plugin}{type=fileselect}{mustexist=false}{group=Debug}
    arg {number=8}{call=--debug-logger}{display=JLinkRTTLogger stdout}{tooltip=File that will contain standard output from JLinkRTTLogger}{type=fileselect}{mustexist=false}{group=Debug}
    value {arg=1}{value=SWD}{display=SWD}{default=true}
    value {arg=1}{value=JTAG}{display=JTAG}{default=false}
    value {arg=1}{value=cJTAG}{display=cJTAG}{default=false}
    value {arg=1}{value=FINE}{display=FINE}{default=false}
    ''').strip()

#TODO: Automatic list of Nordic devices:
# echo expdevlist /tmp/j-link-dev-list.csv > a.jlink
# echo exit >> a.jlink
# JLinkExe -CommandFile a.jlink
# Load j-link-dev-list.csv and filter and put into Device config
# If JLinkExe cannot be found automatially, show text box as it is now,
# but text box and select box should have different ids (or param?) to avoid problems
# during conf saving and loading by Wireshark.

#endregion


#region Help message


def print_help_message():
    help = dedent(f'''
        This is an extcap plugin for Wireshark. It allows you to capture HCI packets
        over Segger J-Link RTT. Place the script in appropriate plugins directory.
        In Wireshark, see: Help => About Wireshark => Folders => Personal Extcap path.

        You need SEGGER J-Link software and drivers installed to use this plugin.
        It requires JLinkRTTLogger{ ".exe" if is_windows else "" } executable.
        ''')
    if is_windows:
        help += dedent('''
            In Windows, you have to run this script once after placing it the plugins
            directory. It will setup the environment and download necessary modules.
            To force setup, use "--initial-setup" option.
            ''')
    else:
        stat = os.stat(__file__)
        if (stat.st_mode & 0o100) == 0:
            try:
                os.chmod(__file__, stat.st_mode | 0o100)
            except:
                help = help.strip() + dedent(f'''
                    Make sure this file has an execute permissions, e.g.:
                    "chmod +x {Path(__file__).name}"
                    ''')
    print(help)


#endregion


#region Arguments parser


class Args(SimpleNamespace):
    extcap_interfaces: bool
    extcap_dlts: bool
    extcap_config: bool
    capture: bool
    device: str
    iface: str
    speed: str
    channel: str
    snr: str
    addr: str
    logger: str
    debug: str
    debug_logger: str
    fifo: str
    initial_setup: bool
    pip_install: bool
    def __init__(self):
        # Arguments parsing
        parser = argparse.ArgumentParser(allow_abbrev=False, argument_default='', add_help=False)
        # Main commands
        parser.add_argument('--extcap-interfaces', action='store_true')
        parser.add_argument('--extcap-dlts', action='store_true')
        parser.add_argument('--extcap-config', action='store_true')
        parser.add_argument('--capture', action='store_true')
        # TODO: --install option that:
            # 1) check whether plugin already installed and which version is it, prompt before overriding,
            # 2) copy script file to extcap dir,
            # 3) run --initial-setup on Windows,
            # 4) lists interfaces to check if it is correctly installed
            # 5) check if 'JLinkRTTLogger -?' works, if not, inform user that he needs JLinkRTTLogger and he have to configure "JLinkRTTLoger Executable" field later.
        # Configuration
        parser.add_argument('--device')
        parser.add_argument('--iface', default='SWD')
        parser.add_argument('--speed', default='4000')
        parser.add_argument('--channel', default='1')
        parser.add_argument('--snr')
        parser.add_argument('--addr')
        parser.add_argument('--logger')
        parser.add_argument('--debug')
        parser.add_argument('--debug-logger')
        # Capture options
        parser.add_argument('--fifo')
        # Windows-only flag
        parser.add_argument('--initial-setup', action='store_true')
        parser.add_argument('--pip-install', action='store_true')
        super(Args, self).__init__(**parser.parse_known_args()[0].__dict__)

args = Args()


#endregion


#region Debug logging


if args.debug:
    debug_file = open(args.debug, 'w')
    def log(*pargs, **kwargs):
        print(*pargs, **kwargs, file=debug_file)
        debug_file.flush()
    def debug_log_close():
        try:
            debug_file.close()
        except:
            pass
    log('Raw arguments:', sys.argv)
    log('Parsed arguments:', args.__dict__)
else:
    def log(*_, **__):
        pass
    def debug_log_close():
        pass


#endregion


#region Watchdog - asynchronous periodic function callbacks


class Watchdog:

    TICK_TIME = 0.5

    @staticmethod
    def init(exception_handler):
        Watchdog._exception_handler = exception_handler
        Watchdog._running = True
        Watchdog._funcs = set()
        Watchdog._lock = threading.Lock()
        Watchdog._thread = threading.Thread(target=Watchdog._watchdog_entry)
        Watchdog._thread.start()

    @staticmethod
    def _watchdog_entry():
        log('Watchdog thread started')
        try:
            with Watchdog._lock:
                while Watchdog._running:
                    for func in (list(Watchdog._funcs) or []):
                        if func() is False:
                            Watchdog._funcs.discard(func)
                        if not Watchdog._running:
                            break
                    if not Watchdog._running:
                        break
                    Watchdog._lock.release()
                    try:
                        sleep(Watchdog.TICK_TIME)
                    finally:
                        Watchdog._lock.acquire()
        except BaseException as ex:
            Watchdog._exception_handler(ex)
        log('Watchdog thread stopped')

    @staticmethod
    def add(function):
        with Watchdog._lock:
            Watchdog._funcs.add(function)

    @staticmethod
    def remove(function):
        with Watchdog._lock:
            Watchdog._funcs.discard(function)

    @staticmethod
    def stop():
        Watchdog._running = False
        if (Watchdog._thread is not None) and  (threading.current_thread().native_id != Watchdog._thread.native_id):
            Watchdog._thread.join()
        Watchdog._thread = None


#endregion


#region Process control utilities


class SignalReason(IntEnum):
    UNKNOWN = 0
    CAPTURE_STOP = 1
    OUTPUT_PIPE = 2
    INPUT_PIPE = 3
    RTT_PROCESS_EXIT = 4


class SignalTerminated(Exception):
    def __init__(self, signal_reason: int):
        super().__init__()
        log('New SignalTerminated, reason', signal_reason)
        self.signal_reason = signal_reason


class Process:

    handler_enabled = False
    signal_reason = SignalReason.UNKNOWN
    exit_code = 0

    @staticmethod
    def setup_signals(default_reason):
        Process.signal_reason = default_reason
        Process.handler_enabled = True
        signal.signal(signal.SIGTERM, Process._signal_handler)

    @staticmethod
    def _signal_handler(_, __):
        if Process.handler_enabled:
            log(f'Signal handler: raising signal with reason:', Process.signal_reason)
            raise SignalTerminated(Process.signal_reason)
        else:
            log('Signal handler ignored')

    @staticmethod
    def disable_signals():
        Process.handler_enabled = False

    @staticmethod
    def raise_signal(signal_reason):
        Process.signal_reason = signal_reason
        log(f'Raising SIGTERM to current process. Signal reason:', Process.signal_reason)
        os.kill(os.getpid(), signal.SIGTERM)

    @staticmethod
    def interrupt_subprocess(process):
        process.send_signal(signal.SIGINT)

    @staticmethod
    def set_exit_code(code):
        if code > Process.exit_code:
            Process.exit_code = code

    @staticmethod
    def get_exit_code():
        return Process.exit_code


class WatchedProcess:

    def __init__(self, callback, *pargs, **kwargs):
        self.callback = callback
        self.process = subprocess.Popen(*pargs, **kwargs)
        Watchdog.add(self._watcher)

    def _watcher(self):
        if self.process.poll() is not None:
            self.callback(self)
            return False

    def _wait_for_exit(self):
        for i in range(0, 40):
            if self.process.poll() is not None:
                log(f'Process stopped with exit code {self.process.returncode}')
                return True
            log('Waiting...')
            sleep(0.3)
        return False

    def stop(self):
        Watchdog.remove(self._watcher)
        if (self.process is not None) and (self.process.poll() is None):
            log('Process still running. Interrupting...')
            Process.interrupt_subprocess(self.process)
            if not self._wait_for_exit():
                log('Cannot interrupt. Terminating...')
                self.process.terminate()
                if not self._wait_for_exit():
                    log('Cannot terminate. Killing...')
                    self.process.kill()
                    if not self._wait_for_exit():
                        log('What? Cannot kill? Better leave it alone.')


#endregion


#region Pipe implementation based on FIFOs


TEMP_FIFO_PREFIX = 'rtt-hci-fifo-'


class Pipe():
    def __init__(self) -> None:
        self.name = None
        self.server = False
        self.fd = None
        #self.poll = None

    def create(self, write: bool) -> str:
        self.name = tempfile.mktemp(prefix=TEMP_FIFO_PREFIX)
        os.mkfifo(self.name)
        self.server = True
        return self.name

    def get_name(self) -> str:
        return self.name

    def open(self, write: bool, name: str = None) -> None:
        if ((name is None) and (self.name is None)) or (self.fd is not None):
            raise ValueError()
        self.name = self.name or name
        self.fd = open(self.name, 'wb' if write else 'rb', 0)

    def closed(self) -> bool:
        return (self.fd is None) or (self.fd.closed)

    def close(self) -> None:
        try:
            if (self.fd is not None) and (not self.fd.closed):
                self.fd.close()
        finally:
            if self.server:
                try:
                    os.unlink(self.name)
                except:
                    pass
            self.name = None
            self.server = False
            self.fd = None

    def read1(self, size: int) -> 'bytes|None':
        return self.fd.read(size)

    def write(self, data: 'bytes|bytearray') -> None:
        self.fd.write(data)

    def writeable(self):
        # In Linux, we don't need to periodically check output pipe, because we will get
        # signal from Wireshark the moment pipe becomes closed.
        return True

    def flush(self) -> None:
        self.fd.flush()


#endregion


#region Windows compatibility layer


def windows_compatibility():
    # Windows related code is placed in a single function, so we will import win32 modules
    # only when it is needed. This avoids import errors on other platforms.

    import win32pipe, win32file, win32console


    NAMED_PIPE_BUFFER_SIZE = 256 * 1024
    NAMED_PIPE_PREFIX = r'\\.\pipe\rtt_hci_'


    class WinProcess(Process):

        @staticmethod
        def setup_signals(default_reason):
            Process.signal_reason = default_reason
            Process.handler_enabled = True
            signal.signal(signal.SIGBREAK, Process._signal_handler)

        @staticmethod
        def raise_signal(signal_reason):
            Process.signal_reason = signal_reason
            log(f'Sending CTRL-BREAK event to current process. Signal reason:', Process.signal_reason)
            win32console.GenerateConsoleCtrlEvent(win32console.CTRL_BREAK_EVENT, os.getpid())

        @staticmethod
        def interrupt_subprocess(process):
            win32console.GenerateConsoleCtrlEvent(win32console.CTRL_BREAK_EVENT, process.pid)


    class WinPipe:
        def __init__(self) -> None:
            self.name = None
            self.server = False
            self.handle = None

        def create(self, write: bool) -> str:
            self.name = NAMED_PIPE_PREFIX + binascii.hexlify(random.randbytes(16)).decode()
            self.handle = win32pipe.CreateNamedPipe(self.name,
                win32pipe.PIPE_ACCESS_OUTBOUND if write else win32pipe.PIPE_ACCESS_INBOUND,
                win32pipe.PIPE_TYPE_BYTE | win32pipe.PIPE_READMODE_BYTE | win32pipe.PIPE_WAIT,
                1, NAMED_PIPE_BUFFER_SIZE, NAMED_PIPE_BUFFER_SIZE, 0, None)
            if self.handle == win32file.INVALID_HANDLE_VALUE:
                self.handle = None
                self.name = None
                raise IOError(f'Cannot create named pipe on "{self.name}"')
            self.server = True
            return self.name

        def get_name(self):
            return self.name

        def open(self, write: bool, name: str = None) -> None:
            if (name is None) and (self.name is None):
                raise ValueError()
            self.name = self.name or name
            if self.server:
                win32pipe.ConnectNamedPipe(self.handle, None) #TODO: Check if (and which) WINAPI functions raises an exceptions in case of error
            elif self.handle is not None:
                raise ValueError()
            else:
                self.handle = win32file.CreateFile(self.name,
                    win32file.GENERIC_WRITE if write else win32file.GENERIC_READ,
                    0, None, win32file.OPEN_EXISTING, 0, None)
                if self.handle == win32file.INVALID_HANDLE_VALUE:
                    self.handle = None
                    raise IOError(f'Cannot open named pipe "{self.name}"')

        def closed(self) -> bool:
            return self.handle is None

        def read1(self, size: int) -> 'bytes|None':
            try:
                rc, data = win32file.ReadFile(self.handle, size)
            except:
                rc = -1
            if rc != 0:
                raise BrokenPipeError(f'Cannot read from pipe "{self.name}"')
            return data

        def write(self, data: 'bytes|bytearray') -> None:
            if isinstance(data, bytearray):
                data = bytes(data)
            offset = 0
            while offset < len(data):
                try:
                    rc, written = win32file.WriteFile(self.handle, data if offset == 0 else data[offset:])
                except:
                    rc = -1
                if rc != 0:
                    raise BrokenPipeError(f'Cannot write to pipe "{self.name}"')
                offset += written

        def writeable(self):
            try:
                win32file.WriteFile(self.handle, b'')
                return True
            except:
                log('Pipe not writeable - cannot write empty to pipe')
                return False

        def flush(self) -> None:
            win32file.FlushFileBuffers(self.handle)

        def close(self) -> None:
            if self.handle is not None:
                win32file.CloseHandle(self.handle)
            self.name = None
            self.server = False
            self.handle = None


    return (WinProcess, WinPipe)


def windows_initial_setup(bat_file: Path, venv_dir: Path):
    bat_content = dedent(r'''
        @echo off
        call "%~dpn0_venv\Scripts\activate.bat" || exit /b 99
        python "%~dpn0.py" %*
        ''')
    print(f'Configuring virtual environment for { bat_file.with_suffix("").name }... This can take a few minutes...')
    with open(bat_file, 'w') as fd:
        fd.write(bat_content)
    subprocess.run([sys.executable, '-m', 'venv', str(venv_dir.resolve())], check=True)
    subprocess.run(['cmd.exe', '/c', str(bat_file.resolve()), '--pip-install'], check=True)
    print('Environment for plugin configured. You can use it in Wireshark now.')


def windows_pip_install():
    subprocess.run(['pip.exe', 'install', 'pywin32'], check=True)


def windows_remove_setup(bat_file, venv_dir):
    try:
        if venv_dir.exists():
            shutil.rmtree(venv_dir, ignore_errors=True)
        if bat_file.exists():
            os.unlink(bat_file)
    except:
        print('Cannot recreate environment. Please retry after manually removing:')
        print(str(bat_file.resolve()))
        print(str(venv_dir.resolve()))
        exit(1)


is_windows = (platform.system().lower() == 'windows')

if is_windows:
    (Process, Pipe) = windows_compatibility()

#endregion


#region HCI, btsnoop, and pcap constants


MAX_PACKET_DATA_LENGTH = 300 # TODO: Check specification (different data lengths for different packet types), also check max packet length in pcap header

ADAPTER_ID = 0

BT_LOG_ERR = 3
BT_LOG_WARN = 4
BT_LOG_INFO = 6
BT_LOG_DBG = 7

BT_MONITOR_NEW_INDEX = 0
BT_MONITOR_DEL_INDEX = 1
BT_MONITOR_COMMAND_PKT = 2
BT_MONITOR_EVENT_PKT = 3
BT_MONITOR_ACL_TX_PKT = 4
BT_MONITOR_ACL_RX_PKT = 5
BT_MONITOR_SCO_TX_PKT = 6
BT_MONITOR_SCO_RX_PKT = 7
BT_MONITOR_OPEN_INDEX = 8
BT_MONITOR_CLOSE_INDEX = 9
BT_MONITOR_INDEX_INFO = 10
BT_MONITOR_VENDOR_DIAG = 11
BT_MONITOR_SYSTEM_NOTE = 12
BT_MONITOR_USER_LOGGING = 13
BT_MONITOR_ISO_TX_PKT = 18
BT_MONITOR_ISO_RX_PKT = 19
BT_MONITOR_NOP = 255

HCI_H4_CMD = 0x01
HCI_H4_ACL = 0x02
HCI_H4_SCO = 0x03
HCI_H4_EVT = 0x04
HCI_H4_ISO = 0x05

OPCODE_TO_HCI_H4 = {
    BT_MONITOR_COMMAND_PKT: (HCI_H4_CMD, 0),
    BT_MONITOR_EVENT_PKT: (HCI_H4_EVT, 1),
    BT_MONITOR_ACL_TX_PKT: (HCI_H4_ACL, 0),
    BT_MONITOR_ACL_RX_PKT: (HCI_H4_ACL, 1),
    BT_MONITOR_ISO_TX_PKT: (HCI_H4_ISO, 0),
    BT_MONITOR_ISO_RX_PKT: (HCI_H4_ISO, 1),
    BT_MONITOR_SCO_TX_PKT: (HCI_H4_SCO, 0),
    BT_MONITOR_SCO_RX_PKT: (HCI_H4_SCO, 1),
}

BT_MONITOR_MAX_OPCODE = 20

BT_MONITOR_EXT_HDR_MAX = 24

BT_MONITOR_COMMAND_DROPS = 1
BT_MONITOR_EVENT_DROPS = 2
BT_MONITOR_ACL_RX_DROPS = 3
BT_MONITOR_ACL_TX_DROPS = 4
BT_MONITOR_SCO_RX_DROPS = 5
BT_MONITOR_SCO_TX_DROPS = 6
BT_MONITOR_OTHER_DROPS = 7

DROP_NAME = {
    BT_MONITOR_COMMAND_DROPS: 'COMMAND',
    BT_MONITOR_EVENT_DROPS: 'EVENT',
    BT_MONITOR_ACL_RX_DROPS: 'ACL_RX',
    BT_MONITOR_ACL_TX_DROPS: 'ACL_TX',
    BT_MONITOR_SCO_RX_DROPS: 'SCO_RX',
    BT_MONITOR_SCO_TX_DROPS: 'SCO_TX',
    BT_MONITOR_OTHER_DROPS: 'OTHER',
}

BT_MONITOR_TS32 = 8

BT_MONITOR_DROPS_MIN = 1
BT_MONITOR_DROPS_MAX = 7

MONITOR_TS_FREQ = 10000


#endregion


#region Parser of BlueZ's btmon-compatible data stream


class CorruptedException(Exception):
    pass


class Packet:
    opcode: int
    timestamp: float
    drops: 'list[int] | None'
    payload: bytearray
    def __init__(self, opcode, timestamp, drops, payload) -> None:
        self.opcode = opcode
        self.timestamp = timestamp
        self.drops = drops
        self.payload = payload


class InvalidPacket:
    start_offset: int
    end_offset: int
    def __init__(self, start_offset) -> None:
        self.start_offset = start_offset
        self.end_offset = start_offset


class BtmonParser:
    '''
    Parser for btmon compatible capture format.
    It is error tolerant - after corrupted input it is able recover and parse
    valid packets afterwards. The corrupted part of input is returned as InvalidPacket.
    '''

    buffer: bytearray
    total_offset: int
    packets: 'list[Packet|InvalidPacket]'
    resync: bool

    def __init__(self):
        self.buffer = bytearray()
        self.total_offset = 0
        self.packets = []
        self.resync = False

    def parse(self, data: bytes):
        self.buffer += data
        offset = 0
        while len(self.buffer) - offset >= 6:
            if self.resync:
                hdr = self.parse_header(offset)
                if hdr is None:
                    size = 1
                    offset += 1
                    continue
                else:
                    log(f'Sync at {self.total_offset + offset} bytes')
                    if (len(self.packets) > 0) and isinstance(self.packets[-1], InvalidPacket):
                        self.packets[-1].end_offset = self.total_offset + offset
                    self.resync = False
            try:
                size = self.parse_packet(offset)
                offset += size
                if size == 0:
                    break
            except CorruptedException:
                log(f'Invalid at {self.total_offset + offset} bytes')
                if (len(self.packets) == 0) or not isinstance(self.packets[-1], InvalidPacket):
                    self.packets.append(InvalidPacket(self.total_offset + offset))
                size = 1
                offset += 1
                self.resync = True
        if offset > 0:
            self.total_offset += offset
            self.buffer = self.buffer[offset:]

    def parse_header(self, offset):
        hdr = struct.unpack_from('<HHBB', self.buffer, offset)
        if (hdr[0] > MAX_PACKET_DATA_LENGTH) or (hdr[0] < 4) or ((hdr[1] > BT_MONITOR_MAX_OPCODE) and (hdr[1] != BT_MONITOR_NOP)) or (hdr[2] != 0) or (hdr[3] > BT_MONITOR_EXT_HDR_MAX) or (4 + hdr[3] > hdr[0]):
            #TODO: Different payload lengths are allowed for different opcodes
            return None
        return hdr

    def parse_packet(self, offset):
        hdr = self.parse_header(offset)
        if hdr is None:
            raise CorruptedException(f'Corrupted at {self.total_offset + offset}')
        (data_len, opcode, _, hdr_len) = hdr
        if offset + data_len + 2 > len(self.buffer):
            return 0
        hdr_offset = offset + 6
        payload_offset = hdr_offset + hdr_len
        payload_len = data_len - 4 - hdr_len
        timestamp = None
        drops = None
        while hdr_len > 0:
            ext_code = self.buffer[hdr_offset]
            if (ext_code == BT_MONITOR_TS32) and (hdr_len >= 5):
                (timestamp, ) = struct.unpack_from('<L', self.buffer, hdr_offset + 1)
                timestamp /= MONITOR_TS_FREQ
                hdr_len -= 5
                hdr_offset += 5
            elif (ext_code >= BT_MONITOR_DROPS_MIN) and (ext_code <= BT_MONITOR_DROPS_MAX) and (hdr_len >= 2):
                drops = drops or ([0] * (BT_MONITOR_DROPS_MAX + 1))
                drops[ext_code] += self.buffer[hdr_offset + 1]
                hdr_len -= 2
                hdr_offset += 2
            else:
                raise CorruptedException(f'Corrupted at {self.total_offset + offset}')
        if timestamp is None:
            raise CorruptedException(f'Corrupted at {self.total_offset + offset}')
        self.packets.append(Packet(opcode, timestamp, drops, self.buffer[payload_offset:payload_offset+payload_len]))
        return data_len + 2

    def get_packets(self):
        if len(self.packets) == 0:
            result = self.packets
        elif isinstance(self.packets[-1], InvalidPacket):
            result = self.packets[:-1]
            self.packets = self.packets[-1:]
        else:
            result = self.packets
            self.packets = []
        return result


#endregion


#region Generator of pcap output understandable by Wireshark


class PcapGenerator:

    output: bytearray
    last_timestamp: float

    def __init__(self):
        self.output = bytearray(b'\xD4\xC3\xB2\xA1\x02\x00\x04\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x04\x00\xFE\x00\x00\x00')
        self.last_timestamp = 0

    def generate(self, packet: 'Packet | InvalidPacket'):
        if isinstance(packet, InvalidPacket):
            self.generate_invalid(packet)
        else:
            self.generate_valid(packet)

    def generate_drops(self, timestamp, drops):
        for i, count in enumerate(drops):
            if count > 0:
                name = DROP_NAME[i] if i in DROP_NAME else 'Some'
                self.generate_capture_log(timestamp, f'{name} packets dropped, count {count}')

    def generate_invalid(self, packet: InvalidPacket):
        self.generate_capture_log(self.last_timestamp, f'Corrupted input detected from offset {packet.start_offset} to {packet.end_offset}')

    def generate_capture_log(self, timestamp, message):
        self.generate_output(timestamp, BT_MONITOR_SYSTEM_NOTE, message.encode('utf-8'))

    def generate_valid(self, packet: Packet):
        if packet.drops is not None:
            self.generate_drops(packet.timestamp, packet.drops)
        self.generate_output(packet.timestamp, packet.opcode, packet.payload)
        self.last_timestamp = packet.timestamp

    def generate_output(self, timestamp, opcode, payload):
        sec = math.floor(timestamp)
        usec = math.floor((timestamp - sec) * 1000000)
        size = 4 + len(payload)
        self.output += struct.pack('<LLLL', sec, usec, size, size)
        self.output += struct.pack('>HH', ADAPTER_ID, opcode)
        self.output += payload

    def get_output(self):
        result = self.output
        if len(self.output) > 0:
            self.output = bytearray()
        return result


#endregion


#region Main capture controlling class


class Capture:

    rtt_process: 'None | WatchedProcess'
    input_pipe: 'None | Pipe'
    output_pipe: 'None | Pipe'
    rtt_stdout: 'None | io.BufferedWriter'
    direction_input: bool

    def __init__(self):
        self.rtt_process = None
        self.input_pipe = None
        self.output_pipe = None
        self.rtt_stdout = None
        self.direction_input = False

    def report_error(self, message):
        log(f'Reporting error message: {message}')
        Process.set_exit_code(1)
        if args.debug.strip() == '':
            message += ('\nIf you want to see more details, enable debug logs'
                        'in interface configuration.')
        else:
            message += (f'\nYou can see more details in debug logs:\n{args.debug.strip()}')
        print('\n' + message, file=sys.stderr)
        if self.output_pipe is None:
            self.output_pipe = Pipe()
            self.output_pipe.open(True, args.fifo)

    def start_rtt_process(self):
        if not args.device.strip():
            self.report_error('Target device not specified!\n'
                              'Open and change interface configuration.')
            raise SignalTerminated(SignalReason.CAPTURE_STOP)

        cmd = [
            args.logger if args.logger.strip() else 'JLinkRTTLogger',
            '-Device', args.device.strip(),
            '-If', args.iface.strip(),
            '-Speed', args.speed.strip(),
            '-RTTChannel', args.channel.strip(),
        ]
        if args.snr.strip():
            cmd.append('-USB')
            cmd.append(args.snr.strip())
        if args.addr.strip():
            if args.addr.strip().find(' ') < 0:
                cmd.append('-RTTAddress')
                cmd.append(args.addr.strip())
            else:
                cmd.append('-RTTSearchRanges')
                cmd.append(args.addr.strip())
        cmd.append(self.input_pipe.get_name())

        log('Subprocess command', cmd)

        if args.debug_logger:
            self.rtt_stdout = open(args.debug_logger, 'wb')
            stdout = self.rtt_stdout
        else:
            stdout = subprocess.DEVNULL

        creationflags = subprocess.CREATE_NEW_PROCESS_GROUP if hasattr(subprocess, 'CREATE_NEW_PROCESS_GROUP') else 0

        try:
            self.rtt_process = WatchedProcess(self.rtt_process_exited, cmd,
                stdout=stdout, stderr=subprocess.STDOUT, creationflags=creationflags)
        except:
            log(traceback.format_exc())
            self.report_error('Can not start JLinkRTTLogger. Check interface configuration.')
            raise SignalTerminated(SignalReason.CAPTURE_STOP)
        log(f'Process created', self.rtt_process.process.pid)

    def watch_output_pipe(self):
        if not self.output_pipe.writeable():
            log(f'Output pipe closed - capture stop request.')
            Watchdog.stop()
            Process.raise_signal(SignalReason.OUTPUT_PIPE)

    def rtt_process_exited(self, proc: WatchedProcess):
        log(f'JLinkRTTLogger exited unexpectedly with status {proc.process.returncode}')
        Watchdog.stop()
        Process.raise_signal(SignalReason.RTT_PROCESS_EXIT)

    def watchdog_exception(self, ex):
        log('Unexpected exception occurred.')
        log(traceback.format_exc())
        self.report_error('Unexpected exception occurred.')
        Process.raise_signal(SignalReason.UNKNOWN)

    def capture_main_loop(self):
        Process.setup_signals(SignalReason.CAPTURE_STOP)

        log('Capturing...')

        self.output_pipe = Pipe()
        self.output_pipe.open(True, args.fifo)
        log('Output Opened')

        Watchdog.init(self.watchdog_exception)

        self.input_pipe = Pipe()
        self.input_pipe.create(False)
        log('Input Created')

        self.start_rtt_process()

        self.input_pipe.open(False)
        log('Input Opened')

        parse = BtmonParser()
        gen = PcapGenerator()

        if is_windows:
            Watchdog.add(self.watch_output_pipe)

        # Main capture transfer loop
        while True:
            pcap_data = gen.get_output()
            log(f'Generated data {len(pcap_data)}')
            if len(pcap_data):
                self.direction_input = False
                self.output_pipe.write(pcap_data)
                self.output_pipe.flush()
                log(f'Write done')
            self.direction_input = True
            data = self.input_pipe.read1(2048)
            if (data is None) or (len(data) == 0):
                log(f'Data ended from JLinkRTTLogger site: {data}')
                raise BrokenPipeError()
            log(f'Parsing {len(data)}')
            parse.parse(data)
            packets = parse.get_packets()
            log(f'Packets payload length: {["invalid" if isinstance(p, InvalidPacket) else len(p.payload) for p in packets]}')
            for packet in packets:
                gen.generate(packet)

    def stop_watchers(self):
        Process.disable_signals()
        Watchdog.stop()

    def capture(self):
        try:
            try:
                try:
                    self.capture_main_loop()
                except BrokenPipeError:
                    self.stop_watchers()
                    log('BrokenPipeError')
                    raise SignalTerminated(SignalReason.INPUT_PIPE if self.direction_input and self.output_pipe.writeable() else SignalReason.OUTPUT_PIPE)
                except KeyboardInterrupt:
                    self.stop_watchers()
                    log('KeyboardInterrupt')
                    raise SignalTerminated(SignalReason.CAPTURE_STOP)
            except SignalTerminated as ex:
                self.stop_watchers()
                log(f'Termination signal received: {str(ex.signal_reason)}')
                if (ex.signal_reason == SignalReason.CAPTURE_STOP) or (ex.signal_reason == SignalReason.OUTPUT_PIPE):
                    log(f'Capture stopped, exiting gracefully')
                elif (ex.signal_reason == SignalReason.RTT_PROCESS_EXIT) or (ex.signal_reason == SignalReason.INPUT_PIPE):
                    self.report_error('JLinkRTTLogger exited unexpectedly.')
        except:
            self.stop_watchers()
            log('Unexpected exception occurred.')
            log(traceback.format_exc())
            self.report_error('Unexpected exception occurred.')
        finally:
            # Try to cleanup resources
            try:
                self.stop_watchers()
            except:
                log(traceback.format_exc())
            try:
                if self.rtt_process is not None:
                    log('Stopping RTT process...')
                    self.rtt_process.stop()
                    log('OK')
            except:
                log(traceback.format_exc())
            try:
                if self.input_pipe is not None:
                    log('Input pipe not closed. Closing...')
                    self.input_pipe.close()
                    log('OK')
            except:
                log(traceback.format_exc())
            try:
                if self.output_pipe is not None:
                    log('Output pipe not closed. Closing...')
                    self.output_pipe.close()
                    log('OK')
            except:
                log(traceback.format_exc())
            try:
                if self.rtt_stdout is not None:
                    log('JLinkRTTLogger standard output file not closed. Closing...')
                    self.rtt_stdout.close()
                    log('OK')
            except:
                log(traceback.format_exc())


#endregion


#region Main function


def main():
    if args.extcap_interfaces:
        print(EXTCAP_INTERFACES)
    elif args.extcap_dlts:
        print(EXTCAP_DLTS)
    elif args.extcap_config:
        print(EXTCAP_CONFIG)
    elif args.capture:
        Capture().capture()
    elif is_windows and args.pip_install:
        windows_pip_install()
    else:
        if is_windows:
            script_file = Path(__file__)
            bat_file = script_file.with_suffix('.bat')
            venv_dir = Path(str(script_file.with_suffix('')) + '_venv')
            setup_needed = ((not bat_file.exists()) or (not venv_dir.exists()) and
                            (script_file.parent.name.lower() == 'extcap'))
            if setup_needed or args.initial_setup:
                if args.initial_setup:
                    windows_remove_setup(bat_file, venv_dir)
                windows_initial_setup(bat_file, venv_dir)
                return
        print_help_message()
        Process.set_exit_code(98)

try:
    main()
except:
    print('Unexpected exception occurred. See debug file for details.', file=sys.stderr)
    if args.debug.strip() != '':
        log(traceback.format_exc())
    else:
        traceback.print_exc(file=sys.stderr)
    Process.set_exit_code(99)
finally:
    log(f'End of log, exit code {Process.get_exit_code()}')
    debug_log_close()

exit(Process.get_exit_code())


#endregion
