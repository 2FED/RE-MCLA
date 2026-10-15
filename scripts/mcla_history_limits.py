"""Bounded reader contracts for legacy and enlarged native history journals.

Accepting a declared capacity never establishes coverage or removes a loss.
Legacy fixture defaults remain 4096; each input's declaration bounds its rows.
"""
LEGACY_SEMAPHORE_CAPACITY = 4096
SEMAPHORE_CAPACITY = 32768
SUPPORTED_SEMAPHORE_CAPACITIES = frozenset({LEGACY_SEMAPHORE_CAPACITY, SEMAPHORE_CAPACITY})
SEMAPHORE_MAX_BYTES = 32 * 1024 * 1024
HANDOFF_MAX_BYTES = 48 * 1024 * 1024
HANDOFF_SCOPE_CAPACITY = 2048
HANDOFF_EVENT_CAPACITY = 65536


def semaphore_capacity(value):
    if type(value) is not int or value not in SUPPORTED_SEMAPHORE_CAPACITIES:
        raise ValueError('supported exact semaphore capacity')
    return value
