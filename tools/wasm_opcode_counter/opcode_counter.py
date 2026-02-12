#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from __future__ import annotations

import argparse
import json
import os
from dataclasses import dataclass
from typing import Dict, List, Optional, Sequence, Tuple


class WasmParseError(RuntimeError):
    pass


class ByteReader:
    __slots__ = ("data", "pos", "end")

    def __init__(self, data: bytes, pos: int = 0, end: Optional[int] = None) -> None:
        self.data = data
        self.pos = pos
        self.end = len(data) if end is None else end

    def at_end(self) -> bool:
        return self.pos >= self.end

    def tell(self) -> int:
        return self.pos

    def seek(self, pos: int) -> None:
        if pos < 0 or pos > self.end:
            raise WasmParseError(f"seek out of range: {pos}")
        self.pos = pos

    def read_u8(self) -> int:
        if self.pos + 1 > self.end:
            raise WasmParseError("unexpected EOF while reading u8")
        b = self.data[self.pos]
        self.pos += 1
        return b

    def read_bytes(self, n: int) -> bytes:
        if n < 0 or self.pos + n > self.end:
            raise WasmParseError("unexpected EOF while reading bytes")
        out = self.data[self.pos : self.pos + n]
        self.pos += n
        return out

    def read_uleb(self, max_bits: int = 32) -> int:
        result = 0
        shift = 0
        while True:
            if shift >= max_bits + 7:
                raise WasmParseError("uleb128 too large")
            b = self.read_u8()
            result |= (b & 0x7F) << shift
            if (b & 0x80) == 0:
                return result
            shift += 7

    def read_sleb(self, max_bits: int = 32) -> int:
        result = 0
        shift = 0
        size = max_bits
        byte = 0
        while True:
            if shift >= max_bits + 7:
                raise WasmParseError("sleb128 too large")
            byte = self.read_u8()
            result |= (byte & 0x7F) << shift
            shift += 7
            if (byte & 0x80) == 0:
                break
        if shift < size and (byte & 0x40) != 0:
            result |= -(1 << shift)
        return result


def parse_code_section_bodies(data: bytes) -> List[bytes]:
    r = ByteReader(data)
    if r.read_bytes(4) != b"\x00asm":
        raise WasmParseError("bad wasm magic")
    if r.read_bytes(4) != b"\x01\x00\x00\x00":
        raise WasmParseError("unsupported wasm version (expected 1)")

    code_bodies: List[bytes] = []
    while not r.at_end():
        sec_id = r.read_u8()
        sec_size = r.read_uleb(32)
        sec_start = r.tell()
        sec_end = sec_start + sec_size
        if sec_end > r.end:
            raise WasmParseError("section size exceeds file length")
        sec = ByteReader(r.data, pos=sec_start, end=sec_end)
        if sec_id == 10:  # code
            cnt = sec.read_uleb(32)
            for _ in range(cnt):
                body_size = sec.read_uleb(32)
                body = sec.read_bytes(body_size)
                code_bodies.append(body)
        r.seek(sec_end)
    return code_bodies


