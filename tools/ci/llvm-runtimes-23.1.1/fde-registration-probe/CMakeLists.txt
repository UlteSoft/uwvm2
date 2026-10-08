cmake_minimum_required(VERSION 3.20)
project(uwvm_paired_c_fde_registration LANGUAGES C)

# Compile/link only. No target executable is run, including cross builds.
# Supply the resolved paired cxx/cxxabi/unwind archives and their ordered OS
# dependencies from the actual runtimes file API, never guessed library names.
if(NOT UWVM_FDE_DECLARATIONS OR NOT IS_ABSOLUTE "${UWVM_FDE_DECLARATIONS}" OR
   NOT EXISTS "${UWVM_FDE_DECLARATIONS}")
  message(FATAL_ERROR "Supply the actual ROS LLVM cmake/unwind.h declaration file")
endif()
file(SHA256 "${UWVM_FDE_DECLARATIONS}" declarations_sha256)
if(NOT declarations_sha256 STREQUAL "d146994076c2729cff1b7bf2bb6ab0f26b6cb04a440940a17bc8be7885e1300d")
  message(FATAL_ERROR "EH-registration declarations differ from the qualified ROS source")
endif()
if(NOT UWVM_FDE_LIBRARIES)
  message(FATAL_ERROR "Supply the actual ordered paired runtime archive/system closure")
endif()
list(LENGTH UWVM_FDE_LIBRARIES library_count)
if(library_count LESS 3)
  message(FATAL_ERROR "All three separate paired runtime archives are required")
endif()
set(expected_names "libc++.a" "libc++abi.a" "libunwind.a")
foreach(index RANGE 0 2)
  list(GET UWVM_FDE_LIBRARIES ${index} library)
  list(GET expected_names ${index} expected_name)
  get_filename_component(actual_name "${library}" NAME)
  if(NOT IS_ABSOLUTE "${library}" OR NOT EXISTS "${library}" OR
     NOT actual_name STREQUAL expected_name)
    message(FATAL_ERROR "Paired runtime archive/order mismatch: ${library}")
  endif()
endforeach()

# STATIC_LIBRARY try_compile avoids the linker, so it cannot prove a symbol.
set(CMAKE_TRY_COMPILE_TARGET_TYPE EXECUTABLE)
include(CheckSymbolExists)
set(CMAKE_REQUIRED_LIBRARIES "${UWVM_FDE_LIBRARIES}")
set(CMAKE_REQUIRED_LINK_OPTIONS "--unwindlib=none")
foreach(probe UWVM_FDE_HAVE_REGISTER_FRAME UWVM_FDE_HAVE_DEREGISTER_FRAME
    UWVM_FDE_HAVE_UNW_ADD_DYNAMIC_FDE)
  unset(${probe} CACHE)
  unset(${probe})
endforeach()
check_symbol_exists(__register_frame "${UWVM_FDE_DECLARATIONS}" UWVM_FDE_HAVE_REGISTER_FRAME)
check_symbol_exists(__deregister_frame "${UWVM_FDE_DECLARATIONS}" UWVM_FDE_HAVE_DEREGISTER_FRAME)
check_symbol_exists(__unw_add_dynamic_fde "${UWVM_FDE_DECLARATIONS}" UWVM_FDE_HAVE_UNW_ADD_DYNAMIC_FDE)
if(NOT UWVM_FDE_HAVE_REGISTER_FRAME OR NOT UWVM_FDE_HAVE_DEREGISTER_FRAME OR
   NOT UWVM_FDE_HAVE_UNW_ADD_DYNAMIC_FDE)
  message(FATAL_ERROR "Paired provider failed an actual C EH-registration link probe")
endif()
