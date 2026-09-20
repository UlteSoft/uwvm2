#!/usr/bin/env python3
"""Regression tests for the lightweight header/module dependency checkers."""

from __future__ import annotations

import importlib.util
import contextlib
import io
import sys
import tempfile
import unittest
from pathlib import Path
from types import ModuleType


THIS_DIR = Path(__file__).resolve().parent


def load_checker(module_name: str, filename: str) -> ModuleType:
    spec = importlib.util.spec_from_file_location(module_name, THIS_DIR / filename)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load checker: {filename}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    spec.loader.exec_module(module)
    return module


PRAGMA_CHECKER = load_checker("uwvm_test_pragma_once_guard", "check_pragma_once_guard.py")
MODULE_CHECKER = load_checker("uwvm_test_module_dependencies", "check_uwvm_module.py")


def module_dependencies(text: str) -> list[str]:
    return MODULE_CHECKER.extract_imports_from_cppm_or_module_cpp(
        text
    ) + MODULE_CHECKER.extract_global_fragment_includes(text)


def guarded_dependencies(text: str) -> list[str]:
    return MODULE_CHECKER.extract_guarded_includes(text)


class PragmaOnceGuardTests(unittest.TestCase):
    def check_temporary_header(self, text: str) -> str | None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "surface.h"
            path.write_text(text, encoding="utf-8")
            return PRAGMA_CHECKER.check_header(str(path))

    def test_dual_surface_include_before_guard_is_reported(self) -> None:
        message = self.check_temporary_header(
            """\
#pragma once
#include <uwvm2/utils/container/impl.h>
#ifndef UWVM_MODULE
#endif
"""
        )

        self.assertIsNotNone(message)
        self.assertIn(":2:", message)
        self.assertIn("escapes", message)

    def test_split_helper_without_module_guard_is_out_of_scope(self) -> None:
        message = self.check_temporary_header(
            """\
#pragma once
#include "native_unwind_platform.h"
"""
        )

        self.assertIsNone(message)


