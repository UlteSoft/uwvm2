#!/usr/bin/env python3
"""Bounded ELF code-section evidence; never assume generated code lives in .text.

ELF32/ELF64, both byte orders, and extended section indices follow:
https://gabi.xinuos.com/elf/02-eheader.html
https://gabi.xinuos.com/elf/03-sheader.html
Only the uwvm_m_<hex>_ namespace is normalized. File offsets are container
placement, not code attributes; section order, identity, flags and bytes remain.
"""
from dataclasses import dataclass
import argparse
import hashlib
import json
import re
import struct
import subprocess
from pathlib import Path

SHF_EXECINSTR = 0x4
SHF_COMPRESSED = 0x800
SHT_NOBITS = 8
MAX_OBJECT_BYTES = 256 * 1024 * 1024
MAX_SECTIONS = 1024 * 1024
MAX_SECTION_NAME_BYTES = 4096
MODULE_NAMESPACE = re.compile(rb"uwvm_m_[0-9a-f]+_")


class ELFCodeError(ValueError):
    """An object cannot provide complete, nonempty executable-section evidence."""


def _slice(data: bytes, offset: int, size: int, description: str) -> bytes:
    # Subtraction form makes the half-open extent explicit; no truncated slice
    # is accepted even though Python itself would silently clamp its endpoint.
    if offset < 0 or size < 0 or offset > len(data) or size > len(data) - offset:
        raise ELFCodeError(f"{description} lies outside the ELF object")
    return data[offset:offset + size]


@dataclass(frozen=True)
class ExecutableSection:
    index: int
    name: bytes
    section_type: int
    flags: int
    address: int
    link: int
    info: int
    alignment: int
    entry_size: int
    data: bytes

    def manifest(self) -> dict:
        return dict(index=self.index, name=self.name.decode("utf-8", "backslashreplace"),
                    name_hex=self.name.hex(), section_type=self.section_type,
                    flags=self.flags, address=self.address, link=self.link,
                    info=self.info, alignment=self.alignment, entry_size=self.entry_size,
                    bytes=len(self.data), sha256=hashlib.sha256(self.data).hexdigest())


@dataclass(frozen=True)
class ExecutableImage:
    elf_class: int
    data_encoding: int
    os_abi: int
    abi_version: int
    machine: int
    flags: int
    sections: tuple[ExecutableSection, ...]

    @property
    def text_bytes(self) -> int:
        # Historical summary field, now covering ALL executable sections.
        return sum(len(section.data) for section in self.sections)

    def manifest(self) -> dict:
        return dict(elf_class=self.elf_class, data_encoding=self.data_encoding,
                    os_abi=self.os_abi, abi_version=self.abi_version, machine=self.machine,
                    flags=self.flags, executable_bytes=self.text_bytes,
                    sections=[section.manifest() for section in self.sections])

    def summary(self) -> dict:
        digest = hashlib.sha256()
        for section in self.sections:
            digest.update(section.data)
        manifest = self.manifest()
        encoded = json.dumps(manifest, sort_keys=True, separators=(",", ":")).encode()
        return dict(text_bytes=self.text_bytes, text_sha256=digest.hexdigest(),
                    executable_sections_sha256=hashlib.sha256(encoded).hexdigest(),
                    executable_section_count=len(self.sections),
                    nonempty_executable_section_count=sum(bool(s.data) for s in self.sections),
                    executable_sections=manifest["sections"])


