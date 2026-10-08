#!/usr/bin/env python3
"""Attach a DAP IDE to the host-owned LLVM-full debugger broker.

The adapter is a host process. It never passes its broker capability, DAP
stdio, or IDE handles to the Wasm VM. Start secure_server.py,
secure_server_macos.py, or uwvm-debug-server.exe from the host first, then
attach with its private socket directory or Windows pipe and capability.
"""

import array
import base64
import ctypes
from decimal import Decimal
import json
import math
import os
from pathlib import Path
import queue
import re
import socket
import stat
import struct
import sys
import threading


MAX_DAP_MESSAGE = 1 << 20
MAX_COMMAND = 8448  # bounded WASIp1 text; VM retains other grammar limits
MAX_REPLY = 65536
THREAD_RE = re.compile(r"^thread (\d+) module=(\d+) function=(\d+) byte-offset=(\d+) generation=(\d+)$")
FRAME_RE = re.compile(r"^\s*#(\d+) module=(\d+) function=(\d+)\b")
SOURCE_RE = re.compile(r"^  source (.+):(\d+):(\d+)$")
NATIVE_PC_RE = re.compile(r"^  native-pc=0x([0-9a-fA-F]{1,16})$")
LOCAL_RE = re.compile(r"^local (\d+) (.+)$")
STOP_ID_RE = re.compile(r"^stop-id ([0-9]{1,20})$")
SOURCE_STOP_RE = re.compile(r"^source-stop ([0-9]{1,20})$")
SOURCE_INTEGER_RE = re.compile(r"^([iu])(8|16|32|64)=(-?(?:0|[1-9][0-9]{0,19}))$")
SOURCE_FLOAT_RE = re.compile(r"^f(32|64) bits=0x([0-9a-fA-F]{1,16})(?: value=(.{1,64}))?$")
BREAK_RE = re.compile(r"^breakpoint (\d+)\b")
WASM_LOCATION_RE = re.compile(r"^wasm:(\d{1,20}):(\d{1,20}):(\d{1,20})$")
WASM_MEMORY_RE = re.compile(r"^wasm-memory:(\d{1,20}):(\d{1,10}):(\d{1,20})$")
EXIT_RE = re.compile(r"^guest exited: (-?\d+)$")


def breakpoint_condition_suffix(point):
    """Bounded HOST syntax only; the controller validates/evaluates at a real stop."""
    condition = point.get("condition", "")
    if not isinstance(condition, str) or len(condition) > 256 or any(not 32 <= ord(c) < 127 for c in condition):
        raise ValueError("condition requires at most 256 printable ASCII bytes")
    return "" if condition == "" else f" if {condition}"


def breakpoint_ignore_count(point):
    """Translate permanent thresholds; never silently downgrade unsupported policies."""
    breakpoint_condition_suffix(point)
    if "logMessage" in point and point["logMessage"] != "":
        raise ValueError("logMessage breakpoints are not implemented")
    if "hitCondition" not in point or point["hitCondition"] == "":
        return None
    condition = point["hitCondition"]
    match = re.fullmatch(r"(>=|>)\s*(0|[1-9][0-9]{0,19})", condition) if isinstance(condition, str) else None
    if match is None:
        raise ValueError("hitCondition supports only >=N or >N; exact/modulo conditions are unavailable")
    threshold = int(match.group(2))
    if threshold >= (1 << 64):
        raise ValueError("hitCondition exceeds the unsigned 64-bit hit counter")
    if match.group(1) == ">" and threshold == (1 << 64) - 1:
        raise ValueError("hitCondition >UINT64_MAX cannot be represented by the saturating hit counter")
    return max(0, threshold - 1) if match.group(1) == ">=" else threshold


def validate_source_expression(expression):
    """Validate bounded read-only source syntax DATA, then form one CLI token.

    This mirrors source_dwarf_expression's unary/postfix precedence and hard
    32-step/32-nesting ceilings under the adapter's existing 256-byte limit.
    Canonical decimal indices retain the prior DAP rule (no redundant leading
    zeroes). No type, variable, address, reference, pause or read authority is
    created; the controller still authenticates the actual current stop/frame
    and copies every pointer target inside its coherent guest transaction.
    """
    if (not isinstance(expression, str) or not expression or len(expression) > 256
            or any(not 32 <= ord(char) < 127 for char in expression)):
        raise ValueError("bounded printable ASCII read-only source expression required")
    cursor, steps, terminal_builtin = 0, 0, False

    def space():
        nonlocal cursor
        while cursor < len(expression) and expression[cursor] == " ":
            cursor += 1

    def identifier(numeric_member=False):
        nonlocal cursor
        if cursor == len(expression):
            raise ValueError("source expression identifier required")
        first = expression[cursor]
        numeric = numeric_member and "0" <= first <= "9"
        if not numeric and first != "_" and not ("a" <= first <= "z" or "A" <= first <= "Z"):
            raise ValueError("source expression identifier required")
        cursor += 1
        while cursor < len(expression):
            char = expression[cursor]
            if numeric:
                valid = "0" <= char <= "9"
            else:
                valid = char == "_" or "a" <= char <= "z" or "A" <= char <= "Z" or "0" <= char <= "9"
            if not valid:
                break
            cursor += 1

    def add_steps(count):
        nonlocal steps
        if count > 32 - steps:
            raise ValueError("source expression exceeds 32 steps")
        steps += count

    def primary(depth):
        nonlocal cursor, terminal_builtin
        space()
        if cursor == len(expression):
            raise ValueError("source expression operand required")
        if expression[cursor] == "(":
            if depth >= 32:
                raise ValueError("source expression exceeds 32 nesting levels")
            cursor += 1
            unary(depth + 1)
            space()
            if cursor == len(expression) or expression[cursor] != ")":
                raise ValueError("source expression closing parenthesis required")
            cursor += 1
            return
        begin = cursor
        identifier()
        # Namespace separators and root segments are adjacent in the actual
        # C++ grammar. Validate before normalization; "ns ::p"/"ns:: p" fail.
        while cursor < len(expression) and expression[cursor] == ":":
            if not expression.startswith("::", cursor):
                raise ValueError("source expression namespace separator malformed")
            cursor += 2
            identifier()
        root = expression[begin:cursor]
        if root in ("len", "cap"):
            space()
            if cursor < len(expression) and expression[cursor] == "(":
                if depth >= 32:
                    raise ValueError("source expression exceeds 32 nesting levels")
                cursor += 1
                unary(depth + 1)
                if terminal_builtin:
                    raise ValueError("Go builtin argument requires one producer reference")
                space()
                if cursor == len(expression) or expression[cursor] != ")":
                    raise ValueError("Go builtin closing parenthesis required")
                cursor += 1
                add_steps(1)
                terminal_builtin = True

    def postfix(depth):
        nonlocal cursor
        primary(depth)
        while True:
            space()
            if cursor == len(expression):
                return
            char = expression[cursor]
            if terminal_builtin and char in ".[-":
                raise ValueError("Go builtin scalar has no source postfix")
            if expression.startswith(".*", cursor):
                add_steps(1)
                cursor += 2
            elif char in (".", "-"):
                arrow = char == "-"
                if arrow and not expression.startswith("->", cursor):
                    raise ValueError("source expression arrow malformed")
                add_steps(2 if arrow else 1)
                cursor += 2 if arrow else 1
                space()
                identifier(numeric_member=True)
            elif char == "[":
                add_steps(1)
                cursor += 1
                space()
                start = cursor
                if cursor < len(expression) and expression[cursor] == "-":
                    cursor += 1
                digits = cursor
                while cursor < len(expression) and "0" <= expression[cursor] <= "9":
                    cursor += 1
                if cursor == digits or cursor - digits > 1 and expression[digits] == "0":
                    raise ValueError("source expression canonical decimal index required")
                index = int(expression[start:cursor])
                if not -(1 << 63) <= index < (1 << 63):
                    raise ValueError("source expression index exceeds signed 64-bit range")
                space()
                if cursor == len(expression) or expression[cursor] != "]":
                    raise ValueError("source expression closing bracket required")
                cursor += 1
            else:
                return  # An owning primary alone consumes its closing ')'.

    def unary(depth):
        nonlocal cursor
        space()
        if cursor == len(expression):
            raise ValueError("source expression operand required")
        if expression[cursor] != "*":
            postfix(depth)
            return
        if depth >= 32:
            raise ValueError("source expression exceeds 32 nesting levels")
        cursor += 1
        unary(depth + 1)  # Postfix binds before this dereference: *p[1].
        if terminal_builtin:
            raise ValueError("Go builtin scalar cannot be dereferenced")
        add_steps(1)

    unary(0)
    space()
    if cursor != len(expression):
        raise ValueError("source expression has unsupported trailing tokens")
    # Spaces are discarded only AFTER the whole finite grammar accepts them.
    # No joined roots, line controls, calls, casts or injected commands survive.
    return expression.replace(" ", "")


def validate_source_evaluation_expression(expression):
    """Bound the controller's read-only scalar syntax before any broker I/O.

    Selector-only queries retain their established grammar and normalization.
    Scalar expressions keep their original spaces (Rust `as`, builtin types,
    character literals). This parser never evaluates, reads or creates a
    pointer; type, arithmetic and coherent guest-copy checks remain in the VM.
    """
    try:
        normalized = validate_source_expression(expression)
        # The scalar parser reserves a root sizeof token even when a selector
        # parser could read it as an identifier. Preserve keyword prefixes.
        if re.match(r"[*(]*sizeof(?:$|[^A-Za-z_0-9])", normalized):
            raise ValueError("sizeof requires its complete scalar operand")
        return normalized
    except ValueError:
        pass
    if (not isinstance(expression, str) or not expression or len(expression) > 256
            or any(not 32 <= ord(char) < 127 for char in expression)):
        raise ValueError("bounded printable ASCII read-only source expression required")
    numeric_types = {
        "float", "f32", "float32", "double", "f64", "float64", "bool", "_Bool",
        "char8_t", "char16_t", "char32_t", "char", "signed char", "i8", "int8", "unsigned char", "u8", "uint8", "byte",
        "short", "short int", "i16", "int16", "unsigned short", "u16", "uint16",
        "int", "signed int", "i32", "int32", "rune", "unsigned", "unsigned int", "u32", "uint32",
        "long long", "long long int", "i64", "int64", "unsigned long long", "u64", "uint64",
        "long", "long int", "isize", "intptr_t", "unsigned long", "usize", "uintptr_t", "size_t", "uint",
    }
    # Expand only the standard finite integer-specifier combinations, in any
    # order. The Zig-specific set below remains separate.
    from itertools import permutations
    for sign in ((), ("signed",), ("unsigned",)):
        for words in (("char",), ("int",), ("short",), ("short", "int"),
                      ("long",), ("long", "int"), ("long", "long"), ("long", "long", "int")):
            numeric_types.update(" ".join(order) for order in permutations(sign + words))
    numeric_types.add("signed")
    zig_types = {"i8", "u8", "i16", "u16", "i32", "u32", "i64", "u64", "isize", "usize", "f32", "f64"}
    operators = (("or", 1), ("and", 2), ("||", 1), ("&&", 2), ("|", 3), ("^", 4), ("&^", 10), ("&", 5),
                 ("==", 6), ("!=", 6), ("<=", 7), (">=", 7), ("<<", 8), (">>", 8),
                 ("<", 7), (">", 7), ("+", 9), ("-", 9), ("*", 10), ("/", 10), ("%", 10))
    decimal_digits = r"[0-9](?:['_]?[0-9])*"
    cursor, nodes = 0, 0

    def space():
        nonlocal cursor
        while cursor < len(expression) and expression[cursor] == " ":
            cursor += 1

    def consume(token):
        nonlocal cursor
        space()
        if expression.startswith(token, cursor):
            cursor += len(token)
            return True
        return False

    def named_open(name, opening):
        nonlocal cursor
        saved = cursor
        if consume(name) and consume(opening):
            return True
        cursor = saved
        return False

    def typename(text):
        # Keep word boundaries. The printable-ASCII gate above remains in force.
        return " ".join(text.split())

    def append():
        nonlocal nodes
        nodes += 1
        if nodes > 128:
            raise ValueError("source scalar expression exceeds 128 nodes")

    def require(token):
        if not consume(token):
            raise ValueError("source scalar expression delimiter required")

    def bounded(depth):
        if depth >= 32:
            raise ValueError("source scalar expression exceeds 32 nesting levels")

    def selector(text):
        normalized = validate_source_expression(text.strip())
        # A scalar leaf starts with a producer identifier. Unary dereference
        # is handled below so arithmetic can never manufacture a read plan.
        if not re.match(r"[A-Za-z_]", normalized):
            raise ValueError("source scalar reference requires a producer name")
        append()
        return normalized.count(".") + normalized.count("->") * 2 + normalized.count("[")

    def postfix(steps):
        nonlocal cursor
        changed = False
        while True:
            space()
            if expression.startswith(".*", cursor):
                cursor += 2
                cost = 1
            elif expression.startswith("->", cursor) or expression.startswith(".", cursor):
                cost = 2 if expression.startswith("->", cursor) else 1
                cursor += cost
                space()
                member = re.match(r"(?:[0-9]+|[A-Za-z_][A-Za-z_0-9]*)", expression[cursor:])
                if member is None:
                    raise ValueError("source scalar reference member required")
                cursor += len(member[0])
            elif expression.startswith("[", cursor):
                cursor += 1
                end = expression.find("]", cursor)
                token = expression[cursor:end].strip() if end >= 0 else ""
                if not re.fullmatch(r"-?(?:0|[1-9][0-9]*)", token):
                    raise ValueError("source scalar reference canonical decimal index required")
                if not -(1 << 63) <= int(token) < (1 << 63):
                    raise ValueError("source scalar reference index exceeds signed 64-bit range")
                cursor = end + 1
                cost = 1
            else:
                break
            if cost > 32 - steps:
                raise ValueError("source scalar reference exceeds 32 steps")
            steps += cost
            changed = True
        if changed:
            append()
        return steps

    def character(start):
        # Shared bounded ASCII token grammar, also used by sizeof's delimiter
        # probe. Numeric escapes consume their longest allowed digit sequence.
        prefix = next((p for p in ("u8", "u", "U") if expression.startswith(p + "'", start)), "")
        width = {"u8": 8, "u": 16, "U": 32}.get(prefix, 0)
        pos = start + len(prefix) + 1
        universal = False
        if pos >= len(expression):
            raise ValueError("character literal operand required")
        char = expression[pos]
        pos += 1
        if char == "\\":
            if pos >= len(expression):
                raise ValueError("character literal escape required")
            escaped = expression[pos]
            pos += 1
            if not width and escaped == "u" and expression[pos:pos + 1] == "{":
                universal = True
                pos += 1
                digits = ""
                while pos < len(expression) and expression[pos] != "}":
                    c = expression[pos]
                    pos += 1
                    if c == "_" and digits:
                        continue
                    if c not in "0123456789abcdefABCDEF" or len(digits) == 6:
                        raise ValueError("finite Rust Unicode escape required")
                    digits += c
                if not digits or pos >= len(expression):
                    raise ValueError("complete Rust Unicode escape required")
                pos += 1
                value = int(digits, 16)
            elif width and escaped in "uU":
                universal = True
                count = 4 if escaped == "u" else 8
                digits = expression[pos:pos + count]
                if len(digits) != count or any(c not in "0123456789abcdefABCDEF" for c in digits):
                    raise ValueError("fixed Unicode character escape requires complete hex digits")
                value = int(digits, 16)
                pos += count
            elif escaped == "x" or escaped in "01234567":
                begin = pos if escaped == "x" else pos - 1
                pos = begin
                digits = "0123456789abcdefABCDEF" if escaped == "x" else "01234567"
                while pos < len(expression) and expression[pos] in digits:
                    if escaped != "x" and pos - begin == 3:
                        break
                    pos += 1
                if pos == begin:
                    raise ValueError("numeric character escape requires digits")
                value = int(expression[begin:pos], 16 if escaped == "x" else 8)
            else:
                simple = {"a": 7, "b": 8, "f": 12, "n": 10, "r": 13, "t": 9, "v": 11,
                          "\\": 92, "'": 39, '"': 34, "?": 63}
                if escaped not in simple:
                    raise ValueError("unsupported character literal escape")
                value = simple[escaped]
        else:
            if char in "'\n\r":
                raise ValueError("unsupported character literal character")
            value = ord(char)
        limit = (1 << width) - 1 if width else 0x10ffff if universal else 127
        if (value > limit or (universal and (value > 0x10ffff or 0xd800 <= value <= 0xdfff
                                               or (width == 8 and value >= 128)))
                or pos >= len(expression) or expression[pos] != "'"):
            raise ValueError("one bounded ASCII character value required")
        return pos + 1

    def primary(depth):
        nonlocal cursor
        bounded(depth)
        space()
        if cursor == len(expression):
            raise ValueError("source scalar expression operand required")
        builtin = re.match(r"(?:len|cap)\s*\(", expression[cursor:])
        if builtin:
            start, nesting = cursor, 1
            cursor += len(builtin[0])
            while cursor < len(expression) and nesting:
                char = expression[cursor]
                cursor += 1
                nesting += (char == "(") - (char == ")")
                if depth + nesting >= 32:
                    raise ValueError("Go builtin operand exceeds its nesting bound")
            if nesting:
                raise ValueError("Go builtin closing parenthesis required")
            validate_source_expression(expression[start:cursor])
            append()
            return None  # int scalar: arithmetic may follow, source postfix cannot.
        if re.match(r"sizeof\b", expression[cursor:]):
            cursor += 6
            if not consume("("):
                unary(depth + 1)
                append()
                return None
            start, nesting = cursor, 1
            while cursor < len(expression) and nesting:
                char = expression[cursor]
                cursor += 1
                if char == "'":
                    try:
                        cursor = character(cursor - 1)
                    except ValueError:
                        pass  # Failed probes consume no token; scalar parse rejects it.
                    else:
                        continue
                nesting += (char == "(") - (char == ")")
                if nesting > 32:
                    raise ValueError("sizeof operand exceeds its nesting bound")
            if nesting:
                raise ValueError("sizeof closing parenthesis required")
            end = cursor
            operand = expression[start:end - 1].strip()
            if typename(operand) not in numeric_types:
                try:
                    normalized = validate_source_expression(operand)
                    if re.fullmatch(r"\(*\s*(?:true|false)\s*\)*", normalized):
                        raise ValueError("sizeof Boolean literal is a scalar operand")
                except ValueError:
                    cursor = start
                    parse(1, depth + 1)
                    require(")")
                    if cursor != end:
                        raise ValueError("sizeof scalar operand delimiter required")
            append()
            return None
        for name, opening, closing, types in (("@as", "(", ",", zig_types), ("static_cast", "<", ">", numeric_types)):
            if named_open(name, opening):
                end = expression.find(closing, cursor)
                if end < 0 or typename(expression[cursor:end]) not in types:
                    raise ValueError("only builtin numeric conversions are available")
                cursor = end + 1
                if name == "static_cast":
                    require("(")
                parse(1, depth + 1)
                require(")")
                append()
                return None
        if consume("("):
            end = expression.find(")", cursor)
            if end >= 0 and typename(expression[cursor:end]) in numeric_types:
                cursor = end + 1
                unary(depth + 1)
                append()
                return None
            reference_steps = parse(1, depth + 1)
            require(")")
            return postfix(reference_steps) if reference_steps is not None else None
        name = re.match(r"[A-Za-z_][A-Za-z_0-9]*", expression[cursor:])
        if name:
            saved = cursor
            cursor += len(name[0])
            if name[0] in numeric_types and consume("("):
                parse(1, depth + 1)
                require(")")
                append()
                return None
            if name[0] in ("true", "false"):
                append()
                return None
            cursor = saved
        if expression[cursor] == "'" or any(expression.startswith(p + "'", cursor) for p in ("u8", "u", "U")):
            cursor = character(cursor)
            append()
            return None
        if expression[cursor].isdigit() or expression[cursor] == ".":
            # Admission is a union of finite language spellings, not CU
            # authority. The VM reparses in the real selected Rust frame.
            rust_digits = r"[0-9][0-9_]*"
            rust_suffix = r"(?:[iu](?:8|16|32|64|size)|f32|f64)"
            rust_real = rf"(?:{rust_digits}\.[0-9][0-9_]*(?:[eE][+-]?_*{rust_digits})?|{rust_digits}[eE][+-]?_*{rust_digits})"
            rust_number = re.match(
                rf"(?P<content>0x[0-9a-fA-F_]*[0-9a-fA-F][0-9a-fA-F_]*|0b[01_]*[01][01_]*|0o[0-7_]*[0-7][0-7_]*|{rust_real}|{rust_digits})(?P<suffix>{rust_suffix})(?![A-Za-z0-9_\'.])",
                expression[cursor:])
            if rust_number and rust_number["content"].startswith(("0x", "0b", "0o")) and rust_number["suffix"] in ("f32", "f64"):
                rust_number = None  # hex f32/f64 may be digits, never a radix float suffix
            if rust_number:
                content = rust_number["content"].replace("_", "")
                suffix = rust_number["suffix"] or ""
                radix = {"0x": 16, "0b": 2, "0o": 8}.get(content[:2], 10)
                floating = suffix in ("f32", "f64") or radix == 10 and ("." in content or "e" in content.lower())
                if floating:
                    if radix != 10 or suffix and suffix not in ("f32", "f64"):
                        raise ValueError("finite Rust floating suffix required")
                    number = float(content)
                    if not math.isfinite(number):
                        raise ValueError("finite Rust floating literal required")
                    significand = re.split("[eE]", content, maxsplit=1)[0]
                    nonzero = any(c in "123456789" for c in significand)
                    if number == 0.0 and nonzero:
                        raise ValueError("source Rust float literal underflows its target")
                    if suffix == "f32":
                        try:
                            packed = struct.pack("!f", number)
                        except OverflowError:
                            raise ValueError("finite Rust float32 literal required") from None
                        narrowed = struct.unpack("!f", packed)[0]
                        if narrowed == 0.0 and nonzero:
                            raise ValueError("source Rust float32 literal underflows its target")
                        if not math.isfinite(narrowed):
                            raise ValueError("finite Rust float32 literal required")
                elif int(content[2:] if radix != 10 else content, radix) >= 1 << 64:
                    raise ValueError("Rust literal exceeds finite 64-bit carrier")
                cursor += len(rust_number[0])
                append()
                return None
            real = re.match(rf"(?:(?:{decimal_digits}\.(?:{decimal_digits})?|\.{decimal_digits})(?:[eE][+-]?{decimal_digits})?|{decimal_digits}[eE][+-]?{decimal_digits})[fF]?", expression[cursor:])
            if real:
                literal = real[0]
                normalized = literal.rstrip("fF").replace("_", "").replace("'", "")
                number = float(normalized)
                if not math.isfinite(number):
                    raise ValueError("finite source float literal required")
                significand = re.split("[eE]", normalized, maxsplit=1)[0]
                nonzero = any(c in "123456789" for c in significand)
                if number == 0.0 and nonzero:
                    raise ValueError("source float literal underflows its target")
                if literal[-1] in "fF":
                    # Nearest-even binary32 overflows at 2**128 - 2**103.
                    # A binary64 parse can round a just-below decimal onto this
                    # exact boundary. Compare its original decimal only there,
                    # keeping finite values that round to FLT_MAX available.
                    overflow = (1 << 128) - (1 << 103)
                    if number > overflow or (number == overflow and Decimal(normalized) >= overflow):
                        raise ValueError("finite source float32 literal required")
                    underflow = math.ldexp(1.0, -150)
                    if number < underflow and nonzero:
                        raise ValueError("source float32 literal underflows its target")
                    if number == underflow and Decimal(normalized) <= Decimal.from_float(underflow):
                        raise ValueError("source float32 literal underflows its target")
                cursor += len(literal)
                append()
                return None
            number = re.match(r"(?:0[xX]_?[0-9a-fA-F](?:[0-9a-fA-F]|['_](?=[0-9a-fA-F]))*|0[bB]_?[01](?:[01]|['_](?=[01]))*|0[oO]_?[0-7](?:[0-7]|['_](?=[0-7]))*|[0-9](?:[0-9]|['_](?=[0-9]))*)(?:[uU](?:ll|LL|[lL])?|(?:ll|LL|[lL])[uU]?)?", expression[cursor:])
            if number is None:
                raise ValueError("source numeric literal required")
            literal = number[0]
            digits = re.sub(r"[uUlL]+$", "", literal).replace("_", "").replace("'", "")
            base = 16 if digits[:2].lower() == "0x" else 2 if digits[:2].lower() == "0b" else 8 if digits[:2].lower() == "0o" or len(digits) > 1 and digits[0] == "0" else 10
            if int(digits[2:] if digits[:2].lower() in ("0x", "0b", "0o") else digits, base) >= 1 << 64:
                raise ValueError("source integer literal exceeds unsigned 64-bit range")
            cursor += len(literal)
            append()
            return None
        start, brackets = cursor, 0
        while cursor < len(expression):
            char = expression[cursor]
            if not brackets and char == " " and re.match(r"(?:as |(?:and|or)\b)", expression[cursor:].lstrip(" ")):
                break
            if not brackets and expression.startswith((".*", "->"), cursor):
                cursor += 2
                continue
            if char == "[":
                brackets += 1
            elif char == "]":
                if not brackets:
                    break
                brackets -= 1
            if not brackets and expression.startswith("::", cursor):
                cursor += 2
                continue
            if not brackets and char in "():+*/%<>?,=!&^|~-":
                break
            cursor += 1
        return selector(expression[start:cursor])

    def unary(depth):
        bounded(depth)
        space()
        if expression.startswith(("++", "--"), cursor):
            raise ValueError("source increment/decrement is unavailable")
        if consume("*"):
            steps = unary(depth + 1)
            if steps is None or steps >= 32:
                raise ValueError("source dereference requires a bounded producer reference")
            append()
            return steps + 1
        for prefix in ("+", "-", "~", "!"):
            if consume(prefix):
                unary(depth + 1)
                append()
                return None
        return primary(depth)

    def parse(minimum, depth):
        nonlocal cursor
        bounded(depth)
        reference_steps = unary(depth + 1)
        while True:
            space()
            if minimum <= 11 and expression.startswith("as ", cursor):
                cursor += 3
                space()
                name = re.match(r"[A-Za-z_][A-Za-z_0-9]*", expression[cursor:])
                if name is None or name[0] not in numeric_types:
                    raise ValueError("only builtin numeric conversions are available")
                cursor += len(name[0])
                append()
                reference_steps = None
                continue
            if expression.startswith(("++", "--"), cursor):
                raise ValueError("source increment/decrement is unavailable")
            # Lexical recognition carries no language authority: the backend
            # checks the selected frame's genuine CU/producer language.
            candidate = next(((token, rank) for token, rank in operators
                              if expression.startswith(token, cursor) and
                              (token not in ("and", "or") or
                               not re.match(r"[A-Za-z_0-9]", expression[cursor + len(token):]))), None)
            if candidate is None or candidate[1] < minimum:
                break
            cursor += len(candidate[0])
            parse(candidate[1] + 1, depth + 1)
            append()
            reference_steps = None
        if minimum == 1 and consume("?"):
            parse(1, depth + 1)
            require(":")
            parse(1, depth + 1)
            append()
            reference_steps = None
        return reference_steps

    parse(1, 0)
    space()
    if cursor != len(expression):
        raise ValueError("source scalar expression has unsupported trailing tokens")
    return expression.strip()


