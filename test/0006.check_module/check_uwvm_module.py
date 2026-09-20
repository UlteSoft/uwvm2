#!/usr/bin/env python3
"""
Check UWVM_MODULE imports across the src tree.

Rules:
- For each pair (.cppm, .h) in the same directory with the same basename,
  every module-correlated dependency included by the header must be imported
  by the module unit or included in its global module fragment.
- For each pair (*.module.cpp, *.default.cpp) in the same directory with the
  same basename, apply the same dependency-coverage and std-header rules.
- Additional explicit module imports are permitted. Named modules do not see
  dependencies that a textual header obtained transitively, so module units
  may legitimately need a larger direct dependency surface.
- A surface that expands UWVM Win32 macros must import the matching provider
  before it is included: the high-level color-policy module for UWVM_COLOR,
  or the low-level text-attribute module for direct UWVM_WIN32_TEXTATTR use.
- Exported named-module imports must not be hidden behind conditional
  preprocessing. Keep the dependency graph stable and guard declarations in
  the paired implementation header instead.
- A module unit whose paired header guards declarations with a derived
  ``UWVM_RUNTIME_*`` backend macro must establish those macros in its own
  global module fragment. Named-module imports never propagate macros.
- Function-like ``UWVM_HAS_*`` feature tests visible in module mode need the
  utils macro provider in that unit's global fragment, before any such test.
- Project self-containment policy: standard C++ headers explicitly included by
  a paired header must also occur in the global module fragment. Do not rely
  on another named module's transitive standard-library declarations.
- xmake must not remove frontend module partitions selected by that stable
  dependency graph.
- Header include order must respect category sequence:
  1) uwvm_predefine/*
  2) project utils/*
  3) parser/*
  4) uwvm/io/*
  5) uwvm/utils/*
  After these categories, the order of any remaining headers is not constrained.
- The category-order style rule applies only to dual-surface headers.
  A *.default.cpp file is an implementation translation unit whose textual
  include order follows implementation dependencies; it is still checked for
  dependency coverage against its *.module.cpp peer.

Notes:
- In .cppm/.module.cpp, we read `import ...;` lines and module-correlated
  includes in the global module fragment.
- The runtime API and shared SIMD standalone headers also map to named modules.
  Runtime API consumers must import its owner, not include it in a global fragment.
- In .h/.default.cpp, we read `#include <.../impl.h>`, mapped standalone headers,
  `#include <fast_io.h>`, and relative `#include "*.h"`
  within the `#ifndef UWVM_MODULE` guarded region, ignoring other includes (e.g., macros, std headers).
- We normalize imports to canonical names so that, for example:
  `#include <uwvm2/utils/container/impl.h>` == `import uwvm2.utils.container;`
  `#include "def.h"` == `import :def;`
  `#include <fast_io.h>` == `import fast_io;`
"""

from __future__ import annotations

import os
import re
import sys
from dataclasses import dataclass
from typing import Dict, List, Tuple, Iterable


# Root directories
THIS_DIR = os.path.abspath(os.path.dirname(__file__))
SRC_ROOT = os.path.abspath(os.path.join(THIS_DIR, "..", "..", "src"))
REPO_ROOT = os.path.abspath(os.path.join(THIS_DIR, "..", ".."))


