#!/usr/bin/env python3
"""Generate backend-fuzzer Wasm binaries and a direct-storage C++ case header."""

from __future__ import annotations

import argparse
import json
import random
import shutil
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


I32 = 0x7F
I64 = 0x7E


def uleb(v: int) -> bytes:
    out = bytearray()
    while True:
        b = v & 0x7F
        v >>= 7
        if v:
            b |= 0x80
        out.append(b)
        if not v:
            return bytes(out)


def sleb(v: int, bits: int = 32) -> bytes:
    out = bytearray()
    more = True
    while more:
        b = v & 0x7F
        v >>= 7
        sign = b & 0x40
        if (v == 0 and sign == 0) or (v == -1 and sign != 0):
            more = False
        else:
            b |= 0x80
        out.append(b)
    return bytes(out)


def u32(v: int) -> int:
    return v & 0xFFFFFFFF


def i32(v: int) -> int:
    v &= 0xFFFFFFFF
    return v - 0x100000000 if v & 0x80000000 else v


def rotl32(x: int, r: int) -> int:
    r &= 31
    x &= 0xFFFFFFFF
    return u32(x if r == 0 else ((x << r) | (x >> (32 - r))))


def rotr32(x: int, r: int) -> int:
    r &= 31
    x &= 0xFFFFFFFF
    return u32(x if r == 0 else ((x >> r) | (x << (32 - r))))


def shr_s32(x: int, r: int) -> int:
    return u32(i32(x) >> (r & 31))


def section(section_id: int, payload: bytes) -> bytes:
    return bytes([section_id]) + uleb(len(payload)) + payload


def vec(items: Iterable[bytes]) -> bytes:
    items = list(items)
    return uleb(len(items)) + b"".join(items)


def limits(min_pages: int, max_pages: int | None = None) -> bytes:
    if max_pages is None:
        return b"\x00" + uleb(min_pages)
    return b"\x01" + uleb(min_pages) + uleb(max_pages)


def memarg(align: int, offset: int = 0) -> bytes:
    return uleb(align) + uleb(offset)


def i32_const(v: int) -> bytes:
    return b"\x41" + sleb(i32(v), 32)


def i64_const(v: int) -> bytes:
    return b"\x42" + sleb(v, 64)


def local_get(i: int) -> bytes:
    return b"\x20" + uleb(i)


def local_set(i: int) -> bytes:
    return b"\x21" + uleb(i)


def local_tee(i: int) -> bytes:
    return b"\x22" + uleb(i)


def check_i32(expected: int) -> bytes:
    return i32_const(expected) + b"\x47\x04\x40\x00\x0b"


def check_i64(expected: int) -> bytes:
    return i64_const(expected) + b"\x52\x04\x40\x00\x0b"


def code_body(local_groups: list[tuple[int, int]], expr: bytes) -> bytes:
    locals_blob = uleb(len(local_groups)) + b"".join(uleb(n) + bytes([t]) for n, t in local_groups)
    return locals_blob + expr


@dataclass
class Func:
    type_index: int
    local_groups: list[tuple[int, int]]
    expr: bytes

    @property
    def body(self) -> bytes:
        return code_body(self.local_groups, self.expr)

    @property
    def expr_offset(self) -> int:
        return len(code_body(self.local_groups, b""))


@dataclass
class Case:
    name: str
    types: list[tuple[list[int], list[int]]]
    funcs: list[Func]
    has_memory: bool = False
    memory_min: int = 0
    memory_max: int = 0
    memory_has_max: bool = False
    expect_trap: bool = False

    def wasm(self) -> bytes:
        out = bytearray(b"\x00asm\x01\x00\x00\x00")

        type_payload = vec(
            b"\x60" + vec(bytes([p]) for p in params) + vec(bytes([r]) for r in results)
            for params, results in self.types
        )
        out += section(1, type_payload)

        out += section(3, vec(uleb(f.type_index) for f in self.funcs))

        if self.has_memory:
            max_pages = self.memory_max if self.memory_has_max else None
            out += section(5, vec([limits(self.memory_min, max_pages)]))

        name = b"main"
        export_payload = vec([uleb(len(name)) + name + b"\x00" + uleb(0)])
        out += section(7, export_payload)

        bodies = []
        for f in self.funcs:
            body = f.body
            bodies.append(uleb(len(body)) + body)
        out += section(10, vec(bodies))
        return bytes(out)


