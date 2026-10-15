"""Bounded diagnostic owner assertion; no game launch, injection or scene proof.

Only the owning supervisor creates fresh manual-reset named events before launch.
Both handles remain held until owned-process cleanup. No inherited handles.
"""
import ctypes as ct
import re


def need(ok, message):
    if not ok:
        raise ValueError(message)


def event_name(token, waiting=False):
    need(type(token) is str and re.fullmatch('[0-9a-f]{32}', token), 'owner-ready token')
    return 'Local\\MCLA_OWNER_' + ('WAITING' if waiting else 'READY') + '_V1_' + token


class NativeEvents:
    def __init__(self, token):
        self.handles = []
        self.sent = False
        self.k = ct.WinDLL('kernel32', use_last_error=True)
        for name, restype, args in (
            ('CreateEventW', ct.c_void_p, [ct.c_void_p, ct.c_int, ct.c_int, ct.c_wchar_p]),
            ('WaitForSingleObject', ct.c_uint32, [ct.c_void_p, ct.c_uint32]),
            ('SetEvent', ct.c_int, [ct.c_void_p]),
            ('CloseHandle', ct.c_int, [ct.c_void_p]),
            ('QueryPerformanceCounter', ct.c_int, [ct.POINTER(ct.c_int64)]),
            ('QueryPerformanceFrequency', ct.c_int, [ct.POINTER(ct.c_int64)])):
            fn = getattr(self.k, name)
            fn.restype, fn.argtypes = restype, args
        try:
            for waiting in (False, True):
                handle = self.k.CreateEventW(None, True, False, event_name(token, waiting))
                error = ct.get_last_error()
                if handle:
                    self.handles.append(handle)
                need(handle and error != 183, 'owner-ready object already exists or create failed')
                need(self.k.WaitForSingleObject(handle, 0) == 258, 'owner-ready object not empty')
            f = ct.c_int64()
            need(self.k.QueryPerformanceFrequency(ct.byref(f)) and 10 <= f.value <= 10**10, 'owner-ready QPF')
            self.frequency = f.value
        except BaseException:
            self.close()
            raise

    def qpc(self):
        value = ct.c_int64()
        need(self.k.QueryPerformanceCounter(ct.byref(value)) and value.value > 0, 'owner-ready QPC')
        return value.value

    def waiting(self):
        need(len(self.handles) == 2 and not self.sent, 'owner-ready handles or already sent')
        result = self.k.WaitForSingleObject(self.handles[1], 0)
        need(result in (0, 258), 'owner-ready waiting query')
        return result == 0

    def signal(self):
        need(self.waiting(), 'native must announce waiting before READY')
        need(self.k.WaitForSingleObject(self.handles[0], 0) == 258, 'READY already signaled')
        before = self.qpc()
        need(self.k.SetEvent(self.handles[0]), 'owner-ready SetEvent failed')
        self.sent = True
        after = self.qpc()
        need(after >= before, 'owner-ready signal clock reversed')
        return dict(schema='mcla-owner-ready-signal-v1', qpc_frequency=self.frequency,
                    signal_before_qpc=before, signal_after_qpc=after,
                    owner_assertion_only=True, scene_proven=False)

    def close(self):
        errors = []
        while self.handles:
            if not self.k.CloseHandle(self.handles.pop()):
                errors.append(ct.get_last_error())
        need(not errors, 'owner-ready handle close failed')


class ReadyLine:
    """Injected nonblocking keystroke parser. NEVER sends input to the game."""
    def __init__(self):
        self.value = ''
        self.extended = False
        self.finished = False
        self.invalid = False

    def feed(self, key):
        need(not self.finished, 'owner-ready line already consumed')
        if self.extended:
            self.extended = False
            return None
        if key in ('\x00', '\xe0'):
            self.extended = True
            return None
        if key == '\x03':
            raise KeyboardInterrupt()
        need(key != '\x1b', 'owner cancelled readiness')
        if key in ('\b', '\x7f'):
            self.value = self.value[:-1]
        elif key in ('\r', '\n'):
            command = self.value.strip().upper() if not self.invalid else ''
            need(command != 'CANCEL', 'owner cancelled readiness')
            if command == 'READY':
                self.finished = True
                return 'READY'
            self.value = ''
            self.invalid = False
            return 'INVALID'
        else:
            if len(key) == 1 and ' ' <= key <= '~' and len(self.value) < 16:
                self.value += key
            else:
                # Never accept a truncated or partially discarded command.
                self.invalid = True
        return None


class ConsoleReady:
    def __init__(self):
        import msvcrt
        self.console = msvcrt
        self.parser = ReadyLine()
        k = ct.WinDLL('kernel32', use_last_error=True)
        k.GetStdHandle.argtypes, k.GetStdHandle.restype = [ct.c_uint32], ct.c_void_p
        k.GetConsoleMode.argtypes, k.GetConsoleMode.restype = [ct.c_void_p, ct.POINTER(ct.c_uint32)], ct.c_int
        k.FlushConsoleInputBuffer.argtypes, k.FlushConsoleInputBuffer.restype = [ct.c_void_p], ct.c_int
        handle = k.GetStdHandle(ct.c_uint32(-10).value)
        mode = ct.c_uint32()
        need(handle and k.GetConsoleMode(handle, ct.byref(mode)), 'interactive console required for READY')
        need(k.FlushConsoleInputBuffer(handle), 'owner-ready console flush failed')
        print('\nLOAD FREE ROAM FIRST. Park and leave the game controls alone.\n'
              'Then Alt+Tab here, type READY and Enter, and return to the game.\n'
              'Capture begins 40 seconds AFTER that confirmation. Case does not matter.\n'
              'Backspace edits. Typos ask again without extending the time limit.\n'
              'Type CANCEL or press Esc to cancel.\n'
              'READY> ', end='', flush=True)

    def poll(self):
        # Bounded work per supervisor heartbeat; no orphan input thread.
        for _ in range(32):
            if not self.console.kbhit():
                break
            key = self.console.getwch()
            before = self.parser.value
            result = self.parser.feed(key)
            if key in ('\r', '\n'):
                print('', flush=True)
            elif len(self.parser.value) < len(before):
                print('\b \b', end='', flush=True)
            elif len(self.parser.value) > len(before):
                print(key, end='', flush=True)
            if result == 'INVALID':
                print('Not confirmed. Type READY when parked, or CANCEL. Time limit unchanged.\n'
                      'READY> ', end='', flush=True)
            elif result == 'READY':
                return result
        return None
