# Metadata-only consumer: CMake exports the real static/runtime system closure.
# It is never built or run; configure probes remain bootstrap compiler inputs.
function(uwvm_ros_export_runtimes_contract)
  if(NOT CMAKE_BUILD_TYPE STREQUAL "Release" OR NOT LLVM_VERSION_MAJOR EQUAL 23 OR
     NOT LLVM_VERSION_MINOR EQUAL 1 OR NOT LLVM_VERSION_PATCH EQUAL 1)
    message(FATAL_ERROR "ROS runtimes require the pinned Release LLVM 23.1.1 sources")
  endif()
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR MSVC OR
     CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC" OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    message(FATAL_ERROR "ROS bundled runtimes require a GNU Clang frontend/ABI")
  endif()
  if(NOT CMAKE_SYSTEM_NAME MATCHES "^(Linux|Darwin|Windows|FreeBSD|OpenBSD|NetBSD|DragonFly)$")
    message(FATAL_ERROR "This platform retains the existing explicit runtime provider")
  endif()
  if(LLVM_DEFAULT_TARGET_TRIPLE MATCHES "(arm64ec|aarch64ec|msvc)")
    message(FATAL_ERROR "MSVC and ARM64EC cannot use the ROS Itanium runtime provider")
  endif()
  if(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND
     NOT LLVM_DEFAULT_TARGET_TRIPLE MATCHES "(mingw|windows-gnu)")
    message(FATAL_ERROR "ROS Windows bundled runtimes require GNU COFF, not MSVC")
  endif()
  if(CMAKE_CROSSCOMPILING OR UWVM_ROS_RUNTIME_REQUIRE_EXPLICIT_TARGET)
    if(NOT CMAKE_C_COMPILER_TARGET OR NOT CMAKE_CXX_COMPILER_TARGET OR NOT CMAKE_ASM_COMPILER_TARGET OR
       NOT CMAKE_C_COMPILER_TARGET STREQUAL CMAKE_CXX_COMPILER_TARGET OR
       NOT CMAKE_CXX_COMPILER_TARGET STREQUAL CMAKE_ASM_COMPILER_TARGET)
      message(FATAL_ERROR "ROS cross runtimes require identical explicit C/C++/ASM targets")
    endif()
  endif()
  foreach(variable LLVM_ENABLE_ASSERTIONS LIBCXX_ENABLE_SHARED LIBCXXABI_ENABLE_SHARED
      LIBUNWIND_ENABLE_SHARED LIBCXX_ABI_UNSTABLE LIBCXXABI_ENABLE_ASSERTIONS LIBUNWIND_ENABLE_ASSERTIONS
      LIBCXX_STATICALLY_LINK_ABI_IN_STATIC_LIBRARY LIBCXXABI_STATICALLY_LINK_UNWINDER_IN_STATIC_LIBRARY)
    if(${variable})
      message(FATAL_ERROR "ROS paired Release static runtime contract violated: ${variable}")
    endif()
  endforeach()
  foreach(variable LIBCXX_ENABLE_STATIC LIBCXXABI_ENABLE_STATIC LIBUNWIND_ENABLE_STATIC
      LIBCXXABI_USE_LLVM_UNWINDER LIBCXXABI_ENABLE_STATIC_UNWINDER
      LIBCXX_ENABLE_EXCEPTIONS LIBCXXABI_ENABLE_EXCEPTIONS LIBCXX_ENABLE_RTTI
      LIBCXX_ENABLE_THREADS LIBCXXABI_ENABLE_THREADS LIBUNWIND_ENABLE_THREADS)
    if(NOT ${variable})
      message(FATAL_ERROR "ROS static runtime provider missing: ${variable}")
    endif()
  endforeach()
  if(NOT LIBCXX_ABI_VERSION STREQUAL "1" OR NOT LIBCXX_ABI_NAMESPACE STREQUAL "__1" OR
     NOT LIBCXX_HARDENING_MODE STREQUAL "none" OR
     NOT LIBCXX_CXX_ABI STREQUAL "libcxxabi")
    message(FATAL_ERROR "ROS libc++ ABI/header contract differs from its paired archives")
  endif()
  foreach(target cxx_static cxxabi_static unwind_static)
    if(NOT TARGET ${target})
      message(FATAL_ERROR "Missing paired ROS runtime target: ${target}")
    endif()
    get_target_property(kind ${target} TYPE)
    if(NOT kind STREQUAL "STATIC_LIBRARY")
      message(FATAL_ERROR "ROS runtime provider must be static: ${target}")
    endif()
  endforeach()
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    # Apple system libraries can call typed allocation even when the bootstrap
    # compiler does not emit it. Use the pinned upstream compatibility shims,
    # retaining exactly these definitions through dead stripping.
    target_sources(cxxabi_static PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../libcxxabi/src/vendor/apple/shims.cpp")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(uwvm_size_type m)
    else()
      set(uwvm_size_type j)
    endif()
    foreach(symbol
        "__ZdaPvRKSt9nothrow_tSt19__type_descriptor_t"
        "__ZdaPvSt19__type_descriptor_t"
        "__ZdaPv${uwvm_size_type}St19__type_descriptor_t"
        "__ZdlPvRKSt9nothrow_tSt19__type_descriptor_t"
        "__ZdlPvSt19__type_descriptor_t"
        "__ZdlPv${uwvm_size_type}St19__type_descriptor_t"
        "__Zna${uwvm_size_type}RKSt9nothrow_tSt19__type_descriptor_t"
        "__Zna${uwvm_size_type}St19__type_descriptor_t"
        "__Znw${uwvm_size_type}RKSt9nothrow_tSt19__type_descriptor_t"
        "__Znw${uwvm_size_type}St19__type_descriptor_t")
      target_link_options(cxxabi_static INTERFACE "LINKER:-u,${symbol}")
    endforeach()
    # Resolve exception entry points from the paired archive before libSystem.
    # Select the needed members rather than force-loading every object: Darwin
    # lld can emit duplicate zero-length compact-unwind entries for empty
    # members, masking a real frame and making even throw/catch(int) terminate.
    target_link_options(unwind_static INTERFACE
      "LINKER:-u,__Unwind_RaiseException" "LINKER:-u,__Unwind_Resume")
  endif()
  add_executable(uwvm_ros_runtimes_contract EXCLUDE_FROM_ALL "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/contract.cpp")
  target_link_libraries(uwvm_ros_runtimes_contract PRIVATE cxx_static cxxabi_static unwind_static)
  target_compile_options(uwvm_ros_runtimes_contract PRIVATE -nostdinc++)
  target_include_directories(uwvm_ros_runtimes_contract SYSTEM PRIVATE
    "${CMAKE_BINARY_DIR}/include/c++/v1" "${CMAKE_CURRENT_SOURCE_DIR}/../libcxxabi/include" "${CMAKE_CURRENT_SOURCE_DIR}/../libunwind/include")
  target_link_options(uwvm_ros_runtimes_contract PRIVATE -nostdlib++ --unwindlib=none)
  foreach(name LLVM_DEFAULT_TARGET_TRIPLE CMAKE_CXX_COMPILER_TARGET)
    string(REPLACE "\\" "\\\\" ${name} "${${name}}")
    string(REPLACE "\"" "\\\"" ${name} "${${name}}")
  endforeach()
  file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/uwvm-runtimes.json" CONTENT
    "{\"version\":\"23.1.1\",\"provider\":\"llvm-static\",\"abi_version\":\"1\",\"host_target\":\"${LLVM_DEFAULT_TARGET_TRIPLE}\",\"compiler_target\":\"${CMAKE_CXX_COMPILER_TARGET}\"}\n")
endfunction()
cmake_language(DEFER CALL uwvm_ros_export_runtimes_contract)
