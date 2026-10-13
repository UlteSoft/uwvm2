// SOURCE ONLY: the fixed Windows x64 RAII component's regular-file launcher.
// Its PowerShell parent first admits this process suspended into its private
// non-inheritable KILL_ON_JOB_CLOSE job. No child uses BREAKAWAY_FROM_JOB.
// This file does not qualify a product debugger, a module build, or a guest.
#include <fast_io.h>
#include <fast_io_dsal/string.h>
#include <uwvm2/utils/control/win32_abi.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

#if !defined(_WIN32) || defined(__CYGWIN__) || (!defined(_M_X64) && !defined(__x86_64__))
# error This test launcher requires the actual Windows x64 SDK.
#endif

namespace abi = ::uwvm2::utils::control::win32_abi;

namespace
{
    // The immutable guest plan supplies only private, absolute drive paths.
    // Shell syntax is never interpreted. Restrict every path before quoting
    // it into CreateProcess's live, writable fast_io concat buffer.
    bool path_ok(::std::wstring_view path) noexcept
    {
        if(path.size() < 4u || path.size() > 2048u || path[1] != L':' || path[2] != L'\\' ||
           path.back() == L'\\') { return false; }
        if(!::fast_io::char_category::is_c_alpha(path[0])) { return false; }
        for(auto character : path)
        {
            if(character < 32 || character == L'"' || character == L'\0') { return false; }
        }
        return true;
    }

    struct attribute_owner
    {
        alignas(::std::max_align_t) ::std::array<::std::byte, 65536u> storage;
        bool initialized{};

        LPPROC_THREAD_ATTRIBUTE_LIST get() noexcept
        {
            // [owned aligned attribute storage, 65536 bytes] storage_end
            // [safe                                        ] SDK size checked
            //  ^^ typed borrow changes no pointer and remains in this owner.
            return reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
        }

        bool prepare() noexcept
        {
            SIZE_T required{};
            abi::uwvm_InitializeProcThreadAttributeList(nullptr, 1u, 0u, ::std::addressof(required));
            if(required == 0u || required > storage.size()) { return false; }
            initialized = abi::uwvm_InitializeProcThreadAttributeList(get(), 1u, 0u,
                ::std::addressof(required)) != 0;
            return initialized;
        }

        ~attribute_owner()
        {
            if(initialized) { abi::uwvm_DeleteProcThreadAttributeList(get()); }
        }
    };

    struct child_owner
    {
        ::fast_io::nt_file process;
        ::fast_io::nt_file thread;
        bool reaped{};

        bool retire() noexcept
        {
            if(!process || reaped) { return true; }
            // Authority is the actual handle returned by our CreateProcess;
            // neither a PID nor a process name is adopted or signalled.
            if(::fast_io::win32::WaitForSingleObject(process.native_handle(), 0u) != WAIT_OBJECT_0)
            {
                if(::fast_io::win32::TerminateProcess(process.native_handle(), 124u) == 0 &&
                   ::fast_io::win32::WaitForSingleObject(process.native_handle(), 0u) != WAIT_OBJECT_0)
                { return false; }
            }
            reaped = ::fast_io::win32::WaitForSingleObject(process.native_handle(), 10000u) == WAIT_OBJECT_0;
            return reaped;
        }

        ~child_owner()
        {
            if(!retire())
            {
                // The launcher must fail closed if an actual owned child
                // cannot retire. The outer non-inheritable job handle still
                // kills all its members when the PowerShell owner disposes.
                ::fast_io::fast_terminate();
            }
        }
    };