OPCODE_NAME: Dict[int, str] = {
    0x00: "unreachable",
    0x01: "nop",
    0x02: "block",
    0x03: "loop",
    0x04: "if",
    0x05: "else",
    0x0B: "end",
    0x0C: "br",
    0x0D: "br_if",
    0x0E: "br_table",
    0x0F: "return",
    0x10: "call",
    0x11: "call_indirect",
    0x1A: "drop",
    0x1B: "select",
    0x1C: "select.t",
    0x20: "local.get",
    0x21: "local.set",
    0x22: "local.tee",
    0x23: "global.get",
    0x24: "global.set",
    # Reference types (commonly enabled in toolchains)
    0x25: "table.get",
    0x26: "table.set",
    # Memory
    0x28: "i32.load",
    0x29: "i64.load",
    0x2A: "f32.load",
    0x2B: "f64.load",
    0x2C: "i32.load8_s",
    0x2D: "i32.load8_u",
    0x2E: "i32.load16_s",
    0x2F: "i32.load16_u",
    0x30: "i64.load8_s",
    0x31: "i64.load8_u",
    0x32: "i64.load16_s",
    0x33: "i64.load16_u",
    0x34: "i64.load32_s",
    0x35: "i64.load32_u",
    0x36: "i32.store",
    0x37: "i64.store",
    0x38: "f32.store",
    0x39: "f64.store",
    0x3A: "i32.store8",
    0x3B: "i32.store16",
    0x3C: "i64.store8",
    0x3D: "i64.store16",
    0x3E: "i64.store32",
    0x3F: "memory.size",
    0x40: "memory.grow",
    # Const
    0x41: "i32.const",
    0x42: "i64.const",
    0x43: "f32.const",
    0x44: "f64.const",
    # Comparisons
    0x45: "i32.eqz",
    0x46: "i32.eq",
    0x47: "i32.ne",
    0x48: "i32.lt_s",
    0x49: "i32.lt_u",
    0x4A: "i32.gt_s",
    0x4B: "i32.gt_u",
    0x4C: "i32.le_s",
    0x4D: "i32.le_u",
    0x4E: "i32.ge_s",
    0x4F: "i32.ge_u",
    0x50: "i64.eqz",
    0x51: "i64.eq",
    0x52: "i64.ne",
    0x53: "i64.lt_s",
    0x54: "i64.lt_u",
    0x55: "i64.gt_s",
    0x56: "i64.gt_u",
    0x57: "i64.le_s",
    0x58: "i64.le_u",
    0x59: "i64.ge_s",
    0x5A: "i64.ge_u",
    0x5B: "f32.eq",
    0x5C: "f32.ne",
    0x5D: "f32.lt",
    0x5E: "f32.gt",
    0x5F: "f32.le",
    0x60: "f32.ge",
    0x61: "f64.eq",
    0x62: "f64.ne",
    0x63: "f64.lt",
    0x64: "f64.gt",
    0x65: "f64.le",
    0x66: "f64.ge",
    # i32 unary
    0x67: "i32.clz",
    0x68: "i32.ctz",
    0x69: "i32.popcnt",
    # i32 binary
    0x6A: "i32.add",
    0x6B: "i32.sub",
    0x6C: "i32.mul",
    0x6D: "i32.div_s",
    0x6E: "i32.div_u",
    0x6F: "i32.rem_s",
    0x70: "i32.rem_u",
    0x71: "i32.and",
    0x72: "i32.or",
    0x73: "i32.xor",
    0x74: "i32.shl",
    0x75: "i32.shr_s",
    0x76: "i32.shr_u",
    0x77: "i32.rotl",
    0x78: "i32.rotr",
    # i64 unary
    0x79: "i64.clz",
    0x7A: "i64.ctz",
    0x7B: "i64.popcnt",
    # i64 binary
    0x7C: "i64.add",
    0x7D: "i64.sub",
    0x7E: "i64.mul",
    0x7F: "i64.div_s",
    0x80: "i64.div_u",
    0x81: "i64.rem_s",
    0x82: "i64.rem_u",
    0x83: "i64.and",
    0x84: "i64.or",
    0x85: "i64.xor",
    0x86: "i64.shl",
    0x87: "i64.shr_s",
    0x88: "i64.shr_u",
    0x89: "i64.rotl",
    0x8A: "i64.rotr",
    # f32 unary
    0x8B: "f32.abs",
    0x8C: "f32.neg",
    0x8D: "f32.ceil",
    0x8E: "f32.floor",
    0x8F: "f32.trunc",
    0x90: "f32.nearest",
    0x91: "f32.sqrt",
    # f32 binary
    0x92: "f32.add",
    0x93: "f32.sub",
    0x94: "f32.mul",
    0x95: "f32.div",
    0x96: "f32.min",
    0x97: "f32.max",
    0x98: "f32.copysign",
    # f64 unary
    0x99: "f64.abs",
    0x9A: "f64.neg",
    0x9B: "f64.ceil",
    0x9C: "f64.floor",
    0x9D: "f64.trunc",
    0x9E: "f64.nearest",
    0x9F: "f64.sqrt",
    # f64 binary
    0xA0: "f64.add",
    0xA1: "f64.sub",
    0xA2: "f64.mul",
    0xA3: "f64.div",
    0xA4: "f64.min",
    0xA5: "f64.max",
    0xA6: "f64.copysign",
    # conversions / reinterpret
    0xA7: "i32.wrap_i64",
    0xA8: "i32.trunc_f32_s",
    0xA9: "i32.trunc_f32_u",
    0xAA: "i32.trunc_f64_s",
    0xAB: "i32.trunc_f64_u",
    0xAC: "i64.extend_i32_s",
    0xAD: "i64.extend_i32_u",
    0xAE: "i64.trunc_f32_s",
    0xAF: "i64.trunc_f32_u",
    0xB0: "i64.trunc_f64_s",
    0xB1: "i64.trunc_f64_u",
    0xB2: "f32.convert_i32_s",
    0xB3: "f32.convert_i32_u",
    0xB4: "f32.convert_i64_s",
    0xB5: "f32.convert_i64_u",
    0xB6: "f32.demote_f64",
    0xB7: "f64.convert_i32_s",
    0xB8: "f64.convert_i32_u",
    0xB9: "f64.convert_i64_s",
    0xBA: "f64.convert_i64_u",
    0xBB: "f64.promote_f32",
    0xBC: "i32.reinterpret_f32",
    0xBD: "i64.reinterpret_f64",
    0xBE: "f32.reinterpret_i32",
    0xBF: "f64.reinterpret_i64",
    # sign-extension ops
    0xC0: "i32.extend8_s",
    0xC1: "i32.extend16_s",
    0xC2: "i64.extend8_s",
    0xC3: "i64.extend16_s",
    0xC4: "i64.extend32_s",
    # reference ops
    0xD0: "ref.null",
    0xD1: "ref.is_null",
    0xD2: "ref.func",
    # prefix
    0xFC: "0xfc",
}


