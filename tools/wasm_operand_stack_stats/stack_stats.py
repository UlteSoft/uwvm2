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

    def remaining(self) -> int:
        return self.end - self.pos

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
            result |= - (1 << shift)
        return result

    def read_name(self) -> str:
        n = self.read_uleb(32)
        raw = self.read_bytes(n)
        try:
            return raw.decode("utf-8")
        except UnicodeDecodeError as e:
            raise WasmParseError(f"invalid utf-8 name: {e}") from e


@dataclass(frozen=True)
class FuncType:
    params: int
    results: int


@dataclass
class WasmModule:
    types: List[FuncType]
    func_type_indices: List[int]  # includes imported functions first
    code_bodies: List[bytes]  # only for local (non-imported) functions


def _parse_limits(r: ByteReader) -> None:
    flags = r.read_u8()
    _ = r.read_uleb(32)  # min
    if flags & 0x01:
        _ = r.read_uleb(32)  # max


def _skip_global_type(r: ByteReader) -> None:
    _ = r.read_u8()  # valtype
    _ = r.read_u8()  # mut


def _skip_init_expr(r: ByteReader) -> None:
    # MVP init_expr is a sequence of const/get_global and ends with 0x0b.
    # We only need to skip bytes until the terminating end opcode.
    while True:
        if r.at_end():
            raise WasmParseError("EOF in init_expr")
        op = r.read_u8()
        if op == 0x0B:
            return
        if op == 0x41:  # i32.const
            _ = r.read_sleb(32)
        elif op == 0x42:  # i64.const
            _ = r.read_sleb(64)
        elif op == 0x43:  # f32.const
            _ = r.read_bytes(4)
        elif op == 0x44:  # f64.const
            _ = r.read_bytes(8)
        elif op == 0x23:  # global.get
            _ = r.read_uleb(32)
        else:
            raise WasmParseError(f"non-MVP init_expr opcode: 0x{op:02x}")


def parse_wasm_module(data: bytes) -> WasmModule:
    r = ByteReader(data)
    if r.read_bytes(4) != b"\x00asm":
        raise WasmParseError("bad wasm magic")
    if r.read_bytes(4) != b"\x01\x00\x00\x00":
        raise WasmParseError("unsupported wasm version (expected 1)")

    types: List[FuncType] = []
    func_type_indices: List[int] = []
    code_bodies: List[bytes] = []

    while not r.at_end():
        sec_id = r.read_u8()
        sec_size = r.read_uleb(32)
        sec_start = r.tell()
        sec_end = sec_start + sec_size
        if sec_end > r.end:
            raise WasmParseError("section size exceeds file length")
        sec = ByteReader(r.data, pos=sec_start, end=sec_end)

        if sec_id == 0:  # custom
            _ = sec.read_name()
        elif sec_id == 1:  # type
            cnt = sec.read_uleb(32)
            for _ in range(cnt):
                form = sec.read_u8()
                if form != 0x60:
                    raise WasmParseError(f"unsupported type form: 0x{form:02x}")
                pcnt = sec.read_uleb(32)
                for _ in range(pcnt):
                    _ = sec.read_u8()
                rcnt = sec.read_uleb(32)
                for _ in range(rcnt):
                    _ = sec.read_u8()
                types.append(FuncType(params=pcnt, results=rcnt))
        elif sec_id == 2:  # import
            cnt = sec.read_uleb(32)
            for _ in range(cnt):
                _ = sec.read_name()
                _ = sec.read_name()
                kind = sec.read_u8()
                if kind == 0x00:  # func
                    typeidx = sec.read_uleb(32)
                    func_type_indices.append(typeidx)
                elif kind == 0x01:  # table
                    _ = sec.read_u8()  # elemtype
                    _parse_limits(sec)
                elif kind == 0x02:  # mem
                    _parse_limits(sec)
                elif kind == 0x03:  # global
                    _skip_global_type(sec)
                else:
                    raise WasmParseError(f"unknown import kind: 0x{kind:02x}")
        elif sec_id == 3:  # function
            cnt = sec.read_uleb(32)
            for _ in range(cnt):
                func_type_indices.append(sec.read_uleb(32))
        elif sec_id == 4:  # table
            cnt = sec.read_uleb(32)
            for _ in range(cnt):
                _ = sec.read_u8()
                _parse_limits(sec)
        elif sec_id == 5:  # memory
            cnt = sec.read_uleb(32)
            for _ in range(cnt):
                _parse_limits(sec)
        elif sec_id == 6:  # global
            cnt = sec.read_uleb(32)
            for _ in range(cnt):
                _skip_global_type(sec)
                _skip_init_expr(sec)
        elif sec_id == 7:  # export
            cnt = sec.read_uleb(32)
            for _ in range(cnt):
                _ = sec.read_name()
                _ = sec.read_u8()
                _ = sec.read_uleb(32)
        elif sec_id == 8:  # start
            _ = sec.read_uleb(32)
        elif sec_id == 9:  # element
            cnt = sec.read_uleb(32)
            for _ in range(cnt):
                # MVP element segment: tableidx + init_expr + vec(funcidx)
                _ = sec.read_uleb(32)
                _skip_init_expr(sec)
                n = sec.read_uleb(32)
                for _ in range(n):
                    _ = sec.read_uleb(32)
        elif sec_id == 10:  # code
            cnt = sec.read_uleb(32)
            for _ in range(cnt):
                body_size = sec.read_uleb(32)
                body = sec.read_bytes(body_size)
                code_bodies.append(body)
        elif sec_id == 11:  # data
            cnt = sec.read_uleb(32)
            for _ in range(cnt):
                _ = sec.read_uleb(32)  # memidx
                _skip_init_expr(sec)
                n = sec.read_uleb(32)
                _ = sec.read_bytes(n)
        else:
            # Skip unknown sections (non-MVP proposals) but still move forward safely.
            pass

        if not sec.at_end():
            raise WasmParseError(f"section {sec_id} not fully consumed ({sec.remaining()} bytes left)")
        r.seek(sec_end)

    return WasmModule(types=types, func_type_indices=func_type_indices, code_bodies=code_bodies)