class I32Program:
    def __init__(self, rng: random.Random, local_count: int) -> None:
        self.rng = rng
        self.local_count = local_count
        self.code = bytearray()
        self.locals = [u32(rng.getrandbits(32)) for _ in range(local_count)]
        self.stack: list[int] = []
        for idx, val in enumerate(self.locals):
            self.code += i32_const(val)
            self.code += local_set(idx)

    def push_const(self) -> None:
        v = self.rng.getrandbits(32)
        self.code += i32_const(v)
        self.stack.append(u32(v))

    def push_local(self) -> None:
        idx = self.rng.randrange(self.local_count)
        self.code += local_get(idx)
        self.stack.append(self.locals[idx])

    def unop(self, op: int) -> None:
        v = self.stack.pop()
        if op == 0x45:  # eqz
            r = 1 if v == 0 else 0
        elif op == 0x67:  # clz
            r = 32 if v == 0 else 32 - v.bit_length()
        elif op == 0x68:  # ctz
            r = 32 if v == 0 else (v & -v).bit_length() - 1
        elif op == 0x69:  # popcnt
            r = v.bit_count()
        else:
            raise AssertionError(op)
        self.code.append(op)
        self.stack.append(u32(r))

    def binop(self, op: int) -> None:
        rhs = self.stack.pop()
        lhs = self.stack.pop()
        if op == 0x6A:
            r = lhs + rhs
        elif op == 0x6B:
            r = lhs - rhs
        elif op == 0x6C:
            r = lhs * rhs
        elif op == 0x71:
            r = lhs & rhs
        elif op == 0x72:
            r = lhs | rhs
        elif op == 0x73:
            r = lhs ^ rhs
        elif op == 0x74:
            r = lhs << (rhs & 31)
        elif op == 0x75:
            r = shr_s32(lhs, rhs)
        elif op == 0x76:
            r = lhs >> (rhs & 31)
        elif op == 0x77:
            r = rotl32(lhs, rhs)
        elif op == 0x78:
            r = rotr32(lhs, rhs)
        elif op == 0x46:
            r = 1 if lhs == rhs else 0
        elif op == 0x47:
            r = 1 if lhs != rhs else 0
        elif op == 0x48:
            r = 1 if i32(lhs) < i32(rhs) else 0
        elif op == 0x49:
            r = 1 if lhs < rhs else 0
        elif op == 0x4E:
            r = 1 if i32(lhs) >= i32(rhs) else 0
        else:
            raise AssertionError(op)
        self.code.append(op)
        self.stack.append(u32(r))

    def local_store(self, tee: bool) -> None:
        idx = self.rng.randrange(self.local_count)
        v = self.stack.pop()
        self.locals[idx] = v
        self.code += local_tee(idx) if tee else local_set(idx)
        if tee:
            self.stack.append(v)

    def select(self) -> None:
        cond = self.stack.pop()
        v2 = self.stack.pop()
        v1 = self.stack.pop()
        self.code.append(0x1B)
        self.stack.append(v1 if cond != 0 else v2)

    def drop(self) -> None:
        self.stack.pop()
        self.code.append(0x1A)

    def finish(self) -> tuple[bytes, int]:
        if not self.stack:
            self.push_local()
        while len(self.stack) > 1:
            self.binop(0x6A)
        return bytes(self.code), self.stack[-1]


