"""Offline fixture-only content identity model; NOT a native capture admission.

No imports of Win32/capture/game adapters, IO, clock reads or randomness. A future
reviewed adapter must supply a fresh session nonce, measured producer records,
owned window pixels and the first eligible frame. WGC time is opaque metadata:
this model never derives compositor time, FPS or game simulation correctness.
Frozen v1 capture/extractor gates are intentionally untouched.
"""
from dataclasses import dataclass
import struct
import zlib

SCHEMA = 'mcla-fixture-content-v2'
NATIVE_ADMITTED = False
WIDTH, HEIGHT = 1280, 720
COLS, ROWS = 32, 10
CELL_W, CELL_H = WIDTH // COLS, HEIGHT // ROWS
BLACK, WHITE = bytes((24, 24, 24, 255)), bytes((232, 232, 232, 255))
U64_MAX = (1 << 64) - 1
MAX_PUBLICATIONS = 1024
_PACKET = struct.Struct('>4s16sQQ')


def need(condition, reason):
    if not condition:
        raise ValueError(reason)


def uint(value, *, positive=False):
    need(type(value) is int and int(positive) <= value <= U64_MAX,
         'unsigned nonwrapping integer required')


@dataclass(frozen=True)
class Identity:
    session: bytes
    challenge: int
    paint: int

    def __post_init__(self):
        need(type(self.session) is bytes and len(self.session) == 16 and any(self.session),
             'nonzero 128-bit session nonce required')
        uint(self.challenge, positive=True)
        uint(self.paint, positive=True)


def encode(identity):
    """Canonical full-client BGRA grid; CRC detects corruption, not forgery."""
    need(type(identity) is Identity, 'identity required')
    packet = _PACKET.pack(b'MFV2', identity.session, identity.challenge, identity.paint)
    packet += struct.pack('>I', zlib.crc32(packet))
    bits = [(byte >> bit) & 1 for byte in packet for bit in range(7, -1, -1)]
    need(len(bits) == COLS * ROWS, 'fixed payload size')
    lines = []
    for row in range(ROWS):
        line = b''.join((WHITE if bit else BLACK) * CELL_W
                        for bit in bits[row * COLS:(row + 1) * COLS])
        lines.append(line * CELL_H)
    return b''.join(lines)


def decode(pixels):
    need(type(pixels) is bytes and len(pixels) == WIDTH * HEIGHT * 4,
         'exact full-client BGRA payload required')
    bits = []
    for row in range(ROWS):
        for col in range(COLS):
            offset = (row * CELL_H * WIDTH + col * CELL_W) * 4
            pixel = pixels[offset:offset + 4]
            need(pixel in (BLACK, WHITE), 'unknown palette or alpha')
            bits.append(int(pixel == WHITE))
    packet = bytes(sum(bits[start + bit] << (7 - bit) for bit in range(8))
                   for start in range(0, len(bits), 8))
    body, checksum = packet[:-4], packet[-4:]
    need(checksum == struct.pack('>I', zlib.crc32(body)), 'identity checksum mismatch')
    magic, session, challenge, paint = _PACKET.unpack(body)
    need(magic == b'MFV2', 'unknown pixel identity schema')
    identity = Identity(session, challenge, paint)
    # Every byte, including interiors, edges and alpha must match. A marker-only
    # match does not approve mixed pixels outside the marker or sampled centers.
    need(pixels == encode(identity), 'mixed or noncanonical full-grid pixels')
    return identity


@dataclass(frozen=True)
class Challenge:
    identity: Identity
    arm_qpc: int
    issue_before_qpc: int
    issue_after_qpc: int

    def __post_init__(self):
        need(type(self.identity) is Identity, 'identity required')
        for value in (self.arm_qpc, self.issue_before_qpc, self.issue_after_qpc):
            uint(value)
        need(self.arm_qpc < self.issue_before_qpc <= self.issue_after_qpc,
             'challenge must be issued strictly after arm')