    ::std::uint_least32_t run(int argc, wchar_t** argv)
    {
        if(argc != 4) { return 2u; }
        // [OS-owned argv slots 0..3] argv_end
        // [safe                   ] argc proves every borrow below; no pointer
        //  ^^ increment or address supplied by the test/guest is dereferenced.
        ::std::wstring_view const executable{argv[1]}, output_path{argv[2]}, error_path{argv[3]};
        if(!path_ok(executable) || !path_ok(output_path) || !path_ok(error_path) ||
           executable == output_path || executable == error_path || output_path == error_path)
        { return 3u; }
        BOOL admitted{};
        if(abi::uwvm_IsProcessInJob(abi::uwvm_GetCurrentProcess(), nullptr,
            ::std::addressof(admitted)) == 0 || !admitted) { return 4u; }
        auto const borrowed_input{static_cast<HANDLE>(::fast_io::win32::GetStdHandle(STD_INPUT_HANDLE))};
        if(borrowed_input == nullptr || borrowed_input == INVALID_HANDLE_VALUE ||
           ::fast_io::win32::GetFileType(borrowed_input) != FILE_TYPE_PIPE) { return 5u; }
        HANDLE input_handle{};
        auto const current{abi::uwvm_GetCurrentProcess()};
        auto const duplicated{abi::uwvm_DuplicateHandle(current, borrowed_input, current,
            ::std::addressof(input_handle), 0u, TRUE, DUPLICATE_SAME_ACCESS)};
        ::fast_io::nt_file input{input_handle};
        if(duplicated == 0 || !input) { return 6u; }

        // All filesystem opens and close ownership use the native fast_io
        // provider. Exclusive creation in the host-selected private directory
        // rejects an existing final component; the parent pins that directory.
        constexpr auto mode{::fast_io::open_mode::out | ::fast_io::open_mode::creat | ::fast_io::open_mode::excl};
        ::fast_io::native_file output{::fast_io::mnp::os_c_str(argv[2]), mode};
        ::fast_io::native_file error{::fast_io::mnp::os_c_str(argv[3]), mode};
        if(::fast_io::win32::GetFileType(output.native_handle()) != FILE_TYPE_DISK ||
           ::fast_io::win32::GetFileType(error.native_handle()) != FILE_TYPE_DISK) { return 7u; }
        if(::fast_io::win32::SetHandleInformation(output.native_handle(), HANDLE_FLAG_INHERIT,
            HANDLE_FLAG_INHERIT) == 0 ||
           ::fast_io::win32::SetHandleInformation(error.native_handle(), HANDLE_FLAG_INHERIT,
            HANDLE_FLAG_INHERIT) == 0) { return 8u; }

        attribute_owner attributes;
        if(!attributes.prepare()) { return 9u; }
        ::std::array<HANDLE, 3u> inherited{input.native_handle(), output.native_handle(), error.native_handle()};
        // [owned three-handle array] handles_end
        // [safe                   ] exactly sizeof(inherited) readable bytes;
        //  ^^ SDK borrows the array until CreateProcess returns synchronously.
        if(abi::uwvm_UpdateProcThreadAttribute(attributes.get(), 0u, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
            inherited.data(), sizeof(inherited), nullptr, nullptr) == 0) { return 10u; }
        STARTUPINFOEXW startup{};
        startup.StartupInfo.cb = sizeof(startup);
        startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        startup.StartupInfo.hStdInput = inherited[0];
        startup.StartupInfo.hStdOutput = inherited[1];
        startup.StartupInfo.hStdError = inherited[2];
        startup.lpAttributeList = attributes.get();
        auto command{::fast_io::wconcat_fast_io(L"\"",
            ::fast_io::basic_io_scatter_t<wchar_t>{executable.data(), executable.size()}, L"\"")};
        PROCESS_INFORMATION created{};
        // [owned bounded mutable concat buffer] command_end
        // [safe                               ] quoted executable <=2050 WCHARs;
        //  ^^ SDK writes only into this live, NUL-terminated string owner.
        auto const success{abi::uwvm_CreateProcessW(argv[1], command.data(), nullptr, nullptr, TRUE,
            EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, nullptr,
            ::std::addressof(startup.StartupInfo), ::std::addressof(created))};
        child_owner child{::fast_io::nt_file{created.hProcess}, ::fast_io::nt_file{created.hThread}};
        if(success == 0 || !child.process || !child.thread) { return 11u; }
        BOOL child_admitted{};
        if(abi::uwvm_IsProcessInJob(child.process.native_handle(), nullptr,
            ::std::addressof(child_admitted)) == 0 || !child_admitted) { return 12u; }
        // CreateProcess is suspended until job inheritance has been witnessed;
        // no branch ever allows an unadmitted child to become runnable.
        if(abi::uwvm_ResumeThread(child.thread.native_handle()) != 1u) { return 13u; }
        ::fast_io::io::println("raii-child-pid=", ::fast_io::mnp::dec(created.dwProcessId),
            " inherited-job=yes output=regular");
        if(::fast_io::win32::WaitForSingleObject(child.process.native_handle(), 90000u) != WAIT_OBJECT_0)
        { return 124u; }
        child.reaped = true;
        ::std::uint_least32_t status{};
        if(::fast_io::win32::GetExitCodeProcess(child.process.native_handle(),
            ::std::addressof(status)) == 0) { return 14u; }
        ::fast_io::io::println("raii-child-exit=", ::fast_io::mnp::hex(status));
        // Checked file close occurs after actual process retirement. Returning
        // unwinds every fast_io handle owner before wmain preserves NTSTATUS.
        output.close();
        error.close();
        return status;
    }
}

int wmain(int argc, wchar_t** argv)
{
    auto const result{run(argc, argv)};
    // Preserve the entire 32-bit status; converting through signed int could
    // conceal an actual Windows exception/death failure in guest acceptance.
    ::fast_io::win32::ExitProcess(result);
    __builtin_trap();
}
