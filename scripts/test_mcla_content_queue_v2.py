"""Selection tests use synthetic journals only. No capture or native imports."""
from dataclasses import replace
import unittest
from unittest.mock import patch

from mcla_content_queue_v2 import Event, validate, NATIVE_ADMITTED, MAX_POLLS
from mcla_content_sequence_v2 import Sample
from mcla_fixture_identity_v2 import Identity, Publication, Observation, encode

NONCE = bytes(range(16))


def event(kind, tick, slot=0, checkout=0):
    return Event(kind, tick, slot, checkout, 123, 456, (100, 200, 300))


def fixture():
    events = [event('create', 1)]
    samples = []
    for slot in range(3):
        arm = 100+slot*100
        identity = Identity(NONCE, slot+1, slot+1)
        samples.append(Sample(slot, 1000, arm, arm+1, arm+10,
                       Publication(identity, arm+2, arm+5, arm+8, True, True),
                       Observation(1, arm+12, arm+18, encode(identity), -42)))
        events.extend((event('arm', arm, slot), event('ack', arm+10, slot),
                       event('checkout', arm+11, slot, slot+1),
                       event('read', arm+20, slot, slot+1),
                       event('release', arm+21, slot, slot+1)))
    events.append(event('close', 400, 3))
    return tuple(events), tuple(samples)


