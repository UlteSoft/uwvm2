# Included by LLVM's project() through CMAKE_PROJECT_LLVM_INCLUDE. Defer until
# all real LLVM targets exist; CMake itself resolves their static dependencies.
# The metadata-only executable is NEVER compiled or run. In particular, cross
# builds need no target llvm-config binary or QEMU just to discover libraries.
# Check this at project() time, BEFORE config-ix.cmake supplies its default:
# config.guess describes the machine doing the build, not the machine on which
# the cross-built library will run. A real AArch64 compiler otherwise configures
# successfully with an x86_64 LLVM_HOST_TRIPLE and Native selects X86 codegen.
# xmake clears previous inferred triple cache entries before reconfiguration;
# the cross toolchain must explicitly supply the target/ABI's host triple.
if((CMAKE_CROSSCOMPILING OR UWVM_ROS_REQUIRE_EXPLICIT_HOST_TRIPLE) AND
   (NOT DEFINED LLVM_HOST_TRIPLE OR LLVM_HOST_TRIPLE STREQUAL ""))
  message(FATAL_ERROR "ROS cross LLVM requires an explicit LLVM_HOST_TRIPLE in the CMake toolchain")
endif()

function(uwvm_ros_export_llvm_contract)
  # Our explicit Release build has its own CRT/ABI/cache policy, independent of
  # the application's debug mode and the JIT's guest optimization level. Reject
  # toolchain overrides before compiling libraries; a single Debug configuration
  # is not an equivalent contract (notably for Windows debug CRT/iterators).
  if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    message(FATAL_ERROR "ROS bundled LLVM must use the Release build configuration")
  endif()
  if(NOT PACKAGE_VERSION STREQUAL "23.1.1-uwvm-ros.6")
    message(FATAL_ERROR "ROS LLVM contract requires the pinned patched release")
  endif()
  # LLVM itself only warns when Native is absent. That is useful for an AOT
  # cross-compiler, but this dependency runs ROS's JIT on its own host ISA.
  # Check this before the default triple: LLVM can leave that triple empty
  # when Native is excluded, and the primary diagnostic is the missing backend.
  if(NOT LLVM_NATIVE_TARGET)
    message(FATAL_ERROR "ROS JIT requires its native LLVM backend: ${LLVM_NATIVE_ARCH}")
  endif()
  # ROS uses the configured default triple explicitly to preserve N32/x32's
  # 64-bit ISA with a 32-bit pointer ABI. LLVM permits an unrelated AOT default
  # even when its libraries run on the native host, but that is not a valid
  # native MCJIT contract. Reject a toolchain override before code can be emitted
  # for a different ISA/ABI. Cross builds should set the HOST triple and let the
  # default derive from it; explicit per-module AOT triples are unaffected.
  if(NOT LLVM_DEFAULT_TARGET_TRIPLE STREQUAL LLVM_HOST_TRIPLE)
    message(FATAL_ERROR "ROS native JIT requires LLVM_DEFAULT_TARGET_TRIPLE to match LLVM_HOST_TRIPLE")
  endif()
  llvm_map_components_to_libnames(uwvm_ros_components
    core support analysis target linker executionengine mcjit runtimedyld
    passes scalaropts transformutils instcombine bitreader bitwriter object
    targetparser nativecodegen)
  foreach(component IN LISTS uwvm_ros_components)
    if(NOT TARGET "${component}")
      message(FATAL_ERROR "Required ROS LLVM component is unavailable: ${component}")
    endif()
    get_target_property(kind "${component}" TYPE)
    if(NOT kind STREQUAL "STATIC_LIBRARY")
      message(FATAL_ERROR "ROS requires static LLVM archives: ${component}")
    endif()
    if(UWVM_ROS_MSVC_RUNTIME)
      # LLVM 23 no longer implements LLVM_USE_CRT_RELEASE. The CMake variable
      # initializes this property when each library is created; checking only
      # the variable later would miss target-specific or toolchain overrides.
      get_target_property(actual_runtime "${component}" MSVC_RUNTIME_LIBRARY)
      # GNU-style clang++ targeting windows-msvc uses the same ABI/property
      # without necessarily setting CMake's MSVC frontend flag. Accept its
      # SIMULATE_ID, not GNU-style MinGW (which has a different CRT model).
      if((NOT MSVC AND NOT CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC") OR
         NOT actual_runtime STREQUAL UWVM_ROS_MSVC_RUNTIME)
        message(FATAL_ERROR
          "ROS LLVM MSVC runtime mismatch for ${component}: expected ${UWVM_ROS_MSVC_RUNTIME}, got ${actual_runtime}")
      endif()
    endif()
  endforeach()
  # Executable target type asks CMake to compute a final, transitive link line;
  # a static-library target alone would only describe an archiver invocation.
  # EXCLUDE_FROM_ALL and xmake's explicit llvm-libraries build keep this stub
  # metadata-only, including when configuring for an ISA the host cannot run.
  add_executable(uwvm_ros_llvm_contract EXCLUDE_FROM_ALL
    "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/contract.cpp")
  target_include_directories(uwvm_ros_llvm_contract PRIVATE
    "${LLVM_MAIN_INCLUDE_DIR}" "${LLVM_INCLUDE_DIR}")
  target_link_libraries(uwvm_ros_llvm_contract PRIVATE ${uwvm_ros_components})
  # The CMake file API exports the full link order, including platform system
  # libraries, groups and any repeats. Do not maintain a second library-name
  # graph in Lua or infer it from filenames. ABI inputs come from the same
  # CMake invocation that compiles these archives.
  foreach(name PACKAGE_VERSION LLVM_HOST_TRIPLE)
    string(REPLACE "\\" "\\\\" ${name} "${${name}}")
    string(REPLACE "\"" "\\\"" ${name} "${${name}}")
  endforeach()
  file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/uwvm-llvm-version.json"
    CONTENT "{\"version\":\"${PACKAGE_VERSION}\",\"host_target\":\"${LLVM_HOST_TRIPLE}\"}\n")
endfunction()
cmake_language(DEFER CALL uwvm_ros_export_llvm_contract)