def random_i32_case(rng: random.Random, index: int, steps: int) -> Case:
    local_count = 8
    p = I32Program(rng, local_count)

    for _ in range(steps):
        depth = len(p.stack)
        action = rng.randrange(100)
        if depth == 0 or (action < 32 and depth < 12):
            if rng.randrange(2):
                p.push_local()
            else:
                p.push_const()
        elif action < 43:
            p.code.append(0x01)  # nop
        elif action < 54 and depth >= 1:
            p.local_store(tee=bool(rng.randrange(2)))
        elif action < 64 and depth >= 1:
            p.unop(rng.choice([0x45, 0x67, 0x68, 0x69]))
        elif action < 72 and depth >= 1:
            p.drop()
        elif action < 82 and depth >= 3:
            p.select()
        elif depth >= 2:
            p.binop(rng.choice([0x6A, 0x6B, 0x6C, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x46, 0x47, 0x48, 0x49, 0x4E]))

    expr, expected = p.finish()
    expr += check_i32(expected)
    expr += b"\x0b"
    return Case(
        name=f"{index:04d}.i32_random",
        types=[([], [])],
        funcs=[Func(0, [(local_count, I32)], expr)],
    )


def loop_case(rng: random.Random, index: int) -> Case:
    bound = rng.randrange(4, 48)
    salt = rng.getrandbits(16)
    acc = 0
    for i in range(bound):
        acc = u32(acc + u32(i ^ salt))

    c = bytearray()
    c += i32_const(0) + local_set(0)
    c += i32_const(0) + local_set(1)
    c += b"\x02\x40"  # block
    c += b"\x03\x40"  # loop
    c += local_get(0) + i32_const(bound) + b"\x4F" + b"\x0D" + uleb(1)  # br_if exit
    c += local_get(1) + local_get(0) + i32_const(salt) + b"\x73\x6A" + local_set(1)
    c += local_get(0) + i32_const(1) + b"\x6A" + local_set(0)
    c += b"\x0C" + uleb(0)
    c += b"\x0b\x0b"
    c += local_get(1) + check_i32(acc)
    c += b"\x0b"
    return Case(
        name=f"{index:04d}.loop_i32",
        types=[([], [])],
        funcs=[Func(0, [(2, I32)], bytes(c))],
    )


def memory_case(rng: random.Random, index: int) -> Case:
    offset = rng.randrange(0, 4096, 4)
    value = rng.getrandbits(32)
    c = bytearray()
    c += i32_const(offset) + i32_const(value) + b"\x36" + memarg(2, 0)
    c += i32_const(offset) + b"\x28" + memarg(2, 0) + check_i32(value)
    c += b"\x3F\x00" + check_i32(1)
    c += i32_const(1) + b"\x40\x00" + check_i32(1)
    c += b"\x3F\x00" + check_i32(2)
    c += b"\x0b"
    return Case(
        name=f"{index:04d}.memory_i32",
        types=[([], [])],
        funcs=[Func(0, [], bytes(c))],
        has_memory=True,
        memory_min=1,
        memory_max=2,
        memory_has_max=True,
    )


def i64_case(rng: random.Random, index: int) -> Case:
    a = rng.getrandbits(40)
    b = rng.getrandbits(40)
    shift = rng.randrange(64)
    expected = ((a + b) ^ ((a << shift) & 0xFFFFFFFFFFFFFFFF)) & 0xFFFFFFFFFFFFFFFF
    expected_signed = expected - (1 << 64) if expected & (1 << 63) else expected

    c = bytearray()
    c += i64_const(a)
    c += i64_const(b)
    c += b"\x7C"  # i64.add
    c += i64_const(a)
    c += i64_const(shift)
    c += b"\x86"  # i64.shl
    c += b"\x85"  # i64.xor
    c += check_i64(expected_signed)
    c += b"\x0b"
    return Case(
        name=f"{index:04d}.i64_mix",
        types=[([], [])],
        funcs=[Func(0, [], bytes(c))],
    )


