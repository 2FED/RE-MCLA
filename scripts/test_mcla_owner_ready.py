import copy
import contextlib
import io
import json
import os
import unittest
import uuid
from mcla_owner_ready import ConsoleReady, NativeEvents, ReadyLine, event_name
from mcla_owner_ready_decode import validate, strict
from test_mcla_game_source_scout_decode import fixture, TOKEN


def data():
    source = fixture(anchor=710000, frequency=10000)
    ready = dict(schema='mcla-owner-ready-v1', token=TOKEN, owner_assertion_only=True,
        scene_proven=False, clock='QueryPerformanceCounter', frequency=10000,
        journal_begin_qpc=10000, journal_end_qpc=1810000, ready_deadline_qpc=1210000,
        opened_qpc=11000, waiting_before_qpc=12000, waiting_after_qpc=13000,
        ready_before_qpc=709998, ready_after_qpc=710000, source_observed_qpc=710000,
        freeze_qpc=1201000)
    signal = dict(schema='mcla-owner-ready-signal-v1', qpc_frequency=10000,
        signal_before_qpc=709999, signal_after_qpc=710002, owner_assertion_only=True, scene_proven=False)
    handoff = dict(token=TOKEN, qpc_frequency=10000, observer_begin_qpc=10000, freeze_qpc=1201000)
    return ready, signal, source, handoff


