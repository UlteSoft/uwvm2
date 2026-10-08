/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/push_macros.h>
# if defined(_WIN32) && !defined(__CYGWIN__) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
#  include <array>
#  include <chrono>
#  include <cstddef>
#  include <cstdint>
#  include <cstring>
#  include <memory>
#  include <new>
#  include <string>
#  include <type_traits>
#  include <utility>
#  include <fast_io.h>
#  include <windows.h>
#  include <uwvm2/utils/control/win32_abi.h>
#  include <tlhelp32.h>
#  include <uwvm2/utils/control/sealed_input.h>
#  include "console.h"
# endif
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger
{
#if defined(_WIN32) && !defined(__CYGWIN__) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    // The shared Win32 ABI declarations preserve SDK parameter types and
    // provide nonthrowing imports. The host controller adopts only a pipe
    // endpoint that was provisioned by the trusted launcher before guest entry.
    // The direct launcher parent creates one local, single-instance MESSAGE
    // named pipe with PIPE_REJECT_REMOTE_CLIENTS, connects its client end,
    // and passes only that client HANDLE via PROC_THREAD_ATTRIBUTE_HANDLE_LIST.
    // The VM accepts a decimal HANDLE value, never a pipe name or path. The
    // handle is adopted before any guest import, file table, or code is live.
    class windows_control_handle final
    {
        // Every kernel capability has one native_file owner. HANDLE values
        // passed to the authenticated pipe/control APIs are synchronous borrows.
        ::fast_io::native_file endpoint_{}, io_event_{}, parent_{};
        ::DWORD parent_pid_{};
        ::FILETIME parent_creation_{};
        ::LUID parent_token_id_{}, parent_authentication_id_{};
        static constexpr ::std::size_t maximum_command_bytes{max_input_command_bytes};
        static constexpr ::std::size_t maximum_reply_bytes{65536u};
        static constexpr ::DWORD io_poll_ms{50u};
        static constexpr ::DWORD response_timeout_ms{5000u};

        windows_control_handle(::fast_io::native_file&& endpoint, ::fast_io::native_file&& event,
                               ::fast_io::native_file&& parent, ::DWORD pid,
                               ::FILETIME creation, ::TOKEN_STATISTICS const& token) noexcept
            : endpoint_{::std::move(endpoint)}, io_event_{::std::move(event)}, parent_{::std::move(parent)}, parent_pid_{pid}, parent_creation_{creation},
              parent_token_id_{token.TokenId}, parent_authentication_id_{token.AuthenticationId} {}

        [[nodiscard]] static ::DWORD direct_parent_pid() noexcept
        {
            auto const snapshot_handle{::uwvm2::utils::control::win32_abi::uwvm_CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0u)};
            if(snapshot_handle == INVALID_HANDLE_VALUE) { return 0u; }
            ::fast_io::native_file snapshot{snapshot_handle};
            ::PROCESSENTRY32W entry{};
            entry.dwSize = sizeof(entry);
            ::DWORD parent{};
            if(::uwvm2::utils::control::win32_abi::uwvm_Process32FirstW(snapshot.native_handle(), ::std::addressof(entry)) != 0)
            {
                do
                {
                    if(entry.th32ProcessID == ::fast_io::win32::GetCurrentProcessId())
                    { parent = entry.th32ParentProcessID; break; }
                } while(::uwvm2::utils::control::win32_abi::uwvm_Process32NextW(snapshot.native_handle(), ::std::addressof(entry)) != 0);
            }
            return parent;
        }
        [[nodiscard]] static bool process_identity(::HANDLE process, ::FILETIME& creation,
                                                    ::TOKEN_STATISTICS& token) noexcept
        {
            ::FILETIME exit{}, kernel{}, user{};
            if(::uwvm2::utils::control::win32_abi::uwvm_GetProcessTimes(process, ::std::addressof(creation), ::std::addressof(exit),
                                 ::std::addressof(kernel), ::std::addressof(user)) == 0) { return false; }
            ::HANDLE process_token{};
            if(::uwvm2::utils::control::win32_abi::uwvm_OpenProcessToken(process, TOKEN_QUERY, ::std::addressof(process_token)) == 0) { return false; }
            ::fast_io::native_file token_owner{process_token};
            ::DWORD length{};
            auto const ok{::uwvm2::utils::control::win32_abi::uwvm_GetTokenInformation(token_owner.native_handle(), TokenStatistics, ::std::addressof(token),
                                                sizeof(token), ::std::addressof(length)) != 0 && length == sizeof(token)};
            return ok;
        }
        [[nodiscard]] bool parent_live() const noexcept
        {
            if(!parent_ || ::fast_io::win32::WaitForSingleObject(parent_.native_handle(), 0u) != WAIT_TIMEOUT ||
               direct_parent_pid() != parent_pid_) { return false; }
            ::DWORD server_pid{};
            if(::uwvm2::utils::control::win32_abi::uwvm_GetNamedPipeServerProcessId(endpoint_.native_handle(), ::std::addressof(server_pid)) == 0 ||
               server_pid != parent_pid_) { return false; }
            ::FILETIME creation{};
            ::TOKEN_STATISTICS token{};
            return process_identity(parent_.native_handle(), creation, token) &&
                   ::std::memcmp(::std::addressof(creation), ::std::addressof(parent_creation_), sizeof(creation)) == 0 &&
                   token.TokenId.LowPart == parent_token_id_.LowPart &&
                   token.TokenId.HighPart == parent_token_id_.HighPart &&
                   token.AuthenticationId.LowPart == parent_authentication_id_.LowPart &&
                   token.AuthenticationId.HighPart == parent_authentication_id_.HighPart;
        }
        // An OVERLAPPED object cannot leave this scope while its kernel I/O is
        // still pending. If cancellation cannot be observed, terminate rather
        // than let a completion write through a dead stack address.
        void cancel_and_drain(::OVERLAPPED& pending) noexcept
        {
            ::uwvm2::utils::control::win32_abi::uwvm_CancelIoEx(endpoint_.native_handle(), ::std::addressof(pending));
            if(::fast_io::win32::WaitForSingleObject(io_event_.native_handle(), response_timeout_ms) != WAIT_OBJECT_0)
            { ::fast_io::win32::ExitProcess(1u); ::std::unreachable(); }
            ::DWORD ignored{};
            static_cast<void>(::uwvm2::utils::control::win32_abi::uwvm_GetOverlappedResult(endpoint_.native_handle(), ::std::addressof(pending),
                                                    ::std::addressof(ignored), FALSE));
        }
        [[nodiscard]] bool send_text(char const* bytes, ::std::size_t size) noexcept
        {
            if(size > maximum_reply_bytes || (size != 0u && bytes == nullptr) || !parent_live()) { return false; }
            if(::uwvm2::utils::control::win32_abi::uwvm_ResetEvent(io_event_.native_handle()) == 0) { return false; }
            ::OVERLAPPED pending{};
            pending.hEvent = io_event_.native_handle();
            ::DWORD written{};
            auto const started{::uwvm2::utils::control::win32_abi::uwvm_WriteFile(endpoint_.native_handle(), bytes, static_cast<::DWORD>(size),
                                           ::std::addressof(written), ::std::addressof(pending))};
            if(started != 0) { return written == size && parent_live(); }
            if(::fast_io::win32::GetLastError() != ERROR_IO_PENDING) { return false; }
            auto const deadline{::std::chrono::steady_clock::now() + ::std::chrono::milliseconds{response_timeout_ms}};
            for(;;)
            {
                auto const wait{::fast_io::win32::WaitForSingleObject(io_event_.native_handle(), io_poll_ms)};
                if(wait == WAIT_OBJECT_0) { break; }
                if(wait != WAIT_TIMEOUT || !parent_live() || ::std::chrono::steady_clock::now() >= deadline)
                { cancel_and_drain(pending); return false; }
            }
            return ::uwvm2::utils::control::win32_abi::uwvm_GetOverlappedResult(endpoint_.native_handle(), ::std::addressof(pending), ::std::addressof(written), FALSE) != 0 &&
                   written == size && parent_live();
        }
        [[nodiscard]] bool send_text(::std::string const& message) noexcept
        {
            if(message.size() > maximum_reply_bytes)
            { return send_literal("error: bounded debugger response exceeded 65536 bytes\n"); }
            return send_text(message.data(), message.size());
        }
        template <::std::size_t N>
        [[nodiscard]] bool send_literal(char const (&message)[N]) noexcept
        { return send_text(message, N - 1u); }

        struct received_message
        {
            ::std::array<char, maximum_command_bytes> bytes{};
            ::std::size_t size{};
            bool valid{};
        };
        [[nodiscard]] received_message receive_message(controller& control) noexcept
        {
            received_message message{};
            if(::uwvm2::utils::control::win32_abi::uwvm_ResetEvent(io_event_.native_handle()) == 0) { return message; }
            ::OVERLAPPED pending{};
            pending.hEvent = io_event_.native_handle();
            ::DWORD count{};
            auto const started{::uwvm2::utils::control::win32_abi::uwvm_ReadFile(endpoint_.native_handle(), message.bytes.data(),
                                          static_cast<::DWORD>(message.bytes.size()),
                                          ::std::addressof(count), ::std::addressof(pending))};
            if(started == 0)
            {
                if(::fast_io::win32::GetLastError() != ERROR_IO_PENDING) { return message; }
                for(;;)
                {
                    auto const wait{::fast_io::win32::WaitForSingleObject(io_event_.native_handle(), io_poll_ms)};
                    if(wait == WAIT_OBJECT_0) { break; }
                    auto const state{control.inspect().execution};
                    if(wait != WAIT_TIMEOUT || !parent_live() ||
                       state == execution_status::exited || state == execution_status::closed)
                    { cancel_and_drain(pending); return message; }
                }
                // ERROR_MORE_DATA means one message exceeded the bounded input capacity. Never
                // accept a truncated prefix or continue reading its tail.
                if(::uwvm2::utils::control::win32_abi::uwvm_GetOverlappedResult(endpoint_.native_handle(), ::std::addressof(pending),
                                         ::std::addressof(count), FALSE) == 0) { return message; }
            }
            if(count == 0u || count > message.bytes.size() || !parent_live()) { return message; }
            // ReadFile initialized exactly [bytes, bytes+count); the parser
            // sees no byte past that region and no trailing partial message.
            message.size = count;
            message.valid = true;
            return message;
        }
        void close_endpoint() noexcept
        {
            if(endpoint_)
            {
                static_cast<void>(::uwvm2::utils::control::win32_abi::uwvm_CancelIoEx(endpoint_.native_handle(), nullptr));
                // reset() retires the sole owner without throwing during detach.
                endpoint_.reset();
            }
        }
        struct close_on_serve_exit
        {
            windows_control_handle& channel;
            controller& control;
            ~close_on_serve_exit()
            {
                // A disconnected controller cannot leave native-step traps or
                // cooperative Wasm guest threads parked indefinitely.
                if(!control.detach_resume()) { ::fast_io::win32::ExitProcess(1u); ::std::unreachable(); }
                channel.close_endpoint();
            }
        };
    public:
        windows_control_handle(windows_control_handle const&) = delete;
        windows_control_handle& operator=(windows_control_handle const&) = delete;
        ~windows_control_handle()
        {
            close_endpoint();
            // Native members retire the event and authenticated parent after
            // the endpoint has completed cancellation/detachment.
        }
        [[nodiscard]] static ::std::unique_ptr<windows_control_handle> adopt(::std::uintptr_t value) noexcept
        {
            if(value < 4u || value == static_cast<::std::uintptr_t>(-1)) { return {}; }
            ::HANDLE endpoint{reinterpret_cast<::HANDLE>(value)};
            // The VM owns the explicitly inherited capability from this point,
            // even if authentication fails. No rejected HANDLE leaks to guest.
            ::fast_io::native_file supplied{endpoint};
            ::std::uint_least32_t inheritance{};  // fast_io Win32 ABI uses uint_least32_t*, not SDK DWORD*.
            ::DWORD flags{}, instances{}, out_buffer{}, in_buffer{}, server_pid{};
            if(::fast_io::win32::GetHandleInformation(endpoint, ::std::addressof(inheritance)) == 0 ||
               (inheritance & HANDLE_FLAG_INHERIT) == 0u || ::fast_io::win32::GetFileType(endpoint) != FILE_TYPE_PIPE ||
               ::uwvm2::utils::control::win32_abi::uwvm_GetNamedPipeInfo(endpoint, ::std::addressof(flags), ::std::addressof(out_buffer),
                                  ::std::addressof(in_buffer), ::std::addressof(instances)) == 0 ||
               (flags & PIPE_SERVER_END) != 0u || (flags & PIPE_TYPE_MESSAGE) == 0u || instances != 1u ||
               out_buffer < maximum_reply_bytes || in_buffer < maximum_command_bytes ||
               ::uwvm2::utils::control::win32_abi::uwvm_GetNamedPipeServerProcessId(endpoint, ::std::addressof(server_pid)) == 0)
            { return {}; }
            auto const parent_pid{direct_parent_pid()};
            if(parent_pid <= 1u || parent_pid == ::fast_io::win32::GetCurrentProcessId() || parent_pid != server_pid) { return {}; }
            ::fast_io::native_file parent{::uwvm2::utils::control::win32_abi::uwvm_OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, parent_pid)};
            if(!parent || ::fast_io::win32::WaitForSingleObject(parent.native_handle(), 0u) != WAIT_TIMEOUT) { return {}; }
            ::FILETIME creation{};
            ::TOKEN_STATISTICS token{};
            if(!process_identity(parent.native_handle(), creation, token)) { return {}; }
            ::DWORD read_mode{PIPE_READMODE_MESSAGE | PIPE_WAIT};
            if(::uwvm2::utils::control::win32_abi::uwvm_SetNamedPipeHandleState(endpoint, ::std::addressof(read_mode), nullptr, nullptr) == 0 ||
               ::fast_io::win32::SetHandleInformation(endpoint, HANDLE_FLAG_INHERIT, 0u) == 0)
            { return {}; }
            ::fast_io::native_file event{::uwvm2::utils::control::win32_abi::uwvm_CreateEventW(nullptr, TRUE, FALSE, nullptr)};
            if(!event) { return {}; }
            auto* raw{new (::std::nothrow) windows_control_handle(
                ::std::move(supplied), ::std::move(event), ::std::move(parent), parent_pid, creation, token)};
            if(raw == nullptr) { return {}; }
            ::std::unique_ptr<windows_control_handle> channel{raw};
            if(!channel->parent_live() ||
               ::uwvm2::utils::control::seal_debug_control_handle_host_api(endpoint) !=
                   ::uwvm2::utils::control::sealed_input_status::ok) { return {}; }
            for(int guest_stdio{}; guest_stdio != 3; ++guest_stdio)
            {
                auto const decision{guest_stdio == 0
                    ? ::uwvm2::utils::control::inspect_guest_file_host_api(guest_stdio)
                    : ::uwvm2::utils::control::inspect_guest_output_host_api(guest_stdio)};
                if(decision != ::uwvm2::utils::control::sealed_input_decision::allow) { return {}; }
            }
            return channel;
        }
        void serve(controller& control) noexcept
        {
            close_on_serve_exit close_after_guest_detach{*this, control};
            using command_view = ::std::remove_cv_t<decltype(console_help)>;
            for(;;)
            {
                auto const state{control.inspect().execution};
                if(state == execution_status::exited || state == execution_status::closed || !parent_live()) { break; }
                auto const message{receive_message(control)};
                if(!message.valid) { break; }
                auto const command{parse_console_command(command_view{message.bytes.data(), message.size})};
                switch(command.kind)
                {
                    case console_command_kind::empty: if(!send_literal("error: empty command\n")) { return; } continue;
                    case console_command_kind::help:
                        if(!send_text(console_help.data(), console_help.size())) { return; }
                        continue;
                    case console_command_kind::quit:
                        static_cast<void>(send_literal("debug control detached; guest continues\n"));
                        return;
                    case console_command_kind::unsupported:
                        {
                            auto const text{unsupported_step_message(command)};
                            if(!send_text(text.data(), text.size())) { return; }
                        }
                        continue;
                    case console_command_kind::invalid:
                        if(!send_literal("error: invalid command; type help\n")) { return; }
                        continue;
                    case console_command_kind::source_step:
                    case console_command_kind::wasm_step:
                    case console_command_kind::wasm_state:
                    case console_command_kind::wasm_path:
                    case console_command_kind::wasm_mutation:
                    case console_command_kind::source_breakpoint:
                    case console_command_kind::source_locals:
                    case console_command_kind::source_type:
                    case console_command_kind::source_value:
                    case console_command_kind::source_frame:
                    case console_command_kind::wasip1_state:
                    case console_command_kind::breakpoint_control:
                    case console_command_kind::assembly_next:
                    case console_command_kind::assembly_finish:
                    case console_command_kind::assembly_step:
                    case console_command_kind::assembly_disassemble:
                    case console_command_kind::assembly_disassemble_range:
                    case console_command_kind::assembly_registers:
                    case console_command_kind::wasm_event:
                    case console_command_kind::wasm_script:
                    case console_command_kind::replacement_file:
                    case console_command_kind::protocol: break;
                }
                auto const reply{control.execute(command)};
                if(!send_text(details::format_reply(reply, command))) { return; }
            }
        }
    };
#endif
}
#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/pop_macros.h>
#endif