def trap_cases(start: int) -> list[Case]:
    return [
        Case(
            name=f"{start:04d}.trap_unreachable",
            types=[([], [])],
            funcs=[Func(0, [], b"\x00\x0b")],
            expect_trap=True,
        ),
        Case(
            name=f"{start + 1:04d}.trap_div_zero",
            types=[([], [])],
            funcs=[Func(0, [], i32_const(7) + i32_const(0) + b"\x6D\x1A\x0b")],
            expect_trap=True,
        ),
        Case(
            name=f"{start + 2:04d}.trap_oob_load",
            types=[([], [])],
            funcs=[Func(0, [], i32_const(65536) + b"\x28" + memarg(2, 0) + b"\x1A\x0b")],
            has_memory=True,
            memory_min=1,
            memory_max=1,
            memory_has_max=True,
            expect_trap=True,
        ),
    ]


def make_cases(count: int, seed: int, include_traps: bool) -> list[Case]:
    rng = random.Random(seed)
    cases: list[Case] = []
    for i in range(count):
        kind = rng.randrange(100)
        if kind < 62:
            cases.append(random_i32_case(rng, len(cases), rng.randrange(48, 128)))
        elif kind < 78:
            cases.append(loop_case(rng, len(cases)))
        elif kind < 90:
            cases.append(memory_case(rng, len(cases)))
        else:
            cases.append(i64_case(rng, len(cases)))
    if include_traps:
        cases.extend(trap_cases(len(cases)))
    return cases


def flatten_cases(cases: list[Case]) -> dict[str, list[int] | list[dict[str, object]]]:
    value_types: list[int] = []
    local_decls: list[int] = []
    code_bytes: list[int] = []
    types: list[dict[str, int]] = []
    funcs: list[dict[str, int]] = []
    case_descs: list[dict[str, object]] = []

    for case in cases:
        type_begin = len(types)
        for params, results in case.types:
            pb = len(value_types)
            value_types.extend(params)
            rb = len(value_types)
            value_types.extend(results)
            types.append(
                {
                    "params_begin": pb,
                    "params_count": len(params),
                    "results_begin": rb,
                    "results_count": len(results),
                }
            )

        func_begin = len(funcs)
        for f in case.funcs:
            lb = len(local_decls) // 2
            for count, typ in f.local_groups:
                local_decls.extend([count, typ])
            cb = len(code_bytes)
            body = f.body
            code_bytes.extend(body)
            funcs.append(
                {
                    "type_index": type_begin + f.type_index,
                    "locals_begin": lb,
                    "locals_count": len(f.local_groups),
                    "code_begin": cb,
                    "code_size": len(body),
                    "expr_offset": f.expr_offset,
                }
            )

        case_descs.append(
            {
                "name": case.name,
                "type_begin": type_begin,
                "type_count": len(case.types),
                "func_begin": func_begin,
                "func_count": len(case.funcs),
                "entry_func_index": 0,
                "has_memory": case.has_memory,
                "memory_min": case.memory_min,
                "memory_max": case.memory_max,
                "memory_has_max": case.memory_has_max,
                "expect_trap": case.expect_trap,
            }
        )

    return {
        "value_types": value_types,
        "local_decls": local_decls,
        "code_bytes": code_bytes,
        "types": types,
        "funcs": funcs,
        "cases": case_descs,
    }


def cxx_array(name: str, typ: str, values: list[int], per_line: int = 24) -> str:
    chunks = []
    for i in range(0, len(values), per_line):
        chunks.append("    " + ", ".join(str(v) for v in values[i : i + per_line]))
    body = ",\n".join(chunks)
    if body:
        body += "\n"
    return f"inline constexpr std::array<{typ}, {len(values)}> {name}{{{{\n{body}}}}};\n"