@dataclass
class CtrlFrame:
    base: int
    label_arity: int
    end_arity: int
    kind: str  # "function" | "block" | "loop" | "if" | "else"
    polymorphic_base: bool
    then_polymorphic_end: bool = False  # if/else only


def _block_arity(types: Sequence[FuncType], blocktype_s33: int) -> Tuple[int, int]:
    # Returns (params_arity, results_arity)
    if blocktype_s33 == -0x40:
        return (0, 0)
    if blocktype_s33 in (-1, -2, -3, -4):  # i32/i64/f32/f64
        return (0, 1)
    if blocktype_s33 >= 0:
        if blocktype_s33 >= len(types):
            raise WasmParseError(f"block typeidx out of range: {blocktype_s33}")
        ft = types[blocktype_s33]
        return (ft.params, ft.results)
    raise WasmParseError(f"invalid blocktype (s33): {blocktype_s33}")


def _pop(stack_h: int, n: int, is_polymorphic: bool, frame_base: int) -> int:
    if n <= 0:
        return stack_h
    if not is_polymorphic:
        if stack_h < n:
            raise WasmParseError(f"operand stack underflow: have {stack_h}, need {n}")
        return stack_h - n
    # Polymorphic: clamp pops at the current frame base.
    return max(frame_base, stack_h - n)


def _push(stack_h: int, n: int) -> int:
    if n <= 0:
        return stack_h
    return stack_h + n


def _read_memarg(sec: ByteReader) -> None:
    _ = sec.read_uleb(32)  # align
    _ = sec.read_uleb(32)  # offset


def _read_blocktype(sec: ByteReader) -> int:
    return sec.read_sleb(33)


def _read_br_table_immediates(sec: ByteReader) -> Tuple[List[int], int]:
    target_count = sec.read_uleb(32)
    targets: List[int] = []
    for _ in range(target_count):
        targets.append(sec.read_uleb(32))
    default = sec.read_uleb(32)
    return targets, default


@dataclass
class FunctionStats:
    instructions_counted: int = 0
    above_threshold: int = 0
    at_or_below_threshold: int = 0
    max_stack_height: int = 0