class QueueTests(unittest.TestCase):
    def test_three_publications_one_session_no_native_or_cadence_claim(self):
        report = validate(*fixture(), NONCE, normal_cleanup=True)
        self.assertTrue(report['checkout_journal_pass'])
        self.assertTrue(report['fixture_content_pass'])
        self.assertFalse(NATIVE_ADMITTED)
        for key in ('native_admitted', 'cadence_admitted', 'fps_proven'):
            self.assertIs(report[key], False)

    def reject(self, events, samples, reason='.'):
        with self.assertRaisesRegex((ValueError, TypeError, AttributeError), reason):
            validate(tuple(events), tuple(samples), NONCE, normal_cleanup=True)

    def test_stale_first_current_second_is_not_a_selection_algorithm(self):
        events, samples = fixture()
        stale = replace(samples[1], observation=replace(samples[1].observation,
                         pixels=samples[0].observation.pixels))
        self.reject(events, (samples[0], stale, samples[2]), 'identity')
        # A replacement checkout is rejected before its content is inspected.
        extra = event('checkout', 212, 1, 3)
        self.reject(events[:9]+(extra,)+events[9:], samples, 'one first-frame')

    def test_null_does_not_prove_freshness(self):
        events, samples = fixture()
        events = events[:8]+(event('null', 211, 1), replace(events[8], tick=212))+events[9:]
        validate(events, samples, NONCE, normal_cleanup=True)
        stale = replace(samples[1], observation=replace(samples[1].observation,
                         pixels=samples[0].observation.pixels))
        self.reject(events, (samples[0], stale, samples[2]), 'identity')

    def test_duplicate_or_unsolicited_publication_fails(self):
        events, samples = fixture()
        for identity in (Identity(NONCE,1,1), Identity(NONCE,2,3)):
            changed = replace(samples[1], publication=replace(samples[1].publication, identity=identity),
                              observation=replace(samples[1].observation, pixels=encode(identity)))
            self.reject(events, (samples[0], changed, samples[2]), 'identity')

    def test_no_recreate_drain_skip_or_missing_release(self):
        events, samples = fixture()
        for index, kind in ((6,'create'), (3,'drain'), (4,'checkout'), (5,'arm')):
            changed = list(events); changed[index] = replace(changed[index], kind=kind)
            self.reject(changed, samples)
        self.reject(events[:-1], samples, 'bounded immutable')
        self.reject(events[:3]+(event('null',111),)+events[3:-1], samples)
        self.reject(events+(event('close',401,3),), samples, 'extra')

    def test_owner_pool_session_checkout_and_clock_drift(self):
        events, samples = fixture()
        for field, value in (('session',124),('pool',457),('owner',(100,201,300)),
                             ('checkout',2),('tick',110),('slot',1)):
            changed = list(events); changed[3] = replace(changed[3], **{field:value})
            self.reject(changed, samples)

    def test_failed_final_cleanup_with_correct_pixels_fails(self):
        for cleanup in (False, None, 1):
            with self.assertRaisesRegex(ValueError, 'cleanup'):
                validate(*fixture(), NONCE, normal_cleanup=cleanup)

    def test_selection_checked_before_pixel_objects_are_touched(self):
        events, _ = fixture()
        class Forbidden:
            def __getattribute__(self, name):
                raise AssertionError('pixels touched before selection validation')
        changed = list(events); changed[4] = replace(changed[4], kind='checkout')
        self.reject(changed, (Forbidden(),)*3, 'one first-frame')

    def test_final_binding_and_deadline_checked_before_any_decode(self):
        events,samples=fixture()
        bad_sample=samples[:2]+(replace(samples[2],issue_after_qpc=311),)
        with patch('mcla_fixture_identity_v2.decode',side_effect=AssertionError('early decode')):
            self.reject(events,bad_sample,'mismatch')
            self.reject(events[:-1]+(event('close',20001,3),),samples,'whole-run deadline')
            self.reject(events,samples[:2]+(replace(samples[2],publication=None),),'nested')
            for field in ('arm_qpc','issue_before_qpc','issue_after_qpc'):
                changed=replace(samples[2],**{field:float(getattr(samples[2],field))})
                self.reject(events,samples[:2]+(changed,),'unsigned')

    def test_sample_metadata_cannot_disagree_with_journal(self):
        events, samples = fixture()
        for field, value in (('arm_qpc',101),('issue_after_qpc',111),('frequency',999),('slot',2)):
            changed = replace(samples[1], **{field:value})
            self.reject(events, (samples[0],changed,samples[2]))
        for before, after in ((210,218),(212,221)):
            changed = replace(samples[1], observation=replace(samples[1].observation,
                              before_qpc=before, after_qpc=after))
            self.reject(events, (samples[0],changed,samples[2]), 'readback')

    def test_too_many_nulls_cannot_extend_poll_budget(self):
        events, samples = fixture()
        # Use wide ticks so the trace fails the poll budget, not event ordering.
        events = events[:3]+tuple(event('null',111+i) for i in range(MAX_POLLS))+(
                    event('checkout', 111+MAX_POLLS,0,1),)+events[4:]
        self.reject(events, samples, 'bounded polls')

    def test_nulls_do_not_reset_slot_deadline(self):
        events, samples = fixture()
        # The last frame arrives late even though all earlier slots were good.
        tail=(event('null',311,2),event('checkout',5290,2,3),
              event('read',5299,2,3),event('release',5300,2,3),event('close',5301,3))
        late=replace(samples[2],observation=replace(samples[2].observation,
                     before_qpc=5291,after_qpc=5298))
        self.reject(events[:13]+tail,samples[:2]+(late,),'fixed slot deadline')

    def test_later_observation_and_corrupt_full_grid_fail(self):
        events,samples=fixture()
        for observation in (replace(samples[2].observation,index=2),
                            replace(samples[2].observation,pixels=samples[2].observation.pixels[:-1]+b'\0')):
            self.reject(events,samples[:2]+(replace(samples[2],observation=observation),))

    def test_session_cleanup_time_stays_bounded(self):
        events,samples=fixture()
        self.reject(events[:-1]+(event('close',20001,3),),samples,'whole-run deadline')

    def test_malformed_and_mutable_journals_rejected(self):
        events, samples = fixture()
        for values in (list(events), (), None):
            with self.assertRaises(ValueError): validate(values,samples,NONCE,normal_cleanup=True)
        for field, value in (('tick',True),('slot',False),('checkout',False),
                             ('owner',(100,True,300)),('session',True)):
            changed = list(events); changed[3] = replace(changed[3], **{field:value})
            self.reject(changed,samples)


if __name__ == '__main__': unittest.main()