IMPORT_LINE_RE = re.compile(r"^\s*(?:export\s+)?import\s+([^;\s]+)\s*;\s*(?://.*)?$")
EXPORTED_IMPORT_LINE_RE = re.compile(r"^\s*export\s+import\s+([^;\s]+)\s*;\s*(?://.*)?$")
MODULE_NAME_RE = re.compile(r"^\s*export\s+module\s+([^;\s]+)\s*;\s*$", re.MULTILINE)
INCLUDE_RE = re.compile(r"^\s*#\s*include\s*([<\"])([^>\"]+)[>\"]")
PP_CONDITIONAL_START_RE = re.compile(r"^\s*#\s*(?:if|ifdef|ifndef)\b")
PP_CONDITIONAL_END_RE = re.compile(r"^\s*#\s*endif\b")
FRONTEND_MODULE_REMOVE_RE = re.compile(
    r"\bremove_files\s*\(\s*['\"]src/uwvm2/uwvm/"
)
GLOBAL_MODULE_FRAGMENT_RE = re.compile(r"^\s*module\s*;\s*(?://.*)?$")
NAMED_MODULE_DECL_RE = re.compile(r"^\s*(?:export\s+)?module\s+[^;]+;")
UWVM_COLOR_MACRO_USE_RE = re.compile(r"\bUWVM_COLOR_[A-Z0-9_]+\b")
UWVM_COLOR_MACRO_HEADER_RE = re.compile(r"^\s*#\s*include\s*[<\"][^>\"]*uwvm_color_push_macro\.h[>\"]", re.MULTILINE)
WIN32_TEXT_ATTR_MACRO_USE_RE = re.compile(r"\bUWVM_WIN32_TEXTATTR_[A-Z0-9_]+\b")
WIN32_TEXT_ATTR_MACRO_HEADER_RE = re.compile(r"^\s*#\s*include\s*[<\"][^>\"]*win32_text_attr_push_macro\.h[>\"]", re.MULTILINE)
BACKEND_CONFIG_GUARD_RE = re.compile(
    r"^\s*#\s*(?:if|ifdef|ifndef)\b[^\n]*\b(UWVM_RUNTIME_(?:UWVM_INTERPRETER|LLVM_JIT|HAS_BACKEND))\b",
    re.MULTILINE,
)
RUNTIME_CONFIG_PUSH_HEADER = "uwvm2/uwvm/runtime/macro/push_macros.h"
RUNTIME_API_HEADER = "uwvm2/runtime/lib/uwvm_runtime.h"
# These standalone public interfaces do not follow the usual */impl.h naming.
# Omitting them hid real run/API and interpreter SIMD dependency failures.
STANDALONE_MODULE_HEADERS = {
    RUNTIME_API_HEADER: "uwvm2.runtime",
    "uwvm2/runtime/compiler/shared/wasm1p1_simd.h": "uwvm2.runtime.compiler.shared.wasm1p1_simd",
}
FEATURE_CONFIG_PUSH_HEADER = "uwvm2/utils/macro/push_macros.h"
STANDARD_CXX_HEADERS = frozenset(
    "algorithm any array atomic barrier bit bitset cassert ccomplex cctype cerrno "
    "cfenv cfloat charconv chrono cinttypes ciso646 climits clocale cmath codecvt "
    "compare complex concepts condition_variable coroutine csetjmp csignal "
    "cstdalign cstdarg cstdbool cstddef cstdint cstdio cstdlib cstring ctgmath "
    "ctime cuchar cwchar cwctype deque exception execution expected filesystem "
    "flat_map flat_set format forward_list fstream functional future generator "
    "hazard_pointer hive initializer_list inplace_vector iomanip ios iosfwd "
    "iostream istream iterator latch limits linalg list locale map mdspan memory "
    "memory_resource mutex new numbers numeric optional ostream print queue "
    "random ranges ratio rcu regex scoped_allocator semaphore set shared_mutex "
    "source_location span spanstream sstream stack stacktrace stdexcept "
    "stdfloat stop_token streambuf string string_view strstream syncstream "
    "system_error text_encoding thread tuple type_traits typeindex typeinfo "
    "unordered_map unordered_set utility valarray variant vector version".split()
)
FEATURE_TEST_RE = re.compile(r"\b(UWVM_HAS_(?:BUILTIN|ATTRIBUTE|CPP_ATTRIBUTE))\s*\(")
PP_DIRECTIVE_RE = re.compile(r"^\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)")
CXX_COMMENT_OR_STRING_RE = re.compile(
    r'R"(?P<raw_delimiter>[^\s()\\]{0,16})\([\s\S]*?\)(?P=raw_delimiter)"'
    r'|//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\''
)
WIN32_TEXT_ATTR_PROVIDER_MODULES = frozenset(
    {
        "uwvm2.utils.ansies",
        "uwvm2.uwvm_predefine.utils.ansies",
        "uwvm2.uwvm.utils.ansies",
    }
)
UWVM_COLOR_PROVIDER_MODULES = frozenset(
    {
        "uwvm2.uwvm_predefine.utils.ansies",
        "uwvm2.uwvm.utils.ansies",
    }
)


@dataclass
class Pair:
    a: str
    b: str


def debug(msg: str) -> None:
    # Toggle for verbose debugging if needed
    # print(f"[DEBUG] {msg}")
    pass


def is_cppm(path: str) -> bool:
    return path.endswith(".cppm")


def is_header(path: str) -> bool:
    return path.endswith(".h") and not path.endswith("impl.h")


def is_default_cpp(path: str) -> bool:
    return path.endswith(".default.cpp")


def is_module_cpp(path: str) -> bool:
    return path.endswith(".module.cpp")


def read_text(path: str) -> str:
    with open(path, "r", encoding="utf-8") as f:
        return f.read()


def list_files(root: str) -> List[str]:
    paths: List[str] = []
    for dirpath, _, filenames in os.walk(root):
        for fn in filenames:
            if fn.startswith("."):
                continue
            paths.append(os.path.join(dirpath, fn))
    return paths


