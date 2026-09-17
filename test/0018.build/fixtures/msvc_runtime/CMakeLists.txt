cmake_minimum_required(VERSION 3.20)
# The real ROS deferred hook is passed via CMAKE_PROJECT_LLVM_INCLUDE. This
# fixture substitutes only the library graph, not CMake's CRT flag lowering.
project(LLVM LANGUAGES CXX)
set(PACKAGE_VERSION "23.1.1-uwvm-ros.6")
set(LLVM_NATIVE_TARGET X86)
set(LLVM_NATIVE_ARCH X86)
set(LLVM_MAIN_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(LLVM_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}")
function(llvm_map_components_to_libnames output)
  set(${output} LLVMFixture PARENT_SCOPE)
endfunction()
add_library(LLVMFixture STATIC probe.cpp)
if(DEFINED UWVM_TEST_EXPECT_DEBUG)
  target_compile_definitions(LLVMFixture PRIVATE
    UWVM_TEST_EXPECT_DEBUG=${UWVM_TEST_EXPECT_DEBUG}
    UWVM_TEST_EXPECT_DLL=${UWVM_TEST_EXPECT_DLL})
endif()
if(UWVM_TEST_OVERRIDE_RUNTIME)
  set_property(TARGET LLVMFixture PROPERTY MSVC_RUNTIME_LIBRARY "${UWVM_TEST_OVERRIDE_RUNTIME}")
endif()
