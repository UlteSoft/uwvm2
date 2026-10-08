/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/push_macros.h>
# if defined(__APPLE__) && defined(__MACH__) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
#  include <array>
#  include <cerrno>
#  include <chrono>
#  include <cstddef>
#  include <cstdint>
#  include <cstring>
#  include <memory>
#  include <new>
#  include <fast_io.h>
#  include <string>
#  include <type_traits>
#  include <utility>
#  include <fcntl.h>
#  include <poll.h>
#  include <signal.h>
#  include <sys/socket.h>
#  include <sys/ucred.h>
#  include <sys/un.h>
#  include <unistd.h>
#  include <mach/message.h>
#  include "posix_abi.h"
#  include <uwvm2/utils/control/sealed_input.h>
#  include "console.h"
# endif
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger
{
#if defined(__APPLE__) && defined(__MACH__) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    // Darwin has no AF_UNIX SOCK_SEQPACKET socketpair. The launcher's *direct
    // child* inherits one end of an unnamed SOCK_STREAM pair. A four-byte
    // network-order length frames each command/reply; no descriptor or path is
    // supplied by Wasm. LOCAL_PEERPID, LOCAL_PEERCRED, and LOCAL_PEERTOKEN bind
    // the connected peer. Parentage and liveness are checked on every I/O.
    class macos_control_fd final
    {
        // The member owns the inherited capability; native_handle() only borrows.
        ::fast_io::native_file descriptor_{};
        ::pid_t peer_pid_{};
        ::uid_t peer_uid_{};
        ::gid_t peer_gid_{};
        ::audit_token_t peer_token_{};
        static constexpr ::std::size_t maximum_command_bytes{max_input_command_bytes};
        static constexpr ::std::size_t maximum_reply_bytes{65536u};
        static constexpr auto frame_timeout{::std::chrono::seconds{5}};

        macos_control_fd(::fast_io::native_file&& descriptor, ::pid_t pid, ::uid_t uid, ::gid_t gid, ::audit_token_t token) noexcept
            : descriptor_{::std::move(descriptor)}, peer_pid_{pid}, peer_uid_{uid}, peer_gid_{gid}, peer_token_{token} {}
        [[nodiscard]] static bool unnamed_unix_socket(int fd, bool peer) noexcept
        {
            ::sockaddr_un name{};
            ::socklen_t length{sizeof(name)};
            auto const result{peer ? posix_abi::getpeername_noexcept(fd, reinterpret_cast<::sockaddr*>(&name), &length)
                                   : posix_abi::getsockname_noexcept(fd, reinterpret_cast<::sockaddr*>(&name), &length)};
            if(result != 0 || name.sun_family != AF_UNIX ||
               length < offsetof(::sockaddr_un, sun_path) || length > sizeof(name)) { return false; }
            auto const path_bytes{static_cast<::std::size_t>(length) - offsetof(::sockaddr_un, sun_path)};
            // [sun_path, sun_path + path_bytes) lies within name because
            // length <= sizeof(name); every byte must be zero for an unnamed
            // socketpair endpoint. A pathname or abstract name is rejected.
            for(::std::size_t i{}; i != path_bytes; ++i)
            { if(name.sun_path[i] != '\0') { return false; } }
            return true;
        }
        [[nodiscard]] bool peer_live() const noexcept
        {
            if(posix_abi::getppid_noexcept() != peer_pid_ || posix_abi::kill_noexcept(peer_pid_, 0) != 0) { return false; }
            ::pid_t current_pid{};
            ::socklen_t length{sizeof(current_pid)};
            if(posix_abi::getsockopt_noexcept(descriptor_.native_handle(), SOL_LOCAL, LOCAL_PEERPID, &current_pid, &length) != 0 ||
               length != sizeof(current_pid) || current_pid != peer_pid_) { return false; }
            ::audit_token_t token{};
            length = sizeof(token);
            if(posix_abi::getsockopt_noexcept(descriptor_.native_handle(), SOL_LOCAL, LOCAL_PEERTOKEN, &token, &length) != 0 ||
               length != sizeof(token) || ::std::memcmp(&token, &peer_token_, sizeof(token)) != 0) { return false; }
            ::xucred credential{};
            length = sizeof(credential);
            return posix_abi::getsockopt_noexcept(descriptor_.native_handle(), SOL_LOCAL, LOCAL_PEERCRED, &credential, &length) == 0 &&
                   length == sizeof(credential) && credential.cr_version == XUCRED_VERSION &&
                   credential.cr_ngroups > 0 && credential.cr_uid == peer_uid_ && credential.cr_gid == peer_gid_;
        }
        [[nodiscard]] bool send_bytes(char const* bytes, ::std::size_t size) const noexcept
        {
            ::std::size_t sent{};
            auto const deadline{::std::chrono::steady_clock::now() + frame_timeout};
            while(sent != size)
            {
                if(!peer_live() || ::std::chrono::steady_clock::now() >= deadline) { return false; }
                // [bytes, bytes + sent) was sent; [bytes + sent, bytes + size)
                // remains readable because sent <= size is maintained below.
                ::ssize_t count{posix_abi::send_noexcept(descriptor_.native_handle(), bytes + sent, size - sent, MSG_DONTWAIT | MSG_NOSIGNAL)};
                if(count > 0) { sent += static_cast<::std::size_t>(count); continue; }
                if(count == -1 && errno == EINTR) { continue; }
                if(count != -1 || (errno != EAGAIN && errno != EWOULDBLOCK)) { return false; }
                ::pollfd wait{descriptor_.native_handle(), POLLOUT, 0};
                int ready{};
                do { ready = posix_abi::poll_noexcept(&wait, 1u, 50); } while(ready == -1 && errno == EINTR);
                if(ready == -1 || (wait.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) { return false; }
            }
            return true;
        }
        [[nodiscard]] bool send_text(char const* bytes, ::std::size_t size) const noexcept
        {
            if(size > maximum_reply_bytes || (size != 0u && bytes == nullptr)) { return false; }
            auto const length{static_cast<::std::uint32_t>(size)};
            auto const header{::fast_io::big_endian(length)};
            static_assert(sizeof(header) == 4u);
            // [four initialized owned wire bytes] unsafe (one-past header)
            // ^^ the object representation supplies the exact bounded prefix;
            // send_bytes borrows it only while this owned word remains alive.
            return send_bytes(reinterpret_cast<char const*>(::std::addressof(header)), sizeof(header)) && send_bytes(bytes, size);
        }
        [[nodiscard]] bool send_text(::std::string const& message) const noexcept
        {
            if(message.size() > maximum_reply_bytes)
            { return send_literal("error: bounded debugger response exceeded 65536 bytes\n"); }
            return send_text(message.data(), message.size());
        }
        template <::std::size_t N>
        [[nodiscard]] bool send_literal(char const (&message)[N]) const noexcept
        { return send_text(message, N - 1u); }

        enum class read_result { data, closed };
        [[nodiscard]] read_result read_exact(char* bytes, ::std::size_t size, controller& control) noexcept
        {
            ::std::size_t received{};
            auto const deadline{::std::chrono::steady_clock::now() + frame_timeout};
            while(received != size)
            {
                if(!peer_live() || ::std::chrono::steady_clock::now() >= deadline) { return read_result::closed; }
                auto const state{control.inspect().execution};
                if(state == execution_status::exited || state == execution_status::closed) { return read_result::closed; }
                // [bytes, bytes + received) was initialized by recvmsg;
                // [bytes + received, bytes + size) is writable, with
                // received <= size. Never read a frame's following bytes.
                ::iovec payload{bytes + received, size - received};
                alignas(::cmsghdr) ::std::array<unsigned char, CMSG_SPACE(16u * sizeof(int))> ancillary{};
                ::msghdr message{};
                message.msg_iov = &payload; message.msg_iovlen = 1u;
                message.msg_control = ancillary.data(); message.msg_controllen = ancillary.size();
                ::ssize_t count{};
                do { count = posix_abi::recvmsg_noexcept(descriptor_.native_handle(), &message, MSG_DONTWAIT); } while(count == -1 && errno == EINTR);
                if(count == 0) { return read_result::closed; }
                if(count == -1)
                {
                    if(errno != EAGAIN && errno != EWOULDBLOCK) { return read_result::closed; }
                    ::pollfd wait{descriptor_.native_handle(), POLLIN, 0};
                    int ready{};
                    do { ready = posix_abi::poll_noexcept(&wait, 1u, 50); } while(ready == -1 && errno == EINTR);
                    if(ready == -1 || (wait.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) { return read_result::closed; }
                    continue;
                }
                bool unexpected{(message.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) != 0};
                // The kernel bounds CMSG_NXTHDR to msg_controllen. Any passed
                // FD is closed immediately, including in a rejected frame.
                for(auto* item{CMSG_FIRSTHDR(&message)}; item != nullptr; item = CMSG_NXTHDR(&message, item))
                {
                    unexpected = true;
                    if(item->cmsg_len < CMSG_LEN(0)) { continue; }
                    if(item->cmsg_level == SOL_SOCKET && item->cmsg_type == SCM_RIGHTS)
                    {
                        auto const length{item->cmsg_len - CMSG_LEN(0)};
                        for(::std::size_t offset{}; offset + sizeof(int) <= length; offset += sizeof(int))
                        {
                            int passed{};
                            // [CMSG_DATA, CMSG_DATA + length) is kernel-owned
                            // ancillary storage; offset + sizeof(int) <= length.
                            ::std::memcpy(&passed, CMSG_DATA(item) + offset, sizeof(passed));
                            ::fast_io::native_file rejected_right{passed};
                        }
                    }
                }
                if(unexpected || !peer_live()) { return read_result::closed; }
                received += static_cast<::std::size_t>(count); // recvmsg wrote at most size-received bytes.
            }
            return read_result::data;
        }
        struct received_frame
        {
            ::std::array<char, maximum_command_bytes> bytes{};
            ::std::size_t size{};
            bool valid{}, closed{};
        };
        [[nodiscard]] received_frame receive_frame(controller& control) noexcept
        {
            received_frame frame{};
            char header[4]{};
            if(read_exact(header, sizeof(header), control) != read_result::data)
            { frame.closed = true; return frame; }
            ::std::uint32_t length{};
            // [four recv-initialized owned wire bytes] unsafe (one-past header)
            // ^^ first                                ^^ header+4 is formed
            // only after read_exact proves the complete four-byte extent.
            auto const parsed{::fast_io::parse_by_scan(header, header + sizeof(header), ::fast_io::mnp::be_get<32u>(length))};
            if(parsed.code != ::fast_io::parse_code::ok || parsed.iter != header + sizeof(header))
            { frame.closed = true; return frame; }
            if(length == 0u || length > maximum_command_bytes) { frame.closed = true; return frame; }
            // [frame.bytes, frame.bytes + length) is writable and bounded by
            // maximum_command_bytes; no pointer crosses the command buffer.
            if(read_exact(frame.bytes.data(), length, control) != read_result::data)
            { frame.closed = true; return frame; }
            frame.size = length;
            frame.valid = true;
            return frame;
        }
        void close_endpoint() noexcept
        {
            if(descriptor_)
            {
                // The sealed CLOEXEC duplicate pins the same socket object.
                // Shutdown invalidates both aliases before descriptor reuse.
                posix_abi::shutdown_noexcept(descriptor_.native_handle(), SHUT_RDWR);
                // reset() is the nonthrowing RAII retirement; no retry can
                // accidentally close a reused POSIX descriptor.
                descriptor_.reset();
            }
        }
        struct close_on_serve_exit
        {
            macos_control_fd& channel;
            controller& control;
            ~close_on_serve_exit()
            {
                if(!control.detach_resume()) { posix_abi::_exit_noexcept(1); }
                channel.close_endpoint();
            }
        };
    public:
        macos_control_fd(macos_control_fd const&) = delete;
        macos_control_fd& operator=(macos_control_fd const&) = delete;
        ~macos_control_fd() { close_endpoint(); }
        [[nodiscard]] static ::std::unique_ptr<macos_control_fd> adopt(int fd) noexcept
        {
            if(fd < 3) { return {}; }
            ::fast_io::native_file supplied{fd};
            int type{};
            ::socklen_t length{sizeof(type)};
            if(posix_abi::getsockopt_noexcept(fd, SOL_SOCKET, SO_TYPE, &type, &length) != 0 || length != sizeof(type) || type != SOCK_STREAM ||
               !unnamed_unix_socket(fd, false) || !unnamed_unix_socket(fd, true)) { return {}; }
            ::pid_t pid{};
            length = sizeof(pid);
            if(posix_abi::getsockopt_noexcept(fd, SOL_LOCAL, LOCAL_PEERPID, &pid, &length) != 0 || length != sizeof(pid) ||
               pid <= 1 || pid == posix_abi::getpid_noexcept() || pid != posix_abi::getppid_noexcept()) { return {}; }
            ::xucred credential{};
            length = sizeof(credential);
            if(posix_abi::getsockopt_noexcept(fd, SOL_LOCAL, LOCAL_PEERCRED, &credential, &length) != 0 ||
               length != sizeof(credential) || credential.cr_version != XUCRED_VERSION || credential.cr_ngroups <= 0 ||
               credential.cr_uid != posix_abi::geteuid_noexcept() || credential.cr_gid != posix_abi::getegid_noexcept()) { return {}; }
            ::audit_token_t token{};
            length = sizeof(token);
            if(posix_abi::getsockopt_noexcept(fd, SOL_LOCAL, LOCAL_PEERTOKEN, &token, &length) != 0 || length != sizeof(token)) { return {}; }
            // Ownership moves only after successful allocation. Rejection still
            // retires the inherited endpoint through supplied's native_file.
            auto channel{::std::unique_ptr<macos_control_fd>{new (::std::nothrow) macos_control_fd(
                ::std::move(supplied), pid, credential.cr_uid, credential.cr_gid, token)}};
            if(!channel) { return {}; }
            if(!channel->peer_live()) { return {}; }
            int const flags{posix_abi::fcntl_noexcept(fd, F_GETFD)};
            int disable_pipe_signal{1};
            if(flags == -1 || posix_abi::fcntl_noexcept(fd, F_SETFD, flags | FD_CLOEXEC) == -1 ||
               posix_abi::setsockopt_noexcept(fd, SOL_SOCKET, SO_NOSIGPIPE, &disable_pipe_signal, sizeof(disable_pipe_signal)) != 0 ||
               ::uwvm2::utils::control::seal_console_input_host_api(fd, true) !=
                   ::uwvm2::utils::control::sealed_input_status::ok) { return {}; }
            for(int guest_stdio{}; guest_stdio != 3; ++guest_stdio)
            {
                if(::uwvm2::utils::control::inspect_guest_file_host_api(guest_stdio) !=
                   ::uwvm2::utils::control::sealed_input_decision::allow) { return {}; }
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
                if(state == execution_status::exited || state == execution_status::closed || !peer_live()) { break; }
                ::pollfd wait{descriptor_.native_handle(), POLLIN, 0};
                int ready{};
                do { ready = posix_abi::poll_noexcept(&wait, 1u, 50); } while(ready == -1 && errno == EINTR);
                if(ready == -1 || (wait.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) { break; }
                if((wait.revents & POLLIN) == 0) { continue; }
                auto const frame{receive_frame(control)};
                if(frame.closed) { break; }
                if(!frame.valid) { continue; }
                auto const command{parse_console_command(command_view{frame.bytes.data(), frame.size})};
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
                            auto const message{unsupported_step_message(command)};
                            if(!send_text(message.data(), message.size())) { return; }
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