def normalize_header_to_import_name(
    header: str,
    *,
    is_local: bool,
    local_header_exists: bool = False,
) -> str | None:
    """Map header include path to canonical import name.

    Examples:
    - "uwvm2/utils/container/impl.h" -> "uwvm2.utils.container"
    - "uwvm2/uwvm/wasm/storage/impl.h" -> "uwvm2.uwvm.wasm.storage"
    - "fast_io.h" -> "fast_io"
    - "def.h" -> ":def" (relative header, treated as partition)

    Returns None for headers to be ignored (e.g., std headers, macro push/pop).
    """

    # Ignore known macro helpers and pop macros
    if "macro/push_macros.h" in header or "macro/pop_macros.h" in header:
        return None
    if "uwvm_color_push_macro.h" in header or "uwvm_color_pop_macro.h" in header:
        return None

    # Ignore STL and C headers
    if not ("/" in header or header.endswith(".h")):
        return None

    # Special-case fast_io
    if header == "fast_io_crypto.h":
        return "fast_io_crypto"
    if header == "fast_io.h" or header.startswith("fast_io_") or header.startswith("fast_io/"):
        return "fast_io"

    if header in STANDALONE_MODULE_HEADERS:
        return STANDALONE_MODULE_HEADERS[header]

    # Other conventional module-correlated headers.
    if header.endswith("/impl.h"):
        core = header[:-len("/impl.h")]
        return core.replace("/", ".")

    # Local relative header like "def.h" or nested like "types/abc.h"
    if is_local and local_header_exists and "/" not in header and header.endswith(".h"):
        base = header[:-2]
        return f":{base}"

    # Other tree headers (non-impl.h) are considered non-UWVM_MODULE imports
    return None


def extract_imports_from_cppm_or_module_cpp(text: str) -> List[str]:
    """Extract ordered list of canonical import names from .cppm or .module.cpp.

    - import uwvm2.utils.container; -> "uwvm2.utils.container"
    - import :def; -> ":def"
    - Duplicates are preserved to catch ordering mistakes; final comparison will dedupe in a stable way.
    """
    imports: List[str] = []
    for line in text.splitlines():
        m = IMPORT_LINE_RE.match(line)
        if not m:
            continue
        name = m.group(1).strip()
        # Filter out internal leader keywords (e.g., import module;) – not expected here
        if name == "module":
            continue
        imports.append(name)
    debug(f"imports-from-cppm/module: {imports}")
    return imports


def find_conditionally_exported_imports(text: str) -> List[Tuple[int, str]]:
    """Return exported named-module imports nested in a #if group.

    Module dependency discovery must not depend on backend macro state. A
    backend-specific partition remains importable when disabled; its paired
    header simply exports no backend declarations.
    """
    conditional_depth = 0
    findings: List[Tuple[int, str]] = []
    for line_number, line in enumerate(text.splitlines(), start=1):
        if PP_CONDITIONAL_START_RE.match(line):
            conditional_depth += 1
            continue
        if PP_CONDITIONAL_END_RE.match(line):
            conditional_depth = max(0, conditional_depth - 1)
            continue
        match = EXPORTED_IMPORT_LINE_RE.match(line)
        if conditional_depth and match is not None:
            findings.append((line_number, match.group(1).strip()))
    return findings


def find_frontend_module_removals(text: str) -> List[int]:
    """Return xmake lines that prune source units from the frontend module graph."""
    return [
        line_number
        for line_number, line in enumerate(text.splitlines(), start=1)
        if FRONTEND_MODULE_REMOVE_RE.search(line) is not None
    ]


def find_backend_config_guards(text: str) -> List[str]:
    """Return derived backend macros used by preprocessing conditionals."""
    return stable_unique(match.group(1) for match in BACKEND_CONFIG_GUARD_RE.finditer(text))


def runtime_config_push_precedes_backend_guards(text: str) -> bool:
    """Check the runtime macro setup inside a module's global fragment.

    ``UWVM_RUNTIME_*`` is derived from command-line selection macros by this
    header.  An import cannot supply that macro state, so the include must be
    before both the named-module declaration and any guarded platform include.
    """
    in_global_fragment = False
    push_line: int | None = None
    first_guard_line: int | None = None

    for line_number, line in enumerate(text.splitlines(), start=1):
        if not in_global_fragment:
            if GLOBAL_MODULE_FRAGMENT_RE.match(line):
                in_global_fragment = True
            continue
        if NAMED_MODULE_DECL_RE.match(line):
            break

        include_match = INCLUDE_RE.match(line)
        if (
            include_match is not None
            and include_match.group(2).strip() == RUNTIME_CONFIG_PUSH_HEADER
            and push_line is None
        ):
            push_line = line_number

        if BACKEND_CONFIG_GUARD_RE.match(line) is not None and first_guard_line is None:
            first_guard_line = line_number

    return push_line is not None and (
        first_guard_line is None or push_line < first_guard_line
    )


