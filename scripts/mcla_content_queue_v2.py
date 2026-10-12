"""Pure three-publication checkout journal verifier, not a native admission.

Selection is validated completely before reading supplied pixel samples. This
does not authenticate a journal or prove native resource identities. No drain,
retry, timestamp-based freshness or content-dependent frame selection exists.
"""
from dataclasses import dataclass

from mcla_fixture_identity_v2 import FixtureModel, Identity, Publication, Observation, need, uint
from mcla_content_sequence_v2 import Sample

NATIVE_ADMITTED = False
PUBLICATIONS = 3
MAX_POLLS = 1000


@dataclass(frozen=True)
class Event:
    kind: str
    tick: int
    slot: int
    checkout: int
    session: int
    pool: int
    # Held process birth identity and exact window; declarations, not OS proof.
    owner: tuple


def validate(events, samples, nonce, *, normal_cleanup):
    """One immutable complete journal and three samples, replayed after exit.

    Failed calls return no partial approval. There is no reusable live selector;
    the future native caller must terminate its attempt on any exception.
    """
    need(type(events) is tuple and 17 <= len(events) <= 17 + 3*MAX_POLLS,
         'bounded immutable journal required')
    need(type(normal_cleanup) is bool and normal_cleanup, 'normal cleanup required')
    first = events[0]
    need(type(first) is Event and first.kind == 'create', 'single initial create required')
    uint(first.session, positive=True); uint(first.pool, positive=True)
    need(type(first.owner) is tuple and len(first.owner) == 3, 'owner identity required')
    for part in first.owner: uint(part, positive=True)
    previous_tick = None
    slot, state, polls = 0, 'create', 0
    records, current = [], {}
    for event in events:
        need(type(event) is Event and type(event.kind) is str, 'exact journal event required')
        uint(event.tick, positive=True); uint(event.slot); uint(event.checkout)
        need(type(event.session) is int and type(event.pool) is int and
             type(event.owner) is tuple and
             (event.session, event.pool, event.owner) == (first.session, first.pool, first.owner),
             'persistent resource or ownership drift')
        for part in event.owner: uint(part, positive=True)
        need(previous_tick is None or event.tick > previous_tick, 'strict journal ordering')
        previous_tick = event.tick
        need(event.slot == slot, 'consecutive journal slots')
        if state == 'create':
            need(event.kind == 'create' and event.checkout == 0, 'one create required')
            state = 'arm'
        elif state == 'arm':
            need(event.kind == 'arm' and event.checkout == 0, 'one arm per slot required')
            current = {'arm': event.tick}; polls = 0; state = 'ack'
        elif state == 'ack':
            need(event.kind == 'ack' and event.checkout == 0, 'publication ack required')
            current['ack'] = event.tick; state = 'poll'
        elif state == 'poll':
            need(event.kind in ('null', 'checkout'), 'first non-null checkout required')
            polls += 1
            need(polls <= MAX_POLLS, 'bounded polls; null never resets budget')
            if event.kind == 'null':
                need(event.checkout == 0, 'null cannot carry a frame')
            else:
                need(event.checkout == slot+1, 'unique checkout ordinal required')
                current['checkout'] = event.tick; state = 'read'
        elif state == 'read':
            need(event.kind == 'read' and event.checkout == slot+1, 'one first-frame read required')
            current['read'] = event.tick; state = 'release'
        elif state == 'release':
            need(event.kind == 'release' and event.checkout == slot+1, 'frame release required')
            current['release'] = event.tick; records.append(current)
            slot += 1; state = 'close' if slot == PUBLICATIONS else 'arm'
        elif state == 'close':
            need(event.kind == 'close' and event.checkout == 0, 'one final normal close required')
            state = 'done'
        else:
            raise ValueError('extra journal event after close')
    need(state == 'done', 'incomplete resource journal')

    # Complete every metadata/binding check before examining any content.
    need(type(samples) is tuple and len(samples) == PUBLICATIONS, 'three immutable samples required')
    frequency = None
    for index, (record, sample) in enumerate(zip(records, samples)):
        need(type(sample) is Sample and type(sample.slot) is int and sample.slot == index,
             'consecutive content slots')
        need(type(sample.publication) is Publication and type(sample.observation) is Observation,
             'exact nested sample records required')
        for tick in (sample.arm_qpc,sample.issue_before_qpc,sample.issue_after_qpc): uint(tick)
        need(sample.publication.identity == Identity(nonce,index+1,index+1),
             'consecutive publication identity required')
        need(type(sample.frequency) is int and 10 <= sample.frequency <= 1_000_000_000,
             'bounded frequency required')
        if frequency is None: frequency = sample.frequency
        need(sample.frequency == frequency, 'frequency drift')
        need(sample.arm_qpc == record['arm'] and sample.issue_after_qpc == record['ack'],
             'sample and journal arm ack mismatch')
        need(record['ack'] < record['checkout'] <= sample.observation.before_qpc <=
             sample.observation.after_qpc <= record['read'] < record['release'],
             'readback must belong to selected checkout')
        need(sample.publication.cleanup_after_qpc <= record['ack'], 'cleanup after ack')
        need(record['release']-record['arm'] < 5*frequency,
             'fixed slot deadline; null never resets deadline')
        need(type(sample.issue_before_qpc) is int and
             record['arm'] < sample.issue_before_qpc <= record['ack'], 'ordered issue bracket')
        need(sample.publication.draw_before_qpc >= sample.issue_before_qpc and
             sample.publication.draw_ok and sample.publication.cleanup_ok,
             'successful ordered publication required')
        need(sample.observation.index == 1, 'first observation only')
    need(events[-1].tick-events[0].tick < 20*frequency, 'fixed whole-run deadline')
    # No content or opaque WGC label influenced checkout selection. A stale
    # first frame fails here, with no later matching-frame substitution.
    model = FixtureModel(nonce)
    for sample in samples:
        model.arm(sample.arm_qpc, sample.issue_before_qpc, sample.issue_after_qpc)
        model.complete(sample.publication)
        model.observe(sample.observation)
    return {'schema': 'mcla-content-queue-model-v2', 'publications': PUBLICATIONS,
            'checkout_journal_pass': True, 'fixture_content_pass': True,
            'native_admitted': False, 'cadence_admitted': False, 'fps_proven': False}
