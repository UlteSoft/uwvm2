/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#if defined(_WIN32) && !defined(__CYGWIN__)
# include <windows.h>
# include <tlhelp32.h>
# include <aclapi.h>
# include <bcrypt.h>
# include <type_traits>

// GNU/Clang bind these nonthrowing SDK-typed declarations directly to the
// Windows export, matching fast_io's stdcall symbol spelling. Pure MSVC has
// no GNU asm-name spelling here: its SDK-typed noexcept wrapper deliberately
// uses fast_io::noexcept_call. That fallback retains the SDK's import/link
// declarations and must be qualified separately with the actual MSVC toolchain.
# if !defined(__clang__) && !defined(__GNUC__)
#  include <fast_io.h>
#  define UWVM_WIN32_FORWARD(...) __VA_OPT__(,) __VA_ARGS__
#  define UWVM_WIN32_NOEXCEPT_IMPORT(result, name, bytes, params, args) \
    inline result WINAPI uwvm_##name params noexcept \
    { return ::fast_io::noexcept_call(::name UWVM_WIN32_FORWARD args); }
# else
#  if defined(_M_HYBRID)
#   define UWVM_WIN32_ASM_ALIAS(name, count) __asm__("#" #name "@" #count)
#  elif defined(__arm64ec__) || defined(_M_ARM64EC)
#   define UWVM_WIN32_ASM_ALIAS(name, count) __asm__("#" #name)
#  elif defined(__i386__) || defined(_M_IX86)
#   if defined(__clang__)
#    define UWVM_WIN32_ASM_ALIAS(name, count) __asm__("_" #name "@" #count)
#   else
#    define UWVM_WIN32_ASM_ALIAS(name, count) __asm__(#name "@" #count)
#   endif
#  else
#   define UWVM_WIN32_ASM_ALIAS(name, count) __asm__(#name)
#  endif
#  define UWVM_WIN32_NOEXCEPT_IMPORT(result, name, bytes, params, args) \
    extern "C" __declspec(dllimport) result WINAPI uwvm_##name params noexcept \
        UWVM_WIN32_ASM_ALIAS(name, bytes)
# endif