def preprocessing_lines(text: str) -> List[str]:
    """Join directive continuations and ignore comments, retaining include strings."""
    text = re.sub(r"\\\r?\n", "", text)
    text = CXX_COMMENT_OR_STRING_RE.sub(
        lambda match: re.sub(r"[^\n]", " ", match.group())
        if match.group().startswith(("//", "/*", 'R"')) else match.group(),
        text,
    )
    return text.splitlines()


def find_textual_runtime_api_in_global_fragment(text: str) -> List[int]:
    """Reject the API placement that hides module-owned configuration types.

    This narrow project rule does not ban arbitrary textual dependencies. The
    runtime API consumer must import uwvm2.runtime, as its implementation does;
    including the API before imports cannot see u8string_view and including its
    dependencies textually instead would mix global/named-module type ownership.
    The owner's own paired-header include after `export module` is intentional.
    """
    in_global_fragment = False
    findings: List[int] = []
    for line_number, line in enumerate(preprocessing_lines(text), start=1):
        if not in_global_fragment:
            if GLOBAL_MODULE_FRAGMENT_RE.match(line):
                in_global_fragment = True
            continue
        if NAMED_MODULE_DECL_RE.match(line):
            break
        include = INCLUDE_RE.match(line)
        if include is not None and include.group(2).strip() == RUNTIME_API_HEADER:
            findings.append(line_number)
    return findings


def find_feature_macro_tests(text: str) -> List[str]:
    """Find feature queries in #if/#elif expressions visible in module mode.

    This deliberately does not interpret arbitrary target conditionals. Only
    exact UWVM_MODULE guards are resolved, so feature checks in the header's
    non-module include block do not create spurious module requirements.
    Macro definitions, comments and ordinary string literals are not queries.
    """
    conditions: List[Tuple[bool | None, bool]] = []
    tests: List[str] = []
    for line in preprocessing_lines(text):
        directive = PP_DIRECTIVE_RE.match(line)
        if directive is None:
            continue
        kind, expression = directive.groups()
        expression = expression.strip()
        if kind in {"if", "ifdef", "ifndef"}:
            module_condition: bool | None = None
            if kind in {"ifdef", "ifndef"} and expression == "UWVM_MODULE":
                module_condition = kind == "ifdef"
            elif kind == "if":
                condition = re.fullmatch(r"(!?)\s*defined\s*(?:\(\s*UWVM_MODULE\s*\)|UWVM_MODULE)", expression)
                if condition is not None:
                    module_condition = not bool(condition.group(1))
            conditions.append((module_condition, module_condition is False))
        elif kind == "endif":
            if conditions:
                conditions.pop()
        elif kind in {"else", "elif"} and conditions:
            module_condition, _ = conditions[-1]
            conditions[-1] = (module_condition, module_condition is True)
        if kind in {"if", "elif"} and not any(disabled for _, disabled in conditions):
            tests.extend(FEATURE_TEST_RE.findall(expression))
    return stable_unique(tests)


def feature_config_push_precedes_tests(text: str) -> bool:
    """Require the real feature provider, not a named import or backend setup.

    Runtime push_macros.h derives UWVM_RUNTIME_* only; it does not define
    UWVM_HAS_*. The utils provider must be unconditional in this global
    fragment and must precede feature tests used for platform includes.
    """
    in_global_fragment = False
    conditional_depth = 0
    has_provider = False
    for line in preprocessing_lines(text):
        if not in_global_fragment:
            if GLOBAL_MODULE_FRAGMENT_RE.match(line):
                in_global_fragment = True
            continue
        if NAMED_MODULE_DECL_RE.match(line):
            break
        directive = PP_DIRECTIVE_RE.match(line)
        if directive is not None:
            kind, expression = directive.groups()
            if kind in {"if", "elif"} and FEATURE_TEST_RE.search(expression) and not has_provider:
                return False
            if kind in {"if", "ifdef", "ifndef"}:
                conditional_depth += 1
            elif kind == "endif":
                conditional_depth = max(0, conditional_depth - 1)
        include = INCLUDE_RE.match(line)
        if include is not None and include.group(2).strip() == FEATURE_CONFIG_PUSH_HEADER and conditional_depth == 0:
            has_provider = True
    return has_provider


def feature_macro_dependency_problem(module_text: str, surface_text: str) -> str | None:
    tests = stable_unique(find_feature_macro_tests(surface_text) + find_feature_macro_tests(module_text))
    if tests and not feature_config_push_precedes_tests(module_text):
        return (
            f"module-visible feature tests use {', '.join(tests)}; "
            f"include <{FEATURE_CONFIG_PUSH_HEADER}> unconditionally in the global module fragment before any feature test"
        )
    return None