def read_executable_sections(data: bytes) -> ExecutableImage:
    if len(data) > MAX_OBJECT_BYTES:
        raise ELFCodeError("ELF object exceeds the evidence-reader size limit")
    ident = _slice(data, 0, 16, "ELF identification")
    if ident[:4] != b"\x7fELF" or ident[4] not in (1, 2) or ident[5] not in (1, 2) or ident[6] != 1:
        raise ELFCodeError("unsupported ELF magic, class, byte order or version")
    endian = "<" if ident[5] == 1 else ">"
    header_format = endian + ("HHIIIIIHHHHHH" if ident[4] == 1 else "HHIQQQIHHHHHH")
    section_format = endian + ("IIIIIIIIII" if ident[4] == 1 else "IIQQQQIIQQ")
    header_size = 16 + struct.calcsize(header_format)
    section_size = struct.calcsize(section_format)
    header = struct.unpack(header_format, _slice(data, 16, header_size - 16, "ELF header"))
    object_type, machine, version, _, _, section_offset, flags, declared_header_size, _, _, entry_size, count, strings_index = header
    if object_type != 1 or version != 1 or declared_header_size != header_size:
        raise ELFCodeError("expected a current-version relocatable ELF object")
    if section_offset < header_size or entry_size != section_size:
        raise ELFCodeError("missing or unsupported ELF section-header table")
    zero = struct.unpack(section_format, _slice(data, section_offset, section_size, "section zero"))
    if zero[0] != 0 or zero[1] != 0:
        raise ELFCodeError("section zero must be the null section")
    if count == 0:
        count = zero[5]
    if strings_index == 0xffff:
        strings_index = zero[6]
    if count < 2 or count > MAX_SECTIONS or count > (len(data) - section_offset) // section_size:
        raise ELFCodeError("invalid or truncated section-header count")
    if strings_index == 0 or strings_index >= count:
        raise ELFCodeError("missing or invalid section-name string table index")
    # The complete table bound is established before any indexed unpack.
    headers = [struct.unpack_from(section_format, data, section_offset + index * section_size)
               for index in range(count)]
    strings_header = headers[strings_index]
    if strings_header[1] != 3 or strings_header[2] & SHF_COMPRESSED:
        raise ELFCodeError("section-name table must be an uncompressed string table")
    strings = _slice(data, strings_header[4], strings_header[5], "section-name string table")
    if not strings or strings[0] != 0 or strings[-1] != 0:
        raise ELFCodeError("section-name table is not NUL delimited")
    executable = []
    ranges = []
    total_bytes = 0
    for index, section in enumerate(headers):
        name_offset, kind, attributes, address, offset, size, link, info, alignment, stride = section
        if not attributes & SHF_EXECINSTR:
            continue
        if kind in (0, SHT_NOBITS) or attributes & SHF_COMPRESSED:
            raise ELFCodeError("executable section has no directly readable machine-code bytes")
        if alignment and alignment & (alignment - 1):
            raise ELFCodeError("executable section alignment is not a power of two")
        if name_offset >= len(strings):
            raise ELFCodeError("executable section name lies outside its string table")
        name_end = strings.find(b"\0", name_offset, min(len(strings), name_offset + MAX_SECTION_NAME_BYTES + 1))
        if name_end < 0:
            raise ELFCodeError("unterminated or oversized executable section name")
        name = MODULE_NAMESPACE.sub(b"uwvm_m_HASH_", strings[name_offset:name_end])
        total_bytes += size
        if total_bytes > len(data):
            raise ELFCodeError("executable section sizes exceed the object extent")
        contents = _slice(data, offset, size, "executable section contents")
        if size:
            ranges.append((offset, offset + size))
        executable.append(ExecutableSection(index, name, kind, attributes, address, link,
                                            info, alignment, stride, contents))
    if not executable or total_bytes == 0:
        raise ELFCodeError("object has no nonempty SHF_EXECINSTR sections")
    ranges.sort()
    if any(right[0] < left[1] for left, right in zip(ranges, ranges[1:])):
        raise ELFCodeError("executable section byte extents overlap")
    return ExecutableImage(ident[4], ident[5], ident[7], ident[8], machine, flags, tuple(executable))


def read_executable_object(path: Path) -> ExecutableImage:
    if path.stat().st_size > MAX_OBJECT_BYTES:
        raise ELFCodeError("ELF object exceeds the evidence-reader size limit")
    return read_executable_sections(path.read_bytes())


def write_executable_evidence(path: Path, output: Path) -> ExecutableImage:
    image = read_executable_object(path)
    for section in image.sections:
        (output / f"native.exec-{section.index:06d}.bin").write_bytes(section.data)
    (output / "executable-sections.json").write_text(json.dumps(image.manifest(), indent=2) + "\n")
    return image


def normalize_relocations(relocations: str) -> str:
    # Ignore only llvm-readobj's input filename, which is not relocation data.
    # Preserve every section index, offset, relocation type, symbol and addend.
    relocations = re.sub(r"uwvm_m_[0-9a-f]+_", "uwvm_m_HASH_", relocations)
    return "\n".join(line for line in relocations.splitlines() if not line.startswith("File:"))


def compare_executable_images(left: ExecutableImage, right: ExecutableImage) -> None:
    if left != right:
        raise ELFCodeError("executable section identity, attributes, alignment or bytes differ")


def review_existing_objects(root: Path, readobj: Path) -> dict:
    """Recheck retained *-short/native.o versus *-explicit/native.o without JIT."""
    rows = []
    for left_path in sorted(root.rglob("native.o")):
        if not left_path.parent.name.endswith("-short"):
            continue
        right_path = left_path.parent.with_name(left_path.parent.name[:-6] + "-explicit") / "native.o"
        row = dict(left=str(left_path), right=str(right_path), passed=False)
        try:
            left = write_executable_evidence(left_path, left_path.parent)
            right = write_executable_evidence(right_path, right_path.parent)
            row.update(left= str(left_path), right=str(right_path), **left.summary())
            row["right_executable_sections"] = right.manifest()
            row["left_object_sha256"] = hashlib.sha256(left_path.read_bytes()).hexdigest()
            row["right_object_sha256"] = hashlib.sha256(right_path.read_bytes()).hexdigest()
            compare_executable_images(left, right)
            reports = []
            for path in (left_path, right_path):
                raw = subprocess.check_output([str(readobj), "--relocations", str(path)], text=True)
                (path.parent / "relocations-requalified.txt").write_text(raw)
                reports.append(normalize_relocations(raw))
            if reports[0] != reports[1]:
                raise ELFCodeError("complete object relocations differ")
            row.update(passed=True, identical_relocations=True)
        except (ELFCodeError, OSError, subprocess.CalledProcessError) as error:
            row["error"] = str(error)
        rows.append(row)
    if not rows:
        raise ELFCodeError("no retained short/explicit object pairs found")
    return dict(passed=all(row["passed"] for row in rows), comparisons=len(rows), checks=rows)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--review", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--llvm-readobj", type=Path, default=Path("/toolchain/bin/llvm-readobj"))
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    subprocess.run(["bash", str(repo / "tools/ci/require_wasm3_test_cgroup.sh")], check=True)
    report = review_existing_objects(args.review, args.llvm_readobj)
    args.out.write_text(json.dumps(report, indent=2) + "\n")
    print("PASS" if report["passed"] else "FAIL", report["comparisons"], "retained executable-section/relocation comparisons")
    raise SystemExit(0 if report["passed"] else 1)


if __name__ == "__main__":
    main()
