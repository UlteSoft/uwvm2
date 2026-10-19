"""Stock CPython 3.14.8 stdlib operations, separate from the frozen fixtures.

Run the exact source with ``python -S -B -X utf8 -c SOURCE π 你好 '' 'space value'``
or as a script with those four arguments. The harness supplies the complete
stdlib/sysconfig under its explicit /python preopen, records this source's SHA,
and compares complete stdout with a matching native oracle. This guest does not
read a host path or infer its source hash from an unverified runtime label.

Each probe reports its own failure. A queue import failure is a queue failure,
not a skipped PASS or proof that unrelated Python operations are unavailable.
The tracing probe uses real Python line/frame events; it is not a WasmDBG bridge.
No native thread, process, socket or complete asyncio operation is tested.
"""
import sys


def _check(condition, message):
    if not condition:
        raise AssertionError(message)


def _quote(text):
    # Use builtins for the report envelope so a broken/missing json module is
    # still reported alongside the other independent probes. The json probe
    # below separately exercises the genuine standard-library implementation.
    result = ['"']
    escapes = {'"': '\\"', '\\': '\\\\', '\b': '\\b', '\f': '\\f',
               '\n': '\\n', '\r': '\\r', '\t': '\\t'}
    for character in text:
        code = ord(character)
        if character in escapes:
            result.append(escapes[character])
        elif code < 32 or 0xD800 <= code <= 0xDFFF:
            result.append('\\u%04x' % code)
        else:
            result.append(character)
    result.append('"')
    return ''.join(result)


def _canonical(value):
    if value is None:
        return 'null'
    if value is True:
        return 'true'
    if value is False:
        return 'false'
    if isinstance(value, str):
        return _quote(value)
    if isinstance(value, int):
        return str(value)
    if isinstance(value, (list, tuple)):
        return '[' + ','.join(_canonical(item) for item in value) + ']'
    if isinstance(value, dict) and all(isinstance(key, str) for key in value):
        return '{' + ','.join(_quote(key) + ':' + _canonical(value[key])
                              for key in sorted(value)) + '}'
    raise TypeError('unsupported report value type: ' + type(value).__name__)


def _runtime_probe():
    _check(sys.implementation.name == 'cpython', 'requires genuine CPython')
    _check(tuple(sys.version_info[:3]) == (3, 14, 8), 'requires pinned CPython 3.14.8')
    return {'implementation': sys.implementation.name, 'version': list(sys.version_info[:3])}


def _args_env_probe():
    import os
    expected_args = ['π', '你好', '', 'space value']
    expected_env = {'UWVM_PY_AUX': 'two-你好', 'UWVM_PY_EMPTY': '',
                    'UWVM_PY_TEST': 'python-π'}
    _check(sys.argv[1:] == expected_args, 'complete UTF-8/empty argv mismatch')
    observed = {name: os.environ[name] for name in expected_env}
    _check(observed == expected_env, 'explicit environment values mismatch')
    _check(os.environ.get('UWVM_PY_MISSING_20261018') is None, 'missing environment key unexpectedly present')
    # PYTHONHOME/PYTHONPATH deliberately differ between the native and guest
    # filesystem layouts. Neither their values nor host paths enter stdout.
    return {'argv': list(sys.argv[1:]), 'environment': observed, 'missing': True}


def _dataclasses_probe():
    import dataclasses

    @dataclasses.dataclass
    class Point:
        value: int
        tags: list = dataclasses.field(default_factory=list)

    point, other = Point(7), Point(8)
    point.tags.append('雪')
    changed = dataclasses.replace(point, value=12)
    fields = dataclasses.fields(Point)
    _check([field.name for field in fields] == ['value', 'tags'], 'dataclass fields mismatch')
    _check(fields[0].type is int and fields[1].type is list, 'dataclass annotation types mismatch')
    _check(other.tags == [] and changed.value == 12 and point.value == 7, 'factory/replace mismatch')
    copied = dataclasses.asdict(point)
    copied['tags'].append('copy')
    _check(point.tags == ['雪'], 'asdict must copy nested mutable data')
    return {'fields': [field.name for field in fields], 'value': point.value,
            'changed': changed.value, 'factory_independent': other.tags == [],
            'asdict': copied}


def _sample(value: int, increment: int = 5) -> int:
    return value + increment


def _annotationlib_probe():
    import annotationlib
    annotations = annotationlib.get_annotations(_sample, format=annotationlib.Format.VALUE)
    _check(annotations == {'value': int, 'increment': int, 'return': int},
           'annotationlib actual annotations mismatch')
    return {'annotations': {key: value.__name__ for key, value in annotations.items()}}


