"""Pure post-collection sequence contract. No native imports or execution."""
from dataclasses import replace
import unittest

from mcla_fixture_identity_v2 import Identity, Observation, Publication, encode
from mcla_content_sequence_v2 import SequenceOracle, Sample, NATIVE_ADMITTED

NONCE = bytes(range(16))


def sample(slot, *, arm=None, end=None, frequency=1000):
    arm = (100 if slot == 0 else 1000 + (slot-1)*100) if arm is None else arm
    end = arm + 30 if end is None else end
    identity = Identity(NONCE, slot+1, slot+1)
    return Sample(slot, frequency, arm, arm+1, arm+10,
                  Publication(identity, arm+2, arm+5, arm+8, True, True),
                  Observation(1, arm+11, end, encode(identity), -42))


def oracle():
    return SequenceOracle(NONCE, frequency=1000, begin_qpc=90, schedule_origin=1000)


class SequenceTests(unittest.TestCase):
    def reject(self, instance, value, reason='.'):
        with self.assertRaisesRegex((ValueError, TypeError, AttributeError), reason):
            instance.accept(value)
        with self.assertRaisesRegex(ValueError, 'terminal'):
            instance.accept(sample(0))
        with self.assertRaisesRegex(ValueError, 'terminal'):
            instance.finish(normal_cleanup=True)

    def test_complete_91_unique_frames_only_earns_offline_content_budget(self):
        instance = oracle()
        for slot in range(91): instance.accept(sample(slot))
        report = instance.finish(normal_cleanup=True)
        self.assertEqual(report['publications'], 91)
        self.assertEqual(report['timed_publications'], 90)
        self.assertEqual(report['max_source_window_ticks'], 28)
        self.assertEqual(report['max_conservative_gap_ticks'], 128)
        self.assertTrue(report['fixture_content_sequence_pass'])
        self.assertTrue(report['synthetic_transport_budget_pass'])
        for key in ('native_admitted', 'cadence_admitted', 'compositor_time_proven', 'game_timing_proven', 'fps_proven'):
            self.assertIs(report[key], False)
        self.assertFalse(NATIVE_ADMITTED)
        with self.assertRaisesRegex(ValueError, 'terminal'): instance.accept(sample(0))
        with self.assertRaisesRegex(ValueError, 'terminal'): instance.finish(normal_cleanup=True)

    def test_missing_warmup_skipped_duplicate_and_malformed_slots_fail(self):
        for slot in (1, -1, True, '0', 92):
            self.reject(oracle(), replace(sample(0), slot=slot))
        instance = oracle(); instance.accept(sample(0))
        self.reject(instance, sample(0), 'consecutive')
        instance = oracle(); instance.accept(sample(0))
        self.reject(instance, sample(2), 'consecutive')
        self.reject(oracle(), None, 'sample')

    def test_wrong_session_stale_ids_and_paint_are_terminal(self):
        for identity in (Identity(bytes(range(1,17)),1,1), Identity(NONCE,2,2), Identity(NONCE,1,2)):
            record = sample(0)
            record = replace(record, publication=replace(record.publication, identity=identity),
                             observation=replace(record.observation, pixels=encode(identity)))
            self.reject(oracle(), record, 'identity')

    def test_stale_pixels_after_valid_metadata_reject_without_replacement(self):
        instance = oracle(); instance.accept(sample(0))
        value = sample(1)
        self.reject(instance, replace(value, observation=replace(value.observation,
                            pixels=encode(Identity(NONCE,1,1)))), 'identity')

    def test_mixed_grid_and_alpha_fail(self):
        value = sample(0)
        self.reject(oracle(), replace(value, observation=replace(value.observation,
                    pixels=value.observation.pixels[:-1]+b'\0')), 'full-grid')

    def test_first_observation_only_no_favorable_frame_selection(self):
        value = sample(0)
        self.reject(oracle(), replace(value, observation=replace(value.observation,index=2)), 'first')

    def test_frequency_and_unsigned_clock_types(self):
        for field in ('frequency','arm_qpc','issue_before_qpc','issue_after_qpc'):
            for invalid in (None, True, -1, 1.5):
                self.reject(oracle(), replace(sample(0), **{field:invalid}))
        self.reject(oracle(), replace(sample(0),frequency=999), 'frequency')

    def test_warmup_must_end_before_origin_and_inside_budget(self):
        self.reject(oracle(), sample(0, end=1001), 'warmup')
        self.reject(oracle(), sample(0, arm=89), 'begin')

    def test_no_early_arms_or_late_catchup(self):
        for arm in (999,1100):
            instance=oracle(); instance.accept(sample(0))
            self.reject(instance, sample(1,arm=arm), 'schedule')

    def test_producer_cleanup_must_precede_ack(self):
        value=sample(0)
        self.reject(oracle(),replace(value,publication=replace(value.publication,cleanup_after_qpc=111)), 'ack')

    def test_source_window_strict_200ms_boundary(self):
        instance=oracle(); instance.accept(sample(0))
        self.reject(instance,sample(1,end=1202),'source window')
        instance=oracle(); instance.accept(sample(0))
        instance.accept(sample(1,end=1201))

    def test_conservative_gap_catches_short_but_late_neighbor(self):
        instance=oracle(); instance.accept(sample(0)); instance.accept(sample(1,end=1030))
        # Both individual windows are short; interval uncertainty still exceeds200ms.
        self.reject(instance,sample(2,arm=1190,end=1220),'conservative gap')

    def test_order_overlap_and_capture_before_cleanup(self):
        instance=oracle(); instance.accept(sample(0)); instance.accept(sample(1,end=1120))
        self.reject(instance,sample(2),'overlap')
        value=sample(0)
        self.reject(oracle(),replace(value,observation=replace(value.observation,before_qpc=109)))

    def test_failed_draw_or_cleanup_never_earns_content(self):
        for name in ('draw_ok','cleanup_ok'):
            value=sample(0)
            self.reject(oracle(),replace(value,publication=replace(value.publication,**{name:False})))

    def test_raw_wgc_labels_never_decide_acceptance(self):
        instance=oracle()
        for slot in range(91):
            value=sample(slot)
            instance.accept(replace(value,observation=replace(value.observation,
                            raw_wgc_100ns=(2**63-1 if slot%2 else -2**63))))
        self.assertTrue(instance.finish(normal_cleanup=True)['fixture_content_sequence_pass'])

    def test_partial_finish_and_bad_cleanup_poison(self):
        for cleanup in (True,False,None,1):
            instance=oracle(); instance.accept(sample(0))
            with self.assertRaises(ValueError): instance.finish(normal_cleanup=cleanup)
            with self.assertRaisesRegex(ValueError,'terminal'): instance.accept(sample(1))

    def test_complete_sequence_still_requires_exact_cleanup_true(self):
        for cleanup in (False, 1, None):
            instance=oracle()
            for slot in range(91): instance.accept(sample(slot))
            with self.assertRaisesRegex(ValueError,'cleanup'): instance.finish(normal_cleanup=cleanup)
            with self.assertRaisesRegex(ValueError,'terminal'): instance.finish(normal_cleanup=True)

    def test_nested_malformed_records_and_extra_publication_are_terminal(self):
        for field in ('publication','observation'):
            self.reject(oracle(),replace(sample(0),**{field:None}),'records')
        instance=oracle()
        for slot in range(91): instance.accept(sample(slot))
        self.reject(instance,sample(91),'consecutive')

    def test_bunched_slots_can_pass_transport_but_never_cadence(self):
        instance=oracle(); instance.accept(sample(0))
        instance.accept(sample(1,arm=1090,end=1120))
        instance.accept(sample(2,arm=1121,end=1151))  # Only31ms apart, not10Hz cadence.
        for slot in range(3,91): instance.accept(sample(slot))
        report=instance.finish(normal_cleanup=True)
        self.assertTrue(report['synthetic_transport_budget_pass'])
        self.assertFalse(report['cadence_admitted'])

    def test_nondivisible_frequency_uses_fixed_rational_schedule(self):
        instance=SequenceOracle(NONCE,frequency=97,begin_qpc=1,schedule_origin=20)
        for slot in range(91):
            arm=1 if slot==0 else 20+(slot-1)*97//10
            identity=Identity(NONCE,slot+1,slot+1)
            instance.accept(Sample(slot,97,arm,arm+1,arm+4,
                            Publication(identity,arm+2,arm+2,arm+3,True,True),
                            Observation(1,arm+5,arm+6,encode(identity),None)))
        self.assertTrue(instance.finish(normal_cleanup=True)['fixture_content_sequence_pass'])

    def test_successful_prefix_does_not_hide_failed_last_frame(self):
        instance=oracle()
        for slot in range(90): instance.accept(sample(slot))
        value=sample(90)
        self.reject(instance,replace(value,observation=replace(value.observation,
                    pixels=encode(Identity(NONCE,90,90)))),'identity')

    def test_invalid_construction_clock_and_overflow(self):
        for kwargs in ({'frequency':True},{'frequency':9},{'frequency':10**9+1},
                       {'begin_qpc':-1},{'schedule_origin':89},
                       {'schedule_origin':5091},{'begin_qpc':2**64-1,'schedule_origin':2**64-1}):
            args=dict(frequency=1000,begin_qpc=90,schedule_origin=1000); args.update(kwargs)
            with self.assertRaises(ValueError): SequenceOracle(NONCE,**args)


if __name__ == '__main__': unittest.main()