@dataclass(frozen=True)
class Publication:
    identity: Identity
    draw_before_qpc: int
    draw_after_qpc: int
    cleanup_after_qpc: int
    draw_ok: bool
    cleanup_ok: bool

    def __post_init__(self):
        need(type(self.identity) is Identity, 'identity required')
        for value in (self.draw_before_qpc, self.draw_after_qpc, self.cleanup_after_qpc):
            uint(value)
        need(self.draw_before_qpc <= self.draw_after_qpc <= self.cleanup_after_qpc,
             'reordered producer bracket')
        need(type(self.draw_ok) is bool and type(self.cleanup_ok) is bool,
             'explicit producer outcomes required')


@dataclass(frozen=True)
class Observation:
    index: int
    before_qpc: int
    after_qpc: int
    pixels: bytes
    raw_wgc_100ns: int | None = None

    def __post_init__(self):
        uint(self.index, positive=True)
        uint(self.before_qpc)
        uint(self.after_qpc)
        need(self.before_qpc <= self.after_qpc, 'reordered capture bracket')
        need(self.raw_wgc_100ns is None or (type(self.raw_wgc_100ns) is int and
             -(1 << 63) <= self.raw_wgc_100ns < (1 << 63)), 'invalid raw WGC label')


class FixtureModel:
    """Single-session sequential mock; any rejected operation poisons the run.

    An adapter must make observation1 the first eligible frame, not a selected
    later success. This offline model cannot attest the adapter or OS behavior.
    IDs are reserved before producer work and never reused, even after failure.
    One challenge permits exactly one completed publication and observation.
    """
    def __init__(self, session):
        Identity(session, 1, 1)
        self.session = session
        self.count = 0
        self.last_after_qpc = None
        self.challenge = None
        self.publication = None
        self.failed = False

    def _run(self, operation):
        need(not self.failed, 'run already failed; no retry or favorable-frame selection')
        try:
            return operation()
        except (ValueError, TypeError, AttributeError, struct.error):
            self.failed = True
            raise

    def arm(self, arm_qpc, issue_before_qpc, issue_after_qpc):
        def operation():
            need(self.challenge is None, 'previous challenge not consumed')
            need(self.count < MAX_PUBLICATIONS, 'publication budget exhausted; no wrap')
            if self.last_after_qpc is not None:
                need(arm_qpc > self.last_after_qpc, 'reordered host records')
            identity = Identity(self.session, self.count + 1, self.count + 1)
            challenge = Challenge(identity, arm_qpc, issue_before_qpc, issue_after_qpc)
            self.count += 1
            self.challenge = challenge
            return challenge
        return self._run(operation)

    def complete(self, publication):
        def operation():
            need(type(publication) is Publication, 'producer record required')
            need(self.challenge is not None and self.publication is None,
                 'exactly one pending producer record required')
            need(publication.identity == self.challenge.identity, 'wrong or stale publication identity')
            need(publication.draw_before_qpc >= self.challenge.issue_before_qpc,
                 'producer predates challenge issue')
            # Producer may draw while the host issue call is still in progress.
            # No unsupported issue_after <= draw_before inequality is imposed.
            need(publication.draw_ok and publication.cleanup_ok, 'drawing or cleanup failed')
            self.publication = publication
        return self._run(operation)

    def observe(self, observation):
        def operation():
            need(type(observation) is Observation, 'observation required')
            need(self.challenge is not None and self.publication is not None,
                 'completed producer record required before content credit')
            need(observation.index == 1, 'first eligible frame only; no frame skipping')
            need(observation.before_qpc >= max(self.challenge.issue_after_qpc,
                                              self.publication.cleanup_after_qpc),
                 'observation predates completed producer/host issue')
            identity = decode(observation.pixels)
            need(identity == self.challenge.identity, 'wrong session or stale pixel identity')
            result = {'schema': SCHEMA, 'identity': identity,
                      'raw_wgc_100ns': observation.raw_wgc_100ns,
                      'fixture_content_model_pass': True,
                      'native_capture_admitted': False, 'compositor_time_proven': False,
                      'guest_timing_proven': False, 'fps_proven': False}
            self.last_after_qpc = observation.after_qpc
            self.challenge = self.publication = None
            return result
        return self._run(operation)
