#!/usr/bin/env python3
"""Regression inputs for JIT evidence with empty .text and code elsewhere.

Run only inside the Linux wasm3 test cgroup. These are constructed ELF objects,
not architecture instructions executed by the test process.
"""
from dataclasses import replace
from pathlib import Path
import struct
import subprocess
import unittest

from elf_executable_sections import (
    ELFCodeError, SHF_EXECINSTR, compare_executable_images,
    normalize_relocations, read_executable_sections,
)


def make_elf(sections, bits=64, big=False, extended=False):
    """Build an independent, small ET_REL fixture with a real shstrtab.

    sections contains (name: bytes, data: bytes, flags: int, alignment: int).
    No code-section naming convention is assumed by the builder.
    """
    order = ">" if big else "<"
    eh_format = order + ("HHIQQQIHHHHHH" if bits == 64 else "HHIIIIIHHHHHH")
    sh_format = order + ("IIQQQQIIQQ" if bits == 64 else "IIIIIIIIII")
    eh_size = 16 + struct.calcsize(eh_format)
    sh_size = struct.calcsize(sh_format)
    names = bytearray(b"\0")
    body = bytearray(eh_size)
    headers = []
    for name, contents, flags, alignment in sections:
        name_offset = len(names)
        names.extend(name + b"\0")
        if alignment:
            body.extend(b"\0" * (-len(body) % alignment))
        offset = len(body)
        body.extend(contents)
        headers.append((name_offset, 1, flags, 0, offset, len(contents), 0, 0, alignment, 0))
    strings_name = len(names)
    names.extend(b".shstrtab\0")
    strings_offset = len(body)
    body.extend(names)
    headers.append((strings_name, 3, 0, 0, strings_offset, len(names), 0, 0, 1, 0))
    body.extend(b"\0" * (-len(body) % (8 if bits == 64 else 4)))
    section_offset = len(body)
    count = len(headers) + 1
    strings_index = count - 1
    zero = (0, 0, 0, 0, 0, count if extended else 0, strings_index if extended else 0, 0, 0, 0)
    for header in [zero] + headers:
        body.extend(struct.pack(sh_format, *header))
    body[:16] = b"\x7fELF" + bytes((2 if bits == 64 else 1, 2 if big else 1, 1, 0, 0)) + bytes(7)
    struct.pack_into(eh_format, body, 16, 1, 62 if bits == 64 else 3, 1, 0, 0,
                     section_offset, 0, eh_size, 0, 0, sh_size,
                     0 if extended else count, 0xffff if extended else strings_index)
    return bytes(body)


def alter_section(data, index, field, value):
    result = bytearray(data)
    order = "<" if data[5] == 1 else ">"
    is64 = data[4] == 2
    eh_format = order + ("HHIQQQIHHHHHH" if is64 else "HHIIIIIHHHHHH")
    sh_format = order + ("IIQQQQIIQQ" if is64 else "IIIIIIIIII")
    header = struct.unpack_from(eh_format, data, 16)
    location = header[5] + index * struct.calcsize(sh_format)
    fields = list(struct.unpack_from(sh_format, data, location))
    fields[field] = value
    struct.pack_into(sh_format, result, location, *fields)
    return bytes(result)


