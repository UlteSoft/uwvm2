"""Real CPython fixture shared by native, WASI, timed and gated entry points."""
import gc
import os
import sys

MAX_ROUNDS = 100_000_000
DEFAULT_ROUNDS = 1_000


class Accumulator:
    __slots__ = ("value",)

    def __init__(self):
        self.value = 0

    def add(self, value):
        self.value = (self.value + value) % 1_000_003


def setup():
    return [(i * 7 + 3) % 97 for i in range(64)], Accumulator()


def kernel(values, accumulator, rounds):
    for round_index in range(rounds):
        for i in range(len(values)):
            index = (round_index + i) % len(values)
            values[index] = (values[index] * 17 + i + round_index % 13) % 997
            accumulator.add(values[index] * (i % 11 + 1))
    return accumulator.value


def arguments():
    try:
        rounds = int(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_ROUNDS
    except ValueError:
        return None
    if not 1 <= rounds <= MAX_ROUNDS:
        return None
    return rounds


def semantics():
    class Base:
        def number(self):
            return 17

    class Derived(Base):
        def number(self):
            return super().number() + 8

    assert Base().number() + Derived().number() == 42
    numbers = list(range(1, 11))
    assert sum(numbers) == 55 and numbers[::-1][0] == 10
    original = {"left": 1, "middle": 2, "right": 3}
    copied = original.copy()
    copied["middle"] = 20
    assert sum(original.values()) == 6 and copied["middle"] == 20
    assert list(original) == ["left", "middle", "right"]
    text = "π你好🙂"
    encoded = text.encode("utf-8")
    assert len(text) == 4 and len(encoded) == 12
    assert encoded.decode("utf-8") == text and text[::-1] == "🙂好你π"
    exceptions = 0
    try:
        numbers[100]
    except IndexError:
        exceptions += 1
    try:
        original["missing"]
    except KeyError:
        exceptions += 1

    class FixtureError(Exception):
        pass

    finally_seen = []
    try:
        try:
            raise ValueError("cause-π")
        except ValueError as cause:
            raise FixtureError("caught-你好") from cause
        finally:
            finally_seen.append(1)
    except FixtureError as caught:
        assert isinstance(caught.__cause__, ValueError)
        assert str(caught) == "caught-你好" and finally_seen == [1]
        exceptions += 1
    assert exceptions == 3
    immutable = bytes(range(16))
    backing = bytearray(immutable)
    view = memoryview(backing)
    view[3] ^= 255
    assert immutable[3] == 3 and backing[3] == 252 and sum(view) == 369
    del backing
    gc.collect()
    assert view[3] == 252  # memoryview keeps its backing allocation alive.
    del view
    was_enabled = gc.isenabled()
    gc.disable()
    cycle = []
    cycle.append(cycle)
    del cycle
    assert gc.collect() >= 1
    if was_enabled:
        gc.enable()
    extras = sys.argv[2:]
    if extras and extras != ["π", "你好", "", "space value"]:
        raise AssertionError("unexpected UTF-8 argument vector")
    env = os.environ.get("UWVM_PY_TEST")
    assert env is None or env == "python-π"
    assert os.environ.get("UWVM_PY_MISSING_20261018", "absent") == "absent"
    return ("python classes=42 arrays=55 dicts=6 unicode=4 utf8=12 "
            "exceptions=3 bytes=120 memory=369 gc=1 "
            f"args={len(extras)} env={int(env is not None)}")


def invalid():
    print("python invalid iterations", file=sys.stderr)
    return 2


def output(prefix, rounds, checksum):
    print(prefix)
    print(f"Iterations: {rounds}")
    print(f"Checksum: {checksum}")