def write_header(path: Path, flat: dict[str, object]) -> None:
    lines: list[str] = []
    lines.append("#pragma once\n")
    lines.append("#include <array>\n#include <cstddef>\n#include <cstdint>\n\n")
    lines.append("namespace uwvm2_backend_fuzzer_generated {\n")
    lines.append("struct type_desc { std::uint32_t params_begin; std::uint32_t params_count; std::uint32_t results_begin; std::uint32_t results_count; };\n")
    lines.append("struct func_desc { std::uint32_t type_index; std::uint32_t locals_begin; std::uint32_t locals_count; std::uint32_t code_begin; std::uint32_t code_size; std::uint32_t expr_offset; };\n")
    lines.append("struct case_desc { char const* name; std::uint32_t type_begin; std::uint32_t type_count; std::uint32_t func_begin; std::uint32_t func_count; std::uint32_t entry_func_index; bool has_memory; std::uint32_t memory_min; std::uint32_t memory_max; bool memory_has_max; bool expect_trap; };\n\n")
    lines.append(cxx_array("k_value_types", "std::uint8_t", flat["value_types"]))  # type: ignore[arg-type]
    lines.append(cxx_array("k_local_decls", "std::uint32_t", flat["local_decls"]))  # type: ignore[arg-type]
    lines.append(cxx_array("k_code_bytes", "std::uint8_t", flat["code_bytes"]))  # type: ignore[arg-type]

    types = flat["types"]  # type: ignore[assignment]
    lines.append(f"inline constexpr std::array<type_desc, {len(types)}> k_types{{{{\n")
    for t in types:  # type: ignore[union-attr]
        lines.append(
            f"    type_desc{{{t['params_begin']}u, {t['params_count']}u, {t['results_begin']}u, {t['results_count']}u}},\n"
        )
    lines.append("}};\n")

    funcs = flat["funcs"]  # type: ignore[assignment]
    lines.append(f"inline constexpr std::array<func_desc, {len(funcs)}> k_funcs{{{{\n")
    for f in funcs:  # type: ignore[union-attr]
        lines.append(
            f"    func_desc{{{f['type_index']}u, {f['locals_begin']}u, {f['locals_count']}u, {f['code_begin']}u, {f['code_size']}u, {f['expr_offset']}u}},\n"
        )
    lines.append("}};\n")

    cases = flat["cases"]  # type: ignore[assignment]
    lines.append(f"inline constexpr std::array<case_desc, {len(cases)}> k_cases{{{{\n")
    for c in cases:  # type: ignore[union-attr]
        name = str(c["name"]).replace("\\", "\\\\").replace('"', '\\"')
        lines.append(
            "    case_desc{"
            f"\"{name}\", {c['type_begin']}u, {c['type_count']}u, {c['func_begin']}u, {c['func_count']}u, {c['entry_func_index']}u, "
            f"{str(c['has_memory']).lower()}, {c['memory_min']}u, {c['memory_max']}u, {str(c['memory_has_max']).lower()}, {str(c['expect_trap']).lower()}"
            "},\n"
        )
    lines.append("}};\n")
    lines.append("} // namespace uwvm2_backend_fuzzer_generated\n")
    path.write_text("".join(lines), encoding="utf-8")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out-dir", required=True, type=Path)
    ap.add_argument("--cases", type=int, default=64)
    ap.add_argument("--seed", type=lambda s: int(s, 0), default=0xC0DEF00D)
    ap.add_argument("--include-traps", action=argparse.BooleanOptionalAction, default=True)
    ap.add_argument("--keep", action="store_true")
    args = ap.parse_args()

    if args.cases < 0:
        raise SystemExit("--cases must be non-negative")

    out_dir: Path = args.out_dir
    corpus_dir = out_dir / "corpus"
    generated_dir = out_dir / "generated"
    if not args.keep and out_dir.exists():
        shutil.rmtree(out_dir)
    corpus_dir.mkdir(parents=True, exist_ok=True)
    generated_dir.mkdir(parents=True, exist_ok=True)

    cases = make_cases(args.cases, args.seed, args.include_traps)
    manifest = []
    for idx, case in enumerate(cases):
        wasm_path = corpus_dir / f"{idx:04d}.{case.name.split('.', 1)[1]}.wasm"
        wasm_path.write_bytes(case.wasm())
        manifest.append(
            {
                "index": idx,
                "name": case.name,
                "wasm": str(wasm_path),
                "expect_trap": case.expect_trap,
            }
        )

    flat = flatten_cases(cases)
    write_header(generated_dir / "backend_fuzzer_cases.inc", flat)
    (out_dir / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print(f"generated {len(cases)} cases in {out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