def trace_operand_stack_stats(
    module: WasmModule,
    threshold: int,
    include_structural: bool,
    per_function: bool,
) -> Tuple[FunctionStats, Optional[Dict[int, FunctionStats]]]:
    if threshold < 0:
        raise ValueError("threshold must be >= 0")

    imported_func_count = len(module.func_type_indices) - len(module.code_bodies)
    if imported_func_count < 0:
        imported_func_count = 0

    func_sigs: List[FuncType] = []
    for typeidx in module.func_type_indices:
        if typeidx >= len(module.types):
            raise WasmParseError(f"function typeidx out of range: {typeidx}")
        func_sigs.append(module.types[typeidx])

    if len(module.code_bodies) != (len(module.func_type_indices) - imported_func_count):
        # Tolerate malformed modules; a strict parser would error here.
        pass

    total = FunctionStats()
    per_func_out: Optional[Dict[int, FunctionStats]] = {} if per_function else None

    def record(func_stats: FunctionStats, stack_h_after: int, opcode: int) -> None:
        if not include_structural and opcode in (0x02, 0x03, 0x04, 0x05, 0x0B):
            return
        func_stats.instructions_counted += 1
        func_stats.max_stack_height = max(func_stats.max_stack_height, stack_h_after)
        if stack_h_after > threshold:
            func_stats.above_threshold += 1
        else:
            func_stats.at_or_below_threshold += 1

    for local_idx, body in enumerate(module.code_bodies):
        func_index = imported_func_count + local_idx
        func_type = func_sigs[func_index] if func_index < len(func_sigs) else FuncType(0, 0)
        stats = FunctionStats()

        sec = ByteReader(body)
        local_decl_count = sec.read_uleb(32)
        for _ in range(local_decl_count):
            _ = sec.read_uleb(32)  # count
            _ = sec.read_u8()  # valtype

        ctrl_stack: List[CtrlFrame] = [
            CtrlFrame(
                base=0,
                label_arity=func_type.results,
                end_arity=func_type.results,
                kind="function",
                polymorphic_base=False,
            )
        ]
        is_polymorphic = False
        stack_h = 0

        while not sec.at_end():
            op_pos = sec.tell()
            opcode = sec.read_u8()
            frame_base = ctrl_stack[-1].base

            if opcode == 0x00:  # unreachable
                is_polymorphic = True
            elif opcode == 0x01:  # nop
                pass
            elif opcode in (0x02, 0x03, 0x04):  # block/loop/if
                blocktype = _read_blocktype(sec)
                params_arity, results_arity = _block_arity(module.types, blocktype)
                if opcode == 0x04:  # if: pop cond
                    stack_h = _pop(stack_h, 1, is_polymorphic, frame_base)
                stack_h = _pop(stack_h, params_arity, is_polymorphic, frame_base)
                kind = "block" if opcode == 0x02 else ("loop" if opcode == 0x03 else "if")
                label_arity = params_arity if kind == "loop" else results_arity
                ctrl_stack.append(
                    CtrlFrame(
                        base=stack_h,
                        label_arity=label_arity,
                        end_arity=results_arity,
                        kind=kind,
                        polymorphic_base=is_polymorphic,
                    )
                )
                # Note: in MVP, block/loop/if do not push results here.
            elif opcode == 0x05:  # else
                if not ctrl_stack:
                    raise WasmParseError("else without control frame")
                frame = ctrl_stack[-1]
                if frame.kind != "if":
                    raise WasmParseError("else outside if")
                frame.then_polymorphic_end = is_polymorphic
                # Reset stack to if entry.
                stack_h = frame.base
                is_polymorphic = frame.polymorphic_base
                frame.kind = "else"
            elif opcode == 0x0B:  # end
                if not ctrl_stack:
                    raise WasmParseError("end without control frame")
                frame = ctrl_stack.pop()

                # Validate / normalize stack at end of frame: keep only declared results.
                if not is_polymorphic:
                    if stack_h < frame.base + frame.end_arity:
                        raise WasmParseError(
                            f"end result underflow: have {stack_h - frame.base}, need {frame.end_arity}"
                        )
                    if stack_h != frame.base + frame.end_arity:
                        raise WasmParseError(
                            f"end result mismatch: have {stack_h - frame.base}, need {frame.end_arity}"
                        )

                stack_h = frame.base + frame.end_arity

                # Restore / merge polymorphic state.
                if frame.kind == "else":
                    is_polymorphic = frame.polymorphic_base or (frame.then_polymorphic_end and is_polymorphic)
                else:
                    is_polymorphic = frame.polymorphic_base

                if frame.kind == "function":
                    if not sec.at_end():
                        raise WasmParseError("trailing bytes after function end")
            elif opcode == 0x0C:  # br
                label_index = sec.read_uleb(32)
                if label_index >= len(ctrl_stack):
                    raise WasmParseError(f"illegal label index: {label_index}")
                target = ctrl_stack[-1 - label_index]
                stack_h = _pop(stack_h, target.label_arity, is_polymorphic, frame_base)
                stack_h = frame_base
                is_polymorphic = True
            elif opcode == 0x0D:  # br_if
                label_index = sec.read_uleb(32)
                if label_index >= len(ctrl_stack):
                    raise WasmParseError(f"illegal label index: {label_index}")
                _ = ctrl_stack[-1 - label_index]
                stack_h = _pop(stack_h, 1, is_polymorphic, frame_base)  # cond
            elif opcode == 0x0E:  # br_table
                targets, default = _read_br_table_immediates(sec)
                if default >= len(ctrl_stack):
                    raise WasmParseError(f"illegal label index: {default}")
                for li in targets:
                    if li >= len(ctrl_stack):
                        raise WasmParseError(f"illegal label index: {li}")
                expected_arity = ctrl_stack[-1 - default].label_arity
                stack_h = _pop(stack_h, 1, is_polymorphic, frame_base)  # idx
                stack_h = _pop(stack_h, expected_arity, is_polymorphic, frame_base)
                stack_h = frame_base
                is_polymorphic = True
            elif opcode == 0x0F:  # return
                return_arity = ctrl_stack[0].end_arity
                stack_h = _pop(stack_h, return_arity, is_polymorphic, frame_base)
                stack_h = frame_base
                is_polymorphic = True
            elif opcode == 0x10:  # call
                funcidx = sec.read_uleb(32)
                if funcidx >= len(func_sigs):
                    raise WasmParseError(f"call funcidx out of range: {funcidx}")
                ft = func_sigs[funcidx]
                stack_h = _pop(stack_h, ft.params, is_polymorphic, frame_base)
                stack_h = _push(stack_h, ft.results)
            elif opcode == 0x11:  # call_indirect
                typeidx = sec.read_uleb(32)
                tableidx = sec.read_uleb(32)
                if typeidx >= len(module.types):
                    raise WasmParseError(f"call_indirect typeidx out of range: {typeidx}")
                ft = module.types[typeidx]
                stack_h = _pop(stack_h, 1, is_polymorphic, frame_base)  # table element index
                stack_h = _pop(stack_h, ft.params, is_polymorphic, frame_base)
                stack_h = _push(stack_h, ft.results)
            elif opcode == 0x1A:  # drop
                stack_h = _pop(stack_h, 1, is_polymorphic, frame_base)
            elif opcode == 0x1B:  # select
                stack_h = _pop(stack_h, 3, is_polymorphic, frame_base)
                stack_h = _push(stack_h, 1)
            elif opcode == 0x20:  # local.get
                _ = sec.read_uleb(32)
                stack_h = _push(stack_h, 1)
            elif opcode == 0x21:  # local.set
                _ = sec.read_uleb(32)
                stack_h = _pop(stack_h, 1, is_polymorphic, frame_base)
            elif opcode == 0x22:  # local.tee
                _ = sec.read_uleb(32)
                stack_h = _pop(stack_h, 1, is_polymorphic, frame_base)
                stack_h = _push(stack_h, 1)
            elif opcode == 0x23:  # global.get
                _ = sec.read_uleb(32)
                stack_h = _push(stack_h, 1)
            elif opcode == 0x24:  # global.set
                _ = sec.read_uleb(32)
                stack_h = _pop(stack_h, 1, is_polymorphic, frame_base)
            elif 0x28 <= opcode <= 0x35:  # loads
                _read_memarg(sec)
                stack_h = _pop(stack_h, 1, is_polymorphic, frame_base)
                stack_h = _push(stack_h, 1)
            elif 0x36 <= opcode <= 0x3E:  # stores
                _read_memarg(sec)
                stack_h = _pop(stack_h, 2, is_polymorphic, frame_base)
            elif opcode == 0x3F:  # memory.size
                memidx = sec.read_u8()
                if memidx != 0:
                    raise WasmParseError("non-zero memidx in memory.size (MVP supports only 0)")
                stack_h = _push(stack_h, 1)
            elif opcode == 0x40:  # memory.grow
                memidx = sec.read_u8()
                if memidx != 0:
                    raise WasmParseError("non-zero memidx in memory.grow (MVP supports only 0)")
                stack_h = _pop(stack_h, 1, is_polymorphic, frame_base)
                stack_h = _push(stack_h, 1)
            elif opcode == 0x41:  # i32.const
                _ = sec.read_sleb(32)
                stack_h = _push(stack_h, 1)
            elif opcode == 0x42:  # i64.const
                _ = sec.read_sleb(64)
                stack_h = _push(stack_h, 1)
            elif opcode == 0x43:  # f32.const
                _ = sec.read_bytes(4)
                stack_h = _push(stack_h, 1)
            elif opcode == 0x44:  # f64.const
                _ = sec.read_bytes(8)
                stack_h = _push(stack_h, 1)
            elif opcode == 0x45:  # i32.eqz
                stack_h = _pop(stack_h, 1, is_polymorphic, frame_base)
                stack_h = _push(stack_h, 1)
            elif 0x46 <= opcode <= 0x4F:  # i32 comparisons
                stack_h = _pop(stack_h, 2, is_polymorphic, frame_base)
                stack_h = _push(stack_h, 1)
            elif opcode == 0x50:  # i64.eqz
                stack_h = _pop(stack_h, 1, is_polymorphic, frame_base)
                stack_h = _push(stack_h, 1)
            elif 0x51 <= opcode <= 0x5A:  # i64 comparisons
                stack_h = _pop(stack_h, 2, is_polymorphic, frame_base)
                stack_h = _push(stack_h, 1)
            elif 0x5B <= opcode <= 0x60:  # f32 comparisons
                stack_h = _pop(stack_h, 2, is_polymorphic, frame_base)
                stack_h = _push(stack_h, 1)
            elif 0x61 <= opcode <= 0x66:  # f64 comparisons
                stack_h = _pop(stack_h, 2, is_polymorphic, frame_base)
                stack_h = _push(stack_h, 1)
            elif opcode in (0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78):
                # i32 binary ops
                stack_h = _pop(stack_h, 2, is_polymorphic, frame_base)
                stack_h = _push(stack_h, 1)
            elif opcode in (0x67, 0x68, 0x69):  # i32 unary ops
                stack_h = _pop(stack_h, 1, is_polymorphic, frame_base)
                stack_h = _push(stack_h, 1)
            elif opcode in (0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A):
                # i64 binary ops
                stack_h = _pop(stack_h, 2, is_polymorphic, frame_base)
                stack_h = _push(stack_h, 1)
            elif opcode in (0x79, 0x7A, 0x7B):  # i64 unary ops
                stack_h = _pop(stack_h, 1, is_polymorphic, frame_base)
                stack_h = _push(stack_h, 1)
            elif 0x8B <= opcode <= 0x91:  # f32 unary
                stack_h = _pop(stack_h, 1, is_polymorphic, frame_base)
                stack_h = _push(stack_h, 1)
            elif 0x92 <= opcode <= 0x98:  # f32 binary
                stack_h = _pop(stack_h, 2, is_polymorphic, frame_base)
                stack_h = _push(stack_h, 1)
            elif 0x99 <= opcode <= 0x9F:  # f64 unary
                stack_h = _pop(stack_h, 1, is_polymorphic, frame_base)
                stack_h = _push(stack_h, 1)
            elif 0xA0 <= opcode <= 0xA6:  # f64 binary
                stack_h = _pop(stack_h, 2, is_polymorphic, frame_base)
                stack_h = _push(stack_h, 1)
            elif 0xA7 <= opcode <= 0xBF:  # conversions / reinterpret
                stack_h = _pop(stack_h, 1, is_polymorphic, frame_base)
                stack_h = _push(stack_h, 1)
            elif 0xC0 <= opcode <= 0xC4:  # sign-extension ops
                stack_h = _pop(stack_h, 1, is_polymorphic, frame_base)
                stack_h = _push(stack_h, 1)
            elif opcode == 0xFC:  # prefixed: saturating trunc / bulk memory / table ops
                sub = sec.read_uleb(32)
                if 0x00 <= sub <= 0x07:
                    # i32/i64.trunc_sat_*: unary (pop 1, push 1)
                    stack_h = _pop(stack_h, 1, is_polymorphic, frame_base)
                    stack_h = _push(stack_h, 1)
                elif sub == 0x08:  # memory.init dataidx memidx
                    _ = sec.read_uleb(32)
                    _ = sec.read_uleb(32)
                    stack_h = _pop(stack_h, 3, is_polymorphic, frame_base)
                elif sub == 0x09:  # data.drop dataidx
                    _ = sec.read_uleb(32)
                elif sub == 0x0A:  # memory.copy memidx memidx
                    _ = sec.read_uleb(32)
                    _ = sec.read_uleb(32)
                    stack_h = _pop(stack_h, 3, is_polymorphic, frame_base)
                elif sub == 0x0B:  # memory.fill memidx
                    _ = sec.read_uleb(32)
                    stack_h = _pop(stack_h, 3, is_polymorphic, frame_base)
                elif sub == 0x0C:  # table.init elemidx tableidx
                    _ = sec.read_uleb(32)
                    _ = sec.read_uleb(32)
                    stack_h = _pop(stack_h, 3, is_polymorphic, frame_base)
                elif sub == 0x0D:  # elem.drop elemidx
                    _ = sec.read_uleb(32)
                elif sub == 0x0E:  # table.copy tableidx tableidx
                    _ = sec.read_uleb(32)
                    _ = sec.read_uleb(32)
                    stack_h = _pop(stack_h, 3, is_polymorphic, frame_base)
                elif sub == 0x0F:  # table.grow tableidx
                    _ = sec.read_uleb(32)
                    stack_h = _pop(stack_h, 2, is_polymorphic, frame_base)
                    stack_h = _push(stack_h, 1)
                elif sub == 0x10:  # table.size tableidx
                    _ = sec.read_uleb(32)
                    stack_h = _push(stack_h, 1)
                elif sub == 0x11:  # table.fill tableidx
                    _ = sec.read_uleb(32)
                    stack_h = _pop(stack_h, 3, is_polymorphic, frame_base)
                else:
                    raise WasmParseError(f"unsupported 0xFC subopcode 0x{sub:x} at body+0x{op_pos:x}")
            else:
                raise WasmParseError(f"unsupported/non-MVP opcode 0x{opcode:02x} at body+0x{op_pos:x}")

            record(stats, stack_h, opcode)

        total.instructions_counted += stats.instructions_counted
        total.above_threshold += stats.above_threshold
        total.at_or_below_threshold += stats.at_or_below_threshold
        total.max_stack_height = max(total.max_stack_height, stats.max_stack_height)

        if per_func_out is not None:
            per_func_out[func_index] = stats

    return total, per_func_out