def _validate_source_metadata_label(label):
    """Count the formatter's original bytes without unescaping display text.

    escaped_metadata_text emits <=4096 input bytes: printable ASCII except
    quote/backslash verbatim, those two as two-byte escapes, and all remaining
    bytes as lowercase fixed-width \\xHH. A truncation suffix is extra only
    after exactly 4096 original bytes. No decoded label becomes a command,
    expression, path or address; this only bounds a display field.
    """
    input_limit = 4096
    truncated = "... (truncated)"
    if len(label) > 4 * input_limit + len(truncated):
        raise ValueError("source-variable label exceeds its bound")
    cursor, input_bytes = 0, 0
    while cursor < len(label):
        if input_bytes == input_limit:
            if label[cursor:] == truncated:
                return
            raise ValueError("source-variable label exceeds its bound")
        char = label[cursor]
        if char == "\\":
            if len(label) - cursor < 2:
                raise ValueError("malformed escaped source-variable label")
            escaped = label[cursor + 1]
            if escaped in ('\\', '"'):
                cursor += 2
            elif escaped == "x" and len(label) - cursor >= 4:
                first, second = label[cursor + 2], label[cursor + 3]
                # The public CPO uses hex<false,true>(unsigned char): two
                # lowercase digits, only for C0/DEL/non-ASCII source bytes.
                if (first not in "0123456789abcdef" or second not in "0123456789abcdef"
                        or not (first in "0189abcdef" or (first == "7" and second == "f"))):
                    raise ValueError("malformed escaped source-variable label")
                cursor += 4
            else:
                raise ValueError("malformed escaped source-variable label")
        elif 32 <= ord(char) < 127 and char != '"':
            cursor += 1
        else:
            raise ValueError("unescaped source-variable control byte")
        input_bytes += 1


def validate_source_evaluation_reply(text, expected_stop, expected_thread=None):
    """Check the detached print packet before publishing its copied display.

    Values/objects/origin receipts remain opaque text. This check grants no
    expression, memory or frame authority; genuine pre/post status is required.
    """
    unsigned_id(expected_stop, "source stop identifier")
    if (not isinstance(text, str) or not text.endswith("\n")
            or len(text.encode("utf-8")) > MAX_REPLY
            or any(not 32 <= ord(char) < 127 for char in text if char != "\n")):
        raise ValueError("malformed source-evaluation reply")
    lines = text[:-1].split("\n")
    match = SOURCE_STOP_RE.fullmatch(lines[0]) if lines else None
    object_header = re.fullmatch(r"source-value stop=([0-9]{1,20}) name=(.+)", lines[0]) if lines else None
    identity = match or object_header
    if (identity is None or int(identity.group(1)) != expected_stop or len(lines) < 2
            or any(not line.strip() or line == "source-stop" or line.startswith(("source-stop ", "source-value stop=", "source-type stop=")) for line in lines[1:])):
        raise ValueError("source-evaluation reply belongs to a different or unidentified stop")
    if object_header is not None:
        if (len(lines) < 3 or lines[-1] != "source-value end" or not object_header[2].strip()
                or any(line in ("source-value end", "source-type end") for line in lines[1:-1])):
            raise ValueError("source-object display is incomplete or contains duplicate trailers")
    elif any(line in ("source-value end", "source-type end") for line in lines[1:]):
        raise ValueError("source numeric packet contains an unrelated object trailer")
    if expected_thread is not None:
        unsigned_id(expected_thread, "source participant identifier")
    origins = [line for line in lines[1:] if line.startswith("source-origin ")]
    if len(origins) > 1:
        raise ValueError("duplicate source-origin display receipt")
    for line in origins:
        origin = re.fullmatch(r"source-origin stop=([0-9]{1,20}) thread=([0-9]{1,20}) .+", line)
        if (origin is None or int(origin[1]) != expected_stop or int(origin[2]) >= 1 << 64
                or expected_thread is not None and int(origin[2]) != expected_thread):
            raise ValueError("source-origin display belongs to a different stop or participant")


def source_float_display(match):
    """Check detached decimal and IEEE carrier agree; grant display only.

    Decimal ratios are rounded once with IEEE ties to even. Going through the
    host binary64 before packing binary32 can double-round midpoint neighbors.
    NaN text carries its class only: its sign and payload stay in copied bits.
    Legacy bits-only packets retain their opaque spelling.
    """
    width, hexadecimal, spelling = match.groups()
    width = int(width)
    if len(hexadecimal) > width // 4:
        raise ValueError("source float bits exceed their actual width")
    bits = int(hexadecimal, 16)
    if spelling is None:
        return None
    fraction = 23 if width == 32 else 52
    exponent_bits = 8 if width == 32 else 11
    exponent_mask = (1 << exponent_bits) - 1
    exponent = (bits >> fraction) & exponent_mask
    payload = bits & ((1 << fraction) - 1)
    sign = bits >> (width - 1)
    if spelling in ("nan", "-nan"):
        if exponent != exponent_mask or payload == 0:
            raise ValueError("source NaN spelling disagrees with its actual bits")
        return spelling
    if spelling in ("inf", "-inf"):
        if exponent != exponent_mask or payload != 0 or sign != spelling.startswith("-"):
            raise ValueError("source infinity spelling disagrees with its actual bits")
        return spelling
    if re.fullmatch(r"-?(?:0|[1-9][0-9]{0,23})(?:\.[0-9]{1,24})?"
                    r"(?:[eE][+-]?[0-9]{1,4})?", spelling) is None:
        raise ValueError("malformed bounded source floating display")
    if exponent == exponent_mask:
        raise ValueError("finite source spelling disagrees with its actual bits")
    decimal = Decimal(spelling)
    decimal_sign = int(decimal.is_signed())
    if not decimal:
        rounded = decimal_sign << (width - 1)
    else:
        minimum, maximum, bias = (-126, 127, 127) if width == 32 else (-1022, 1023, 1023)
        # Reject huge exponents before materializing an unbounded power of ten.
        low, high = (-46, 38) if width == 32 else (-324, 308)
        if not low <= decimal.adjusted() <= high:
            raise ValueError("source decimal overflows or underflows its actual width")
        numerator, denominator = decimal.copy_abs().as_integer_ratio()
        power = numerator.bit_length() - denominator.bit_length()
        if ((power >= 0 and numerator < denominator << power)
                or (power < 0 and numerator << -power < denominator)):
            power -= 1
        effective_power = max(power, minimum)
        scale = effective_power - fraction
        if scale >= 0:
            denominator <<= scale
        else:
            numerator <<= -scale
        significand, remainder = divmod(numerator, denominator)
        if 2 * remainder > denominator or (2 * remainder == denominator and significand & 1):
            significand += 1
        if significand == 0:
            raise ValueError("nonzero source decimal underflows its actual width")
        if significand >= 1 << (fraction + 1):
            significand >>= 1
            effective_power += 1
        if effective_power > maximum:
            raise ValueError("finite source decimal overflows its actual width")
        if significand < 1 << fraction:
            rounded = significand
        else:
            rounded = ((effective_power + bias) << fraction) | (significand - (1 << fraction))
        rounded |= decimal_sign << (width - 1)
    if rounded != bits:
        raise ValueError("source decimal disagrees with its actual IEEE bits")
    return spelling


def source_evaluation_display(text):
    """Project bounded copied scalar roots into DAP display fields only.

    Called after the complete packet and real pre/post stop have been checked.
    Compound/unknown packets keep their opaque display. No query, expression,
    address or expansion authority is derived from these formatter labels.
    """
    body = {"result": text.rstrip("\n"), "variablesReference": 0,
            "presentationHint": {"attributes": ["readOnly"]}}
    rows = text.rstrip("\n").split("\n")
    numeric_stop = SOURCE_STOP_RE.fullmatch(rows[0]) if rows else None
    if numeric_stop is not None:
        stop = int(numeric_stop[1])
        validate_source_evaluation_reply(text, stop)
        values = [line for line in rows[1:] if not line.startswith("source-origin ")]
        if len(values) == 1 and re.fullmatch(r"source (?:parameter|local) [A-Za-z_][A-Za-z0-9_]{0,127} type=.+ = (?:[iu](?:8|16|32|64)=|bool=|f(?:32|64) bits=0x).+", values[0]):
            # The current frame and complete pre/post stop were checked by the
            # caller. Decode this detached single scalar, retaining its producer
            # type label; a label never becomes a read or expansion capability.
            packet = rows[0] + "\n" + values[0] + "\n"
            value, = parse_source_values(packet, stop)
            floating = SOURCE_FLOAT_RE.fullmatch(values[0].rsplit(" = ", 1)[1])
            if floating is not None:
                if floating.group(3) is None:
                    return body
                display = value["value"]
            else:
                display = value["value"]
            body.update(result=display, type=value["type"])
        return body
    if len(rows) != 3 or not rows[0].startswith("source-value stop=") or rows[-1] != "source-value end":
        return body
    scalar = re.fullmatch(r"(object|\$(?:len|cap|expression)): type=(bool|float|double), "
                          r"offset=0, bytes=(1|2|4|8), value=(.{1,64})", rows[1])
    if scalar is not None:
        label, kind, size, spelling = scalar.groups()
        if label == "object" and re.fullmatch(
                r"source-value stop=[0-9]{1,20} name=[A-Za-z_][A-Za-z0-9_]{0,127}", rows[0]) is None:
            return body
        if label not in ("$expression", "object") or int(size) != {"bool": 1, "float": 4, "double": 8}[kind]:
            raise ValueError("copied scalar display has an inconsistent type or width")
        if kind == "bool":
            if spelling not in ("true", "false"):
                raise ValueError("copied Boolean display requires its actual truth spelling")
        elif spelling not in ("inf", "-inf", "nan", "-nan"):
            # Preserve the formatter's spelling, including -0 and IEEE special
            # values. NaN text supplies no sign/payload bits or other authority.
            # Shortest FLT_MAX decimals can exceed its exact mathematical value
            # while still rounding to FLT_MAX; validate the actual IEEE width.
            if re.fullmatch(r"-?(?:0|[1-9][0-9]{0,23})(?:\.[0-9]{1,24})?"
                            r"(?:[eE][+-]?[0-9]{1,4})?", spelling) is None:
                raise ValueError("malformed bounded copied floating display")
            number = float(spelling)
            if not math.isfinite(number):
                raise ValueError("finite copied decimal overflows binary64")
            if kind == "float":
                try:
                    number = struct.unpack("!f", struct.pack("!f", number))[0]
                except OverflowError:
                    raise ValueError("finite copied decimal overflows binary32") from None
                if not math.isfinite(number):
                    raise ValueError("finite copied decimal overflows binary32")
            if number == 0.0 and Decimal(spelling) != 0:
                raise ValueError("nonzero copied decimal underflows its actual IEEE width")
        body.update(result=spelling, type=kind)
        return body
    if rows[1].startswith(tuple(f"{label}: type={kind}," for label in ("object", "$len", "$cap", "$expression")
                                for kind in ("bool", "float", "double"))):
        raise ValueError("malformed bounded copied scalar display")
    # Only canonical copied primitives emitted by the controller are admitted.
    # Long carries its explicit guest width; labels never supply ABI/type facts.
    integer_types = {
        "int": ((1, 2, 4, 8), False), "unsigned int": ((4,), True),
        "signed integer": ((1, 2, 4, 8), False), "unsigned integer": ((1, 2, 4, 8), True),
        "Rust char": ((4,), True), "char8_t": ((1,), True), "char16_t": ((2,), True), "char32_t": ((4,), True),
        "signed char": ((1,), False), "unsigned char": ((1,), True),
        "short": ((2,), False), "unsigned short": ((2,), True),
        "long": ((4, 8), False), "unsigned long": ((4, 8), True),
        "long long": ((8,), False), "unsigned long long": ((8,), True),
    }
    root = re.fullmatch(r"(object|\$(?:len|cap|expression)): type=(int|unsigned int|signed integer|unsigned integer|"
                        r"Rust char|char8_t|char16_t|char32_t|signed char|unsigned char|short|unsigned short|long|unsigned long|long long|unsigned long long), "
                        r"offset=0, bytes=(1|2|4|8), value=(0|-?[1-9][0-9]{0,19})", rows[1])
    if root is None:
        if rows[1].startswith(tuple(f"{label}: type={kind}," for label in ("object", "$len", "$cap", "$expression")
                                    for kind in integer_types)):
            raise ValueError("malformed finite integer display")
        return body
    label, kind, size, spelling = root.groups()
    if label == "object" and re.fullmatch(
            r"source-value stop=[0-9]{1,20} name=[A-Za-z_][A-Za-z0-9_]{0,127}", rows[0]) is None:
        return body
    if label in ("$len", "$cap") and kind != "int":
        raise ValueError("inconsistent finite integer display type")
    sizes, unsigned = integer_types[kind]
    if int(size) not in sizes:
        raise ValueError("finite integer display requires its canonical primitive width")
    width, value = int(size) * 8, int(spelling)
    lower, upper = (0, (1 << width) - 1) if unsigned else (-(1 << (width - 1)), (1 << (width - 1)) - 1)
    if kind == "Rust char" and (value > 0x10ffff or 0xd800 <= value <= 0xdfff):
        raise ValueError("Rust char requires an actual Unicode scalar")
    if not lower <= value <= upper or label in ("$len", "$cap") and value < 0:
        raise ValueError("finite integer display exceeds its actual width")
    body.update(result=spelling, type="char" if kind == "Rust char" else kind)
    return body


def _summarize_copied_variants(nodes):
    """Bounded summaries of already selected copied DATA, with original views.

    Each nested selection needs its own validated variant-part/active branch.
    No ABI inference, payload selection, selector, pointer follow or query is
    performed. Incomplete payloads retain only the proven outer case name.
    """
    def selected(owner):
        if (owner['variant_role'] or owner['copied_value'] or owner['variant_reason']
                or len(owner['children']) != 1):
            return None
        group = nodes[owner['children'][0]]
        if group['variant_role'] != 'variant-part' or group['variant_reason']:
            return None
        active = [nodes[i] for i in group['children'] if nodes[i]['variant_role'] == 'active-variant']
        if len(active) != 1 or active[0]['variant_reason']:
            return None
        branch = active[0]
        case = branch['name']
        if len(case.encode('utf-8')) > 256:
            return None
        roots = branch['children']
        charge = 1 + len(group['children'])
        # Keep the canonical case wrapper in the expansion tree.
        if len(roots) == 1:
            wrapper = nodes[roots[0]]
            if (wrapper['name'] == case and wrapper['type'] == case
                    and not wrapper['copied_value'] and not wrapper['variant_role']
                    and not wrapper['variant_reason']):
                roots = wrapper['children']; charge += 1
        return case, roots, charge

    def label(path):
        text = ''
        for part in path:
            text += part if part.startswith('[') or not text else '.' + part
        return text

    def summary(selection, budget, depth):
        case, roots, charge = selection
        budget[0] += charge
        if budget[0] > 64 or depth > 8:
            return case, False
        pending = [(i, ()) for i in reversed(roots)]
        fields, paths = [], set()
        while pending:
            index, prefix = pending.pop(); node = nodes[index]; budget[0] += 1
            path = prefix + (node['name'],)
            if (budget[0] > 64 or depth + len(path) > 8 or node['variant_role'] or node['variant_reason']
                    or re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]{0,127}|\[(?:0|[1-9][0-9]{0,19})\]", node['name']) is None):
                return case, False
            if node['children']:
                if node['copied_value']:
                    return case, False
                nested = selected(node)
                if nested is None:
                    pending.extend((i, path) for i in reversed(node['children']))
                    continue
                value, complete = summary(nested, budget, depth + len(path))
                if not complete:
                    return case, False
            elif node['copied_value']:
                value = node['value']
            elif node['size'] == 0:
                continue
            else:
                return case, False
            if path in paths or budget[1] >= 32:
                return case, False
            paths.add(path); budget[1] += 1
            fields.append(label(path) + ': ' + value)
            candidate = case + ' { ' + ', '.join(fields) + ' }'
            if len(candidate.encode('utf-8')) > 256:
                return case, False
        return (case + ' { ' + ', '.join(fields) + ' }' if fields else case), True

    for owner in nodes:
        selection = selected(owner)
        if selection is not None:
            owner['value'], _ = summary(selection, [0, 0], 0)


def parse_source_object_display(text, expected_stop, expected_thread):
    """Parse a complete copied tree for presentation, never for queries.

    Labels and guest pointer spellings remain opaque. No selector, evaluateName,
    memoryReference or pointer-following permission is reconstructed from them.
    Extended/omitted formatter packets keep their original unexpandable display.
    """
    validate_source_evaluation_reply(text, expected_stop, expected_thread)
    rows = text[:-1].split("\n")
    if not rows[0].startswith("source-value stop="):
        return None
    if any(row.startswith("source-object output-truncated ") for row in rows[1:-1]):
        return None
    if re.match(r"(?:\$len|\$cap|\$expression): type=", rows[1]):
        return None  # Preserve the stricter finite builtin/expression grammar.
    if len(rows) - 2 > 1024:
        raise ValueError("source object exceeds its copied node budget")
    nodes, ancestry = [], []
    pattern = re.compile(r"( *)(.+): type=(.+), offset=(0|[1-9][0-9]{0,19}), bytes=(0|[1-9][0-9]{0,19})(.*)")
    for row in rows[1:-1]:
        # Text follows the structural fields and may itself contain their
        # delimiters. Keep its canonical formatter escapes as display DATA;
        # never unescape it into a selector, path, address or terminal bytes.
        structural, separator, copied_text = row.partition(", text=")
        if separator:
            if (copied_text.endswith(" (truncated)")
                    or re.search(r", omitted-elements=[0-9]+$", copied_text)):
                return None  # This grammar cannot prove a complete text value.
            _validate_source_metadata_label(copied_text)
        match = pattern.fullmatch(structural)
        if match is None:
            return None
        spaces, name, kind, offset, size, suffix = match.groups()
        if (len(spaces) % 2 or len(spaces) > 64 or name != name.strip()
                or structural.count(": type=") != 1 or structural.count(", offset=") != 1 or structural.count(", bytes=") != 1):
            raise ValueError("ambiguous source object row")
        _validate_source_metadata_label(name)
        _validate_source_metadata_label(kind)
        depth, offset, size = len(spaces) // 2, int(offset), int(size)
        if offset >= 1 << 64 or size > 65536 or offset + size >= 1 << 64:
            raise ValueError("source object extent exceeds its bounded guest image")
        if (not nodes and depth != 0 or nodes and depth == 0 or depth > len(ancestry)):
            raise ValueError("source object tree has an invalid parent")
        parent = ancestry[depth - 1] if depth else None
        if parent is not None:
            owner = nodes[parent]
            if offset < owner['offset'] or offset + size > owner['offset'] + owner['size']:
                raise ValueError("source object child exceeds its copied parent")
        variant = re.fullmatch(r", (variant-part|active-variant)(, default)?(?:, unavailable=(.+))?", suffix)
        role, reason = None, None
        if variant is not None:
            role, default, reason = variant.groups()
            # These flags describe the VM's already selected copied tree.
            # They grant no discriminant evaluation, payload selection or
            # pointer/read authority to the adapter.
            if kind != "unavailable" or separator:
                return None
            if parent is None or offset != owner['offset'] or size != owner['size']:
                raise ValueError("variant display must cover its copied owner")
            if role == "variant-part":
                if name != "<variant-part>" or default or owner['variant_role'] == "variant-part":
                    return None
                value = "variant-part"
            else:
                if owner['variant_role'] != "variant-part" or owner['variant_reason']:
                    raise ValueError("active variant lacks an available copied variant part")
                if any(nodes[i]['variant_role'] == "active-variant" for i in owner['children']):
                    raise ValueError("copied variant part has multiple active branches")
                value = "active variant" + (" (default)" if default else "")
            if reason:
                reasons = ({"variant discriminant or selector is unavailable", "variant selectors are ambiguous"}
                           if role == "variant-part" else
                           {"variant discriminant or selector is unavailable", "nested variant layout is unavailable"})
                if reason not in reasons:
                    return None
                value += ", unavailable (" + reason + ")"
        elif suffix.startswith(", value="):
            value = suffix.removeprefix(", value=")
            if not value:
                raise ValueError("invalid source object scalar display")
            # An enum annotation is emitted only after an integer value by the
            # VM formatter. Validate the complete numeric width and original
            # escaped label bytes; retain both as read-only display DATA.
            # Enumerator names are never decoded or used as query selectors.
            enumeration = re.fullmatch(r"(0|-?[1-9][0-9]{0,19}) \((.+)\)", value)
            if enumeration is not None:
                number, enumerator = enumeration.groups()
                if (size not in (1, 2, 4, 8)
                        or not -(1 << (size * 8 - 1)) <= int(number) < 1 << (size * 8)):
                    raise ValueError("enum display is outside its copied integer width")
                _validate_source_metadata_label(enumerator)
                if enumerator.endswith("... (truncated)"):
                    return None  # A partial name cannot qualify a complete tree.
            elif len(value) > 4096:
                raise ValueError("invalid source object scalar display")
            # Other scalar kinds have no ABI tag in this text protocol.
            elif value.startswith("guest:"):
                pointer = re.fullmatch(r"guest:0x([0-9a-fA-F]{1,16})", value)
                if pointer is None or size not in (4, 8) or int(pointer[1], 16) >= 1 << (size * 8):
                    raise ValueError("invalid copied guest pointer display")
            elif re.fullmatch(r"-?[0-9]+", value):
                if (size not in (1, 2, 4, 8) or not re.fullmatch(r"0|-?[1-9][0-9]{0,19}", value)
                        or not -(1 << (size * 8 - 1)) <= int(value) < 1 << (size * 8)):
                    # The text packet has no scalar encoding. A typedef can
                    # name a float whose formatter prints -0 or a large decimal.
                    # Keep its complete original display; infer no integer ABI.
                    return None
            elif value not in ("true", "false"):
                # Unknown scalar decorations and variant metadata retain
                # their complete raw display.
                if not re.fullmatch(r"[-+]?(?:inf|nan|(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][-+]?[0-9]+)?)", value):
                    return None
        elif suffix.startswith(", unavailable="):
            reason = suffix.removeprefix(", unavailable=")
            if not reason or len(reason) > 4096:
                raise ValueError("invalid source object unavailability display")
            value = "unavailable (" + reason + ")"
        elif not suffix:
            value = kind
        else:
            return None
        if separator:
            if suffix and not suffix.startswith(", value=guest:"):
                return None  # No text decoration of unknown scalar/reason rows.
            quoted = '"' + copied_text + '"'
            value = value + " " + quoted if suffix else quoted
        index = len(nodes)
        nodes.append(dict(name=name, type=kind, value=value, offset=offset, size=size,
                          children=[], variant_role=role, variant_reason=reason,
                          copied_value=suffix.startswith(", value=") or bool(separator),
                          indexed=re.fullmatch(r"\[(?:0|[1-9][0-9]{0,19})\]", name) is not None))
        if parent is not None:
            nodes[parent]['children'].append(index)
        ancestry[depth:] = [index]
    _summarize_copied_variants(nodes)
    return nodes


def parse_source_values(text, expected_stop):
    """Display finite copied values; labels grant no expression/address access.

    Never unescape metadata into paths or commands. The console grammar has no
    quoted fields, so ambiguous delimiter-bearing labels fail without a prefix.
    """
    unsigned_id(expected_stop, "source stop identifier")
    if not isinstance(text, str) or not text.endswith("\n") or "\r" in text or len(text.encode("utf-8")) > MAX_REPLY:
        raise ValueError("malformed source-value reply")
    lines = text[:-1].split("\n")
    if any(any(not 32 <= ord(char) < 127 for char in line) for line in lines):
        raise ValueError("unescaped source-value control byte")
    match = SOURCE_STOP_RE.fullmatch(lines[0]) if lines else None
    if match is None or int(match.group(1)) != expected_stop:
        raise ValueError("source-value reply belongs to a different or unidentified stop")
    rows = lines[1:]
    truncated = None
    if rows and rows[-1].startswith("source-variables "):
        trailer = re.fullmatch(r"source-variables shown=([0-9]{1,4}) total=([0-9]{1,4}) truncated=true", rows[-1])
        if trailer is None:
            raise ValueError("invalid source-variable truncation trailer")
        shown, total = map(int, trailer.groups())
        rows = rows[:-1]
        if shown != len(rows) or not 0 <= shown < total <= 1024:
            raise ValueError("source-variable truncation count mismatch")
        truncated = total - shown
    if truncated is None and rows == ["no active source variables"]:
        return []
    if truncated is None and len(rows) == 1 and rows[0].startswith("source locals unavailable: "):
        if not 0 < len(rows[0].removeprefix("source locals unavailable: ")) <= 4096:
            raise ValueError("invalid source unavailability reason")
        return [{"name": "<source locals>", "value": rows[0].removeprefix("source locals unavailable: "),
                 "type": "unavailable", "variablesReference": 0,
                 "presentationHint": {"attributes": ["readOnly"]}}]
    if (not rows and truncated is None) or len(rows) > 1024:
        raise ValueError("invalid source-variable count")
    variables = []
    for line in rows:
        if any(not 32 <= ord(char) < 127 for char in line):
            raise ValueError("unescaped source-variable control byte")
        if line.startswith("source parameter "):
            label = line.removeprefix("source parameter ")
        elif line.startswith("source local "):
            label = line.removeprefix("source local ")
        else:
            raise ValueError("malformed source-variable row")
        if label.count(" type=") != 1 or label.count(" = ") != 1:
            raise ValueError("ambiguous source-variable label")
        name, typed = label.split(" type=", 1)
        type_name, value = typed.split(" = ", 1)
        # Bound original formatter bytes, not just the expanded ASCII width.
        # Leave escapes intact: source labels grant no read/evaluate authority.
        _validate_source_metadata_label(name)
        _validate_source_metadata_label(type_name)
        if value in ("bool=true", "bool=false"):
            value = value.removeprefix("bool=")
        elif match := SOURCE_INTEGER_RE.fullmatch(value):
            width, number = int(match.group(2)), int(match.group(3))
            lower, upper = (-(1 << (width - 1)), (1 << (width - 1)) - 1) if match.group(1) == "i" else (0, (1 << width) - 1)
            if not lower <= number <= upper or (match.group(1) == "u" and match.group(3).startswith("-")):
                raise ValueError("source integer is outside its actual width")
            value = match.group(3)
        elif match := SOURCE_FLOAT_RE.fullmatch(value):
            spelling = source_float_display(match)
            if spelling is not None:
                value = spelling
        elif value.startswith("unavailable (") and value.endswith(")") and 14 < len(value) <= 4096:
            pass
        else:
            raise ValueError("unsupported or malformed source-variable value")
        variables.append({"name": name or "<anonymous>", "type": type_name, "value": value,
                          "variablesReference": 0, "presentationHint": {"attributes": ["readOnly"]}})
    if truncated is not None:
        variables.append({"name": "<source locals truncated>", "value": f"{truncated} additional variables; use print NAME to inspect",
                          "type": "unavailable", "variablesReference": 0,
                          "presentationHint": {"attributes": ["readOnly"]}})
    return variables