def _inspect_probe():
    import inspect

    class WithProperty:
        @property
        def forbidden(self):
            raise AssertionError('getattr_static executed a descriptor')

    signature = inspect.signature(_sample)
    _check(list(signature.parameters) == ['value', 'increment'], 'signature parameter order mismatch')
    _check(signature.parameters['value'].annotation is int and
           signature.parameters['increment'].default == 5 and
           signature.return_annotation is int, 'signature annotations/default mismatch')
    descriptor = inspect.getattr_static(WithProperty(), 'forbidden')
    _check(isinstance(descriptor, property), 'getattr_static did not preserve descriptor')
    return {'signature': str(signature), 'descriptor_not_executed': True}


def _typing_probe():
    import typing
    import _typing
    parameter = typing.TypeVar('T')
    _check(isinstance(parameter, _typing.TypeVar), '_typing TypeVar unavailable or mismatched')

    class Box(typing.Generic[parameter]):
        def __init__(self, value):
            self.value = value

    def hinted(value: 'int') -> 'list[int]':
        return [value]

    hinted.__annotations__['value'] = typing.ForwardRef('int')
    hints = typing.get_type_hints(hinted, globalns={'int': int, 'list': list})
    _check(hints['value'] is int and hints['return'] == list[int], 'ForwardRef/get_type_hints mismatch')
    alias = Box[int]
    _check(typing.get_origin(alias) is Box and typing.get_args(alias) == (int,), 'Generic alias mismatch')
    _check(alias(7).value == 7, 'Generic instance mismatch')
    return {'typevar': parameter.__name__, 'generic_value': alias(7).value,
            'hints': {'value': hints['value'].__name__, 'return': str(hints['return'])}}


def _weakref_probe():
    import weakref
    import gc

    class Token:
        pass

    events = []
    value = Token()
    reference = weakref.ref(value, lambda ignored: events.append('weakref'))
    finalizer = weakref.finalize(value, events.append, 'finalize')
    del value
    gc.collect()
    _check(reference() is None and not finalizer.alive and sorted(events) == ['finalize', 'weakref'],
           'weakref/finalize lifetime mismatch')
    cycle = Token()
    cycle.self = cycle
    cycle_reference = weakref.ref(cycle, lambda ignored: events.append('cycle'))
    del cycle
    gc.collect()
    _check(cycle_reference() is None and events.count('cycle') == 1, 'cyclic object was not collected')
    return {'events': sorted(events), 'referent_dead': True, 'cycle_dead': True, 'finalizer_alive': finalizer.alive}


def _collections_probe():
    from collections import Counter, defaultdict, deque
    pending = deque([1, 2])
    pending.appendleft(0)
    pending.append(3)
    _check(pending.popleft() == 0 and pending.pop() == 3, 'deque end operations mismatch')
    pending.rotate(1)
    groups = defaultdict(list)
    groups['雪'].append(7)
    counter = Counter('banana')
    _check(list(pending) == [2, 1] and counter == {'a': 3, 'b': 1, 'n': 2}, 'deque/Counter mismatch')
    return {'deque': list(pending), 'defaultdict': dict(groups), 'counter': dict(counter)}


def _queue_probe():
    import queue
    pending = queue.Queue(maxsize=2)
    pending.put_nowait('π')
    pending.put_nowait('')
    full = False
    try:
        pending.put_nowait('overflow')
    except queue.Full:
        full = True
    values = [pending.get_nowait(), pending.get_nowait()]
    empty = False
    try:
        pending.get_nowait()
    except queue.Empty:
        empty = True
    _check(values == ['π', ''] and full and empty, 'nonblocking Queue/Full/Empty mismatch')
    return {'scope': 'single-thread-nonblocking', 'values': values, 'full': full, 'empty': empty}


def _traceback_probe():
    import traceback

    def fail_leaf():
        raise ValueError('probe-π')

    try:
        fail_leaf()
    except ValueError as error:
        only = traceback.format_exception_only(error)
        stack = traceback.extract_tb(error.__traceback__)
        rendered = ''.join(traceback.format_exception(error))
        _check(only == ['ValueError: probe-π\n'] and stack[-1].name == 'fail_leaf' and
               'ValueError: probe-π' in rendered, 'traceback type/message/frame mismatch')
        # A -c string has no automatic source loader. A script or loader may
        # supply source lines. Report that observed boundary, never host paths.
        return {'exception_only': only, 'stack_names': [frame.name for frame in stack],
                'source_line_available': [bool(frame.line) for frame in stack],
                'source_text_claim': 'observed availability only'}
    raise AssertionError('expected leaf exception was not raised')


def _json_probe():
    import json
    original = {'text': 'π你好🙂', 'empty': '', 'numbers': [0, 9223372036854775807], 'none': None}
    encoded = json.dumps(original, ensure_ascii=False, sort_keys=True, separators=(',', ':'))
    _check(json.loads(encoded) == original and '🙂' in encoded, 'JSON Unicode/integer roundtrip mismatch')
    invalid = False
    try:
        json.loads('{"x":]')
    except json.JSONDecodeError:
        invalid = True
    _check(invalid, 'JSON invalid input did not fail')
    return {'encoded': encoded, 'invalid_rejected': invalid}