FC_SUBOP_NAME: Dict[int, str] = {
    0x00: "i32.trunc_sat_f32_s",
    0x01: "i32.trunc_sat_f32_u",
    0x02: "i32.trunc_sat_f64_s",
    0x03: "i32.trunc_sat_f64_u",
    0x04: "i64.trunc_sat_f32_s",
    0x05: "i64.trunc_sat_f32_u",
    0x06: "i64.trunc_sat_f64_s",
    0x07: "i64.trunc_sat_f64_u",
    0x08: "memory.init",
    0x09: "data.drop",
    0x0A: "memory.copy",
    0x0B: "memory.fill",
    0x0C: "table.init",
    0x0D: "elem.drop",
    0x0E: "table.copy",
    0x0F: "table.grow",
    0x10: "table.size",
    0x11: "table.fill",
}


STRUCTURAL = {0x02, 0x03, 0x04, 0x05, 0x0B}


def _skip_memarg(r: ByteReader) -> None:
    _ = r.read_uleb(32)  # align
    _ = r.read_uleb(32)  # offset


def _skip_blocktype(r: ByteReader) -> None:
    _ = r.read_sleb(33)


def _skip_br_table(r: ByteReader) -> None:
    target_count = r.read_uleb(32)
    for _ in range(target_count):
        _ = r.read_uleb(32)
    _ = r.read_uleb(32)  # default


def _skip_vec_valtype(r: ByteReader) -> None:
    n = r.read_uleb(32)
    for _ in range(n):
        _ = r.read_u8()


