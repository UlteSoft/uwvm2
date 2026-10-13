// SOURCE ONLY: current Windows CLI launcher; actual SDK build/VM pending.
// The outer test owner admits this process suspended into its unique kill-on-
// close job. CreateProcess child inheritance has no breakaway flag. A regular
// file output sink preserves the product's strict sealed-console policy.
#include <fast_io.h>
#include <uwvm2/utils/control/win32_abi.h>
#include <array>
#include <memory>
#include <string>
#include <string_view>
#if !defined(_WIN32) || (!defined(_M_X64) && !defined(__x86_64__))
# error Native Windows x64 launcher required.
#endif
namespace abi = ::uwvm2::utils::control::win32_abi;
static bool path_ok(::std::wstring_view value) noexcept
{
    if(value.empty() || value.size() > 2048u || value.back() == L'\\') { return false; }
    for(auto c : value) { if(c < 32 || c == L'"') { return false; } }
    return true;
}
int wmain(int argc, wchar_t** argv)
{
    if(argc != 8) { return 2; }
    // [OS-owned argv slots 0..7] argv_end
    // [safe                   ] argc proved every slot; immutable string borrows
    //  ^^ no Wasm supplied address and no pointer advance is accepted.
    ::std::wstring_view const pe{argv[1]}, wasm{argv[2]}, out{argv[3]}, err{argv[4]},
        policy{argv[5]}, repository{argv[6]}, scenario{argv[7]};
    if(!path_ok(pe) || !path_ok(wasm) || !path_ok(out) || !path_ok(err) || out == err ||
       (policy != L"instruction" && policy != L"unwind") ||
       (repository != L"ordinary" && repository != L"ros") ||
       (scenario != L"full" && scenario != L"unsupported")) { return 3; }
    BOOL admitted{};
    if(abi::uwvm_IsProcessInJob(abi::uwvm_GetCurrentProcess(), nullptr, ::std::addressof(admitted)) == 0 || !admitted)
    { return 4; }
    auto const input{static_cast<HANDLE>(::fast_io::win32::GetStdHandle(STD_INPUT_HANDLE))};
    if(input == nullptr || input == INVALID_HANDLE_VALUE || ::fast_io::win32::GetFileType(input) != FILE_TYPE_PIPE) { return 5; }
    // Retain only this host-selected input and two independent regular files in
    // the actual child's handle list. No job handle or launcher's captured pipe
    // output is inherited by the product. stdout/stderr never alias input.
    SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
    auto const output{abi::uwvm_CreateFileW(argv[3], GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        ::std::addressof(security), CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr)};
    if(output == INVALID_HANDLE_VALUE) { return 6; }
    auto const error{abi::uwvm_CreateFileW(argv[4], GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        ::std::addressof(security), CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr)};
    if(error == INVALID_HANDLE_VALUE) { ::fast_io::win32::CloseHandle(output); return 7; }
    auto const cleanup_files{[&]() noexcept { ::fast_io::win32::CloseHandle(error); ::fast_io::win32::CloseHandle(output); }};
    if(::fast_io::win32::GetFileType(output) != FILE_TYPE_DISK || ::fast_io::win32::GetFileType(error) != FILE_TYPE_DISK)
    { cleanup_files(); return 17; }
    if(::fast_io::win32::SetHandleInformation(input, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT) == 0)
    { cleanup_files(); return 8; }
    ::std::wstring command{L"\""}; command.append(pe); command += L"\" -m debug-jit ";
    if(scenario == L"unsupported")
    { command += repository == L"ros" ? L"-Rint " : L"-Rcc jit -Rcm lazy "; }
    else { command += repository == L"ros" ? L"-Raot " : L"-Rcc jit -Rcm full "; }
    command += L"-Rct 0 -Rllvm-call-stack "; command.append(policy);
    command += L" -Rllvm-exception-dispatch native-unwind -Rllvm-cache-path disable --run \"";
    command.append(wasm); command += L'"';
    if(command.size() >= 8192u) { cleanup_files(); return 9; }
    SIZE_T bytes{};
    abi::uwvm_InitializeProcThreadAttributeList(nullptr, 1u, 0u, ::std::addressof(bytes));
    if(bytes == 0u || bytes > 65536u) { cleanup_files(); return 10; }
    auto const heap{abi::uwvm_GetProcessHeap()};
    auto* const storage{abi::uwvm_HeapAlloc(heap, 0u, bytes)};
    if(storage == nullptr) { cleanup_files(); return 11; }
    // [heap-owned attribute storage bytes] attribute_end
    // [safe                             ] SDK supplies extent, capped above;
    //  ^^ typed borrow remains in this owner until CreateProcess returns.
    auto* const attributes{reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage)};
    if(abi::uwvm_InitializeProcThreadAttributeList(attributes, 1u, 0u, ::std::addressof(bytes)) == 0)
    { abi::uwvm_HeapFree(heap, 0u, storage); cleanup_files(); return 12; }
    ::std::array<HANDLE, 3u> inherited{input, output, error};
    // [owned three-handle array] handles_end
    // [safe                   ] attribute borrow size is exactly sizeof array;
    //  ^^ array and SDK attribute owner outlive the synchronous child creation.
    if(abi::uwvm_UpdateProcThreadAttribute(attributes, 0u, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
        inherited.data(), sizeof(inherited), nullptr, nullptr) == 0)
    { abi::uwvm_DeleteProcThreadAttributeList(attributes); abi::uwvm_HeapFree(heap, 0u, storage); cleanup_files(); return 13; }
    STARTUPINFOEXW startup{}; startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES; startup.StartupInfo.hStdInput = input;
    startup.StartupInfo.hStdOutput = output; startup.StartupInfo.hStdError = error; startup.lpAttributeList = attributes;
    PROCESS_INFORMATION child{};
    // [owned bounded writable command string] command_end
    // [safe                                 ] SDK mutates only this live buffer;
    //  ^^ no pointer is advanced or guest memory/address borrowed.
    auto const created{abi::uwvm_CreateProcessW(argv[1], command.data(), nullptr, nullptr, TRUE,
        EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW, nullptr, nullptr, ::std::addressof(startup.StartupInfo), ::std::addressof(child))};
    abi::uwvm_DeleteProcThreadAttributeList(attributes); abi::uwvm_HeapFree(heap, 0u, storage); cleanup_files();
    if(created == 0) { return 14; }
    BOOL child_admitted{};
    if(abi::uwvm_IsProcessInJob(child.hProcess, nullptr, ::std::addressof(child_admitted)) == 0 || !child_admitted)
    { ::fast_io::win32::TerminateProcess(child.hProcess, 124u); return 15; }
    ::fast_io::io::println("current-debug-child-pid=", ::fast_io::mnp::dec(child.dwProcessId), " inherited-job=yes");
    if(::fast_io::win32::WaitForSingleObject(child.hProcess, 90'000u) != WAIT_OBJECT_0)
    { ::fast_io::win32::TerminateProcess(child.hProcess, 124u); return 124; }
    ::std::uint_least32_t status{};
    if(::fast_io::win32::GetExitCodeProcess(child.hProcess, ::std::addressof(status)) == 0) { return 16; }
    ::fast_io::win32::CloseHandle(child.hThread); ::fast_io::win32::CloseHandle(child.hProcess);
    // Preserve the REAL full-width child NTSTATUS. A crash must never become a
    // small generic exit code which could pass an unsupported-mode negative.
    ::fast_io::win32::ExitProcess(status);
    __builtin_trap(); // The OS exit primitive must not return.
}