# These are bounded detached Wasm DATA. Query-local graph numbers are never
# converted to VM addresses, object tokens, expressions or restore permission.
_WASM_STATE_HEADER = re.compile(r"Wasm state thread=([0-9]{1,20}) module=([0-9]{1,20}) epoch=([0-9]{1,20}) first=([0-9]{1,20}) total=([0-9]{1,20})")
_WASM_TYPE = r"(?:unknown|i32|i64|f32|f64|v128|\(ref (?:null )?(?:func|extern|any|eq|i31|struct|array|exn|noexn|nofunc|noextern|none|type-index=[0-9]{1,10} module=[0-9]{1,20})\))"
_WASM_VALUE = re.compile(r"(" + _WASM_TYPE + r") = (.+)")
_WASM_REFERENCE = re.compile(r"(struct|array|exception|opaque host-reference|extern-wrapper) #([0-9]{1,3})")
_WASM_UNAVAILABLE = {
    "no current value", "local is not initialized", "typed site is incomplete",
    "reference identity could not be resolved", "actual reference root borrow is unavailable",
    "opaque host payload is not inspectable",
}
_WASM_STATE_UNAVAILABLE = {
    "requires LLVM-JIT full", "typed value observation was not reserved before compilation",
    "requires a current cooperative Wasm stop", "stop or code generation is stale",
    "participant is not in the actual stopped cohort", "the complete current participant cohort was not captured",
    "current activation ownership is unavailable", "this control or exception site has no complete typed packet",
    "actual GC root and store borrowing is unavailable", "foreign state cannot be borrowed safely",
    "invalid Wasm state selector", "Wasm index or page is out of range", "bounded Wasm state query exceeded its quota",
    "Wasm state copy allocation failed", "invalid copied Wasm state data",
}


def _wasm_value(text):
    match = _WASM_VALUE.fullmatch(text)
    if match is None:
        raise ValueError("invalid typed Wasm value")
    kind, value = match.groups()
    referenced = None
    if kind.startswith("(ref "):
        if match := re.search(r"type-index=([0-9]+) module=([0-9]+)", kind):
            if int(match[1]) >= 1 << 32 or int(match[2]) >= 1 << 64:
                raise ValueError("Wasm reference declaration exceeds its width")
    if value.startswith("unavailable (") and value.endswith(")"):
        if value[13:-1] not in _WASM_UNAVAILABLE:
            raise ValueError("unknown typed Wasm unavailability reason")
    elif kind == "unknown":
        raise ValueError("unknown declaration cannot carry an available value")
    elif kind in ("i32", "i64"):
        if kind == "i32" and (match := re.fullmatch(r"packed-u(8|16)=([0-9]{1,5})", value)):
            if not 0 <= int(match[2]) < 1 << int(match[1]):
                raise ValueError("invalid packed GC field")
        elif (match := re.fullmatch(r"-?(?:0|[1-9][0-9]{0,19})", value)):
            width = 32 if kind == "i32" else 64
            if not -(1 << (width - 1)) <= int(value) < (1 << (width - 1)):
                raise ValueError("invalid Wasm integer")
        else:
            raise ValueError("invalid Wasm integer value")
    elif kind in ("f32", "f64"):
        width = 8 if kind == "f32" else 16
        if re.fullmatch(r"bits=0x[0-9a-f]{1," + str(width) + r"}", value) is None:
            raise ValueError("invalid preserved Wasm floating-point bits")
    elif kind == "v128":
        if re.fullmatch(r"bytes=[0-9a-f]{32}", value) is None:
            raise ValueError("invalid Wasm SIMD carrier")
    elif value == "null":
        if not kind.startswith("(ref null "):
            raise ValueError("non-nullable Wasm reference is null")
    elif match := _WASM_REFERENCE.fullmatch(value):
        referenced = (int(match[2]), match[1])
        if not 1 <= referenced[0] <= 128:
            raise ValueError("Wasm graph reference outside bounded dense objects")
    elif match := re.fullmatch(r"i31 signed=(-?[0-9]{1,10}) unsigned=([0-9]{1,10})", value):
        signed, unsigned = map(int, match.groups())
        if not 0 <= unsigned < (1 << 31) or signed != (unsigned if unsigned < 1 << 30 else unsigned - (1 << 31)):
            raise ValueError("Wasm i31 signed and unsigned representations differ")
    elif match := re.fullmatch(r"function module=([0-9]{1,20}) index=([0-9]{1,20})", value):
        if any(int(number) >= 1 << 64 for number in match.groups()):
            raise ValueError("invalid Wasm function identity labels")
    elif value != "function identity unavailable":
        raise ValueError("unknown typed Wasm reference value")
    return {"type": kind, "value": value, "object": referenced}


_WASM_LAYOUT_SELECTIONS = ("controls", "handlers", "control-params", "control-results", "handler-params")
_WASM_VALUE_SELECTIONS = ("locals", "operands", "saved", "globals", "table")
_WASM_OPERAND_SNAPSHOT_NOTE = "Note: Last Wasm safepoint snapshot; may differ from current native state."
_WASM_PRETRAP_INPUT_NOTE = "Note: Pre-trap Wasm inputs; no instruction result."
_WASM_UNCAUGHT_INPUT_NOTE = "Note: Uncaught Wasm snapshot; stack has not unwound."


def parse_wasm_layout(text, selected, thread, module, first, stop_id, index=0, count=64):
    """Finite copied lexical DATA, never native caught state or restore authority."""
    unsigned_id(thread, "Wasm layout thread"); unsigned_id(stop_id, "Wasm layout stop")
    if (selected not in _WASM_LAYOUT_SELECTIONS or type(module) is not int or not 0 <= module < 1 << 64
            or type(first) is not int or not 0 <= first < 1 << 64 or type(index) is not int or not 0 <= index < 1 << 64
            or (selected in ("controls", "handlers") and index != 0)
            or type(count) is not int or not 1 <= count <= 64 or not isinstance(text, str)
            or not text.endswith("\n") or len(text) > 33024 or any(not 32 <= ord(c) < 127 for c in text if c != "\n")):
        raise ValueError("invalid bounded lexical Wasm page")
    lines = text[:-1].split("\n")
    if not lines or lines[0] != f"wasm-stop {stop_id}":
        raise ValueError("Wasm layout belongs to a different stop")
    if len(lines) == 2 and lines[1].startswith("Wasm state unavailable: "):
        reason = lines[1].removeprefix("Wasm state unavailable: ")
        if reason not in _WASM_STATE_UNAVAILABLE: raise ValueError("unknown Wasm layout unavailability")
        return {"unavailable": reason, "rows": [], "objects": [], "total": 0, "truncated": False}
    head = _WASM_STATE_HEADER.fullmatch(lines[1]) if len(lines) > 1 else None
    if head is None: raise ValueError("complete Wasm layout header required")
    actual_thread, actual_module, epoch, actual_first, total = map(int, head.groups())
    if (actual_thread != thread or actual_first != first or not 0 < epoch < 1 << 64
            or any(number >= 1 << 64 for number in (actual_thread, actual_module, total)) or first > total):
        raise ValueError("Wasm layout differs from requested actual stop")
    shown = min(count, total-first); more = shown != total-first
    if len(lines) != 2+shown+int(more): raise ValueError("Wasm lexical page is incomplete")
    if more and lines[-1] != f"Wasm layout page next={first+shown}":
        raise ValueError("Wasm layout continuation differs from original indices")
    rows = []
    for ordinal, line in enumerate(lines[2:2+shown], first):
        if selected == "controls":
            match = re.fullmatch(r"control ([0-9]{1,20}) kind=(function|block|loop|if-then|if-else) entry=([0-9]{1,20}) end=([0-9]{1,20}) height=([0-9]{1,20}) saved-first=([0-9]{1,20}) saved-count=([0-9]{1,20}) params=([0-9]{1,20}) results=([0-9]{1,20})", line)
            if match is None: raise ValueError("malformed Wasm control declaration")
            number, kind, *numbers = match.groups(); numbers = list(map(int, numbers))
            if (int(number) != ordinal or any(n >= 1 << 64 for n in numbers) or numbers[0] > numbers[1]
                    or numbers[4] > numbers[5] or numbers[4] >= (1 << 64)-numbers[3] or (kind not in ("if-then", "if-else") and numbers[4] != 0)
                    or (kind == "function" and ordinal != 0)):
                raise ValueError("invalid bounded Wasm control declaration")
            name, type_name = f"control {number}", "lexical Wasm control"
        elif selected == "handlers":
            match = re.fullmatch(r"handler ([0-9]{1,20}) catch=(catch|catch-ref|catch-all|catch-all-ref) tag-index=([0-9]{1,20}) target-control=([0-9]{1,20}) target-offset=([0-9]{1,20}) params=([0-9]{1,20})", line)
            if match is None: raise ValueError("malformed Wasm handler declaration")
            number, kind, *numbers = match.groups(); numbers = list(map(int, numbers))
            if int(number) != ordinal or any(n >= 1 << 64 for n in numbers) or (kind.startswith("catch-all") and (numbers[0] != 0 or numbers[3] != 0)):
                raise ValueError("invalid bounded Wasm handler declaration")
            name, type_name = f"handler {number}", "lexical Wasm handler clause"
        else:
            prefix = f"{'handler' if selected == 'handler-params' else 'control'} {index} {'result' if selected == 'control-results' else 'parameter'} "
            match = re.fullmatch(re.escape(prefix)+r"([0-9]{1,20}) ("+_WASM_TYPE+r")", line)
            if match is None or int(match[1]) != ordinal or match[2] == "unknown":
                raise ValueError("invalid original-index Wasm declaration type")
            if typed := re.search(r"type-index=([0-9]+) module=([0-9]+)",match[2]):
                if int(typed[1]) >= 1 << 32 or int(typed[2]) >= 1 << 64:
                    raise ValueError("Wasm declaration exceeds its exact width")
            name, type_name = prefix+match[1], match[2]
        rows.append({"name": name, "index": ordinal, "type": type_name,
                     "value": line[len(name)+1:] if selected in ("controls", "handlers") else "declared type",
                     "object": None})
    return {"rows": rows, "objects": [], "total": total, "module": actual_module, "epoch": epoch, "truncated": more}


def parse_wasm_path(text, stop_id, expected_depth, expected_session=None, expected_handle=None, members=False):
    """Bounded path DATA header, never a runtime/GC/native permission."""
    unsigned_id(stop_id, "Wasm path stop")
    if (not isinstance(text, str) or not text.endswith("\n") or len(text.encode("utf-8")) > 33280
            or any(not 32 <= ord(c) < 127 for c in text if c != "\n")
            or type(expected_depth) is not int or not 0 <= expected_depth <= 4096):
        raise ValueError("invalid bounded Wasm path reply")
    if text.startswith("Wasm path unavailable: "):
        reason = text[:-1].removeprefix("Wasm path unavailable: ")
        if reason not in _WASM_STATE_UNAVAILABLE:
            raise ValueError("unknown Wasm path unavailability")
        return {"unavailable": reason}, None
    lines = text[:-1].split("\n")
    match = re.fullmatch(r"Wasm path session=([0-9]{1,20}) handle=([0-9]{1,20}) depth=([0-9]{1,4}) view=3 protocol=1", lines[1]) if len(lines) > 1 else None
    if lines[0] != f"wasm-stop {stop_id}" or match is None:
        raise ValueError("complete current Wasm path/view/protocol header required")
    session, handle, depth = map(int, match.groups())
    if (not 0 < session < 1 << 64 or not 0 < handle < 1 << 64 or depth != expected_depth
            or (expected_session is not None and session != expected_session)
            or (expected_handle is not None and handle != expected_handle)
            or (not members and len(lines) != 2) or (members and len(lines) < 3)):
        raise ValueError("Wasm path belongs to a different session, root path or stop")
    packet = "\n".join([lines[0], *lines[2:]]) + "\n" if members else None
    return {"session": session, "handle": handle, "depth": depth}, packet


def parse_wasm_state(text, selected, thread, module, first, stop_id, table=0,
                     member_first=0, member_count=0, path=(), path_token=None):
    """Decode finite copied DATA; original root/path is reborrowed each query.

    Dense object numbers are reply-local labels only. Runtime authority remains
    the actual current cohort, canonical captures, host close, N GC leases and
    store publication. Adapter status before/after retains the complete stop.
    """
    unsigned_id(thread, "Wasm state thread")
    unsigned_id(stop_id, "Wasm state stop")
    member_mode = member_count != 0
    compressed = path_token is not None
    if compressed and (not member_mode or not isinstance(path, (tuple, list)) or not isinstance(path_token, tuple) or len(path_token) != 3
            or any(type(n) is not int for n in path_token) or not 0 < path_token[0] < 1 << 64
            or not 0 < path_token[1] < 1 << 64 or path_token[2] != len(path)):
        raise ValueError("invalid original-path compression labels")
    if (selected not in _WASM_VALUE_SELECTIONS or type(module) is not int or not 0 <= module < 1 << 64
            or type(first) is not int or not 0 <= first < 1 << 64 or type(table) is not int or not 0 <= table < 1 << 64
            or type(member_first) is not int or not 0 <= member_first < 1 << 64
            or type(member_count) is not int or not 0 <= member_count <= 64
            or not isinstance(path, (tuple, list)) or len(path) > (4096 if compressed else 16)
            or any(type(index) is not int or not 0 <= index < 1 << 64 for index in path)
            or (not member_mode and (member_first != 0 or path))
            or not isinstance(text, str) or not text.endswith("\n") or len(text.encode("utf-8")) > 33024
            or any(not 32 <= ord(c) < 127 for c in text if c != "\n")):
        raise ValueError("invalid bounded Wasm state packet")
    lines = text[:-1].split("\n")
    if not lines or lines[0] != f"wasm-stop {stop_id}":
        raise ValueError("Wasm state packet belongs to a different stop")
    begin = 1
    input_note = lines[begin] if len(lines) > begin and lines[begin] in (_WASM_PRETRAP_INPUT_NOTE, _WASM_UNCAUGHT_INPUT_NOTE) else None
    pretrap = input_note is not None
    if pretrap:
        if selected != "operands":
            raise ValueError("terminal Wasm snapshot note requires an operand page")
        begin += 1
    if len(lines) == begin + 1 and lines[begin].startswith("Wasm state unavailable: "):
        reason = lines[begin].removeprefix("Wasm state unavailable: ")
        if reason not in _WASM_STATE_UNAVAILABLE:
            raise ValueError("unknown Wasm state unavailability reason")
        result = {"unavailable": reason, "rows": [], "objects": [], "total": 0, "truncated": False}
        if pretrap:
            result["snapshotNote"] = input_note
        return result
    head = _WASM_STATE_HEADER.fullmatch(lines[begin]) if len(lines) > begin else None
    if head is None:
        raise ValueError("complete typed Wasm state header required")
    actual_thread, actual_module, epoch, actual_first, total = map(int, head.groups())
    if (actual_thread != thread or (selected in ("globals", "table") and actual_module != module)
            or actual_first != first or not 0 < epoch < 1 << 64 or any(n >= 1 << 64 for n in (actual_thread, actual_module, total))
            or first > total):
        raise ValueError("Wasm state page differs from requested actual stop")
    result = {"rows": [], "objects": [], "total": total, "module": actual_module, "epoch": epoch, "truncated": False}
    if pretrap:
        result["snapshotNote"] = input_note
    begin += 1
    if len(lines) > begin and lines[begin] == _WASM_OPERAND_SNAPSHOT_NOTE:
        if selected != "operands":
            raise ValueError("operand snapshot note requires an operand page")
        result["snapshotNote"] = (input_note + "\n" if pretrap else "") + _WASM_OPERAND_SNAPSHOT_NOTE
        begin += 1
    if member_mode:
        meta = re.fullmatch(r"Wasm members object=([0-9]{1,3}) first=([0-9]{1,20}) count=([0-9]{1,2}) path=(.+)", lines[begin]) if len(lines) > begin else None
        spelling = (f"@{path_token[0]}:{path_token[1]} depth={path_token[2]}" if compressed else
                    ",".join(map(str, path)) if path else "-")
        if (meta is None or not 1 <= int(meta[1]) <= 128 or int(meta[2]) != member_first
                or int(meta[3]) != member_count or meta[4] != spelling):
            raise ValueError("Wasm member page differs from its original root and path")
        result["selected_object"] = int(meta[1]); begin += 1
    current = None
    members = 0
    trailer = None
    object_map = {}
    for line in lines[begin:]:
        if match := re.fullmatch(r"Wasm state truncated: rows=([0-9]{1,2}) objects=([0-9]{1,3})", line):
            if trailer is not None or tuple(map(int, match.groups())) != (len(result["rows"]), len(result["objects"])):
                raise ValueError("Wasm state truncation counts differ")
            trailer = line; result["truncated"] = True
            continue
        if trailer is not None:
            raise ValueError("Wasm state data follows truncation trailer")
        if line.startswith("object #"):
            match = re.fullmatch(r"object #([0-9]{1,3}) (.+) members=([0-9]{1,20})(?: first=([0-9]{1,20}) next=([0-9]{1,20}) more=(yes|no))?", line)
            if (match is None or not 1 <= int(match[1]) <= 128 or int(match[1]) in object_map
                    or len(object_map) == 128 or int(match[3]) >= 1 << 64 or (member_mode and match[4] is None)):
                raise ValueError("invalid bounded dense Wasm object")
            if member_mode:
                if not result["objects"] and int(match[1]) != result["selected_object"]:
                    raise ValueError("selected member page must precede metadata")
            elif int(match[1]) != len(result["objects"]) + 1:
                raise ValueError("ordinary Wasm graph is not dense and ordered")
            label = match[2]; kind = label.split(" ", 1)[0]
            if kind in ("struct", "array"):
                meta = re.fullmatch(r"(struct|array) module=([0-9]{1,20}) type=([0-9]{1,10})", label)
                if meta is None or int(meta[2]) >= 1 << 64 or int(meta[3]) >= 1 << 32:
                    raise ValueError("invalid GC object type identity")
            elif kind == "exception":
                if label != "exception tag identity unavailable":
                    meta = re.fullmatch(r"exception tag-module=([0-9]{1,20}) tag=([0-9]{1,20})", label)
                    if meta is None or any(int(n) >= 1 << 64 for n in meta.groups()):
                        raise ValueError("invalid exception tag identity")
            elif label == "opaque host-reference (payload unavailable)":
                kind = "opaque host-reference"
                if int(match[3]) != 0:
                    raise ValueError("opaque host-reference payload cannot be inspected")
            elif label == "extern-wrapper":
                if int(match[3]) != 1:
                    raise ValueError("external wrapper cardinality invalid")
            else:
                raise ValueError("unknown copied Wasm object kind")
            first_member = int(match[4]) if match[4] is not None else 0
            next_member = int(match[5]) if match[5] is not None else None
            if first_member > int(match[3]) or (next_member is not None and not first_member <= next_member <= int(match[3])):
                raise ValueError("invalid original-index Wasm member extent")
            current = {"id": int(match[1]), "kind": kind, "label": label, "total": int(match[3]), "members": [],
                       "first": first_member, "next": next_member, "more": match[6] == "yes", "paged": match[4] is not None,
                       "marker": None, "truncated": False}
            object_map[current["id"]] = current; result["objects"].append(current)
        elif line in ("  members truncated", "  metadata only; expand through the original root and member path",
                      "  member page; continue with original root/path and next", "  final member page; earlier members are outside this window"):
            if current is None or current["marker"] is not None:
                raise ValueError("invalid duplicate GC member marker")
            current["marker"] = line
        elif line.startswith("  "):
            match = re.fullmatch(r"  ([0-9]{1,20}) (.+?)(?: (mutable|immutable))?", line)
            if (current is None or match is None or current["marker"] is not None
                    or int(match[1]) != current["first"] + len(current["members"]) or members == 256):
                raise ValueError("invalid copied original-index GC member")
            value = _wasm_value(match[2]); value["name"] = f"[{match[1]}]"; value["index"] = int(match[1])
            if match[3] is not None: value["mutable"] = match[3] == "mutable"
            current["members"].append(value); members += 1
        else:
            prefix = f"table {table} element " if selected == "table" else {"locals": "local ", "operands": "operand ", "saved": "saved-parameter ", "globals": "global "}[selected]
            match = re.fullmatch(re.escape(prefix) + r"([0-9]{1,20}) (.+)", line)
            if current is not None or match is None or len(result["rows"]) == 64 or int(match[1]) != first + len(result["rows"]):
                raise ValueError("invalid original-index Wasm value row")
            value = _wasm_value(match[2]); value["name"] = prefix + match[1]; value["index"] = int(match[1])
            result["rows"].append(value)
    if len(result["rows"]) > total - first or (member_mode and len(result["rows"]) != 1):
        raise ValueError("Wasm page row count exceeds actual cardinality")
    if sorted(object_map) != list(range(1, len(object_map) + 1)):
        raise ValueError("Wasm object labels are not bounded and dense")
    result["objects"] = [object_map[index] for index in range(1, len(object_map) + 1)]
    if member_mode and result["selected_object"] not in object_map:
        raise ValueError("selected rooted member object is absent")
    if compressed and (len(result["objects"]) > 2 + member_count or result["selected_object"] > 2
            or result["rows"][0]["object"] is None or result["rows"][0]["object"][0] != 1
            or result["rows"][0]["object"][1] == "opaque host-reference"):
        raise ValueError("compressed original path must retain only root/current shallow metadata")
    for obj in result["objects"]:
        n = len(obj["members"])
        if n > obj["total"] - obj["first"]:
            raise ValueError("GC member count exceeds actual cardinality")
        if obj["paged"]:
            if obj["next"] != obj["first"] + n or obj["more"] != (obj["next"] != obj["total"]):
                raise ValueError("GC member window skips or rewinds a member")
            selected_object = member_mode and obj["id"] == result["selected_object"]
            if selected_object:
                if (obj["kind"] == "opaque host-reference" or obj["first"] != member_first
                        or n != min(member_count, obj["total"] - member_first)):
                    raise ValueError("selected GC page differs from the requested exact window")
            elif member_mode and (n != 0 or obj["first"] != 0):
                raise ValueError("shallow rooted page unexpectedly contains child members")
            marker = ("  metadata only; expand through the original root and member path" if member_mode and not selected_object and obj["total"] != 0 else
                      "  member page; continue with original root/path and next" if obj["more"] else
                      "  final member page; earlier members are outside this window" if obj["first"] != 0 else None)
            if obj["marker"] != marker:
                raise ValueError("GC member pagination marker differs from its exact extent")
            obj["truncated"] = obj["first"] != 0 or n != obj["total"]
        else:
            # Legacy complete formatter packets remain readable during upgrade.
            obj["next"] = n; obj["more"] = n != obj["total"]
            obj["truncated"] = obj["marker"] == "  members truncated"
            if obj["truncated"] != obj["more"]:
                raise ValueError("legacy GC member count differs from actual cardinality")
    for value in result["rows"] + [v for obj in result["objects"] for v in obj["members"]]:
        if value["object"] is not None:
            number, kind = value["object"]
            if number not in object_map or object_map[number]["kind"] != kind:
                raise ValueError("Wasm graph points to an absent or differently typed object")
    for obj in result["objects"]:
        if obj["kind"] != "extern-wrapper": continue
        seen = set(); current = obj
        while current["kind"] == "extern-wrapper":
            if current["id"] in seen: raise ValueError("external wrapper-only reference cycle")
            seen.add(current["id"])
            if not current["members"]: break
            destination = current["members"][0]["object"]
            if destination is None: break
            current = object_map[destination[0]]
    return result



_WASIP1_STATUS = frozenset(("ok", "invalid request", "requires LLVM JIT full", "requires current cooperative stop",
    "state observation not selected", "stale stop or generation", "incomplete or unauthenticated cohort",
    "host activity is busy or untracked", "WASIp1 environment unavailable", "text storage is unowned, ambiguous or over limit",
    "guest descriptor unavailable", "descriptor rights changed", "cannot increase descriptor capabilities", "entry not found",
    "debug resource limit", "allocation failed", "resource rollback unavailable", "native resource operation failed"))
_WASIP1_TEXT = re.compile(r'(?:[!#-\[\]-~]|\\x[0-9a-f]{2})*')


def parse_wasip1_state(text, selected, module, first, count, stop_id):
    """Detached bounded WASIp1 display; no native handle/path or replay authority."""
    if (not isinstance(text, str) or not text.isascii() or not text.endswith("\n") or "\r" in text
            or len(text) > 33024 or selected not in ("args", "env", "fds", "preopens")):
        raise ValueError("malformed bounded WASIp1 state reply")
    lines = text.splitlines()
    if len(lines) < 2 or lines[0] != f"wasip1-stop {stop_id}":
        raise ValueError("WASIp1 state belongs to a different stop")
    header = re.fullmatch(r"wasip1 module=([0-9]{1,20}) status=(.+) epoch=([0-9]{1,20}) applied=([01]) shared-environment=([01]) total=([0-9]{1,20})", lines[1])
    if (header is None or int(header[1]) != module or header[2] not in _WASIP1_STATUS
            or int(header[3]) >= 1 << 64 or int(header[6]) >= 1 << 64 or header[4] != "0"):
        raise ValueError("invalid read-only WASIp1 header")
    result = {"moduleId": module, "runtimeEpoch": int(header[3]), "status": header[2],
        "available": header[2] == "ok", "sharedEnvironment": header[5] == "1", "total": int(header[6]), "variables": [], "more": False, "next": first}
    if header[2] != "ok":
        if len(lines) != 2 or result["total"] != 0:
            raise ValueError("unavailable WASIp1 query contains state")
        return result
    if result["runtimeEpoch"] == 0 or result["total"] > (4096 if selected in ("args", "env") else 65536):
        raise ValueError("unbounded WASIp1 runtime cardinality")
    raw_bytes = 0
    def check_text(value):
        nonlocal raw_bytes
        if _WASIP1_TEXT.fullmatch(value) is None:
            raise ValueError("WASIp1 text is not canonically escaped")
        # Four printed bytes represent one raw byte; the printer never emits
        # bare quotes, spaces, backslashes, controls or non-ASCII bytes.
        n = len(value) - 3 * value.count("\\x")
        if n > 4096 or raw_bytes + n > 4096:
            raise ValueError("WASIp1 copied-text budget exceeded")
        raw_bytes += n
    for line in lines[2:]:
        if result["more"]:
            raise ValueError("WASIp1 state follows its pagination trailer")
        if trailer := re.fullmatch(r"  more next=([0-9]{1,20})", line):
            value = int(trailer[1])
            expected = first if not result["variables"] else result["next"]
            if value != expected or value >= 1 << 64:
                raise ValueError("WASIp1 pagination skips or rewinds a row")
            result["more"] = True
            continue
        if len(result["variables"]) == count or len(result["variables"]) == 64:
            raise ValueError("WASIp1 row count exceeds requested page")
        if selected in ("args", "env"):
            row = re.fullmatch(r'  \[([0-9]{1,20})\] "(.*)"', line)
            expected = first + len(result["variables"])
            if row is None or int(row[1]) != expected or expected >= result["total"]:
                raise ValueError("WASIp1 text row is not the original index")
            check_text(row[2]); result["next"] = expected + 1
            result["variables"].append({"name": f"[{expected}]", "value": '"' + row[2] + '"',
                "type": "argument bytes" if selected == "args" else "environment bytes", "variablesReference": 0})
        else:
            row = re.fullmatch(r'  fd=([0-9]{1,10}) storage-kind=(file|file observer|directory|socket|socket observer) rights-base=0x([0-9a-f]{1,16}) rights-inheriting=0x([0-9a-f]{1,16}) preopened=([01])(?: guest-name="(.*)")?(?: managed-resource=([0-9]{1,20}))?', line)
            if (row is None or int(row[1]) > 0x7fffffff or int(row[1]) < result["next"]
                    or (row[7] is not None and int(row[7]) >= 1 << 64) or (row[5] == "1") != (row[6] is not None) or (selected == "preopens" and row[5] != "1")):
                raise ValueError("invalid sorted guest WASIp1 descriptor")
            if row[6] is not None: check_text(row[6])
            number = int(row[1]); result["next"] = number + 1
            result["variables"].append({"name": f"fd {number}", "value": line.strip(), "type": "guest descriptor metadata",
                "variablesReference": 0, "descriptor": number, "rightsBase": int(row[3], 16), "rightsInheriting": int(row[4], 16),
                "preopened": row[5] == "1", "managedResource": int(row[7] or 0)})
    if len(result["variables"]) > result["total"]:
        raise ValueError("WASIp1 rows exceed actual cardinality")
    for variable in result["variables"]:
        variable["presentationHint"] = {"attributes": ["readOnly"]}
    return result