class ExecutableSectionEvidence(unittest.TestCase):
    def test_empty_text_with_function_section_all_formats(self):
        for bits in (32, 64):
            for big in (False, True):
                for extended in (False, True):
                    with self.subTest(bits=bits, big=big, extended=extended):
                        obj = make_elf([(b".text", b"", 6, 4),
                                        (b".text.uwvm_m_ab123_f0", b"\x90\xc3", 6, 16)],
                                       bits, big, extended)
                        image = read_executable_sections(obj)
                        self.assertEqual(image.text_bytes, 2)
                        self.assertEqual(image.sections[0].data, b"")
                        self.assertEqual(image.sections[1].name, b".text.uwvm_m_HASH_f0")
                        self.assertEqual(image.sections[1].data, b"\x90\xc3")
                        self.assertEqual(image.summary()["nonempty_executable_section_count"], 1)

    def test_large_code_model_ltext_and_arbitrary_names(self):
        for name, flags in ((b".ltext", 0x10000006), (b"odd-code-name", SHF_EXECINSTR)):
            with self.subTest(name=name):
                image = read_executable_sections(make_elf([(b".text", b"", 6, 16),
                                                          (name, b"\x90\xc3", flags, 32)]))
                self.assertEqual(image.sections[1].name, name)
                self.assertEqual(image.sections[1].flags, flags)
                self.assertEqual(image.sections[1].alignment, 32)
                self.assertEqual(image.text_bytes, 2)

    def test_no_executable_bytes_is_a_failure(self):
        for sections in ([(b".text", b"", 6, 16)], [(b".data", b"\x90\xc3", 3, 1)]):
            with self.subTest(sections=sections), self.assertRaises(ELFCodeError):
                read_executable_sections(make_elf(sections))

    def test_empty_text_cannot_hide_changed_code(self):
        images = [read_executable_sections(make_elf([(b".text", b"", 6, 16),
                                                   (b".text.fn", code, 6, 16)]))
                  for code in (b"\x90\xc3", b"\x31\xc3")]
        with self.assertRaises(ELFCodeError):
            compare_executable_images(*images)

    def test_section_split_cannot_hide_behind_identical_concatenation(self):
        left = read_executable_sections(make_elf([(b".text.fn", b"\x90\xc3", 6, 16)]))
        right = read_executable_sections(make_elf([(b".text.fn", b"\x90", 6, 16),
                                                 (b".text.other", b"\xc3", 6, 16)]))
        self.assertEqual(left.summary()["text_sha256"], right.summary()["text_sha256"])
        self.assertNotEqual(left.summary()["executable_sections_sha256"], right.summary()["executable_sections_sha256"])
        with self.assertRaises(ELFCodeError):
            compare_executable_images(left, right)

    def test_only_module_namespace_is_normalized(self):
        base = read_executable_sections(make_elf([(b".text.uwvm_m_ab123_f0", b"\xc3", 6, 16)]))
        same = read_executable_sections(make_elf([(b".text.uwvm_m_123ff_f0", b"\xc3", 6, 16)]))
        compare_executable_images(base, same)
        other = read_executable_sections(make_elf([(b".text.uwvm_m_123ff_f1", b"\xc3", 6, 16)]))
        with self.assertRaises(ELFCodeError):
            compare_executable_images(base, other)
        for field, value in (("alignment", 32), ("flags", 7), ("section_type", 7),
                             ("address", 32), ("link", 2), ("info", 2), ("entry_size", 1)):
            with self.subTest(field=field), self.assertRaises(ELFCodeError):
                compare_executable_images(base, replace(base, sections=(replace(base.sections[0], **{field: value}),)))
        with self.assertRaises(ELFCodeError):
            compare_executable_images(base, replace(base, machine=183))

    def test_bounded_headers_sections_names_and_extended_indices(self):
        valid = make_elf([(b".text.fn", b"\x90\xc3", 6, 16)])
        invalid = [valid[:end] for end in (0, 4, 15, 16, 63, len(valid) - 1)]
        invalid.extend([alter_section(valid, 1, 4, len(valid)),
                        alter_section(valid, 1, 5, len(valid)),
                        alter_section(valid, 1, 0, 0xffffffff),
                        alter_section(valid, 1, 1, 8),
                        alter_section(valid, 1, 2, 6 | 0x800),
                        alter_section(valid, 1, 8, 3)])
        extended = make_elf([(b".text.fn", b"\xc3", 6, 16)], extended=True)
        invalid.extend([alter_section(extended, 0, 5, 0xffffffffffffffff),
                        alter_section(extended, 0, 6, 0xffffffff)])
        for number, obj in enumerate(invalid):
            with self.subTest(number=number), self.assertRaises(ELFCodeError):
                read_executable_sections(obj)

    def test_overlapping_code_sections_are_rejected(self):
        obj = make_elf([(b".text.one", b"\x90\xc3", 6, 16),
                        (b".text.two", b"\x90\xc3", 6, 16)])
        # ELF64 header's section table offset is at byte 40. Section one's
        # sh_offset is its fifth field, at +24 in the ELF64 section header.
        table = struct.unpack_from("<Q", obj, 40)[0]
        first_offset = struct.unpack_from("<Q", obj, table + 64 + 24)[0]
        with self.assertRaises(ELFCodeError):
            read_executable_sections(alter_section(obj, 2, 4, first_offset))

    def test_full_relocations_preserve_offsets_types_addends_and_section_names(self):
        original = "File: /left/native.o\nRelocations [\n Section (3) .rela.ltext {\n 0x10 R_X86_64_64 uwvm_m_abcd_f0 0x0\n }\n]\n"
        renamed = original.replace("/left/", "/right/").replace("abcd", "deadbeef")
        self.assertEqual(normalize_relocations(original), normalize_relocations(renamed))
        for before, after in (("0x10", "0x18"), ("R_X86_64_64", "R_X86_64_PC32"),
                              ("0x0", "0x4"), (".rela.ltext", ".rela.text"),
                              ("_f0", "_f1"), ("Section (3)", "Section (4)")):
            with self.subTest(before=before):
                self.assertNotEqual(normalize_relocations(original), normalize_relocations(original.replace(before, after)))


if __name__ == "__main__":
    repo = Path(__file__).resolve().parents[2]
    subprocess.run(["bash", str(repo / "tools/ci/require_wasm3_test_cgroup.sh")], check=True)
    unittest.main()
