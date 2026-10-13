// Windows host-owned late LLVM-full debugger broker. Build this as a separate
// native Windows executable; it is never linked into the VM or guest ABI.
#if defined(_WIN32) && !defined(__CYGWIN__)
# define _WIN32_WINNT 0x0A00
# include <windows.h>
# include <aclapi.h>
# include <bcrypt.h>
# include <fast_io.h>
# include <uwvm2/utils/control/win32_abi.h>
# include <fast_io_dsal/string_view.h>
# include <array>
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <limits>
# include <memory>
# include <stdexcept>
# include <string>
# include <vector>

namespace
{
    constexpr ::DWORD command_limit{8448u}, reply_limit{65536u};
    constexpr ::DWORD idle_poll_ms{50u};
    struct handle_owner
    {
        ::HANDLE value{};
        handle_owner() = default;
        explicit handle_owner(::HANDLE h) noexcept : value{h} {}
        handle_owner(handle_owner const&) = delete;
        handle_owner& operator=(handle_owner const&) = delete;
        ~handle_owner() { if(value != nullptr && value != INVALID_HANDLE_VALUE) { ::fast_io::win32::CloseHandle(value); } }
        [[nodiscard]] bool valid() const noexcept { return value != nullptr && value != INVALID_HANDLE_VALUE; }
        [[nodiscard]] ::HANDLE release() noexcept { auto const old{value}; value = nullptr; return old; }
    };
    struct private_acl
    {
        ::SECURITY_DESCRIPTOR descriptor{};
        ::PACL acl{};
        ::std::vector<::std::byte> token_user{};
        ::SECURITY_ATTRIBUTES attributes{sizeof(attributes), ::std::addressof(descriptor), FALSE};
        private_acl() = default;
        private_acl(private_acl const&) = delete;
        private_acl& operator=(private_acl const&) = delete;
        ~private_acl() { if(acl != nullptr) { ::fast_io::win32::LocalFree(acl); } }
        [[nodiscard]] bool initialize()
        {
            handle_owner token{};
            if(::uwvm2::utils::control::win32_abi::uwvm_OpenProcessToken(::uwvm2::utils::control::win32_abi::uwvm_GetCurrentProcess(), TOKEN_QUERY, ::std::addressof(token.value)) == 0)
            { return false; }
            ::DWORD length{};
            static_cast<void>(::uwvm2::utils::control::win32_abi::uwvm_GetTokenInformation(token.value, TokenUser, nullptr, 0u, ::std::addressof(length)));
            if(length < sizeof(::TOKEN_USER) || length > 65536u) { return false; }
            token_user.resize(length);
            if(::uwvm2::utils::control::win32_abi::uwvm_GetTokenInformation(token.value, TokenUser, token_user.data(), length,
                                     ::std::addressof(length)) == 0) { return false; }
            auto* user{reinterpret_cast<::TOKEN_USER*>(token_user.data())};
            if(::uwvm2::utils::control::win32_abi::uwvm_IsValidSid(user->User.Sid) == 0) { return false; }
            ::EXPLICIT_ACCESSW access{};
            access.grfAccessPermissions = GENERIC_ALL;
            access.grfAccessMode = SET_ACCESS;
            access.grfInheritance = NO_INHERITANCE;
            access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
            access.Trustee.TrusteeType = TRUSTEE_IS_USER;
            access.Trustee.ptstrName = static_cast<::LPWSTR>(user->User.Sid);
            if(::uwvm2::utils::control::win32_abi::uwvm_SetEntriesInAclW(1u, ::std::addressof(access), nullptr, ::std::addressof(acl)) != ERROR_SUCCESS ||
               ::uwvm2::utils::control::win32_abi::uwvm_InitializeSecurityDescriptor(::std::addressof(descriptor), SECURITY_DESCRIPTOR_REVISION) == 0 ||
               ::uwvm2::utils::control::win32_abi::uwvm_SetSecurityDescriptorDacl(::std::addressof(descriptor), TRUE, acl, FALSE) == 0)
            { return false; }
            return true;
        }
        [[nodiscard]] ::PSID owner_sid() const noexcept
        { return reinterpret_cast<::TOKEN_USER const*>(token_user.data())->User.Sid; }
    };
    [[nodiscard]] bool random_bytes(unsigned char* bytes, ::std::size_t length) noexcept
    {
        return length <= 65536u && ::uwvm2::utils::control::win32_abi::uwvm_BCryptGenRandom(nullptr, bytes, static_cast<::ULONG>(length),
                                                     BCRYPT_USE_SYSTEM_PREFERRED_RNG) >= 0;
    }
    [[nodiscard]] ::std::wstring hex_wide(unsigned char const* bytes, ::std::size_t length)
    {
        static constexpr wchar_t digits[]{L"0123456789abcdef"};
        ::std::wstring result{};
        result.reserve(length * 2u);
        for(::std::size_t i{}; i != length; ++i)
        { result.push_back(digits[bytes[i] >> 4u]); result.push_back(digits[bytes[i] & 15u]); }
        return result;
    }
    [[nodiscard]] ::std::string generated_ascii(::std::wstring const& text)
    {
        ::std::string result{};
        result.reserve(text.size());
        // Pipe names and capabilities are generated from fixed ASCII literals
        // and hexadecimal bytes; only those host-generated values enter here.
        for(auto const character : text)
        {
            if(character > 0x7fu)
            { throw ::std::runtime_error("non-ASCII generated debugger endpoint"); }
            result.push_back(static_cast<char>(character));
        }
        return result;
    }
    [[nodiscard]] bool parse_hex(wchar_t const* text, unsigned char* bytes, ::std::size_t length) noexcept
    {
        if(text == nullptr || length > (::std::numeric_limits<::std::size_t>::max() - 1u) / 2u) { return false; }
        auto const expected{length * 2u};
        // argv is NUL-terminated: inspect at most expected characters plus
        // the required terminator, never an unbounded external string.
        for(::std::size_t i{}; i != expected; ++i)
        { if(text[i] == L'\0') { return false; } }
        if(text[expected] != L'\0') { return false; }
        auto nibble{[](wchar_t c) noexcept -> int
        { return c >= L'0' && c <= L'9' ? c - L'0' : c >= L'a' && c <= L'f' ? c - L'a' + 10 : -1; }};
        for(::std::size_t i{}; i != length; ++i)
        {
            auto const high{nibble(text[2u * i])}, low{nibble(text[2u * i + 1u])};
            if(high < 0 || low < 0) { return false; }
            bytes[i] = static_cast<unsigned char>((high << 4u) | low);
        }
        return true;
    }
    [[nodiscard]] bool equal_secret(unsigned char const* lhs, unsigned char const* rhs, ::std::size_t length) noexcept
    {
        unsigned difference{};
        for(::std::size_t i{}; i != length; ++i) { difference |= lhs[i] ^ rhs[i]; }
        return difference == 0u;
    }
    [[nodiscard]] bool trusted_console() noexcept
    {
        ::std::uint_least32_t mode{};
        return ::fast_io::win32::GetConsoleMode(::fast_io::win32::GetStdHandle(STD_OUTPUT_HANDLE), ::std::addressof(mode)) != 0;
    }
    [[nodiscard]] bool same_owner(::HANDLE process, private_acl const& security) noexcept
    {
        handle_owner token{};
        if(::uwvm2::utils::control::win32_abi::uwvm_OpenProcessToken(process, TOKEN_QUERY, ::std::addressof(token.value)) == 0) { return false; }
        ::DWORD length{};
        static_cast<void>(::uwvm2::utils::control::win32_abi::uwvm_GetTokenInformation(token.value, TokenUser, nullptr, 0u, ::std::addressof(length)));
        if(length < sizeof(::TOKEN_USER) || length > 65536u) { return false; }
        ::std::vector<::std::byte> user(length);
        if(::uwvm2::utils::control::win32_abi::uwvm_GetTokenInformation(token.value, TokenUser, user.data(), length, ::std::addressof(length)) == 0)
        { return false; }
        return ::uwvm2::utils::control::win32_abi::uwvm_EqualSid(reinterpret_cast<::TOKEN_USER*>(user.data())->User.Sid, security.owner_sid()) != 0;
    }
    [[nodiscard]] bool outside_guest_job(::HANDLE candidate, ::HANDLE guest_job) noexcept
    {
        ::BOOL is_member{};
        return ::uwvm2::utils::control::win32_abi::uwvm_IsProcessInJob(candidate, guest_job, ::std::addressof(is_member)) != 0 && is_member == FALSE;
    }
    [[nodiscard]] bool exact_message(::HANDLE pipe, char* bytes, ::DWORD capacity, ::DWORD& size) noexcept
    {
        size = 0u;
        if(::uwvm2::utils::control::win32_abi::uwvm_ReadFile(pipe, bytes, capacity, ::std::addressof(size), nullptr) == 0)
        { return false; } // ERROR_MORE_DATA is a rejected oversized message.
        return size != 0u && size <= capacity;
    }
    [[nodiscard]] bool write_message(::HANDLE pipe, char const* bytes, ::DWORD size) noexcept
    {
        ::DWORD written{};
        return ::uwvm2::utils::control::win32_abi::uwvm_WriteFile(pipe, bytes, size, ::std::addressof(written), nullptr) != 0 && written == size;
    }
    // The external host listener is created FILE_FLAG_OVERLAPPED so the broker
    // can observe guest exit while accepting. Its data I/O must also supply an
    // OVERLAPPED object; a synchronous ReadFile on that HANDLE is unsupported.
    [[nodiscard]] bool external_message(::HANDLE pipe, char* bytes, ::DWORD capacity,
                                        ::DWORD& size, bool writing) noexcept
    {
        handle_owner event{::uwvm2::utils::control::win32_abi::uwvm_CreateEventW(nullptr, TRUE, FALSE, nullptr)};
        if(!event.valid()) { return false; }
        ::OVERLAPPED pending{};
        pending.hEvent = event.value;
        size = 0u;
        auto const started{writing
            ? ::uwvm2::utils::control::win32_abi::uwvm_WriteFile(pipe, bytes, capacity, ::std::addressof(size), ::std::addressof(pending))
            : ::uwvm2::utils::control::win32_abi::uwvm_ReadFile(pipe, bytes, capacity, ::std::addressof(size), ::std::addressof(pending))};
        if(started != 0) { return size != 0u && size <= capacity; }
        if(::fast_io::win32::GetLastError() != ERROR_IO_PENDING) { return false; }
        if(::fast_io::win32::WaitForSingleObject(event.value, 5000u) != WAIT_OBJECT_0)
        {
            ::uwvm2::utils::control::win32_abi::uwvm_CancelIoEx(pipe, ::std::addressof(pending));
            if(::fast_io::win32::WaitForSingleObject(event.value, 5000u) != WAIT_OBJECT_0)
            { ::fast_io::win32::TerminateProcess(::uwvm2::utils::control::win32_abi::uwvm_GetCurrentProcess(), 125u); }
            ::DWORD ignored{};
            static_cast<void>(::uwvm2::utils::control::win32_abi::uwvm_GetOverlappedResult(pipe, ::std::addressof(pending), ::std::addressof(ignored), FALSE));
            return false;
        }
        return ::uwvm2::utils::control::win32_abi::uwvm_GetOverlappedResult(pipe, ::std::addressof(pending), ::std::addressof(size), FALSE) != 0 &&
               size != 0u && size <= capacity; // ERROR_MORE_DATA is rejected.
    }
    [[nodiscard]] bool message_ready(::HANDLE pipe, ::HANDLE guest) noexcept
    {
        for(;;)
        {
            ::DWORD available{};
            if(::uwvm2::utils::control::win32_abi::uwvm_PeekNamedPipe(pipe, nullptr, 0u, nullptr, ::std::addressof(available), nullptr) == 0)
            { return false; }
            if(available != 0u) { return true; }
            if(guest != nullptr && ::fast_io::win32::WaitForSingleObject(guest, 0u) != WAIT_TIMEOUT) { return false; }
            ::fast_io::win32::Sleep(idle_poll_ms);
        }
    }
    [[nodiscard]] ::std::wstring quoted(wchar_t const* text)
    {
        ::std::wstring result{L'"'};
        ::std::size_t backslashes{};
        for(auto const* p{text}; *p != L'\0'; ++p)
        {
            if(*p == L'\\') { ++backslashes; continue; }
            if(*p == L'"') { result.append(backslashes * 2u + 1u, L'\\'); result.push_back(*p); backslashes = 0u; continue; }
            result.append(backslashes, L'\\'); backslashes = 0u;
            result.push_back(*p);
        }
        result.append(backslashes * 2u, L'\\');
        result.push_back(L'"');
        return result;
    }
    [[nodiscard]] int serve(int argc, wchar_t** argv)
    {
        if(argc < 4 || !trusted_console())
        { ::fast_io::io::perrln("serve requires a real host console, uwvm.exe, and explicit LLVM-full run arguments"); return 2; }
        private_acl security{};
        if(!security.initialize()) { return 3; }
        ::std::array<unsigned char, 32u> secret{};
        ::std::array<unsigned char, 16u> name_nonce{};
        if(!random_bytes(secret.data(), secret.size()) || !random_bytes(name_nonce.data(), name_nonce.size()))
        { return 4; }
        auto const control_name{::std::wstring{L"\\\\.\\pipe\\uwvm-debug-vm-"} + hex_wide(name_nonce.data(), name_nonce.size())};
        auto const server_name{::std::wstring{L"\\\\.\\pipe\\uwvm-debug-host-"} + hex_wide(name_nonce.data(), name_nonce.size())};
        handle_owner vm_server{::uwvm2::utils::control::win32_abi::uwvm_CreateNamedPipeW(control_name.c_str(),
            PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
            1u, reply_limit, reply_limit, 0u, ::std::addressof(security.attributes))};
        if(!vm_server.valid()) { return 5; }
        handle_owner vm_client{::uwvm2::utils::control::win32_abi::uwvm_CreateFileW(control_name.c_str(), GENERIC_READ | GENERIC_WRITE | FILE_WRITE_ATTRIBUTES,
            0u, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr)};
        if(!vm_client.valid()) { return 6; }
        if(::uwvm2::utils::control::win32_abi::uwvm_ConnectNamedPipe(vm_server.value, nullptr) == 0 && ::fast_io::win32::GetLastError() != ERROR_PIPE_CONNECTED)
        { return 7; }
        handle_owner inherited{};
        if(::uwvm2::utils::control::win32_abi::uwvm_DuplicateHandle(::uwvm2::utils::control::win32_abi::uwvm_GetCurrentProcess(), vm_client.value, ::uwvm2::utils::control::win32_abi::uwvm_GetCurrentProcess(),
                             ::std::addressof(inherited.value), 0u, TRUE, DUPLICATE_SAME_ACCESS) == 0)
        { return 8; }
        ::SIZE_T attribute_bytes{};
        static_cast<void>(::uwvm2::utils::control::win32_abi::uwvm_InitializeProcThreadAttributeList(nullptr, 1u, 0u, ::std::addressof(attribute_bytes)));
        if(attribute_bytes == 0u || attribute_bytes > 65536u) { return 9; }
        auto* attributes{static_cast<::LPPROC_THREAD_ATTRIBUTE_LIST>(::uwvm2::utils::control::win32_abi::uwvm_HeapAlloc(::uwvm2::utils::control::win32_abi::uwvm_GetProcessHeap(), 0u, attribute_bytes))};
        if(attributes == nullptr) { return 10; }
        struct attribute_owner
        {
            ::LPPROC_THREAD_ATTRIBUTE_LIST value{};
            bool initialized{};
            ~attribute_owner()
            {
                if(initialized) { ::uwvm2::utils::control::win32_abi::uwvm_DeleteProcThreadAttributeList(value); }
                if(value != nullptr) { ::uwvm2::utils::control::win32_abi::uwvm_HeapFree(::uwvm2::utils::control::win32_abi::uwvm_GetProcessHeap(), 0u, value); }
            }
        } owned_attributes{attributes};
        if(::uwvm2::utils::control::win32_abi::uwvm_InitializeProcThreadAttributeList(attributes, 1u, 0u, ::std::addressof(attribute_bytes)) == 0)
        { return 11; }
        owned_attributes.initialized = true;
        if(::uwvm2::utils::control::win32_abi::uwvm_UpdateProcThreadAttribute(attributes, 0u, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                       ::std::addressof(inherited.value), sizeof(inherited.value), nullptr, nullptr) == 0)
        { return 12; }
        ::std::wstring command{quoted(argv[2])};
        command.append(L" --debug-jit-control-handle ");
        command.append(::fast_io::basic_general_concat<false, wchar_t, ::std::wstring>(
            ::fast_io::mnp::dec(reinterpret_cast<::std::uintptr_t>(inherited.value))));
        for(int i{3}; i != argc; ++i) { command.push_back(L' '); command.append(quoted(argv[i])); }
        ::STARTUPINFOEXW startup{};
        startup.StartupInfo.cb = sizeof(startup);
        startup.lpAttributeList = attributes;
        // Assignment while suspended makes the guest and every descendant a
        // kernel-enforced set. A client in that set can never authorize itself,
        // even if Toolhelp parent records disappear or PIDs are recycled.
        handle_owner guest_job{::uwvm2::utils::control::win32_abi::uwvm_CreateJobObjectW(nullptr, nullptr)};
        if(!guest_job.valid()) { return 13; }
        ::PROCESS_INFORMATION child{};
        if(::uwvm2::utils::control::win32_abi::uwvm_CreateProcessW(argv[2], command.data(), nullptr, nullptr, TRUE,
                            EXTENDED_STARTUPINFO_PRESENT | CREATE_SUSPENDED, nullptr, nullptr,
                            ::std::addressof(startup.StartupInfo), ::std::addressof(child)) == 0)
        { return 14; }
        handle_owner guest{child.hProcess};
        handle_owner guest_thread{child.hThread};
        if(::uwvm2::utils::control::win32_abi::uwvm_AssignProcessToJobObject(guest_job.value, guest.value) == 0 ||
           ::uwvm2::utils::control::win32_abi::uwvm_ResumeThread(guest_thread.value) == static_cast<::DWORD>(-1))
        { ::fast_io::win32::TerminateProcess(guest.value, 125u); return 15; }
        ::fast_io::win32::CloseHandle(inherited.release());
        ::fast_io::win32::CloseHandle(vm_client.release());
        // The token is emitted only to a real host console, never a file, pipe,
        // environment variable, guest argv, or guest-visible WASI preopen.
        ::fast_io::io::println("debug server: ", generated_ascii(server_name),
                               "\ncapability: ", generated_ascii(hex_wide(secret.data(), secret.size())));
        bool detached{};
        while(::fast_io::win32::WaitForSingleObject(guest.value, 0u) == WAIT_TIMEOUT && !detached)
        {
            handle_owner listener{::uwvm2::utils::control::win32_abi::uwvm_CreateNamedPipeW(server_name.c_str(),
                PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED,
                PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
                1u, reply_limit, command_limit, 0u, ::std::addressof(security.attributes))};
            if(!listener.valid()) { return 16; }
            handle_owner connect_event{::uwvm2::utils::control::win32_abi::uwvm_CreateEventW(nullptr, TRUE, FALSE, nullptr)};
            if(!connect_event.valid()) { return 17; }
            ::OVERLAPPED pending{};
            pending.hEvent = connect_event.value;
            auto const connected{::uwvm2::utils::control::win32_abi::uwvm_ConnectNamedPipe(listener.value, ::std::addressof(pending))};
            auto const connect_error{connected == 0 ? ::fast_io::win32::GetLastError() : ERROR_SUCCESS};
            if(connect_error != ERROR_SUCCESS && connect_error != ERROR_IO_PENDING &&
               connect_error != ERROR_PIPE_CONNECTED)
            { return 18; }
            if(connected != 0 || connect_error == ERROR_PIPE_CONNECTED) { ::uwvm2::utils::control::win32_abi::uwvm_SetEvent(connect_event.value); }
            while(::fast_io::win32::WaitForSingleObject(connect_event.value, idle_poll_ms) == WAIT_TIMEOUT &&
                  ::fast_io::win32::WaitForSingleObject(guest.value, 0u) == WAIT_TIMEOUT) {}
            if(::fast_io::win32::WaitForSingleObject(guest.value, 0u) != WAIT_TIMEOUT)
            {
                ::uwvm2::utils::control::win32_abi::uwvm_CancelIoEx(listener.value, ::std::addressof(pending));
                ::fast_io::win32::WaitForSingleObject(connect_event.value, 5000u);
                break;
            }
            ::DWORD client_pid{};
            auto const identified{::uwvm2::utils::control::win32_abi::uwvm_GetNamedPipeClientProcessId(listener.value, ::std::addressof(client_pid)) != 0};
            auto const identity_error{identified ? ERROR_SUCCESS : ::fast_io::win32::GetLastError()};
            if(!identified || client_pid <= 1u || client_pid == ::fast_io::win32::GetCurrentProcessId())
            {
                ::fast_io::io::perrln("debug client rejected: process identity (error=",
                                      ::fast_io::mnp::dec(identity_error), ")");
                continue;
            }
            handle_owner client{::uwvm2::utils::control::win32_abi::uwvm_OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, client_pid)};
            if(!client.valid() || ::fast_io::win32::WaitForSingleObject(client.value, 0u) != WAIT_TIMEOUT ||
               !same_owner(client.value, security) || !outside_guest_job(client.value, guest_job.value))
            { ::fast_io::io::perrln("debug client rejected: owner identity"); continue; }
            ::std::array<char, command_limit> command_bytes{};
            ::DWORD size{};
            char ready_message[]{"ready\n"};
            ::DWORD ready_size{};
            if(!message_ready(listener.value, guest.value) ||
               !external_message(listener.value, command_bytes.data(), command_limit, size, false) ||
               size != secret.size() ||
               !equal_secret(reinterpret_cast<unsigned char const*>(command_bytes.data()), secret.data(), secret.size()))
            { ::fast_io::io::perrln("debug client rejected: authentication"); continue; }
            if(!external_message(listener.value, ready_message, 6u, ready_size, true) || ready_size != 6u)
            { ::fast_io::io::perrln("debug client disconnected before ready"); continue; }
            while(::fast_io::win32::WaitForSingleObject(guest.value, 0u) == WAIT_TIMEOUT &&
                  ::fast_io::win32::WaitForSingleObject(client.value, 0u) == WAIT_TIMEOUT)
            {
                ::DWORD observed_client{};
                if(!message_ready(listener.value, guest.value) ||
                   !external_message(listener.value, command_bytes.data(), command_limit, size, false) ||
                   ::uwvm2::utils::control::win32_abi::uwvm_GetNamedPipeClientProcessId(listener.value, ::std::addressof(observed_client)) == 0 ||
                   observed_client != client_pid || !same_owner(client.value, security) ||
                   !outside_guest_job(client.value, guest_job.value)) { break; }
                bool const quit{size == 4u && ::std::memcmp(command_bytes.data(), "quit", 4u) == 0};
                if(!write_message(vm_server.value, command_bytes.data(), size)) { break; }
                ::std::array<char, reply_limit> reply{};
                ::DWORD reply_size{};
                ::DWORD sent_reply{};
                if(!message_ready(vm_server.value, guest.value) ||
                   !exact_message(vm_server.value, reply.data(), reply_limit, reply_size) ||
                   !external_message(listener.value, reply.data(), reply_size, sent_reply, true) ||
                   sent_reply != reply_size) { break; }
                if(quit) { detached = true; break; }
            }
            static_cast<void>(::fast_io::win32::DisconnectNamedPipe(listener.value));
        }
        if(detached) { ::fast_io::io::println("debugger detached; guest continues"); return 0; }
        ::std::uint_least32_t child_status{};
        return ::fast_io::win32::GetExitCodeProcess(guest.value, ::std::addressof(child_status)) != 0 && child_status < 256u
            ? static_cast<int>(child_status) : 125;
    }
    [[nodiscard]] int connect(int argc, wchar_t** argv)
    {
        if(argc != 4) { return 2; }
        ::std::array<unsigned char, 32u> secret{};
        if(!parse_hex(argv[3], secret.data(), secret.size())) { return 3; }
        handle_owner pipe{::uwvm2::utils::control::win32_abi::uwvm_CreateFileW(argv[2], GENERIC_READ | GENERIC_WRITE, 0u, nullptr,
                                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
        if(!pipe.valid()) { return 4; }
        ::DWORD state{PIPE_READMODE_MESSAGE};
        if(::uwvm2::utils::control::win32_abi::uwvm_SetNamedPipeHandleState(pipe.value, ::std::addressof(state), nullptr, nullptr) == 0 ||
           !write_message(pipe.value, reinterpret_cast<char const*>(secret.data()), static_cast<::DWORD>(secret.size())))
        { return 5; }
        ::std::array<char, reply_limit> reply{};
        ::DWORD length{};
        if(!exact_message(pipe.value, reply.data(), reply_limit, length) || length != 6u ||
           ::std::memcmp(reply.data(), "ready\n", 6u) != 0) { return 6; }
        ::std::array<char, command_limit> line{};
        for(;;)
        {
            ::std::size_t size{};
            for(;;)
            {
                char character{};
                // [safe one-byte character] unsafe (one-past)
                //                          ^^ read_some may return the one-past end.
                auto const next{::fast_io::operations::read_some(::fast_io::in(),
                                                                  ::std::addressof(character),
                                                                  ::std::addressof(character) + 1u)};
                if(next == ::std::addressof(character))
                { return size == 0u ? 0 : 7; }
                if(character == '\n') { break; }
                if(size == line.size()) { return 7; }
                // [safe line bytes] unsafe (one-past)
                //                  ^^ size advances only after the capacity check.
                line[size++] = character;
            }
            if(size == 0u || !write_message(pipe.value, line.data(), static_cast<::DWORD>(size)) ||
               !exact_message(pipe.value, reply.data(), reply_limit, length)) { return 8; }
            ::fast_io::io::print(::fast_io::out(), ::fast_io::string_view{reply.data(), length});
            if(size == 4u && ::std::memcmp(line.data(), "quit", 4u) == 0) { break; }
        }
        return 0;
    }
}
int wmain(int argc, wchar_t** argv)
{
    try
    {
        if(argc >= 2 && ::wcscmp(argv[1], L"serve") == 0) { return serve(argc, argv); }
        if(argc >= 2 && ::wcscmp(argv[1], L"connect") == 0) { return connect(argc, argv); }
        ::fast_io::io::perrln("usage: secure_server_windows serve uwvm.exe [LLVM-full run arguments...] | connect PIPE TOKEN");
        return 2;
    }
    catch(...) { return 125; }
}
#else
int main() { return 2; }
#endif
