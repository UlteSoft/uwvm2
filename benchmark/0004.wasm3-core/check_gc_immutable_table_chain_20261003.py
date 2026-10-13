#!/usr/bin/env python3
"""Pure JSON semantic adapter for a keeper-owned component, never a launcher.

An accepted JSON row does not authenticate its producer, binary, source,
provider, UID/TID, cgroup, pause, PMU, or generated VM code. Those boundaries
remain the existing actual keeper/guardian's independent responsibility.
"""
import importlib.util
from pathlib import Path

_ORACLE_PATH = Path(__file__).with_name('prepare_gc_immutable_table_chain_20261003.py')
_SPEC = importlib.util.spec_from_file_location('immutable_chain_oracle_20261003', _ORACLE_PATH)
if _SPEC is None or _SPEC.loader is None:
    raise ValueError('Missing the exact immutable table-chain oracle')
_ORACLE = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(_ORACLE)

_KEYS = frozenset((
    'family', 'collection_mode', 'count', 'single_cas', 'state_u32',
    'root_checksum_u32', 'chain_ns', 'collection_ns', 'maximum_collection_ns',
    'timed_collections', 'timed_reclaimed', 'qualification_reclaimed',
    'actual_table_slots', 'actual_static_root_visitor',
    'component_storage_chain_only', 'vm_qualified',
))


def require(condition, message):
    if not condition:
        raise ValueError(message)


def semantic_receipt(payload, *, count, mode, single_cas):
    """Check one already decoded row against independent planned controls.

    This function performs no IO or native execution and grants no permission
    to reuse an old runtime object/ELF/provider or an unobserved OS process.
    """
    require(type(single_cas) is bool, 'Expected publication macro must be an exact bool')
    expected = _ORACLE.oracle(count, mode)
    require(type(payload) is dict and set(payload) == _KEYS, 'Unexpected or incomplete immutable-chain schema')
    require(payload['family'] == 'gc-immutable-table-chain' and
            payload['collection_mode'] == mode, 'Wrong component family/collection policy')
    require(type(payload['single_cas']) is bool and payload['single_cas'] is single_cas,
            'Actual publication witness differs from the coherent compiled profile')
    for name, wanted in (
        ('count', count), ('state_u32', expected['state_u32']),
        ('root_checksum_u32', expected['root_checksum_u32']),
        ('timed_collections', expected['timed_collections']),
        ('timed_reclaimed', expected['timed_reclaimed']),
        ('qualification_reclaimed', expected['qualification_reclaimed']),
        ('actual_table_slots', 1024),
    ):
        require(type(payload[name]) is int and payload[name] == wanted,
                'Immutable-chain mismatch: ' + name)
    require(payload['timed_reclaimed'] + payload['qualification_reclaimed'] == count,
            'Exact total reclamation does not equal allocations')
    for name in ('actual_static_root_visitor', 'component_storage_chain_only'):
        require(type(payload[name]) is bool and payload[name] is True,
                'Missing component source-path witness: ' + name)
    require(type(payload['vm_qualified']) is bool and payload['vm_qualified'] is False,
            'A storage component cannot qualify automatic VM roots/JIT code')
    for name in ('chain_ns', 'collection_ns', 'maximum_collection_ns'):
        require(type(payload[name]) is int and 0 <= payload[name] < (1 << 64),
                'Invalid unsigned native clock carrier: ' + name)
    require(payload['maximum_collection_ns'] <= payload['collection_ns'],
            'Collection maximum exceeds the sum')
    if payload['timed_collections'] == 0:
        require(payload['collection_ns'] == payload['maximum_collection_ns'] == 0,
                'No timed collection can have a nonzero measured collection interval')
    return dict(schema='gc-immutable-table-chain-20261003-semantic-v1',
        semantic_row_matches=True, count=count, mode=mode, single_cas=single_cas,
        exact_total_reclaimed=count, state_u32=expected['state_u32'],
        root_checksum_u32=expected['root_checksum_u32'],
        original_wasm_identity=expected['original_wasm_identity'],
        source_or_producer_authenticated_by_this_adapter=False,
        native_execution_authenticated_by_this_adapter=False,
        actual_provider=None, actual_cgroup=None, actual_process_identity=None,
        performance_accepted=False, vm_qualified=False)


def pure_schema_controls():
    """Synthetic data controls only; none is an actual native receipt."""
    rows, rejected = 0, 0
    for count in (1024, 4096, 16_000_000):
        for mode in _ORACLE.MODES:
            expected = _ORACLE.oracle(count, mode)
            for single_cas in (False, True):
                row = dict(family='gc-immutable-table-chain', collection_mode=mode,
                    count=count, single_cas=single_cas, state_u32=expected['state_u32'],
                    root_checksum_u32=expected['root_checksum_u32'], chain_ns=1,
                    collection_ns=expected['timed_collections'],
                    maximum_collection_ns=int(expected['timed_collections'] != 0),
                    timed_collections=expected['timed_collections'],
                    timed_reclaimed=expected['timed_reclaimed'],
                    qualification_reclaimed=expected['qualification_reclaimed'],
                    actual_table_slots=1024, actual_static_root_visitor=True,
                    component_storage_chain_only=True, vm_qualified=False)
                kwargs = dict(count=count, mode=mode, single_cas=single_cas)
                semantic_receipt(row, **kwargs)
                rows += 1
                for key, value in (
                    ('single_cas', not single_cas), ('state_u32', expected['state_u32'] ^ 1),
                    ('root_checksum_u32', expected['root_checksum_u32'] ^ 1),
                    ('timed_reclaimed', expected['timed_reclaimed'] + 1),
                    ('qualification_reclaimed', expected['qualification_reclaimed'] + 1),
                    ('actual_table_slots', 1034), ('actual_static_root_visitor', False),
                    ('vm_qualified', True), ('count', True), ('chain_ns', -1),
                ):
                    changed = dict(row)
                    changed[key] = value
                    try:
                        semantic_receipt(changed, **kwargs)
                    except ValueError:
                        rejected += 1
                    else:
                        raise ValueError('Malformed synthetic row passed: ' + key)
    return dict(synthetic_valid_rows=rows, synthetic_invalid_rows_rejected=rejected,
        synthetic_only=True, native_executed=False, performance_accepted=False,
        vm_qualified=False)
