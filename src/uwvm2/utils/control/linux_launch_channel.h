/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/push_macros.h>
# include <fast_io_dsal/array.h>
# include <cerrno>
# include <cstddef>
# include <cstdint>
# include <memory>
# include <utility>
# include <fast_io.h>
# include "session.h"
# if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(__linux__)
#  include <fcntl.h>
#  include <poll.h>
#  include <sys/random.h>
#  include <sys/socket.h>
#  include <sys/syscall.h>
#  include <sys/types.h>
#  include <unistd.h>
#  include "posix_abi.h"
# endif
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::utils::control
{
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(__linux__) && defined(SYS_pidfd_open) && defined(SCM_CREDENTIALS)
    // An actual, unnamed launch transport, created BEFORE fork and guest entry.
    // The launcher retains one endpoint; its VM child retains the other. There
    // is no raw-FD adoption, bind/listen, pathname, argv or environment fallback.
    // Every packet authenticates kernel SCM_CREDENTIALS against the creator's
    // PID/UID/GID. A creator pidfd opened before fork also rejects its death/PID
    // reuse. Native host code remains trusted; guest tables must never expose
    // these handles. This initial adapter does not implement exec bootstrapping.
    class linux_launch_channel
    {
        enum class role { unselected, launcher, receiver, closed } selected{};
        // These exclusively own inherited socket/pidfd handles. Numeric
        // native_handle() views below are synchronous borrows, never adopters.
        ::fast_io::native_file launcher_fd{}, receiver_fd{}, creator_pidfd{};
        ::pid_t const creator_pid;
        ::uid_t const creator_uid;
        ::gid_t const creator_gid;
        ::pid_t receiver_pid{};
        launch_config options;
        launch_permit permit{};
        ::std::unique_ptr<control_session> session{};
        bool authenticated{};
        ::fast_io::array<wire_byte, max_frame_bytes> packet{};

        explicit linux_launch_channel(launch_config const& config) noexcept
            : creator_pid{::uwvm2::utils::control::posix_abi::getpid_noexcept()}, creator_uid{::uwvm2::utils::control::posix_abi::getuid_noexcept()}, creator_gid{::uwvm2::utils::control::posix_abi::getgid_noexcept()}, options{config} {}
        static void retire_file(::fast_io::native_file& file) noexcept
        {
            // release() first invalidates the persistent owner. The sole
            // temporary native_file retires it once, without close()'s throwing
            // error API or an EINTR retry on a possibly recycled Linux FD.
            ::fast_io::native_file retired{file.release()};
        }
        [[nodiscard]] error launcher_live() const noexcept
        {
            ::pollfd descriptor{creator_pidfd.native_handle(), POLLIN, 0};
            int status;
            do { status = ::uwvm2::utils::control::posix_abi::poll_noexcept(::std::addressof(descriptor), 1, 0); } while(status == -1 && errno == EINTR);
            if(status == -1) { return error::transport_failure; }
            if(status != 0 || descriptor.revents != 0) { return error::launcher_exited; }
            return error::none;
        }
        [[nodiscard]] control_session::receive_result reject(error why) noexcept
        { close(); return {why, 0u, {}}; }
    public:
        linux_launch_channel(linux_launch_channel const&) = delete;
        linux_launch_channel& operator=(linux_launch_channel const&) = delete;
        ~linux_launch_channel() { close(); }
        [[nodiscard]] static ::std::unique_ptr<linux_launch_channel> create(launch_config config, error& failure)
        {
            // Peer fields are derived from this launch, not a caller's claimed
            // identity. Console qualification here only checks capabilities and
            // routing identity; the eventual receiver is always an external peer.
            config.origin = launch_origin::console;
            config.vm_process = static_cast<::std::uint64_t>(::uwvm2::utils::control::posix_abi::getpid_noexcept());
            failure = qualify(config);
            if(failure != error::none) { return {}; }
            auto result{::std::unique_ptr<linux_launch_channel>{new linux_launch_channel{config}}};
            // Adopt the newly created exclusive FD before any later failure;
            // the result's native_file members unwind every partial creation.
            auto const opened_pidfd{::fast_io::system_call<SYS_pidfd_open, int>(result->creator_pid, 0u)};
            // Real Linux success is a nonnegative FD. Reject native -errno
            // BEFORE any owner adopts it; no retry or alternate identity source.
            if(opened_pidfd < 0)
            {
                errno = -opened_pidfd; // preserve the former SDK syscall failure's observable errno
                failure = error::unsupported_transport; return {};
            }
            result->creator_pidfd.reset(opened_pidfd);
            int endpoints[2];
            if(::uwvm2::utils::control::posix_abi::socketpair_noexcept(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, 0, endpoints) != 0)
            { failure = error::transport_failure; return {}; }
            // socketpair published both exclusive handles; these noexcept
            // adoptions cannot throw between the first and second transfer.
            result->launcher_fd.reset(endpoints[0]); result->receiver_fd.reset(endpoints[1]);
            int enable{1};
            if(::uwvm2::utils::control::posix_abi::setsockopt_noexcept(result->receiver_fd.native_handle(), SOL_SOCKET, SO_PASSCRED, ::std::addressof(enable), sizeof(enable)) != 0)
            { failure = error::transport_failure; return {}; }
            ::std::uint64_t binding{};
            ::std::size_t obtained{};
            auto const bytes{reinterpret_cast<unsigned char*>(::std::addressof(binding))};
            while(obtained != sizeof(binding))
            {
                // [obtained bytes | remaining bytes] entirely inside binding.
                auto const count{::uwvm2::utils::control::posix_abi::getrandom_noexcept(bytes + obtained, sizeof(binding) - obtained, 0)};
                if(count == -1 && errno == EINTR) { continue; }
                if(count <= 0) { failure = error::transport_failure; return {}; }
                obtained += static_cast<::std::size_t>(count);
            }
            if(binding == 0u) { failure = error::transport_failure; return {}; }
            result->options.origin = launch_origin::launcher_channel;
            result->options.launcher_process = static_cast<::std::uint64_t>(result->creator_pid);
            result->options.channel_binding = binding;
            failure = error::none;
            return result;
        }
        [[nodiscard]] launch_config receiver_config() const noexcept
        {
            auto config{options};
            config.vm_process = static_cast<::std::uint64_t>(::uwvm2::utils::control::posix_abi::getpid_noexcept());
            return config;
        }
        [[nodiscard]] error select_launcher() noexcept
        {
            if(selected != role::unselected) { return error::invalid_state; }
            if(::uwvm2::utils::control::posix_abi::getpid_noexcept() != creator_pid) { return error::wrong_peer; }
            retire_file(receiver_fd); retire_file(creator_pidfd); selected = role::launcher;
            return error::none;
        }
        [[nodiscard]] error select_receiver(launch_permit&& grant)
        {
            if(selected != role::unselected) { return error::invalid_state; }
            if(::uwvm2::utils::control::posix_abi::getpid_noexcept() == creator_pid) { return error::same_process; }
            receiver_pid = ::uwvm2::utils::control::posix_abi::getpid_noexcept();
            retire_file(launcher_fd);
            permit = ::std::move(grant);
            session = ::std::make_unique<control_session>();
            selected = role::receiver;
            return error::none;
        }
        // Host event-loop polling only. Knowing/inheriting this FD does not mint
        // a permit, replace kernel credentials, or bypass per-packet checks.
        [[nodiscard]] int native_handle() const noexcept
        { return selected == role::launcher ? launcher_fd.native_handle() : selected == role::receiver ? receiver_fd.native_handle() : -1; }
        [[nodiscard]] error send_packet(input_buffer data) noexcept
        {
            if(selected != role::launcher) { return error::invalid_state; }
            if(::uwvm2::utils::control::posix_abi::getpid_noexcept() != creator_pid) { return error::wrong_peer; }
            auto const size{remaining_bytes(data)};
            if(size == 0u) { return error::malformed; }
            if(size > max_frame_bytes) { return error::oversized; }
            ::ssize_t count;
            do { count = ::uwvm2::utils::control::posix_abi::send_noexcept(launcher_fd.native_handle(), data.curr_ptr, size, MSG_DONTWAIT | MSG_NOSIGNAL); } while(count == -1 && errno == EINTR);
            if(count == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) { return error::would_block; }
            return count >= 0 && static_cast<::std::size_t>(count) == size ? error::none : error::transport_failure;
        }
        [[nodiscard]] control_session::receive_result receive()
        {
            if(selected != role::receiver) { return {error::invalid_state, 0u, {}}; }
            if(::uwvm2::utils::control::posix_abi::getpid_noexcept() != receiver_pid) { return reject(error::wrong_peer); }
            if(session->status() == session_state::pending) { return {error::busy, 0u, {}}; }
            if(auto const live{launcher_live()}; live != error::none) { return reject(live); }
            ::iovec data{packet.data(), packet.size()};
            // Alignment permits cmsghdr access; CMSG_DATA itself may have weaker
            // alignment, so credentials and received FDs are copied with FastIO memcpy.
            alignas(::cmsghdr) ::fast_io::array<::std::byte, CMSG_SPACE(sizeof(::ucred)) + CMSG_SPACE(16 * sizeof(int))> ancillary{};
            ::msghdr message{};
            message.msg_iov = ::std::addressof(data); message.msg_iovlen = 1;
            message.msg_control = ancillary.data(); message.msg_controllen = ancillary.size();
            ::ssize_t count;
            do { count = ::uwvm2::utils::control::posix_abi::recvmsg_noexcept(receiver_fd.native_handle(), ::std::addressof(message), MSG_DONTWAIT | MSG_CMSG_CLOEXEC); }
            while(count == -1 && errno == EINTR);
            if(count == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) { return {error::would_block, 0u, {}}; }
            if(count < 0) { return reject(error::transport_failure); }

            ::ucred credentials{};
            unsigned credential_count{};
            bool unexpected{};
            // Kernel-generated cmsg lengths are bounded by msg_controllen.
            // Each CMSG_NXTHDR advance is checked by the libc bounds helper.
            for(auto* item{CMSG_FIRSTHDR(::std::addressof(message))}; item != nullptr;
                item = CMSG_NXTHDR(::std::addressof(message), item))
            {
                if(item->cmsg_level == SOL_SOCKET && item->cmsg_type == SCM_CREDENTIALS && item->cmsg_len == CMSG_LEN(sizeof(::ucred)))
                { ::fast_io::freestanding::my_memcpy(::std::addressof(credentials), CMSG_DATA(item), sizeof(credentials)); ++credential_count; }
                else
                {
                    unexpected = true;
                    if(item->cmsg_level == SOL_SOCKET && item->cmsg_type == SCM_RIGHTS && item->cmsg_len >= CMSG_LEN(0))
                    {
                        auto const bytes{item->cmsg_len - CMSG_LEN(0)};
                        for(::std::size_t offset{}; offset + sizeof(int) <= bytes; offset += sizeof(int))
                        {
                            int received_fd;
                            // [rights bytes] offset+sizeof(int) <= checked cmsg payload.
                            ::fast_io::freestanding::my_memcpy(::std::addressof(received_fd), CMSG_DATA(item) + offset, sizeof(received_fd));
                            // Every delivered SCM_RIGHTS FD becomes an owner
                            // immediately, even on the rejecting packet path.
                            ::fast_io::native_file received{received_fd};
                        }
                    }
                }
            }
            // Always close delivered SCM_RIGHTS before rejecting, including
            // ancillary truncation. Linux closes rights not delivered to us.
            if(unexpected || (message.msg_flags & MSG_CTRUNC) != 0) { return reject(error::unexpected_ancillary); }
            if((message.msg_flags & MSG_TRUNC) != 0) { return reject(error::oversized); }
            if(count == 0)
            {
                auto const end{session->finish_stream()}; close();
                return {end == error::none ? error::closed : end, 0u, {}};
            }
            if(credential_count != 1) { return reject(error::unauthorized); }
            if(credentials.pid == receiver_pid) { return reject(error::same_process); }
            if(credentials.pid != creator_pid || credentials.uid != creator_uid || credentials.gid != creator_gid)
            { return reject(error::wrong_peer); }
            // A pidfd remains tied to the original creator even after PID reuse.
            // Recheck after receipt so a dead launcher's buffered packets cannot
            // authorize a new command merely because the socket was inherited.
            if(auto const live{launcher_live()}; live != error::none) { return reject(live); }
            if(!authenticated)
            {
                auto const auth{session->attach_launcher(::std::move(permit),
                    {static_cast<::std::uint64_t>(credentials.pid), options.channel_binding})};
                if(auth != error::none) { return reject(auth); }
                authenticated = true;
            }
            // recvmsg wrote at most packet.size(); MSG_TRUNC was rejected above.
            auto result{session->receive(input_buffer{packet.data(), packet.data() + count})};
            // One packet may hold one whole frame or a fragment, never silently
            // drop a concatenated second frame from a record-oriented transport.
            if(result.status == error::none && result.consumed != static_cast<::std::size_t>(count)) { return reject(error::malformed); }
            if(result.status != error::none) { close(); }
            return result;
        }
        [[nodiscard]] error complete(request_ticket const& ticket, host_completion result) noexcept
        {
            if(selected != role::receiver || ::uwvm2::utils::control::posix_abi::getpid_noexcept() != receiver_pid || !authenticated) { return error::unauthorized; }
            return session->complete(ticket, result);
        }
        void close() noexcept
        {
            if(session) { session->disconnect(); }
            retire_file(launcher_fd); retire_file(receiver_fd); retire_file(creator_pidfd);
            selected = role::closed;
        }
    };
#endif
}

#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/pop_macros.h>
#endif