def missing_standard_headers(module_text: str, header_text: str) -> List[str]:
    """Check explicit header parity, not arbitrary preprocessor conditions.

    This is a project self-containment rule, not a complete C++ visibility
    analysis. Includes from every target branch count; real target builds
    still verify their guards and the declarations actually used. A named
    module import is deliberately not a substitute for a direct std header.
    """
    expected: set[str] = set()
    provided: set[str] = set()
    for line in preprocessing_lines(header_text):
        match = INCLUDE_RE.match(line)
        if match and match.group(1) == "<" and match.group(2) in STANDARD_CXX_HEADERS:
            expected.add(match.group(2))

    in_global_fragment = False
    for line in preprocessing_lines(module_text):
        if not in_global_fragment:
            if GLOBAL_MODULE_FRAGMENT_RE.match(line):
                in_global_fragment = True
            continue
        if NAMED_MODULE_DECL_RE.match(line):
            break
        match = INCLUDE_RE.match(line)
        if match and match.group(1) == "<" and match.group(2) in STANDARD_CXX_HEADERS:
            provided.add(match.group(2))
    return sorted(expected - provided)


def win32_text_attr_provider_precedes_surface(
    module_text: str, surface_text: str, surface_filename: str
) -> bool:
    """Check that Win32 color manipulators are visible before a textual surface.

    In module builds the macro headers intentionally do not textually redeclare
    ``win32_text_attr``.  Direct text-attribute macros need its owning module;
    UWVM_COLOR also needs the high-level ``log_win32_use_ansi_b`` policy.  Both
    providers must be imported before the paired implementation surface.
    """
    needs_color_provider = (
        UWVM_COLOR_MACRO_HEADER_RE.search(surface_text) is not None
        and UWVM_COLOR_MACRO_USE_RE.search(surface_text) is not None
    )
    needs_text_attr_provider = (
        WIN32_TEXT_ATTR_MACRO_HEADER_RE.search(surface_text) is not None
        and WIN32_TEXT_ATTR_MACRO_USE_RE.search(surface_text) is not None
    )
    if not needs_color_provider and not needs_text_attr_provider:
        return True

    preceding_imports: set[str] = set()
    surface_line: int | None = None
    for line_number, line in enumerate(module_text.splitlines()):
        import_match = IMPORT_LINE_RE.match(line)
        if import_match is not None:
            preceding_imports.add(import_match.group(1).strip())

        include_match = INCLUDE_RE.match(line)
        if include_match is not None and include_match.group(1) == '"':
            header = include_match.group(2).strip()
            if header == surface_filename or header.endswith("/" + surface_filename):
                surface_line = line_number
                break

    if surface_line is None:
        return False
    if needs_color_provider and preceding_imports.isdisjoint(UWVM_COLOR_PROVIDER_MODULES):
        return False
    if needs_text_attr_provider and preceding_imports.isdisjoint(WIN32_TEXT_ATTR_PROVIDER_MODULES):
        return False
    return True


def local_header_exists(source_path: str | None, header: str) -> bool:
    """Return whether a quoted include resolves beside its source file."""
    return source_path is not None and os.path.isfile(os.path.join(os.path.dirname(source_path), header))


def extract_global_fragment_includes(text: str, *, source_path: str | None = None) -> List[str]:
    """Extract module-correlated textual dependencies from `module;`.

    A dependency mirrored textually in a module unit's global fragment is
    already available to that translation unit and must not be diagnosed as a
    missing named-module import.
    """
    includes: List[str] = []
    in_global_fragment = False
    for line in text.splitlines():
        if not in_global_fragment:
            if GLOBAL_MODULE_FRAGMENT_RE.match(line):
                in_global_fragment = True
            continue
        if NAMED_MODULE_DECL_RE.match(line):
            break
        match = INCLUDE_RE.match(line)
        if match is None:
            continue
        header = match.group(2).strip()
        is_local = match.group(1) == '"'
        norm = normalize_header_to_import_name(
            header,
            is_local=is_local,
            local_header_exists=is_local and local_header_exists(source_path, header),
        )
        if norm is not None:
            includes.append(norm)
    return includes