def _enum_probe():
    import enum

    class Color(enum.Enum):
        RED = 1
        GREEN = 2
        ALSO_RED = 1

    _check(Color(1) is Color.RED and Color.ALSO_RED is Color.RED and
           [item.name for item in Color] == ['RED', 'GREEN'], 'Enum value/alias mismatch')
    return {'members': [item.name for item in Color], 'alias': Color.ALSO_RED.name}


def _re_probe():
    import re
    match = re.fullmatch(r'(?P<word>[^=]+)=(?P<number>[0-9]+)', '雪=42')
    _check(match is not None and match.groupdict() == {'word': '雪', 'number': '42'}, 'regex Unicode/groups mismatch')
    _check(re.sub(r'[0-9]+', 'N', 'π17-42') == 'πN-N', 'regex substitution mismatch')
    return {'groups': match.groupdict(), 'substitution': 'πN-N'}


def _math_probe():
    import math
    _check(math.isqrt(18446744073709551615) == 4294967295 and math.sqrt(81) == 9 and
           math.gcd(84, 30) == 6 and not math.isfinite(float('inf')), 'math operation mismatch')
    return {'isqrt': math.isqrt(18446744073709551615), 'sqrt_integer': int(math.sqrt(81)), 'gcd': math.gcd(84, 30)}


def _struct_probe():
    import struct
    values = (0x12345678, 0xFEDCBA9876543210, 1.5)
    encoded = struct.pack('<IQd', *values)
    _check(len(encoded) == 20 and struct.unpack('<IQd', encoded) == values, 'LE struct numeric roundtrip mismatch')
    _check(encoded.hex() == '785634121032547698badcfe000000000000f83f', 'LE struct exact bytes mismatch')
    return {'format': '<IQd', 'bytes': len(encoded), 'hex': encoded.hex()}


def inner():
    counter = 7
    counter += 5
    return counter


def _python_trace_probe():
    events = []
    code = inner.__code__

    def trace(frame, event, argument):
        if frame.f_code is code and event in ('line', 'return'):
            copied = dict(frame.f_locals)
            record = {'event': event, 'line': frame.f_lineno - code.co_firstlineno, 'locals': copied}
            if event == 'return':
                record['value'] = argument
            events.append(record)
        return trace

    previous = sys.gettrace()
    try:
        sys.settrace(trace)
        returned = inner()
    finally:
        sys.settrace(previous)
    expected = [
        {'event': 'line', 'line': 1, 'locals': {}},
        {'event': 'line', 'line': 2, 'locals': {'counter': 7}},
        {'event': 'line', 'line': 3, 'locals': {'counter': 12}},
        {'event': 'return', 'line': 3, 'locals': {'counter': 12}, 'value': 12},
    ]
    _check(returned == 12 and events == expected, 'real Python line/frame locals or return mismatch')
    return {'function': code.co_name, 'events': events, 'return': returned,
            'scope': 'Python sys.settrace only', 'uwvm_debugger_bridge': False}


def main():
    probes = [
        ('runtime', _runtime_probe), ('args_env', _args_env_probe),
        ('dataclasses', _dataclasses_probe), ('annotationlib', _annotationlib_probe),
        ('inspect', _inspect_probe), ('typing', _typing_probe),
        ('weakref_gc', _weakref_probe), ('collections', _collections_probe),
        ('queue', _queue_probe), ('traceback', _traceback_probe),
        ('json', _json_probe), ('enum', _enum_probe), ('re', _re_probe),
        ('math', _math_probe), ('struct', _struct_probe), ('python_trace', _python_trace_probe),
    ]
    results = {}
    for name, probe in probes:
        try:
            results[name] = {'status': 'PASS', 'details': probe()}
        except Exception as error:
            results[name] = {'status': 'FAIL', 'error_type': type(error).__name__, 'error': str(error)}
    passed = all(row['status'] == 'PASS' for row in results.values())
    report = {'schema': 1, 'kind': 'stock-cpython-stdlib-operations', 'passed': passed,
              'summary': {'passed': sum(row['status'] == 'PASS' for row in results.values()),
                          'failed': sum(row['status'] == 'FAIL' for row in results.values()), 'total': len(results)},
              'probes': results, 'claims': {'python_trace_is_uwvm_debugger_bridge': False,
                                         'native_concurrency_qualified': False, 'complete_asyncio_qualified': False}}
    print(_canonical(report), flush=True)
    return 0 if passed else 1


if __name__ == '__main__':
    raise SystemExit(main())
