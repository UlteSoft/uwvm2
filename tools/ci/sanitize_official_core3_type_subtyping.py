#!/usr/bin/env python3
"""Bypass one wasm-tools text parser limitation before WAST JSON conversion.

The pinned Core 3 testsuite's type-subtyping.wast contains an assert_invalid
module with two supertypes. The Core 3 binary grammar can encode this invalid
subtype, but wasm-tools 1.259 rejects its WAT text before converting the rest
of the script. An equivalent hand-encoded binary is tested separately by
run_official_core3_type_subtyping_delta.py. Keep the original and its hash.
"""

import hashlib
import json
import sys
from pathlib import Path


ASSERTION = """(assert_invalid
  (module
    (type $parent1 (sub (struct)))
    (type $parent2 (sub (struct)))
    (type $child (sub $parent1 $parent2 (struct))))
  "multiple supertypes"
)
"""


def digest(data):
    return hashlib.sha256(data).hexdigest()


def main():
    source, target, manifest = map(Path, sys.argv[1:4])
    original = source.read_bytes()
    marker = ASSERTION.encode()
    if original.count(marker) != 1:
        raise SystemExit("expected exactly one multi-supertype text assertion")
    sanitized = original.replace(marker, b"", 1)
    target.write_bytes(sanitized)
    manifest.write_text(json.dumps({
        "source": str(source),
        "source_sha256": digest(original),
        "sanitized": str(target),
        "sanitized_sha256": digest(sanitized),
        "removed_assertion_sha256": digest(marker),
        "removed_assertion": ASSERTION,
        "reason": "wasm-tools 1.259 json-from-wast rejects this invalid WAT before converting later binary tests; an equivalent binary with two supertype indices is tested separately.",
    }, indent=2) + "\n")


if __name__ == "__main__":
    main()