def extract_guarded_includes(
    text: str,
    *,
    base_module: str | None = None,
    include_local_partitions: bool = True,
    source_path: str | None = None,
) -> List[str]:
    """Extract ordered list of canonical import names from the #ifndef UWVM_MODULE region.

    Only keep includes that map to module-like imports:
    - <.../impl.h>
    - <fast_io.h>
    - "*.h" (local headers)
    """
    lines = text.splitlines()

    # Find the first #ifndef UWVM_MODULE and match until its corresponding #endif
    start_idx = None
    depth = 0
    for i, ln in enumerate(lines):
        if re.match(r"^\s*#\s*ifndef\s+UWVM_MODULE\b", ln):
            start_idx = i
            depth = 1
            break
    if start_idx is None:
        return []

    includes: List[str] = []
    i = start_idx + 1
    while i < len(lines) and depth > 0:
        ln = lines[i]
        if re.match(r"^\s*#\s*if(n?def|)\b", ln):
            depth += 1
        elif re.match(r"^\s*#\s*endif\b", ln):
            depth -= 1
            i += 1
            continue
        elif depth > 0:
            m = INCLUDE_RE.match(ln)
            if m:
                is_local = m.group(1) == '"'
                hdr = m.group(2).strip()
                resolved_local_header = is_local and local_header_exists(source_path, hdr)
                norm = None
                # Special mapping for relative submodule aggregator paths like "sub/impl.h"
                if resolved_local_header and base_module and (not ("/" in hdr and hdr.startswith("uwvm2/"))):
                    # relative include
                    if hdr.endswith("/impl.h") and "/" in hdr:
                        sub = hdr[: -len("/impl.h")]
                        # Map to fully-qualified dotted name using base module
                        norm = f"{base_module}.{sub.replace('/', '.')}"
                    elif hdr.endswith(".h") and "/" not in hdr:
                        # local partition header, map to partition
                        norm = f":{hdr[:-2]}"
                if norm is None and (include_local_partitions or not is_local):
                    norm = normalize_header_to_import_name(
                        hdr,
                        is_local=is_local,
                        local_header_exists=resolved_local_header,
                    )
                if norm is not None:
                    includes.append(norm)
        i += 1

    debug(f"includes-from-guard: {includes}")
    return includes


def stable_unique(seq: Iterable[str]) -> List[str]:
    seen = set()
    out: List[str] = []
    for x in seq:
        if x in seen:
            continue
        seen.add(x)
        out.append(x)
    return out


def _strip_prefix(name: str) -> str:
    return name[6:] if name.startswith("uwvm2.") else name


def _is_predefine(name: str) -> bool:
    n = _strip_prefix(name)
    return n.startswith("uwvm_predefine.")


def _is_utils(name: str) -> bool:
    n = _strip_prefix(name)
    # Exclude uwvm_predefine subtree entirely from utils detection, even though it contains "utils" segment
    if n.startswith("uwvm_predefine."):
        return False
    # Third-party fast_io imports conventionally lead many module dependency
    # lists and are not part of the project's internal utils ordering rule.
    return n.startswith("utils.")


def _is_parser(name: str) -> bool:
    n = _strip_prefix(name)
    return n.startswith("parser.")


def _is_uwvm_io(name: str) -> bool:
    n = _strip_prefix(name)
    return n == "uwvm.io" or n.startswith("uwvm.io.")


def _is_uwvm_utils(name: str) -> bool:
    n = _strip_prefix(name)
    return n == "uwvm.utils" or n.startswith("uwvm.utils.")


def check_order(seq: List[str]) -> Tuple[bool, str | None]:
    """Enforce category ordering constraints with a late utils window.

    Required relative order of blocks (if present):
      predefine -> utils(early) -> parser -> uwvm/io -> uwvm/utils -> utils(late)
    After that, the order of any remaining headers is unconstrained.
    Each block may be absent; constraints only apply when both sides exist.
    All early-utils must appear before the first parser; all parser before first uwvm/io; etc.
    """
    if not seq:
        return True, None

    # Build index lists
    idx_pre: List[int] = []
    idx_utils: List[int] = []
    idx_parser: List[int] = []
    idx_uio: List[int] = []
    idx_uutil: List[int] = []

    for i, name in enumerate(seq):
        if name.startswith(":"):
            continue  # local headers treated as unconstrained later
        if _is_predefine(name):
            idx_pre.append(i)
        elif _is_utils(name):
            idx_utils.append(i)
        elif _is_parser(name):
            idx_parser.append(i)
        elif _is_uwvm_io(name):
            idx_uio.append(i)
        elif _is_uwvm_utils(name):
            idx_uutil.append(i)

    pre0 = idx_pre[0] if idx_pre else None
    utils0 = idx_utils[0] if idx_utils else None
    parser0 = idx_parser[0] if idx_parser else None
    uio0 = idx_uio[0] if idx_uio else None
    uutil0 = idx_uutil[0] if idx_uutil else None

    # Partition utils into early and late based on uutil0 (if exists)
    early_utils = [i for i in idx_utils if uutil0 is None or i < uutil0]
    late_utils = [i for i in idx_utils if uutil0 is not None and i > uutil0]

    def fail(msg: str) -> Tuple[bool, str]:
        return False, msg

    # predefine < utils(early)
    if pre0 is not None and early_utils:
        if pre0 > min(early_utils):
            return fail(f"uwvm_predefine/* must appear before utils/* (found pre at {pre0}, utils at {min(early_utils)})")

    # utils(early) < parser
    if early_utils and parser0 is not None:
        if max(early_utils) > parser0:
            return fail(f"utils/* (early) must appear before parser/* (last early utils at {max(early_utils)}, parser at {parser0})")

    # parser < uwvm/io
    if parser0 is not None and uio0 is not None:
        if parser0 > uio0:
            return fail(f"parser/* must appear before uwvm/io/* (parser at {parser0}, uwvm/io at {uio0})")

    # uwvm/io < uwvm/utils
    if uio0 is not None and uutil0 is not None:
        if uio0 > uutil0:
            return fail(f"uwvm/io/* must appear before uwvm/utils/* (uwvm/io at {uio0}, uwvm/utils at {uutil0})")

    # uwvm/utils < utils(late) if both present
    if uutil0 is not None and late_utils:
        if uutil0 > min(late_utils):
            return fail(
                f"uwvm/utils/* must appear before late utils/* (uwvm/utils at {uutil0}, late utils at {min(late_utils)})"
            )

    return True, None


