#!/usr/bin/env python3
"""Fingerprint the source inputs of the isolated Wasm 3 regression build.

The remote test copy has no Git metadata. Use a content digest of every source
and bundled dependency, never a fabricated Git revision or an unsafe cache flag.
Call before and after compilation, and reject a changed digest.
"""
import hashlib
import json
from pathlib import Path
import sys

root = Path(sys.argv[1]).resolve()
entries = []
for directory in ('src', 'third-parties'):
    directory_path = root / directory
    if not directory_path.is_dir():
        raise SystemExit(f'missing source/dependency directory: {directory_path}')
    # Isolated builds may symlink third-parties to one immutable shared tree.
    # Resolve the directory root explicitly and hash the target's file bytes
    # under stable repository-relative paths, independent of its mount path.
    resolved_directory = directory_path.resolve(strict=True)
    for path in sorted(resolved_directory.rglob('*')):
        # Finder metadata is not a source or bundled dependency. Its presence
        # depends on whether the tree is on macOS or Linux and must not change
        # the identity of otherwise byte-identical build inputs.
        if path.is_file() and path.name != '.DS_Store' and not path.name.startswith('._'):
            digest = hashlib.sha256(path.read_bytes()).hexdigest()
            relative = Path(directory) / path.relative_to(resolved_directory)
            entries.append({'path': relative.as_posix(), 'sha256': digest})
if not entries:
    raise SystemExit('missing source inputs')
payload = json.dumps(entries, sort_keys=True, separators=(',', ':')).encode()
source_id = 'sha256:' + hashlib.sha256(payload).hexdigest()
Path(sys.argv[2]).write_text(json.dumps({'source_id': source_id, 'files': entries}, indent=2) + '\n')
print(source_id)
