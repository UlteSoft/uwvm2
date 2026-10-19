#!/usr/bin/env python3
"""Preserve base classes, removing only versioned module metadata for TeaVM."""
import argparse
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import stat
import zipfile


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def bounded_read(path, maximum):
    with path.open('rb') as stream:
        raw = stream.read(maximum + 1)
    if len(raw) > maximum:
        raise ValueError('file byte budget exceeded')
    return raw


def base_manifest(raw):
    if len(raw) > 65536:
        raise ValueError('manifest exceeds 64 KiB')
    text = raw.decode('utf-8').replace('\r\n', '\n')
    if '\r' in text:
        raise ValueError('bare CR in manifest')
    lines, main, skipping = [], True, False
    for line in text.split('\n'):
        if not line:
            main, skipping = False, False
        elif not line.startswith(' '):
            skipping = main and line.lower().startswith('multi-release:')
        if not skipping:
            lines.append(line)
    return '\r\n'.join(lines).encode()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--receipt', type=Path, required=True)
    args = parser.parse_args()
    original = args.input.resolve(strict=True)
    if len({original, args.out.resolve(), args.receipt.resolve()}) != 3:
        raise ValueError('three distinct paths required')
    before = bounded_read(original, 32 << 20)
    entries, names, classes, removed, total = [], set(), {}, [], 0
    with zipfile.ZipFile(io.BytesIO(before)) as source:
        for item in source.infolist():
            path = PurePosixPath(item.filename)
            if (not item.filename or path.is_absolute() or '..' in path.parts or
                    '\\' in item.filename or '\0' in item.filename or item.filename in names):
                raise ValueError('unsafe or duplicate JAR member')
            names.add(item.filename)
            total += item.file_size
            if len(names) > 10000 or total > 128 << 20 or item.flag_bits & 1:
                raise ValueError('bounded unencrypted JAR required')
            if stat.S_IFMT(item.external_attr >> 16) == stat.S_IFLNK:
                raise ValueError('JAR symlink is unsupported')
            upper = item.filename.upper()
            if upper.startswith('META-INF/') and upper.endswith(('.SF', '.RSA', '.DSA', '.EC')):
                raise ValueError('signed JAR must not be rewritten')
            if item.filename.startswith('META-INF/versions/'):
                if not item.is_dir() and not (len(path.parts) == 4 and path.parts[2].isdigit()
                                             and path.parts[3] == 'module-info.class'):
                    raise ValueError('versioned implementation/resource must not be silently lowered')
                removed.append(item.filename)
                continue
            raw = source.read(item)
            if item.filename == 'META-INF/MANIFEST.MF':
                raw = base_manifest(raw)
            elif item.filename.endswith('.class'):
                classes[item.filename] = sha(raw)
            entries.append((item, raw))
    # Validate before creating output. JAR code is never extracted or executed.
    with zipfile.ZipFile(args.out, 'x', compression=zipfile.ZIP_DEFLATED) as target:
        for item, raw in entries:
            target.writestr(item, raw)
    with zipfile.ZipFile(args.out) as target:
        actual = {name: sha(target.read(name)) for name in target.namelist() if name.endswith('.class')}
        if actual != classes:
            raise ValueError('base-class bytes changed')
    if bounded_read(original, 32 << 20) != before:
        raise ValueError('original JAR changed')
    result = {'scope': 'base classes byte-exact; only versioned module descriptors and Multi-Release declaration removed',
              'original': {'path': str(original), 'bytes': len(before), 'sha256': sha(before)},
              'filtered': {'path': str(args.out.resolve()), 'bytes': args.out.stat().st_size,
                           'sha256': sha(bounded_read(args.out, 128 << 20))},
              'removed': removed, 'base_classes': len(classes), 'base_classes_byte_exact': True,
              'no_archive_code_executed': True}
    with args.receipt.open('x') as stream:
        stream.write(json.dumps(result, indent=2) + '\n')
    print(json.dumps({'base_classes': len(classes), 'removed_module_entries': len(removed),
                      'filtered_sha256': result['filtered']['sha256']}))


if __name__ == '__main__':
    main()