def compare_dependency_coverage(module_dependencies: List[str], header_includes: List[str]) -> Tuple[bool, List[str]]:
    """Require every header dependency in the corresponding module unit.

    Extra module imports are intentional and safe: module mode cannot rely on
    declarations exposed transitively by textual includes. Repeated imports or
    includes are ignored after their first occurrence.
    """
    dependencies = stable_unique(module_dependencies)
    includes = stable_unique(header_includes)
    missing = [name for name in includes if name not in dependencies]

    if not missing:
        return True, []

    return False, ["Header dependencies missing from module unit: " + ", ".join(missing)]


def find_cppm_h_pairs(files: List[str]) -> List[Pair]:
    pairs: List[Pair] = []
    # Group by directory + stem
    by_dir: Dict[Tuple[str, str], Dict[str, str]] = {}
    for p in files:
        d = os.path.dirname(p)
        base = os.path.basename(p)
        stem, ext = os.path.splitext(base)
        key = (d, stem)
        by_dir.setdefault(key, {})[ext] = p
    for (d, stem), mp in by_dir.items():
        if ".cppm" in mp and ".h" in mp:
            pairs.append(Pair(mp[".cppm"], mp[".h"]))
    return pairs


def is_impl_pair(pair: Pair) -> bool:
    return os.path.basename(pair.a) == "impl.cppm" and os.path.basename(pair.b) == "impl.h"


def extract_module_name(text: str) -> str | None:
    m = MODULE_NAME_RE.search(text)
    if m:
        return m.group(1).strip()
    return None


def find_module_default_pairs(files: List[str]) -> List[Pair]:
    pairs: List[Pair] = []
    by_dir: Dict[str, Dict[str, str]] = {}
    for p in files:
        if not (is_module_cpp(p) or is_default_cpp(p)):
            continue
        d = os.path.dirname(p)
        by_dir.setdefault(d, {})[os.path.basename(p)] = p

    for d, mp in by_dir.items():
        # Build basenames without suffix
        names = list(mp.keys())
        stems = set()
        for n in names:
            if n.endswith(".module.cpp"):
                stems.add(n[:-len(".module.cpp")])
            elif n.endswith(".default.cpp"):
                stems.add(n[:-len(".default.cpp")])
        for stem in stems:
            mod = os.path.join(d, stem + ".module.cpp")
            dfl = os.path.join(d, stem + ".default.cpp")
            if os.path.exists(mod) and os.path.exists(dfl):
                pairs.append(Pair(mod, dfl))
    return pairs