class ModuleDependencyTests(unittest.TestCase):
    def test_implementation_coroutine_header_is_checked_by_main(self) -> None:
        previous_root = MODULE_CHECKER.SRC_ROOT
        previous_repo = MODULE_CHECKER.REPO_ROOT
        try:
            with tempfile.TemporaryDirectory() as directory:
                MODULE_CHECKER.SRC_ROOT = MODULE_CHECKER.REPO_ROOT = directory
                source = Path(directory) / 'entry.module.cpp'
                (Path(directory) / 'entry.default.cpp').write_text(
                    '#ifndef UWVM_MODULE\n#include <coroutine>\n#endif\n')
                for supplied, status in ((False, 1), (True, 0)):
                    source.write_text(('#include <coroutine>\n' if supplied else '') +
                                      'import task;\n#include "entry.default.cpp"\n')
                    output = io.StringIO()
                    with contextlib.redirect_stdout(output):
                        self.assertEqual(MODULE_CHECKER.main(), status)
                    self.assertEqual('[STANDARD HEADER]' in output.getvalue(), not supplied)
        finally:
            MODULE_CHECKER.SRC_ROOT = previous_root
            MODULE_CHECKER.REPO_ROOT = previous_repo

    def test_standalone_api_and_simd_headers_require_their_modules(self) -> None:
        for header, owner in MODULE_CHECKER.STANDALONE_MODULE_HEADERS.items():
            with self.subTest(header=header):
                required = guarded_dependencies(f"#ifndef UWVM_MODULE\n#include <{header}>\n#endif\n")
                self.assertEqual(required, [owner])
                self.assertFalse(MODULE_CHECKER.compare_dependency_coverage([], required)[0])
                self.assertTrue(MODULE_CHECKER.compare_dependency_coverage([owner], required)[0])

    def test_runtime_api_cannot_be_textual_in_consumer_global_fragment(self) -> None:
        for quote in ('"', '<'):
            close = '"' if quote == '"' else '>'
            with self.subTest(quote=quote):
                text = f"module;\n#include {quote}{MODULE_CHECKER.RUNTIME_API_HEADER}{close}\nexport module test;\n"
                self.assertEqual(MODULE_CHECKER.find_textual_runtime_api_in_global_fragment(text), [2])

    def test_runtime_owner_header_and_consumer_import_are_allowed(self) -> None:
        for text in (
            'module;\nexport module uwvm2.runtime;\n#include "uwvm_runtime.h"\n',
            'module;\nexport module test;\nimport uwvm2.runtime;\n',
        ):
            self.assertEqual(MODULE_CHECKER.find_textual_runtime_api_in_global_fragment(text), [])

    def test_runtime_api_guard_ignores_comments_and_raw_strings(self) -> None:
        include = f"#include <{MODULE_CHECKER.RUNTIME_API_HEADER}>\n"
        text = 'module;\n/*\n' + include + '*/\nR"tag(\n' + include + ')tag";\nexport module test;\n'
        self.assertEqual(MODULE_CHECKER.find_textual_runtime_api_in_global_fragment(text), [])

    def test_standard_headers_missing_from_global_fragment_are_reported(self) -> None:
        self.assertEqual(
            MODULE_CHECKER.missing_standard_headers(
                "module;\n#include <cstddef>\nexport module test;\n",
                "#ifndef UWVM_MODULE\n#include <cstddef>\n#include <cstring>\n"
                "#include <concepts>\n#endif\n",
            ),
            ["concepts", "cstring"],
        )

    def test_standard_headers_in_global_fragment_are_sufficient(self) -> None:
        self.assertEqual(
            MODULE_CHECKER.missing_standard_headers(
                "module;\n#include <cstring>\n#include <memory>\nexport module test;\n",
                "#include <cstring>\n#include <cstring>\n",
            ),
            [],
        )

    def test_standard_header_after_named_module_is_not_global(self) -> None:
        self.assertEqual(
            MODULE_CHECKER.missing_standard_headers(
                "module;\nexport module test;\n#include <cstring>\n",
                "#include <cstring>\n",
            ),
            ["cstring"],
        )

    def test_standard_header_named_import_does_not_satisfy_project_policy(self) -> None:
        self.assertEqual(
            MODULE_CHECKER.missing_standard_headers(
                "export module test;\nimport std;\nimport fast_io;\n",
                "#include <cstring>\n",
            ),
            ["cstring"],
        )

    def test_standard_header_scan_ignores_comments_and_raw_strings(self) -> None:
        noise = '/*\n#include <bit>\n*/\nR"tag(\n#include <limits>\n)tag";\n'
        self.assertEqual(MODULE_CHECKER.missing_standard_headers("", noise), [])
        self.assertEqual(
            MODULE_CHECKER.missing_standard_headers(
                "module;\n" + noise + "export module test;\n", "#include <bit>\n"
            ),
            ["bit"],
        )

    def test_standard_header_scan_ignores_project_and_platform_headers(self) -> None:
        self.assertEqual(
            MODULE_CHECKER.missing_standard_headers(
                "", '#include <sys/mman.h>\n#include <fast_io.h>\n#include "memory"\n'
            ),
            [],
        )

    def test_standard_header_continuations_and_target_branches(self) -> None:
        self.assertEqual(
            MODULE_CHECKER.missing_standard_headers(
                "module;\n#if TARGET\n# include \\\n<cstring>\n#endif\nexport module test;\n",
                "#if TARGET\n#include <cstring>\n#else\n#include <bit>\n#endif\n",
            ),
            ["bit"],
        )

    def test_conditional_export_import_is_reported(self) -> None:
        module_text = """\
export module uwvm2.example;
#if defined(UWVM_RUNTIME_LLVM_JIT)
export import :runtime_aot;
#endif
"""

        self.assertEqual(
            MODULE_CHECKER.find_conditionally_exported_imports(module_text),
            [(3, ":runtime_aot")],
        )

    def test_backend_guard_after_unconditional_import_is_legal(self) -> None:
        module_text = """\
export module uwvm2.example;
export import :runtime_aot;
#if defined(UWVM_RUNTIME_LLVM_JIT)
int backend_declaration;
#endif
"""

        self.assertEqual(
            MODULE_CHECKER.find_conditionally_exported_imports(module_text), []
        )

    def test_frontend_module_partition_removal_is_reported(self) -> None:
        xmake_text = """\
add_files("src/uwvm2/uwvm/**.cppm")
remove_files("src/uwvm2/uwvm/cmdline/params/runtime_aot.cppm")
"""

        self.assertEqual(
            MODULE_CHECKER.find_frontend_module_removals(xmake_text), [2]
        )

    def test_backend_config_guard_is_recognized(self) -> None:
        header_text = """\
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
int backend_declaration;
#endif
"""

        self.assertEqual(
            MODULE_CHECKER.find_backend_config_guards(header_text),
            ["UWVM_RUNTIME_UWVM_INTERPRETER"],
        )

    def test_runtime_config_push_in_global_fragment_is_legal(self) -> None:
        module_text = """\
module;
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
#if defined(UWVM_RUNTIME_LLVM_JIT)
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#endif
export module uwvm2.example;
"""

        self.assertTrue(
            MODULE_CHECKER.runtime_config_push_precedes_backend_guards(module_text)
        )

    def test_runtime_config_push_after_backend_guard_is_rejected(self) -> None:
        module_text = """\
module;
#if defined(UWVM_RUNTIME_LLVM_JIT)
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#endif
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
export module uwvm2.example;
"""

        self.assertFalse(
            MODULE_CHECKER.runtime_config_push_precedes_backend_guards(module_text)
        )

    def test_feature_query_without_own_provider_is_rejected(self) -> None:
        module_text = """\
export module uwvm2.example;
import uwvm2.utils.macro;
#include "surface.h"
"""
        for feature_macro in ("UWVM_HAS_BUILTIN", "UWVM_HAS_ATTRIBUTE", "UWVM_HAS_CPP_ATTRIBUTE"):
            with self.subTest(feature_macro=feature_macro):
                problem = MODULE_CHECKER.feature_macro_dependency_problem(
                    module_text, f"#if {feature_macro}(feature)\n#endif\n"
                )
                self.assertIsNotNone(problem)
                self.assertIn(feature_macro, problem)

    def test_feature_provider_in_global_fragment_is_legal(self) -> None:
        module_text = """\
module;
#include <uwvm2/utils/macro/push_macros.h>
export module uwvm2.example;
#include "surface.h"
"""
        self.assertIsNone(MODULE_CHECKER.feature_macro_dependency_problem(
            module_text, "#if UWVM_HAS_BUILTIN(__builtin_available)\n#endif\n"
        ))

    def test_runtime_backend_provider_does_not_define_feature_queries(self) -> None:
        module_text = """\
module;
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
export module uwvm2.example;
"""
        self.assertFalse(MODULE_CHECKER.feature_config_push_precedes_tests(module_text))

    def test_feature_provider_after_module_declaration_is_rejected(self) -> None:
        module_text = """\
module;
export module uwvm2.example;
#include <uwvm2/utils/macro/push_macros.h>
"""
        self.assertFalse(MODULE_CHECKER.feature_config_push_precedes_tests(module_text))

    def test_feature_provider_must_precede_platform_test(self) -> None:
        module_text = """\
module;
#if UWVM_HAS_BUILTIN(__builtin_available)
#include <unistd.h>
#endif
#include <uwvm2/utils/macro/push_macros.h>
export module uwvm2.example;
"""
        self.assertFalse(MODULE_CHECKER.feature_config_push_precedes_tests(module_text))

    def test_conditionally_included_feature_provider_is_rejected(self) -> None:
        module_text = """\
module;
#if defined(__APPLE__)
#include <uwvm2/utils/macro/push_macros.h>
#endif
export module uwvm2.example;
"""
        self.assertFalse(MODULE_CHECKER.feature_config_push_precedes_tests(module_text))

    def test_nonmodule_include_block_feature_queries_are_ignored(self) -> None:
        header_text = """\
#ifndef UWVM_MODULE
#include <uwvm2/utils/macro/push_macros.h>
#if UWVM_HAS_BUILTIN(__builtin_alloca)
#include <alloca.h>
#endif
#endif
"""
        self.assertEqual(MODULE_CHECKER.find_feature_macro_tests(header_text), [])
        self.assertIsNone(MODULE_CHECKER.feature_macro_dependency_problem(
            "export module uwvm2.example;\n", header_text
        ))

    def test_module_else_branch_feature_query_is_checked(self) -> None:
        header_text = """\
#if !defined(UWVM_MODULE)
#if UWVM_HAS_BUILTIN(nonmodule_only)
#endif
#else
#if UWVM_HAS_CPP_ATTRIBUTE(maybe_unused)
#endif
#endif
"""
        self.assertEqual(MODULE_CHECKER.find_feature_macro_tests(header_text), ["UWVM_HAS_CPP_ATTRIBUTE"])

    def test_nonmodule_else_branch_feature_query_is_ignored(self) -> None:
        header_text = """\
#ifdef UWVM_MODULE
#else
#if UWVM_HAS_BUILTIN(nonmodule_only)
#endif
#endif
"""
        self.assertEqual(MODULE_CHECKER.find_feature_macro_tests(header_text), [])

    def test_feature_query_continuation_and_elif_are_recognized(self) -> None:
        header_text = (
            "#if defined(__APPLE__) && \\\n"
            "    UWVM_HAS_BUILTIN(__builtin_available)\n"
            "#elif UWVM_HAS_ATTRIBUTE(noreturn)\n"
            "#endif\n"
        )
        self.assertEqual(MODULE_CHECKER.find_feature_macro_tests(header_text), ["UWVM_HAS_BUILTIN", "UWVM_HAS_ATTRIBUTE"])

    def test_feature_documentation_definitions_and_literals_are_ignored(self) -> None:
        header_text = '''\
/*
#if UWVM_HAS_BUILTIN(commented_out)
*/
// #if UWVM_HAS_BUILTIN(commented_out)
#define UWVM_HAS_BUILTIN(...) 0
char const* example = "UWVM_HAS_CPP_ATTRIBUTE(maybe_unused)";
char const* raw_example = R"example(
#if UWVM_HAS_ATTRIBUTE(documented_only)
)example";
#if defined(__APPLE__) // UWVM_HAS_BUILTIN(commented_out)
#endif
'''
        self.assertEqual(MODULE_CHECKER.find_feature_macro_tests(header_text), [])

    def test_actual_madvise_partition_has_its_own_feature_provider(self) -> None:
        source_dir = THIS_DIR.parent.parent / "src/uwvm2/utils/madvise"
        module_text = (source_dir / "madvise.cppm").read_text(encoding="utf-8")
        header_text = (source_dir / "madvise.h").read_text(encoding="utf-8")
        self.assertIn("UWVM_HAS_BUILTIN", MODULE_CHECKER.find_feature_macro_tests(header_text))
        self.assertIsNone(MODULE_CHECKER.feature_macro_dependency_problem(module_text, header_text))

    def test_additional_direct_module_import_is_legal(self) -> None:
        module_text = """\
export module uwvm2.example;
import uwvm2.utils.container;
import uwvm2.parser;
"""
        header_text = """\
#pragma once
#ifndef UWVM_MODULE
#include <uwvm2/utils/container/impl.h>
#endif
"""

        ok, differences = MODULE_CHECKER.compare_dependency_coverage(
            module_dependencies(module_text), guarded_dependencies(header_text)
        )

        self.assertTrue(ok)
        self.assertEqual(differences, [])

    def test_header_dependency_missing_from_module_is_reported(self) -> None:
        module_text = "export module uwvm2.example;\n"
        header_text = """\
#pragma once
#ifndef UWVM_MODULE
#include <uwvm2/utils/container/impl.h>
#endif
"""

        ok, differences = MODULE_CHECKER.compare_dependency_coverage(
            module_dependencies(module_text), guarded_dependencies(header_text)
        )

        self.assertFalse(ok)
        self.assertEqual(
            differences,
            ["Header dependencies missing from module unit: uwvm2.utils.container"],
        )

    def test_global_module_fragment_include_satisfies_header_dependency(self) -> None:
        module_text = """\
module;
#include <uwvm2/utils/container/impl.h>
export module uwvm2.example;
"""
        header_text = """\
#pragma once
#ifndef UWVM_MODULE
#include <uwvm2/utils/container/impl.h>
#endif
"""

        ok, differences = MODULE_CHECKER.compare_dependency_coverage(
            module_dependencies(module_text), guarded_dependencies(header_text)
        )

        self.assertTrue(ok)
        self.assertEqual(differences, [])

    def test_quoted_errno_system_header_is_not_a_partition(self) -> None:
        self.assertIsNone(
            MODULE_CHECKER.normalize_header_to_import_name("errno.h", is_local=True)
        )

    def test_existing_quoted_header_is_a_local_partition(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "surface.h"
            (Path(directory) / "define.h").write_text("#pragma once\n", encoding="utf-8")
            source.write_text(
                """\
#pragma once
#ifndef UWVM_MODULE
#include "define.h"
#endif
""",
                encoding="utf-8",
            )

            dependencies = MODULE_CHECKER.extract_guarded_includes(
                source.read_text(encoding="utf-8"), source_path=str(source)
            )

        self.assertEqual(dependencies, [":define"])

    def test_import_with_trailing_comment_is_recognized(self) -> None:
        imports = MODULE_CHECKER.extract_imports_from_cppm_or_module_cpp(
            "import uwvm2.utils.container; // dependency rationale\n"
        )

        self.assertEqual(imports, ["uwvm2.utils.container"])

    def test_win32_color_provider_must_precede_textual_surface(self) -> None:
        surface_text = """\
#include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_push_macro.h>
inline void report() { use(UWVM_COLOR_RED); }
"""
        module_text = """\
export module uwvm2.example;
#include "surface.h"
import uwvm2.uwvm_predefine.utils.ansies;
"""

        self.assertFalse(
            MODULE_CHECKER.win32_text_attr_provider_precedes_surface(
                module_text, surface_text, "surface.h"
            )
        )

    def test_win32_color_provider_before_textual_surface_is_legal(self) -> None:
        surface_text = """\
#include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_push_macro.h>
inline void report() { use(UWVM_COLOR_RED); }
"""
        module_text = """\
export module uwvm2.example;
import uwvm2.uwvm_predefine.utils.ansies;
#include "surface.h"
"""

        self.assertTrue(
            MODULE_CHECKER.win32_text_attr_provider_precedes_surface(
                module_text, surface_text, "surface.h"
            )
        )

    def test_win32_color_partition_is_not_an_external_provider(self) -> None:
        surface_text = """\
#include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_push_macro.h>
inline void report() { use(UWVM_COLOR_RED); }
"""
        module_text = """\
export module uwvm2.example;
import uwvm2.utils.ansies:win32_text_attr;
#include "surface.h"
"""

        # Partitions may only be imported by units of their owning named module;
        # ordinary consumers must import the primary module or a re-exporter.
        self.assertFalse(
            MODULE_CHECKER.win32_text_attr_provider_precedes_surface(
                module_text, surface_text, "surface.h"
            )
        )

    def test_low_level_text_attr_provider_cannot_satisfy_uwvm_color(self) -> None:
        surface_text = """\
#include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_push_macro.h>
inline void report() { use(UWVM_COLOR_RED); }
"""
        module_text = """\
export module uwvm2.example;
import uwvm2.utils.ansies;
#include "surface.h"
"""

        self.assertFalse(
            MODULE_CHECKER.win32_text_attr_provider_precedes_surface(
                module_text, surface_text, "surface.h"
            )
        )

    def test_low_level_provider_satisfies_direct_text_attr_macros(self) -> None:
        surface_text = """\
#include <uwvm2/utils/ansies/win32_text_attr_push_macro.h>
inline void report() { use(UWVM_WIN32_TEXTATTR_RED); }
"""
        module_text = """\
export module uwvm2.example;
import uwvm2.utils.ansies;
#include "surface.h"
"""

        self.assertTrue(
            MODULE_CHECKER.win32_text_attr_provider_precedes_surface(
                module_text, surface_text, "surface.h"
            )
        )

    def test_win32_color_documentation_without_macro_header_is_ignored(self) -> None:
        surface_text = "/// UWVM_COLOR_RED is documented here.\n"
        module_text = """\
export module uwvm2.example;
#include "surface.h"
"""

        self.assertTrue(
            MODULE_CHECKER.win32_text_attr_provider_precedes_surface(
                module_text, surface_text, "surface.h"
            )
        )


if __name__ == "__main__":
    unittest.main()