def decode_and_skip_immediates(opcode: int, r: ByteReader) -> str:
    if opcode in (0x02, 0x03, 0x04):  # block/loop/if
        _skip_blocktype(r)
        return OPCODE_NAME.get(opcode, f"opcode_0x{opcode:02x}")
    if opcode in (0x0C, 0x0D, 0x10, 0x20, 0x21, 0x22, 0x23, 0x24, 0xD2):  # u32 immediates
        _ = r.read_uleb(32)
        return OPCODE_NAME.get(opcode, f"opcode_0x{opcode:02x}")
    if opcode == 0x0E:  # br_table
        _skip_br_table(r)
        return OPCODE_NAME.get(opcode, f"opcode_0x{opcode:02x}")
    if opcode == 0x11:  # call_indirect: typeidx, tableidx
        _ = r.read_uleb(32)
        _ = r.read_uleb(32)
        return OPCODE_NAME.get(opcode, f"opcode_0x{opcode:02x}")
    if opcode == 0x1C:  # select.t
        _skip_vec_valtype(r)
        return OPCODE_NAME.get(opcode, "select.t")
    if opcode in (0x25, 0x26):  # table.get/set tableidx
        _ = r.read_uleb(32)
        return OPCODE_NAME.get(opcode, f"opcode_0x{opcode:02x}")
    if 0x28 <= opcode <= 0x3E:  # loads/stores
        _skip_memarg(r)
        return OPCODE_NAME.get(opcode, f"opcode_0x{opcode:02x}")
    if opcode in (0x3F, 0x40):  # memory.size/grow memidx
        _ = r.read_uleb(32)
        return OPCODE_NAME.get(opcode, f"opcode_0x{opcode:02x}")
    if opcode == 0x41:  # i32.const
        _ = r.read_sleb(32)
        return "i32.const"
    if opcode == 0x42:  # i64.const
        _ = r.read_sleb(64)
        return "i64.const"
    if opcode == 0x43:  # f32.const
        _ = r.read_bytes(4)
        return "f32.const"
    if opcode == 0x44:  # f64.const
        _ = r.read_bytes(8)
        return "f64.const"
    if opcode == 0xD0:  # ref.null heaptype (s33)
        _ = r.read_sleb(33)
        return "ref.null"
    if opcode == 0xFC:  # prefix
        sub = r.read_uleb(32)
        name = FC_SUBOP_NAME.get(sub, f"0xfc.sub_0x{sub:x}")
        # Skip immediates for supported subops
        if 0x00 <= sub <= 0x07:
            return name
        if sub == 0x08:  # memory.init dataidx memidx
            _ = r.read_uleb(32)
            _ = r.read_uleb(32)
            return name
        if sub == 0x09:  # data.drop dataidx
            _ = r.read_uleb(32)
            return name
        if sub == 0x0A:  # memory.copy memidx memidx
            _ = r.read_uleb(32)
            _ = r.read_uleb(32)
            return name
        if sub == 0x0B:  # memory.fill memidx
            _ = r.read_uleb(32)
            return name
        if sub == 0x0C:  # table.init elemidx tableidx
            _ = r.read_uleb(32)
            _ = r.read_uleb(32)
            return name
        if sub == 0x0D:  # elem.drop elemidx
            _ = r.read_uleb(32)
            return name
        if sub == 0x0E:  # table.copy tableidx tableidx
            _ = r.read_uleb(32)
            _ = r.read_uleb(32)
            return name
        if sub == 0x0F:  # table.grow tableidx
            _ = r.read_uleb(32)
            return name
        if sub == 0x10:  # table.size tableidx
            _ = r.read_uleb(32)
            return name
        if sub == 0x11:  # table.fill tableidx
            _ = r.read_uleb(32)
            return name
        raise WasmParseError(f"unsupported 0xFC subopcode 0x{sub:x}")

    # Most MVP numeric ops have no immediates.
    return OPCODE_NAME.get(opcode, f"opcode_0x{opcode:02x}")


@dataclass
class Counts:
    total: int = 0
    per_mnemonic: Dict[str, int] = None  # type: ignore[assignment]

    def __post_init__(self) -> None:
        if self.per_mnemonic is None:
            self.per_mnemonic = {}

    def add(self, mnemonic: str) -> None:
        self.total += 1
        self.per_mnemonic[mnemonic] = self.per_mnemonic.get(mnemonic, 0) + 1


def count_opcodes_in_code(
    code_bodies: Sequence[bytes],
    exclude_structural: bool,
    per_function: bool,
) -> Tuple[Counts, Optional[Dict[int, Counts]]]:
    total = Counts()
    per_func: Optional[Dict[int, Counts]] = {} if per_function else None

    for func_idx, body in enumerate(code_bodies):
        r = ByteReader(body)
        local_decl_count = r.read_uleb(32)
        for _ in range(local_decl_count):
            _ = r.read_uleb(32)  # count
            _ = r.read_u8()  # valtype

        func_counts = Counts()
        while not r.at_end():
            op_pos = r.tell()
            opcode = r.read_u8()
            mnemonic = decode_and_skip_immediates(opcode, r)

            if exclude_structural and opcode in STRUCTURAL:
                continue

            func_counts.add(mnemonic)
            total.add(mnemonic)

            if opcode == 0x00:  # unreachable (no immediate)
                pass

            if opcode == 0xFD:
                raise WasmParseError(f"SIMD prefix 0xFD not supported at body+0x{op_pos:x}")

        if per_func is not None:
            per_func[func_idx] = func_counts

    return total, per_func


