#!/usr/bin/env python3
"""Compare Core 3 validation decisions at each compiler's execution entry.

Run only in the remote Linux test cgroup. This checks observable correctness;
it does not claim that compilation decodes/validates a function only once.
The invalid function is always reachable from _start, so lazy compilation must
admit or reject the same bytes as pure validation and full compilation.

Uses the existing typed-reference corpus and exn/memarg fixtures. wasm-tools
independently checks each binary before it is used by any product mode.
https://webassembly.github.io/spec/core/valid/instructions.html
https://webassembly.github.io/spec/core/binary/instructions.html#binary-memarg
"""

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import resource
import subprocess


ANSI = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")
BASE_FEATURES = ("-WFE-reference-types", "-WFE-function-references", "-WFE-gc",
                 "-WFE-exceptions", "-WFE-tail-call", "-WFD-memory64")
TYPED_CASES = (
    "typed-param", "typed-block-nonnull-result", "typed-block-nonnull-rejects-null",
    "typed-local-unset", "typed-local-nonnull", "typed-local-wrong-set",
    "typed-local-block-no-escape", "typed-local-preinit-survives", "typed-local-else-reset", "typed-local-tee",
    "typed-br-table-subtype-labels", "typed-if-implicit-subtype",
    "rec-singleton-direct", "rec-multi-forward", "rec-multi-wrong-heap",
)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def typed_cases(root):
    path = root / "test/0013.uwvm_int/run_call_ref_typed_core3.py"
    spec = importlib.util.spec_from_file_location("core3_typed_validation_cases", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.CASES, path


def configurations(ros, lazy_verification):
    yield "validation", ("-m", "validation")
    if ros:
        yield "int-full", ("-Rint",)
        yield "jit-full", ("-Raot", "-Rllvm-cache-path", "disable")
    else:
        for engine in ("int", "jit"):
            modes = ("full", "lazy", "lazy+verification") if lazy_verification else ("full", "lazy")
            for mode in modes:
                flags = ("-Rcc", engine, "-Rcm", mode, "-Rct", "0")
                if engine == "jit":
                    flags += ("-Rllvm-cache-path", "disable")
                yield engine + "-" + mode, flags


def classify(returncode, text):
    """Never turn an unrelated CLI/loader/compiler failure into a valid rejection."""
    plain = ANSI.sub("", text).lower()
    if returncode is None:
        return "timeout"
    if any(marker in plain for marker in (
            "invalid parameter:", "unknown parameter", "unrecognized option",
            "invalid compilation mode", "unsupported current mode")):
        return "cli-error"
    if "parsing error in webassembly file" in plain or "illegal webassembly file format" in plain:
        return "parse-error"
    if "validation error in webassembly code" in plain:
        return "validation-error"
    if "runtime crash (catch unreachable)" in plain:
        return "runtime-unreachable"
    if "runtime crash (null reference)" in plain:
        return "runtime-null-reference"
    if "runtime crash" in plain or "wasm trap:" in plain:
        return "runtime-other-trap"
    if returncode == 0:
        return "success"
    return "unclassified-error"


def uleb(value):
    result = bytearray()
    while True:
        byte = value & 0x7f
        value >>= 7
        result.append(byte | (0x80 if value else 0))
        if not value:
            return bytes(result)


def read_uleb(data, offset, end):
    value = 0
    for shift in range(0, 35, 7):
        if offset >= end:
            raise ValueError("truncated fixture section length")
        byte = data[offset]
        offset += 1
        value |= (byte & 0x7f) << shift
        if not byte & 0x80:
            return value, offset
    raise ValueError("fixture section length exceeds u32 encoding")


def rewrite_memarg(binary, replacement):
    """Rewrite the exact one-function base fixture; repair both nested lengths.

    The section/body decoder is bounded. Matching the complete function body
    avoids accidentally patching identical bytes in names or another immediate.
    """
    if binary[:8] != b"\0asm\x01\0\0\0":
        raise ValueError("unexpected fixture header")
    result = bytearray(binary[:8])
    cursor = 8
    replaced = False
    expected_body = b"\x00\x41\x00\x28\x02\x00\x1a\x0b"
    while cursor < len(binary):
        section_id = binary[cursor]
        size, begin = read_uleb(binary, cursor + 1, len(binary))
        end = begin + size
        if end > len(binary):
            raise ValueError("fixture section exceeds file")
        payload = binary[begin:end]
        if section_id == 10:
            if replaced:
                raise ValueError("duplicate code section")
            count, body_size_begin = read_uleb(binary, begin, end)
            body_size, body_begin = read_uleb(binary, body_size_begin, end)
            if count != 1 or body_begin + body_size != end or binary[body_begin:end] != expected_body:
                raise ValueError("memory32 base fixture body changed")
            body = expected_body[:5] + replacement + expected_body[6:]
            payload = uleb(1) + uleb(len(body)) + body
            replaced = True
        result += bytes([section_id]) + uleb(len(payload)) + payload
        cursor = end
    if not replaced:
        raise ValueError("fixture has no code section")
    return bytes(result)


def rewrite_first_function_type(binary, kind):
    """Replace exactly the first empty function type with a GC composite.

    Start with valid text so a WAT parser cannot hide the product validation
    hole by rejecting a type-use during name resolution. Rebuild only the type
    section length, preserving all instruction type indices and body bytes.
    """
    if binary[:8] != b"\0asm\x01\0\0\0":
        raise ValueError("unexpected type-kind fixture header")
    result, cursor, replaced = bytearray(binary[:8]), 8, False
    replacement = {"struct": b"\x5f\x00", "array": b"\x5e\x7f\x00"}[kind]
    while cursor < len(binary):
        section_id = binary[cursor]
        size, begin = read_uleb(binary, cursor + 1, len(binary))
        end = begin + size
        if end > len(binary):
            raise ValueError("type-kind section exceeds file")
        payload = binary[begin:end]
        if section_id == 1:
            if replaced:
                raise ValueError("duplicate type section")
            count, first = read_uleb(binary, begin, end)
            if count < 1 or first + 3 > end or binary[first:first + 3] != b"\x60\x00\x00":
                raise ValueError("first type is not the expected empty function")
            payload = binary[begin:first] + replacement + binary[first + 3:end]
            replaced = True
        result += bytes([section_id]) + uleb(len(payload)) + payload
        cursor = end
    if not replaced:
        raise ValueError("type-kind fixture has no type section")
    return bytes(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--uwvm", type=Path, required=True)
    parser.add_argument("--wasm-tools", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--source-id", required=True)
    parser.add_argument("--source-root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--ros", action="store_true")
    parser.add_argument("--lazy-verification", action="store_true")
    parser.add_argument("--only-case", action="append", help="exact case name, repeatable")
    parser.add_argument("--timeout", type=int, default=45)
    args = parser.parse_args()
    root = args.source_root.resolve(strict=True)
    subprocess.run(["bash", str(root / "tools/ci/require_wasm3_test_cgroup.sh")], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.out.mkdir(parents=True, exist_ok=False)
    fixtures = root / "test/0017.runtime/fixtures"
    existing, typed_path = typed_cases(root)
    cases = {}
    for name in TYPED_CASES:
        outcome, wat = existing[name]
        cases[name] = {"wat": wat, "valid": outcome != "validation", "run": "success"}
    for operation in ("loop", "try_table"):
        cases["typed-local-" + operation.replace("_", "-") + "-no-escape"] = {
            "wat": existing["typed-local-block-no-escape"][1].replace(
                "block ref.func $f local.set $slot end", operation + " ref.func $f local.set $slot end"),
            "valid": False, "run": "success"}
    # No exception-typed signature/local/block can mask the immediate's own gate.
    for operation in ("ref.test", "ref.cast"):
        for heap in ("exn", "noexn"):
            name = operation.replace(".", "-") + "-" + heap + "-unreachable"
            wat = ("(module (func $probe unreachable " + operation + " (ref null " + heap + ") drop) "
                   "(func (export \"_start\") call $probe))")
            cases[name] = {"wat": wat, "valid": True, "run": "runtime-unreachable", "eh_gate": True}
    for name in ("ref-test-exn-null", "ref-cast-exn-null", "br-on-cast-exn-null"):
        cases[name] = {"wat": (fixtures / (name + ".wat")).read_text(), "valid": True, "run": "success"}
    for kind, declaration, get in (
            ("struct", "(struct (field (ref null exn)))", "struct.new_default $box struct.get $box 0"),
            ("array", "(array (ref null exn))", "i32.const 1 array.new_default $box i32.const 0 array.get $box")):
        # Exercise the field's exnref carrier both directly at throw_ref and
        # across a function-result ABI. Treating it as funcref must not pass.
        cases[kind + "-exn-result"] = {
            "wat": ("(module (type $box " + declaration + ") (func $get (result exnref) " + get + ") "
                    "(func (export \"_start\") call $get ref.is_null i32.eqz if unreachable end))"),
            "valid": True, "run": "success"}
        cases[kind + "-exn-throw-null"] = {
            "wat": "(module (type $box " + declaration + ") (func (export \"_start\") " + get + " throw_ref))",
            "valid": True, "run": "runtime-null-reference"}
    for unreachable in (False, True):
        name = "throw-wrong-gc-payload" + ("-unreachable" if unreachable else "")
        cases[name] = {
            "wat": ("(module (type $a (struct (field i32))) (type $b (array i32)) "
                    "(tag $e (param (ref null $a))) (func (export \"_start\") " +
                    ("unreachable " if unreachable else "") + "ref.null $b throw $e))"),
            "valid": False, "run": "success"}
    cases["throw-gc-subtype-caught"] = {
        "wat": """(module
          (type $base (sub (struct (field i32))))
          (type $child (sub $base (struct (field i32) (field i64))))
          (tag $e (param (ref null $base)))
          (func (export "_start")
            block $caught (result (ref null $base))
              try_table (catch $e $caught)
                struct.new_default $child throw $e
              end
              unreachable
            end
            ref.is_null if unreachable end))""",
        "valid": True, "run": "success"}
    cases["abstract-gc-null-with-function-types"] = {
        "wat": """(module (type $f (func)) (func (export "_start")
          ref.null any drop ref.null eq drop ref.null i31 drop
          ref.null struct drop ref.null array drop ref.null none drop))""",
        "valid": True, "run": "success"}
    # Bottom reference heaps share a machine carrier with their top heap, but
    # validation must preserve their precise type at every observable boundary.
    for heap, top_heap in (("noextern", "extern"), ("noexn", "exn")):
        for boundary, body in (
                ("result", "ref.null " + heap),
                ("local", "(local $slot (ref null " + heap + ")) ref.null " + heap +
                 " local.set $slot local.get $slot"),
                ("select", "ref.null " + heap + " ref.null " + heap +
                 " i32.const 0 select (result (ref null " + heap + "))"),
                ("block", "block (result (ref null " + heap + ")) ref.null " + heap + " end")):
            cases["null-bottom-" + heap + "-" + boundary] = {
                "wat": '(module (func $pick (result (ref null ' + heap + ')) ' + body + ') '
                       '(func (export "_start") call $pick ref.is_null i32.eqz if unreachable end))',
                "valid": True, "run": "success", "gc_independent": heap == "noexn"}
        cases["null-top-not-bottom-" + heap] = {
            "wat": '(module (func $pick (result (ref null ' + heap + ')) ref.null ' + top_heap + ') '
                   '(func (export "_start") call $pick drop))',
            "valid": False, "run": "success", "gc_independent": heap == "noexn"}
    for kind in ("struct", "array"):
        for operation, body in (
                ("block", "block (type $bad) end"),
                ("loop", "loop (type $bad) end"),
                ("if", "if (type $bad) end"),
                ("try-table", "try_table (type $bad) end"),
                ("call-indirect", "call_indirect (type $bad)"),
                ("return-call-indirect", "return_call_indirect (type $bad)"),
                ("call-ref", "call_ref $bad"),
                ("return-call-ref", "return_call_ref $bad")):
            # Keep _start on a distinct explicit type index: mutating type 0 must only
            # invalidate the instruction, never its containing function.
            cases[operation + "-" + kind + "-type-index"] = {
                "wat": '(module (type $bad (func)) (type $entry (func)) (table 1 funcref) '
                       '(func (export "_start") (type $entry) unreachable ' + body + '))',
                "replace_first_type": kind, "valid": False, "run": "success"}
    for heap in ("any", "eq", "i31", "struct", "array", "extern"):
        for operation in ("call_indirect", "return_call_indirect"):
            cases[operation.replace("_", "-") + "-" + heap + "-table"] = {
                "wat": '(module (type $f (func)) (table 1 (ref null ' + heap + ')) '
                       '(func (export "_start") unreachable ' + operation + ' (type $f)))',
                "valid": False, "run": "success"}
    cases["typed-function-table-call"] = {
        "wat": '(module (type $s (struct)) (type $f (func)) (table 1 (ref null $f)) '
               '(func $target (type $f)) (elem (i32.const 0) (ref $f) (ref.func $target)) '
               '(func (export "_start") block (type $f) end '
               'i32.const 0 call_indirect (type $f) ref.func $target call_ref $f))',
        "valid": True, "run": "success"}
    # Core 3 call_indirect is table.get + ref.cast + call_ref. A declared
    # function subtype therefore matches its ancestor, even when canonical IDs
    # differ. The local slot forces dynamic call_ref instead of a ref.func fold.
    for operation, body in (
            ("call-indirect", "i32.const 0 call_indirect (type $base)"),
            ("return-call-indirect", "i32.const 0 return_call_indirect (type $base)"),
            ("call-ref", "(local $slot (ref null $base)) ref.func $target local.set $slot "
             "local.get $slot call_ref $base"),
            ("return-call-ref", "(local $slot (ref null $base)) ref.func $target local.set $slot "
             "local.get $slot return_call_ref $base")):
        cases["function-subtype-" + operation] = {
            "wat": '(module (type $base (sub (func (result i32)))) '
                   '(type $child (sub $base (func (result i32)))) '
                   '(table 1 funcref) (func $target (type $child) i32.const 42) '
                   '(elem (i32.const 0) func $target) (func $pick (result i32) ' + body + ') '
                   '(func (export "_start") call $pick i32.const 42 i32.ne if unreachable end))',
            "valid": True, "run": "success"}
    cases["function-subtype-child-and-parent-indirect"] = {
        "wat": '(module (type $base (sub (func (result i32)))) '
               '(type $child (sub $base (func (result i32)))) (table 1 funcref) '
               '(func $target (type $child) i32.const 42) (elem (i32.const 0) func $target) '
               '(func (export "_start") i32.const 0 call_indirect (type $child) '
               'i32.const 42 i32.ne if unreachable end '
               'i32.const 0 call_indirect (type $base) i32.const 42 i32.ne if unreachable end))',
        "valid": True, "run": "success"}
    cases["function-unrelated-call-indirect"] = {
        "wat": '(module (type $base (sub (func (result i32)))) '
               '(type $child (sub $base (func (result i32)))) (type $other (func (result i32))) '
               '(table 1 funcref) (func $target (type $other) i32.const 42) '
               '(elem (i32.const 0) func $target) '
               '(func (export "_start") i32.const 0 call_indirect (type $base) drop))',
        "valid": True, "run": "runtime-other-trap", "trap_diagnostic": "call_indirect: signature mismatch"}
    cases["function-subtype-covariant-call-indirect"] = {
        "wat": '(module (type $object-base (sub (struct (field i32)))) '
               '(type $object-child (sub $object-base (struct (field i32) (field i64)))) '
               '(type $base (sub (func (result (ref null $object-base))))) '
               '(type $child (sub $base (func (result (ref $object-child))))) '
               '(table 1 funcref) (func $target (type $child) struct.new_default $object-child) '
               '(elem (i32.const 0) func $target) (func (export "_start") '
               'i32.const 0 call_indirect (type $base) ref.is_null if unreachable end))',
        "valid": True, "run": "success"}
    cases["function-subtype-contravariant-call-ref"] = {
        "wat": '(module (type $object-base (sub (struct (field i32)))) '
               '(type $object-child (sub $object-base (struct (field i32) (field i64)))) '
               '(type $base (sub (func (param (ref null $object-child)) (result i32)))) '
               '(type $child (sub $base (func (param (ref null $object-base)) (result i32)))) '
               '(func $target (type $child) i32.const 42) (elem declare func $target) '
               '(func $pick (result i32) (local $slot (ref null $base)) '
               'ref.func $target local.set $slot ref.null $object-child local.get $slot call_ref $base) '
               '(func (export "_start") call $pick i32.const 42 i32.ne if unreachable end))',
        "valid": True, "run": "success"}
    # These exact ref.func operands exercise LLVM's direct-call fold. The
    # validator must retain defined-type matching before native ABI equality.
    cases["function-subtype-contravariant-known-call-ref"] = {
        "wat": '(module (type $object-base (sub (struct (field i32)))) '
               '(type $object-child (sub $object-base (struct (field i32) (field i64)))) '
               '(type $base (sub (func (param (ref null $object-child)) (result i32)))) '
               '(type $child (sub $base (func (param (ref null $object-base)) (result i32)))) '
               '(func $target (type $child) i32.const 42) (elem declare func $target) '
               '(func (export "_start") ref.null $object-child ref.func $target call_ref $base '
               'i32.const 42 i32.ne if unreachable end))',
        "valid": True, "run": "success"}
    cases["function-subtype-contravariant-known-return-call-ref"] = {
        "wat": '(module (type $object-base (sub (struct (field i32)))) '
               '(type $object-child (sub $object-base (struct (field i32) (field i64)))) '
               '(type $base (sub (func (param (ref null $object-child)) (result i32)))) '
               '(type $child (sub $base (func (param (ref null $object-base)) (result i32)))) '
               '(func $target (type $child) i32.const 42) (elem declare func $target) '
               '(func $pick (result i32) ref.null $object-child ref.func $target return_call_ref $base) '
               '(func (export "_start") call $pick i32.const 42 i32.ne if unreachable end))',
        "valid": True, "run": "success"}
    cases["function-subtype-covariant-known-call-ref"] = {
        "wat": '(module (type $object-base (sub (struct (field i32)))) '
               '(type $object-child (sub $object-base (struct (field i32) (field i64)))) '
               '(type $base (sub (func (result (ref null $object-base))))) '
               '(type $child (sub $base (func (result (ref $object-child))))) '
               '(func $target (type $child) struct.new_default $object-child) (elem declare func $target) '
               '(func (export "_start") ref.func $target call_ref $base ref.is_null if unreachable end))',
        "valid": True, "run": "success"}
    cases["function-subtype-covariant-known-return-call-ref"] = {
        "wat": '(module (type $object-base (sub (struct (field i32)))) '
               '(type $object-child (sub $object-base (struct (field i32) (field i64)))) '
               '(type $base (sub (func (result (ref null $object-base))))) '
               '(type $child (sub $base (func (result (ref $object-child))))) '
               '(func $target (type $child) struct.new_default $object-child) (elem declare func $target) '
               '(func $pick (result (ref null $object-base)) ref.func $target return_call_ref $base) '
               '(func (export "_start") call $pick ref.is_null if unreachable end))',
        "valid": True, "run": "success"}
    # Same carrier signature and parent do not erase closed recursive-group
    # identity. Ordinary duplicate singleton final functions would be equal,
    # so the unrelated target deliberately belongs to a two-member group.
    cases["function-rec-group-final-known-call-ref-reject"] = {
        "wat": '(module (type $base (sub (func (result i32)))) '
               '(type $singleton (sub final $base (func (result i32)))) '
               '(rec (type $group-child (sub final $base (func (result i32)))) (type $extra (func))) '
               '(func $target (type $group-child) i32.const 42) (elem declare func $target) '
               '(func (export "_start") ref.func $target call_ref $singleton drop))',
        "valid": False, "run": "success"}
    cases["function-rec-group-final-call-indirect-trap"] = {
        "wat": '(module (type $base (sub (func (result i32)))) '
               '(type $singleton (sub final $base (func (result i32)))) '
               '(rec (type $group-child (sub final $base (func (result i32)))) (type $extra (func))) '
               '(table 1 funcref) (func $target (type $group-child) i32.const 42) '
               '(elem (i32.const 0) func $target) '
               '(func (export "_start") i32.const 0 call_indirect (type $singleton) drop))',
        "valid": True, "run": "runtime-other-trap", "trap_diagnostic": "call_indirect: signature mismatch"}
    cases["function-rec-group-final-base-calls"] = {
        "wat": '(module (type $base (sub (func (result i32)))) '
               '(type $singleton (sub final $base (func (result i32)))) '
               '(rec (type $group-child (sub final $base (func (result i32)))) (type $extra (func))) '
               '(table 1 funcref) (func $target (type $group-child) i32.const 42) '
               '(elem (i32.const 0) func $target) '
               '(func (export "_start") ref.func $target call_ref $base '
               'i32.const 42 i32.ne if unreachable end '
               'i32.const 0 call_indirect (type $base) i32.const 42 i32.ne if unreachable end))',
        "valid": True, "run": "success"}
    cases["function-canonical-duplicate-alias-calls"] = {
        "wat": '(module (type $base (sub (func (result i32)))) '
               '(type $child (sub $base (func (result i32)))) '
               '(type $base-alias (sub (func (result i32)))) '
               '(type $child-alias (sub $base-alias (func (result i32)))) '
               '(table 2 funcref) (func $first (type $child) i32.const 42) '
               '(func $second (type $child-alias) i32.const 42) (elem (i32.const 0) func $first $second) '
               '(func (export "_start") '
               'ref.func $first call_ref $base-alias i32.const 42 i32.ne if unreachable end '
               'ref.func $first call_ref $child-alias i32.const 42 i32.ne if unreachable end '
               'ref.func $second call_ref $base i32.const 42 i32.ne if unreachable end '
               'ref.func $second call_ref $child i32.const 42 i32.ne if unreachable end '
               'i32.const 0 call_indirect (type $base-alias) i32.const 42 i32.ne if unreachable end '
               'i32.const 0 call_indirect (type $child-alias) i32.const 42 i32.ne if unreachable end '
               'i32.const 1 call_indirect (type $base) i32.const 42 i32.ne if unreachable end '
               'i32.const 1 call_indirect (type $child) i32.const 42 i32.ne if unreachable end))',
        "valid": True, "run": "success"}
    # Dynamic function references keep their declared subtype through a cast
    # and through typed GC fields, including allocations during initialization.
    cases["function-defined-heap-ref-test-cast"] = {
        "wat": '(module (type $base (sub (func (result i32)))) '
               '(type $child (sub $base (func (result i32)))) (type $other (func (result i64))) '
               '(table 1 funcref) (func $target (type $child) i32.const 42) '
               '(elem (i32.const 0) func $target) (func (export "_start") '
               'i32.const 0 table.get 0 ref.test (ref $child) i32.eqz if unreachable end '
               'i32.const 0 table.get 0 ref.test (ref $other) if unreachable end '
               'i32.const 0 table.get 0 ref.cast (ref $base) call_ref $base '
               'i32.const 42 i32.ne if unreachable end '
               'i32.const 0 table.get 0 ref.cast (ref $child) call_ref $child '
               'i32.const 42 i32.ne if unreachable end))',
        "valid": True, "run": "success"}
    cases["function-typed-struct-field-constexpr"] = {
        "wat": '(module (type $base (sub (func (result i32)))) '
               '(type $child (sub $base (func (result i32)))) (type $box (struct (field (ref $base)))) '
               '(func $target (type $child) i32.const 42) (elem declare func $target) '
               '(global $root (ref $box) (struct.new $box (ref.func $target))) '
               '(func (export "_start") global.get $root struct.get $box 0 call_ref $base '
               'i32.const 42 i32.ne if unreachable end))',
        "valid": True, "run": "success"}
    cases["function-typed-array-field-constexpr"] = {
        "wat": '(module (type $base (sub (func (result i32)))) '
               '(type $child (sub $base (func (result i32)))) (type $functions (array (ref $base))) '
               '(func $target (type $child) i32.const 42) (elem declare func $target) '
               '(global $root (ref $functions) (array.new_fixed $functions 1 (ref.func $target))) '
               '(func (export "_start") global.get $root i32.const 0 array.get $functions '
               'ref.cast (ref $child) call_ref $child i32.const 42 i32.ne if unreachable end))',
        "valid": True, "run": "success"}
    cases["nofunc-table-polymorphic-call"] = {
        "wat": '(module (type $f (func)) (table 1 (ref null nofunc)) '
               '(func (export "_start") unreachable call_indirect (type $f)))',
        "valid": True, "run": "runtime-unreachable"}
    cases["numeric-plan-tag-caught"] = {
        "wat": '(module (tag $e (param i32)) (func $raise i32.const 7 throw $e) '
               '(func (export "_start") block $done (result i32) '
               'try_table (catch $e $done) call $raise end unreachable end '
               'i32.const 7 i32.ne if unreachable end))',
        "valid": True, "run": "success"}
    for unreachable in (False, True):
        cases["numeric-plan-wrong-payload" + ("-unreachable" if unreachable else "")] = {
            "wat": '(module (tag $e (param i32)) (func $first) '
                   '(func (export "_start") call $first ' +
                   ('unreachable ' if unreachable else '') + 'i64.const 7 throw $e))',
            "valid": False, "run": "success"}
    cases["numeric-plan-valid-native-fallback"] = {
        "wat": '(module (tag $e (param i32)) (memory 1) '
               '(func (export "_start") i32.const 0 i32.load drop))',
        "valid": True, "run": "success"}
    # A physical Unknown slot from unreachable select must retain Bot typing.
    # Calling the probe with zero compiles it lazily without executing its trap.
    for operation in ("grow", "copy", "fill", "init"):
        for valid in (True, False):
            suffix = "valid" if valid else "wrong_i32"
            name = "memory64-polymorphic-" + operation + ("-valid" if valid else "-wrong-i32")
            cases[name] = {
                "wat": (fixtures / ("wasm3_memory64_polymorphic_" + operation + "_" + suffix + ".wat")).read_text(),
                "valid": valid, "run": "success", "memory64": True}
    memarg_wat = (fixtures / "wasm3_padded_memarg_base.wat").read_text()
    # Core 3 memory/table.copy uses each selected address width and the
    # smaller width for its count. Both directions execute in called bodies,
    # so lazy entry cannot hide an invalid count in an uncalled function.
    for storage in ("memory", "table"):
        wat = (fixtures / ("wasm3_mixed_" + storage + "_copy.wat")).read_text()
        marker = "i32.const 1 ;; mixed-copy-count"
        if wat.count(marker) != 1:
            raise ValueError("mixed-copy fixture must have one negative-control site")
        for valid in (True, False):
            name = "mixed-" + storage + "-copy" + ("-valid" if valid else "-wrong-i64-count")
            cases[name] = {"wat": wat if valid else wat.replace(marker, "i64.const 1 ;; mixed-copy-count"),
                           "valid": valid, "run": "success", "memory64": storage == "memory",
                           "extra_features": ("--wasm-feature-enable-multi-memory",) if storage == "memory" else
                                             ("--wasm-feature-enable-table64",)}

    # Core 3 definite initialization is independent of stack polymorphism and
    # is restored when try_table/control frames end. Called bodies force lazy
    # entries to validate the same exceptional paths as full and pure modes.
    for suffix, valid, gc_independent in (
            ("catch_payload_valid", True, False),
            ("catch_payload_no_escape", False, False),
            ("preinit_catch_valid", True, False),
            ("nested_catch_no_escape", False, False),
            ("catch_ref_valid", True, True),
            ("catch_ref_no_escape", False, True),
            ("throw_unreachable_unset", False, False),
            ("throw_unreachable_initialized_valid", True, False)):
        name = "eh-local-" + suffix.replace("_", "-")
        cases[name] = {"wat": (fixtures / ("wasm3_eh_local_" + suffix + ".wat")).read_text(),
                       "valid": valid, "run": "success", "gc_independent": gc_independent}

    # Core 3 GC segment types, mutation and packed storage stay constrained even
    # in unreachable code. Valid called bodies execute actual segment readback;
    # the large fixed-count probe is lazily compiled without allocating its array.
    for suffix, valid, simd in (
            ("new_elem_subtype_valid", True, False),
            ("new_elem_nullable_invalid", False, False),
            ("init_elem_subtype_valid", True, False),
            ("init_elem_immutable_invalid", False, False),
            ("new_data_reference_invalid", False, False),
            ("new_data_immutable_valid", True, False),
            ("init_data_immutable_invalid", False, False),
            ("copy_packed_width_invalid", False, False),
            ("data_vector_valid", True, True),
            ("fixed_bot_valid", True, True),
            ("fixed_bot_wrong_known_invalid", False, True)):
        name = "gc-array-" + suffix.replace("_", "-")
        cases[name] = {"wat": (fixtures / ("wasm3_gc_array_" + suffix + ".wat")).read_text(),
                       "valid": valid, "run": "success",
                       "extra_features": ("-WFE-simd",) if simd else ()}

    # All four current Core3 catch kinds precede a real later direct call.
    # _start reaches the probe, so lazy compilation cannot hide this syntax.
    for kind in ("catch", "catch_ref", "catch_all", "catch_all_ref"):
        cases["lazy-eh-scan-" + kind.replace("_", "-")] = {
            "wat": (fixtures / ("wasm3_lazy_eh_scan_" + kind + ".wat")).read_text(),
            "valid": True, "run": "success"}

    # Typed select carries a complete Core3 valtype, not one projected byte.
    # A later direct call verifies that lazy boundary discovery remains aligned.
    for kind, valid in (("struct_nullable", True), ("struct_nonnull", True),
                        ("function_nullable", True), ("exn_nullable", True),
                        ("noexn_nullable", True), ("nonnull_rejects_null", False)):
        cases["lazy-select-scan-" + kind.replace("_", "-")] = {
            "wat": (fixtures / ("wasm3_lazy_select_scan_" + kind + ".wat")).read_text(),
            "valid": valid, "run": "success"}

    # Runtime block signatures reuse the first complete decode and retain exact
    # heap/nullability plus mixed-table function-kind checks in every called mode.
    for kind, valid in (("nonnull_struct", True), ("nullable_function", True),
                        ("noexn", True), ("multivalue", True), ("nonnull_rejects_null", False)):
        cases["decoded-block-" + kind.replace("_", "-")] = {
            "wat": (fixtures / ("wasm3_decoded_block_" + kind + ".wat")).read_text(),
            "valid": valid, "run": "success"}

    # Legacy funcref/externref block results need exact nullable abstract heaps,
    # because the funcref execution carrier also represents unrelated GC values.
    for kind, valid in (("funcref_rejects_any", False), ("funcref_rejects_struct", False),
                        ("funcref_rejects_none", False), ("externref_rejects_exn", False),
                        ("externref_rejects_gc", False), ("funcref_typed_function", True),
                        ("funcref_nofunc", True), ("externref_conversion", True)):
        cases["decoded-block-" + kind.replace("_", "-")] = {
            "wat": (fixtures / ("wasm3_decoded_block_" + kind + ".wat")).read_text(),
            "valid": valid, "run": "success"}

    # Function-only ref.null retains the decoded heap through nullable select,
    # non-null narrowing and a later real call_ref. Wrong hierarchy stays invalid.
    for kind, valid in (("nonnull_function", True), ("function_select", True),
                        ("invalid_function_result", False)):
        cases["ref-null-decoded-" + kind.replace("_", "-")] = {
            "wat": (fixtures / ("wasm3_ref_null_decoded_" + kind + ".wat")).read_text(),
            "valid": valid, "run": "success", "gc_independent": True}

    # Register-ring join snapshots describe physical carriers; exact validation
    # types survive below control frames and declared parameters/results stay widened.
    for kind, valid in (("i31_else", True), ("struct_else", True),
                        ("function_else", True), ("i31_no_else", True),
                        ("i31_then_merge", True), ("i31_branch_merge", True),
                        ("declared_any_rejects_i31", False), ("function_rejects_struct", False),
                        ("dead_i31_else", True), ("dead_i31_no_else", True),
                        ("dead_any_rejects_i31", False)):
        cases["int-join-reference-" + kind.replace("_", "-")] = {
            "wat": (fixtures / ("wasm3_int_join_reference_" + kind + ".wat")).read_text(),
            "valid": valid, "run": "runtime-unreachable" if kind.startswith("dead_") and valid else "success"}

    # A dead execution edge still validates declaration-typed block parameters/results.
    # _start reaches every probe, so lazy translation cannot skip the regression body.
    for kind, valid in (("results_select", True), ("mixed_results", True),
                        ("outer_live_else", True), ("block_params", True),
                        ("loop_params", True), ("if_params_else", True),
                        ("if_params_no_else", True), ("typed_select_wrong_heap", False),
                        ("typed_select_nullable", False)):
        cases["dead-codegen-model-" + kind.replace("_", "-")] = {
            "wat": (fixtures / ("wasm3_dead_codegen_model_" + kind + ".wat")).read_text(),
            "valid": valid, "run": "success" if kind == "outer_live_else" else "runtime-unreachable",
            "extra_features": ("-WFE-simd",) if kind == "mixed_results" else ()}

    cases["memory32-padded-u64-zero"] = {
        "wat": memarg_wat, "valid": True, "run": "success", "offset": b"\x80\x80\x80\x80\x80\x00"}
    cases["memory32-offset-overflow"] = {
        "wat": memarg_wat, "valid": False, "run": "success", "offset": uleb(1 << 32)}
    if args.only_case:
        unknown = set(args.only_case) - cases.keys()
        if unknown:
            parser.error(f"unknown cases: {sorted(unknown)}")
        cases = {name: case for name, case in cases.items() if name in args.only_case}
    modes = tuple(configurations(args.ros, args.lazy_verification))
    rows = []
    artifacts = {}

    def command(label, argv):
        argv = [str(part) for part in argv]
        try:
            process = subprocess.run(argv, capture_output=True, timeout=args.timeout)
            code, raw = process.returncode, process.stdout + process.stderr
        except subprocess.TimeoutExpired as error:
            code, raw = None, (error.stdout or b"") + (error.stderr or b"")
        (args.out / (label + ".log")).write_bytes(raw)
        return code, raw.decode(errors="replace"), argv

    def record(row):
        rows.append(row)
        (args.out / "runs.json").write_text(json.dumps(rows, indent=2) + "\n")
        print(json.dumps({key: row[key] for key in ("label", "phase", "passed")}), flush=True)

    for name, case in cases.items():
        wat, wasm = args.out / (name + ".wat"), args.out / (name + ".wasm")
        wat.write_text(case["wat"] + "\n")
        code, text, argv = command(name + "-assemble", (args.wasm_tools, "parse", wat, "-o", wasm))
        record({"label": name + "-assemble", "phase": "oracle-assembly", "command": argv,
                "exit": code, "passed": code == 0})
        if code != 0:
            continue
        if "replace_first_type" in case:
            wasm.write_bytes(rewrite_first_function_type(wasm.read_bytes(), case["replace_first_type"]))
        if "offset" in case:
            wasm.write_bytes(rewrite_memarg(wasm.read_bytes(), case["offset"]))
        artifacts[name] = {"wat_sha256": digest(wat), "wasm_sha256": digest(wasm)}
        code, text, argv = command(name + "-oracle", (args.wasm_tools, "validate", "--features", "all", wasm))
        # Invalid syntax is not silently accepted as a malformed binary: require
        # the official validator to identify a validation error at an offset.
        oracle_rejection = code is not None and code > 0 and "offset" in text.lower() and "error" in text.lower()
        oracle_passed = code == 0 if case["valid"] else oracle_rejection
        record({"label": name + "-oracle", "phase": "oracle-validation", "command": argv,
                "exit": code, "expected_valid": case["valid"], "passed": oracle_passed})
        if not oracle_passed:
            continue
        enabled_features = BASE_FEATURES
        if case.get("memory64"):
            enabled_features = tuple(flag for flag in BASE_FEATURES if flag != "-WFD-memory64") + ("-WFE-memory64",)
        enabled_features += case.get("extra_features", ())
        policies = [("enabled", enabled_features, None, None)]
        if name == "memory32-padded-u64-zero":
            # An explicit legacy grammar still uses u32 memarg encoding. The
            # same value is valid Core 3 but overlong in Core 2's encoding.
            policies.append(("legacy-wasm2", ("-WF2",), "validation-error", None))
        if name.startswith("rec-") and case["valid"]:
            flags = tuple(flag for flag in BASE_FEATURES if flag != "-WFE-gc") + ("-WFD-gc",)
            policies.append(("gc-off", flags, "parse-error", "--wasm-feature-enable-gc"))
        if case.get("eh_gate"):
            flags = tuple(flag for flag in BASE_FEATURES if flag != "-WFE-exceptions") + ("-WFD-exceptions",)
            policies.append(("exceptions-off", flags, "validation-error", "--wasm-feature-enable-exceptions"))
        if case.get("gc_independent"):
            # exn/noexn depend on exceptions, not GC. Run the very same bytes
            # through the fallback reference decoder with GC explicitly off.
            flags = tuple(flag for flag in BASE_FEATURES if flag != "-WFE-gc") + ("-WFD-gc",)
            policies.append(("gc-off", flags, None, None))
        for policy, feature_flags, forced_outcome, required_diagnostic in policies:
            for mode, mode_flags in modes:
                expected = forced_outcome or ("validation-error" if not case["valid"] else
                                              "success" if mode == "validation" else case["run"])
                label = name + "-" + policy + "-" + mode.replace("+", "-")
                code, text, argv = command(label, (args.uwvm, *mode_flags, *feature_flags, "--run", wasm))
                actual = classify(code, text)
                passed = actual == expected and ((code == 0) == (expected == "success"))
                if required_diagnostic:
                    passed = passed and required_diagnostic in ANSI.sub("", text)
                if case.get("trap_diagnostic") and mode != "validation" and forced_outcome is None:
                    passed = passed and case["trap_diagnostic"] in ANSI.sub("", text)
                record({"label": label, "phase": "product", "case": name, "policy": policy, "mode": mode,
                        "command": argv, "exit": code, "actual": actual, "expected": expected,
                        "required_diagnostic": required_diagnostic, "wasm_sha256": artifacts[name]["wasm_sha256"],
                        "passed": passed})

    summary = {
        "passed": bool(rows) and all(row["passed"] for row in rows),
        "source_id": args.source_id, "repository": "ros" if args.ros else "ordinary",
        "runner_sha256": digest(Path(__file__)), "typed_corpus_sha256": digest(typed_path),
        "uwvm_sha256": digest(args.uwvm), "wasm_tools_sha256": digest(args.wasm_tools),
        "cases": len(cases), "modes": [mode for mode, _ in modes], "checks": len(rows),
        "product_checks": sum(row["phase"] == "product" for row in rows), "artifacts": artifacts,
        "failures": [row for row in rows if not row["passed"]],
        "cgroup": {key: Path("/sys/fs/cgroup", key).read_text().strip()
                   for key in ("memory.max", "memory.swap.max", "cpuset.cpus.effective")},
    }
    (args.out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps({key: summary[key] for key in ("passed", "cases", "checks", "product_checks")}), flush=True)
    raise SystemExit(0 if summary["passed"] else 1)


if __name__ == "__main__":
    main()
