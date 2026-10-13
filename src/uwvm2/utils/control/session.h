/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/push_macros.h>
# include "protocol.h"
# if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
# include <memory>
# include <mutex>
# include <optional>
# include <utility>
# include <variant>
# include <vector>
# endif
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::utils::control
{
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    enum class backend { interpreter, llvm };
    enum class compile_mode { full, lazy, lazy_verified, tiered };
    enum class execution_state { running, stopped };
    enum class session_state { unauthorized, ready, pending, closed };
    enum class launch_origin { console, launcher_channel };

    struct launch_config
    {
        bool debug_enabled{}, replacement_enabled{}; // Both disabled by default.
        backend compiler{backend::interpreter};
        compile_mode mode{compile_mode::full};
        instance_id instance{}; // Public routing identity, NOT an authentication secret.
        ::std::uint64_t generation{1}, vm_process{}, launcher_process{}, channel_binding{};
        launch_origin origin{launch_origin::launcher_channel};
        execution_state initial_execution{execution_state::running};
    };
    [[nodiscard]] inline constexpr error qualify(launch_config const& config) noexcept
    {
        if(!config.debug_enabled && !config.replacement_enabled) { return error::disabled; }
        if(config.compiler != backend::llvm) { return error::unsupported_backend; }
        if(config.mode != compile_mode::full) { return error::unsupported_mode; }
        bool nonzero{};
        for(auto const byte: config.instance) { nonzero = nonzero || byte != 0u; }
        if(!nonzero || config.generation == 0u || config.vm_process == 0u) { return error::invalid_launch; }
        if(config.origin == launch_origin::launcher_channel &&
           (config.launcher_process == 0u || config.launcher_process == config.vm_process || config.channel_binding == 0u))
        { return error::invalid_launch; }
        return error::none;
    }
    // This is supplied only by a trusted host transport adapter after authenticating
    // its launch channel. It is NEVER decoded from a control message. A PID, UID,
    // socket FD, environment variable or loopback address cannot construct a permit.
    // The adapter must bind this evidence to that exact connection, not copy a
    // peer's claimed PID. No OS transport or credential verifier is implemented here.
    struct authenticated_launch_peer
    {
        ::std::uint64_t process{}, channel_binding{};
    };
    namespace details
    {
        struct authority
        {
            ::std::mutex mutex{};
            launch_config const config;
            bool alive{true};
            ::std::uint64_t epoch{1}, issued{}, active_session{}, last_request{};
            execution_state execution;
            explicit authority(launch_config const& value) : config{value}, execution{value.initial_execution} {}
        };
    }
    class launch_authority;
    class control_session;
    class launch_permit
    {
        friend class launch_authority;
        friend class control_session;
        ::std::shared_ptr<details::authority> owner{};
        ::std::uint64_t epoch{}, serial{};
        launch_permit(::std::shared_ptr<details::authority> value, ::std::uint64_t current, ::std::uint64_t id) noexcept
            : owner{::std::move(value)}, epoch{current}, serial{id} {}
    public:
        launch_permit() noexcept = default; // An empty handle never grants authority.
        launch_permit(launch_permit const&) = delete;
        launch_permit& operator=(launch_permit const&) = delete;
        launch_permit(launch_permit&&) noexcept = default;
        launch_permit& operator=(launch_permit&&) noexcept = default;
    };

    // Construct only in trusted launch setup, before guest execution. There is
    // deliberately no enable/update-from-wire API. A disabled authority remains
    // disabled and allocates nothing. Native plugins are trusted host code.
    class launch_authority
    {
        ::std::shared_ptr<details::authority> owner{};
        error admission{error::disabled};
    public:
        launch_authority() noexcept = default;
        explicit launch_authority(launch_config const& config) : admission{qualify(config)}
        { if(admission == error::none) { owner = ::std::make_shared<details::authority>(config); } }
        launch_authority(launch_authority const&) = delete;
        launch_authority& operator=(launch_authority const&) = delete;
        ~launch_authority() { revoke(); }
        [[nodiscard]] error status() const noexcept { return admission; }
        [[nodiscard]] launch_permit issue_permit()
        {
            if(!owner) { return {}; }
            ::std::lock_guard lock{owner->mutex};
            if(!owner->alive || owner->issued == (::std::numeric_limits<::std::uint64_t>::max)()) { return {}; }
            return launch_permit{owner, owner->epoch, ++owner->issued};
        }
        void revoke() noexcept
        {
            if(!owner) { return; }
            ::std::lock_guard lock{owner->mutex};
            // Retained session/ticket owners keep metadata alive, but can never
            // authorize work after revocation or the launch owner's destruction.
            owner->alive = false;
            owner->active_session = 0u;
        }
    };

    struct status_command {};
    struct pause_command {};
    struct resume_command {};
    struct step_command { ::std::uint64_t thread; };
    struct read_memory_command
    { ::std::uint64_t module; ::std::uint32_t memory; ::std::uint64_t offset; ::std::uint32_t length; };
    struct replace_function_command
    {
        ::std::uint64_t module, function, expected_generation;
        // Owning bytes survive decoder reuse and transport disconnect. This is
        // an untrusted function body, never native code or an authorization to
        // publish it. The runtime must validate complete canonical type/ABI,
        // module context and old-generation identity before any replacement.
        ::std::vector<::std::byte> body;
    };
    struct detach_command {};
    struct breakpoint_set_command { ::std::uint64_t module, function, offset; };
    struct breakpoint_clear_command { ::std::uint64_t identifier; };
    struct breakpoint_list_command {};
    struct backtrace_command { ::std::uint64_t thread; };
    struct locals_command { ::std::uint64_t thread; };
    using command = ::std::variant<status_command, pause_command, resume_command, step_command,
                                   read_memory_command, replace_function_command, detach_command, breakpoint_set_command, breakpoint_clear_command,
                                   breakpoint_list_command, backtrace_command, locals_command>;

    class request_ticket
    {
        friend class control_session;
        ::std::shared_ptr<details::authority> owner{};
        ::std::uint64_t session{}, id{};
        command value{};
        request_ticket(::std::shared_ptr<details::authority> shared, ::std::uint64_t token,
                       frame_header const& header, command&& request)
            : owner{::std::move(shared)}, session{token}, id{header.request_id}, value{::std::move(request)} {}
    public:
        request_ticket(request_ticket const&) = delete;
        request_ticket& operator=(request_ticket const&) = delete;
        request_ticket(request_ticket&&) noexcept = default;
        request_ticket& operator=(request_ticket&&) noexcept = default;
        [[nodiscard]] command const& get() const noexcept { return value; }
        [[nodiscard]] ::std::uint64_t request_id() const noexcept { return id; }
    };
    enum class host_completion { rejected, inspected, paused, resumed, stepped, replaced, detached, configured };

    // One host management thread owns a session and its decoder. Runtime code
    // may revoke the launch authority concurrently; all authority/state changes
    // share its cold mutex. These objects are never read on guest execution or
    // guest-memory hot paths. Disconnect preserves the last host-confirmed
    // running/stopped state; it does not fabricate a resume or patch completion.
    class control_session
    {
        ::std::shared_ptr<details::authority> owner{};
        ::std::uint64_t serial{}, pending_id{};
        operation pending_operation{};
        session_state state{session_state::unauthorized};
        frame_decoder decoder{};

        [[nodiscard]] error live_locked() const noexcept
        {
            if(!owner->alive) { return error::revoked; }
            if(owner->active_session != serial || state == session_state::closed) { return error::closed; }
            return error::none;
        }
        void close_locked() noexcept
        {
            if(owner->active_session == serial) { owner->active_session = 0u; }
            state = session_state::closed;
            pending_id = 0u;
            decoder.reset();
        }
        [[nodiscard]] error attach(launch_permit&& permit, launch_origin origin, authenticated_launch_peer peer)
        {
            // Consume even failed credentials. The moved-from permit has no owner
            // and cannot authenticate a second connection by replaying its bits.
            auto candidate{::std::move(permit.owner)};
            if(owner || state != session_state::unauthorized) { return error::busy; }
            if(!candidate) { return error::unauthorized; }
            ::std::lock_guard lock{candidate->mutex};
            if(!candidate->alive || permit.epoch != candidate->epoch) { return error::revoked; }
            if(candidate->config.origin != origin) { return error::unauthorized; }
            if(origin == launch_origin::launcher_channel)
            {
                if(peer.process == candidate->config.vm_process) { return error::same_process; }
                if(peer.process != candidate->config.launcher_process || peer.channel_binding != candidate->config.channel_binding)
                { return error::wrong_peer; }
            }
            if(candidate->active_session != 0u) { return error::busy; }
            serial = permit.serial;
            candidate->active_session = serial;
            owner = ::std::move(candidate); // Shared ownership remains live through lock's destruction.
            state = session_state::ready;
            return error::none;
        }
        [[nodiscard]] error parse_command(frame_header const& header, input_buffer bytes, command& out) const
        {
            auto const command_kind{header.command};
            if(command_kind == operation::replace_function)
            {
                if(!owner->config.replacement_enabled) { return error::unavailable_capability; }
            }
            else if(command_kind != operation::status && command_kind != operation::detach && !owner->config.debug_enabled)
            { return error::unavailable_capability; }

            if((command_kind == operation::pause && owner->execution != execution_state::running) ||
               ((command_kind == operation::resume || command_kind == operation::step || command_kind == operation::read_memory ||
                 command_kind == operation::replace_function || command_kind == operation::locals) && owner->execution != execution_state::stopped))
            { return error::invalid_state; }

            auto const size{remaining_bytes(bytes)};
            if(command_kind == operation::breakpoint_set)
            {
                if(size != 24u) { return error::malformed; }
                breakpoint_set_command request{};
                ::fast_io::io::scan(bytes, ::fast_io::mnp::le_get<64>(request.module), ::fast_io::mnp::le_get<64>(request.function),
                                   ::fast_io::mnp::le_get<64>(request.offset));
                out = request;
            }
            else if(command_kind == operation::breakpoint_clear || command_kind == operation::backtrace || command_kind == operation::locals)
            {
                if(size != 8u) { return error::malformed; }
                ::std::uint64_t id{};
                ::fast_io::io::scan(bytes, ::fast_io::mnp::le_get<64>(id));
                if(id == 0u) { return error::malformed; }
                if(command_kind == operation::breakpoint_clear) { out = breakpoint_clear_command{id}; }
                else if(command_kind == operation::backtrace) { out = backtrace_command{id}; }
                else { out = locals_command{id}; }
            }
            else if(command_kind == operation::step)
            {
                if(size != 8u) { return error::malformed; }
                step_command request{};
                ::fast_io::io::scan(bytes, ::fast_io::mnp::le_get<64>(request.thread));
                out = request;
            }
            else if(command_kind == operation::read_memory)
            {
                if(size != 32u) { return error::malformed; }
                read_memory_command request{};
                ::std::uint32_t reserved1{}, reserved2{};
                ::fast_io::io::scan(bytes, ::fast_io::mnp::le_get<64>(request.module), ::fast_io::mnp::le_get<32>(request.memory),
                                   ::fast_io::mnp::le_get<32>(reserved1), ::fast_io::mnp::le_get<64>(request.offset),
                                   ::fast_io::mnp::le_get<32>(request.length), ::fast_io::mnp::le_get<32>(reserved2));
                if(reserved1 != 0u || reserved2 != 0u) { return error::malformed; }
                if(request.length > max_memory_read_bytes) { return error::oversized; }
                if(request.offset > (::std::numeric_limits<::std::uint64_t>::max)() - request.length) { return error::malformed; }
                out = request; // Runtime must still check the current Wasm memory's actual size.
            }
            else if(command_kind == operation::replace_function)
            {
                if(size < 32u) { return error::malformed; }
                replace_function_command request{};
                ::std::uint32_t body_size{}, reserved{};
                ::fast_io::io::scan(bytes, ::fast_io::mnp::le_get<64>(request.module), ::fast_io::mnp::le_get<64>(request.function),
                                   ::fast_io::mnp::le_get<64>(request.expected_generation), ::fast_io::mnp::le_get<32>(body_size),
                                   ::fast_io::mnp::le_get<32>(reserved));
                if(reserved != 0u) { return error::malformed; }
                if(body_size > max_function_body_bytes) { return error::oversized; }
                if(body_size == 0u || body_size != remaining_bytes(bytes) || request.expected_generation == 0u) { return error::malformed; }
                // [32 consumed metadata bytes][body_size authenticated bytes] end
                // [safe                                                         ]
                //                              ^^ bytes.curr_ptr: the fixed LE
                // scan advanced exactly 32. body_size == remaining_bytes(bytes),
                // so first+body_size is in this same frame, possibly one-past.
                // One owning copy after validation; the ticket survives decoder reuse.
                auto const first{reinterpret_cast<::std::byte const*>(bytes.curr_ptr)};
                request.body.assign(first, first + body_size);
                out = ::std::move(request);
            }
            else
            {
                if(size != 0u) { return error::malformed; }
                switch(command_kind)
                {
                    case operation::status: out = status_command{}; break;
                    case operation::breakpoint_list: out = breakpoint_list_command{}; break;
                    case operation::pause: out = pause_command{}; break;
                    case operation::resume: out = resume_command{}; break;
                    case operation::detach: out = detach_command{}; break;
                    default: return error::unsupported_command;
                }
            }
            return error::none;
        }
    public:
        struct receive_result { error status; ::std::size_t consumed; ::std::optional<request_ticket> request; };
        control_session() noexcept = default;
        control_session(control_session const&) = delete;
        control_session& operator=(control_session const&) = delete;
        ~control_session() { disconnect(); }
        [[nodiscard]] error attach_launcher(launch_permit&& permit, authenticated_launch_peer peer)
        { return attach(::std::move(permit), launch_origin::launcher_channel, peer); }
        [[nodiscard]] error attach_console(launch_permit&& permit)
        { return attach(::std::move(permit), launch_origin::console, {}); }
        [[nodiscard]] session_state status() const noexcept { return state; }
        [[nodiscard]] receive_result receive(input_buffer bytes)
        {
            if(!owner) { return {error::unauthorized, 0u, {}}; }
            ::std::lock_guard lock{owner->mutex};
            auto const live{live_locked()};
            if(live != error::none) { return {live, 0u, {}}; }
            if(state == session_state::pending) { return {error::busy, 0u, {}}; }
            auto const part{decoder.feed(bytes)};
            if(part.status != error::none) { close_locked(); return {part.status, part.consumed, {}}; }
            if(!part.complete) { return {error::none, part.consumed, {}}; }
            auto const header{decoder.header()};
            error failure{};
            if(header.instance != owner->config.instance) { failure = error::wrong_instance; }
            else if(header.generation != owner->config.generation) { failure = error::stale_generation; }
            else if(owner->last_request == (::std::numeric_limits<::std::uint64_t>::max)()) { failure = error::exhausted; }
            else if(header.request_id != owner->last_request + 1u) { failure = error::replay; }
            command parsed{};
            if(failure == error::none) { failure = parse_command(header, decoder.payload(), parsed); }
            if(failure != error::none) { close_locked(); return {failure, part.consumed, {}}; }
            // Strictly monotonic across reconnects. A rejected host operation is
            // still consumed; retry must use a new request ID, never replay it.
            owner->last_request = header.request_id;
            pending_id = header.request_id;
            pending_operation = header.command;
            state = session_state::pending;
            request_ticket accepted{owner, serial, header, ::std::move(parsed)};
            decoder.reset();
            return {error::none, part.consumed, ::std::move(accepted)};
        }
        // These acknowledgements come from the host runtime, never the wire.
        // paused/stepped require all affected executions at verified safe points;
        // replaced requires successful ABI validation and atomic publication.
        [[nodiscard]] error complete(request_ticket const& ticket, host_completion result) noexcept
        {
            if(!owner) { return error::unauthorized; }
            ::std::lock_guard lock{owner->mutex};
            auto const live{live_locked()};
            if(live != error::none) { return live; }
            if(state != session_state::pending || ticket.owner != owner || ticket.session != serial || ticket.id != pending_id)
            { return error::wrong_completion; }
            bool const valid{result == host_completion::rejected ||
                ((pending_operation == operation::status || pending_operation == operation::breakpoint_list ||
                  pending_operation == operation::backtrace || pending_operation == operation::locals) && result == host_completion::inspected) ||
                ((pending_operation == operation::breakpoint_set || pending_operation == operation::breakpoint_clear) &&
                 result == host_completion::configured) ||
                (pending_operation == operation::pause && result == host_completion::paused) ||
                (pending_operation == operation::resume && result == host_completion::resumed) ||
                // A canceled step can reach a verified cooperative pause after
                // its native event/worker retires. This host-only ACK completes
                // that same pending step; it must not leave the session busy.
                (pending_operation == operation::step &&
                 (result == host_completion::stepped || result == host_completion::paused)) ||
                (pending_operation == operation::read_memory && result == host_completion::inspected) ||
                (pending_operation == operation::replace_function && result == host_completion::replaced) ||
                (pending_operation == operation::detach && result == host_completion::detached)};
            if(!valid) { return error::wrong_completion; }
            if(result == host_completion::paused) { owner->execution = execution_state::stopped; }
            if(result == host_completion::resumed) { owner->execution = execution_state::running; }
            if(result == host_completion::detached) { close_locked(); }
            else { pending_id = 0u; state = session_state::ready; }
            return error::none;
        }
        // HOST ONLY: synchronize an asynchronous breakpoint/step stop after the
        // manager has proved domain quiescence. No wire operation can call this.
        // A pending ticket must be completed first; do not overwrite its state.
        [[nodiscard]] error confirm_execution_state(execution_state value) noexcept
        {
            if(!owner) { return error::unauthorized; }
            ::std::lock_guard lock{owner->mutex};
            auto const live{live_locked()};
            if(live != error::none) { return live; }
            if(state != session_state::ready) { return error::busy; }
            owner->execution = value;
            return error::none;
        }
        [[nodiscard]] error finish_stream() noexcept
        {
            if(!owner) { return error::unauthorized; }
            ::std::lock_guard lock{owner->mutex};
            auto const live{live_locked()};
            if(live != error::none) { return live; }
            auto const failure{decoder.finish_stream()};
            close_locked();
            return failure;
        }
        void disconnect() noexcept
        {
            if(owner) { ::std::lock_guard lock{owner->mutex}; close_locked(); }
            else { state = session_state::closed; }
        }
    };
#endif
}
#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/pop_macros.h>
#endif
