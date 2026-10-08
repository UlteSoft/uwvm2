# Cross-build LLVM's native libraries for the Windows x64 uwvm JIT.
# CMake's ASM language is detected separately from C and C++; without the
# explicit target, BLAKE3's Windows-named .S objects become host ELF members
# inside an otherwise COFF archive. Set this before project()/enable_language().
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER_TARGET x86_64-w64-windows-gnu CACHE STRING "Windows x64 C target" FORCE)
set(CMAKE_CXX_COMPILER_TARGET x86_64-w64-windows-gnu CACHE STRING "Windows x64 C++ target" FORCE)
set(CMAKE_ASM_COMPILER_TARGET x86_64-w64-windows-gnu CACHE STRING "Windows x64 ASM target" FORCE)
set(LLVM_HOST_TRIPLE x86_64-w64-windows-gnu CACHE STRING "Windows x64 LLVM host target" FORCE)
set(LLVM_DEFAULT_TARGET_TRIPLE x86_64-w64-windows-gnu CACHE STRING "Windows x64 LLVM default target" FORCE)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