def parse_inline_labels(text, selected_thread):
    """Copy bounded console inline labels; no frame/value/address authority.

    format_reply emits current-physical metadata inner-to-outer before #0.
    Keep escaped text literal. The unquoted display grammar must fail closed
    when a name or path contains its call-site delimiter ambiguously.
    """
    unsigned_id(selected_thread, "inline selected thread")
    if (not isinstance(text, str) or not text.endswith("\n") or "\r" in text
            or len(text.encode("utf-8")) > MAX_REPLY):
        raise ValueError("malformed inline-display reply")
    labels, current, thread_count, frame_started, unavailable, strings = [], None, 0, False, False, 0
    for line in text[:-1].split("\n"):
        if match := THREAD_RE.fullmatch(line):
            thread_count += 1
            current = int(match.group(1))
        elif line.startswith("  #"):
            frame_started = True
        elif line.startswith("  inline "):
            if current != selected_thread or thread_count != 1 or frame_started:
                raise ValueError("inline display does not precede its selected physical frame")
            if any(ord(char) < 32 or ord(char) == 127 for char in line):
                raise ValueError("unescaped inline-display control byte")
            label = line.removeprefix("  inline ")
            if label.startswith("metadata unavailable: "):
                if labels or unavailable or not 0 < len(label) <= 4096:
                    raise ValueError("ambiguous inline metadata availability")
                unavailable = True
                continue
            if unavailable or len(labels) >= 64 or label.count(" call-site ") > 1:
                raise ValueError("ambiguous or excessive inline display")
            if " call-site " in label:
                name, site = label.split(" call-site ", 1)
                match = re.fullmatch(r"(.+):([0-9]{1,20}):([0-9]{1,20})", site)
                if (match is None or any(int(value) >= 1 << 64 for value in match.groups()[1:])
                        or len(match.group(1).encode("utf-8")) > 4096):
                    raise ValueError("invalid inline call-site display")
            else:
                name = label
            if len(name.encode("utf-8")) > 4096:
                raise ValueError("inline name exceeds its display bound")
            strings += len(label.encode("utf-8"))
            if strings > 16384:
                raise ValueError("inline display string budget exceeded")
            labels.append(label or "<anonymous>")
    if thread_count != 1 or current != selected_thread:
        raise ValueError("inline display selected-thread binding differs")
    return labels


def parse_source_frames(text, thread, stop_id, *, with_page=False, wasm=False):
    """Stop-bound labels from the complete canonical chain, in bounded pages."""
    lines = text.splitlines()
    prefix = "wasm-frames" if wasm else "source-frames"
    header = re.fullmatch(prefix + r" stop=([0-9]{1,20}) thread=([0-9]{1,20}) selected=([0-9]{1,20}) count=([0-9]{1,3})(?: total=([0-9]{1,20}) first=([0-9]{1,20}) physical=([0-9]{1,20}))?", lines[0]) if lines else None
    if header is None or len(lines) < 2 or lines[-1] != prefix + " end":
        raise ValueError("complete authenticated frame reply required")
    actual_stop, actual_thread, selected, count = map(int, header.groups()[:4])
    paged = header[5] is not None
    total, first, physical = map(int, header.groups()[4:]) if paged else (count, 0, None)
    if (any(value >= 1 << 64 for value in (actual_stop, actual_thread, selected, count, total, first))
            or actual_stop != stop_id or actual_thread != thread or not 0 < total
            or not 0 <= selected < total or not 0 <= count <= 128 or not 0 <= first <= total
            or count > total - first or len(lines) != count + 2
            or (paged and not 0 <= physical < total)):
        raise ValueError("frame page differs from actual identified stop")
    rows = []
    found_physical = False
    for line in lines[1:-1]:
        match = re.fullmatch(r"  frame ([0-9]{1,20}) kind=(inline|physical|caller) module=([0-9]{1,20}) function=([0-9]{1,20}) generation=([0-9]{1,20}) runtime-epoch=([0-9]{1,20}) scope-unit=([0-9]{1,20}) scope-offset=([0-9]{1,20}) variables=(current|caller-unavailable) name=(.*)", line)
        if match is None:
            raise ValueError("malformed frame row")
        ordinal, kind, module, function, generation, epoch, unit, offset, availability, name = match.groups()
        numbers = tuple(map(int, (ordinal, module, function, generation, epoch, unit, offset)))
        if (numbers[0] != first + len(rows) or any(value >= 1 << 64 for value in numbers) or numbers[3] == 0 or numbers[4] == 0
                or len(name.encode("utf-8")) > 1024 or any(ord(c) < 32 or ord(c) == 127 for c in name)):
            raise ValueError("frame row bound or identity invalid")
        if paged:
            expected = "inline" if numbers[0] < physical else "physical" if numbers[0] == physical else "caller"
            if kind != expected or (kind != "caller" and availability != "current"):
                raise ValueError("frame role differs from canonical physical ordinal")
        elif kind == "physical":
            if found_physical or availability != "current":
                raise ValueError("exactly one current physical frame required")
            found_physical = True
        elif kind == "inline":
            if found_physical or availability != "current":
                raise ValueError("inline frames must precede the current physical frame")
        elif not found_physical:
            raise ValueError("caller must follow the current physical frame")
        if kind == "caller" and availability == "current" and (wasm or numbers[6] == 0):
            # Only the source producer can qualify a saved caller through its
            # authenticated frame-memory callback. Identity-only Wasm pages
            # and caller labels without a concrete DIE grant no source read.
            raise ValueError("saved caller requires authenticated source scope metadata")
        rows.append({"ordinal": numbers[0], "kind": kind, "module": numbers[1], "function": numbers[2],
                     "generation": numbers[3], "runtime_epoch": numbers[4], "scope": (numbers[5], numbers[6]),
                     "current": availability == "current", "name": name, "stop_id": stop_id, "thread": thread})
    if not paged and not found_physical:
        raise ValueError("actual current physical source frame missing")
    page = {"rows": rows, "total": total, "first": first, "selected": selected, "physical": physical}
    return page if with_page else rows


def unsigned_id(value, name):
    # DAP IDs are JSON integers. Reject bool, floats, strings and containers
    # before a request can change the VM's debugger state.
    if type(value) is not int or not 0 < value < (1 << 64):
        raise ValueError(f"valid {name} required")
    return value


def exact(stream, length):
    data = bytearray()
    while len(data) != length:
        part = stream.read(length - len(data))
        if not part:
            raise EOFError("DAP input closed")
        data.extend(part)
    return bytes(data)


def read_dap(stream):
    total = 0
    length = None
    while True:
        line = stream.readline(4097)
        if not line:
            if total == 0:
                return None
            raise EOFError("incomplete DAP header")
        total += len(line)
        if total > 4096 or not line.endswith(b"\r\n"):
            raise ValueError("invalid DAP header")
        if line == b"\r\n":
            break
        name, separator, value = line[:-2].partition(b": ")
        if separator != b": ":
            raise ValueError("invalid DAP header field")
        if name.lower() == b"content-type" and value == b"application/vscode-jsonrpc; charset=utf-8":
            continue
        if name.lower() != b"content-length" or length is not None:
            raise ValueError("invalid DAP Content-Length")
        if not value.isdigit() or len(value) > 7:
            raise ValueError("invalid DAP message length")
        length = int(value)
    if length is None or length < 2 or length > MAX_DAP_MESSAGE:
        raise ValueError("DAP message length out of bounds")
    message = json.loads(exact(stream, length))
    if not isinstance(message, dict) or message.get("type") != "request":
        raise ValueError("expected DAP request")
    if type(message.get("seq")) is not int or not 0 < message["seq"] < (1 << 31):
        raise ValueError("invalid DAP request sequence")
    if not isinstance(message.get("command"), str) or len(message["command"]) > 128:
        raise ValueError("invalid DAP command")
    if "arguments" in message and not isinstance(message["arguments"], dict):
        raise ValueError("invalid DAP arguments")
    return message


def private_capability(directory):
    root = Path(directory).absolute()
    info = root.lstat()
    if not stat.S_ISDIR(info.st_mode) or info.st_uid != os.geteuid() or info.st_mode & 0o077:
        raise ValueError("debug broker directory must be real and owner-only")
    flags = os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0)
    fd = os.open(root / "capability", flags)
    try:
        info = os.fstat(fd)
        if not stat.S_ISREG(info.st_mode) or info.st_uid != os.geteuid() or info.st_mode & 0o077:
            raise ValueError("debug broker capability must be private")
        capability = os.read(fd, 33)
        if len(capability) != 32:
            raise ValueError("invalid debug broker capability")
    finally:
        os.close(fd)
    return root, capability


class UnixBroker:
    def __init__(self, directory):
        root, capability = private_capability(directory)
        kind = socket.SOCK_SEQPACKET if sys.platform == "linux" else socket.SOCK_STREAM
        self.channel = socket.socket(socket.AF_UNIX, kind)
        self.channel.settimeout(8)
        try:
            self.channel.connect(str(root / "control.sock"))
            self.send_raw(capability)
            if self.receive_raw() != b"ready\n":
                raise ValueError("debug broker rejected the capability")
            self.first_request = True
        except BaseException:
            self.channel.close()
            raise

    def send_raw(self, data):
        if sys.platform == "linux":
            if self.channel.send(data) != len(data):
                raise ConnectionError("short broker packet")
        else:
            self.channel.sendall(struct.pack("!I", len(data)) + data)

    def receive_raw(self):
        if sys.platform == "linux":
            data = self._recv_checked(MAX_REPLY)
            if not data:
                raise ConnectionError("broker channel closed")
            return data
        header = self._recv_exact(4)
        length = struct.unpack("!I", header)[0]
        if not 0 < length <= MAX_REPLY:
            raise ConnectionError("invalid broker reply length")
        return self._recv_exact(length)

    def _recv_exact(self, length):
        result = bytearray()
        while len(result) != length:
            part = self._recv_checked(length - len(result))
            if not part:
                raise ConnectionError("broker channel closed")
            result.extend(part)
        return bytes(result)

    def _recv_checked(self, capacity):
        data, ancillary, flags, _ = self.channel.recvmsg(
            capacity, socket.CMSG_SPACE(16 * array.array("i").itemsize))
        for level, kind, payload in ancillary:
            if level == socket.SOL_SOCKET and kind == socket.SCM_RIGHTS:
                rights = array.array("i")
                rights.frombytes(payload[:len(payload) - len(payload) % rights.itemsize])
                for descriptor in rights:
                    os.close(descriptor)
        if ancillary or flags & (socket.MSG_TRUNC | socket.MSG_CTRUNC):
            raise ConnectionError("broker sent ancillary rights or a truncated packet")
        return data

    def request(self, command):
        data = command.encode("utf-8")
        if not 0 < len(data) <= MAX_COMMAND:
            raise ValueError("debug command exceeds 8448 UTF-8 input bytes")
        # Linux's launch broker permits one bounded wait for full JIT startup.
        # Authentication still uses 8 seconds; later commands also use 8.
        startup, self.first_request = self.first_request, False
        self.channel.settimeout(125 if startup and sys.platform == "linux" else 8)
        try:
            self.send_raw(data)
            return self.receive_raw().decode("utf-8", "replace")
        except BaseException:
            # A timed-out packet has no sequence number. Retire this connection
            # rather than letting its eventual reply answer another request.
            self.channel.close()
            raise
        finally:
            if self.channel.fileno() >= 0:
                self.channel.settimeout(8)

    def close(self):
        self.channel.close()


class WindowsBroker:
    def __init__(self, pipe_name, capability):
        if not pipe_name.startswith("\\\\.\\pipe\\uwvm-debug-host-"):
            raise ValueError("invalid Windows debug broker pipe")
        secret = bytes.fromhex(capability)
        if len(secret) != 32:
            raise ValueError("invalid Windows debug broker capability")
        from ctypes import wintypes
        api = ctypes.WinDLL("kernel32", use_last_error=True)
        self.api = api
        self.wintypes = wintypes
        api.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
                                    wintypes.LPVOID, wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
        api.CreateFileW.restype = wintypes.HANDLE
        api.SetNamedPipeHandleState.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD),
                                                 wintypes.LPVOID, wintypes.LPVOID]
        api.SetNamedPipeHandleState.restype = wintypes.BOOL
        api.ReadFile.argtypes = [wintypes.HANDLE, wintypes.LPVOID, wintypes.DWORD,
                                 ctypes.POINTER(wintypes.DWORD), wintypes.LPVOID]
        api.ReadFile.restype = wintypes.BOOL
        api.WriteFile.argtypes = [wintypes.HANDLE, wintypes.LPCVOID, wintypes.DWORD,
                                  ctypes.POINTER(wintypes.DWORD), wintypes.LPVOID]
        api.WriteFile.restype = wintypes.BOOL
        api.CloseHandle.argtypes = [wintypes.HANDLE]
        api.CloseHandle.restype = wintypes.BOOL
        handle = api.CreateFileW(pipe_name, 0xC0000000, 0, None, 3, 0, None)
        if handle in (None, ctypes.c_void_p(-1).value):
            raise OSError(ctypes.get_last_error(), "open Windows debug broker")
        self.handle = handle
        try:
            mode = wintypes.DWORD(2)  # PIPE_READMODE_MESSAGE
            if not api.SetNamedPipeHandleState(handle, ctypes.byref(mode), None, None):
                raise OSError(ctypes.get_last_error(), "set broker message mode")
            self.send_raw(secret)
            if self.receive_raw() != b"ready\n":
                raise ValueError("debug broker rejected the capability")
        except BaseException:
            self.close()
            raise

    def send_raw(self, data):
        payload = ctypes.create_string_buffer(data)
        written = self.wintypes.DWORD()
        if not self.api.WriteFile(self.handle, payload, len(data), ctypes.byref(written), None) or written.value != len(data):
            raise OSError(ctypes.get_last_error(), "write Windows debug broker")

    def receive_raw(self):
        buffer = ctypes.create_string_buffer(MAX_REPLY)
        count = self.wintypes.DWORD()
        if not self.api.ReadFile(self.handle, buffer, MAX_REPLY, ctypes.byref(count), None) or not count.value:
            raise OSError(ctypes.get_last_error(), "read Windows debug broker")
        return buffer.raw[:count.value]

    def request(self, command):
        data = command.encode("utf-8")
        if not 0 < len(data) <= MAX_COMMAND:
            raise ValueError("debug command exceeds 8448 UTF-8 input bytes")
        self.send_raw(data)
        return self.receive_raw().decode("utf-8", "replace")

    def close(self):
        if getattr(self, "handle", None) is not None:
            self.api.CloseHandle(self.handle)
            self.handle = None


def parse_native_disassembly(text, location, count):
    """Accept exact bounded controller output; no address is accepted as input.

    Real runtime ownership is verified by the code-copy API, not this parser.
    The public stop label only detects stale replies. Unavailable trailing rows
    are DAP invalid filler with no address or byte disclosure. The wire PC is
    checked for continuity internally; it is not a decoded instruction.
    """
    if (type(count) is not int or not 1 <= count <= 32 or not isinstance(text, str)
            or not text.endswith("\n") or "\r" in text or len(text.encode("utf-8")) > MAX_REPLY
            or any(ord(character) < 32 and character != "\n" or ord(character) == 127 for character in text)):
        raise ValueError("malformed native-disassembly reply")
    lines = text.splitlines()
    if len(lines) != count + 2 or lines[-1] != "native-disassembly-end":
        raise ValueError("incomplete native-disassembly reply")
    header = re.fullmatch(r"native-disassembly stop=([0-9]{1,20}) thread=([0-9]{1,20}) module=([0-9]{1,20}) "
                          r"function=([0-9]{1,20}) function-generation=([0-9]{1,20}) runtime-epoch=([0-9]{1,20})(?: origin=(?:native-instruction-stop|safepoint-code-view))?", lines[0])
    if header is None:
        raise ValueError("invalid native-disassembly identity")
    stop, thread, module, function, generation, epoch = map(int, header.groups())
    if (not 0 < stop < 1 << 64 or not 0 < thread < 1 << 64 or not 0 <= module < 1 << 64
            or not 0 <= function < 1 << 64 or not 0 < generation < 1 << 64 or not 0 < epoch < 1 << 64
            or (stop, thread, module, function, epoch) !=
               (location["stop_id"], location["id"], location["module"], location["function"], location["generation"])):
        raise ValueError("native-disassembly reply belongs to another stop/code owner")
    pc = int(location["native_pc"], 16)
    instructions = []
    unavailable = False
    total_bytes = 0
    for index, line in enumerate(lines[1:-1]):
        row = re.fullmatch(r"  instruction ([0-9]{1,2}) pc=0x([0-9a-fA-F]{1,16})(?: bytes=([0-9a-fA-F]{2}(?: [0-9a-fA-F]{2}){0,14})  (.{1,508})| (unavailable))", line)
        if row is None or int(row[1]) != index or int(row[2], 16) != pc or pc == 0:
            raise ValueError("invalid or noncontiguous native-disassembly row")
        if row[5] is not None:
            unavailable = True
            instructions.append({"address": "-1", "instruction": "<unavailable instruction>",
                                 "presentationHint": "invalid"})
            continue
        if unavailable:
            raise ValueError("decoded row follows unavailable native boundary")
        raw = row[3].split(" ")
        total_bytes += len(raw)
        if total_bytes > 480 or pc > (1 << 64) - 1 - len(raw):
            raise ValueError("native-disassembly code extent overflow")
        instructions.append({"address": f"0x{pc:x}", "instructionBytes": " ".join(raw).lower(),
                             "instruction": row[4], "presentationHint": "normal"})
        pc += len(raw)
    return instructions


def parse_native_disassembly_range(text, location, count, byte_offset, instruction_offset, resolve_symbols):
    """Exact owned-function protocol, not an address or expression evaluator."""
    if (type(count) is not int or not 1 <= count <= 32 or not isinstance(text, str)
            or not text.endswith("\n") or "\r" in text or len(text.encode("utf-8")) > MAX_REPLY
            or any(ord(c) < 32 and c != "\n" or ord(c) == 127 for c in text)):
        raise ValueError("malformed owned-function disassembly reply")
    lines = text[:-1].split("\n")
    if not lines or lines[-1] != "native-disassembly-end":
        raise ValueError("incomplete owned-function disassembly reply")
    header = re.fullmatch(r"native-disassembly-range stop=([0-9]{1,20}) thread=([0-9]{1,20}) module=([0-9]{1,20}) "
                          r"function=([0-9]{1,20}) function-generation=([0-9]{1,20}) runtime-epoch=([0-9]{1,20}) "
                          r"(?:origin=(?:native-instruction-stop|safepoint-code-view) )?reference-pc=0x([0-9a-fA-F]{1,16}) owner-begin=0x([0-9a-fA-F]{1,16}) owner-end=0x([0-9a-fA-F]{1,16}) "
                          r"byte-offset=(-?[0-9]{1,6}) instruction-offset=(-?[0-9]{1,5}) resolve-symbols=([01])", lines[0])
    if header is None:
        raise ValueError("invalid owned-function disassembly header")
    stop, thread, module, function, generation, epoch = map(int, header.groups()[:6])
    pc, begin, end = (int(value, 16) for value in header.groups()[6:9])
    if (not 0 < stop < 1 << 64 or not 0 < thread < 1 << 64 or not 0 <= module < 1 << 64
            or not 0 <= function < 1 << 64 or not 0 < generation < 1 << 64 or not 0 < epoch < 1 << 64
            or (stop, thread, module, function, epoch) !=
               (location["stop_id"], location["id"], location["module"], location["function"], location["generation"])
            or not 0 < begin <= pc < end < 1 << 64 or not 0 < end - begin <= 65536
            or ("native_pc" in location and pc != int(location["native_pc"], 16))
            or tuple(map(int, header.groups()[9:])) != (byte_offset, instruction_offset, int(resolve_symbols))):
        raise ValueError("owned-function reply belongs to another stop, owner or offset")
    identity = (stop, thread, module, function, generation, epoch, pc, begin, end)
    rows = lines[1:-1]
    name = None
    if rows and rows[0].startswith("native-function-name "):
        if not resolve_symbols:
            raise ValueError("unsolicited native symbol name")
        name = rows.pop(0).removeprefix("native-function-name ")
        if not name or len(name.encode("utf-8")) > 1024:
            raise ValueError("native symbol name exceeds bounded escaped display")
    # Escaped optional display name is kept separate from permissions, but
    # all pages must still reflect the same name attempt/omission.
    identity += (name,)
    if len(rows) != count:
        raise ValueError("owned-function instruction count differs")
    instructions, previous_end, had_normal, trailing = [], None, False, False
    for index, line in enumerate(rows):
        invalid = re.fullmatch(r"  instruction ([0-9]{1,2}) unavailable", line)
        if invalid is not None:
            if int(invalid[1]) != index:
                raise ValueError("invalid filler index")
            if had_normal or (byte_offset == 0 and instruction_offset == 0):
                trailing = True
            instructions.append({"address": "-1", "instruction": "<unavailable instruction>", "presentationHint": "invalid"})
            continue
        row = re.fullmatch(r"  instruction ([0-9]{1,2}) pc=0x([0-9a-fA-F]{1,16}) bytes=([0-9a-fA-F]{2}(?: [0-9a-fA-F]{2}){0,14})  (.{1,508})", line)
        if row is None or int(row[1]) != index or trailing:
            raise ValueError("malformed or resumed-after-gap instruction row")
        address = int(row[2], 16)
        raw = row[3].split(" ")
        if not begin <= address < end or len(raw) > end - address or (previous_end is not None and address != previous_end):
            raise ValueError("instruction extends outside actual owner or skips a boundary")
        previous_end, had_normal = address + len(raw), True
        instruction = {"address": f"0x{address:x}", "instructionBytes": " ".join(raw).lower(),
                       "instruction": row[4], "presentationHint": "normal"}
        if name is not None:
            instruction["symbol"] = name
        instructions.append(instruction)
    if byte_offset == 0 and instruction_offset == 0 and (not instructions or
            (instructions[0]["presentationHint"] == "normal" and int(instructions[0]["address"], 16) != pc)):
        raise ValueError("current stopped PC was not actually decoded")
    return identity, instructions


def parse_status(text):
    state = "running"
    reason = "pause"
    threads = []
    current = None
    exit_code = None
    native_stop = False
    uncaught_stop = False
    uncaught_thread = None
    stop_id = None
    for line in text.splitlines():
        if line.startswith("stopped: "):
            state = "stopped"
            uncaught_stop = line == "stopped: Uncaught Wasm exception before unwind; continue propagates"
            reason = ("exception" if uncaught_stop or line == "stopped: Wasm trap after failure; pre-trap operands; continue terminates" else
                      "breakpoint" if "breakpoint" in line else "step" if "step" in line else "pause")
            native_stop = line == "stopped: native instruction step"
        elif line.startswith("prepared;"):
            state, reason = "stopped", "entry"
        elif line == "running":
            state = "running"
        elif line == "debug domain closed":
            state = "closed"
        elif match := EXIT_RE.match(line):
            state, exit_code = "exited", int(match.group(1))
        elif line.startswith("uncaught-thread "):
            if not uncaught_stop or uncaught_thread is not None or threads or not re.fullmatch(r"uncaught-thread [1-9][0-9]*", line):
                raise ValueError("invalid uncaught Wasm participant")
            uncaught_thread = unsigned_id(int(line.split()[1]), "uncaught participant")
        elif line.startswith("stop-id "):
            match = STOP_ID_RE.fullmatch(line)
            if match is None or state != "stopped" or stop_id is not None or threads:
                raise ValueError("invalid or ambiguous stopped identity")
            stop_id = unsigned_id(int(match.group(1)), "stop identifier")
        elif match := THREAD_RE.match(line):
            current = {"id": int(match.group(1)), "module": int(match.group(2)),
                       "function": int(match.group(3)), "offset": int(match.group(4)),
                       "generation": int(match.group(5))}
            if stop_id is not None:
                current["stop_id"] = stop_id
            if current["id"] == uncaught_thread:
                current["uncaught_exception"] = True
            threads.append(current)
        elif current is not None and (match := SOURCE_RE.match(line)):
            current["source"] = {"path": match.group(1), "line": int(match.group(2)),
                                 "column": int(match.group(3))}
        elif line.startswith("  native-pc="):
            match = NATIVE_PC_RE.fullmatch(line)
            if (current is None or state != "stopped" or not native_stop or match is None
                    or int(match.group(1), 16) == 0 or "native_pc" in current
                    or current["id"] == 0 or current["generation"] == 0
                    or any(current[key] >= 1 << 64
                           for key in ("id", "module", "function", "offset", "generation"))):
                raise ValueError("invalid native stopped-thread location")
            # Only the authenticated controller's complete stopped reply supplies
            # this PC. It is display metadata, never an address-reading capability.
            current["native_pc"] = f"0x{int(match.group(1), 16):x}"
    if uncaught_stop and (uncaught_thread is None or sum(t.get("uncaught_exception", False) for t in threads) != 1):
        raise ValueError("uncaught Wasm stop lacks its actual participant")
    native_threads = [thread for thread in threads if "native_pc" in thread]
    if (len(native_threads) > 1 or any("source" in thread for thread in native_threads)
            or (native_threads and (state != "stopped" or not native_stop))
            or (native_threads and len({thread["id"] for thread in threads}) != len(threads))):
        raise ValueError("ambiguous native stopped-thread location")
    return state, reason, threads, exit_code