def count_single_mnemonic_in_code(
    code_bodies: Sequence[bytes],
    exclude_structural: bool,
    mnemonic: str,
    per_function: bool,
) -> Tuple[int, Optional[Dict[int, int]]]:
    total = 0
    per_func: Optional[Dict[int, int]] = {} if per_function else None

    for func_idx, body in enumerate(code_bodies):
        r = ByteReader(body)
        local_decl_count = r.read_uleb(32)
        for _ in range(local_decl_count):
            _ = r.read_uleb(32)  # count
            _ = r.read_u8()  # valtype

        func_total = 0
        while not r.at_end():
            opcode = r.read_u8()
            decoded = decode_and_skip_immediates(opcode, r)
            if exclude_structural and opcode in STRUCTURAL:
                continue
            if decoded == mnemonic:
                func_total += 1
                total += 1

        if per_func is not None:
            per_func[func_idx] = func_total

    return total, per_func


def main(argv: Optional[Sequence[str]] = None) -> int:
    ap = argparse.ArgumentParser(description="Count opcode occurrences in the Wasm code section.")
    ap.add_argument("wasm", help="Input .wasm file path")
    mode = ap.add_mutually_exclusive_group(required=True)
    mode.add_argument("--mnemonic", help="Print only the count of this mnemonic (e.g. i32.const)")
    mode.add_argument("--top", type=int, help="Print top-N mnemonics (e.g. 50)")
    ap.add_argument("--exclude-structural", action="store_true", help="Exclude block/loop/if/else/end")
    ap.add_argument("--per-function", action="store_true", help="Also output per-function counts")
    ap.add_argument("--json", action="store_true", help="Output as JSON")
    args = ap.parse_args(argv)

    with open(args.wasm, "rb") as f:
        data = f.read()

    code_bodies = parse_code_section_bodies(data)
    wasm_abs = os.path.abspath(args.wasm)

    if args.mnemonic:
        count, per_func_count = count_single_mnemonic_in_code(
            code_bodies,
            exclude_structural=bool(args.exclude_structural),
            mnemonic=args.mnemonic,
            per_function=bool(args.per_function),
        )
        if args.json:
            payload = {"wasm": wasm_abs, "mnemonic": args.mnemonic, "count": count}
            if per_func_count is not None:
                payload["per_function"] = {str(k): v for k, v in per_func_count.items()}
            print(json.dumps(payload, ensure_ascii=False, indent=2))
        else:
            print(f"wasm: {wasm_abs}")
            print(f"{args.mnemonic}: {count}")
        return 0

    top = int(args.top or 0)
    if top < 0:
        raise SystemExit("--top must be >= 0")

    total, per_func = count_opcodes_in_code(
        code_bodies,
        exclude_structural=bool(args.exclude_structural),
        per_function=bool(args.per_function),
    )
    items = sorted(total.per_mnemonic.items(), key=lambda kv: (-kv[1], kv[0]))
    top_n = items[:top]

    payload = {
        "wasm": wasm_abs,
        "exclude_structural": bool(args.exclude_structural),
        "functions": len(code_bodies),
        "total_instructions_counted": total.total,
        "counts": dict(items),
    }
    if per_func is not None:
        payload["per_function"] = {str(k): v.per_mnemonic for k, v in per_func.items()}

    if args.json:
        print(json.dumps(payload, ensure_ascii=False, indent=2))
    else:
        print(f"wasm: {wasm_abs}")
        print(f"exclude_structural: {payload['exclude_structural']}")
        print(f"functions: {payload['functions']}")
        print(f"total_instructions_counted: {payload['total_instructions_counted']}")
        print(f"top {len(top_n)}:")
        for name, cnt in top_n:
            print(f"  {name}: {cnt}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