def main(argv: Optional[Sequence[str]] = None) -> int:
    ap = argparse.ArgumentParser(description="Count operand stack heights after each Wasm1 opcode in code section.")
    ap.add_argument("wasm", help="Input .wasm file path")
    ap.add_argument("--threshold", type=int, required=True, help="Stack height threshold (exceed if height > threshold)")
    ap.add_argument("--include-structural", action="store_true", help="Also count block/loop/if/else/end opcodes")
    ap.add_argument("--per-function", action="store_true", help="Output per-function stats (local functions)")
    ap.add_argument("--json", action="store_true", help="Output as JSON")
    args = ap.parse_args(argv)

    wasm_path = args.wasm
    with open(wasm_path, "rb") as f:
        data = f.read()

    module = parse_wasm_module(data)
    total, per_func = trace_operand_stack_stats(
        module,
        threshold=args.threshold,
        include_structural=args.include_structural,
        per_function=args.per_function,
    )

    payload = {
        "wasm": os.path.abspath(wasm_path),
        "threshold": args.threshold,
        "include_structural": bool(args.include_structural),
        "instructions_counted": total.instructions_counted,
        "above_threshold": total.above_threshold,
        "at_or_below_threshold": total.at_or_below_threshold,
        "max_stack_height": total.max_stack_height,
    }
    if per_func is not None:
        payload["per_function"] = {
            str(k): {
                "instructions_counted": v.instructions_counted,
                "above_threshold": v.above_threshold,
                "at_or_below_threshold": v.at_or_below_threshold,
                "max_stack_height": v.max_stack_height,
            }
            for k, v in per_func.items()
        }

    if args.json:
        print(json.dumps(payload, ensure_ascii=False, indent=2))
    else:
        print(f"wasm: {payload['wasm']}")
        print(f"threshold: {payload['threshold']}")
        print(f"include_structural: {payload['include_structural']}")
        print(f"instructions_counted: {payload['instructions_counted']}")
        print(f"> threshold: {payload['above_threshold']}")
        print(f"<= threshold: {payload['at_or_below_threshold']}")
        print(f"max_stack_height: {payload['max_stack_height']}")
        if per_func is not None:
            print(f"functions: {len(per_func)}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
