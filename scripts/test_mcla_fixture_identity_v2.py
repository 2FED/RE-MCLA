"""Offline only: unique fixture content model, not Windows/WGC/game timing."""
from dataclasses import replace
import struct
import unittest
import zlib

from mcla_fixture_identity_v2 import (
    BLACK, CELL_H, CELL_W, COLS, HEIGHT, MAX_PUBLICATIONS, NATIVE_ADMITTED,
    U64_MAX, WHITE, WIDTH, Challenge, FixtureModel, Identity, Observation,
    Publication, decode, encode,
)

NONCE = bytes(range(16))


def prepared():
    model = FixtureModel(NONCE)
    challenge = model.arm(100, 101, 110)
    # Producer begins inside host issue bracket, which is allowed.
    publication = Publication(challenge.identity, 102, 105, 108, True, True)
    return model, challenge, publication


def observed(identity, **changes):
    return replace(Observation(1, 110, 120, encode(identity), 999999), **changes)


class CodecTests(unittest.TestCase):
    def test_exact_roundtrips_and_unique_ids_beyond_old_eight_phase_period(self):
        images = []
        for number in (1, 2, 8, 9, 256, U64_MAX):
            identity = Identity(NONCE, number, number)
            image = encode(identity)
            self.assertEqual(len(image), WIDTH * HEIGHT * 4)
            self.assertEqual(decode(image), identity)
            images.append(image)
        self.assertEqual(len(set(images)), len(images))

    def test_nonce_and_unsigned_nonwrapping_ids(self):
        for nonce in (b'', bytes(16), bytes(15), bytes(17), bytearray(NONCE), 'x' * 16):
            with self.subTest(nonce_type=type(nonce)), self.assertRaises(ValueError):
                Identity(nonce, 1, 1)
        for value in (-1, 0, True, 1.0, U64_MAX + 1, '1'):
            for slot in (0, 1):
                args = [1, 1]; args[slot] = value
                with self.subTest(value=value, slot=slot), self.assertRaises(ValueError):
                    Identity(NONCE, *args)

    def test_truncation_extension_and_nonbytes_rejected(self):
        image = encode(Identity(NONCE, 1, 1))
        for invalid in (image[:-1], image + b'\0', bytearray(image), None):
            with self.assertRaises(ValueError):
                decode(invalid)

    def test_every_grid_cell_interior_and_last_pixel_checked(self):
        image = encode(Identity(NONCE, 1, 1))
        for cell in range(320):
            row, col = divmod(cell, COLS)
            offset = ((row * CELL_H + 1) * WIDTH + col * CELL_W + 1) * 4
            changed = bytearray(image); changed[offset] ^= 1
            with self.subTest(cell=cell), self.assertRaisesRegex(ValueError, 'full-grid'):
                decode(bytes(changed))
        changed = image[:-1] + bytes((image[-1] ^ 1,))
        with self.assertRaises(ValueError):
            decode(changed)

    def test_sampled_bit_corruption_rejects_checksum(self):
        changed = bytearray(encode(Identity(NONCE, 1, 1)))
        changed[:4] = WHITE if changed[:4] == BLACK else BLACK
        with self.assertRaisesRegex(ValueError, 'checksum'):
            decode(bytes(changed))

    def test_mixed_valid_frames_rejected(self):
        a, b = encode(Identity(NONCE, 1, 1)), encode(Identity(NONCE, 9, 9))
        # Alternate scanlines, keeping a valid sampled identity in each cell.
        stride = WIDTH * 4
        mixed = b''.join((a if row % 2 == 0 else b)[row * stride:(row + 1) * stride]
                         for row in range(HEIGHT))
        with self.assertRaisesRegex(ValueError, 'full-grid'):
            decode(mixed)

    def test_unknown_magic_even_with_valid_checksum(self):
        body = struct.pack('>4s16sQQ', b'MFV1', NONCE, 1, 1)
        packet = body + struct.pack('>I', zlib.crc32(body))
        bits = [(byte >> bit) & 1 for byte in packet for bit in range(7, -1, -1)]
        image = b''.join(b''.join((WHITE if bit else BLACK) * CELL_W
                        for bit in bits[row * COLS:(row + 1) * COLS]) * CELL_H
                        for row in range(10))
        with self.assertRaisesRegex(ValueError, 'schema'):
            decode(image)


