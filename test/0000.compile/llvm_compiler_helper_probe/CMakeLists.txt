# Configure-only regression for the actual LLVM helper-link probe patch.
# Run only through the Linux keeper's 64 GiB/swap-free build supervisor.
# A real target link is checked; target binaries are never executed.
# -DUWVM_LLVM_CONFIG_IX=<actual patched vendor config-ix.cmake> is mandatory.
cmake_minimum_required(VERSION 3.14)
project(uwvm_llvm_helper_link_probe LANGUAGES C)
include(CheckFunctionExists)
if(NOT DEFINED UWVM_LLVM_CONFIG_IX OR NOT EXISTS "${UWVM_LLVM_CONFIG_IX}")
  message(FATAL_ERROR "An actual patched LLVM config-ix.cmake is required")
endif()
file(READ "${UWVM_LLVM_CONFIG_IX}" uwvm_actual_config)
set(uwvm_function_begin "  function(uwvm_check_linkable_compiler_helper symbol result)")
set(uwvm_function_end "  endfunction()")
string(FIND "${uwvm_actual_config}" "${uwvm_function_begin}" uwvm_begin)
if(uwvm_begin LESS 0)
  message(FATAL_ERROR "The actual compiler-helper link function is missing")
endif()
string(SUBSTRING "${uwvm_actual_config}" ${uwvm_begin} -1 uwvm_tail)
string(FIND "${uwvm_tail}" "${uwvm_function_end}" uwvm_end)
if(uwvm_end LESS 0)
  message(FATAL_ERROR "The actual compiler-helper link function is truncated")
endif()
string(LENGTH "${uwvm_function_end}" uwvm_end_size)
math(EXPR uwvm_function_size "${uwvm_end} + ${uwvm_end_size}")
string(SUBSTRING "${uwvm_tail}" 0 ${uwvm_function_size} uwvm_function)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/actual-helper.cmake" "${uwvm_function}\n")
include("${CMAKE_CURRENT_BINARY_DIR}/actual-helper.cmake")

# Reproduce the old false-positive mechanism with the real CMake checker.
# The invented symbol has no definition and no target executable is run.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
check_function_exists(uwvm_deliberately_missing_link_probe_20261002 UWVM_COMPILE_ONLY_FALSE_POSITIVE)
if(NOT UWVM_COMPILE_ONLY_FALSE_POSITIVE)
  message(FATAL_ERROR "The compile-only control did not reproduce the expected unresolved-symbol archive")
endif()

# Existing HAVE_* cache values must not hide a disproved link result.
set(HAVE_UWVM_MISSING_HELPER 1 CACHE INTERNAL "Old compile-only false positive" FORCE)
uwvm_check_linkable_compiler_helper(uwvm_deliberately_missing_link_probe_20261002 HAVE_UWVM_MISSING_HELPER)
if(HAVE_UWVM_MISSING_HELPER OR UWVM_LINK_HAVE_UWVM_MISSING_HELPER)
  message(FATAL_ERROR "A nonexistent helper or stale HAVE cache was accepted")
endif()

# A negative result is meaningful only when the same toolchain can link libc.
set(HAVE_UWVM_PRESENT_HELPER 0 CACHE INTERNAL "Old false negative" FORCE)
uwvm_check_linkable_compiler_helper(malloc HAVE_UWVM_PRESENT_HELPER)
if(NOT HAVE_UWVM_PRESENT_HELPER OR NOT UWVM_LINK_HAVE_UWVM_PRESENT_HELPER)
  message(FATAL_ERROR "The actual target libc positive control did not link")
endif()
if(NOT CMAKE_TRY_COMPILE_TARGET_TYPE STREQUAL "STATIC_LIBRARY")
  message(FATAL_ERROR "The helper changed the caller's cross-toolchain target type")
endif()
file(SHA256 "${UWVM_LLVM_CONFIG_IX}" uwvm_config_sha)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/probe-result.json"
  "{\"passed\":true,\"source_config_sha256\":\"${uwvm_config_sha}\",\"compile_only_false_positive\":true,\"missing_helper_rejected\":true,\"stale_cache_rechecked\":true,\"target_libc_linked\":true,\"caller_target_type_unchanged\":true,\"target_binary_executed\":false}\n")
message(STATUS "UWVM helper link-probe controls passed; no target executable ran")
