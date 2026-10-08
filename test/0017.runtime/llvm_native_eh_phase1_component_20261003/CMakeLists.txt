cmake_minimum_required(VERSION 3.20)
project(UwvmNativeEhPhase1Component LANGUAGES C CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR
   NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$" OR
   NOT CMAKE_C_COMPILER_ID STREQUAL "Clang" OR
   NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  message(FATAL_ERROR "Initial component requires native Linux x86_64 Clang; no cross-platform qualification")
endif()

option(EH_OBSERVER_ON "Exact-1 observer, must match both paired provider compile commands" OFF)
foreach(name EH_CXX_INCLUDE EH_UNWIND_INCLUDE EH_FAST_IO_INCLUDE)
  if(NOT IS_DIRECTORY "${${name}}")
    message(FATAL_ERROR "Supply the actual source-bound directory ${name}")
  endif()
endforeach()
foreach(name EH_CXX_ARCHIVE EH_CXXABI_ARCHIVE EH_UNWIND_ARCHIVE)
  if(NOT EXISTS "${${name}}")
    message(FATAL_ERROR "Resolve actual paired target archive ${name} from CMake File API")
  endif()
endforeach()
foreach(header __config_site __config thread)
  if(NOT EXISTS "${EH_CXX_INCLUDE}/${header}")
    message(FATAL_ERROR "Actual built cxx-headers closure is missing ${header}")
  endif()
endforeach()
if(NOT EXISTS "${EH_UNWIND_INCLUDE}/unwind.h" OR
   NOT EXISTS "${EH_FAST_IO_INCLUDE}/fast_io.h")
  message(FATAL_ERROR "Missing actual pinned unwind/fast_io headers")
endif()

add_executable(uwvm-native-eh-phase1-component
  ../llvm_native_eh_phase1_observer_component_20261003.cc
  ../llvm_native_eh_phase1_observer_plain_frames_20261003.c)
set_target_properties(uwvm-native-eh-phase1-component PROPERTIES
  C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS YES
  CXX_STANDARD 23 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
target_include_directories(uwvm-native-eh-phase1-component PRIVATE
  "${EH_UNWIND_INCLUDE}" "${EH_FAST_IO_INCLUDE}")
target_include_directories(uwvm-native-eh-phase1-component SYSTEM PRIVATE "${EH_CXX_INCLUDE}")
target_compile_options(uwvm-native-eh-phase1-component PRIVATE
  -O3 -g -fexceptions -funwind-tables -fno-optimize-sibling-calls
  "$<$<COMPILE_LANGUAGE:CXX>:-nostdinc++>"
  "$<$<COMPILE_LANGUAGE:CXX>:-frtti>")
if(EH_OBSERVER_ON)
  target_compile_definitions(uwvm-native-eh-phase1-component PRIVATE
    UWVM_EXPERIMENTAL_NATIVE_EH_PHASE1_OBSERVER=1)
endif()
# A driver default must not silently choose another C++/EH provider. Its actual
# compiler-rt builtins, target libc/sysroot, linker and final ELF still need the
# keeper's identity/loaded-provider audit; this project does not invent them.
target_link_options(uwvm-native-eh-phase1-component PRIVATE
  -nostdlib++ --rtlib=compiler-rt --unwindlib=none
  "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/component.link.map")
target_link_libraries(uwvm-native-eh-phase1-component PRIVATE
  "-Wl,--start-group" "${EH_CXX_ARCHIVE}" "${EH_CXXABI_ARCHIVE}"
  "${EH_UNWIND_ARCHIVE}" "-Wl,--end-group" pthread dl m)
if(DEFINED EH_ACTUAL_COMPILER_RT_BUILTINS AND NOT EH_ACTUAL_COMPILER_RT_BUILTINS STREQUAL "")
  if(NOT EXISTS "${EH_ACTUAL_COMPILER_RT_BUILTINS}")
    message(FATAL_ERROR "The bound compiler-rt builtins archive does not exist")
  endif()
  target_link_libraries(uwvm-native-eh-phase1-component PRIVATE "${EH_ACTUAL_COMPILER_RT_BUILTINS}")
endif()