class LifecycleTests(unittest.TestCase):
    def test_completed_challenge_credit_has_no_timing_claim(self):
        model, challenge, publication = prepared()
        model.complete(publication)
        result = model.observe(observed(challenge.identity))
        self.assertTrue(result['fixture_content_model_pass'])
        self.assertEqual(result['raw_wgc_100ns'], 999999)
        for field in ('native_capture_admitted', 'compositor_time_proven',
                      'guest_timing_proven', 'fps_proven'):
            self.assertFalse(result[field])
        self.assertFalse(NATIVE_ADMITTED)

    def test_raw_wgc_labels_are_opaque_not_freshness_or_cadence(self):
        for raw in (None, -100, 0, 1, (1 << 63) - 1):
            model, challenge, publication = prepared(); model.complete(publication)
            result = model.observe(observed(challenge.identity, raw_wgc_100ns=raw))
            self.assertEqual(result['raw_wgc_100ns'], raw)

    def test_invalid_brackets_and_types(self):
        identity = Identity(NONCE, 1, 1)
        factories = (
            lambda: Challenge(identity, 100, 100, 101),
            lambda: Challenge(identity, 100, 102, 101),
            lambda: Challenge(identity, -1, 1, 2),
            lambda: Challenge(identity, True, 2, 3),
            lambda: Publication(identity, 103, 102, 104, True, True),
            lambda: Publication(identity, 101, 102, 101, True, True),
            lambda: Publication(identity, 101, 102, 103, 1, True),
            lambda: Observation(1, 120, 110, b''),
            lambda: Observation(1, 110, U64_MAX + 1, b''),
            lambda: Observation(1, 110, 120, b'', 1 << 63),
            lambda: Observation(True, 110, 120, b''),
        )
        for factory in factories:
            with self.assertRaises(ValueError): factory()

    def test_missing_record_is_terminal_not_retry(self):
        model, challenge, publication = prepared()
        with self.assertRaisesRegex(ValueError, 'completed producer'):
            model.observe(observed(challenge.identity))
        with self.assertRaisesRegex(ValueError, 'already failed'):
            model.complete(publication)

    def test_failed_drawing_cleanup_stale_or_duplicate_publication(self):
        for change in ({'draw_ok': False}, {'cleanup_ok': False},
                       {'identity': Identity(NONCE, 2, 2)},
                       {'draw_before_qpc': 100}):
            model, _, publication = prepared()
            with self.assertRaises(ValueError): model.complete(replace(publication, **change))
            self.assertTrue(model.failed)
        model, _, publication = prepared(); model.complete(publication)
        with self.assertRaises(ValueError): model.complete(publication)

    def test_wrong_session_stale_paint_and_challenge_reject_terminally(self):
        for wrong in (Identity(b'x' * 16, 1, 1), Identity(NONCE, 1, 2), Identity(NONCE, 2, 1)):
            model, challenge, publication = prepared(); model.complete(publication)
            with self.assertRaisesRegex(ValueError, 'identity'): model.observe(observed(wrong))
            with self.assertRaisesRegex(ValueError, 'already failed'):
                model.observe(observed(challenge.identity))

    def test_no_favorable_frame_selection_or_early_capture(self):
        for change in ({'index': 2}, {'before_qpc': 109}, {'pixels': b''}):
            model, challenge, publication = prepared(); model.complete(publication)
            with self.assertRaises(ValueError): model.observe(observed(challenge.identity, **change))
            self.assertTrue(model.failed)

    def test_sequential_challenges_never_reuse_identity(self):
        model, challenge, publication = prepared(); model.complete(publication)
        old = observed(challenge.identity); model.observe(old)
        second = model.arm(121, 122, 123)
        self.assertEqual((second.identity.challenge, second.identity.paint), (2, 2))
        model.complete(Publication(second.identity, 124, 125, 126, True, True))
        with self.assertRaises(ValueError):
            model.observe(replace(old, before_qpc=127, after_qpc=128))

    def test_successful_second_challenge_and_raw_label_repetition(self):
        model, challenge, publication = prepared(); model.complete(publication)
        first = model.observe(observed(challenge.identity, raw_wgc_100ns=5))
        second = model.arm(121, 122, 123)
        model.complete(Publication(second.identity, 124, 125, 126, True, True))
        result = model.observe(observed(second.identity, before_qpc=127,
                                        after_qpc=128, raw_wgc_100ns=5))
        self.assertNotEqual(first['identity'], result['identity'])
        self.assertEqual(result['raw_wgc_100ns'], 5)
        self.assertIsNone(model.challenge)
        self.assertFalse(model.failed)

    def test_wrong_object_methods_poison_run(self):
        for method in ('complete', 'observe'):
            model, _, _ = prepared()
            with self.assertRaises(ValueError): getattr(model, method)({})
            self.assertTrue(model.failed)
            with self.assertRaisesRegex(ValueError, 'already failed'):
                model.arm(121, 122, 123)
        model = FixtureModel(NONCE)
        with self.assertRaises(ValueError): model.arm('100', 101, 110)
        self.assertTrue(model.failed)

    def test_reordered_or_overlapping_host_challenges_rejected(self):
        model, _, _ = prepared()
        with self.assertRaises(ValueError): model.arm(121, 122, 123)
        model, challenge, publication = prepared(); model.complete(publication)
        model.observe(observed(challenge.identity))
        with self.assertRaises(ValueError): model.arm(119, 122, 123)

    def test_budget_exhaustion_no_wrap(self):
        model = FixtureModel(NONCE)
        # White-box boundary without allocating thousands of full pixel grids.
        model.count = MAX_PUBLICATIONS
        with self.assertRaisesRegex(ValueError, 'budget'): model.arm(1, 2, 3)
        self.assertTrue(model.failed)


if __name__ == '__main__':
    unittest.main()