namespace uwvm2::utils::control::win32_abi
{
    UWVM_WIN32_NOEXCEPT_IMPORT(PVOID, AddVectoredExceptionHandler, 8, (ULONG arg0, PVECTORED_EXCEPTION_HANDLER arg1), (arg0, arg1));
    UWVM_WIN32_NOEXCEPT_IMPORT(BOOL, ConnectNamedPipe, 8, (HANDLE arg0, LPOVERLAPPED arg1), (arg0, arg1));
    UWVM_WIN32_NOEXCEPT_IMPORT(HANDLE, CreateFileW, 28, (LPCWSTR arg0, DWORD arg1, DWORD arg2, LPSECURITY_ATTRIBUTES arg3, DWORD arg4, DWORD arg5, HANDLE arg6), (arg0, arg1, arg2, arg3, arg4, arg5, arg6));
    UWVM_WIN32_NOEXCEPT_IMPORT(HANDLE, CreateNamedPipeW, 32, (LPCWSTR arg0, DWORD arg1, DWORD arg2, DWORD arg3, DWORD arg4, DWORD arg5, DWORD arg6, LPSECURITY_ATTRIBUTES arg7), (arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7));
    UWVM_WIN32_NOEXCEPT_IMPORT(BOOL, CreateProcessW, 40, (LPCWSTR arg0, LPWSTR arg1, LPSECURITY_ATTRIBUTES arg2, LPSECURITY_ATTRIBUTES arg3, BOOL arg4, DWORD arg5, LPVOID arg6, LPCWSTR arg7, LPSTARTUPINFOW arg8, LPPROCESS_INFORMATION arg9), (arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9));
    UWVM_WIN32_NOEXCEPT_IMPORT(BOOL, DuplicateHandle, 28, (HANDLE arg0, HANDLE arg1, HANDLE arg2, LPHANDLE arg3, DWORD arg4, BOOL arg5, DWORD arg6), (arg0, arg1, arg2, arg3, arg4, arg5, arg6));
    UWVM_WIN32_NOEXCEPT_IMPORT(BOOL, GetProcessTimes, 20, (HANDLE arg0, LPFILETIME arg1, LPFILETIME arg2, LPFILETIME arg3, LPFILETIME arg4), (arg0, arg1, arg2, arg3, arg4));
    UWVM_WIN32_NOEXCEPT_IMPORT(BOOL, ReadFile, 20, (HANDLE arg0, LPVOID arg1, DWORD arg2, LPDWORD arg3, LPOVERLAPPED arg4), (arg0, arg1, arg2, arg3, arg4));
    UWVM_WIN32_NOEXCEPT_IMPORT(BOOL, WriteFile, 20, (HANDLE arg0, LPCVOID arg1, DWORD arg2, LPDWORD arg3, LPOVERLAPPED arg4), (arg0, arg1, arg2, arg3, arg4));
    UWVM_WIN32_NOEXCEPT_IMPORT(LPVOID, HeapAlloc, 12, (HANDLE arg0, DWORD arg1, SIZE_T arg2), (arg0, arg1, arg2));
    UWVM_WIN32_NOEXCEPT_IMPORT(BOOL, HeapFree, 12, (HANDLE arg0, DWORD arg1, LPVOID arg2), (arg0, arg1, arg2));
    UWVM_WIN32_NOEXCEPT_IMPORT(HANDLE, GetProcessHeap, 0, (void), ());
    UWVM_WIN32_NOEXCEPT_IMPORT(BOOL, AssignProcessToJobObject, 8, (HANDLE arg0, HANDLE arg1), (arg0, arg1));
    UWVM_WIN32_NOEXCEPT_IMPORT(NTSTATUS, BCryptGenRandom, 16, (BCRYPT_ALG_HANDLE arg0, PUCHAR arg1, ULONG arg2, ULONG arg3), (arg0, arg1, arg2, arg3));
    UWVM_WIN32_NOEXCEPT_IMPORT(BOOL, CancelIoEx, 8, (HANDLE arg0, LPOVERLAPPED arg1), (arg0, arg1));
    // Synchronous debugger pipe worker: use exact SDK result, WINAPI and
    // LPTHREAD_START_ROUTINE types; 32-bit stdcall byte counts are explicit.
    UWVM_WIN32_NOEXCEPT_IMPORT(BOOL, CancelSynchronousIo, 4, (HANDLE arg0), (arg0));
    UWVM_WIN32_NOEXCEPT_IMPORT(HANDLE, CreateThread, 24, (LPSECURITY_ATTRIBUTES arg0, SIZE_T arg1, LPTHREAD_START_ROUTINE arg2, LPVOID arg3, DWORD arg4, LPDWORD arg5), (arg0, arg1, arg2, arg3, arg4, arg5));
    UWVM_WIN32_NOEXCEPT_IMPORT(BOOL, GetHandleInformation, 8, (HANDLE arg0, LPDWORD arg1), (arg0, arg1));
    UWVM_WIN32_NOEXCEPT_IMPORT(ULONGLONG, GetTickCount64, 0, (void), ());
    UWVM_WIN32_NOEXCEPT_IMPORT(BOOL, GetExitCodeThread, 8, (HANDLE arg0, LPDWORD arg1), (arg0, arg1));
    static_assert(::std::is_convertible_v<decltype(&uwvm_CancelSynchronousIo), decltype(&::CancelSynchronousIo)>);
    static_assert(::std::is_convertible_v<decltype(&uwvm_CreateThread), decltype(&::CreateThread)>);
    static_assert(::std::is_convertible_v<decltype(&uwvm_GetHandleInformation), decltype(&::GetHandleInformation)>);
    static_assert(::std::is_convertible_v<decltype(&uwvm_GetTickCount64), decltype(&::GetTickCount64)>);
    static_assert(::std::is_convertible_v<decltype(&uwvm_GetExitCodeThread), decltype(&::GetExitCodeThread)>);
    UWVM_WIN32_NOEXCEPT_IMPORT(HANDLE, CreateEventW, 16, (LPSECURITY_ATTRIBUTES arg0, BOOL arg1, BOOL arg2, LPCWSTR arg3), (arg0, arg1, arg2, arg3));
    UWVM_WIN32_NOEXCEPT_IMPORT(HANDLE, CreateJobObjectW, 8, (LPSECURITY_ATTRIBUTES arg0, LPCWSTR arg1), (arg0, arg1));
    UWVM_WIN32_NOEXCEPT_IMPORT(HANDLE, CreateToolhelp32Snapshot, 8, (DWORD arg0, DWORD arg1), (arg0, arg1));
    UWVM_WIN32_NOEXCEPT_IMPORT(void, DeleteProcThreadAttributeList, 4, (LPPROC_THREAD_ATTRIBUTE_LIST arg0), (arg0));
    UWVM_WIN32_NOEXCEPT_IMPORT(BOOL, EqualSid, 8, (PSID arg0, PSID arg1), (arg0, arg1));
    UWVM_WIN32_NOEXCEPT_IMPORT(HANDLE, GetCurrentProcess, 0, (void), ());
    UWVM_WIN32_NOEXCEPT_IMPORT(BOOL, GetNamedPipeClientProcessId, 8, (HANDLE arg0, PULONG arg1), (arg0, arg1));
    UWVM_WIN32_NOEXCEPT_IMPORT(BOOL, GetNamedPipeInfo, 20, (HANDLE arg0, LPDWORD arg1, LPDWORD arg2, LPDWORD arg3, LPDWORD arg4), (arg0, arg1, arg2, arg3, arg4));
    UWVM_WIN32_NOEXCEPT_IMPORT(BOOL, GetNamedPipeServerProcessId, 8, (HANDLE arg0, PULONG arg1), (arg0, arg1));
    UWVM_WIN32_NOEXCEPT_IMPORT(BOOL, GetOverlappedResult, 16, (HANDLE arg0, LPOVERLAPPED arg1, LPDWORD arg2, BOOL arg3), (arg0, arg1, arg2, arg3));
    UWVM_WIN32_NOEXCEPT_IMPORT(DWORD, GetProcessIdOfThread, 4, (HANDLE arg0), (arg0));
    UWVM_WIN32_NOEXCEPT_IMPORT(BOOL, GetThreadContext, 8, (HANDLE arg0, LPCONTEXT arg1), (arg0, arg1));
    UWVM_WIN32_NOEXCEPT_IMPORT(BOOL, GetTokenInformation, 20, (HANDLE arg0, TOKEN_INFORMATION_CLASS arg1, LPVOID arg2, DWORD arg3, PDWORD arg4), (arg0, arg1, arg2, arg3, arg4));
    UWVM_WIN32_NOEXCEPT_IMPORT(BOOL, InitializeProcThreadAttributeList, 16, (LPPROC_THREAD_ATTRIBUTE_LIST arg0, DWORD arg1, DWORD arg2, PSIZE_T arg3), (arg0, arg1, arg2, arg3));
    UWVM_WIN32_NOEXCEPT_IMPORT(BOOL, InitializeSecurityDescriptor, 8, (PSECURITY_DESCRIPTOR arg0, DWORD arg1), (arg0, arg1));
    UWVM_WIN32_NOEXCEPT_IMPORT(BOOL, IsProcessInJob, 12, (HANDLE arg0, HANDLE arg1, PBOOL arg2), (arg0, arg1, arg2));
    UWVM_WIN32_NOEXCEPT_IMPORT(BOOL, IsValidSid, 4, (PSID arg0), (arg0));
    UWVM_WIN32_NOEXCEPT_IMPORT(HANDLE, OpenProcess, 12, (DWORD arg0, BOOL arg1, DWORD arg2), (arg0, arg1, arg2));
    UWVM_WIN32_NOEXCEPT_IMPORT(BOOL, OpenProcessToken, 12, (HANDLE arg0, DWORD arg1, PHANDLE arg2), (arg0, arg1, arg2));
    UWVM_WIN32_NOEXCEPT_IMPORT(HANDLE, OpenThread, 12, (DWORD arg0, BOOL arg1, DWORD arg2), (arg0, arg1, arg2));
    UWVM_WIN32_NOEXCEPT_IMPORT(BOOL, PeekNamedPipe, 24, (HANDLE arg0, LPVOID arg1, DWORD arg2, LPDWORD arg3, LPDWORD arg4, LPDWORD arg5), (arg0, arg1, arg2, arg3, arg4, arg5));
    UWVM_WIN32_NOEXCEPT_IMPORT(BOOL, Process32FirstW, 8, (HANDLE arg0, LPPROCESSENTRY32W arg1), (arg0, arg1));
    UWVM_WIN32_NOEXCEPT_IMPORT(BOOL, Process32NextW, 8, (HANDLE arg0, LPPROCESSENTRY32W arg1), (arg0, arg1));
    UWVM_WIN32_NOEXCEPT_IMPORT(ULONG, RemoveVectoredExceptionHandler, 4, (PVOID arg0), (arg0));
    UWVM_WIN32_NOEXCEPT_IMPORT(BOOL, ResetEvent, 4, (HANDLE arg0), (arg0));
    UWVM_WIN32_NOEXCEPT_IMPORT(DWORD, ResumeThread, 4, (HANDLE arg0), (arg0));
    UWVM_WIN32_NOEXCEPT_IMPORT(DWORD, SetEntriesInAclW, 16, (ULONG arg0, PEXPLICIT_ACCESS_W arg1, PACL arg2, PACL* arg3), (arg0, arg1, arg2, arg3));
    UWVM_WIN32_NOEXCEPT_IMPORT(BOOL, SetEvent, 4, (HANDLE arg0), (arg0));
    UWVM_WIN32_NOEXCEPT_IMPORT(BOOL, SetNamedPipeHandleState, 16, (HANDLE arg0, LPDWORD arg1, LPDWORD arg2, LPDWORD arg3), (arg0, arg1, arg2, arg3));
    UWVM_WIN32_NOEXCEPT_IMPORT(BOOL, SetSecurityDescriptorDacl, 16, (PSECURITY_DESCRIPTOR arg0, BOOL arg1, PACL arg2, BOOL arg3), (arg0, arg1, arg2, arg3));
    UWVM_WIN32_NOEXCEPT_IMPORT(DWORD, SuspendThread, 4, (HANDLE arg0), (arg0));
    UWVM_WIN32_NOEXCEPT_IMPORT(BOOL, UpdateProcThreadAttribute, 28, (LPPROC_THREAD_ATTRIBUTE_LIST arg0, DWORD arg1, DWORD_PTR arg2, PVOID arg3, SIZE_T arg4, PVOID arg5, PSIZE_T arg6), (arg0, arg1, arg2, arg3, arg4, arg5, arg6));
    UWVM_WIN32_NOEXCEPT_IMPORT(void, SetLastError, 4, (DWORD arg0), (arg0));
    UWVM_WIN32_NOEXCEPT_IMPORT(BOOL, GetNumberOfConsoleInputEvents, 8, (HANDLE arg0, LPDWORD arg1), (arg0, arg1));
    UWVM_WIN32_NOEXCEPT_IMPORT(BOOL, GetConsoleScreenBufferInfo, 8, (HANDLE arg0, PCONSOLE_SCREEN_BUFFER_INFO arg1), (arg0, arg1));
}
# undef UWVM_WIN32_NOEXCEPT_IMPORT
# if defined(UWVM_WIN32_ASM_ALIAS)
#  undef UWVM_WIN32_ASM_ALIAS
# endif
# if defined(UWVM_WIN32_FORWARD)
#  undef UWVM_WIN32_FORWARD
# endif
#endif