def main() -> int:
    print(f"Scanning src root: {SRC_ROOT}")
    files = list_files(SRC_ROOT)

    cppm_h_pairs = find_cppm_h_pairs(files)
    mod_def_pairs = find_module_default_pairs(files)

    problems: List[str] = []

    for path in files:
        if not is_cppm(path):
            continue
        for line_number in find_textual_runtime_api_in_global_fragment(read_text(path)):
            problems.append(
                f"[API OWNERSHIP] {path}:{line_number}: import uwvm2.runtime instead of "
                "including its API in the global module fragment"
            )
        for line_number, imported_name in find_conditionally_exported_imports(read_text(path)):
            problems.append(
                f"[CONDITIONAL IMPORT] {path}:{line_number}: export import {imported_name}; "
                "must stay unconditional; guard declarations in the partition header"
            )

    xmake_path = os.path.join(REPO_ROOT, "xmake.lua")
    if os.path.isfile(xmake_path):
        for line_number in find_frontend_module_removals(read_text(xmake_path)):
            problems.append(
                f"[MODULE PRUNING] {xmake_path}:{line_number}: frontend .cppm files "
                "must not be removed from the module dependency graph"
            )

    # Check .cppm <-> .h
    for pair in cppm_h_pairs:
        cppm_txt = read_text(pair.a)
        h_txt = read_text(pair.b)

        missing_std = missing_standard_headers(cppm_txt, h_txt)
        if missing_std:
            problems.append(
                f"[STANDARD HEADER] {pair.a}: global module fragment is missing "
                + ", ".join(f"<{header}>" for header in missing_std)
                + f" explicitly included by {pair.b}"
            )

        feature_problem = feature_macro_dependency_problem(cppm_txt, h_txt)
        if feature_problem is not None:
            problems.append(f"[FEATURE MACRO] {pair.a}: {feature_problem}")

        backend_guards = find_backend_config_guards(h_txt)
        if backend_guards and not runtime_config_push_precedes_backend_guards(cppm_txt):
            problems.append(
                f"[BACKEND MACRO] {pair.a}: paired header uses {', '.join(backend_guards)}, "
                f"but <{RUNTIME_CONFIG_PUSH_HEADER}> is missing from the global module fragment"
            )

        cppm_backend_guards = find_backend_config_guards(cppm_txt)
        if cppm_backend_guards and not runtime_config_push_precedes_backend_guards(cppm_txt):
            problems.append(
                f"[BACKEND MACRO] {pair.a}: global module fragment uses {', '.join(cppm_backend_guards)} "
                f"before including <{RUNTIME_CONFIG_PUSH_HEADER}>"
            )

        imports_cppm = extract_imports_from_cppm_or_module_cpp(cppm_txt)
        dependencies_cppm = imports_cppm + extract_global_fragment_includes(cppm_txt, source_path=pair.a)

        if is_impl_pair(pair):
            # Special logic for impl.cppm <-> impl.h
            base_module = extract_module_name(cppm_txt)
            if base_module is None:
                base_module = ""

            includes_h = extract_guarded_includes(h_txt, base_module=base_module, source_path=pair.b)
        else:
            includes_h = extract_guarded_includes(h_txt, source_path=pair.b)

        # Enforce category order only on header-side includes, not on module import lines
        ok_order_h, msg_h = check_order(includes_h)
        if not ok_order_h:
            problems.append(f"[ORDER] {pair.b}: {msg_h}")

        ok_eq, diffs = compare_dependency_coverage(dependencies_cppm, includes_h)
        if not ok_eq:
            problems.append(f"[MISMATCH] {pair.a} <-> {pair.b}")
            problems.extend(["  " + d for d in diffs])

        if not win32_text_attr_provider_precedes_surface(cppm_txt, h_txt, os.path.basename(pair.b)):
            problems.append(
                f"[ORDER] {pair.a}: the required Win32 macro provider must be imported before {os.path.basename(pair.b)}"
            )

    # Check *.module.cpp <-> *.default.cpp
    for pair in mod_def_pairs:
        mod_txt = read_text(pair.a)
        def_txt = read_text(pair.b)

        # Implementation TUs also define coroutines and instantiate templates;
        # an imported task/container type does not expose its std header.
        missing_std = missing_standard_headers("module;\n" + mod_txt, def_txt)
        if missing_std:
            problems.append(
                f"[STANDARD HEADER] {pair.a}: textual preamble is missing "
                + ", ".join(f"<{header}>" for header in missing_std)
                + f" explicitly included by {pair.b}"
            )

        imports_mod = extract_imports_from_cppm_or_module_cpp(mod_txt)
        dependencies_mod = imports_mod + extract_global_fragment_includes(mod_txt, source_path=pair.a)
        # Quoted helper headers in a *.default.cpp are textual implementation
        # details mirrored by includes in the module unit's global fragment;
        # they are not named-module imports or partitions.
        includes_def = extract_guarded_includes(def_txt, include_local_partitions=False, source_path=pair.b)

        ok_eq, diffs = compare_dependency_coverage(dependencies_mod, includes_def)
        if not ok_eq:
            problems.append(f"[MISMATCH] {pair.a} <-> {pair.b}")
            problems.extend(["  " + d for d in diffs])

        if not win32_text_attr_provider_precedes_surface(mod_txt, def_txt, os.path.basename(pair.b)):
            problems.append(
                f"[ORDER] {pair.a}: the required Win32 macro provider must be imported before {os.path.basename(pair.b)}"
            )

    if problems:
        print("Found issues:")
        for p in problems:
            print(p)
        return 1
    else:
        print("All UWVM_MODULE imports are consistent and ordered correctly.")
        return 0


if __name__ == "__main__":
    sys.exit(main())