class ReadyTests(unittest.TestCase):
    def validate(self, parts):
        ready, signal, source, handoff = parts
        return validate(json.dumps(ready).encode(), signal, source, handoff, TOKEN)

    def test_real_source_shape_and_native_wakes_before_post_signal(self):
        value = self.validate(data())
        self.assertTrue(value['owner_ready_assertion_validated'])
        self.assertFalse(value['scene_proven'])
        self.assertFalse(value['fps_proven'])

    def test_red_old_early_presentation_anchor_precedes_owner_readiness(self):
        parts = data()
        parts[2]['readiness_observed_qpc'] = 100000
        parts[2]['sequence']['begin_qpc'] = 500000
        with self.assertRaisesRegex(ValueError, 'paired headers'):
            self.validate(parts)

    def test_schema_and_all_integer_fields_strict(self):
        for key in data()[0]:
            parts = data()
            del parts[0][key]
            with self.subTest(key=key), self.assertRaises(ValueError):
                self.validate(parts)
        for key, value in data()[0].items():
            if type(value) is not int:
                continue
            for invalid in (True, 0, -1, 2**63, '1', 1.0):
                parts = data()
                parts[0][key] = invalid
                with self.subTest(key=key, value=invalid), self.assertRaises(ValueError):
                    self.validate(parts)
        parts = data()
        parts[0]['unexpected'] = 1
        with self.assertRaises(ValueError):
            self.validate(parts)

    def test_boundaries_and_cannot_launder_cut_with_earlier_precheck(self):
        for index, key, value in ((0,'source_observed_qpc',1210000), (0,'ready_after_qpc',1210000),
            (0,'freeze_qpc',1810000), (0,'freeze_qpc',1200999), (0,'journal_end_qpc',1810001),
            (0,'scene_proven',True), (1,'signal_before_qpc',710001),
            (1,'signal_after_qpc',709998), (1,'signal_after_qpc',1210000),
            (1,'qpc_frequency',9999), (2,'clock','wrong'), (3,'freeze_qpc',1810000)):
            parts = data()
            parts[index][key] = value
            with self.subTest(key=key,value=value), self.assertRaises(ValueError):
                self.validate(parts)

    def test_bad_token_duplicate_json_and_nonfinite(self):
        for token in (1, None, 'x'*32, 'A'*32, TOKEN+'0'):
            ready, signal, source, handoff = data()
            for obj in (ready,source,handoff): obj['token'] = token
            with self.assertRaises(ValueError):
                validate(json.dumps(ready).encode(),signal,source,handoff,token)
        for raw in (b'{"a":1,"a":2}', b'{"a":NaN}', b'{}'+b' '*4096):
            with self.assertRaises(ValueError): strict(raw)

    def test_ready_line_bounded_exact_once(self):
        parser = ReadyLine()
        for ch in 'READX\bY': self.assertIsNone(parser.feed(ch))
        self.assertEqual(parser.feed('\r'), 'READY')
        with self.assertRaises(ValueError): parser.feed('\r')
        for line in ('reaDY', 'ready', ' READY', 'READY '):
            parser = ReadyLine()
            for ch in line: parser.feed(ch)
            self.assertEqual(parser.feed('\r'), 'READY')

    def test_typo_or_empty_line_reprompts_without_consuming_assertion(self):
        parser = ReadyLine()
        for line in ('', 'READ', 'READYx'):
            for ch in line: parser.feed(ch)
            self.assertEqual(parser.feed('\r'), 'INVALID')
            self.assertFalse(parser.finished)
            self.assertEqual(parser.value, '')
        for ch in 'reaDY': parser.feed(ch)
        self.assertEqual(parser.feed('\r'), 'READY')

    def test_overflow_cannot_truncate_to_ready(self):
        parser = ReadyLine()
        for ch in 'READY' + ' ' * 12: parser.feed(ch)
        self.assertLessEqual(len(parser.value), 16)
        self.assertEqual(parser.feed('\r'), 'INVALID')
        for ch in 'ready': parser.feed(ch)
        self.assertEqual(parser.feed('\r'), 'READY')

    def test_explicit_cancel(self):
        for line in ('cancel\r', ' CANCEL \r', '\x1b'):
            parser = ReadyLine()
            with self.assertRaisesRegex(ValueError, 'owner cancelled'):
                for ch in line: parser.feed(ch)

    def test_console_visible_edit_retry_and_bounded_poll(self):
        class Keys:
            def __init__(self, text): self.keys = list(text)
            def kbhit(self): return bool(self.keys)
            def getwch(self): return self.keys.pop(0)
        console = ConsoleReady.__new__(ConsoleReady)
        console.parser = ReadyLine()
        console.console = Keys('wrong\r\xe0KreaDx\bY\r')
        out = io.StringIO()
        with contextlib.redirect_stdout(out): result = console.poll()
        self.assertEqual(result, 'READY')
        self.assertIn('wrong\n', out.getvalue())
        self.assertIn('READY> ', out.getvalue())
        self.assertIn('reaDx\b \bY\n', out.getvalue())
        self.assertNotIn('K', out.getvalue())
        console.parser = ReadyLine()
        console.console = Keys('\r' * 64)
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertIsNone(console.poll())
        self.assertEqual(len(console.console.keys), 32)

    def test_delete_and_unsupported_character_cannot_authorize(self):
        parser = ReadyLine()
        for ch in 'READx\x7fY': parser.feed(ch)
        self.assertEqual(parser.feed('\r'), 'READY')
        parser = ReadyLine()
        for ch in 'READY\u044f': parser.feed(ch)
        self.assertEqual(parser.feed('\r'), 'INVALID')

    def test_extended_keys_and_ctrl_c(self):
        parser = ReadyLine()
        for key in ('\x00','R','\xe0','E'): self.assertIsNone(parser.feed(key))
        self.assertEqual(parser.value, '')
        with self.assertRaises(KeyboardInterrupt): parser.feed('\x03')

    @unittest.skipUnless(os.name == 'nt', 'Windows event-only native test')
    def test_fresh_kernel_objects_collision_partial_cleanup_signal_once(self):
        token = uuid.uuid4().hex
        owner = NativeEvents(token)
        try:
            with self.assertRaisesRegex(ValueError,'already exists'): NativeEvents(token)
            self.assertFalse(owner.waiting())
            with self.assertRaisesRegex(ValueError,'announce'): owner.signal()
            self.assertTrue(owner.k.SetEvent(owner.handles[1]))
            value = owner.signal()
            self.assertLessEqual(value['signal_before_qpc'], value['signal_after_qpc'])
            with self.assertRaises(ValueError): owner.signal()
        finally:
            owner.close()
        # Waiting-only collision exercises cleanup of newly created READY too.
        k = owner.k
        waiting = k.CreateEventW(None,True,False,event_name(token,True))
        try:
            with self.assertRaisesRegex(ValueError,'already exists'): NativeEvents(token)
        finally:
            k.CloseHandle(waiting)
        fresh = NativeEvents(token)
        fresh.close()

if __name__ == '__main__': unittest.main()
