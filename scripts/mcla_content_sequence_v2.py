"""Offline, bounded synthetic sequence oracle; never a native admission.

Replay only, outside the native timed path. Each Sample must represent the first
eligible observation after one completed publication; a future native adapter
must prove that selection and ownership separately. The oracle has no IO,
clock reads, native imports, sleeps, randomness or execution entry point.

Source-content uncertainty is conservatively bounded by producer draw begin
and host read completion, not the opaque WGC timestamp. These are synthetic
transport budgets, not compositor cadence or passive game's sample times.
"""
from dataclasses import dataclass

from mcla_fixture_identity_v2 import (
    FixtureModel, Identity, Observation, Publication, need, uint,
)

NATIVE_ADMITTED = False
TIMED_SAMPLES = 90
TOTAL_SAMPLES = 1 + TIMED_SAMPLES
HZ = 10


@dataclass(frozen=True)
class Sample:
    # Validation lives in accept so every malformed submitted record poisons
    # the sequence, including records with malformed nested values.
    slot: int
    frequency: int
    arm_qpc: int
    issue_before_qpc: int
    issue_after_qpc: int
    publication: Publication
    observation: Observation


class SequenceOracle:
    def __init__(self, session, *, frequency, begin_qpc, schedule_origin):
        Identity(session, 1, 1)
        need(type(frequency) is int and HZ <= frequency <= 1_000_000_000,
             'bounded sequence frequency')
        uint(begin_qpc, positive=True); uint(schedule_origin, positive=True)
        need(begin_qpc <= schedule_origin <= begin_qpc + 5*frequency,
             'schedule origin after begin and bounded warmup')
        uint(begin_qpc + 15*frequency); uint(schedule_origin + 12*frequency)
        self._frequency, self._begin, self._origin = frequency, begin_qpc, schedule_origin
        self._model = FixtureModel(session)
        self._count = 0
        self._terminal = False
        self._previous_end = None
        self._previous_timed_lower = None
        self._max_window = self._max_gap = 0

    def accept(self, value):
        """Consumes exactly the next slot; any failure is permanent.

        Caller records are untrusted evidence. This models their consistency,
        not authentic native execution. Accepted pixel buffers are not retained.
        """
        need(not self._terminal, 'terminal sequence; no retry or extra records')
        self._terminal = True  # Reset only after every validation succeeds.
        need(type(value) is Sample, 'exact sequence sample required')
        need(type(value.slot) is int and value.slot == self._count < TOTAL_SAMPLES,
             'consecutive bounded slots including warmup')
        need(type(value.frequency) is int and value.frequency == self._frequency,
             'sequence frequency drift')
        for tick in (value.arm_qpc, value.issue_before_qpc, value.issue_after_qpc): uint(tick)
        need(type(value.publication) is Publication and type(value.observation) is Observation,
             'exact publication and observation records required')
        pub, obs, frequency = value.publication, value.observation, self._frequency
        need(value.arm_qpc >= self._begin, 'arm before helper begin')
        need(self._previous_end is None or self._previous_end <= value.arm_qpc,
             'sequential sample overlap')
        need(pub.cleanup_after_qpc <= value.issue_after_qpc,
             'producer cleanup after synchronous ack')
        need(obs.after_qpc < self._begin + 15*frequency, 'overall sequence deadline')
        if value.slot == 0:
            need(obs.after_qpc <= self._origin and obs.after_qpc-value.arm_qpc < 5*frequency,
                 'warmup must complete before schedule origin within budget')
        else:
            # Rational fixed schedule: no sliding origin, early arm or catchup.
            scheduled = self._origin + (value.slot-1)*frequency//HZ
            next_scheduled = self._origin + value.slot*frequency//HZ
            need(scheduled <= value.arm_qpc < next_scheduled, 'fixed schedule violation')
            need(obs.after_qpc < self._origin + 12*frequency, 'timed sequence deadline')
            window = obs.after_qpc - pub.draw_before_qpc
            need(window >= 0 and window*5 < frequency, 'source window must be below 200ms')
            self._max_window = max(self._max_window, window)
            if self._previous_timed_lower is not None:
                gap = obs.after_qpc - self._previous_timed_lower
                need(gap >= 0 and gap*5 < frequency, 'conservative gap must be below 200ms')
                self._max_gap = max(self._max_gap, gap)

        # Reuses the independently tested whole-grid/session/nonwrapping identity
        # oracle. Raw WGC labels cannot rescue or reject these host/content bounds.
        self._model.arm(value.arm_qpc, value.issue_before_qpc, value.issue_after_qpc)
        self._model.complete(pub)
        self._model.observe(obs)
        self._previous_end = obs.after_qpc
        if value.slot:
            self._previous_timed_lower = pub.draw_before_qpc
        self._count += 1
        self._terminal = False

    def finish(self, *, normal_cleanup):
        need(not self._terminal, 'terminal sequence; no retry or repeated finish')
        self._terminal = True
        need(self._count == TOTAL_SAMPLES, 'complete warmup plus 90 timed samples required')
        need(type(normal_cleanup) is bool and normal_cleanup, 'normal cleanup must be reported')
        return {
            'schema': 'mcla-fixture-sequence-model-v2',
            'publications': self._count, 'timed_publications': TIMED_SAMPLES,
            'frequency': self._frequency,
            'max_source_window_ticks': self._max_window,
            'max_conservative_gap_ticks': self._max_gap,
            'fixture_content_sequence_pass': True,
            'synthetic_transport_budget_pass': True,
            'native_admitted': False, 'cadence_admitted': False, 'compositor_time_proven': False,
            'game_timing_proven': False, 'fps_proven': False,
        }