# Exact public register vocabulary shared with native_registers.h. The sets
# describe copied display values; they never authorize reading a target address.
def native_register_layout(architecture):
    gp, fp, forbidden, pc, bits = {
        "x86_64": (["rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp"] + [f"r{i}" for i in range(8, 16)] + ["rip", "rflags"],
                   [f"xmm{i}" for i in range(16)] + [f"st{i}" for i in range(8)] + ["fcw", "fsw", "ftw", "fop", "mxcsr", "mxcsr_mask"],
                   {"rbp", "rsp", "rflags", *[f"st{i}" for i in range(8)], "fcw", "fsw", "ftw", "fop", "mxcsr", "mxcsr_mask"}, "rip", (64,)),
        "aarch64": ([f"x{i}" for i in range(29)] + ["fp", "lr", "sp", "pc", "cpsr"], [f"v{i}" for i in range(32)],
                    {"x18", "fp", "lr", "sp", "cpsr"}, "pc", (64,)),
        "i686": (["eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "esp", "eip", "eflags"], [f"xmm{i}" for i in range(8)],
                 {"ebp", "esp", "eflags"}, "eip", (32,)),
        "powerpc": ([f"r{i}" for i in range(32)] + ["pc", "msr", "lr", "ctr", "cr", "xer"], [f"f{i}" for i in range(32)] + [f"v{i}" for i in range(32)],
                    {"r1", "r2", "r13", "r31", "msr", "lr", "ctr", "cr", "xer"}, "pc", (32, 64)),
        "mips64": (["zero", "at", "v0", "v1"] + [f"a{i}" for i in range(8)] + [f"t{i}" for i in range(4)] + [f"s{i}" for i in range(8)] + ["t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra", "pc", "status", "hi", "lo"],
                   [f"f{i}" for i in range(32)], {"zero", "k0", "k1", "gp", "sp", "fp", "ra", "status", "hi", "lo"}, "pc", (64,)),
        "riscv64": (["zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2", "s0", "s1"] + [f"a{i}" for i in range(8)] + [f"s{i}" for i in range(2, 12)] + ["t3", "t4", "t5", "t6", "pc", "status"],
                    [f"f{i}" for i in range(32)], {"zero", "ra", "sp", "gp", "tp", "s0", "status"}, "pc", (64,)),
        "loongarch64": (["zero", "ra", "tp", "sp"] + [f"a{i}" for i in range(8)] + [f"t{i}" for i in range(9)] + ["r21", "fp"] + [f"s{i}" for i in range(9)] + ["pc", "status"],
                       [f"f{i}" for i in range(32)], {"zero", "ra", "tp", "sp", "r21", "fp", "status"}, "pc", (64,)),
        "sparc64": ([f"{bank}{i}" for bank in "goli" for i in range(8)] + ["pc", "npc", "tstate", "y"], [f"d{i}" for i in range(32)] + [f"f{i}" for i in range(32)],
                    {"g0", "g6", "g7", "o6", "o7", "i6", "i7", "npc", "tstate", "y"}, "pc", (64,)),
        "s390x": ([f"r{i}" for i in range(16)] + ["pc", "psw"], [f"f{i}" for i in range(16)], {"r14", "r15", "psw"}, "pc", (64,)),
        "arm": ([f"r{i}" for i in range(13)] + ["sp", "lr", "pc", "cpsr"], [f"d{i}" for i in range(32)], {"r11", "sp", "lr", "cpsr"}, "pc", (32,)),
    }.get(architecture, (None, None, None, None, None))
    if gp is None:
        raise ValueError("unsupported native register architecture")
    return gp, fp, forbidden, pc, bits


def parse_native_registers(text, location):
    """Strict read-only numeric projection, never a process memory reference."""
    if not isinstance(text, str) or len(text) > 16384:
        raise ValueError("bounded native register reply required")
    lines = text.splitlines()
    if len(lines) < 3 or lines[-1] != "native-registers end":
        raise ValueError("incomplete native register reply")
    header = re.fullmatch(r"native-registers stop=([0-9]{1,20}) thread=([0-9]{1,20}) frame=0 module=([0-9]{1,20}) function=([0-9]{1,20}) generation=([0-9]{1,20}) epoch=([0-9]{1,20}) architecture=([a-z0-9_]+)(?: word-bits=(32|64))?", lines[0])
    if header is None:
        raise ValueError("invalid native register identity")
    stop, thread, module, function, generation, epoch = map(int, header.groups()[:6])
    if (any(not 0 <= value < 1 << 64 for value in (stop, thread, module, function, generation, epoch))
            or min(stop, thread, generation, epoch) == 0
            or (stop, thread, module, function, epoch) != (location["stop_id"], location["id"], location["module"], location["function"], location["generation"])):
        raise ValueError("native register reply belongs to another stop or owner")
    architecture = header[7]
    gp, fp, forbidden, pc_name, allowed_bits = native_register_layout(architecture)
    if header[8] is None and architecture not in ("x86_64", "aarch64"):
        raise ValueError("explicit target word width required")
    word_bits = int(header[8] or 64)
    if word_bits not in allowed_bits:
        raise ValueError("native word width disagrees with architecture")
    # The historical AArch64 all-register reply contained GPRs only.
    names = gp if header[8] is None and architecture == "aarch64" else gp + fp
    if len(lines) != len(names) + 2:
        raise ValueError("short or excessive native register set")
    values = []
    for expected, line in zip(names, lines[1:-1]):
        row = re.fullmatch(r"  ([a-z][a-z0-9_]*)=(unavailable|0x[?0-9a-fA-F]{8,32})", line)
        if row is None or row[1] != expected:
            raise ValueError("invalid, duplicated or reordered native register")
        value = row[2]
        if value != "unavailable":
            if expected in forbidden:
                raise ValueError("unqualified native control or runtime register")
            width = (32 if expected.startswith(("xmm", "v")) or architecture == "loongarch64" else
                     8 if architecture == "sparc64" and expected.startswith("f") else 16) if expected in fp else word_bits // 4
            digits = value[2:]
            if len(digits) != width or not re.fullmatch(r"(?:\?{0}|\?{8}|\?{16}|\?{24})[0-9a-fA-F]+", digits):
                raise ValueError("invalid partial numeric register bits")
            known = len(digits.lstrip("?"))
            if known not in (8, 16, 32) or known > width:
                raise ValueError("unqualified upper numeric register bits")
        if expected == pc_name and (value == "unavailable" or "?" in value or int(value, 16) != int(location["native_pc"], 16)):
            raise ValueError("native register PC disagrees with the actual stop")
        values.append({"name": expected, "value": value.lower(), "variablesReference": 0,
                       "presentationHint": {"attributes": ["readOnly"]}})
    return architecture, values


def native_register_selector(expression, architecture):
    if not isinstance(expression, str) or len(expression) > 32 or re.fullmatch(r"\$?[a-z][a-z0-9_]*(?:\([0-7]\))?", expression) is None:
        raise ValueError("a single native register name is required; address expressions are unavailable")
    name = expression.removeprefix("$")
    if architecture == "x86_64":
        name = {"pc": "rip", "sp": "rsp", "fp": "rbp", "eflags": "rflags", "flags": "rflags", "ps": "rflags"}.get(name, name)
        if re.fullmatch(r"st\([0-7]\)", name): name = "st" + name[3]
    elif architecture == "aarch64":
        name = {"x29": "fp", "x30": "lr", "flags": "cpsr", "ps": "cpsr"}.get(name, name)
    elif architecture == "i686":
        name = {"pc": "eip", "sp": "esp", "fp": "ebp", "flags": "eflags", "ps": "eflags"}.get(name, name)
    else:
        name = {"sp": {"powerpc": "r1", "riscv64": "sp", "sparc64": "o6", "s390x": "r15"}.get(architecture, "sp"),
                "fp": {"powerpc": "r31", "riscv64": "s0", "sparc64": "i6", "arm": "r11"}.get(architecture, "fp")}.get(name, name)
    return name


class Adapter:
    def __init__(self, output):
        self.output = output
        self.sequence = 1
        self.broker = None
        self.threads = []
        self.state = "detached"
        self.stop_key = None
        self.breakpoints = {}
        self.instruction_breakpoints = []
        self.frames = {}
        self.scopes = {}
        self.wasm_local_scope_stops = {}
        self.native_frames = set()
        self.native_register_scopes = {}
        self.native_code_refs = {}
        self.source_frames = set()
        self.source_frame_ordinals = {}
        self.source_scope_stops = {}
        self.source_object_views = {}
        self.source_scope_frames = {}
        self.source_pointer_views = {}
        self.source_object_nodes = self.source_object_bytes = 0
        self.wasm_scope_stops = {}
        self.wasip1_scope_stops = {}
        self.wasm_object_views = {}
        self.deep_wasm_paths = {}
        self.wasm_path_indices = 0
        self.next_reference = 1
        self.module_id = 0
        self.step_level = "source"
        self.inline_frames = set()
        self.finished = False
        self.supports_memory_event = False
        self.supports_invalidated_event = False
        self.supports_variable_type = False

    def emit(self, message):
        message["seq"] = self.sequence
        self.sequence += 1
        payload = json.dumps(message, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
        self.output.write(b"Content-Length: " + str(len(payload)).encode("ascii") + b"\r\n\r\n" + payload)
        self.output.flush()

    def respond(self, request, body=None, error=None):
        result = {"type": "response", "request_seq": request["seq"],
                  "command": request["command"], "success": error is None}
        if error is not None:
            result["message"] = str(error)
        if body is not None:
            result["body"] = body
        self.emit(result)

    def event(self, name, body=None):
        result = {"type": "event", "event": name}
        if body is not None:
            result["body"] = body
        self.emit(result)

    def command(self, text):
        if self.broker is None:
            raise ValueError("attach to a host debug broker first")
        # A timeout or transport failure can happen after an actual resume.
        # Retire stopped references before every console spelling that can
        # advance execution, including aliases sent through DAP evaluate.
        words = text.split()
        # The VM has only the exact "replace" spelling (no replacement alias).
        # Retire before sending even a malformed/failed replacement: its reply
        # may fail after publication or transport loss. Old IDs never resurrect.
        replacing = bool(words and words[0] == "replace")
        # A bounded VM script may commit an ABI-checked replacement in any
        # child. Retire BEFORE sending the whole batch, including malformed,
        # rejected or diagnostic-only batches. Do not infer child success or a
        # new pause/generation from its transcript or truncated display.
        scripted = bool(words and words[0] == "wasm-script")
        if replacing or scripted or words and words[0] in ("continue", "c", "step", "s", "ni", "nexti", "finish", "fin", "quit", "q"):
            self.invalidate_stop()
        try:
            reply = self.broker.request(text)
        except (OSError, ValueError, RuntimeError, ConnectionError):
            self.invalidate_stop()
            raise
        if reply.startswith("error: "):
            self.invalidate_stop()
            raise RuntimeError(reply.strip())
        if replacing:
            # Replacement text carries only the replaced function generation,
            # not a current stop. Fetch genuine complete status synchronously;
            # a queued scopes/variables request must not rely on idle polling.
            try:
                refreshed = self.command("status")
                # parse_status intentionally treats unrecognized historical text
                # as running. A replacement refresh must not use that fallback
                # or emit a stopped event before its complete identity exists.
                lines = refreshed.splitlines()
                state, _, threads, _ = parse_status(refreshed)
                if (not lines or not lines[0].startswith("stopped: ") or
                        sum(line.startswith("stopped: ") for line in lines) != 1 or
                        state != "stopped" or not threads):
                    raise ValueError("no complete authoritative stopped snapshot after replacement")
                for thread in threads:
                    unsigned_id(thread["id"], "replacement stopped participant")
                    unsigned_id(thread["generation"], "replacement runtime generation")
                    unsigned_id(thread.get("stop_id"), "replacement stop identifier")
                    if any(not 0 <= thread[key] < (1 << 64) for key in ("module", "function", "offset")):
                        raise ValueError("replacement stopped location is out of range")
                if (len({thread["id"] for thread in threads}) != len(threads) or
                        len({thread["stop_id"] for thread in threads}) != 1):
                    raise ValueError("ambiguous replacement stopped identity")
                self.observe(refreshed)
            except (OSError, ValueError, RuntimeError) as error:
                self.invalidate_stop()
                raise RuntimeError("replacement acknowledged; stop refresh failed; "
                                   "all debugger references remain retired: " + str(error)) from error
        return reply

    def invalidate_stop(self):
        self.stop_key = None
        self.frames.clear()
        self.scopes.clear()
        self.wasm_local_scope_stops.clear()
        self.native_frames.clear()
        self.native_register_scopes.clear()
        self.native_code_refs.clear()
        self.source_frames.clear()
        self.source_frame_ordinals.clear()
        self.source_scope_stops.clear()
        self.source_object_views.clear()
        self.source_scope_frames.clear()
        self.source_pointer_views.clear()
        self.source_object_nodes = self.source_object_bytes = 0
        self.wasm_scope_stops.clear()
        self.wasip1_scope_stops.clear()
        self.wasm_object_views.clear(); self.deep_wasm_paths.clear(); self.wasm_path_indices = 0
        self.inline_frames.clear()

    def source_object_display(self, text, expected_key, frame_id, row):
        nodes = parse_source_object_display(text, row['stop_id'], row['thread'])
        if nodes is None:
            return source_evaluation_display(text)
        if not nodes:
            raise ValueError("empty source object display")
        retained = any(node['children'] for node in nodes)
        byte_count = len(text.encode('utf-8'))
        if retained and (self.source_object_nodes + len(nodes) > 8192 or self.source_object_bytes + byte_count > 512 * 1024):
            raise ValueError("copied source object views exceed the per-stop budget")
        references = [self.new_reference() if node['children'] else 0 for node in nodes]
        variables = []
        for index, node in enumerate(nodes):
            variable = dict(name=node['name'], type=node['type'], value=node['value'],
                            variablesReference=references[index], presentationHint={'attributes': ['readOnly']})
            if node['children']:
                variable['namedVariables'] = sum(not nodes[child]['indexed'] for child in node['children'])
                variable['indexedVariables'] = sum(nodes[child]['indexed'] for child in node['children'])
            if not self.supports_variable_type:
                variable.pop('type')
            variables.append(variable)
        # Allocate references only after the entire immutable copied image has
        # passed validation. Expansion serves this snapshot and issues status
        # checks only; labels cannot cause another print/read/dereference.
        for index, node in enumerate(nodes):
            if references[index]:
                self.source_object_views[references[index]] = (expected_key, frame_id,
                    tuple((dict(variables[child]), nodes[child]['indexed']) for child in node['children']))
        if retained:
            self.source_object_nodes += len(nodes)
            self.source_object_bytes += byte_count
        body = dict(variables[0]);body['result'] = body.pop('value');body.pop('name')
        return body

    def source_root_packet(self, expected_key, frame_id, row, name, dereference=False):
        """Read only a reselected current symbol; never use copied address bits."""
        if (not isinstance(name, str) or re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]{0,127}", name) is None
                or name in ("true", "false", "nil", "len", "cap", "sizeof", "alignof")
                or not row["current"]):
            raise ValueError("an actual current source symbol is required")
        def current():
            if (self.state != "stopped" or self.stop_key != expected_key
                    or self.source_frame_ordinals.get(frame_id) != (expected_key, row)
                    or frame_id in self.native_frames):
                raise ValueError("source symbol belongs to a retired source frame")
        self.observe(self.command("status"), notify=False);current()
        # Only this finite spelling can take the optional read path. Logical
        # unavailability is NOT permission to retry a transport/malformed error.
        expression = ("*" if dereference else "") + name
        try:
            reply = self.broker.request(f"print-frame {row['thread']} {row['stop_id']} {row['ordinal']} {expression}")
        except (OSError, ValueError, RuntimeError, ConnectionError):
            self.invalidate_stop()
            raise
        self.observe(self.command("status"), notify=False);current()
        unavailable = "error: source value unavailable: current captured location, layout or single unshared local-memory read is unavailable\n"
        if reply == unavailable:
            return None
        if not isinstance(reply, str) or reply.startswith("error: "):
            raise ValueError("source symbol read failed: " + str(reply).strip())
        validate_source_evaluation_reply(reply, row["stop_id"], row["thread"])
        if not reply.startswith(f"source-value stop={row['stop_id']} name={expression}\n"):
            return None  # Numeric/extended packets keep their original scope row.
        nodes = parse_source_object_display(reply, row["stop_id"], row["thread"])
        return (reply, nodes) if nodes else None

    def source_scope_objects(self, reference, expected_key, row, variables, all_variables):
        frame_id = self.source_scope_frames.get(reference)
        if frame_id is None:
            return variables  # A legacy display label grants no frame selection.
        counts = {}
        for variable in all_variables:
            counts[variable["name"]] = counts.get(variable["name"], 0) + 1
        reads, projected_bytes, result = 0, 0, []
        for variable in variables:
            name = variable["name"]
            if (reads >= 32 or counts[name] != 1
                    or variable["value"] != "unavailable (unsupported or unknown scalar type)"
                    or re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]{0,127}", name) is None
                    or name in ("true", "false", "nil", "len", "cap", "sizeof", "alignof")):
                result.append(variable);continue
            reads += 1
            packet = self.source_root_packet(expected_key, frame_id, row, name)
            if packet is None:
                result.append(variable);continue
            text, nodes = packet
            # Bound the extra scope reply metadata before creating any views.
            cost = len(nodes[0]["type"]) * 2 + len(nodes[0]["value"]) * 2 + 256
            if projected_bytes + cost > 65536:
                result.append(variable);continue
            projected_bytes += cost
            display = self.source_object_display(text, expected_key, frame_id, row)
            value = dict(display);value["name"] = name;value["value"] = value.pop("result")
            # Pointer values are only display. A lazy view stores the original
            # validated symbol/frame, never an offset decoded from these bits.
            if re.fullmatch(r"guest:0x[0-9a-fA-F]{1,16}", value["value"]) and int(value["value"][8:], 16) != 0:
                size = len(text.encode("utf-8"))
                if self.source_object_nodes + 1 > 8192 or self.source_object_bytes + size > 512 * 1024:
                    raise ValueError("source pointer views exceed the per-stop budget")
                pointer = self.new_reference()
                self.source_pointer_views[pointer] = (expected_key, frame_id, row, name)
                self.source_object_nodes += 1;self.source_object_bytes += size
                value["variablesReference"] = pointer
            result.append(value)
        return result

    def source_pointer_members(self, reference):
        expected_key, frame_id, row, name = self.source_pointer_views[reference]
        packet = self.source_root_packet(expected_key, frame_id, row, name, dereference=True)
        if reference not in self.source_pointer_views:
            raise ValueError("source pointer view retired during its read")
        if packet is None:
            value = dict(name="<unavailable>", value="pointee has no supported captured layout or guest-memory read",
                         variablesReference=0, presentationHint={"attributes": ["readOnly"]})
            copied = ((value, False),)
        else:
            text, _ = packet
            display = self.source_object_display(text, expected_key, frame_id, row)
            copied_reference = display["variablesReference"]
            if copied_reference:
                _, _, copied = self.source_object_views.pop(copied_reference)
            else:
                size = len(text.encode("utf-8"))
                if self.source_object_nodes + 1 > 8192 or self.source_object_bytes + size > 512 * 1024:
                    raise ValueError("copied source pointees exceed the per-stop budget")
                self.source_object_nodes += 1;self.source_object_bytes += size
                value = dict(display);value["name"] = "*";value["value"] = value.pop("result")
                copied = ((value, False),)
        # From this point expansion serves the copied snapshot only. Member
        # labels and nested pointers cannot create another read recipe.
        self.source_object_views[reference] = (expected_key, frame_id, copied)
        del self.source_pointer_views[reference]

    def query_native_registers(self, frame_id):
        frame = self.frames.get(frame_id)
        expected_key = self.stop_key
        if frame_id not in self.native_frames or frame is None or frame[1] != -1 or expected_key is None:
            raise ValueError("current native frame required")
        self.observe(self.command("status"), notify=False)
        if self.state != "stopped" or self.stop_key != expected_key or frame_id not in self.native_frames:
            raise ValueError("native registers belong to a retired stop")
        locations = [row for row in self.threads if row["id"] == frame[0]]
        if len(locations) != 1 or "stop_id" not in locations[0] or "native_pc" not in locations[0]:
            raise ValueError("identified native instruction stop required")
        architecture, values = parse_native_registers(self.command(f"info registers {frame[0]} {locations[0]['stop_id']} all"), locations[0])
        self.observe(self.command("status"), notify=False)
        if self.state != "stopped" or self.stop_key != expected_key or frame_id not in self.native_frames:
            raise ValueError("native registers retired during copy")
        return architecture, values

    def current_frame(self, frame_id):
        frame = self.frames.get(frame_id)
        expected_key = self.stop_key
        if frame is None or expected_key is None:
            raise ValueError("stale or unknown stack frame")
        # A different controller can advance the VM before the adapter's idle
        # poll. Never publish a new scope or source from a cached stopped label.
        self.observe(self.command("status"), notify=False)
        if self.state != "stopped" or self.stop_key != expected_key or frame_id not in self.frames:
            raise ValueError("stack frame belongs to a retired stop")
        return frame

    def new_reference(self):
        # DAP frame/scope references are session-unique signed 32-bit IDs.
        # Exhaustion refuses a new reference; it must never wrap to an old ID.
        if not 0 < self.next_reference <= (1 << 31) - 1:
            raise ValueError("DAP stopped-reference space exhausted")
        reference = self.next_reference
        self.next_reference += 1
        return reference

    def _decode_status(self, reply):
        try:
            return parse_status(reply)
        except ValueError:
            self.invalidate_stop()
            raise

    def observe(self, reply, notify=True):
        return self._commit_status(self._decode_status(reply), notify)

    def _commit_status(self, snapshot, notify=True):
        # Only a previously decoded snapshot reaches this publication path.
        state, reason, threads, code = snapshot
        self.threads = threads
        self.state = state
        if state == "stopped":
            key = (reason, tuple((t["id"], t["module"], t["function"], t["offset"],
                                  t["generation"], t.get("native_pc"), t.get("stop_id")) for t in threads))
            if key != self.stop_key:
                self.stop_key = key
                self.frames.clear()
                self.scopes.clear()
                self.wasm_local_scope_stops.clear()
                self.native_frames.clear()
                self.native_register_scopes.clear()
                self.native_code_refs.clear()
                self.source_frames.clear()
                self.source_frame_ordinals.clear()
                self.source_scope_stops.clear()
                self.source_object_views.clear()
                self.source_scope_frames.clear()
                self.source_pointer_views.clear()
                self.source_object_nodes = self.source_object_bytes = 0
                self.wasm_scope_stops.clear()
                self.wasip1_scope_stops.clear()
                self.wasm_object_views.clear(); self.deep_wasm_paths.clear(); self.wasm_path_indices = 0
                self.inline_frames.clear()
                # DAP clients can retain an old frame/scope ID across stops.
                # Keep IDs session-unique so an old request cannot alias a
                # different frame after this snapshot is replaced.
                if notify:
                    self.event("stopped", {"reason": reason, "threadId": threads[0]["id"] if threads else 1,
                                           "allThreadsStopped": True})
                return True
        elif state == "exited":
            self.invalidate_stop()
            if not self.finished:
                self.finished = True
                self.event("exited", {"exitCode": code or 0})
                self.event("terminated")
        elif state == "running":
            self.invalidate_stop()
        elif state == "closed":
            self.invalidate_stop()
            if not self.finished:
                self.finished = True
                self.event("terminated")
        return False

    def poll(self):
        if self.broker is not None and not self.finished:
            self.observe(self.command("status"))

    def native_range_page(self, location, count, byte_offset, instruction_offset, resolve_symbols):
        text = self.command(f"disassemble-range {location['id']} {location['stop_id']} {count} "
                            f"{byte_offset} {instruction_offset} {int(resolve_symbols)}")
        return parse_native_disassembly_range(text, location, count, byte_offset, instruction_offset, resolve_symbols)

    def qualify_native_code_reference(self, location):
        # A top physical cooperative frame gets code display permission only
        # after a REAL private-owner readonly query, not from its Wasm offset.
        expected_key = self.stop_key
        if expected_key is None or "stop_id" not in location or "native_pc" in location:
            return None
        try:
            identity, rows = self.native_range_page(location, 1, 0, 0, False)
            self.observe(self.command("status"), notify=False)
            current = [entry for entry in self.threads if entry["id"] == location["id"]]
            if (self.state != "stopped" or self.stop_key != expected_key or current != [location]
                    or len(rows) != 1):
                return None
            reference = self.new_reference()
            self.native_code_refs[reference] = (location["id"], expected_key, identity)
            return f"uwvm-native-code:{reference}"
        except (OSError, ValueError, RuntimeError, ConnectionError):
            # No qualified owner/backend is an explicit lack of code reference;
            # it never becomes a source/locals/address capability.
            return None

    def disassemble_owned_range(self, request, args, reference, count):
        match = re.fullmatch(r"uwvm-native-(stop|code):([1-9][0-9]{0,9})", reference)
        if match is None:
            raise ValueError("current opaque native instruction reference required")
        kind, identifier = match[1], int(match[2])
        expected_key = self.stop_key
        if kind == "stop":
            frame = self.frames.get(identifier)
            if frame is None or identifier not in self.native_frames or frame[1] != -1:
                raise ValueError("stale native stopped-frame reference")
            thread, saved_identity = frame[0], None
        else:
            saved = self.native_code_refs.get(identifier)
            if saved is None or saved[1] != expected_key:
                raise ValueError("stale cooperative native-code reference")
            thread, _, saved_identity = saved
        if expected_key is None:
            raise ValueError("native reference has no current stop")
        self.observe(self.command("status"))
        current = [entry for entry in self.threads if entry["id"] == thread]
        if (self.state != "stopped" or self.stop_key != expected_key or len(current) != 1 or
                "stop_id" not in current[0] or
                (kind == "stop" and (identifier not in self.native_frames or "native_pc" not in current[0])) or
                (kind == "code" and (identifier not in self.native_code_refs or "native_pc" in current[0]))):
            raise ValueError("native reference belongs to a retired or unidentified stop")
        location = current[0]
        byte_offset, instruction_offset = args.get("offset", 0), args.get("instructionOffset", 0)
        resolve = args.get("resolveSymbols", False)
        identity, instructions = None, []
        for first in range(0, count, 32):
            page_count = min(32, count - first)
            # Separate fresh complete observations around EACH <=32-row page.
            # An invisible external resume+same-PC repark has a distinct stop ID.
            self.observe(self.command("status"))
            if self.state != "stopped" or self.stop_key != expected_key:
                raise ValueError("native stop changed before disassembly page")
            page_identity, page = self.native_range_page(location, page_count, byte_offset, instruction_offset + first, resolve)
            self.observe(self.command("status"))
            if (self.state != "stopped" or self.stop_key != expected_key or
                    (kind == "stop" and identifier not in self.native_frames) or
                    (kind == "code" and identifier not in self.native_code_refs)):
                raise ValueError("native stop changed during disassembly page")
            if saved_identity is not None and page_identity[:9] != saved_identity[:9]:
                raise ValueError("native code owner changed after reference qualification")
            if identity is None:
                identity = page_identity
            elif page_identity != identity:
                raise ValueError("native code owner or compilation generation changed between pages")
            instructions.extend(page)
        # Across pages the same real owner must preserve an unbroken decoded
        # region. Prefix/trailing fillers carry no invented or readable address.
        previous_end, normal, trailing, symbol = None, False, False, None
        for row in instructions:
            if row["presentationHint"] == "invalid":
                if normal or (byte_offset == 0 and instruction_offset == 0):
                    trailing = True
                continue
            if trailing:
                # Every page was still authenticated against the same stop and
                # owner above. A hidden row ends public continuity: later pages
                # contribute invalid fillers without bytes/text or invented PC.
                row.clear()
                row.update(address="-1", instruction="<unavailable instruction>", presentationHint="invalid")
                continue
            address = int(row["address"], 16)
            if previous_end is not None and address != previous_end:
                raise ValueError("native page boundaries are not contiguous")
            previous_end, normal = address + len(row["instructionBytes"].split()), True
            if "symbol" in row:
                if symbol is None:
                    symbol = row["symbol"]
                elif row["symbol"] != symbol:
                    raise ValueError("native function display name changed between pages")
        if len(instructions) != count:
            raise ValueError("native paged count differs")
        self.respond(request, {"instructions": instructions})


    def query_wasip1_state(self, selected, module, first, count, *, expected_stop=None):
        if selected not in ("args", "env", "fds", "preopens"):
            raise ValueError("WASIp1 selection must be args, env, fds or preopens")
        if (type(module) is not int or not 0 <= module < 1 << 64 or type(first) is not int or not 0 <= first < 1 << 64
                or type(count) is not int or not 1 <= count <= 64):
            raise ValueError("bounded WASIp1 module and page required")
        self.observe(self.command("status"), notify=False)
        if self.state != "stopped" or not self.threads or any(item.get("native_pc") for item in self.threads):
            raise ValueError("WASIp1 state requires a current cooperative Wasm stop")
        if expected_stop is not None and self.stop_key != expected_stop:
            raise ValueError("WASIp1 scope belongs to a retired stop")
        expected = self.stop_key
        stop_id = self.threads[0].get("stop_id")
        if not stop_id or expected is None:
            raise ValueError("WASIp1 state requires a canonical current stop identifier")
        copied = parse_wasip1_state(self.command(f"info wasip1 {selected} {module} {first} {count}"), selected, module, first, count, stop_id)
        self.observe(self.command("status"), notify=False)
        if self.state != "stopped" or self.stop_key != expected:
            raise ValueError("WASIp1 state retired while copying its environment")
        return copied

    def cooperative_memory_stop(self):
        """Fresh bounded participant cohort; no native address authority."""
        self.observe(self.command("status"), notify=False)
        if (self.state != "stopped" or self.stop_key is None or not self.threads
                or len(self.threads) > 256 or len({t["id"] for t in self.threads}) != len(self.threads)
                or any(not t.get("stop_id") or t.get("native_pc")
                       or not 0 < t["id"] < 1 << 64 or not 0 < t["generation"] < 1 << 64
                       or any(not 0 <= t[k] < 1 << 64 for k in ("module", "function", "offset"))
                       for t in self.threads)):
            raise ValueError("Wasm memory requires a current identified cooperative stop")
        return self.stop_key, min(t["id"] for t in self.threads), self.threads[0]["stop_id"]

    def read_wasm_memory(self, module, memory, address, count):
        """Fresh guest selector, never a native address or cached frame capability."""
        expected, _, _ = self.cooperative_memory_stop()
        data = bytearray()
        # The console formatter copies <=256 guest bytes per request. Retain
        # the original complete cohort across every page and the final copy.
        # A zero-byte request still asks the runtime to check its actual range.
        for first in range(0, max(1, count), 256):
            self.observe(self.command("status"), notify=False)
            if self.state != "stopped" or self.stop_key != expected:
                raise ValueError("Wasm memory stop changed before a page copy")
            size = min(256, count - first)
            reply = self.command(f"memory {module} {memory} {address + first} {size}")
            if (not isinstance(reply, str) or
                    (size == 0 and reply != "memory: <empty>\n") or
                    (size != 0 and re.fullmatch(r"memory:(?: [0-9a-fA-F]{2}){" + str(size) + r"}\n", reply) is None)):
                raise ValueError("malformed or incomplete bounded Wasm memory page")
            copied = bytes.fromhex(reply[7:-1]) if size else b""
            self.observe(self.command("status"), notify=False)
            if self.state != "stopped" or self.stop_key != expected:
                raise ValueError("Wasm memory stop changed during page copy")
            data.extend(copied)
        return bytes(data)

    def write_wasm_memory(self, module, memory, address, data):
        """One complete native commit; no chunked or replayed partial writes."""
        if not data:
            self.read_wasm_memory(module, memory, address, 0)
            return
        _, participant, previous_stop = self.cooperative_memory_stop()
        self.invalidate_stop()  # retire copied values BEFORE the single write
        try:
            packet = self.command(f"set wasm memory {module} {memory} {participant} {address} bytes {data.hex()}")
        except (OSError, ValueError, RuntimeError, ConnectionError) as error:
            raise RuntimeError("Wasm memory write outcome is unconfirmed; inspect memory before retrying: " + str(error)) from error
        match = re.fullmatch(r"wasm-stop ([0-9]{1,20})\nWasm mutation v=3 status=(.{1,120}) applied=([01]) reason=(.{1,120}) "
                             r"runtime=([0-9]{1,20}) module=([0-9]{1,20}) target=memory index=([0-9]{1,10}) "
                             r"element=([0-9]{1,20}) bytes=([0-9]{1,3}) address-bytes=([048])\n", packet)
        if (match is None or any(int(match[k]) >= 1 << 64 for k in (1, 5, 6, 8))
                or int(match[1]) == 0 or int(match[7]) >= 1 << 32
                or (int(match[6]), int(match[7]), int(match[8])) != (module, memory, address)
                or match[2] not in {"available", *_WASM_STATE_UNAVAILABLE}
                or match[4] not in {"none", "global is immutable", "GC member is immutable",
                                   "actual value does not match the canonical destination type",
                                   "original current typed source is unavailable", "reference retention failed; destination retained its old value"}
                or (match[3] == "1") != (match[2] == "available")
                or (match[3] == "1" and (match[4] != "none" or int(match[5]) == 0
                    or int(match[1]) <= previous_stop or int(match[9]) != len(data) or match[10] == "0"))
                or (match[3] == "0" and (int(match[9]) != 0 or match[10] != "0"))):
            raise ValueError("unconfirmed Wasm memory write acknowledgement; inspect memory before retrying")
        if match[3] != "1":
            raise ValueError("Wasm memory write refused: " + match[2] + "; " + match[4])
        # The acknowledged full commit is durable even if another host resumes
        # afterward. No post-commit status/read failure may erase this success
        # or invite a blind retry. Cached labels stay retired until refreshed.

    def paged_wasip1_scope(self, expected, module, selected, first, count):
        """DAP start is a dense ordinal; producer FD cursors are guest numbers.

        Copy detached, read-only DATA under the original complete stop. Native
        handles, paths, resources and mutation authority are never borrowed.
        """
        probe = self.query_wasip1_state(selected, module, 0, 1, expected_stop=expected)
        if not probe["available"]:
            return [{"name": "WASIp1 unavailable", "value": probe["status"], "type": "unavailable",
                     "variablesReference": 0, "presentationHint": {"attributes": ["readOnly"]}}]
        total = probe["total"]
        if (len(probe["variables"]) != min(1, total) or probe["more"] != (total > 1)
                or any(t["module"] == module and t["generation"] != probe["runtimeEpoch"] for t in self.threads)):
            raise ValueError("WASIp1 scope probe differs from the original module or cardinality")
        if first >= total:
            return []
        wanted = min(count, total - first) if count else total - first
        if wanted > 1024:
            raise ValueError("WASIp1 scope exceeds 1024 rows; request an explicit bounded page")
        identity = tuple(probe[k] for k in ("moduleId", "runtimeEpoch", "sharedEnvironment", "total"))
        textual = selected in ("args", "env")
        ordinal = first if textual else 0
        cursor = ordinal
        end = first + wanted
        values = []; copied_bytes = 0; borrows = 1
        while ordinal < end:
            if borrows == 2048:
                raise ValueError("WASIp1 dense descriptor scan budget exceeded")
            limit = min(64, end - ordinal)
            page = self.query_wasip1_state(selected, module, cursor, limit, expected_stop=expected)
            borrows += 1
            if (not page["available"] or tuple(page[k] for k in ("moduleId", "runtimeEpoch", "sharedEnvironment", "total")) != identity):
                raise ValueError("WASIp1 environment changed between scope pages")
            rows = page["variables"]
            next_ordinal = ordinal + len(rows)
            if (not rows or next_ordinal > total or page["next"] <= cursor
                    or page["more"] != (next_ordinal < total)):
                raise ValueError("WASIp1 scope page is incomplete or makes no progress")
            # A legal short producer page can be limited by its 4096-byte text
            # budget. Follow the authenticated continuation until DAP's full
            # requested window is copied; never publish a truncated prefix.
            for index, row in enumerate(rows, ordinal):
                if index >= first:
                    copied_bytes += len(json.dumps(row, ensure_ascii=False, separators=(",", ":")).encode("utf-8")) + 1
                    if copied_bytes > MAX_DAP_MESSAGE - 4096:
                        raise ValueError("WASIp1 scope response exceeds its byte budget; request a smaller page")
                    values.append(row)
            ordinal, cursor = next_ordinal, page["next"]
        return values

    def edit_wasip1_state(self, args):
        module = args.get("moduleId", self.module_id)
        if type(module) is not int or not 0 <= module < 1 << 64:
            raise ValueError("bounded WASIp1 module required")
        operation = args.get("operation")
        def index():
            value = args.get("index")
            if type(value) is not int or not 0 <= value < 4096:
                raise ValueError("bounded WASIp1 argument index required")
            return value
        def text(field, key=False, binary=False):
            # Unicode is explicit UTF-8; valueHex/nameHex preserve opaque bytes.
            # Encode to one canonical hex token, with no console interpolation.
            raw = args.get(field)
            encoded = args.get(field + "Hex")
            if (field in args) == (field + "Hex" in args):
                raise ValueError("supply exactly one of " + field + " or " + field + "Hex")
            if encoded is not None:
                if not isinstance(encoded, str) or len(encoded) > 8192 or re.fullmatch(r"(?:[0-9a-fA-F]{2})*", encoded) is None:
                    raise ValueError("bounded hexadecimal WASIp1 bytes required")
                data = bytes.fromhex(encoded)
            elif isinstance(raw, str):
                data = raw.encode("utf-8")
            else:
                raise ValueError("WASIp1 text must be a string")
            if len(data) > 4096 or (not binary and b"\x00" in data) or (key and (not data or b"=" in data)):
                raise ValueError("invalid WASIp1 byte text")
            return data
        def token(data): return data.hex() if data else "-"
        if operation in ("replaceArgument", "insertArgument"):
            verb = "arg" if operation == "replaceArgument" else "arg-insert"
            spelling = f"set wasip1 {verb} {module} {index()} {token(text('value'))}"
        elif operation == "removeArgument":
            spelling = f"unset wasip1 arg {module} {index()}"
        elif operation == "setEnvironment":
            name, value = text("name", True), text("value")
            if len(name) + 1 + len(value) > 4096:
                raise ValueError("WASIp1 environment entry exceeds 4096 bytes")
            spelling = f"set wasip1 env {module} {token(name)} {token(value)}"
        elif operation == "removeEnvironment":
            spelling = f"unset wasip1 env {module} {token(text('name', True))}"
        elif operation == "createFile":
            spelling = f"set wasip1 file {module} {token(text('value', binary=True))}"
        elif operation in ("duplicateDescriptor", "closeDescriptor"):
            values = [args.get(key) for key in ("descriptor", "expectedBase", "expectedInheriting")]
            if any(type(value) is not int or not 0 <= value < 1 << 64 for value in values) or values[0] > 0x7fffffff:
                raise ValueError("unsigned guest FD and rights masks required")
            verb = "set wasip1 fd-dup" if operation == "duplicateDescriptor" else "unset wasip1 fd"
            spelling = verb + " " + str(module) + " " + " ".join(map(str, values))
        elif operation in ("exportPortableCheckpointGroup", "importPortableCheckpointGroup"):
            path = text("path")
            path.decode("utf-8")
            if not path:
                raise ValueError("nonempty checkpoint file path required")
            if "bindings" in args:
                raise ValueError("group bindings belong to each environment")
            environments = args.get("environments")
            if not isinstance(environments, list) or not 1 <= len(environments) <= 16:
                raise ValueError("1..16 target environments required")
            exporting = operation == "exportPortableCheckpointGroup"
            seen_modules, selectors, text_bytes = set(), [], len(path)
            for row in environments:
                if not isinstance(row, dict) or set(row) - {"moduleId", "bindings"}:
                    raise ValueError("environment needs moduleId and optional import bindings")
                target = row.get("moduleId")
                if type(target) is not int or not 0 <= target < 1 << 64 or target in seen_modules:
                    raise ValueError("distinct unsigned target modules required")
                seen_modules.add(target)
                mappings = row.get("bindings", [])
                if not isinstance(mappings, list) or len(mappings) > 64 or (exporting and mappings):
                    raise ValueError("at most 64 explicit import bindings per environment required")
                seen_resources, words = set(), []
                for item in mappings:
                    if not isinstance(item, dict) or set(item) != {"resource", "descriptor"}:
                        raise ValueError("binding needs resource and target guest descriptor")
                    resource, fd = item["resource"], item["descriptor"]
                    if type(resource) is not int or not 0 <= resource < 65536 or resource in seen_resources or type(fd) is not int or not 0 <= fd <= 0x7fffffff:
                        raise ValueError("invalid or duplicate portable binding")
                    seen_resources.add(resource); words.append(f"{resource}={fd}")
                bindings = ",".join(words)
                text_bytes += len(bindings)
                if text_bytes > 4096:
                    raise ValueError("group path and bindings exceed text quota")
                selectors.append(str(target) + (":" + bindings if bindings else ""))
            module = environments[0]["moduleId"]
            if "moduleId" in args and args["moduleId"] != module:
                raise ValueError("group moduleId must equal its first target")
            noun = "export-group" if exporting else "import-group"
            spelling = f"set wasip1 {noun} {token(path)} " + " ".join(selectors)
        elif operation in ("exportPortableCheckpoint", "importPortableCheckpoint"):
            path = text("path")
            path.decode("utf-8")  # Native checkpoint filenames have one UTF-8 interpretation.
            if not path:
                raise ValueError("nonempty checkpoint file path required")
            verb = "export" if operation == "exportPortableCheckpoint" else "import"
            spelling = f"set wasip1 {verb} {module} {token(path)}"
            mappings = args.get("bindings", [])
            if not isinstance(mappings, list) or len(mappings) > 64 or (verb == "export" and mappings):
                raise ValueError("at most 64 explicit import bindings required")
            seen, words = set(), []
            for item in mappings:
                if not isinstance(item, dict) or set(item) != {"resource", "descriptor"}:
                    raise ValueError("binding needs resource and target guest descriptor")
                resource, fd = item["resource"], item["descriptor"]
                if type(resource) is not int or not 0 <= resource < 65536 or resource in seen or type(fd) is not int or not 0 <= fd <= 0x7fffffff:
                    raise ValueError("invalid or duplicate portable binding")
                seen.add(resource); words.append(f"{resource}={fd}")
            if words:
                spelling += " " + ",".join(words)
        elif operation in ("saveCheckpoint", "restoreCheckpoint", "dropCheckpoint"):
            slot = args.get("slot")
            if type(slot) is not int or not 0 <= slot < 8:
                raise ValueError("WASIp1 checkpoint slot must be 0..7")
            verb = "unset" if operation == "dropCheckpoint" else "set"
            noun = "restore" if operation == "restoreCheckpoint" else "checkpoint"
            spelling = f"{verb} wasip1 {noun} {module} {slot}"
            if operation == "restoreCheckpoint":
                mode = args.get("resourceMode")
                if mode not in ("bindings", "strict"):
                    raise ValueError("explicit WASIp1 resourceMode bindings or strict required")
                spelling += " " + mode
        elif operation == "reduceRights":
            values = [args.get(key) for key in ("descriptor", "expectedBase", "expectedInheriting", "newBase", "newInheriting")]
            if any(type(value) is not int or not 0 <= value < 1 << 64 for value in values) or values[0] > 0x7fffffff:
                raise ValueError("unsigned guest FD and rights masks required")
            spelling = "set wasip1 rights " + str(module) + " " + " ".join(map(str, values))
        else:
            raise ValueError("unknown WASIp1 edit operation")
        # Account for the longest bounded stop token before any transport.
        if len(spelling.encode("utf-8")) + len(" if-stop ") + 20 > MAX_COMMAND:
            raise ValueError("WASIp1 edit exceeds the bounded command size")
        self.observe(self.command("status"), notify=False)
        if self.state != "stopped" or not self.threads or any(item.get("native_pc") for item in self.threads):
            raise ValueError("WASIp1 edit requires a current cooperative Wasm stop")
        expected, stop_id = self.stop_key, self.threads[0].get("stop_id")
        if not stop_id or expected is None or ("stopId" in args and (type(args["stopId"]) is not int or args["stopId"] != stop_id)):
            raise ValueError("WASIp1 edit belongs to a retired stop")
        self.invalidate_stop()  # before any write, including a refused mutation
        try:
            packet = self.command(spelling + f" if-stop {stop_id}")
        except (OSError, ValueError, RuntimeError, ConnectionError) as error:
            raise RuntimeError("WASIp1 edit outcome is unconfirmed; inspect state before retrying") from error
        match = re.match(r"wasip1-stop ([0-9]{1,20})\nwasip1 module=([0-9]{1,20}) status=(.+) epoch=([0-9]{1,20}) applied=([01]) shared-environment=([01]) total=([0-9]{1,20})\n", packet)
        if (match is None or int(match[1]) != stop_id or int(match[2]) != module or match[3] not in _WASIP1_STATUS
                or int(match[4]) >= 1 << 64 or int(match[7]) > 65536 or ((match[5] == "1") != (match[3] == "ok"))):
            raise ValueError("malformed WASIp1 edit acknowledgement; inspect state before retrying")
        # Every edit commits on ok. Refusals must be nonapplied; accepting
        # ok/applied=0 would turn an unconfirmed mutation into an available result.
        applied = match[5] == "1"
        extra = packet[match.end():]
        details = {}
        if operation in ("createFile", "duplicateDescriptor", "closeDescriptor") and applied:
            fd = re.fullmatch(r"wasip1-fd affected=([0-9]{1,10})\n", extra)
            if fd is None or int(fd[1]) > 0x7fffffff:
                raise ValueError("malformed WASIp1 FD acknowledgement")
            details["descriptor"] = int(fd[1])
        elif operation in ("exportPortableCheckpointGroup", "importPortableCheckpointGroup") and (extra or match[3] == "ok"):
            cp = re.fullmatch(r"wasip1-portable-group environments=([0-9]{1,2}) resources=([0-9]{1,5}) format=1 content=external atomic=true\n"
                r"reminder: checkpoint Wasm and WASIp1 together at the same cooperative stop; WASIp1-only restore does not restore Wasm state.\n"
                r'(?:diagnostic="((?:[!#-\[\]-~]|\\x[0-9a-f]{2})*)"\n)?', extra)
            if (cp is None or int(cp[1]) != len(environments) or int(cp[2]) > 65536 or int(cp[2]) != int(match[7])
                    or (cp[3] and len(cp[3]) > 16384)):
                raise ValueError("malformed portable group checkpoint acknowledgement")
            details = {"environmentCount": int(cp[1]), "resources": int(cp[2]), "formatVersion": 1, "contentMode": "external",
                "groupAtomic": True, "externalIORollback": False, "wasmCheckpointRequired": True, "diagnostic": cp[3] or ""}
        elif operation in ("exportPortableCheckpoint", "importPortableCheckpoint") and (extra or match[3] == "ok"):
            cp = re.fullmatch(r"wasip1-portable resources=([0-9]{1,5}) format=1 content=external\n"
                r"reminder: checkpoint Wasm and WASIp1 together at the same cooperative stop; WASIp1-only restore does not restore Wasm state.\n"
                r'(?:diagnostic="((?:[!#-\[\]-~]|\\x[0-9a-f]{2})*)"\n)?', extra)
            if cp is None or int(cp[1]) > 65536 or int(cp[1]) != int(match[7]):
                raise ValueError("malformed portable checkpoint acknowledgement")
            details = {"resources": int(cp[1]), "formatVersion": 1, "contentMode": "external", "externalIORollback": False,
                "wasmCheckpointRequired": True, "diagnostic": cp[2] or ""}
        elif operation in ("saveCheckpoint", "restoreCheckpoint", "dropCheckpoint") and (extra or match[3] == "ok"):
            cp = re.fullmatch(r"wasip1-checkpoint slot=([0-7]) managed=([0-9]{1,5}) retained-external=([0-9]{1,5}) external-io-rollback=false\n"
                r"reminder: checkpoint Wasm and WASIp1 together at the same cooperative stop; WASIp1-only restore does not restore Wasm state.\n", extra)
            # A committed capsule always carries a recorded runtime epoch.
            if (cp is None or int(cp[1]) != slot or int(cp[2]) + int(cp[3]) > 65536
                    or (match[3] == "ok" and int(match[4]) == 0)):
                raise ValueError("malformed WASIp1 checkpoint acknowledgement")
            details = {"slot": slot, "managedResources": int(cp[2]), "retainedExternalResources": int(cp[3]),
                "externalIORollback": False, "wasmCheckpointRequired": True}
        elif extra:
            raise ValueError("unexpected WASIp1 edit acknowledgement")
        # The complete acknowledgement already determines the operation's
        # outcome. This optional observation cannot undo a confirmed commit or
        # turn a confirmed refusal into an unknown outcome. Never replay it.
        observed = True
        try:
            refreshed = self.command("status")
            snapshot = self._decode_status(refreshed)
            lines = refreshed.splitlines()
            if snapshot[0] == "stopped":
                entries = snapshot[2]
                if (not lines or not lines[0].startswith("stopped: ")
                        or sum(line.startswith("stopped: ") for line in lines) != 1
                        or not entries or len(entries) > 256
                        or len({t["id"] for t in entries}) != len(entries)
                        or any(not t.get("stop_id") or not 0 < t["id"] < 1 << 64
                               or not 0 < t["generation"] < 1 << 64
                               or any(not 0 <= t[k] < 1 << 64 for k in ("module", "function", "offset"))
                               for t in entries)):
                    raise ValueError("incomplete post-edit stopped observation")
            elif (refreshed not in ("running\n", "debug domain closed\n")
                  and not (snapshot[0] == "exited" and EXIT_RE.fullmatch(refreshed.rstrip("\n")))):
                raise ValueError("unrecognized post-edit observation")
            self._commit_status(snapshot, notify=False)
        except (OSError, ValueError, RuntimeError, ConnectionError):
            self.invalidate_stop()
            observed = False
        current = (observed and self.state == "stopped" and self.stop_key == expected
                   and not any(item.get("native_pc") for item in self.threads))
        if not observed:
            details["stopObservationDiagnostic"] = "post-edit status unavailable; operation outcome is confirmed; refresh state before further debugging"
        return {"moduleId": module, "stopId": stop_id, "status": match[3], "applied": applied,
                "available": match[3] == "ok", "stopCurrent": current, "stopObservationAvailable": observed, "runtimeEpoch": int(match[4]),
                "sharedEnvironment": match[6] == "1", "total": int(match[7]), **details}

    def query_wasm_state(self, thread, module, frame, selected, first=0, count=64, table=0, *, expected_stop=None):
        if (selected not in (*_WASM_VALUE_SELECTIONS, *_WASM_LAYOUT_SELECTIONS) or
                type(module) is not int or not 0 <= module < 1 << 64 or type(frame) is not int or not 0 <= frame < (1 << 64)
                or type(first) is not int or not 0 <= first < 1 << 64 or type(count) is not int or not 1 <= count <= 64
                or type(table) is not int or not 0 <= table < 1 << 64):
            raise ValueError("bounded Wasm state indices required")
        self.observe(self.command("status"), notify=False)
        expected_key = self.stop_key
        current = [entry for entry in self.threads if entry["id"] == thread]
        # Logical root queries acquire the current stop after refreshing status.
        # Cached scopes/objects supply their original stop and cannot rebind.
        if (self.state != "stopped" or expected_key is None or len(current) != 1
                or (expected_stop is not None and expected_key != expected_stop)
                or "native_pc" in current[0] or "stop_id" not in current[0]):
            raise ValueError("Wasm state requires a current identified cooperative stop")
        spelling = (f"locals wasm {thread} {frame} {first} {count}" if selected == "locals" else
                    f"operands {thread} {frame} {first} {count}" if selected == "operands" else
                    f"saved {thread} {frame} {first} {count}" if selected == "saved" else
                    f"{selected} {thread} {frame} {first} {count}" if selected in ("controls", "handlers") else
                    f"{selected} {thread} {frame} {table} {first} {count}" if selected in ("control-params", "control-results", "handler-params") else
                    f"globals {thread} {module} {first} {count}" if selected == "globals" else
                    f"table {thread} {module} {table} {first} {count}")
        packet = self.command(spelling)
        copied = (parse_wasm_layout(packet, selected, thread, module, first, current[0]["stop_id"], table, count)
                  if selected in _WASM_LAYOUT_SELECTIONS else parse_wasm_state(packet, selected, thread, module, first, current[0]["stop_id"], table))
        self.observe(self.command("status"), notify=False)
        if self.state != "stopped" or self.stop_key != expected_key:
            raise ValueError("Wasm state stop retired during copy")
        return expected_key, copied

    def paged_wasm_scope(self, expected_key, selection, first, count):
        # The scope keeps the original participant/module/frame/selector. Probe
        # cardinality under a fresh real borrow, then copy original indices in
        # bounded pages. Never publish a prefix across an epoch or stop change.
        thread, module, frame, selected, table = selection
        _, probe = self.query_wasm_state(thread, module, frame, selected, 0, 1, table, expected_stop=expected_key)
        if "unavailable" in probe:
            return [{"name": "<Wasm state unavailable>", "type": "unavailable", "value": probe["unavailable"],
                     "variablesReference": 0, "presentationHint": {"attributes": ["readOnly"]}}]
        total = probe["total"]
        location = next(entry for entry in self.threads if entry["id"] == thread)
        if (probe["module"] != module or (module == location["module"] and probe["epoch"] != location["generation"])
                or len(probe["rows"]) != min(1, total)):
            raise ValueError("Wasm scope cardinality probe differs from the original activation")
        remaining = max(0, total - first)
        wanted = remaining if count == 0 else min(count, remaining)
        if wanted > 1024:
            raise ValueError("more than 1024 remaining Wasm scope entries; request an explicit bounded page")
        result = []
        end = first + wanted
        while first < end:
            copied_count = min(64, end - first)
            if first == 0 and copied_count == 1:
                copied = probe
            else:
                _, copied = self.query_wasm_state(thread, module, frame, selected, first, copied_count, table,
                                                 expected_stop=expected_key)
            if ("unavailable" in copied or copied["total"] != total or copied["module"] != probe["module"]
                    or copied["epoch"] != probe["epoch"] or copied.get("snapshotNote") != probe.get("snapshotNote")
                    or len(copied["rows"]) != copied_count):
                raise ValueError("Wasm scope page changed or could not be filled; request a smaller page")
            result.extend(self.wasm_variables(copied["rows"], copied["objects"], expected_key, selection))
            first += copied_count
        return result

    def paged_wasm_locals(self, thread, expected_key, first, count):
        # The legacy locals formatter is a bounded diagnostic snapshot. Obtain
        # cardinality from the typed producer, then borrow the actual requested
        # local indices; slicing that diagnostic prefix cannot reach deep locals.
        _, probe = self.query_wasm_state(thread, 0, 0, "locals", 0, 1, expected_stop=expected_key)
        if "unavailable" in probe:
            return [{"name": "<Wasm locals unavailable>", "type": "unavailable", "value": probe["unavailable"],
                     "variablesReference": 0, "presentationHint": {"attributes": ["readOnly"]}}]
        total = probe["total"]
        location = next(entry for entry in self.threads if entry["id"] == thread)
        if (probe["module"] != location["module"] or probe["epoch"] != location["generation"]
                or len(probe["rows"]) != min(1, total)):
            raise ValueError("Wasm locals cardinality probe differs from the current activation")
        remaining = max(0, total - first)
        wanted = remaining if count == 0 else min(count, remaining)
        if wanted > 1024:
            raise ValueError("more than 1024 remaining Wasm locals; request an explicit bounded page")
        result = []
        end = first + wanted
        while first < end:
            copied_count = min(64, end - first)
            if first == 0 and copied_count == 1:
                copied = probe
            else:
                _, copied = self.query_wasm_state(thread, probe["module"], 0, "locals", first, copied_count, expected_stop=expected_key)
            if ("unavailable" in copied or copied["total"] != total or copied["module"] != probe["module"]
                    or copied["epoch"] != probe["epoch"] or len(copied["rows"]) != copied_count):
                raise ValueError("Wasm locals page changed or could not be filled; request a smaller page")
            values = self.wasm_variables(copied["rows"], copied["objects"], expected_key,
                                         (thread, copied["module"], 0, "locals", 0))
            for value in values:
                # Preserve the ordinary scope's familiar integer/float spelling
                # while retaining exact typed reference/GC expansion metadata.
                separator = "=" if value["type"] in ("i32", "i64") else " " if value["type"] in ("f32", "f64", "v128") else " = "
                value["value"] = value["type"] + separator + value["value"]
            result.extend(values)
            first += copied_count
        return result

    def query_deep_wasm_members(self, thread, module, frame, selected, table, root, path, first, count, expected_key, stop_id):
        # The cache stores original selectors+paths and scalar DATA labels only.
        # Backend rewalks the entire real root under fresh native proof EVERY
        # create/extend/page; no reply object ID or host token is an input.
        locus = (thread, module, frame, selected, table, root)
        anchors = [(anchor, token) for anchor, token in self.deep_wasm_paths.items()
                   if anchor[:7] == (expected_key, *locus) and len(anchor[7]) <= len(path)
                   and tuple(path[:len(anchor[7])]) == anchor[7]]
        previous = max(anchors, key=lambda item: len(item[0][7])) if anchors else None
        depth = len(previous[0][7]) if previous else 0
        token = previous[1] if previous else None
        while depth < len(path) or token is None:
            part = tuple(path[depth:depth + 16])
            if token is None:
                spelling = f"path create {selected} {thread} {module} {frame} {table} {root}"
            else:
                spelling = f"path extend {token['session']} {token['handle']}"
            if part: spelling += " " + " ".join(map(str, part))
            data, _ = parse_wasm_path(self.command(spelling), stop_id, depth + len(part),
                                      expected_session=token["session"] if token else None)
            self.observe(self.command("status"), notify=False)
            if self.state != "stopped" or self.stop_key != expected_key:
                raise ValueError("Wasm path stop retired during complete root reborrow")
            if "unavailable" in data:
                return {**data, "rows": [], "objects": [], "total": 0, "truncated": False}
            if token is not None:
                # Extension replaces the parent with a NEW immutable path label;
                # stale aliases of the retired predecessor cannot revive it.
                self.deep_wasm_paths = {anchor: old for anchor, old in self.deep_wasm_paths.items()
                    if (old["session"], old["handle"]) != (token["session"], token["handle"])}
            depth += len(part); token = data
            if len(self.deep_wasm_paths) == 128:
                raise ValueError("bounded Wasm path DATA cache exhausted")
            self.deep_wasm_paths[(expected_key, *locus, tuple(path[:depth]))] = token
        spelling = f"path members {token['session']} {token['handle']} {first} {count}"
        data, packet = parse_wasm_path(self.command(spelling), stop_id, len(path),
            expected_session=token["session"], expected_handle=token["handle"], members=True)
        if "unavailable" in data:
            return {**data, "rows": [], "objects": [], "total": 0, "truncated": False}
        return parse_wasm_state(packet, selected, thread, module, root, stop_id, table,
            member_first=first, member_count=count, path=path,
            path_token=(data["session"], data["handle"], data["depth"]))

    def query_wasm_members(self, thread, module, frame, selected, table, root, path, first, count, *, expected_stop=None):
        unsigned_id(thread, "Wasm member thread")
        if (selected not in _WASM_VALUE_SELECTIONS
                or type(module) is not int or not 0 <= module < 1 << 64
                or type(frame) is not int or not 0 <= frame < (1 << 64)
                or type(table) is not int or not 0 <= table < 1 << 64 or (selected != "table" and table != 0)
                or (selected in ("globals", "table") and frame != 0)
                or type(root) is not int or not 0 <= root < 1 << 64
                or type(first) is not int or not 0 <= first < 1 << 64
                or type(count) is not int or not 1 <= count <= 64
                or not isinstance(path, (list, tuple)) or len(path) > 4096
                or any(type(index) is not int or not 0 <= index < 1 << 64 for index in path)):
            raise ValueError("bounded original Wasm root, path and member page required")
        self.observe(self.command("status"), notify=False)
        expected_key = self.stop_key
        current = [entry for entry in self.threads if entry["id"] == thread]
        # Logical root queries acquire the current stop after refreshing status.
        # Cached scopes/objects supply their original stop and cannot rebind.
        if (self.state != "stopped" or expected_key is None or len(current) != 1
                or (expected_stop is not None and expected_key != expected_stop)
                or "native_pc" in current[0] or "stop_id" not in current[0]):
            raise ValueError("Wasm members require a current identified cooperative stop")
        if len(path) > 16:
            copied = self.query_deep_wasm_members(thread, module, frame, selected, table, root, path, first, count,
                expected_key, current[0]["stop_id"])
        else:
            spelling = f"members {selected} {thread} {module} {frame} {table} {root} {first} {count}"
            if path: spelling += " " + " ".join(map(str, path))
            copied = parse_wasm_state(self.command(spelling), selected, thread, module, root, current[0]["stop_id"], table,
                                      member_first=first, member_count=count, path=path)
        self.observe(self.command("status"), notify=False)
        if self.state != "stopped" or self.stop_key != expected_key:
            raise ValueError("Wasm member stop retired during copy")
        return expected_key, copied

    def paged_wasm_members(self, expected_key, displayed, anchor, first, count):
        # Cardinality is copied from a fresh complete root/path reborrow. The
        # cached graph is display DATA, never a member buffer or object token.
        thread, module, frame, selected, table, root, path = anchor
        _, probe = self.query_wasm_members(thread, module, frame, selected, table, root, path, 0, 1,
                                           expected_stop=expected_key)
        if "unavailable" in probe:
            return [{"name": "<Wasm members unavailable>", "type": "unavailable", "value": probe["unavailable"],
                     "variablesReference": 0}]
        target = probe["objects"][probe["selected_object"] - 1]
        identity = (target["kind"], target["label"], target["total"])
        if (probe["module"] != module or identity != (displayed["kind"], displayed["label"], displayed["total"])
                or len(target["members"]) != min(1, target["total"])):
            raise ValueError("Wasm member cardinality probe differs from the original object display")
        remaining = max(0, target["total"] - first)
        wanted = remaining if count == 0 else min(count, remaining)
        if wanted > 1024:
            raise ValueError("more than 1024 remaining Wasm members; request an explicit bounded page")
        result = []
        end = first + wanted
        while first < end:
            copied_count = min(64, end - first)
            if first == 0 and copied_count == 1:
                copied = probe
            else:
                _, copied = self.query_wasm_members(thread, module, frame, selected, table, root, path, first, copied_count,
                                                    expected_stop=expected_key)
            if "unavailable" in copied or (copied["module"], copied["epoch"]) != (probe["module"], probe["epoch"]):
                raise ValueError("Wasm members changed or became unavailable during pagination")
            page = copied["objects"][copied["selected_object"] - 1]
            if (page["kind"], page["label"], page["total"]) != identity or len(page["members"]) != copied_count:
                raise ValueError("Wasm member page changed or could not be filled; request a smaller page")
            result.extend(self.wasm_variables(page["members"], copied["objects"], expected_key,
                (thread, copied["module"], frame, selected, table), (root, path)))
            first += copied_count
        return result

    def wasm_variables(self, values, graph, expected_key, locus, parent=None):
        # Session IDs retain original root/path display metadata, never dense
        # reply object ID authority. Every expansion reborrows that exact root
        # through the current canonical cohort + hostclose + N GC lease scope.
        if len(values) > 4096 - len(self.wasm_object_views):
            raise ValueError("bounded Wasm graph reference space exhausted")
        object_map = {obj["id"]: obj for obj in graph}
        result = []
        for value in values:
            ref = 0
            anchor = (*locus, value["index"], ()) if parent is None else (*locus, parent[0], (*parent[1], value["index"]))
            if value["object"] is not None:
                obj = object_map[value["object"][0]]
                if obj["kind"] != "opaque host-reference" and obj["total"] != 0 and len(anchor[-1]) <= 4096:
                    if len(anchor[-1]) > 32768 - self.wasm_path_indices:
                        raise ValueError("bounded original Wasm path DATA budget exhausted")
                    ref = self.new_reference(); self.wasm_path_indices += len(anchor[-1])
                    self.wasm_object_views[ref] = (expected_key, obj, anchor)
            item = {"name": value["name"], "type": value["type"], "value": value["value"],
                    "variablesReference": ref, "presentationHint": {"attributes": ["readOnly"]}}
            if ref and obj["total"] < 1 << 31:
                # DAP counts are int32 display hints, never a root, path or
                # memory capability. Larger Wasm cardinalities stay unclamped.
                item.update(indexedVariables=obj["total"], namedVariables=0)
            result.append(item)
        return result

    def handle(self, request):
        command = request.get("command")
        args = request.get("arguments") or {}
        if not isinstance(command, str) or not isinstance(args, dict):
            self.invalidate_stop()
            self.respond(request, error="invalid DAP request")
            return
        try:
            if command == "initialize":
                if any(type(args.get(key, False)) is not bool for key in ("supportsMemoryEvent", "supportsInvalidatedEvent")):
                    raise ValueError("memory and invalidation event capabilities must be boolean")
                if type(args.get("supportsVariableType", False)) is not bool:
                    raise ValueError("variable type capability must be boolean")
                self.supports_memory_event = args.get("supportsMemoryEvent", False)
                self.supports_invalidated_event = args.get("supportsInvalidatedEvent", False)
                self.supports_variable_type = args.get("supportsVariableType", False)
                self.respond(request, {"supportsConfigurationDoneRequest": True,
                                       "supportsEvaluateForHovers": True,
                                       "supportsSteppingGranularity": True,
                                       "supportsInstructionBreakpoints": True,
                                       "supportsHitConditionalBreakpoints": True,
                                       "supportsConditionalBreakpoints": True,
                                       "supportsLogPoints": False,
                                       "supportsReadMemoryRequest": True,
                                       "supportsWriteMemoryRequest": True,
                                       "supportsDisassembleRequest": True,
                                       "supportsTerminateRequest": False})
            elif command == "attach":
                if self.broker is not None:
                    raise ValueError("already attached")
                step_level = args.get("stepLevel", "source")
                if step_level not in ("source", "wasm", "native"):
                    raise ValueError("stepLevel must be source, wasm or native")
                module_id = args.get("moduleId", 0)
                if type(module_id) is not int or not 0 <= module_id < (1 << 32):
                    raise ValueError("moduleId out of range")
                if sys.platform == "win32":
                    if not isinstance(args.get("pipeName"), str) or not isinstance(args.get("capability"), str):
                        raise ValueError("Windows broker pipe and capability are required")
                    broker = WindowsBroker(args["pipeName"], args["capability"])
                elif sys.platform in ("linux", "darwin"):
                    if not isinstance(args.get("socketDir"), str):
                        raise ValueError("Unix broker directory is required")
                    broker = UnixBroker(args["socketDir"])
                else:
                    raise ValueError("host platform lacks a qualified debug broker")
                try:
                    initial = broker.request("status")
                    if isinstance(initial, bytes):
                        initial = initial.decode("utf-8", "replace")
                    if initial.startswith("error: "):
                        raise RuntimeError(initial.strip())
                    observation = self._decode_status(initial)
                except BaseException:
                    broker.close()
                    raise
                self.broker = broker
                self.module_id = module_id
                self.step_level = step_level
                self.respond(request)
                self.event("initialized")
                self._commit_status(observation)
            elif command == "configurationDone":
                self.respond(request)
            elif command == "threads":
                self.observe(self.command("status"))
                self.respond(request, {"threads": [{"id": t["id"], "name": f"Wasm thread {t['id']}"}
                                                    for t in self.threads] or
                                                   ([{"id": 1, "name": "Wasm thread 1"}] if self.state != "exited" else [])})
            elif command == "pause":
                reply = self.command("pause")
                observation = self._decode_status(reply)
                self.respond(request)
                self._commit_status(observation)
            elif command == "continue":
                thread = unsigned_id(args.get("threadId", 1), "Wasm threadId")
                reply = self.command("continue")
                observation = self._decode_status(reply)
                self.respond(request, {"allThreadsContinued": True})
                self.event("continued", {"threadId": thread, "allThreadsContinued": True})
                self._commit_status(observation)
            elif command in ("stepIn", "next", "stepOut"):
                thread = unsigned_id(args.get("threadId"), "Wasm threadId")
                granularity = args.get("granularity", "statement")
                # Source is the default IDE level: ordinary next/stepOut must
                # reach the real source policy instead of failing statement mode.
                # An unavailable source scope is an explicit controller error;
                # never call a Wasm step and describe it as source success.
                if self.step_level == "wasm":
                    if granularity not in ("statement", "instruction"):
                        raise ValueError("Wasm stepping requires statement or instruction granularity")
                    policy = "" if command == "stepIn" else " over" if command == "next" else " out"
                    native = f"step wasm {thread}{policy}"
                elif granularity == "line" or granularity == "statement" and self.step_level == "source":
                    policy = "into" if command == "stepIn" else "over" if command == "next" else "out"
                    native = f"step source {thread} {policy}"
                elif (granularity == "instruction" or
                      granularity == "statement" and self.step_level == "native") and command in ("stepIn", "next", "stepOut"):
                    # The controller admits NI only at its real top native trap.
                    # It alone proves Wasm owner/call continuation and owns cancel
                    # and ACK; no DAP PC, frame label or Wasm/source fallback does.
                    native = (f"step asm {thread}" if command == "stepIn" else
                              f"ni {thread}" if command == "next" else f"finish asm {thread}")
                else:
                    raise ValueError("this step kind has no equivalent at the selected level/granularity")
                # An execution request may move the guest before a reply is
                # lost/canceled or repeats a display stop. Retire all old value,
                # frame and code references BEFORE contacting the broker.
                self.invalidate_stop()
                reply = self.command(native)
                # Decode before sending success or movement events. A rejected
                # snapshot gets exactly one failure response, never success
                # followed by a second response for the same request sequence.
                observation = self._decode_status(reply)
                self.respond(request)
                if reply.startswith(("native instruction ", "native finish ", "pause/step timed out;")):
                    # A bounded native/source/Wasm step may end at a real pause
                    # without completing its requested movement. Preserve the
                    # controller's diagnostic instead of silently discarding it.
                    self.event("output", {"category": "console", "output": reply.splitlines()[0] + "\n"})
                self._commit_status(observation)
            elif command == "stackTrace":
                thread = unsigned_id(args.get("threadId"), "Wasm threadId")
                start, levels = args.get("startFrame", 0), args.get("levels", 0)
                if type(start) is not int or not 0 <= start < (1 << 64) or type(levels) is not int or not 0 <= levels < (1 << 64):
                    raise ValueError("valid stack frame page required")
                page_count = min(levels or 128, 128)
                total_frames = None
                # A backtrace reply contains only the requested participant.
                # Refresh the complete stop first so it cannot replace the
                # multi-thread stop key or bind another participant's PC.
                changed = self.observe(self.command("status"), notify=False)
                current = [entry for entry in self.threads if entry["id"] == thread]
                if self.state != "stopped" or len(current) != 1:
                    raise ValueError("selected thread has no complete stopped location")
                trace = self.command(f"bt {thread}")
                state, reason, entries, _ = parse_status(trace)
                if state != "stopped" or len(entries) != 1 or entries[0] != current[0]:
                    raise ValueError("backtrace does not match the selected stopped thread")
                source = next((t.get("source") for t in entries if t["id"] == thread), None)
                frames = []
                location = entries[0]
                language_rows = []
                if self.step_level == "wasm" and "native_pc" not in location and "stop_id" in location:
                    expected_key = self.stop_key
                    try:
                        page = parse_source_frames(self.command(f"frames wasm {thread} {location['stop_id']} {start} {page_count}"),
                                                   thread, location["stop_id"], with_page=True, wasm=True)
                    except RuntimeError as error:
                        self.observe(self.command("status"), notify=False)
                        if self.state != "stopped" or self.stop_key != expected_key:
                            raise ValueError("Wasm frame stop changed during query")
                        # A diagnostic bt window cannot establish the size or
                        # typed ordinals of the complete activation chain.
                        raise ValueError("complete Wasm activation page unavailable: " + str(error)) from error
                    else:
                        if page["first"] != min(start, page["total"]) or len(page["rows"]) != min(page_count, page["total"] - page["first"]):
                            raise ValueError("Wasm frame page differs from requested window")
                        self.observe(self.command("status"), notify=False)
                        if self.state != "stopped" or self.stop_key != expected_key:
                            raise ValueError("Wasm frame stop changed during query")
                        for row in page["rows"]:
                            if row["ordinal"] == 0 and (row["module"], row["function"], row["runtime_epoch"]) != (location["module"], location["function"], location["generation"]):
                                raise ValueError("current Wasm activation differs from stopped location")
                            frame_id = self.new_reference()
                            self.frames[frame_id] = (thread, row["ordinal"], row["module"], row["function"])
                            frame = {"id": frame_id, "name": f"Wasm frame {row['ordinal']} module={row['module']} function={row['function']}",
                                     "line": 0, "column": 0, "moduleId": str(row["module"]), "canRestart": False}
                            if row["ordinal"] == 0:
                                frame["instructionPointerReference"] = f"wasm:{location['module']}:{location['function']}:{location['offset']}"
                            frames.append(frame)
                        self.respond(request, {"stackFrames": frames, "totalFrames": page["total"]})
                        if changed:
                            self.event("stopped", {"reason": reason, "threadId": thread, "allThreadsStopped": True})
                        return
                # Source line tables may deliberately mark an instruction line 0.
                # The authenticated activation/DIE page, not a display line,
                # establishes whether source variables belong to this real stop.
                if "native_pc" not in location and "stop_id" in location:
                    expected_key = self.stop_key
                    try:
                        frame_command = f"frames {thread} {location['stop_id']}" if start == 0 else f"frames {thread} {location['stop_id']} {start} {page_count}"
                        page = parse_source_frames(self.command(frame_command), thread, location["stop_id"], with_page=True)
                        if page["first"] != min(start, page["total"]):
                            raise ValueError("source frame page differs from requested window")
                        # Reobserve the whole participant cohort after copying a
                        # source page, before publishing any opaque frame IDs.
                        self.observe(self.command("status"), notify=False)
                        if self.state != "stopped" or self.stop_key != expected_key:
                            raise ValueError("source frame stop changed during query")
                        language_rows, total_frames = page["rows"][:page_count], page["total"]
                    except RuntimeError:
                        # Metadata failure leaves Wasm backtrace labels usable;
                        # reobserve the COMPLETE real stop after command() retired
                        # its opaque references. No source variable IDs survive.
                        self.observe(self.command("status"), notify=False)
                        if self.state != "stopped" or self.stop_key != expected_key:
                            raise ValueError("source frame stop changed while metadata was unavailable")
                    for row in language_rows:
                        if row["current"] and row["kind"] != "caller" and (row["module"], row["function"], row["runtime_epoch"]) != (location["module"], location["function"], location["generation"]):
                            raise ValueError("current inline/physical frame differs from actual source activation")
                        frame_id = self.new_reference()
                        index = 0 if row["kind"] == "physical" else -2 if row["kind"] == "inline" else -3
                        self.frames[frame_id] = (thread, index, row["module"], row["function"])
                        self.source_frame_ordinals[frame_id] = (expected_key, row)
                        if row["current"]:
                            self.source_frames.add(frame_id)
                        if row["kind"] == "inline":
                            self.inline_frames.add(frame_id)
                        frame = {"id": frame_id, "name": row["name"] or "<anonymous>", "line": 0, "column": 0,
                                 "moduleId": str(row["module"]), "canRestart": False}
                        if row["ordinal"] == 0 and source:
                            frame["source"] = {"name": Path(source["path"]).name, "path": source["path"]}
                            frame["line"], frame["column"] = source["line"], source["column"]
                        elif row["kind"] == "caller":
                            availability = "saved caller values" if row["current"] else "caller values unavailable"
                            frame["name"] += f" module={row['module']} function={row['function']} ({availability})"
                            frame["presentationHint"] = "label"
                        if row["kind"] == "physical":
                            # The actual current physical Code PC is shared by
                            # its inline scopes; no older caller PC is invented.
                            frame["instructionPointerReference"] = (
                                f"wasm:{location['module']}:{location['function']}:{location['offset']}")
                            if self.step_level == "native":
                                native_reference = self.qualify_native_code_reference(location)
                                if native_reference is not None:
                                    frame["instructionPointerReference"] = native_reference
                        frames.append(frame)
                inline_labels = parse_inline_labels(trace, thread)
                if "native_pc" in location and inline_labels:
                    raise ValueError("native stop cannot retain inline source metadata")
                if total_frames is None and not language_rows and "native_pc" not in location and "stop_id" in location:
                    for label in inline_labels:
                        frame_id = self.new_reference()
                        self.frames[frame_id] = (thread, -2, location["module"], location["function"])
                        self.inline_frames.add(frame_id)
                        # Metadata labels show the real current physical frame's
                        # inner-to-outer inline chain. A call-site is not a caller
                        # Wasm PC/current line. Labels have no source read, local,
                        # expression, memory, instruction or restart capability.
                        frames.append({"id": frame_id, "name": "Inlined " + label,
                                       "line": 0, "column": 0, "presentationHint": "label",
                                       "moduleId": str(location["module"]), "canRestart": False})
                if "native_pc" in location:
                    if any(FRAME_RE.match(line) for line in trace.splitlines()):
                        raise ValueError("native stop cannot reuse a Wasm caller-frame snapshot")
                    frame_id = self.new_reference()
                    self.frames[frame_id] = (thread, -1, location["module"], location["function"])
                    self.native_frames.add(frame_id)
                    # This is one authenticated instruction-position label, not
                    # a reconstructed caller stack. The original Wasm offset,
                    # DWARF source and locals were invalidated by the native step.
                    # The opaque ID never accepts an address/offset. Only a fresh
                    # complete identified stop may request the bounded runtime
                    # code-copy command; no readMemory/write/breakpoint authority.
                    frames.append({"id": frame_id,
                                   "name": ("Native JIT instruction (Wasm origin "
                                            f"module={location['module']} function={location['function']} "
                                            f"generation={location['generation']})"),
                                   "line": 0, "column": 0, "presentationHint": "label",
                                   "moduleId": str(location["module"]), "canRestart": False,
                                   "instructionPointerReference": f"uwvm-native-stop:{frame_id}"})
                for line in (() if total_frames is not None else trace.splitlines()):
                    match = FRAME_RE.match(line)
                    if not match:
                        continue
                    index, module, function = map(int, match.groups())
                    frame_id = self.new_reference()
                    self.frames[frame_id] = (thread, index, module, function)
                    frame = {"id": frame_id, "name": line.strip(), "line": 1, "column": 1}
                    if index == 0:
                        location = next((t for t in entries if t["id"] == thread), None)
                        if location is not None:
                            frame["instructionPointerReference"] = (
                                f"wasm:{module}:{function}:{location['offset']}")
                    if index == 0 and source:
                        if "stop_id" in location:
                            self.source_frames.add(frame_id)
                        frame["source"] = {"name": Path(source["path"]).name, "path": source["path"]}
                        frame["line"], frame["column"] = source["line"], source["column"]
                    else:
                        frame["source"] = {"name": f"wasm module {module}", "sourceReference": frame_id}
                    if index == 0 and self.step_level == "native" and location is not None:
                        native_reference = self.qualify_native_code_reference(location)
                        if native_reference is not None:
                            frame["instructionPointerReference"] = native_reference
                    frames.append(frame)
                # A failed/private query cannot leave labels whose stop retired
                # while this stack display was being assembled.
                if self.state != "stopped" or any(frame["id"] not in self.frames for frame in frames):
                    raise ValueError("stop changed or native owner qualification was unavailable during stack display")
                if total_frames is None:
                    total_frames = len(frames)
                    frames = frames[start:start + page_count]
                self.respond(request, {"stackFrames": frames, "totalFrames": total_frames})
                if changed:
                    self.event("stopped", {"reason": reason, "threadId": thread,
                                           "allThreadsStopped": True})
            elif command == "disassemble":
                reference = args.get("memoryReference")
                count = args.get("instructionCount")
                if (not isinstance(reference, str) or type(count) is not int or not 1 <= count <= 512
                        or type(args.get("offset", 0)) is not int or not -65536 <= args.get("offset", 0) <= 65536
                        or type(args.get("instructionOffset", 0)) is not int or not -8192 <= args.get("instructionOffset", 0) <= 8192
                        or type(args.get("resolveSymbols", False)) is not bool):
                    raise ValueError("native disassembly requires a current opaque reference, count 1..512 and bounded signed offsets")
                # Preserve the original r2 zero-offset <=32 native-stop wire
                # subset. IDE symbols, offsets, bigger windows and cooperative
                # code references always use the new exact-owner range query.
                if (reference.startswith("uwvm-native-code:") or count > 32 or args.get("offset", 0) != 0 or
                        args.get("instructionOffset", 0) != 0 or args.get("resolveSymbols", False)):
                    self.disassemble_owned_range(request, args, reference, count)
                    return
                if (not isinstance(reference, str) or not re.fullmatch(r"uwvm-native-stop:[1-9][0-9]{0,9}", reference)
                        or type(count) is not int or not 1 <= count <= 32
                        or any(type(args.get(name, 0)) is not int or args.get(name, 0) != 0
                               for name in ("offset", "instructionOffset"))
                        or type(args.get("resolveSymbols", False)) is not bool or args.get("resolveSymbols", False)):
                    raise ValueError("native disassembly requires a current opaque stop reference, count 1..32 and zero offsets; symbol resolution unavailable")
                frame_id = int(reference.split(":", 1)[1])
                frame = self.frames.get(frame_id)
                expected_key = self.stop_key
                if (frame is None or frame_id not in self.native_frames or frame[1] != -1 or expected_key is None):
                    raise ValueError("stale or nonnative instruction reference")
                thread = frame[0]
                cached = [entry for entry in self.threads if entry["id"] == thread]
                if len(cached) != 1 or "stop_id" not in cached[0] or "native_pc" not in cached[0]:
                    raise ValueError("legacy native display label has no identified read request")
                self.observe(self.command("status"))
                current = [entry for entry in self.threads if entry["id"] == thread]
                if (self.state != "stopped" or self.stop_key != expected_key or frame_id not in self.native_frames
                        or len(current) != 1 or "native_pc" not in current[0] or "stop_id" not in current[0]):
                    raise ValueError("native instruction reference belongs to a retired or unidentified stop")
                trace = self.command(f"bt {thread}")
                state, _, entries, _ = parse_status(trace)
                if (state != "stopped" or entries != current or any(FRAME_RE.match(line) for line in trace.splitlines())):
                    raise ValueError("native backtrace does not match the actual stopped instruction")
                location = current[0]
                text = self.command(f"disassemble {thread} {location['stop_id']} {count}")
                instructions = parse_native_disassembly(text, location, count)
                # Another authorized host could resume invisibly. Refresh after
                # the copy as well; an identical PC with a new stop ID is stale.
                self.observe(self.command("status"))
                if self.state != "stopped" or self.stop_key != expected_key or frame_id not in self.native_frames:
                    raise ValueError("native stop changed during disassembly")
                self.respond(request, {"instructions": instructions})
            elif command == "scopes":
                frame = self.current_frame(unsigned_id(args.get("frameId"), "frameId"))
                scopes = []
                if args["frameId"] in self.native_frames:
                    if self.state != "stopped" or self.stop_key is None or frame[1] != -1:
                        raise ValueError("native frame belongs to a retired stop")
                    identified = [row for row in self.threads if row["id"] == frame[0] and "stop_id" in row and "native_pc" in row]
                    if len(identified) == 1:
                        reference = self.new_reference()
                        self.native_register_scopes[reference] = (self.stop_key, args["frameId"])
                        scopes.append({"name": "Registers", "presentationHint": "registers",
                                       "variablesReference": reference, "expensive": False})
                language_frame = self.source_frame_ordinals.get(args["frameId"])
                if language_frame is not None:
                    expected_key, row = language_frame
                    if self.state != "stopped" or self.stop_key != expected_key:
                        raise ValueError("selected source frame belongs to a retired stop")
                    reference = self.new_reference()
                    self.scopes[reference] = frame[0]
                    self.source_scope_stops[reference] = (expected_key, row)
                    self.source_scope_frames[reference] = args["frameId"]
                    scopes.append({"name": "Source variables" if row["current"] else "Caller variables unavailable",
                                   "variablesReference": reference, "expensive": False})
                if frame[1] == 0:
                    reference = self.new_reference()
                    self.scopes[reference] = frame[0]
                    self.wasm_local_scope_stops[reference] = self.stop_key
                    scopes.append({"name": "Wasm locals", "variablesReference": reference, "expensive": False})
                    if args["frameId"] in self.source_frames and language_frame is None:
                        reference = self.new_reference()
                        self.scopes[reference] = frame[0]
                        self.source_scope_stops[reference] = self.stop_key
                        scopes.append({"name": "Source variables", "variablesReference": reference, "expensive": False})
                if self.step_level == "wasm" and frame[1] >= 0:
                    # A source physical row denotes Wasm frame zero; inline or
                    # caller source labels never invent an observed native PC.
                    for title, selected in (("Typed Wasm locals", "locals"), ("Wasm operand stack (last safepoint; may differ from native state)", "operands"), ("Wasm saved if parameters", "saved"),
                                            ("Wasm control stack", "controls"), ("Wasm handler clauses", "handlers"), ("Wasm globals", "globals"), ("Wasm table 0", "table")):
                        if selected == "operands" and self.stop_key is not None and self.stop_key[0] == "exception":
                            if any(t.get("uncaught_exception") for t in self.threads):
                                if any(t["id"] == frame[0] and t.get("uncaught_exception") for t in self.threads):
                                    title = "Wasm uncaught snapshot (before unwind)"
                            else:
                                title = "Wasm pre-trap inputs (no instruction result)"
                        reference = self.new_reference()
                        self.scopes[reference] = frame[0]
                        self.wasm_scope_stops[reference] = (self.stop_key, (frame[0], frame[2], 0 if selected in ("globals", "table") else frame[1], selected, 0))
                        scopes.append({"name": title, "variablesReference": reference, "expensive": True})
                if (frame[1] == 0 and self.state == "stopped" and self.stop_key is not None
                        and self.threads and all(t.get("stop_id") for t in self.threads)
                        and not any(t.get("native_pc") for t in self.threads)):
                    for title, selected in (("WASIp1 arguments", "args"), ("WASIp1 environment", "env"),
                                            ("WASIp1 descriptors", "fds"), ("WASIp1 preopens", "preopens")):
                        reference = self.new_reference()
                        self.wasip1_scope_stops[reference] = (self.stop_key, frame[2], selected)
                        scopes.append({"name": title, "variablesReference": reference, "expensive": True})
                self.respond(request, {"scopes": scopes})
            elif command == "uwvm/wasip1Edit":
                edited = self.edit_wasip1_state(args)
                self.respond(request, edited)
                if edited["applied"] and self.supports_invalidated_event:
                    self.event("invalidated", {"areas": ["stacks", "variables"]})
            elif command == "uwvm/wasip1State":
                self.respond(request, self.query_wasip1_state(args.get("selection"), args.get("moduleId", self.module_id),
                    args.get("start", 0), args.get("count", 64)))
            elif command == "uwvm/wasmState":
                thread = unsigned_id(args.get("threadId"), "Wasm state thread")
                selected = args.get("selection")
                if selected not in (*_WASM_VALUE_SELECTIONS, *_WASM_LAYOUT_SELECTIONS):
                    raise ValueError("unknown bounded Wasm state selection")
                expected_key, copied = self.query_wasm_state(thread, args.get("moduleId", self.module_id),
                    args.get("frame", 0), selected, args.get("start", 0), args.get("count", 64),
                    args.get("index", 0) if selected in ("control-params", "control-results", "handler-params") else args.get("table", 0))
                variables = self.wasm_variables(copied["rows"], copied["objects"], expected_key,
                    (thread, copied.get("module", args.get("moduleId", self.module_id)), 0 if selected in ("globals", "table") else args.get("frame", 0), selected, args.get("table", 0)))
                self.respond(request, {"variables": variables, "total": copied["total"], "truncated": copied["truncated"],
                    "unavailable": copied.get("unavailable"), "moduleId": copied.get("module"), "runtimeEpoch": copied.get("epoch"),
                    "snapshotNote": copied.get("snapshotNote")})
            elif command == "uwvm/wasmMembers":
                thread = unsigned_id(args.get("threadId"), "Wasm member thread")
                module, frame = args.get("moduleId", self.module_id), args.get("frame", 0)
                selected, table, root = args.get("selection"), args.get("table", 0), args.get("root")
                path, first, count = args.get("path", []), args.get("start", 0), args.get("count", 64)
                expected_key, copied = self.query_wasm_members(thread, module, frame, selected, table, root, path, first, count)
                if "unavailable" in copied:
                    self.respond(request, {"variables": [], "unavailable": copied["unavailable"]})
                else:
                    target = copied["objects"][copied["selected_object"] - 1]
                    variables = self.wasm_variables(target["members"], copied["objects"], expected_key,
                        (thread, copied["module"], frame, selected, table), (root, tuple(path)))
                    self.respond(request, {"variables": variables, "total": target["total"], "first": target["first"],
                        "next": target["next"], "more": target["more"], "moduleId": copied["module"], "runtimeEpoch": copied["epoch"]})
            elif command == "variables":
                reference = unsigned_id(args.get("variablesReference"), "variablesReference")
                if reference in self.wasip1_scope_stops:
                    expected_key, module, selected = self.wasip1_scope_stops[reference]
                    start, count = args.get("start", 0), args.get("count", 0)
                    if (type(start) is not int or type(count) is not int or not 0 <= start < 1 << 64 or not 0 <= count <= 1024
                            or args.get("filter") not in (None, "named", "indexed")):
                        raise ValueError("bounded dense-index WASIp1 scope pagination required")
                    self.observe(self.command("status"), notify=False)
                    if (self.state != "stopped" or self.stop_key != expected_key or reference not in self.wasip1_scope_stops
                            or any(t.get("native_pc") for t in self.threads)):
                        raise ValueError("WASIp1 scope belongs to a retired or native stop")
                    if args.get("filter") == "named":
                        self.respond(request, {"variables": []}); return
                    values = self.paged_wasip1_scope(expected_key, module, selected, start, count)
                    if self.stop_key != expected_key or reference not in self.wasip1_scope_stops:
                        raise ValueError("WASIp1 scope retired during pagination")
                    self.respond(request, {"variables": values})
                    return
                if reference in self.native_register_scopes:
                    expected_key, frame_id = self.native_register_scopes[reference]
                    if self.stop_key != expected_key or args.get("filter") not in (None, "named", "indexed"):
                        raise ValueError("retired or invalid native register scope")
                    start, count = args.get("start", 0), args.get("count", 64)
                    if type(count) is int and count == 0: count = 64
                    if type(start) is not int or type(count) is not int or not 0 <= start <= 64 or not 1 <= count <= 64:
                        raise ValueError("bounded native register page required")
                    _, values = self.query_native_registers(frame_id)
                    if reference not in self.native_register_scopes or self.stop_key != expected_key:
                        raise ValueError("native register scope retired during copy")
                    self.respond(request, {"variables": [] if args.get("filter") == "indexed" else values[start:start + count]})
                    return
                if reference in self.wasm_object_views:
                    expected_key, obj, anchor = self.wasm_object_views[reference]
                    start, count = args.get("start", 0), args.get("count", 0)
                    if (type(start) is not int or type(count) is not int or not 0 <= start < 1 << 64 or not 0 <= count <= 1024
                            or args.get("filter") not in (None, "named", "indexed")):
                        raise ValueError("bounded original-index GC member pagination required")
                    self.observe(self.command("status"), notify=False)
                    if self.state != "stopped" or self.stop_key != expected_key or reference not in self.wasm_object_views:
                        raise ValueError("Wasm object display belongs to a retired stop")
                    if args.get("filter") == "named":
                        self.respond(request, {"variables": []}); return
                    values = self.paged_wasm_members(expected_key, obj, anchor, start, count)
                    if self.stop_key != expected_key or reference not in self.wasm_object_views:
                        raise ValueError("Wasm member display retired during pagination")
                    self.respond(request, {"variables": values})
                    return
                if reference in self.wasm_scope_stops:
                    expected_key, selection = self.wasm_scope_stops[reference]
                    start, count = args.get("start", 0), args.get("count", 0)
                    if (type(start) is not int or type(count) is not int or not 0 <= start < 1 << 64 or not 0 <= count <= 1024
                            or args.get("filter") not in (None, "named", "indexed")):
                        raise ValueError("bounded original-index Wasm scope pagination required")
                    self.observe(self.command("status"), notify=False)
                    if self.state != "stopped" or self.stop_key != expected_key or reference not in self.wasm_scope_stops:
                        raise ValueError("Wasm value scope belongs to a retired stop")
                    if args.get("filter") == "named":
                        self.respond(request, {"variables": []}); return
                    values = self.paged_wasm_scope(expected_key, selection, start, count)
                    if self.stop_key != expected_key or reference not in self.wasm_scope_stops:
                        raise ValueError("Wasm value scope retired during pagination")
                    self.respond(request, {"variables": values})
                    return
                if reference in self.source_pointer_views:
                    start, count = args.get("start", 0), args.get("count", 0)
                    if (type(start) is not int or type(count) is not int or not 0 <= start <= 1024
                            or not 0 <= count <= 1024 or args.get("filter") not in (None, "named", "indexed")):
                        raise ValueError("bounded source-pointer pagination required")
                    self.source_pointer_members(reference)
                if reference in self.source_object_views:
                    expected_key, frame_id, copied = self.source_object_views[reference]
                    start, count = args.get('start', 0), args.get('count', 0)
                    selected = args.get('filter')
                    if (type(start) is not int or type(count) is not int or not 0 <= start <= 1024
                            or not 0 <= count <= 1024 or selected not in (None, 'named', 'indexed')):
                        raise ValueError("bounded copied source-object pagination required")
                    self.observe(self.command('status'), notify=False)
                    if (self.state != 'stopped' or self.stop_key != expected_key
                            or reference not in self.source_object_views or frame_id not in self.source_frame_ordinals):
                        raise ValueError("source object view belongs to a retired source frame")
                    values = [dict(value) for value, indexed in copied
                              if selected is None or indexed == (selected == 'indexed')]
                    values = values[start:] if count == 0 else values[start:start + count]
                    self.observe(self.command('status'), notify=False)
                    if (self.state != 'stopped' or self.stop_key != expected_key
                            or reference not in self.source_object_views or frame_id not in self.source_frame_ordinals):
                        raise ValueError("source object view retired during pagination")
                    self.respond(request, {'variables': values})
                    return
                thread = self.scopes.get(reference)
                if thread is None:
                    raise ValueError("stale or unknown scope")
                if reference in self.source_scope_stops:
                    start, count = args.get("start", 0), args.get("count", 0)
                    if (type(start) is not int or type(count) is not int or not 0 <= start <= 1024 or not 0 <= count <= 1024
                            or args.get("filter") not in (None, "named", "indexed")):
                        raise ValueError("bounded source-variable pagination required")
                    source_scope = self.source_scope_stops[reference]
                    selected_row = source_scope[1] if isinstance(source_scope, tuple) and len(source_scope) == 2 and isinstance(source_scope[1], dict) else None
                    expected_key = source_scope[0] if selected_row is not None else source_scope
                    # A genuine complete stop must still match, even if a loop
                    # revisited exactly the same PC and function generation.
                    self.observe(self.command("status"))
                    current = [entry for entry in self.threads if entry["id"] == thread]
                    if (self.state != "stopped" or self.stop_key != expected_key or reference not in self.source_scope_stops
                            or len(current) != 1 or "native_pc" in current[0] or "stop_id" not in current[0]):
                        raise ValueError("source scope belongs to a retired stop")
                    if selected_row is not None and not selected_row["current"]:
                        self.respond(request, {"variables": [{"name": "<unavailable>", "value": "caller PC and locals were not captured",
                            "type": "unavailable", "variablesReference": 0, "presentationHint": {"attributes": ["readOnly"]}}]})
                        return
                    spelling = f"locals source {thread}" if selected_row is None else f"locals source {thread} {current[0]['stop_id']} {selected_row['ordinal']}"
                    variables = parse_source_values(self.command(spelling), current[0]["stop_id"])
                    self.observe(self.command("status"), notify=False)
                    if self.state != "stopped" or self.stop_key != expected_key or reference not in self.source_scope_stops:
                        raise ValueError("source scope retired during variable copy")
                    if args.get("filter") == "indexed":
                        variables = []
                    else:
                        # Probe only this requested page, not hidden locals.
                        all_variables = variables
                        variables = variables[start:] if count == 0 else variables[start:start + count]
                        if selected_row is not None:
                            variables = self.source_scope_objects(reference, expected_key, selected_row, variables, all_variables)
                        # The scalar copy and each optional symbol read already
                        # have their own real post-read status observation.
                        if self.state != "stopped" or self.stop_key != expected_key or reference not in self.source_scope_stops:
                            raise ValueError("source scope retired during object projection")
                    self.respond(request, {"variables": variables})
                    return
                expected_key = self.wasm_local_scope_stops.get(reference)
                if expected_key is None:
                    raise ValueError("stale or unknown Wasm local scope")
                start, count = args.get("start", 0), args.get("count", 0)
                if (type(start) is not int or type(count) is not int or not 0 <= start < 1 << 64 or not 0 <= count <= 1024
                        or args.get("filter") not in (None, "named", "indexed")):
                    raise ValueError("bounded Wasm locals pagination required")
                self.observe(self.command("status"), notify=False)
                if self.state != "stopped" or self.stop_key != expected_key or reference not in self.wasm_local_scope_stops:
                    raise ValueError("Wasm local scope belongs to a retired stop")
                if args.get("filter") == "named":
                    self.respond(request, {"variables": []})
                    return
                current = [entry for entry in self.threads if entry["id"] == thread]
                if len(current) == 1 and "stop_id" in current[0]:
                    variables = self.paged_wasm_locals(thread, expected_key, start, count)
                    if self.stop_key != expected_key or reference not in self.wasm_local_scope_stops:
                        raise ValueError("Wasm local scope retired during pagination")
                    self.respond(request, {"variables": variables})
                    return
                reply = self.command(f"locals {thread}")
                variables = []
                for line in reply.splitlines():
                    match = LOCAL_RE.match(line)
                    if match:
                        value = match.group(2)
                        variables.append({"name": f"local {match.group(1)}", "value": value,
                                          "type": value.split("=", 1)[0], "variablesReference": 0})
                self.observe(self.command("status"), notify=False)
                if self.state != "stopped" or self.stop_key != expected_key or reference not in self.wasm_local_scope_stops:
                    raise ValueError("Wasm local scope retired during copy")
                variables = variables[start:] if count == 0 else variables[start:start + count]
                self.respond(request, {"variables": variables})
            elif command == "readMemory":
                reference = args.get("memoryReference")
                match = WASM_MEMORY_RE.fullmatch(reference) if isinstance(reference, str) else None
                if match is None:
                    raise ValueError("expected wasm-memory:MODULE:MEMORY:BYTE_OFFSET")
                module, memory, address = map(int, match.groups())
                displacement = args.get("offset", 0)
                count = args.get("count")
                if type(displacement) is not int or type(count) is not int or not 0 <= count <= 65536:
                    raise ValueError("bounded Wasm memory offset and count required")
                address += displacement
                if (module >= 1 << 64 or memory >= 1 << 32 or not 0 <= address < 1 << 64
                        or address > (1 << 64) - 1 - count):
                    raise ValueError("Wasm memory range out of bounds")
                data = self.read_wasm_memory(module, memory, address, count)
                self.respond(request, {"address": str(address),
                                       "data": base64.b64encode(data).decode("ascii"),
                                       "unreadableBytes": 0})
            elif command == "writeMemory":
                reference = args.get("memoryReference")
                match = WASM_MEMORY_RE.fullmatch(reference) if isinstance(reference, str) else None
                encoded = args.get("data")
                displacement = args.get("offset", 0)
                partial = args.get("allowPartial", False)
                if (match is None or type(displacement) is not int or abs(displacement) > (1 << 53) - 1
                        or type(partial) is not bool or partial
                        or not isinstance(encoded, str) or len(encoded) > 344 or not encoded.isascii()):
                    raise ValueError("bounded guest memory write requires base64 data, signed offset and allowPartial=false")
                data = base64.b64decode(encoded, validate=True)
                if len(data) > 256 or base64.b64encode(data).decode("ascii") != encoded:
                    raise ValueError("canonical base64 for at most 256 Wasm bytes required")
                module, memory, address = map(int, match.groups())
                address += displacement
                if (module >= 1 << 64 or memory >= 1 << 32 or not 0 <= address < 1 << 64
                        or address > (1 << 64) - 1 - len(data)):
                    raise ValueError("Wasm memory write range out of bounds")
                self.write_wasm_memory(module, memory, address, data)
                self.respond(request, {"offset": displacement, "bytesWritten": len(data)})
                if data and self.supports_memory_event:
                    self.event("memory", {"memoryReference": reference, "offset": displacement, "count": len(data)})
                if data and self.supports_invalidated_event:
                    self.event("invalidated", {"areas": ["stacks", "variables"]})
            elif command == "source":
                ref = unsigned_id(args.get("sourceReference"), "sourceReference")
                frame = self.frames.get(ref)
                if frame is None or ref in self.native_frames or ref in self.inline_frames:
                    raise ValueError("unknown Wasm frame source")
                frame = self.current_frame(ref)
                self.respond(request, {"content": f";; module {frame[2]} function {frame[3]}\n"
                                                  ";; Exact byte offset is shown in the current thread location.\n",
                                       "mimeType": "text/x-wat"})
            elif command == "setBreakpoints":
                source = args.get("source") or {}
                if not isinstance(source, dict) or not isinstance(args.get("breakpoints", []), list):
                    raise ValueError("invalid source breakpoints")
                path = source.get("path")
                if not isinstance(path, str) or not path or any(ord(c) < 32 for c in path):
                    raise ValueError("source path required")
                for identifier in self.breakpoints.pop(path, []):
                    try:
                        self.command(f"delete {identifier}")
                    except RuntimeError:
                        pass  # Function replacement may already have invalidated it.
                registered = []
                for point in args.get("breakpoints", []):
                    if not isinstance(point, dict):
                        registered.append({"verified": False, "message": "invalid source breakpoint"})
                        continue
                    line = point.get("line")
                    if type(line) is not int or not 0 < line < (1 << 64):
                        registered.append({"verified": False, "message": "invalid source line"})
                        continue
                    try:
                        ignore = breakpoint_ignore_count(point)
                        suffix = ("" if ignore is None else f" ignore {ignore}") + breakpoint_condition_suffix(point)
                        reply = self.command(f"break-source {self.module_id} {path}:{line}{suffix}")
                        match = BREAK_RE.match(reply)
                        if not match:
                            raise RuntimeError(reply.strip())
                        identifier = int(match.group(1))
                        self.breakpoints.setdefault(path, []).append(identifier)
                        registered.append({"id": identifier, "verified": True, "line": line})
                    except (RuntimeError, ValueError) as error:
                        registered.append({"verified": False, "line": line, "message": str(error)})
                self.respond(request, {"breakpoints": registered})
            elif command == "setInstructionBreakpoints":
                if not isinstance(args.get("breakpoints", []), list):
                    raise ValueError("invalid instruction breakpoints")
                for identifier in self.instruction_breakpoints:
                    try:
                        self.command(f"delete {identifier}")
                    except RuntimeError:
                        pass  # Function replacement may already have invalidated it.
                self.instruction_breakpoints.clear()
                registered = []
                for point in args.get("breakpoints", []):
                    reference = point.get("instructionReference") if isinstance(point, dict) else None
                    match = WASM_LOCATION_RE.fullmatch(reference) if isinstance(reference, str) else None
                    if match is None:
                        registered.append({"verified": False, "message": "expected wasm:MODULE:FUNCTION:BYTE_OFFSET"})
                        continue
                    module, function, byte_offset = map(int, match.groups())
                    offset = point.get("offset", 0)
                    if type(offset) is not int:
                        registered.append({"verified": False, "message": "invalid Wasm instruction offset"})
                        continue
                    byte_offset += offset
                    if any(value < 0 or value >= (1 << 64) for value in (module, function, byte_offset)):
                        registered.append({"verified": False, "message": "Wasm instruction offset out of range"})
                        continue
                    try:
                        ignore = breakpoint_ignore_count(point)
                        suffix = ("" if ignore is None else f" ignore {ignore}") + breakpoint_condition_suffix(point)
                        reply = self.command(f"break {module} {function} {byte_offset}{suffix}")
                        breakpoint = BREAK_RE.match(reply)
                        if breakpoint is None:
                            raise RuntimeError(reply.strip())
                        identifier = int(breakpoint.group(1))
                        self.instruction_breakpoints.append(identifier)
                        registered.append({"id": identifier, "verified": True,
                                           "instructionReference": f"wasm:{module}:{function}:{byte_offset}"})
                    except (RuntimeError, ValueError) as error:
                        registered.append({"verified": False, "message": str(error)})
                self.respond(request, {"breakpoints": registered})
            elif command == "evaluate":
                expression = args.get("expression")
                if not isinstance(expression, str):
                    raise ValueError("debug console expression required")
                language_frame = self.source_frame_ordinals.get(args.get("frameId")) if type(args.get("frameId")) is int else None
                source_context = args.get("context") in ("watch", "hover", "variables")
                if source_context and type(args.get("frameId")) is int and args["frameId"] in self.native_frames:
                    # Validate syntax before any broker command; no native
                    # dereference/arithmetic or language-evaluation fallback.
                    native_register_selector(expression, "x86_64")
                    architecture, values = self.query_native_registers(args["frameId"])
                    selected = native_register_selector(expression, architecture)
                    matched = [row for row in values if row["name"] == selected]
                    if len(matched) != 1: raise ValueError("register unavailable on the current architecture")
                    self.respond(request, {"result": matched[0]["value"], "variablesReference": 0,
                                           "presentationHint": {"attributes": ["readOnly"]}})
                    return
                if source_context:
                    if language_frame is None:
                        raise ValueError("watch/hover requires an actual current opaque source frame; no console command fallback")
                    expected_key, row = language_frame
                    if not row["current"]:
                        raise ValueError("caller source PC and locals were not captured")
                    source_expression = validate_source_evaluation_expression(expression)
                    self.observe(self.command("status"), notify=False)
                    if self.state != "stopped" or self.stop_key != expected_key or args["frameId"] not in self.source_frame_ordinals:
                        raise ValueError("selected source frame belongs to a retired stop")
                    result = self.command(f"print-frame {row['thread']} {row['stop_id']} {row['ordinal']} {source_expression}")
                    self.observe(self.command("status"), notify=False)
                    if self.state != "stopped" or self.stop_key != expected_key or args["frameId"] not in self.source_frame_ordinals:
                        raise ValueError("selected source frame retired during value copy")
                    validate_source_evaluation_reply(result, row["stop_id"], row["thread"])
                else:
                    if expression.lstrip().startswith(("set wasip1 ", "unset wasip1 ", "set wasm ", "wasm-script ")):
                        # A successful host-environment mutation leaves the
                        # Wasm stop in place, but copied debugger displays must
                        # retire BEFORE issuing the write. Reference numbers
                        # remain session-unique even if its stop key is unchanged.
                        self.invalidate_stop()
                    result = self.command(expression)
                words = expression.split()
                observation = self._decode_status(result) if (
                    not source_context and words and words[0] in
                    ("continue", "c", "pause", "step", "s", "ni", "nexti", "finish", "fin", "status", "wait")) else None
                display = self.source_object_display(result, expected_key, args["frameId"], row) if source_context else {"result": result.rstrip("\n"), "variablesReference": 0}
                if source_context and not self.supports_variable_type:
                    display.pop("type", None)
                self.respond(request, display)
                if observation is not None:
                    # Commit after the response, without reparsing. A terminal
                    # reply still emits exited before the endpoint's idle poll.
                    self._commit_status(observation)
            elif command == "disconnect":
                self.invalidate_stop()
                if self.broker is not None:
                    self.broker.close()
                    self.broker = None
                self.respond(request)
                if not self.finished:
                    self.finished = True
                    self.event("terminated")
            else:
                raise ValueError(f"DAP request {command!r} is unavailable")
        except (OSError, ValueError, RuntimeError, ConnectionError, KeyError) as error:
            self.invalidate_stop()
            self.respond(request, error=error)


def main():
    adapter = Adapter(sys.stdout.buffer)
    inbox = queue.Queue()

    def input_reader():
        try:
            while True:
                request = read_dap(sys.stdin.buffer)
                inbox.put(request)
                if request is None:
                    break
        except (EOFError, ValueError, json.JSONDecodeError) as error:
            print(f"DAP input: {error}", file=sys.stderr)
            inbox.put(None)

    threading.Thread(target=input_reader, daemon=True).start()
    while True:
        try:
            request = inbox.get(timeout=0.25)
        except queue.Empty:
            if adapter.broker is not None and not adapter.finished:
                try:
                    adapter.poll()
                except (OSError, ValueError, RuntimeError, ConnectionError) as error:
                    print(f"debug broker: {error}", file=sys.stderr)
                    adapter.finished = True
                    adapter.invalidate_stop()
                    adapter.event("terminated")
            continue
        if request is None:
            break
        adapter.handle(request)
    if adapter.broker is not None:
        adapter.broker.close()
    adapter.invalidate_stop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
