/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/push_macros.h>
# if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
# include <algorithm>
# include <array>
# include <cerrno>
# include <fast_io_dsal/array.h>
# include <fast_io_dsal/string.h>
# include <chrono>
# include <condition_variable>
# include <cstddef>
# include <cstdint>
# include <fast_io.h>
# include <limits>
# include <memory>
# include <mutex>
# include <optional>
# include <span>
# include <string>
# include <string_view>
# include <thread>
# include <utility>
# include <variant>
# include <vector>
# include <fast_io_unit/string.h>
# if defined(__linux__) || (defined(__APPLE__) && defined(__MACH__))
#  include <fcntl.h>
#  include <sys/stat.h>
#  include <unistd.h>
#  include "posix_abi.h"
# endif
# include <uwvm2/utils/control/impl.h>
# include <uwvm2/utils/thread/cooperative_pause_domain.h>
# include <uwvm2/runtime/lib/uwvm_runtime.h>
# include "command.h"
# include "management_wait_interrupt.h"
# include "native_step.h"
# include "native_continuation_linux.h"
# include "native_wasm_call_continuation.h"
# include "native_registers.h"
# include "wasm_events.h"
# include "wasm_state.h"
# include "wasm_mutation.h"
# include "wasm_path.h"
# include "wasip1_state.h"
# include "wasip1_calls.h"
# include "native_disassembly.h"
# include "native_disassembly_window.h"
# include "native_next_policy.h"
# include "native_wasm_step_boundary.h"
# include "native_branch_display.h"
# include "source_map.h"
# include "source_dwarf_query.h"
# include "source_dwarf_values.h"
# include "source_dwarf_objects.h"
# include "source_dwarf_selectors.h"
# include "source_dwarf_expression.h"
# include "source_language_expression.h"
# include "source_scalar_expression.h"
# include "source_frames.h"
# include "source_step_policy.h"
# if defined(UWVM_USE_LLVM_JIT) || defined(UWVM_USE_DEFAULT_JIT)
#  include "source_dwarf_index.h"
# endif
# endif
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger
{
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    enum class execution_status { running, stopping, stopped, exited, closed };
    enum class stop_reason { initial, requested, breakpoint, step, native_step, wasm_catchpoint, wasm_trap, wasm_uncaught };
    struct breakpoint
    {
        ::std::uint64_t identifier{}, module{}, function{}, offset{};
        ::std::uint64_t hits{}, ignore_remaining{}; bool enabled{true};
        ::fast_io::array<char, 256u> condition{};
        ::std::size_t condition_size{};
    };
    struct source_function_span
    {
        // Both fields are offsets in the Code section CONTENT. The expression
        // excludes the body-size prefix and local declaration vector.
        ::std::uint64_t function{}, expression_begin{}, expression_size{};
    };
    enum class source_inline_unavailable_reason
    { none, no_bound_metadata, invalid_metadata, metadata_limit, no_current_frame,
      stale_generation_or_stop, unmapped, ambiguous, allocation_failure, native_stop };
    struct stopped_thread
    {
        ::std::uint_least64_t identifier{};
        ::uwvm2::utils::thread::cooperative_pause_location location{};
        ::uwvm2::runtime::exception::diagnostic_trace_ref trace{};
        ::std::optional<source_location> source{};
        ::std::optional<::std::uintptr_t> native_pc{};
        ::std::optional<::uwvm2::runtime::lib::llvm_jit_debug_native_position> native_wasm_position{};
        ::std::vector<source_dwarf::inline_frame> source_inline_frames{};
        bool source_inline_available{};
        source_inline_unavailable_reason source_inline_reason{source_inline_unavailable_reason::no_bound_metadata};
    };
    struct local_value
    {
        ::std::uint_least8_t type{};
        ::fast_io::array<::std::byte, ::uwvm2::runtime::lib::llvm_jit_debug_local_slot_bytes> bytes{};
        // Preserve the original local index/type even when the fused validator
        // has not proven this nondefaultable local initialized at this site.
        // Unavailable carrier bytes are never loaded, copied or interpreted.
        bool available{};
    };
    enum class source_frame_kind { inline_scope, physical, caller };
    struct source_frame_view
    {
        source_dwarf::die_key scope{};
        ::std::size_t scope_index{source_dwarf::no_record};
        ::std::uint64_t module{}, function{}, function_generation{}, runtime_epoch{}, incarnation{};
        ::std::string name{};
        source_frame_kind kind{source_frame_kind::physical};
        bool variables_available{}; // Only the current authenticated saved-frame API admits caller DATA; no native stack borrow.
    };
    struct source_constant_origin
    {
        // Copied metadata provenance, never a runtime ticket or read authority.
        source_dwarf::die_key variable{}, scope{}, type{};
        ::std::uint64_t participant{}, code_offset{};
        bool available{};
    };
    struct native_display_identity
    {
        ::uwvm2::runtime::lib::llvm_jit_debug_native_target target{};
        bool native_instruction_stop{};
        ::std::uintptr_t pc{};
        ::std::size_t size{}; // no raw byte storage exists in the public reply
        ::std::uint_least64_t participant{}, module{}, function{}, function_generation{}, runtime_epoch{};
        native_display_identity& operator=(::uwvm2::runtime::lib::llvm_jit_debug_native_code_bytes const& source) noexcept
        {
            target = source.target; native_instruction_stop = source.native_instruction_stop; pc = source.pc; size = 0u;
            participant = source.participant; module = source.module; function = source.function;
            function_generation = source.function_generation; runtime_epoch = source.runtime_epoch;
            return *this;
        }
    };

    struct controller_reply
    {
        ::uwvm2::utils::control::error status{};
        execution_status execution{execution_status::running};
        stop_reason reason{stop_reason::requested};
        bool timed_out{};
        ::std::int_least64_t guest_exit_code{};
        ::std::uint64_t breakpoint_identifier{};
        ::std::uint64_t breakpoint_condition_identifier{};
        ::uwvm2::utils::control::error breakpoint_condition_error{};
        ::std::uint64_t source_breakpoint_function{}, source_breakpoint_offset{};
        ::std::uint_least64_t replacement_generation{};
        ::std::uintptr_t native_step_from{}, native_step_to{};
        native_next_policy::reason native_next_reason{native_next_policy::reason::none};
        native_disassembly::instruction native_instruction{};
        ::std::vector<stopped_thread> threads{};
        ::std::vector<breakpoint> breakpoints{};
        ::std::vector<::std::byte> memory{};
        ::std::vector<local_value> locals{};
        wasm_state::view wasm_state_values{};
        wasm_mutation::result wasm_mutation_value{};
        wasm_path::reply wasm_path_values{};
        wasip1_state::view wasip1_state_values{};
        wasip1_calls::view wasip1_trace_values{};
        ::std::size_t total_local_count{};
        bool locals_available{};
        ::std::vector<source_dwarf::numeric_variable> source_locals{};
        ::std::vector<source_dwarf::object_node> source_object_type{};
        bool source_object_type_available{}, source_object_value_available{};
        bool source_locals_available{};
        source_constant_origin source_constant{};
        ::std::vector<source_frame_view> source_frames{};
        ::std::uint64_t selected_source_frame{}, source_frame_thread{}, source_frame_total{}, source_frame_first{}, source_frame_physical{};
        bool source_frames_available{}, source_frame_caller_unavailable{}, source_frame_out_of_range{}, source_frame_step_unavailable{};
        source_inline_unavailable_reason source_locals_reason{source_inline_unavailable_reason::no_current_frame};
        // Public lifetime labels only, never the private pause ticket or read authority.
        ::std::uint64_t stop_identifier{}, source_stop_identifier{};
        ::std::uint64_t disassembly_stop_identifier{};
        native_display_identity disassembly_code{};
        ::fast_io::array<native_disassembly::instruction, 32u> disassembly{};
        ::fast_io::array<native_branch_display::annotation, 32u> disassembly_destinations{};
        ::std::size_t disassembly_count{};
        ::std::uintptr_t disassembly_owner_begin{}, disassembly_owner_end{};
        ::fast_io::array<char8_t, 256u> disassembly_function_name{};
        ::std::size_t disassembly_function_name_size{};
        native_registers::display_snapshot registers{};
        ::std::uint64_t registers_stop_identifier{}, wasm_catchpoint_identifier{}, wasm_terminal_participant{};
        ::std::uint64_t native_caller_stop_identifier{};
        ::std::optional<::uwvm2::runtime::lib::llvm_jit_debug_native_caller_view> native_caller{};
        ::std::optional<::uwvm2::runtime::lib::llvm_jit_debug_native_backtrace_view> native_backtrace{};
        ::std::uint64_t wasm_trace_overwritten{};
        ::std::uint64_t wasm_trace_oldest_sequence{}, wasm_trace_newest_sequence{}, wasm_trace_next_sequence{}, wasm_trace_remaining{};
        bool wasm_trace_page_available{}, wasm_trace_cursor_gap{};
        bool wasm_trace_enabled{};
        wasm_events::category wasm_trace_filter{wasm_events::category::all};
        ::std::vector<wasm_events::record> wasm_trace{};
        ::std::vector<wasm_events::catchpoint> wasm_catchpoints{};
        ::std::vector<console_command> script_commands{};
        ::std::vector<controller_reply> script_replies{};
    };

    // Exactly one trusted management thread calls command/status methods. Guest
    // observers may call on_safe_point concurrently. Only debug-enabled generated
    // code calls the observer; ordinary execution has no controller/lock/lookup.
    class controller final : public ::std::enable_shared_from_this<controller>
    {
        using domain_type = ::uwvm2::utils::thread::cooperative_pause_domain;
        using location_type = ::uwvm2::utils::thread::cooperative_pause_location;
        using trace_ref = ::uwvm2::runtime::exception::diagnostic_trace_ref;
        using clock_type = ::std::chrono::steady_clock;
        using control_error = ::uwvm2::utils::control::error;
        using completion = ::uwvm2::utils::control::host_completion;
        using operation = ::uwvm2::utils::control::operation;
        using memory_reader = bool (*)(::std::uint_least64_t, ::std::uint_least32_t,
            ::std::uint_least64_t, void*, ::std::size_t) noexcept;
        struct trace_record
        {
            ::std::uint_least64_t participant{};
            trace_ref trace{};
            ::fast_io::array<local_value, ::uwvm2::runtime::lib::llvm_jit_debug_max_captured_locals> locals{};
            ::std::size_t captured_count{}, total_count{};
            bool locals_available{};
            ::uwvm2::runtime::lib::llvm_jit_debug_native_step_site native_site{};
            location_type native_site_location{};
            domain_type::pause_ticket capture_ticket{};
            ::uwvm2::runtime::lib::llvm_jit_debug_activation_capture_owner activation_capture{};
            ::uwvm2::runtime::lib::llvm_jit_checkpoint_thread_capture_owner typed_capture{};
            ::uwvm2::runtime::lib::llvm_jit_checkpoint_capture_status typed_capture_status{
                ::uwvm2::runtime::lib::llvm_jit_checkpoint_capture_status::not_selected};
            ::std::uint_least64_t function_generation{};
        };
        ::uwvm2::utils::control::launch_config config_;
        ::uwvm2::utils::control::launch_authority authority_;
        ::uwvm2::utils::control::control_session session_{};
        ::std::shared_ptr<domain_type> domain_;
        ::std::mutex mutex_{};
        ::std::condition_variable changed_{};
        domain_type::pause_ticket pause_{};
        ::std::uint64_t next_stop_identifier_{1u}, stop_identifier_{};
        wasm_path::ledger wasm_paths_{};
        ::fast_io::array<breakpoint, 256> breakpoints_{};
        ::std::uint64_t pending_breakpoint_{}, pending_breakpoint_participant_{}, condition_checked_stop_{};
        control_error condition_error_{};
        ::fast_io::array<wasm_events::catchpoint, 256u> wasm_catchpoints_{};
        wasm_events::trace_buffer wasm_trace_{};
        wasm_events::category wasm_trace_filter_{wasm_events::category::all};
        bool wasm_trace_enabled_{};
        ::std::size_t active_wasm_catchpoints_{};
        ::std::uint64_t next_wasm_catchpoint_{1u}, stopped_wasm_catchpoint_{}, terminal_wasm_participant_{};
        ::std::vector<trace_record> traces_;
        struct source_module
        {
            source_map lines{};
            source_map_error status{source_map_error::missing_debug_line};
            bool line_only{}; // Successful original v3 mapping; no DWARF scope/variable identity.
            ::std::vector<source_function_span> functions{};
            // Retires the ORIGINAL DWARF/Code mapping after replacement. Wasm
            // opcode bytes have their own actual committed generation below;
            // a fresh body can be inspected without reviving old source metadata.
            ::std::vector<bool> invalidated{};
            ::std::vector<::uwvm2::runtime::lib::llvm_jit_debug_source_function> wasm_functions{};
            ::std::uint64_t wasm_runtime_epoch{};
#if defined(UWVM_USE_LLVM_JIT) || defined(UWVM_USE_DEFAULT_JIT)
            ::std::unique_ptr<source_dwarf::index> dwarf{};
            source_dwarf::error dwarf_status{source_dwarf::error::missing_sections};
            ::uwvm2::runtime::lib::llvm_jit_debug_source_binding_owner binding{};
#endif
        };
        [[nodiscard]] static bool source_static_local_intact(source_module const& source,
            ::uwvm2::runtime::lib::llvm_jit_debug_source_position const& position,
            source_dwarf::variable_selection const& selected) noexcept
        {
            if(!selected.immutable_cpp_object_pointer || !selected.static_location ||
               selected.location.kind != source_dwarf::plan_kind::wasm_local_value ||
               selected.location.storage != source_dwarf::wasm_location_space::local) { return true; }
            if(source.wasm_runtime_epoch != position.runtime_epoch) { return false; }
            auto const found{::std::lower_bound(source.wasm_functions.begin(),source.wasm_functions.end(),position.function,
                [](auto const& function,::std::uint64_t index) noexcept { return function.function < index; })};
            if(found == source.wasm_functions.end() || found->function != position.function ||
               found->function_generation != position.function_generation || !found->instruction_safe_points_complete ||
               found->expression_bytes.size() != found->expression_size) { return false; }
            // Static DWARF cannot keep an immutable C++ this pointer in a
            // local that this actual validated function overwrites. Never
            // reinterpret a reused integer value as a guessed guest address.
            return source_dwarf::immutable_wasm_local(found->expression_bytes,found->instruction_safe_point_bits,
                selected.location.local_index) == source_dwarf::immutable_local_status::intact;
        }
        struct source_probe
        {
            control_error status{};
            ::std::optional<source_location> value{};
        };
        ::std::vector<source_module> source_modules_{};
        struct source_frame_cursor
        {
            ::std::uint64_t participant{}, stop{}, ordinal{}, top_incarnation{}, module{}, function{}, runtime_epoch{}, function_generation{};
            source_dwarf::die_key scope{};
            ::uwvm2::runtime::lib::llvm_jit_debug_source_binding_owner binding{};
            bool bound{};
        } source_frame_cursor_{};
        source_step::origin source_step_origin_{};
        ::uwvm2::runtime::lib::llvm_jit_debug_activation_snapshot source_step_origin_activation_{};
        control_error source_step_error_{};
        bool source_step_active_{};
        bool wasm_step_active_{};
        wasm_step_policy wasm_step_policy_{wasm_step_policy::into};
        ::std::size_t source_step_stop_count_{};
        inline static constexpr ::std::size_t max_source_step_stops_{65'536u};
        ::std::uint64_t next_breakpoint_{1}, next_request_{1}, step_thread_{};
        native_step::session native_session_{};
        native_continuation_linux::event_owner native_continuation_event_{};
#if defined(__linux__) && (defined(__x86_64__) || UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE)
        ::uwvm2::runtime::lib::llvm_jit_debug_native_activation_cursor_owner native_activation_cursor_{};
        // SAME actual native session may run several NI plans while its worker
        // remains borrowed in a real signal handler. Retain every canonical
        // closed call owner until disable -> actual worker ACK -> backend clear.
        // Fixed cold capacity avoids allocation after event enable or unpark.
        ::std::array<::uwvm2::runtime::lib::llvm_jit_debug_native_call_continuation_owner, 256u> native_call_continuation_owners_{};
        // A successful perf trap disables but does NOT retire this worker's
        // signal-handler/TLS borrow. Retain each real descriptor alongside its
        // canonical owner until disable ALL -> actual worker ACK -> clear.
        ::std::array<native_continuation_linux::event_owner, 256u> native_call_continuation_events_{};
        ::std::size_t native_call_continuation_count_{};
        ::std::array<::uwvm2::runtime::lib::llvm_jit_debug_native_return_event, 256u> native_return_continuation_owners_{};
        ::std::array<native_continuation_linux::event_owner, 256u> native_return_continuation_events_{};
        ::std::size_t native_return_continuation_count_{};
#endif
        // Code inspection only; never resurrect previous source/locals after a native step.
        ::uwvm2::runtime::lib::llvm_jit_debug_activation_capture_owner native_code_capture_{};
        location_type native_location_{}, native_anchor_location_{};
        ::std::uint_least64_t native_participant_{};
        ::std::uintptr_t native_pc_{};
        bool native_owned_{}, native_external_parked_{};
        stop_reason reason_{stop_reason::requested};
        bool exited_{};
        bool source_installation_closed_{};
        ::std::int_least64_t exit_code_{};
        control_error admission_{};
        trace_ref (*capture_trace_)() noexcept{};
        memory_reader read_memory_{};
        // Owned HOST coordinator; wire commands still have one management
        // caller. It only filters genuine complete conditional stops while
        // sharing mutex_, so background execution needs no status polling.
        ::fast_io::native_thread condition_worker_{};
        bool condition_worker_stopping_{};
        ::std::uint64_t condition_resumed_stop_{};

        // Caller owns mutex_. Each actual pause, completed machine step or
        // replacement retires IDE labels even when the displayed PC is identical.
        // Exhaustion disables labels; it never wraps and resurrects an old one.
        void advance_stop_identifier_locked() noexcept
        {
            source_frame_cursor_ = {}; wasm_paths_.clear(); // retire DATA selections BEFORE minting a new stop label.
            stop_identifier_ = next_stop_identifier_;
            if(next_stop_identifier_ != 0u) { ++next_stop_identifier_; }
        }
        [[nodiscard]] domain_type::pause_ticket request_pause_locked() noexcept
        {
            auto ticket{domain_->request_pause()};
            if(ticket) { advance_stop_identifier_locked(); }
            return ticket;
        }

        // Called only from the actual compiler-emitted observer. The copied
        // expression came from the runtime-private bound publication. A stale
        // replacement/epoch never reuses old bytes as a new function's opcode.
        [[nodiscard]] wasm_events::record wasm_event_locked(::std::uint64_t participant, location_type location) const noexcept
        {
            wasm_events::record entry{};
            entry.participant = participant; entry.module = location.code_unit;
            entry.function = location.function; entry.offset = location.offset;
            entry.runtime_epoch = location.code_generation;
            if(location.code_unit >= source_modules_.size()) { return entry; }
            auto const& module{source_modules_[static_cast<::std::size_t>(location.code_unit)]};
            if(module.wasm_runtime_epoch == 0u || module.wasm_runtime_epoch != location.code_generation) { return entry; }
            auto const found{::std::lower_bound(module.wasm_functions.begin(), module.wasm_functions.end(), location.function,
                [](auto const& function, ::std::uint64_t value) { return function.function < value; })};
            if(found == module.wasm_functions.end() || found->function != location.function) { return entry; }
            auto const index{static_cast<::std::size_t>(found - module.wasm_functions.begin())};
            if(index >= module.invalidated.size() || found->function_generation == 0u ||
               location.offset >= found->expression_size) { return entry; }
            entry.function_generation = found->function_generation;
            // The private source-image budget can omit bytes of a large valid
            // function. Keep its authenticated generation so `catch wasm all`
            // can still stop at genuine emitted safe points; specific opcode
            // categories require an available bounded opcode copy.
            if(found->expression_bytes.size() != found->expression_size) { return entry; }
            entry.instruction = wasm_events::decode(found->expression_bytes, location.offset);
            return entry;
        }

        [[nodiscard]] source_probe find_source_locked(location_type location) const noexcept
        {
            if(location.code_unit >= source_modules_.size()) { return {control_error::source_debug_info_unavailable, {}}; }
            auto const& module{source_modules_[static_cast<::std::size_t>(location.code_unit)]};
            if(module.status == source_map_error::missing_debug_line)
            { return {control_error::source_debug_info_unavailable, {}}; }
            if(module.status != source_map_error::none)
            { return {control_error::source_debug_info_invalid, {}}; }
            auto const found{::std::lower_bound(module.functions.begin(), module.functions.end(), location.function,
                [](source_function_span const& entry, ::std::uint64_t id) { return entry.function < id; })};
            if(found == module.functions.end() || found->function != location.function)
            { return {control_error::source_location_unmapped, {}}; }
            auto const index{static_cast<::std::size_t>(found - module.functions.begin())};
            if(index >= module.invalidated.size()) { return {control_error::source_debug_info_invalid, {}}; }
            if(module.invalidated[index])
            { return {control_error::source_debug_info_unavailable, {}}; }
            if(location.offset >= found->expression_size ||
               location.offset > ::std::numeric_limits<::std::uint64_t>::max() - found->expression_begin)
            { return {control_error::source_debug_info_invalid, {}}; }
            // Callers pass an actual compiler-emitted safe-point location, or
            // a runtime-authenticated exact native provenance row of ORIGINAL
            // generation-one code. Both offsets come from actual LLVM opcode
            // lowering metadata, never a requested decimal/native address.
            // This immutable line lookup grants display metadata only.
            auto mapped{module.lines.lookup(location.code_unit, found->expression_begin + location.offset)};
            if(!mapped) { return {control_error::source_location_unmapped, {}}; }
            return {control_error::none, mapped};
        }

        [[nodiscard]] static control_error read_replacement_source(console_command const& command,
                                                                     ::std::vector<::std::byte>& body)
        {
            if(command.replacement_path_size == 0u || command.replacement_path_size >= command.replacement_path.size() ||
               command.replacement_path[command.replacement_path_size] != '\0') { return control_error::malformed; }
#if defined(__linux__) || (defined(__APPLE__) && defined(__MACH__))
            auto opened{::fast_io::posix_openat_nothrow(::fast_io::posix_at_entry{AT_FDCWD},
                command.replacement_path.data(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK)};
            if(!opened) { return control_error::invalid_replacement_source; }
            // This native_file owns the descriptor before any allocation can
            // throw. All failure paths retire it exactly once through fast_io.
            auto& file{opened.file};
            auto const before{::fast_io::posix_status_nothrow(file)};
            bool valid{before && before.value.type == ::fast_io::file_type::regular && before.value.size != 0u &&
                       before.value.size <= ::uwvm2::utils::control::max_function_body_bytes};
            if(valid)
            {
                body.resize(static_cast<::std::size_t>(before.value.size));
                ::std::size_t consumed{};
                while(consumed != body.size())
                {
                    auto const remaining{body.size() - consumed};
                    // [body.data(), body.data()+body.size()) is the owned body.
                    // [safe                              ] unsafe (one-past)
                    //             ^^ consumed < size before forming this cursor;
                    // the synchronous read cannot write beyond remaining bytes.
                    auto const destination{body.data() + consumed};
                    auto const result{::fast_io::posix_read_nothrow(file, destination, remaining)};
                    if(result.error == EINTR) { continue; }
                    if(!result || result.transferred == 0u || result.transferred > remaining)
                    { valid = false; break; }
                    // [already initialized prefix][transferred <= remaining]
                    // [safe                       ] unsafe (one-past)
                    //             ^^ scalar consumed advances only after this
                    // extent proof; the next pointer is formed at the loop head.
                    consumed += result.transferred;
                }
                if(valid)
                {
                    ::std::byte extra{};
                    ::fast_io::posix_read_result result{};
                    do { result = ::fast_io::posix_read_nothrow(file, ::std::addressof(extra), 1u); }
                    while(result.error == EINTR);
                    auto const after{::fast_io::posix_status_nothrow(file)};
                    valid = result && result.transferred == 0u && after &&
                            before.value.dev == after.value.dev && before.value.ino == after.value.ino &&
                            before.value.size == after.value.size && before.value.mtim == after.value.mtim &&
                            before.value.ctim == after.value.ctim;
                }
            }
            // close_nothrow releases ownership before one close, preserving
            // the checked-close rejection without a second destructor close.
            if(!::fast_io::posix_close_nothrow(file)) { valid = false; }
            if(!valid) { body.clear(); return control_error::invalid_replacement_source; }
            return control_error::none;
#elif defined(_WIN32) && !defined(_WIN32_WINDOWS) && !defined(__CYGWIN__) && !defined(__WINE__) && !defined(__BIONIC__)
            // The counted, terminated path was bounded against its OWNED array
            // above; fast_io alone converts its code units to a native path.
            // [safe path.data() ... path.data()+path_size] NUL (within array)
            //  ^^ borrowed only until the synchronous readonly factory returns.
            auto opened{::fast_io::nt_open_readonly_sync_nothrow(::fast_io::mnp::os_c_str_with_known_size(
                command.replacement_path.data(), command.replacement_path_size))};
            if(!opened) { return control_error::invalid_replacement_source; }
            // The factory's native_file owners retain both file and completion
            // event before resize or any later operation can throw. Read-only
            // sharing is not proof against an older writable mapping: compare
            // actual metadata and subsequently validate ONLY this owned body.
            auto& file{opened.file};
            auto const before{::fast_io::nt_readonly_sync_status_nothrow(file)};
            bool valid{before && before.value.type == ::fast_io::file_type::regular && before.value.size != 0u &&
                       before.value.size <= ::uwvm2::utils::control::max_function_body_bytes};
            if(valid)
            {
                body.resize(static_cast<::std::size_t>(before.value.size));
                ::std::size_t consumed{};
                while(consumed != body.size())
                {
                    auto const remaining{body.size() - consumed};
                    // [body.data(), body.data()+body.size()) is OWNED and <=65536.
                    // [safe                              ] unsafe (one-past)
                    //             ^^ consumed < size checked before forming the
                    // destination. The factory-only read retains this range
                    // and its native IOSB until any pending IO actually ends.
                    auto const destination{body.data() + consumed};
                    auto const result{::fast_io::nt_readonly_sync_read_nothrow(file, destination, remaining)};
                    if(!result || result.transferred == 0u || result.transferred > remaining)
                    { valid = false; break; }
                    // [already initialized prefix][transferred <= remaining]
                    // [safe                       ] unsafe (one-past)
                    //             ^^ scalar consumed advances after the extent
                    // proof; the next native pointer is formed at the loop head.
                    consumed += result.transferred;
                }
                if(valid)
                {
                    ::std::byte extra{};
                    // [safe one-byte owned extra] end
                    //  ^^ address passed with its exact byte count, no advance.
                    auto const result{::fast_io::nt_readonly_sync_read_nothrow(file, ::std::addressof(extra), 1u)};
                    auto const after{::fast_io::nt_readonly_sync_status_nothrow(file)};
                    valid = result && result.transferred == 0u && after &&
                            before.value.dev == after.value.dev && before.value.ino == after.value.ino &&
                            before.value.size == after.value.size && before.value.mtim == after.value.mtim &&
                            before.value.ctim == after.value.ctim;
                }
            }
            // Each native_file releases its single handle before one checked
            // native close. A failure rejects the body without double-close.
            if(!::fast_io::nt_readonly_sync_close_nothrow(file)) { valid = false; }
            if(!valid) { body.clear(); return control_error::invalid_replacement_source; }
            return control_error::none;
#else
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
            try
            {
                // The native file wrapper uses the platform ABI directly and
                // does not follow a path's final symlink by default.
                ::fast_io::native_file file{::fast_io::mnp::os_c_str(command.replacement_path.data()),
                                            ::fast_io::open_mode::in};
                auto const length{::fast_io::file_size(file)};
                if(length == 0u || length > ::uwvm2::utils::control::max_function_body_bytes)
                { return control_error::invalid_replacement_source; }
                body.resize(length);
                // [safe body.data() ... body.data()+length] unsafe (one-past)
                //                     ^^ The file size is bounded before resize.
                ::fast_io::operations::read_all_bytes(file, body.data(), body.data() + body.size());
                ::std::byte extra{};
                // [safe one-byte extra] unsafe (one-past)
                //                  ^^ Probe one extra byte to reject a grown file.
                if(::fast_io::operations::read_some_bytes(file, &extra, &extra + 1u) != &extra)
                { body.clear(); return control_error::invalid_replacement_source; }
                return control_error::none;
            }
            catch(::fast_io::error const&)
            { body.clear(); return control_error::invalid_replacement_source; }
#else
            // This SDK has no qualified error-returning filesystem provider.
            // Do not compile unconditional try/catch or substitute a throwing
            // open that terminates in noEH. Reject until its provider qualifies.
            body.clear(); return control_error::invalid_replacement_source;
#endif
#endif
        }

        explicit controller(::uwvm2::utils::control::launch_config config, ::std::size_t capacity,
                            trace_ref (*capture)() noexcept, memory_reader reader)
            : config_{config}, authority_{config}, domain_{::std::make_shared<domain_type>(capacity)}, traces_(capacity),
              capture_trace_{capture}, read_memory_{reader}
        {
            admission_ = authority_.status();
            if(admission_ == control_error::none) { admission_ = session_.attach_console(authority_.issue_permit()); }
            if(admission_ == control_error::none)
            { condition_worker_ = ::fast_io::native_thread{[this]() noexcept { manage_conditions(); }}; }
        }
        static void observe(void* context, ::std::uint_least64_t participant, location_type location) noexcept
        {
            // Runtime retains observer.context through execution drain. The raw
            // pointer borrows that owner solely for this synchronous callback.
            static_cast<controller*>(context)->on_safe_point(participant, location);
        }
        static void before_park(void* context, ::std::uint_least64_t participant, location_type location,
                                ::uwvm2::runtime::lib::llvm_jit_debug_local_view locals) noexcept
        {
            // Same owning runtime observer context as observe; called only on
            // the actual execution thread when poll has accepted a pause.
            static_cast<controller*>(context)->capture_before_park(participant, location, locals);
        }
        static bool observe_terminal(void* context, ::std::uint_least64_t participant, location_type location,
                                     wasm_events::category category) noexcept
        {
            auto& self{*static_cast<controller*>(context)};
            ::std::unique_lock lock{self.mutex_, ::std::defer_lock};
#if defined(__linux__) && (defined(__x86_64__) || UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE)
            // An opaque native continuation may need its actual worker ACK
            // while the manager holds mutex_. Apply the safe-point protocol
            // here too; no host PC/registers become a native stop authority.
            while(!lock.try_lock())
            { static_cast<void>(native_step::skip_pause_for_continuation()); ::std::this_thread::yield(); }
#else
            lock.lock();
#endif
            if(self.exited_) { return false; }
            auto event{self.wasm_event_locked(participant, location)};
            if(event.function_generation == 0u) { return false; }
            event.trapped = category == wasm_events::category::trap;
            event.uncaught = category == wasm_events::category::uncaught;
            if(self.wasm_trace_enabled_ && (self.wasm_trace_filter_ == wasm_events::category::all ||
               self.wasm_trace_filter_ == category))
            { if(!self.wasm_trace_.append(event)) { self.wasm_trace_enabled_ = false; } }
            for(auto const& point : self.wasm_catchpoints_)
            {
                if(point.identifier == 0u || !point.enabled || point.event != category ||
                   point.module != location.code_unit || point.runtime_epoch != location.code_generation ||
                   (!point.all_functions && point.function != location.function)) { continue; }
                if(!self.pause_) { self.pause_ = self.request_pause_locked(); }
                if(!self.pause_) { return false; }
                self.cancel_step_locked(); self.step_thread_ = 0u;
                self.reason_ = category == wasm_events::category::trap ? stop_reason::wasm_trap : stop_reason::wasm_uncaught;
                self.stopped_wasm_catchpoint_ = point.identifier; self.terminal_wasm_participant_ = participant;
                self.changed_.notify_all(); return true;
            }
            return false;
        }
        static bool observe_trap(void* context, ::std::uint_least64_t participant, location_type location) noexcept
        { return observe_terminal(context, participant, location, wasm_events::category::trap); }
        static bool observe_uncaught(void* context, ::std::uint_least64_t participant, location_type location) noexcept
        { return observe_terminal(context, participant, location, wasm_events::category::uncaught); }
        static bool retire_on_close(void* context, domain_type const* closed_domain) noexcept
        {
            if(context == nullptr || closed_domain == nullptr) { return false; }
            // [runtime-retained actual observer context] context_end
            // [safe                                   ] strong context ownership
            //  ^^ is copied BEFORE publication unlock/close; no guest pointer.
            auto& self{*static_cast<controller*>(context)};
            // Compare the supplied identity BEFORE any access through it. Only
            // this controller's OWNED actual domain is queried, never the input.
            if(closed_domain != self.domain_.get() || !self.domain_->is_closed()) { return false; }
            ::std::lock_guard lock{self.mutex_};
            self.cancel_step_locked();
            if(!self.release_native_locked()) { return false; }
            self.pause_ = {}; self.stopped_wasm_catchpoint_ = 0u; self.terminal_wasm_participant_ = 0u;
            self.clear_traces_locked(); self.changed_.notify_all();
            return true;
        }
        // Caller owns mutex_. Both observer lock paths invoke the SAME event
        // recorder exactly once; cancellation ACK does not skip Wasm tracing.
        void on_safe_point_locked(::std::uint_least64_t participant, location_type location) noexcept
        {
            if(exited_) { return; }
            source_installation_closed_ = true;
            // This observer is absent from ordinary LLVM-full code. Recording
            // adds neither generated memory guards nor any normal-mode locks.
            wasm_events::record event{};
            if(wasm_trace_enabled_ || active_wasm_catchpoints_ != 0u)
            { event = wasm_event_locked(participant, location); }
            if(wasm_trace_enabled_ && wasm_events::matches(wasm_trace_filter_, event.instruction))
            { if(!wasm_trace_.append(event)) { wasm_trace_enabled_ = false; } }
            if(!pause_)
            {
                bool hit{step_thread_ != 0u && step_thread_ == participant};
                auto reason{stop_reason::step};
                // Source stepping requests a real stop at this opcode. Policy
                // runs on the manager AFTER all participants have parked and
                // both source PC and true event identities can be authenticated.
                // Source stepping retains its original breakpoint policy. Wasm
                // over/out honor explicit break/catchpoints while skipping calls.
                // All participants resume and then cooperatively park.
                if(step_thread_ == 0u || wasm_step_active_ || source_step_active_)
                {
                    if(active_wasm_catchpoints_ != 0u && event.function_generation != 0u)
                    {
                        for(auto const& point : wasm_catchpoints_)
                        {
                            if(point.identifier != 0u && point.enabled && point.module == location.code_unit &&
                               point.runtime_epoch == location.code_generation &&
                               (point.all_functions || point.function == location.function) &&
                               wasm_events::matches(point.event, event.instruction))
                            {
                                hit = true; reason = stop_reason::wasm_catchpoint;
                                stopped_wasm_catchpoint_ = point.identifier; break;
                            }
                        }
                    }
                    for(auto& point : breakpoints_)
                    {
                        if(point.identifier != 0u && point.enabled && point.module == location.code_unit && point.function == location.function &&
                           point.offset == location.offset)
                        {
                            if(point.hits != ::std::numeric_limits<::std::uint64_t>::max()) { ++point.hits; }
                            if(point.ignore_remaining != 0u) { --point.ignore_remaining; continue; }
                            hit = true;
                            if(reason != stop_reason::wasm_catchpoint)
                            {
                                reason = stop_reason::breakpoint;
                                pending_breakpoint_ = point.identifier; pending_breakpoint_participant_ = participant;
                            }
                            break;
                        }
                    }
                }
                if(hit)
                {
                    pause_ = request_pause_locked();
                    if(pause_)
                    {
                        reason_ = reason;
                        auto const conditional{::std::find_if(breakpoints_.begin(),breakpoints_.end(),
                            [&](auto const& point) { return point.identifier == pending_breakpoint_ && point.condition_size != 0u; })};
                        if((wasm_step_active_ || source_step_active_) && reason != stop_reason::step &&
                           (reason != stop_reason::breakpoint || conditional == breakpoints_.end()))
                        { cancel_step_locked(); source_step_error_ = control_error::none; }
                        if(!source_step_active_ && !wasm_step_active_) { step_thread_ = 0u; }
                        changed_.notify_all();
                    }
                }
            }
        }
        void on_safe_point(::std::uint_least64_t participant, location_type location) noexcept
        {
#if defined(__linux__) && (defined(__x86_64__) || UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE)
            // This pointer is the ACTUAL selected worker's private TLS borrower,
            // not a requested participant/PC or a copied debugger label. Its
            // active identity is compared BEFORE reading the fixed session.
            auto const* const continuation{native_step::details::current_thread_session};
            if(continuation != nullptr &&
               native_step::details::active.load(::std::memory_order_acquire) == continuation)
            {
                // [actual TLS-borrowed fixed session] session_end
                // [safe                            ] only this worker retires
                //  ^^ its TLS borrower; clear/destruction require that real ACK.
                auto const phase{continuation->state.load(::std::memory_order_acquire)};
                if(phase == native_step::phase::continuation_running ||
                   phase == native_step::phase::continuation_cancel_pending)
                {
                    ::std::unique_lock lock{mutex_, ::std::defer_lock};
                    while(!lock.try_lock())
                    {
                        // A manager can cancel AFTER the state check above and
                        // wait for this worker while retaining mutex_. Service
                        // that real cancellation without owning mutex_, then
                        // retry the SAME observer instead of losing its event.
                        // ACK clears TLS and lets the manager retire the fixed
                        // session: NEVER read `continuation` again in this loop.
                        static_cast<void>(native_step::skip_pause_for_continuation());
                        ::std::this_thread::yield();
                    }
                    on_safe_point_locked(participant, location);
                    return;
                }
            }
#endif
            // Ordinary observers keep their original blocking lock semantics.
            // Ordinary LLVM-full code has no observer or continuation branch.
            ::std::lock_guard lock{mutex_};
            on_safe_point_locked(participant, location);
        }
        void capture_before_park(::std::uint_least64_t participant, location_type location,
                                 ::uwvm2::runtime::lib::llvm_jit_debug_local_view locals) noexcept
        {
            ::std::lock_guard lock{mutex_};
            if(!pause_ || exited_) { return; }
            // Cold capture runs on the actual guest native thread, before poll
            // parks and before any frame returns. It is never manager backtrace.
            // traces_ has fixed capacity, so observer stores without allocating.
            for(auto& record : traces_)
            {
                if(record.participant == participant || record.participant == 0u)
                {
                    record.participant = participant;
                    record.trace = capture_trace_ == nullptr ? trace_ref{} : capture_trace_();
                    record.native_site = ::uwvm2::runtime::lib::llvm_jit_capture_debug_native_step_site_host_api();
                    record.native_site_location = location;
                    record.capture_ticket = pause_;
                    record.activation_capture = ::uwvm2::runtime::lib::llvm_jit_debug_capture_activation_host_api(pause_);
                    record.function_generation = locals.function_generation;
                    // Only THIS actual before-park runtime callback can mint
                    // typed operands. A copied PC/local view is not authority.
                    // Retire the old owner before receiving the new episode.
                    record.typed_capture = {};
                    auto typed{::uwvm2::runtime::lib::llvm_jit_checkpoint_capture_thread_host_api(pause_)};
                    record.typed_capture_status = typed.status;
                    record.typed_capture = ::std::move(typed.capture);
                    record.captured_count = record.total_count = 0u;
                    record.locals_available = false;
                    if(locals.captured_count <= record.locals.size() && locals.total_count >= locals.captured_count &&
                       locals.types != nullptr && (locals.captured_count == 0u || (locals.values != nullptr && locals.availability != nullptr)))
                    {
                        // [compiler-owned flags ... captured_count<=256] end
                        // [safe                                        ] validate ALL markers first;
                        //  ^^ each checked flag refers to the same original local index.
                        for(::std::size_t i{}; i != locals.captured_count; ++i)
                        { if(locals.availability[i] > 1u) { return; } }
                        for(::std::size_t i{}; i != locals.captured_count; ++i)
                        {
                            record.locals[i] = {}; record.locals[i].type = locals.types[i];
                            record.locals[i].available = locals.availability[i] != 0u;
                            if(!record.locals[i].available) { continue; }
                            for(::std::size_t byte{}; byte != record.locals[i].bytes.size(); ++byte)
                            {
                                // [compiler-owned snapshot of captured_count * 16 bytes] end
                                // [safe                                               ] i < count, byte < 16; multiplication is bounded by 256.
                                //  ^^ read only a PROVEN available live slot; flag0 never reads its payload.
                                record.locals[i].bytes[byte] = locals.values[i * record.locals[i].bytes.size() + byte];
                            }
                        }
                        record.captured_count = locals.captured_count;
                        record.total_count = locals.total_count;
                        record.locals_available = true;
                    }
                    return;
                }
            }
        }
        void query_wasm_state_locked(wasm_state::request const& query, controller_reply& result) noexcept
        {
            result.wasm_state_values = {}; result.wasm_state_values.requested = wasm_state::unavailable_request(query);
            if(!wasm_state::valid(query))
            { result.wasm_state_values.result = wasm_state::status::invalid_selection; return; }
            if(!pause_ || exited_ || native_owned_)
            { result.wasm_state_values.result = wasm_state::status::requires_current_cooperative_stop; return; }
            if(result.stop_identifier == 0u || result.stop_identifier != stop_identifier_)
            { result.wasm_state_values.result = wasm_state::status::stale_stop_or_generation; return; }
            ::fast_io::array<::uwvm2::runtime::lib::llvm_jit_checkpoint_thread_capture_owner, 256u> captures{};
            ::std::size_t count{}; bool requested_participant{};
            // inspect() copied the current complete domain population. Old
            // trace slots can outlive a retired participant: NEVER submit those
            // stale slots as the current cohort, nor silently drop a live one.
            for(auto const& thread : result.threads)
            {
                auto const found{::std::find_if(traces_.begin(), traces_.end(),
                    [&](auto const& item) { return item.participant == thread.identifier; })};
                if(found == traces_.end())
                { result.wasm_state_values.result = wasm_state::status::incomplete_cohort; return; }
                auto const& record{*found};
                requested_participant |= record.participant == query.participant;
                if(!record.typed_capture)
                {
                    result.wasm_state_values.result =
                        (record.typed_capture_status == ::uwvm2::runtime::lib::llvm_jit_checkpoint_capture_status::activation_resource_exhausted ||
                         record.typed_capture_status == ::uwvm2::runtime::lib::llvm_jit_checkpoint_capture_status::allocation_failed ||
                         record.typed_capture_status == ::uwvm2::runtime::lib::llvm_jit_checkpoint_capture_status::registry_exhausted) ?
                            wasm_state::status::resource_limit :
                        record.typed_capture_status == ::uwvm2::runtime::lib::llvm_jit_checkpoint_capture_status::not_selected ?
                            wasm_state::status::not_selected : wasm_state::status::unavailable_typed_site;
                    return;
                }
                if(count == captures.size())
                { result.wasm_state_values.result = wasm_state::status::resource_limit; return; }
                captures[count++] = record.typed_capture; // fixed owner array; extent checked BEFORE write.
            }
            if(!requested_participant)
            { result.wasm_state_values.result = wasm_state::status::missing_participant; return; }
            // Management call only: NEVER enclose in a domain callback. Runtime
            // acquires ONE complete cohort -> actual host closure -> N GC leases
            // and roots -> publication, authenticates the actual opaque owners,
            // and returns detached Wasm DATA after releasing every native borrow.
            result.wasm_state_values = ::uwvm2::runtime::lib::llvm_jit_debug_query_wasm_state_host_api(
                pause_, {captures.data(), count}, query);
        }
        void mutate_wasm_state_locked(wasm_mutation::request const& query, controller_reply& result) noexcept
        {
            result.wasm_mutation_value = {}; result.wasm_mutation_value.target=query.target;result.wasm_mutation_value.module=query.module;
            result.wasm_mutation_value.index=query.index;result.wasm_mutation_value.element=query.element;
            if(!wasm_mutation::valid(query))
            { result.wasm_mutation_value.status = wasm_state::status::invalid_selection; return; }
            if(!pause_ || exited_ || native_owned_)
            { result.wasm_mutation_value.status = wasm_state::status::requires_current_cooperative_stop; return; }
            if(result.stop_identifier == 0u || result.stop_identifier != stop_identifier_)
            { result.wasm_mutation_value.status = wasm_state::status::stale_stop_or_generation; return; }
            if(query.source==wasm_mutation::source_kind::original_path || query.target==wasm_mutation::destination::member_path)
            {
#if defined(__cpp_exceptions)
                try
#endif
                {
                    wasm_path::stop_key actual{};actual.stop=result.stop_identifier;
                    if(result.threads.empty() || result.threads.size()>wasm_path::maximum_cohort)
                    { result.wasm_mutation_value.status=wasm_state::status::incomplete_cohort;return; }
                    actual.runtime_epoch=result.threads.front().location.code_generation;
                    actual.cohort.reserve(result.threads.size());
                    for(auto const& thread:result.threads)
                    {
                        auto const found{::std::find_if(traces_.begin(),traces_.end(),
                            [&](auto const& record) { return record.participant==thread.identifier; })};
                        if(found==traces_.end() || !found->typed_capture || thread.native_pc || found->function_generation==0u)
                        { result.wasm_mutation_value.status=wasm_state::status::incomplete_cohort;return; }
                        actual.cohort.push_back({thread.identifier,thread.location.code_unit,thread.location.function,
                            thread.location.offset,thread.location.code_generation,found->function_generation});
                    }
                    ::std::sort(actual.cohort.begin(),actual.cohort.end(),
                        [](auto const& a,auto const& b) { return a.participant<b.participant; });
                    // query validation proved suffix<=16 BEFORE forming span.
                    // [owned fixed suffix0..16] [checked count<=16] end
                    // [safe] no supplied pointer or native payload is consumed.
                    bool const target_path{query.target==wasm_mutation::destination::member_path};
                    auto prepared{target_path ? wasm_paths_.prepare_value(query.target_path_session,query.target_path_handle,
                        {query.target_path_suffix.data(),query.target_path_suffix_size},actual) :
                        wasm_paths_.prepare_value(query.path_session,query.path_handle,
                            {query.path_suffix.data(),query.path_suffix_size},actual)};
                    if(!prepared.data_prepared)
                    { result.wasm_mutation_value.status=prepared.result;return; }
                    if(prepared.query.participant!=query.participant)
                    { result.wasm_mutation_value.status=wasm_state::status::stale_stop_or_generation;return; }
                    auto resolved{query};
                    if(target_path)
                    { resolved.target=wasm_mutation::destination::member;resolved.target_original=::std::move(prepared.query); }
                    else
                    { resolved.source=wasm_mutation::source_kind::original_root;resolved.original=::std::move(prepared.query); }
                    // At most TWO recursions: each consumes one DATA-only path
                    // selector into immutable original indices. Both root borrows
                    // use the same complete genuine current stop/cohort identity.
                    // After resolution both selectors are ORIGINAL,
                    // so the same actual pause/cohort/host/N/source/generation
                    // API below must reborrow every complete original index.
                    // Existing successful commit advances stop ID/clears ledger.
                    mutate_wasm_state_locked(resolved,result);return;
                }
#if defined(__cpp_exceptions)
                catch(...)
                { result.wasm_mutation_value.status=wasm_state::status::allocation_failed;return; }
#endif
            }
            ::fast_io::array<::uwvm2::runtime::lib::llvm_jit_checkpoint_thread_capture_owner, 256u> captures{};
            ::std::size_t count{}; bool requested_participant{};
            // inspect() copied the current complete domain population. Old
            // trace slots can outlive a retired participant: NEVER submit those
            // stale slots as the current cohort, nor silently drop a live one.
            for(auto const& thread : result.threads)
            {
                auto const found{::std::find_if(traces_.begin(), traces_.end(),
                    [&](auto const& item) { return item.participant == thread.identifier; })};
                if(found == traces_.end())
                { result.wasm_mutation_value.status = wasm_state::status::incomplete_cohort; return; }
                auto const& record{*found};
                requested_participant |= record.participant == query.participant;
                if(!record.typed_capture)
                {
                    result.wasm_mutation_value.status =
                        (record.typed_capture_status == ::uwvm2::runtime::lib::llvm_jit_checkpoint_capture_status::activation_resource_exhausted ||
                         record.typed_capture_status == ::uwvm2::runtime::lib::llvm_jit_checkpoint_capture_status::allocation_failed ||
                         record.typed_capture_status == ::uwvm2::runtime::lib::llvm_jit_checkpoint_capture_status::registry_exhausted) ?
                            wasm_state::status::resource_limit :
                        record.typed_capture_status == ::uwvm2::runtime::lib::llvm_jit_checkpoint_capture_status::not_selected ?
                            wasm_state::status::not_selected : wasm_state::status::unavailable_typed_site;
                    return;
                }
                if(count == captures.size())
                { result.wasm_mutation_value.status = wasm_state::status::resource_limit; return; }
                captures[count++] = record.typed_capture; // fixed owner array; extent checked BEFORE write.
            }
            if(!requested_participant)
            { result.wasm_mutation_value.status = wasm_state::status::missing_participant; return; }
            // Management call only: NEVER enclose in a domain callback. Runtime
            // acquires ONE complete cohort -> actual host closure -> N GC leases
            // and roots -> publication, authenticates the actual opaque owners,
            // and returns detached Wasm DATA after releasing every native borrow.
            result.wasm_mutation_value = ::uwvm2::runtime::lib::llvm_jit_debug_mutate_wasm_state_host_api(
                pause_, {captures.data(), count}, query);
            if(result.wasm_mutation_value.applied)
            {
                // Existing snapshots/path labels cannot survive a state write.
                // A fresh label retains the SAME genuine pause ticket/captures.
                advance_stop_identifier_locked();result.stop_identifier=stop_identifier_;
            }
        }
        void query_wasm_path_locked(wasm_path::request const& query,controller_reply& result) noexcept
        {
            result.wasm_path_values={};
            if(!wasm_path::valid(query)) { return; }
            if(query.action==wasm_path::operation::clear)
            { wasm_paths_.clear();result.wasm_path_values.result=wasm_state::status::available;result.wasm_path_values.cleared=true;return; }
            if(!pause_ || exited_ || native_owned_ || result.stop_identifier==0u || result.stop_identifier!=stop_identifier_)
            { result.wasm_path_values.result=wasm_state::status::requires_current_cooperative_stop;return; }
#if defined(__cpp_exceptions)
            try
#endif
            {
                wasm_path::stop_key actual{};actual.stop=result.stop_identifier;
                if(result.threads.empty() || result.threads.size()>wasm_path::maximum_cohort)
                { result.wasm_path_values.result=wasm_state::status::incomplete_cohort;return; }
                actual.runtime_epoch=result.threads.front().location.code_generation;
                actual.cohort.reserve(result.threads.size());
                for(auto const& thread:result.threads)
                {
                    auto const found{::std::find_if(traces_.begin(),traces_.end(),
                        [&](auto const& record) { return record.participant==thread.identifier; })};
                    if(found==traces_.end() || !found->typed_capture || thread.native_pc || found->function_generation==0u)
                    { result.wasm_path_values.result=wasm_state::status::incomplete_cohort;return; }
                    actual.cohort.push_back({thread.identifier,thread.location.code_unit,thread.location.function,
                        thread.location.offset,thread.location.code_generation,found->function_generation});
                }
                ::std::sort(actual.cohort.begin(),actual.cohort.end(),
                    [](auto const& a,auto const& b) { return a.participant<b.participant; });
                if(!wasm_path::valid(actual))
                { result.wasm_path_values.result=wasm_state::status::stale_stop_or_generation;return; }
                auto const prepared{wasm_paths_.prepare(query,actual)};
                if(!prepared.data_prepared)
                { result.wasm_path_values.result=prepared.result;return; }
                // A path label grants NOTHING. The exact current original root
                // and complete owned path undergo the ordinary fresh canonical
                // capture -> full cohort -> hostclose -> N leases -> source/
                // current-generation/publication proof on EVERY operation.
                query_wasm_state_locked(prepared.query,result);
                if(result.wasm_state_values.result!=wasm_state::status::available)
                { result.wasm_path_values.result=result.wasm_state_values.result;return; }
                if(!wasm_state::valid(result.wasm_state_values) || result.wasm_state_values.runtime_epoch!=actual.runtime_epoch ||
                   result.stop_identifier!=stop_identifier_)
                { wasm_paths_.clear();result.wasm_state_values={};result.wasm_path_values.result=wasm_state::status::stale_stop_or_generation;return; }
                if(query.action==wasm_path::operation::members)
                { result.wasm_path_values={wasm_state::status::available,query.session,query.handle,prepared.query.long_path.size()}; }
                else { result.wasm_path_values=wasm_paths_.commit(prepared,actual); }
            }
#if defined(__cpp_exceptions)
            catch(...)
            { result.wasm_state_values={};result.wasm_path_values={};result.wasm_path_values.result=wasm_state::status::allocation_failed; }
#endif
        }
        ::fast_io::array<::uwvm2::runtime::lib::llvm_jit_wasip1_environment_capsule_owner,8u> wasip1_checkpoints_{};
        void query_wasip1_state_locked(wasip1_state::request const& query, controller_reply& result, ::std::uint64_t expected_stop = 0u) noexcept
        {
            result.wasip1_state_values = {}; result.wasip1_state_values.operation = query.operation;
            result.wasip1_state_values.module = query.module;
            if(!wasip1_state::valid(query))
            { result.wasip1_state_values.result = wasip1_state::status::invalid_request; return; }
            if(wasip1_calls::is_trace(query.operation))
            { result.wasip1_trace_values = wasip1_calls::apply(query); return; }
            if(!pause_ || exited_ || native_owned_)
            { result.wasip1_state_values.result = wasip1_state::status::requires_current_cooperative_stop; return; }
            if(result.stop_identifier == 0u || result.stop_identifier != stop_identifier_ ||
               (expected_stop != 0u && expected_stop != stop_identifier_))
            { result.wasip1_state_values.result = wasip1_state::status::stale_stop_or_generation; return; }
            ::fast_io::array<::uwvm2::runtime::lib::llvm_jit_checkpoint_thread_capture_owner, 256u> captures{};
            ::std::size_t count{};
            // inspect() copied the current complete domain population. Old
            // trace slots can outlive a retired participant: NEVER submit those
            // stale slots as the current cohort, nor silently drop a live one.
            for(auto const& thread : result.threads)
            {
                auto const found{::std::find_if(traces_.begin(), traces_.end(),
                    [&](auto const& item) { return item.participant == thread.identifier; })};
                if(found == traces_.end())
                { result.wasip1_state_values.result = wasip1_state::status::incomplete_cohort; return; }
                auto const& record{*found};
                if(!record.typed_capture)
                {
                    result.wasip1_state_values.result =
                        (record.typed_capture_status == ::uwvm2::runtime::lib::llvm_jit_checkpoint_capture_status::activation_resource_exhausted ||
                         record.typed_capture_status == ::uwvm2::runtime::lib::llvm_jit_checkpoint_capture_status::allocation_failed ||
                         record.typed_capture_status == ::uwvm2::runtime::lib::llvm_jit_checkpoint_capture_status::registry_exhausted) ?
                            wasip1_state::status::resource_limit :
                        record.typed_capture_status == ::uwvm2::runtime::lib::llvm_jit_checkpoint_capture_status::not_selected ?
                            wasip1_state::status::not_selected : wasip1_state::status::incomplete_cohort;
                    return;
                }
                if(count == captures.size())
                { result.wasip1_state_values.result = wasip1_state::status::resource_limit; return; }
                captures[count++] = record.typed_capture; // fixed owner array; extent checked BEFORE write.
            }
            // Management call only: NEVER enclose in a domain callback. Runtime
            // acquires ONE complete cohort -> actual host closure -> N GC leases
            // and roots -> publication, authenticates the actual opaque owners,
            // and returns detached Wasm DATA after releasing every native borrow.
#include "controller_wasip1_portable.h"
            if(query.operation==wasip1_state::action::checkpoint_save || query.operation==wasip1_state::action::checkpoint_restore ||
               query.operation==wasip1_state::action::checkpoint_drop)
            {
                namespace lib=::uwvm2::runtime::lib;
                using cs=lib::llvm_jit_wasip1_environment_capsule_status;
                auto& out{result.wasip1_state_values};out.checkpoint_operation=true;out.checkpoint_slot=query.index;
                auto& slot{wasip1_checkpoints_[static_cast<::std::size_t>(query.index)]}; // valid() proved index<8.
                // Detached metadata copying may allocate. Validate it before
                // replacing a saved owner or committing a live restoration.
                auto read_metadata=[&](lib::llvm_jit_wasip1_environment_capsule_owner const& saved)
                {
                    auto const data{lib::llvm_jit_checkpoint_copy_wasip1_capsule_data_host_api(saved)};
                    if(data.status!=cs::captured || data.data.module!=query.module)
                    {
                        out.result=data.status==cs::allocation_failed?wasip1_state::status::allocation_failed:
                            wasip1_state::status::entry_not_found;
                        return false;
                    }
                    out.managed_resources=data.data.managed_resources;out.retained_external_resources=data.data.retained_external_resources;
                    out.observed_runtime_epoch=data.data.observed_runtime_epoch;
                    return true;
                };
                cs outcome{cs::invalid_capsule_owner};
                if(query.operation==wasip1_state::action::checkpoint_save)
                {
                    lib::llvm_jit_wasip1_environment_capsule_request request{};request.module=query.module;
                    request.recording_label[0u]=::std::byte{0x57u};request.recording_label[1u]=static_cast<::std::byte>(query.index+1u);
                    auto captured{lib::llvm_jit_checkpoint_capture_wasip1_environment_host_api(pause_,{captures.data(),count},request)};
                    outcome=captured.status;
                    if(outcome==cs::captured)
                    {
                        if(!read_metadata(captured.capsule)) { return; }
                        slot=::std::move(captured.capsule);out.mutation_applied=true;
                    }
                }
                else if(slot)
                {
                    if(!read_metadata(slot)) { return; }
                    if(query.operation==wasip1_state::action::checkpoint_drop)
                    { slot.reset();out.mutation_applied=true;out.result=wasip1_state::status::ok;return; }
                    outcome=lib::llvm_jit_checkpoint_restore_wasip1_environment_host_api(pause_,{captures.data(),count},slot,
                        {query.module,query.strict_resources});out.mutation_applied=outcome==cs::restored;
                }
                switch(outcome)
                {
                    case cs::captured:case cs::restored:out.result=wasip1_state::status::ok;break;
                    case cs::invalid_capsule_owner:out.result=wasip1_state::status::entry_not_found;break;
                    case cs::unsupported_resource:out.result=wasip1_state::status::unavailable_resource_rollback;break;
                    case cs::stale_environment:out.result=wasip1_state::status::stale_stop_or_generation;break;
                    case cs::resource_limit:case cs::registry_exhausted:out.result=wasip1_state::status::resource_limit;break;
                    case cs::allocation_failed:out.result=wasip1_state::status::allocation_failed;break;
                    case cs::native_operation_failed:out.result=wasip1_state::status::native_operation_failed;break;
                    default:out.result=wasip1_state::status::unavailable_environment;break;
                }
                return;
            }
            result.wasip1_state_values = ::uwvm2::runtime::lib::llvm_jit_debug_query_wasip1_state_host_api(
                pause_, {captures.data(), count}, query);
        }
        void clear_traces_locked() noexcept
        { source_frame_cursor_ = {}; wasm_paths_.clear(); for(auto& trace : traces_) { trace.trace = {}; trace.captured_count = trace.total_count = 0u;
                                    trace.locals_available = false; trace.native_site = {};
                                    trace.native_site_location = {}; trace.capture_ticket = {}; trace.activation_capture.reset(); trace.typed_capture.reset();
                                    trace.typed_capture_status = ::uwvm2::runtime::lib::llvm_jit_checkpoint_capture_status::not_selected;
                                    trace.function_generation = 0u; } }
        enum class source_caller_route { current, caller, rejected };
        struct source_caller_selection
        {
            ::uwvm2::runtime::lib::llvm_jit_debug_activation_identity identity{};
            source_module const* source{}; // Controller-owned immutable metadata ONLY.
        };
        // Fresh leaf metadata chooses a saved incarnation only. No caller Code
        // PC/local/memory is read here: the runtime's complete typed-cohort API
        // must later authenticate its actual saved plan/source/callsite jointly.
        [[nodiscard]] source_caller_route source_caller_selection_locked(trace_record const& record,
            console_command const& command, source_caller_selection& chosen, controller_reply& result)
        {
            chosen = {};
#if defined(UWVM_USE_LLVM_JIT) || defined(UWVM_USE_DEFAULT_JIT)
            bool const saved{source_frame_cursor_.bound && source_frame_cursor_.participant == record.participant};
            if(!command.source_frame_explicit && !saved) { return source_caller_route::current; }
            auto const location{record.native_site_location};
            if(native_owned_ || !pause_ || !record.activation_capture || !record.capture_ticket ||
               (command.disassembly_stop_identifier != 0u && command.disassembly_stop_identifier != stop_identifier_) ||
               location.code_unit >= source_modules_.size())
            { result.status = control_error::invalid_state; return source_caller_route::rejected; }
            // [owned source table ... code_unit<size] end
            // [safe] actual bounded leaf module precedes metadata index.
            auto const& source{source_modules_[static_cast<::std::size_t>(location.code_unit)]};
            ::uwvm2::runtime::lib::llvm_jit_debug_source_activation_snapshot actual{};
            if(!source.binding || !source.dwarf || source.dwarf_status != source_dwarf::error::none ||
               !::uwvm2::runtime::lib::llvm_jit_debug_query_source_activation_host_api(record.activation_capture, source.binding, actual) ||
               !actual.source_available || actual.activation.participant != record.participant ||
               actual.activation.location.code_unit != location.code_unit || actual.activation.location.function != location.function ||
               actual.activation.location.offset != location.offset || actual.activation.location.code_generation != location.code_generation ||
               actual.source.module != location.code_unit || actual.source.function != location.function ||
               actual.source.runtime_epoch != location.code_generation || actual.source.function_generation != record.function_generation ||
               actual.activation.frames.empty())
            { result.status = control_error::source_location_unmapped; return source_caller_route::rejected; }
            auto const& top{actual.activation.frames.back()};
            if(saved && (source_frame_cursor_.stop != stop_identifier_ || source_frame_cursor_.top_incarnation != top.incarnation ||
                source_frame_cursor_.module != actual.source.module || source_frame_cursor_.function != actual.source.function ||
                source_frame_cursor_.runtime_epoch != actual.source.runtime_epoch ||
                source_frame_cursor_.function_generation != actual.source.function_generation ||
                source_frame_cursor_.binding.get() != source.binding.get() || source_frame_cursor_.binding.owner_before(source.binding) ||
                source.binding.owner_before(source_frame_cursor_.binding)))
            { result.status = control_error::invalid_state; return source_caller_route::rejected; }
            ::std::vector<source_frames::frame> frames{};
            if(source_frames::current_frames(source.dwarf->scopes(), actual.source.code_offset, frames) != source_frames::error::none || frames.empty())
            { result.status = control_error::source_location_unmapped; return source_caller_route::rejected; }
            auto const ordinal{command.source_frame_explicit ? command.source_frame_ordinal : source_frame_cursor_.ordinal};
            if(ordinal < frames.size()) { return source_caller_route::current; }
            if(ordinal >= frames.size() + actual.activation.frames.size() - 1u)
            { result.source_frame_out_of_range = true; result.status = control_error::source_location_unmapped; return source_caller_route::rejected; }
            // Both actual extents above are bounded. Subtractions BEFORE index:
            // [owned real outer->inner activations ... caller ... leaf] end
            // [safe] ordinal-inline_count < frames_count-1 proves caller>=0.
            auto const depth{static_cast<::std::size_t>(ordinal - frames.size())};
            auto const index{actual.activation.frames.size() - 2u - depth};
            auto const& caller{actual.activation.frames[index]};
            if(caller.incarnation == 0u || caller.function_generation == 0u || caller.runtime_epoch != location.code_generation ||
               caller.module >= source_modules_.size())
            { result.source_frame_caller_unavailable = true; result.status = control_error::source_location_unmapped; return source_caller_route::rejected; }
            auto const& member{source_modules_[static_cast<::std::size_t>(caller.module)]};
            // [owned source table ... caller.module<size] end
            // [safe] bound before cached metadata index; its name/owner is NOT
            // a runtime read grant and still must match the actual saved source.
            if(!member.binding || !member.dwarf || member.dwarf_status != source_dwarf::error::none)
            { result.source_frame_caller_unavailable = true; result.status = control_error::source_debug_info_unavailable; return source_caller_route::rejected; }
            chosen.identity = caller; chosen.source = ::std::addressof(member);
            return source_caller_route::caller;
#else
            (void)record; (void)command; (void)result; return source_caller_route::current;
#endif
        }
        // Map EVERY current domain participant to its actual typed capture.
        // A selected thread is UI filtering only, never permission to omit N.
        [[nodiscard]] bool source_capture_cohort_locked(controller_reply const& result, ::std::uint64_t selected,
            ::fast_io::array<::uwvm2::runtime::lib::llvm_jit_checkpoint_thread_capture_owner, 256u>& captures,
            ::std::size_t& count) const noexcept
        {
            count = 0u; bool present{};
            if(result.threads.empty() || result.threads.size() > captures.size()) { return false; }
            for(auto const& participant : result.threads)
            {
                auto const found{::std::find_if(traces_.begin(), traces_.end(),
                    [&](auto const& trace) noexcept { return trace.participant == participant.identifier; })};
                if(found == traces_.end() || !found->typed_capture || count == captures.size()) { return false; }
                present |= found->participant == selected;
                captures[count++] = found->typed_capture; // [safe] fixed extent BEFORE index/advance.
            }
            return present;
        }
        // Numeric local enumeration is also inside the SAME authenticated caller
        // read transaction. DWARF receives native copied scalars, not raw JIT/VM
        // stack addresses; REF payloads stay unavailable in the runtime adapter.
        void source_caller_locals_locked(trace_record const& record, console_command const& command,
            source_caller_selection const& chosen, controller_reply& result)
        {
#if defined(UWVM_USE_LLVM_JIT) || defined(UWVM_USE_DEFAULT_JIT)
            result.source_stop_identifier = pause_ ? stop_identifier_ : 0u;
            result.source_locals_reason = source_inline_unavailable_reason::stale_generation_or_stop;
            if(chosen.source == nullptr || chosen.identity.incarnation == 0u)
            { result.status = control_error::source_location_unmapped; return; }
            ::fast_io::array<::uwvm2::runtime::lib::llvm_jit_checkpoint_thread_capture_owner, 256u> captures{};
            ::std::size_t count{};
            if(!source_capture_cohort_locked(result, record.participant, captures, count))
            { result.status = control_error::source_location_unmapped; return; }
            struct context { trace_record const& record; source_caller_selection const& chosen; controller_reply& result; }
                state{record, chosen, result};
            auto const callback{+[](void* opaque, ::uwvm2::runtime::lib::llvm_jit_debug_source_memory_view& reader) noexcept
            {
                if(opaque == nullptr) { return false; }
                auto const& state{*static_cast<context const*>(opaque)}; // THIS synchronous lexical recipe only.
                auto& result{state.result};
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
                try
                {
#endif
                    auto const& chosen{state.chosen}; auto const& source{*chosen.source};
                    ::uwvm2::runtime::lib::llvm_jit_debug_source_activation_snapshot actual{};
                    ::uwvm2::runtime::lib::llvm_jit_debug_source_frame_locals locals{};
                    if(!reader.copy_position(actual) || !actual.source_available || actual.activation.frames.empty() ||
                       !reader.copy_frame_locals(locals) || locals.participant != state.record.participant ||
                       locals.incarnation != chosen.identity.incarnation || locals.values.size() > 256u ||
                       locals.values.size() > locals.total_count || locals.total_count > (::std::numeric_limits<::std::size_t>::max)() ||
                       actual.activation.participant != state.record.participant ||
                       actual.activation.frames.back().incarnation != chosen.identity.incarnation ||
                       actual.source.module != chosen.identity.module || actual.source.function != chosen.identity.function ||
                       actual.source.runtime_epoch != chosen.identity.runtime_epoch ||
                       actual.source.function_generation != chosen.identity.function_generation)
                    { result.status = control_error::source_location_unmapped; return false; }
                    ::std::array<source_dwarf::copied_numeric_local, 256u> copied{};
                    for(::std::size_t i{}; i != locals.values.size(); ++i)
                    {
                        // [owned actual copied local prefix ... i<size<=256] end
                        // [safe] equal fixed bounds before native-carrier copy.
                        copied[i].wasm_type = locals.values[i].wasm_type; copied[i].available = locals.values[i].available;
                        if(copied[i].available)
                        { ::fast_io::freestanding::my_memcpy(copied[i].bytes.data(), locals.values[i].native_bytes.data(), copied[i].bytes.size()); }
                    }
                    ::std::vector<source_frames::frame> frames{};
                    if(source_frames::current_frames(source.dwarf->scopes(), actual.source.code_offset, frames) != source_frames::error::none || frames.empty())
                    { result.status = control_error::source_location_unmapped; return false; }
                    auto const anchor{frames.size() - 1u}; // Actual physical scope, not another inline frame.
                    auto const status{source_dwarf::query_numeric_variables(source.dwarf->scopes(), source.dwarf->types(),
                        source.dwarf->variables(), actual.source.code_offset, {copied.data(), locals.values.size()},
                        static_cast<::std::size_t>(locals.total_count), result.source_locals)};
                    if(status != source_dwarf::inline_query_error::none)
                    { result.status = control_error::source_location_unmapped; return false; }
                    source_frame_variables::error filter_error{source_frame_variables::error::none};
                    ::std::erase_if(result.source_locals, [&](auto const& value)
                    {
                        source_frame_variables::selection selected{};
                        auto const status{source_frame_variables::named(source.dwarf->scopes(), source.dwarf->variables(), source.dwarf->types().size(),
                            actual.source.code_offset, anchor, ::std::string_view{value.name}, selected)};
                        if(status != source_frame_variables::error::none && status != source_frame_variables::error::unavailable) { filter_error = status; }
                        return status != source_frame_variables::error::none || selected.variable_identity != value.identity;
                    });
                    if(filter_error != source_frame_variables::error::none)
                    { result.source_locals.clear(); result.status = control_error::source_debug_info_invalid; return false; }
                    result.source_locals_available = true; result.source_locals_reason = source_inline_unavailable_reason::none;
                    return true;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
                }
                catch(...) { result.source_locals.clear(); result.status = control_error::exhausted; return false; }
#endif
            }};
            if(!::uwvm2::runtime::lib::llvm_jit_debug_with_source_frame_memory_host_api(pause_, {captures.data(), count},
                record.activation_capture, chosen.source->binding, chosen.identity.incarnation, ::std::addressof(state), callback))
            { result.source_locals.clear(); result.source_locals_available = false; result.source_frame_caller_unavailable = true;
              if(result.status == control_error::none) { result.status = control_error::source_location_unmapped; } }
            else if(result.source_locals_available)
            {
                // The saved-frame callback has returned and released its runtime
                // read guard. Reenter the canonical selected-frame transaction
                // only here, under the unchanged controller-owned pause. Never
                // nest a runtime lease or substitute current/callee storage.
                for(auto& value : result.source_locals)
                {
                    if(value.kind != source_dwarf::numeric_kind::unavailable ||
                       (value.reason != source_dwarf::numeric_unavailable_reason::local_not_captured &&
                        value.reason != source_dwarf::numeric_unavailable_reason::unsupported_plan &&
                        value.reason != source_dwarf::numeric_unavailable_reason::incomplete_composite)) { continue; }
                    source_dwarf::source_expression expression{};
                    expression.root_name = ::fast_io::concat_std(::std::string_view{value.name});
                    controller_reply resolved{}; resolved.threads = result.threads;
                    source_expression_locked(record, command, expression, resolved, ::std::addressof(chosen));
                    if(resolved.status != control_error::none) { continue; }
                    if(resolved.source_locals_available && resolved.source_locals.size() == 1u &&
                       resolved.source_locals[0u].identity == value.identity)
                    { value = ::std::move(resolved.source_locals[0u]); }
                    else if(resolved.source_object_value_available && resolved.source_object_type.size() == 1u)
                    {
                        auto const& node{resolved.source_object_type[0u]};
                        if(!node.value_available || node.reason != source_dwarf::object_unavailable_reason::none ||
                           node.kind != source_dwarf::type_kind::scalar || node.scalar_kind == source_dwarf::numeric_kind::unavailable) { continue; }
                        value.type_name = node.type_name;
                        value.kind = node.scalar_kind; value.bits = node.bits; value.byte_count = node.scalar_bytes;
                        value.reason = source_dwarf::numeric_unavailable_reason::none;
                    }
                }
            }
#else
            (void)record; (void)command; (void)chosen; result.status = control_error::source_debug_info_unavailable;
#endif
        }

        // Enrich a caller label ONLY from its actual selected saved-frame read.
        // Failure preserves the explicit unavailable row; no VM unwind/native PC
        // is exposed and no metadata-only ordinal is upgraded to a read owner.
        void source_caller_frame_view_locked(trace_record const& record,
            ::uwvm2::runtime::lib::llvm_jit_debug_activation_identity const& caller,
            controller_reply const& result, source_frame_view& view)
        {
#if defined(UWVM_USE_LLVM_JIT) || defined(UWVM_USE_DEFAULT_JIT)
            if(caller.module >= source_modules_.size()) { return; }
            auto const& source{source_modules_[static_cast<::std::size_t>(caller.module)]};
            if(!source.binding || !source.dwarf || source.dwarf_status != source_dwarf::error::none) { return; }
            ::fast_io::array<::uwvm2::runtime::lib::llvm_jit_checkpoint_thread_capture_owner, 256u> captures{};
            ::std::size_t count{};
            if(!source_capture_cohort_locked(result, record.participant, captures, count)) { return; }
            auto candidate{view}; // Detached tentative DATA; publish ONLY after actual callback/manager success.
            struct context { trace_record const& record; source_module const& source;
                ::uwvm2::runtime::lib::llvm_jit_debug_activation_identity const& caller; source_frame_view& view; }
                state{record, source, caller, candidate};
            auto const callback{+[](void* opaque, ::uwvm2::runtime::lib::llvm_jit_debug_source_memory_view& reader) noexcept
            {
                if(opaque == nullptr) { return false; }
                auto const& state{*static_cast<context const*>(opaque)}; // THIS live lexical recipe, never guest memory.
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
                try
                {
#endif
                    ::uwvm2::runtime::lib::llvm_jit_debug_source_activation_snapshot actual{};
                    ::uwvm2::runtime::lib::llvm_jit_debug_source_frame_locals locals{};
                    if(!reader.copy_position(actual) || !actual.source_available || actual.activation.frames.empty() ||
                       !reader.copy_frame_locals(locals) || actual.activation.participant != state.record.participant ||
                       actual.activation.frames.back().incarnation != state.caller.incarnation ||
                       locals.participant != state.record.participant || locals.incarnation != state.caller.incarnation ||
                       actual.source.module != state.caller.module || actual.source.function != state.caller.function ||
                       actual.source.runtime_epoch != state.caller.runtime_epoch || actual.source.function_generation != state.caller.function_generation)
                    { return false; }
                    ::std::vector<source_frames::frame> frames{};
                    if(source_frames::current_frames(state.source.dwarf->scopes(), actual.source.code_offset, frames) != source_frames::error::none || frames.empty())
                    { return false; }
                    // [owned actual caller scope path ... physical last] end
                    // [safe] nonempty bounded path proved BEFORE back(). No
                    // inlined child/leaf PC substitutes for this physical caller.
                    auto const& physical{frames.back()};
                    state.view.scope = physical.identity; state.view.scope_index = physical.scope_index; state.view.name = ::fast_io::concat_std(::std::string_view{physical.name});
                    state.view.variables_available = true;
                    return true;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
                }
                catch(...) { return false; }
#endif
            }};
            if(::uwvm2::runtime::lib::llvm_jit_debug_with_source_frame_memory_host_api(pause_, {captures.data(), count},
                record.activation_capture, source.binding, caller.incarnation, ::std::addressof(state), callback))
            { view = ::std::move(candidate); }
#else
            (void)record; (void)caller; (void)result; (void)view;
#endif
        }

        // Caller holds only controller mutex_. Runtime acquires its genuine
        // execution lease -> ONE domain guard -> publication guard. In particular
        // there is no outer while_stopped/with_stopped_participant lock here.
        void source_locals_locked(trace_record const& record, console_command const& command, controller_reply& result)
        {
            // Same controller mutex as the actual copied-value/ticket query.
            // The label alone authenticates nothing; the runtime still verifies
            // record.capture_ticket, publication/source owner and generation.
            result.source_stop_identifier = pause_ ? stop_identifier_ : 0u;
            result.source_locals_reason = source_inline_unavailable_reason::no_current_frame;
            if(native_owned_ && native_external_parked_ && record.participant == native_participant_)
            { result.source_locals_reason = source_inline_unavailable_reason::native_stop; return; }
            source_caller_selection caller{};
            auto const caller_route{source_caller_selection_locked(record, command, caller, result)};
            if(caller_route == source_caller_route::rejected) { return; }
            if(caller_route == source_caller_route::caller) { source_caller_locals_locked(record, command, caller, result); return; }
            if(!record.locals_available || !record.capture_ticket || record.function_generation == 0u) { return; }
#if defined(UWVM_USE_LLVM_JIT) || defined(UWVM_USE_DEFAULT_JIT)
            auto const location{record.native_site_location}; // complete actual bridge location; not command-supplied numbers.
            if(location.code_unit >= source_modules_.size()) { return; }
            auto const& source{source_modules_[static_cast<::std::size_t>(location.code_unit)]};
            result.source_locals_reason = source_inline_unavailable_reason::no_bound_metadata;
            if(source.dwarf_status == source_dwarf::error::allocation_failure)
            { result.source_locals_reason = source_inline_unavailable_reason::allocation_failure; return; }
            if(source.dwarf_status == source_dwarf::error::limit_exceeded)
            { result.source_locals_reason = source_inline_unavailable_reason::metadata_limit; return; }
            if(source.dwarf_status != source_dwarf::error::none)
            {
                if(source.dwarf_status != source_dwarf::error::missing_sections)
                { result.source_locals_reason = source_inline_unavailable_reason::invalid_metadata; }
                return;
            }
            if(!source.dwarf || !source.binding) { return; }
            result.source_locals_reason = source_inline_unavailable_reason::stale_generation_or_stop;
            ::uwvm2::runtime::lib::llvm_jit_debug_source_activation_snapshot actual{};
            if(command.disassembly_stop_identifier != 0u && command.disassembly_stop_identifier != stop_identifier_)
            { result.status = control_error::invalid_state; return; }
            // The CAPTURED ticket authenticates this copy, even across a resume
            // and re-pause at the same PC. Current pause_/matching decimal PC
            // alone must never turn an old copied value into a current value.
            if(!::uwvm2::runtime::lib::llvm_jit_debug_query_source_activation_host_api(record.activation_capture, source.binding, actual) ||
               !actual.source_available || actual.activation.participant != record.participant ||
               actual.activation.location.code_unit != location.code_unit || actual.activation.location.function != location.function ||
               actual.activation.location.offset != location.offset || actual.activation.location.code_generation != location.code_generation ||
               actual.source.module != location.code_unit || actual.source.function != location.function ||
               actual.source.runtime_epoch != location.code_generation || actual.source.function_generation != record.function_generation) { return; }
            auto const position{actual.source};
            ::std::optional<::std::size_t> frame_anchor{};
            auto const frame_status{source_frame_anchor_locked(record, command, actual, source, frame_anchor, result)};
            if(frame_status != control_error::none) { result.status = frame_status; return; }
            auto const function{::std::find_if(source.functions.begin(), source.functions.end(),
                [&](auto const& span) { return span.function == position.function; })};
            if(function == source.functions.end() || location.offset >= function->expression_size ||
               function->expression_begin > (::std::numeric_limits<::std::uint64_t>::max)() - location.offset ||
               position.code_offset != function->expression_begin + location.offset) { return; }
            // [controller-owned copied slots: fixed 256 * 16 bytes] end
            // [safe                                                 ] no generated-frame/native address is retained or read.
            //  ^^ all borrows below are immutable controller/index storage while
            //     mutex_ excludes resume, replace, reset and another command.
            ::std::array<source_dwarf::copied_numeric_local, ::uwvm2::runtime::lib::llvm_jit_debug_max_captured_locals> copied{};
            if(record.captured_count > copied.size() || record.captured_count > record.total_count) { return; }
            for(::std::size_t i{}; i != record.captured_count; ++i)
            {
                copied[i].wasm_type = record.locals[i].type;
                copied[i].available = record.locals[i].available;
                if(!copied[i].available) { continue; }
                for(::std::size_t byte{}; byte != copied[i].bytes.size(); ++byte)
                { copied[i].bytes[byte] = record.locals[i].bytes[byte]; }
            }
            auto const queried{source_dwarf::query_numeric_variables(source.dwarf->scopes(), source.dwarf->types(),
                source.dwarf->variables(), position.code_offset, {copied.data(), record.captured_count}, record.total_count,
                result.source_locals)};
            if(queried == source_dwarf::inline_query_error::none && frame_anchor)
            {
                // All values already came from this private copied capture.
                // Filtering a selected scope cannot create another read or slot.
                source_frame_variables::error filter_error{source_frame_variables::error::none};
                ::std::erase_if(result.source_locals, [&](auto const& value)
                {
                    source_frame_variables::selection chosen{};
                    auto const status{source_frame_variables::named(source.dwarf->scopes(), source.dwarf->variables(),
                        source.dwarf->types().size(), position.code_offset, *frame_anchor, value.name, chosen)};
                    if(status != source_frame_variables::error::none && status != source_frame_variables::error::unavailable)
                    { filter_error = status; }
                    return status != source_frame_variables::error::none || chosen.variable_identity != value.identity;
                });
                if(filter_error != source_frame_variables::error::none)
                {
                    result.source_locals.clear(); result.source_locals_available = false;
                    result.status = filter_error == source_frame_variables::error::limit_exceeded ||
                        filter_error == source_frame_variables::error::allocation_failure ? control_error::exhausted : control_error::source_debug_info_invalid;
                    return;
                }
            }
            if(queried == source_dwarf::inline_query_error::none)
            {
                // Refine unsupported direct-local observations through the
                // canonical stopped frame reader. Each query reauthenticates
                // the SAME still-owned pause while mutex_ excludes resume or
                // mutation. Missing producer locations are never synthesized.
                for(auto& value : result.source_locals)
                {
                    if(value.kind != source_dwarf::numeric_kind::unavailable ||
                       (value.reason != source_dwarf::numeric_unavailable_reason::local_not_captured &&
                        value.reason != source_dwarf::numeric_unavailable_reason::unsupported_plan &&
                        value.reason != source_dwarf::numeric_unavailable_reason::incomplete_composite)) { continue; }
                    source_dwarf::source_expression expression{}; expression.root_name=::fast_io::concat_std(::std::string_view{value.name});
                    // Keep the actual stopped cohort for the canonical reader.
                    // An empty temporary reply cannot authenticate its captures.
                    controller_reply resolved{}; resolved.threads = result.threads;
                    source_expression_locked(record,command,expression,resolved);
                    if(resolved.status != control_error::none) { continue; }
                    if(resolved.source_locals_available && resolved.source_locals.size()==1u && resolved.source_locals[0u].identity==value.identity)
                    { value=::std::move(resolved.source_locals[0u]); }
                    else if(resolved.source_object_value_available && resolved.source_object_type.size()==1u)
                    {
                        auto const& node{resolved.source_object_type[0u]};
                        if(!node.value_available || node.reason!=source_dwarf::object_unavailable_reason::none ||
                           node.kind!=source_dwarf::type_kind::scalar || node.scalar_kind==source_dwarf::numeric_kind::unavailable) { continue; }
                        value.type_name=node.type_name;
                        value.kind=node.scalar_kind;value.bits=node.bits;value.byte_count=node.scalar_bytes;value.reason=source_dwarf::numeric_unavailable_reason::none;
                    }
                }
            }
            result.source_locals_available = queried == source_dwarf::inline_query_error::none;
            switch(queried)
            {
                case source_dwarf::inline_query_error::none: result.source_locals_reason = source_inline_unavailable_reason::none; break;
                case source_dwarf::inline_query_error::unavailable: result.source_locals_reason = source_inline_unavailable_reason::unmapped; break;
                case source_dwarf::inline_query_error::ambiguous: result.source_locals_reason = source_inline_unavailable_reason::ambiguous; break;
                case source_dwarf::inline_query_error::limit_exceeded: result.source_locals_reason = source_inline_unavailable_reason::metadata_limit; break;
                case source_dwarf::inline_query_error::allocation_failure: result.source_locals_reason = source_inline_unavailable_reason::allocation_failure; break;
                case source_dwarf::inline_query_error::malformed: result.source_locals_reason = source_inline_unavailable_reason::invalid_metadata; break;
            }
#endif
        }

        // Metadata only: a private jointly authenticated activation query is
        // repeated on EACH command. The ordinal is never a read capability.
        [[nodiscard]] control_error source_frame_anchor_locked(trace_record const& record, console_command const& command,
            ::uwvm2::runtime::lib::llvm_jit_debug_source_activation_snapshot const& actual,
            source_module const& source, ::std::optional<::std::size_t>& anchor, controller_reply& result)
        {
            anchor.reset();
#if defined(UWVM_USE_LLVM_JIT) || defined(UWVM_USE_DEFAULT_JIT)
            if(actual.activation.frames.empty() || !source.dwarf || !source.binding)
            { return control_error::source_location_unmapped; }
            // [owned actual activation identities ... last] end
            // [safe                                       ] nonempty/cap checked BEFORE back().
            auto const& top{actual.activation.frames.back()};
            if(top.incarnation == 0u || top.module != actual.source.module || top.function != actual.source.function ||
               top.runtime_epoch != actual.source.runtime_epoch || top.function_generation != actual.source.function_generation)
            { return control_error::source_location_unmapped; }
            bool const saved{source_frame_cursor_.bound && source_frame_cursor_.participant == record.participant};
            if(saved && (source_frame_cursor_.stop != stop_identifier_ || source_frame_cursor_.top_incarnation != top.incarnation ||
                source_frame_cursor_.module != actual.source.module || source_frame_cursor_.function != actual.source.function ||
                source_frame_cursor_.runtime_epoch != actual.source.runtime_epoch ||
                source_frame_cursor_.function_generation != actual.source.function_generation ||
                source_frame_cursor_.binding.get() != source.binding.get() || source_frame_cursor_.binding.owner_before(source.binding) ||
                source.binding.owner_before(source_frame_cursor_.binding)))
            { return control_error::invalid_state; }
            if(!command.source_frame_explicit && !saved) { return control_error::none; }
            auto const ordinal{command.source_frame_explicit ? command.source_frame_ordinal : source_frame_cursor_.ordinal};
            ::std::vector<source_frames::frame> frames{};
            if(source_frames::current_frames(source.dwarf->scopes(), actual.source.code_offset, frames) != source_frames::error::none)
            { return control_error::source_location_unmapped; }
            if(ordinal >= frames.size())
            {
                // Runtime caller identities contain no caller PC/captured locals.
                // Reject without reading any carrier or calculating a guest address.
                // Both extents are independently <=256/64, so this scalar sum
                // cannot overflow even on a 32-bit host; no pointer is formed.
                if(ordinal < frames.size() + actual.activation.frames.size() - 1u) { result.source_frame_caller_unavailable = true; }
                else { result.source_frame_out_of_range = true; }
                return control_error::source_location_unmapped;
            }
            // [owned concrete metadata frames ... ordinal ... end]
            // [safe                                             ] ordinal < size BEFORE narrowing/subscript.
            auto const index{static_cast<::std::size_t>(ordinal)};
            if(saved && !command.source_frame_explicit && frames[index].identity != source_frame_cursor_.scope)
            { return control_error::invalid_state; }
            anchor = index; return control_error::none;
#else
            return control_error::source_debug_info_unavailable;
#endif
        }
        void source_frames_locked(trace_record const& record, console_command const& command, controller_reply& result)
        {
            result.source_stop_identifier = pause_ ? stop_identifier_ : 0u;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
            try
            {
#endif
#if defined(UWVM_USE_LLVM_JIT) || defined(UWVM_USE_DEFAULT_JIT)
            if(native_owned_ || !pause_ || stop_identifier_ == 0u || command.disassembly_stop_identifier != stop_identifier_ ||
               !record.activation_capture || !record.capture_ticket || record.function_generation == 0u)
            { result.status = control_error::invalid_state; return; }
            if(command.wasm_frames)
            {
                ::uwvm2::runtime::lib::llvm_jit_debug_activation_snapshot actual{};
                if(!::uwvm2::runtime::lib::llvm_jit_debug_query_activation_host_api(record.activation_capture, actual) ||
                   actual.participant != record.participant || actual.frames.empty() ||
                   (actual.location.code_unit != record.native_site_location.code_unit || actual.location.function != record.native_site_location.function ||
                    actual.location.offset != record.native_site_location.offset || actual.location.code_generation != record.native_site_location.code_generation))
                { result.status = control_error::invalid_state; return; }
                result.source_frame_total = actual.frames.size(); result.source_frame_first = (::std::min)(command.frame_page_first, result.source_frame_total);
                auto const shown{(::std::min)(command.frame_page_count, result.source_frame_total - result.source_frame_first)};
                for(::std::uint64_t n{}; n != shown; ++n)
                {
                    auto const ordinal{result.source_frame_first + n};
                    auto const& frame{actual.frames[actual.frames.size() - 1u - static_cast<::std::size_t>(ordinal)]};
                    source_frame_view view{}; view.kind = ordinal == 0u ? source_frame_kind::physical : source_frame_kind::caller;
                    view.name = ::fast_io::concat_std("Wasm activation"); view.module = frame.module; view.function = frame.function;
                    view.function_generation = frame.function_generation; view.runtime_epoch = frame.runtime_epoch;
                    view.incarnation = frame.incarnation; view.variables_available = ordinal == 0u;
                    result.source_frames.push_back(::std::move(view));
                }
                result.source_frames_available = true; result.source_frame_thread = record.participant; return;
            }
            auto const location{record.native_site_location};
            if(location.code_unit >= source_modules_.size()) { result.status = control_error::source_debug_info_unavailable; return; }
            // [owned source modules ... checked module] end
            // [safe                                   ] module < size BEFORE narrow/index.
            auto const& source{source_modules_[static_cast<::std::size_t>(location.code_unit)]};
            if(source.dwarf_status != source_dwarf::error::none || !source.dwarf || !source.binding)
            { result.status = control_error::source_debug_info_unavailable; return; }
            ::uwvm2::runtime::lib::llvm_jit_debug_source_activation_snapshot actual{};
            if(!::uwvm2::runtime::lib::llvm_jit_debug_query_source_activation_host_api(record.activation_capture, source.binding, actual) ||
               !actual.source_available || actual.activation.participant != record.participant ||
               actual.activation.location.code_unit != location.code_unit || actual.activation.location.function != location.function ||
               actual.activation.location.offset != location.offset || actual.activation.location.code_generation != location.code_generation ||
               actual.source.module != location.code_unit || actual.source.function != location.function ||
               actual.source.runtime_epoch != location.code_generation || actual.source.function_generation != record.function_generation ||
               actual.activation.frames.empty())
            { result.status = control_error::source_location_unmapped; return; }
            ::std::vector<source_frames::frame> current{};
            auto const queried{source_frames::current_frames(source.dwarf->scopes(), actual.source.code_offset, current)};
            if(queried != source_frames::error::none || current.empty())
            { result.status = control_error::source_location_unmapped; return; }
            if(actual.activation.frames.size() - 1u > (::std::numeric_limits<::std::size_t>::max)() - current.size())
            { result.status = control_error::exhausted; return; }
            auto const count{current.size() + actual.activation.frames.size() - 1u};
            // [actual activation frames ... top] end; nonempty was checked above.
            // [safe                                 ] metadata identity borrow, no frame pointers.
            auto const& top{actual.activation.frames.back()};
            if(top.incarnation == 0u || top.module != actual.source.module || top.function != actual.source.function ||
               top.runtime_epoch != actual.source.runtime_epoch || top.function_generation != actual.source.function_generation)
            { result.status = control_error::source_location_unmapped; return; }
            auto const make_view{[&](::std::size_t ordinal)
            {
                source_frame_view view{};
                if(ordinal < current.size())
                {
                    auto const& frame{current[ordinal]};
                    view.scope = frame.identity; view.scope_index = frame.scope_index;
                    view.kind = frame.kind == source_dwarf::scope_kind::inline_subprogram ? source_frame_kind::inline_scope : source_frame_kind::physical;
                    view.name = ::fast_io::concat_std(::std::string_view{frame.name});
                    view.module = top.module; view.function = top.function; view.function_generation = top.function_generation;
                    view.runtime_epoch = top.runtime_epoch; view.incarnation = top.incarnation; view.variables_available = true;
                }
                else
                {
                    auto const& caller{actual.activation.frames[actual.activation.frames.size() - 2u - (ordinal - current.size())]};
                    view.kind = source_frame_kind::caller; view.name = ::fast_io::concat_std("Wasm caller");
                    view.module = caller.module; view.function = caller.function; view.function_generation = caller.function_generation;
                    view.runtime_epoch = caller.runtime_epoch; view.incarnation = caller.incarnation;
                    source_caller_frame_view_locked(record, caller, result, view);
                }
                return view;
            }};
            ::std::uint64_t selected{};
            if(source_frame_cursor_.bound && source_frame_cursor_.participant == record.participant)
            {
                if(source_frame_cursor_.stop != stop_identifier_ || source_frame_cursor_.top_incarnation != top.incarnation ||
                   source_frame_cursor_.module != actual.source.module || source_frame_cursor_.function != actual.source.function ||
                   source_frame_cursor_.runtime_epoch != actual.source.runtime_epoch ||
                   source_frame_cursor_.function_generation != actual.source.function_generation ||
                   source_frame_cursor_.binding.get() != source.binding.get() || source_frame_cursor_.binding.owner_before(source.binding) ||
                   source.binding.owner_before(source_frame_cursor_.binding))
                { result.source_frames.clear(); result.status = control_error::invalid_state; return; }
                selected = source_frame_cursor_.ordinal;
            }
            if(selected >= count) { result.source_frames.clear(); result.status = control_error::invalid_state; return; }
            switch(command.frame_action)
            {
                case source_frame_action::list: case source_frame_action::show: break;
                case source_frame_action::select: selected = command.source_frame_ordinal; break;
                case source_frame_action::up:
                    if(command.source_frame_ordinal > count - 1u - selected) { result.source_frame_out_of_range = true; result.status = control_error::source_location_unmapped; return; }
                    selected += command.source_frame_ordinal; break; // subtraction proof BEFORE addition.
                case source_frame_action::down:
                    if(command.source_frame_ordinal > selected) { result.source_frame_out_of_range = true; result.status = control_error::source_location_unmapped; return; }
                    selected -= command.source_frame_ordinal; break; // checked scalar subtraction.
                default: result.status = control_error::malformed; return;
            }
            if(selected >= count) { result.source_frame_out_of_range = true; result.status = control_error::source_location_unmapped; return; }
            // [owned frames ... selected ... end]
            // [safe                            ] selected < size BEFORE narrowing/subscript.
            auto const chosen{make_view(static_cast<::std::size_t>(selected))};
            result.source_frame_total = count; result.source_frame_physical = current.size() - 1u;
            result.source_frame_first = command.frame_page_explicit ? (::std::min)(command.frame_page_first, result.source_frame_total) : command.frame_action == source_frame_action::list ? 0u : selected / 128u * 128u;
            auto const shown{(::std::min)(command.frame_page_count, result.source_frame_total - result.source_frame_first)};
            for(::std::uint64_t i{}; i != shown; ++i)
            { result.source_frames.push_back(make_view(static_cast<::std::size_t>(result.source_frame_first + i))); }
            source_frame_cursor_ = {}; source_frame_cursor_.bound = true;
            source_frame_cursor_.participant = record.participant; source_frame_cursor_.stop = stop_identifier_;
            source_frame_cursor_.top_incarnation = top.incarnation; source_frame_cursor_.module = actual.source.module;
            source_frame_cursor_.function = actual.source.function; source_frame_cursor_.runtime_epoch = actual.source.runtime_epoch;
            source_frame_cursor_.function_generation = actual.source.function_generation;
            source_frame_cursor_.scope = chosen.scope; source_frame_cursor_.ordinal = selected; source_frame_cursor_.binding = source.binding;
            result.source_frames_available = true; result.selected_source_frame = selected; result.source_frame_thread = record.participant;
#else
            result.status = control_error::source_debug_info_unavailable;
#endif
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
            }
            catch(...)
            { result.source_frames.clear(); result.source_frames_available = false; result.status = control_error::exhausted; }
#endif
        }

        static void source_numeric_identity(::std::span<source_dwarf::type_record const> types, ::std::size_t index,
            source_scalar_expression::integer& value, unsigned guest_bits) noexcept
        {
            if(index >= types.size()) { return; }
            (void)source_scalar_expression::attach_standard_integer_type(types[index],value,guest_bits);
        }
        [[nodiscard]] static source_scalar_expression::language_semantics source_numeric_language(source_module const& source,
            ::std::uint64_t actual_pc, ::std::optional<::std::size_t> frame_anchor) noexcept
        {
#if defined(UWVM_USE_LLVM_JIT) || defined(UWVM_USE_DEFAULT_JIT)
            if(!source.dwarf) { return source_scalar_expression::language_semantics::shared_numeric; }
            source_frames::language_context language{};
            if(source_frames::current_language(source.dwarf->scopes(),actual_pc,frame_anchor.value_or(0u),language) != source_frames::error::none)
            { return source_scalar_expression::language_semantics::shared_numeric; }
            return source_scalar_expression::language_from_dwarf(language.language,language.tinygo_producer,language.zig_producer);
#else
            (void)source; (void)actual_pc; (void)frame_anchor;
            return source_scalar_expression::language_semantics::shared_numeric;
#endif
        }
        // Cold expression path: no ordinary guest/JIT load/store checks change.
        // Reader authentication and every dereference share ONE actual memory
        // transaction, rather than stitching multiple independent snapshots.
        void source_expression_locked(trace_record const& record, console_command const& command,
            source_dwarf::source_expression const& expression, controller_reply& result,
            source_caller_selection const* caller = nullptr, source_scalar_expression::program const* scalar = nullptr)
        {
#if defined(UWVM_USE_LLVM_JIT) || defined(UWVM_USE_DEFAULT_JIT)
            auto const location{record.native_site_location};
            if(caller != nullptr && (caller->source == nullptr || caller->identity.incarnation == 0u))
            { result.status = control_error::source_location_unmapped; return; }
            if(location.code_unit >= source_modules_.size()) { result.status = control_error::source_debug_info_unavailable; return; }
            // [controller-owned source modules ... checked code_unit] end
            // [safe                                                   ] checked BEFORE narrowing/subscript.
            auto const& source{caller == nullptr ? source_modules_[static_cast<::std::size_t>(location.code_unit)] : *caller->source};
            if(source.dwarf_status != source_dwarf::error::none || !source.dwarf || !source.binding)
            { result.status = control_error::source_debug_info_unavailable; return; }
            ::fast_io::array<::uwvm2::runtime::lib::llvm_jit_checkpoint_thread_capture_owner, 256u> captures{};
            ::std::size_t capture_count{}; bool selected_present{};
            if(result.threads.empty() || result.threads.size() > captures.size())
            { result.status = control_error::source_location_unmapped; return; }
            // Match the current actual domain inspection, rather than every old
            // trace slot. Never drop a live participant or submit a retired one.
            for(auto const& participant : result.threads)
            {
                auto const found{::std::find_if(traces_.begin(), traces_.end(),
                    [&](auto const& trace) noexcept { return trace.participant == participant.identifier; })};
                if(found == traces_.end() || !found->typed_capture || capture_count == captures.size())
                { result.status = control_error::source_location_unmapped; return; }
                selected_present |= found->participant == record.participant;
                captures[capture_count++] = found->typed_capture; // [safe] fixed extent proved BEFORE indexed write/advance.
            }
            if(!selected_present) { result.status = control_error::source_location_unmapped; return; }
            struct context
            {
                controller* self; trace_record const* record; console_command const* command;
                source_dwarf::source_expression const* expression; source_module const* source;
                controller_reply* result; source_caller_selection const* caller; source_scalar_expression::program const* scalar;
            } state{this, ::std::addressof(record), ::std::addressof(command), ::std::addressof(expression),
                    ::std::addressof(source), ::std::addressof(result), caller, scalar};
            auto const callback{+[](void* opaque, ::uwvm2::runtime::lib::llvm_jit_debug_source_memory_view& reader) noexcept
            {
                if(opaque == nullptr) { return false; }
                // [actual synchronous caller-owned context] lifetime_end
                // [safe                                  ] API does not publish/store opaque;
                //  ^^ adopts only THIS method's live stack context, never guest/UI memory.
                auto const& state{*static_cast<context const*>(opaque)};
                auto& result{*state.result};
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
                try
                {
#endif
                    auto const& record{*state.record}; auto const& command{*state.command}; auto const& source{*state.source};
                    auto const location{record.native_site_location};
                    ::uwvm2::runtime::lib::llvm_jit_debug_source_activation_snapshot actual{};
                    if(!reader.copy_position(actual) || !actual.source_available || actual.activation.participant != record.participant)
                    { result.status = control_error::source_location_unmapped; return false; }
                    ::std::optional<::std::size_t> frame_anchor{};
                    if(state.caller == nullptr)
                    {
                        if(actual.activation.location.code_unit != location.code_unit || actual.activation.location.function != location.function ||
                           actual.activation.location.offset != location.offset || actual.activation.location.code_generation != location.code_generation ||
                           actual.source.module != location.code_unit || actual.source.function != location.function ||
                           actual.source.runtime_epoch != location.code_generation || actual.source.function_generation != record.function_generation)
                        { result.status = control_error::source_location_unmapped; return false; }
                        auto const frame_status{state.self->source_frame_anchor_locked(record, command, actual, source, frame_anchor, result)};
                        if(frame_status != control_error::none) { result.status = frame_status; return false; }
                    }
                    else
                    {
                        auto const& identity{state.caller->identity};
                        if(actual.activation.frames.empty() || actual.activation.frames.back().incarnation != identity.incarnation ||
                           actual.activation.location.code_unit != identity.module || actual.activation.location.function != identity.function ||
                           actual.activation.location.code_generation != identity.runtime_epoch || actual.source.module != identity.module ||
                           actual.source.function != identity.function || actual.source.runtime_epoch != identity.runtime_epoch ||
                           actual.source.function_generation != identity.function_generation)
                        { result.status = control_error::source_location_unmapped; return false; }
                        // The true caller API supplied its sealed callsite PC and
                        // saved native locals. A leaf PC/ordinal cannot replace it.
                        ::std::vector<source_frames::frame> frames{};
                        if(source_frames::current_frames(source.dwarf->scopes(), actual.source.code_offset, frames) != source_frames::error::none || frames.empty())
                        { result.status = control_error::source_location_unmapped; return false; }
                        frame_anchor = frames.size() - 1u; // This saved physical caller's concrete scope.
                    }
                    auto const types{source.dwarf->types()}; auto const scopes{source.dwarf->scopes()};

                    ::uwvm2::runtime::lib::llvm_jit_debug_source_frame_locals saved_locals{};
                    if(actual.activation.frames.empty() || !reader.copy_frame_locals(saved_locals) ||
                       saved_locals.participant != record.participant || saved_locals.incarnation != actual.activation.frames.back().incarnation ||
                       saved_locals.values.size() > 256u || saved_locals.operands.size() > 256u || saved_locals.globals.size() > 256u ||
                       saved_locals.values.size() > saved_locals.total_count || saved_locals.operands.size() > saved_locals.operand_count ||
                       saved_locals.globals.size() > saved_locals.global_count)
                    { result.status = control_error::source_location_unmapped; return false; }
                    ::std::array<source_dwarf::copied_numeric_local,768u> copied{};
                    auto const captured_count{saved_locals.values.size()+saved_locals.operands.size()+saved_locals.globals.size()};
                    auto const total_count{captured_count}; bool const locals_available{true};
                    ::std::size_t copied_index{};
                    for(auto const* group : {::std::addressof(saved_locals.values),::std::addressof(saved_locals.operands),::std::addressof(saved_locals.globals)})
                    {
                        for(auto const& slot : *group)
                        {
                            auto& target{copied[copied_index++]}; target.wasm_type=slot.wasm_type; target.available=slot.available;
                            if(slot.available) { ::fast_io::freestanding::my_memcpy(target.bytes.data(),slot.native_bytes.data(),target.bytes.size()); }
                        }
                    }
                    // Remap each space only after its own captured-prefix bound
                    // check. An uncaptured local can never alias an appended
                    // operand/global slot. All carriers belong to THIS stop.
                    auto const remap{[&](source_dwarf::location_plan& plan)
                    {
                        auto const atom{[&](source_dwarf::location_plan& item)
                        {
                            if(item.kind != source_dwarf::plan_kind::wasm_local_value && item.kind != source_dwarf::plan_kind::wasm_local_frame_base) { return; }
                            ::std::size_t base{}, count{saved_locals.values.size()};
                            if(item.storage == source_dwarf::wasm_location_space::operand) { base=count;count=saved_locals.operands.size(); }
                            else if(item.storage == source_dwarf::wasm_location_space::global) { base=count+saved_locals.operands.size();count=saved_locals.globals.size(); }
                            else if(item.storage != source_dwarf::wasm_location_space::local) { count=0u; }
                            if(item.local_index >= count) { item.kind=source_dwarf::plan_kind::unavailable;item.reason=source_dwarf::unavailable_reason::no_location;return; }
                            item.local_index += base; item.storage=source_dwarf::wasm_location_space::local;
                        }};
                        atom(plan); for(auto& piece : plan.pieces) { atom(piece.atom); }
                    }};
                    ::std::size_t issued_copies{};
                    // The evaluator receives the ABI before resolving operands.
                    // Reader width (or authenticated DWARF for copied values)
                    // is interpretation data; it grants no memory read authority.
                    auto const scalar_address_bytes{reader.address_bytes() == 4u || reader.address_bytes() == 8u ?
                        reader.address_bytes() : source.dwarf->address_bytes()};
                    unsigned selected_address_bits{static_cast<unsigned>(scalar_address_bytes)*8u};
                    auto const leaf{[&](source_dwarf::source_expression const& expression, console_command_kind kind, controller_reply& result) -> bool
                    {
                    source_dwarf::variable_selection selected{};
                    auto status{source_dwarf::query_named_variable(scopes, types, source.dwarf->variables(), actual.source.code_offset,
                        expression.root_name, selected, {}, frame_anchor)};
                    if(status != source_dwarf::inline_query_error::none || selected.type >= types.size())
                    { result.status = control_error::source_location_unmapped; return false; }
                    auto width{reader.address_bytes()};
                    if(width != 4u && width != 8u)
                    {
                        if(selected.location.address_bytes != 4u && selected.location.address_bytes != 8u)
                        { result.status = control_error::source_location_unmapped; return false; }
                        // This actual DWARF unit width serves ONLY type/copied
                        // numeric interpretation when no unique memory exists.
                        // It grants NO bytes: the runtime's copy_guest gate still
                        // rejects every ambiguous/imported/shared backend access.
                        width = selected.location.address_bytes;
                    }
                    selected_address_bits = static_cast<unsigned>(width)*8u;
                    if(kind == console_command_kind::source_type)
                    {
                        status = source_language_expression::type(types, selected.type, expression.steps, width, result.source_object_type);
                        if(status == source_dwarf::inline_query_error::none)
                        { result.source_object_type_available = true; return true; }
                    }
                    else
                    {
                        if(!selected.location_available || selected.location.address_bytes != width)
                        { result.status = control_error::source_location_unmapped; return false; }
                        if(!source_static_local_intact(source,actual.source,selected))
                        { result.status = control_error::source_location_unmapped; return false; }
                        remap(selected.location);
                        if(expression.steps.empty() &&
                           (selected.location.kind == source_dwarf::plan_kind::wasm_local_value ||
                            selected.location.kind == source_dwarf::plan_kind::constant_value ||
                            selected.location.kind == source_dwarf::plan_kind::composite_value))
                        {
                            source_dwarf::numeric_variable value{}; value.identity = selected.identity;
                            value.name = ::fast_io::concat_std(::std::string_view{expression.root_name});
                            auto const& type{types[selected.type]}; value.type_name = ::fast_io::concat_std(::std::string_view{type.name});
                            source_dwarf::value_details::copy_value(selected.location, type, {copied.data(), captured_count}, total_count, value);
                            if(value.kind != source_dwarf::numeric_kind::unavailable)
                            {
                                if(selected.location.direct_constant_attribute)
                                {
                                    if(selected.location.kind != source_dwarf::plan_kind::constant_value || selected.scope >= scopes.size())
                                    { result.status = control_error::source_debug_info_invalid; return false; }
                                    // [owned scoped source metadata ... selected.scope<size] end
                                    // [safe] actual callback's Code PC and bounded DIE IDs
                                    // only; this origin DATA creates no memory/read grant.
                                    result.source_constant = {selected.identity, scopes[selected.scope].identity,
                                        type.identity, record.participant, actual.source.code_offset, true};
                                }
                                result.source_locals.push_back(::std::move(value)); result.source_locals_available = true; return true;
                            }
                        }
                        ::std::vector<source_dwarf::object_node> layout{};
                        status = source_dwarf::query_type_layout(types, selected.type, layout);
                        if(status != source_dwarf::inline_query_error::none || layout.empty() ||
                           layout[0u].reason != source_dwarf::object_unavailable_reason::none ||
                           layout[0u].byte_size == 0u || layout[0u].byte_size > 65536u)
                        { result.status = control_error::source_location_unmapped; return false; }
                        auto const extent{static_cast<::std::size_t>(layout[0u].byte_size)};
                        ::std::vector<::std::byte> bytes{}, known{}, pointer_known{};
                        auto const same_position{[&](::uwvm2::runtime::lib::llvm_jit_debug_source_object_copy const& object) noexcept
                        {
                            return object.address_bytes == width && object.position.source_available &&
                                object.position.activation.participant == actual.activation.participant &&
                                object.position.activation.location.code_unit == actual.activation.location.code_unit &&
                                object.position.activation.location.function == actual.activation.location.function &&
                                object.position.activation.location.offset == actual.activation.location.offset &&
                                object.position.activation.location.code_generation == actual.activation.location.code_generation &&
                                object.position.source.module == actual.source.module && object.position.source.function == actual.source.function &&
                                object.position.source.code_offset == actual.source.code_offset && object.position.source.runtime_epoch == actual.source.runtime_epoch &&
                                object.position.source.function_generation == actual.source.function_generation;
                        }};
                        auto const read{[&](::std::uint64_t offset, ::std::size_t size, ::std::vector<::std::byte>& target) noexcept
                        {
                            target.clear();
                            if(issued_copies >= 32u) { return false; }
                            ++issued_copies; // [safe] actual cold copy allowance charged before runtime read.
                            ::uwvm2::runtime::lib::llvm_jit_debug_source_object_copy object{};
                            if(!reader.copy_guest(offset, size, object) || !same_position(object) || object.guest_offset != offset || object.bytes.size() != size)
                            { return false; }
                            target = ::std::move(object.bytes); return true; // complete owned bytes only.
                        }};
                        if(selected.location.kind == source_dwarf::plan_kind::absolute_guest_offset ||
                           selected.location.kind == source_dwarf::plan_kind::frame_relative_offset)
                        {
                            ::std::uint64_t offset{};
                            if(selected.location.kind == source_dwarf::plan_kind::absolute_guest_offset)
                            {
                                if(source_dwarf::resolve_absolute_guest_offset(selected.location, offset) != source_dwarf::object_location_error::none)
                                { result.status = control_error::source_location_unmapped; return false; }
                            }
                            else
                            {
                                if(!locals_available || selected.physical_scope >= scopes.size())
                                { result.status = control_error::source_location_unmapped; return false; }
                                // [owned scopes ... physical_scope<size] end
                                // [safe                              ] checked BEFORE metadata borrow;
                                //  ^^ existing role-aware frame-base plan consumes copied carriers only.
                                auto frame_base{scopes[selected.physical_scope].frame_base};
                                for(auto& entry : frame_base) { remap(entry.plan); }
                                if(source_dwarf::resolve_frame_relative_offset(selected.location, frame_base,
                                    actual.source.code_offset, {copied.data(), captured_count}, total_count, offset) !=
                                    source_dwarf::object_location_error::none)
                                { result.status = control_error::source_location_unmapped; return false; }
                            }
                            if(!read(offset, extent, bytes)) { result.status = control_error::source_location_unmapped; return false; }
                        }
                        else
                        {
                            if(!locals_available && selected.location.kind != source_dwarf::plan_kind::constant_value)
                            { result.status = control_error::source_location_unmapped; return false; }
                            auto plan{selected.location};
                            if(plan.kind != source_dwarf::plan_kind::composite_value)
                            {
                                if(plan.kind != source_dwarf::plan_kind::wasm_local_value && plan.kind != source_dwarf::plan_kind::constant_value)
                                { result.status = control_error::source_location_unmapped; return false; }
                                if(plan.kind == source_dwarf::plan_kind::wasm_local_value)
                                {
                                    if(plan.local_index >= captured_count || plan.local_index >= total_count)
                                    { result.status = control_error::source_location_unmapped; return false; }
                                    auto const& slot{copied[static_cast<::std::size_t>(plan.local_index)]};
                                    // Pointer values require an actual integer carrier of the
                                    // declared memory width. Numeric/REF tags cannot be forged.
                                    if(!slot.available || (width == 4u ? slot.wasm_type != 0x7fu : slot.wasm_type != 0x7eu) || extent != width)
                                    { result.status = control_error::source_location_unmapped; return false; }
                                }
                                source_dwarf::location_piece piece{}; piece.atom = plan; piece.bit_size = extent * 8u;
                                plan = {}; plan.kind = source_dwarf::plan_kind::composite_value; plan.reason = source_dwarf::unavailable_reason::none;
                                plan.address_bytes = width; plan.composite_bits = piece.bit_size; plan.pieces.push_back(::std::move(piece));
                            }
                            source_dwarf::composite_value copied_object{};
                            if(source_dwarf::materialize_location_pieces(plan, {copied.data(), captured_count}, total_count,
                                {}, extent, copied_object, {64u,65536u,768u,65536u}) != source_dwarf::piece_query_error::none || copied_object.bytes.size() != extent ||
                                copied_object.known_bits.size() != extent)
                            { result.status = control_error::source_location_unmapped; return false; }
                            // Keep actual aggregate float values displayable,
                            // but mark only genuine integer/constant carrier bits
                            // eligible for a conventional guest-pointer plan.
                            auto integer_locals{copied};
                            for(::std::size_t i{}; i != captured_count; ++i)
                            {
                                if(integer_locals[i].wasm_type != 0x7fu && integer_locals[i].wasm_type != 0x7eu)
                                { integer_locals[i].available = false; }
                            }
                            source_dwarf::composite_value pointer_object{};
                            if(source_dwarf::materialize_location_pieces(plan, {integer_locals.data(), captured_count}, total_count,
                                {}, extent, pointer_object, {64u,65536u,768u,65536u}) != source_dwarf::piece_query_error::none || pointer_object.known_bits.size() != extent)
                            { result.status = control_error::source_location_unmapped; return false; }
                            bytes = ::std::move(copied_object.bytes); known = ::std::move(copied_object.known_bits);
                            pointer_known = ::std::move(pointer_object.known_bits);
                        }
                        // Exact scoped root was selected INSIDE the same runtime
                        // transaction from the actual returned Code PC. Guest
                        // locals/type/member text do not create a read token.
                        status = source_language_expression::value(types, selected.type, expression.steps,
                            ::std::move(bytes), ::std::move(known), width, read, result.source_object_type, {32u - issued_copies}, pointer_known);
                        if(status == source_dwarf::inline_query_error::none)
                        { result.source_object_value_available = true; return true; }
                    }
                    result.source_object_type.clear();
                    result.status = status == source_dwarf::inline_query_error::allocation_failure || status == source_dwarf::inline_query_error::limit_exceeded ?
                        control_error::exhausted : status == source_dwarf::inline_query_error::malformed ? control_error::source_debug_info_invalid :
                        control_error::source_location_unmapped;
                    return false;
                    }};
                    if(state.scalar == nullptr) { return leaf(*state.expression, command.kind, result); }
                    source_scalar_expression::integer value{};
                    auto const resolver{[&](source_dwarf::source_expression const& expression, bool size, source_scalar_expression::integer& out) -> bool
                    {
                        controller_reply resolved{};
                        if(!leaf(expression, size ? console_command_kind::source_type : console_command_kind::source_value, resolved)) { return false; }
                        if(size)
                        {
                            if(resolved.source_object_type.empty() || resolved.source_object_type[0u].reason != source_dwarf::object_unavailable_reason::none || resolved.source_object_type[0u].bit_field)
                            { return false; }
                            out = {resolved.source_object_type[0u].byte_size, selected_address_bits, true}; return true;
                        }
                        if(resolved.source_object_type.size() == 1u)
                        {
                            auto const& n{resolved.source_object_type[0u]};
                            if(!n.value_available || n.kind == source_dwarf::type_kind::pointer || n.scalar_bytes == 0u || n.scalar_bytes > 8u ||
                               (n.scalar_kind != source_dwarf::numeric_kind::boolean && n.scalar_kind != source_dwarf::numeric_kind::signed_integer &&
                                n.scalar_kind != source_dwarf::numeric_kind::unsigned_integer && n.scalar_kind != source_dwarf::numeric_kind::f32_bits && n.scalar_kind != source_dwarf::numeric_kind::f64_bits)) { return false; }
                            out = source_scalar_expression::from_dwarf_numeric(n.scalar_kind,n.bits,static_cast<unsigned>(n.scalar_bytes)*8u); source_numeric_identity(types,n.type,out,selected_address_bits); return true;
                        }
                        if(resolved.source_locals.size() == 1u)
                        {
                            auto const& n{resolved.source_locals[0u]};
                            if(n.kind != source_dwarf::numeric_kind::boolean && n.kind != source_dwarf::numeric_kind::signed_integer &&
                               n.kind != source_dwarf::numeric_kind::unsigned_integer && n.kind != source_dwarf::numeric_kind::f32_bits && n.kind != source_dwarf::numeric_kind::f64_bits) { return false; }
                            if(n.byte_count == 0u || n.byte_count > 8u) { return false; }
                            out = source_scalar_expression::from_dwarf_numeric(n.kind,n.bits,static_cast<unsigned>(n.byte_count)*8u);
                            if(out.category == source_scalar_expression::value_category::integer)
                            {
                                controller_reply layout{};
                                if(leaf(expression,console_command_kind::source_type,layout) && layout.source_object_type.size() == 1u)
                                { source_numeric_identity(types,layout.source_object_type[0u].type,out,selected_address_bits); }
                            }
                            return true;
                        }
                        return false;
                    }};
                    auto const type_resolver{[&](source_dwarf::source_expression const& expression, bool size, source_scalar_expression::integer& out) -> bool
                    {
                        // Type metadata from this actual selected frame only.
                        // source_type takes no location/carrier/guest read path,
                        // so an optimized-away or null dead-arm value is unused.
                        controller_reply resolved{};
                        if(!leaf(expression,console_command_kind::source_type,resolved) || resolved.source_object_type.empty()) { return false; }
                        auto const& n{resolved.source_object_type[0u]};
                        if(n.reason != source_dwarf::object_unavailable_reason::none || n.bit_field) { return false; }
                        if(size) { out = {n.byte_size,selected_address_bits,true}; return true; }
                        if(resolved.source_object_type.size() != 1u || n.kind != source_dwarf::type_kind::scalar ||
                           n.scalar_bytes == 0u || n.scalar_bytes > 8u ||
                           (n.scalar_kind != source_dwarf::numeric_kind::boolean && n.scalar_kind != source_dwarf::numeric_kind::signed_integer &&
                            n.scalar_kind != source_dwarf::numeric_kind::unsigned_integer && n.scalar_kind != source_dwarf::numeric_kind::f32_bits &&
                            n.scalar_kind != source_dwarf::numeric_kind::f64_bits)) { return false; }
                        out = source_scalar_expression::from_dwarf_numeric(n.scalar_kind,0u,static_cast<unsigned>(n.scalar_bytes)*8u);
                        source_numeric_identity(types,n.type,out,selected_address_bits); return true;
                    }};
                    auto const evaluated{source_scalar_expression::evaluate(*state.scalar, resolver, value, selected_address_bits,type_resolver,source_numeric_language(source,actual.source.code_offset,frame_anchor))};
                    if(evaluated != source_scalar_expression::error::none)
                    { result.status = evaluated == source_scalar_expression::error::limit_exceeded || evaluated == source_scalar_expression::error::allocation_failure ?
                          control_error::exhausted : control_error::source_location_unmapped; return false; }
                    source_dwarf::object_node computed{}; computed.name = ::fast_io::concat_std("$expression");
                    computed.type_name = ::fast_io::concat_std(source_scalar_expression::copied_type_name(value));
                    computed.kind = source_dwarf::type_kind::scalar; computed.scalar_kind = value.category == source_scalar_expression::value_category::boolean ? source_dwarf::numeric_kind::boolean : value.floating ?
                        (value.width == 32u ? source_dwarf::numeric_kind::f32_bits : source_dwarf::numeric_kind::f64_bits) : value.unsigned_value ?
                        source_dwarf::numeric_kind::unsigned_integer : source_dwarf::numeric_kind::signed_integer;
                    computed.scalar_bytes = static_cast<::std::uint8_t>(value.width/8u); computed.byte_size = computed.scalar_bytes;
                    computed.bits = value.bits; computed.value_available = true;
                    result.source_object_type.push_back(::std::move(computed)); result.source_object_value_available = true; return true;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
                }
                catch(...) { result.source_object_type.clear(); result.status = control_error::exhausted; return false; }
#endif
            }};
            ::uwvm2::runtime::lib::llvm_jit_debug_activation_snapshot selected_activation{};
            if(!::uwvm2::runtime::lib::llvm_jit_debug_query_activation_host_api(record.activation_capture,selected_activation) ||
               selected_activation.participant != record.participant || selected_activation.frames.empty())
            { result.status = control_error::source_location_unmapped; return; }
            auto const incarnation{caller == nullptr ? selected_activation.frames.back().incarnation : caller->identity.incarnation};
            auto const accepted{::uwvm2::runtime::lib::llvm_jit_debug_with_source_frame_memory_host_api(pause_, {captures.data(), capture_count},
                record.activation_capture, source.binding, incarnation, ::std::addressof(state), callback)};
            if(!accepted)
            {
                result.source_object_type.clear(); result.source_object_value_available = result.source_object_type_available = false;
                if(caller != nullptr) { result.source_locals.clear(); result.source_locals_available = false; result.source_frame_caller_unavailable = true; }
                if(result.status == control_error::none) { result.status = control_error::source_location_unmapped; }
            }
#else
            (void)record; (void)command; (void)expression; result.status = control_error::source_debug_info_unavailable;
#endif
        }

        // Pure scalar/type operands consume ONE authenticated stopped position
        // and immutable controller-owned copies. A failed checkpoint observation
        // or absent linear memory cannot invalidate an already copied scalar.
        // No guest read, native pointer, saved PC or independent per-operand
        // runtime transaction is used here. A reachable memory operand restarts
        // the entire expression through the existing coherent byte-reader path.
        bool source_copied_scalar_locked(trace_record const& record, console_command const& command,
            source_scalar_expression::program const& scalar, controller_reply& result)
        {
#if defined(UWVM_USE_LLVM_JIT) || defined(UWVM_USE_DEFAULT_JIT)
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
            try {
#endif
                auto const location{record.native_site_location};
                if(location.code_unit >= source_modules_.size()) { result.status = control_error::source_debug_info_unavailable; return true; }
                auto const& source{source_modules_[static_cast<::std::size_t>(location.code_unit)]};
                if(source.dwarf_status != source_dwarf::error::none || !source.dwarf || !source.binding)
                { result.status = source.dwarf_status == source_dwarf::error::missing_sections ?
                      control_error::source_debug_info_unavailable : control_error::source_debug_info_invalid; return true; }
                ::uwvm2::runtime::lib::llvm_jit_debug_source_activation_snapshot actual{};
                if(!::uwvm2::runtime::lib::llvm_jit_debug_query_source_activation_host_api(record.activation_capture, source.binding, actual) ||
                   !actual.source_available || actual.activation.participant != record.participant ||
                   actual.activation.location.code_unit != location.code_unit || actual.activation.location.function != location.function ||
                   actual.activation.location.offset != location.offset || actual.activation.location.code_generation != location.code_generation ||
                   actual.source.module != location.code_unit || actual.source.function != location.function ||
                   actual.source.runtime_epoch != location.code_generation || actual.source.function_generation != record.function_generation)
                { result.status = control_error::source_location_unmapped; return true; }
                ::std::optional<::std::size_t> frame_anchor{};
                auto const frame_status{source_frame_anchor_locked(record,command,actual,source,frame_anchor,result)};
                if(frame_status != control_error::none) { result.status = frame_status; return true; }
                auto const width{source.dwarf->address_bytes()};
                ::std::array<source_dwarf::copied_numeric_local, ::uwvm2::runtime::lib::llvm_jit_debug_max_captured_locals> copied{};
                if((width != 4u && width != 8u) || record.captured_count > copied.size() || record.captured_count > record.total_count)
                { result.status = control_error::source_location_unmapped; return true; }
                for(::std::size_t i{}; i != record.captured_count; ++i)
                {
                    // [owned fixed capture slots ... i<checked captured_count] end
                    // [safe] BOTH equal fixed extents checked before these borrows.
                    copied[i].wasm_type = record.locals[i].type;
                    copied[i].available = record.locals_available && record.locals[i].available;
                    if(copied[i].available)
                    { ::fast_io::freestanding::my_memcpy(copied[i].bytes.data(),record.locals[i].bytes.data(),copied[i].bytes.size()); }
                }
                bool needs_memory{};
                auto const resolve{[&](source_dwarf::source_expression const& expression, bool size, source_scalar_expression::integer& out) -> bool
                {
                    source_dwarf::variable_selection selected{};
                    auto const selected_status{source_dwarf::query_named_variable(source.dwarf->scopes(),source.dwarf->types(),source.dwarf->variables(),
                        actual.source.code_offset,expression.root_name,selected,{},frame_anchor)};
                    if(selected_status != source_dwarf::inline_query_error::none || selected.type >= source.dwarf->types().size()) { return false; }
                    if(size)
                    {
                        ::std::vector<source_dwarf::object_node> layout{};
                        if(source_language_expression::type(source.dwarf->types(),selected.type,expression.steps,width,layout) !=
                           source_dwarf::inline_query_error::none || layout.empty() || layout[0u].reason != source_dwarf::object_unavailable_reason::none || layout[0u].bit_field)
                        { return false; }
                        out = {layout[0u].byte_size,static_cast<unsigned>(width)*8u,true}; return true;
                    }
                    if(!expression.steps.empty()) { needs_memory = true; return false; }
                    if(!selected.location_available) { return false; }
                    if(selected.location.storage != source_dwarf::wasm_location_space::local ||
                       ::std::any_of(selected.location.pieces.begin(),selected.location.pieces.end(),[](auto const& piece)
                           { return piece.atom.storage != source_dwarf::wasm_location_space::local; }))
                    { needs_memory = true; return false; }
                    if(selected.location.kind == source_dwarf::plan_kind::absolute_guest_offset ||
                       selected.location.kind == source_dwarf::plan_kind::frame_relative_offset)
                    { needs_memory = true; return false; }
                    source_dwarf::numeric_variable value{};
                    source_dwarf::value_details::copy_value(selected.location,source.dwarf->types()[selected.type],
                        {copied.data(),record.captured_count},record.total_count,value);
                    if(value.kind != source_dwarf::numeric_kind::boolean && value.kind != source_dwarf::numeric_kind::signed_integer &&
                       value.kind != source_dwarf::numeric_kind::unsigned_integer && value.kind != source_dwarf::numeric_kind::f32_bits && value.kind != source_dwarf::numeric_kind::f64_bits)
                    { needs_memory = value.reason == source_dwarf::numeric_unavailable_reason::unsupported_plan; return false; }
                    if(value.byte_count == 0u || value.byte_count > 8u) { return false; }
                    out = source_scalar_expression::from_dwarf_numeric(value.kind,value.bits,static_cast<unsigned>(value.byte_count)*8u); source_numeric_identity(source.dwarf->types(),selected.type,out,static_cast<unsigned>(width)*8u); return true;
                }};
                auto const resolve_type{[&](source_dwarf::source_expression const& expression, bool size, source_scalar_expression::integer& out) -> bool
                {
                    // This copied-local path has no memory reader. Metadata
                    // queries must still use the same actual selected frame.
                    source_dwarf::variable_selection selected{};
                    if(source_dwarf::query_named_variable(source.dwarf->scopes(),source.dwarf->types(),source.dwarf->variables(),
                        actual.source.code_offset,expression.root_name,selected,{},frame_anchor) != source_dwarf::inline_query_error::none ||
                       selected.type >= source.dwarf->types().size()) { return false; }
                    ::std::vector<source_dwarf::object_node> layout{};
                    if(source_language_expression::type(source.dwarf->types(),selected.type,expression.steps,width,layout) !=
                       source_dwarf::inline_query_error::none || layout.empty()) { return false; }
                    auto const& n{layout[0u]};
                    if(n.reason != source_dwarf::object_unavailable_reason::none || n.bit_field) { return false; }
                    if(size) { out = {n.byte_size,static_cast<unsigned>(width)*8u,true}; return true; }
                    if(layout.size() != 1u || n.kind != source_dwarf::type_kind::scalar ||
                       n.scalar_bytes == 0u || n.scalar_bytes > 8u ||
                       (n.scalar_kind != source_dwarf::numeric_kind::boolean && n.scalar_kind != source_dwarf::numeric_kind::signed_integer &&
                        n.scalar_kind != source_dwarf::numeric_kind::unsigned_integer && n.scalar_kind != source_dwarf::numeric_kind::f32_bits &&
                        n.scalar_kind != source_dwarf::numeric_kind::f64_bits)) { return false; }
                    out = source_scalar_expression::from_dwarf_numeric(n.scalar_kind,0u,static_cast<unsigned>(n.scalar_bytes)*8u);
                    source_numeric_identity(source.dwarf->types(),n.type,out,static_cast<unsigned>(width)*8u); return true;
                }};
                source_scalar_expression::integer value{};
                auto const status{source_scalar_expression::evaluate(scalar,resolve,value,static_cast<unsigned>(width)*8u,resolve_type,source_numeric_language(source,actual.source.code_offset,frame_anchor))};
                if(needs_memory) { return false; } // No output/read occurred; coherent retry owns ALL operands.
                if(status != source_scalar_expression::error::none)
                { result.status = status == source_scalar_expression::error::limit_exceeded || status == source_scalar_expression::error::allocation_failure ?
                      control_error::exhausted : control_error::source_location_unmapped; return true; }
                source_dwarf::object_node computed{}; computed.name = ::fast_io::concat_std("$expression");
                computed.type_name = ::fast_io::concat_std(source_scalar_expression::copied_type_name(value));
                computed.kind = source_dwarf::type_kind::scalar; computed.scalar_kind = value.category == source_scalar_expression::value_category::boolean ? source_dwarf::numeric_kind::boolean : value.floating ?
                    (value.width == 32u ? source_dwarf::numeric_kind::f32_bits : source_dwarf::numeric_kind::f64_bits) : value.unsigned_value ?
                    source_dwarf::numeric_kind::unsigned_integer : source_dwarf::numeric_kind::signed_integer;
                computed.scalar_bytes = static_cast<::std::uint8_t>(value.width/8u); computed.byte_size = computed.scalar_bytes;
                computed.bits = value.bits; computed.value_available = true;
                result.source_object_type.push_back(::std::move(computed)); result.source_object_value_available = true; return true;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
            } catch(...) { result.source_object_type.clear(); result.status = control_error::exhausted; return true; }
#endif
#else
            (void)record; (void)command; (void)scalar; result.status = control_error::source_debug_info_unavailable; return true;
#endif
        }

        void source_type_locked(trace_record const& record, console_command const& command, controller_reply& result)
        {
            result.source_stop_identifier = pause_ ? stop_identifier_ : 0u;
            if(native_owned_ || !pause_ || command.disassembly_stop_identifier != stop_identifier_ ||
               !record.capture_ticket || !record.activation_capture || record.function_generation == 0u ||
               command.source_variable_name_size == 0u || command.source_variable_name_size > command.source_variable_name.size())
            { result.status = control_error::invalid_state; return; }
            ::std::string_view const requested{command.source_variable_name.data(), command.source_variable_name_size};
            source_scalar_expression::program scalar{};
            source_dwarf::source_expression simple{};
            auto const simple_status{source_dwarf::parse_source_expression(requested, simple)};
            bool const scalar_query{command.kind == console_command_kind::source_value &&
                (simple_status != source_dwarf::object_selector_error::none || requested == "true" || requested == "false")};
            if(scalar_query && source_scalar_expression::parse_admitted(requested, scalar) != source_scalar_expression::error::none)
            { result.status = control_error::malformed; return; }
            source_caller_selection caller{};
            auto const caller_route{source_caller_selection_locked(record, command, caller, result)};
            if(caller_route == source_caller_route::rejected) { return; }
            if(scalar_query)
            {
                if(caller_route != source_caller_route::caller && source_copied_scalar_locked(record,command,scalar,result)) { return; }
                source_expression_locked(record, command, simple, result, caller_route == source_caller_route::caller ? ::std::addressof(caller) : nullptr,
                    ::std::addressof(scalar)); return;
            }
            if(caller_route == source_caller_route::caller)
            {
                source_dwarf::source_expression expression{};
                auto const parsed{source_dwarf::parse_source_expression(
                    {command.source_variable_name.data(), command.source_variable_name_size}, expression)};
                if(parsed != source_dwarf::object_selector_error::none)
                { result.status = parsed == source_dwarf::object_selector_error::limit_exceeded ? control_error::exhausted : control_error::malformed; return; }
                source_expression_locked(record, command, expression, result, ::std::addressof(caller)); return;
            }
            // Parse only bounded command-owned DATA. Complex source pointer
            // expressions take the new coherent cold path; ordinary existing
            // root/member/index queries retain their current precise behavior.
            source_dwarf::source_expression parsed_expression{};
            ::std::string_view requested_expression{command.source_variable_name.data(), command.source_variable_name_size};
            if(source_dwarf::parse_source_expression(requested_expression, parsed_expression) == source_dwarf::object_selector_error::none &&
               !parsed_expression.steps.empty())
            { source_expression_locked(record, command, parsed_expression, result); return; }
#if defined(UWVM_USE_LLVM_JIT) || defined(UWVM_USE_DEFAULT_JIT)
            auto const location{record.native_site_location};
            if(location.code_unit >= source_modules_.size()) { result.status = control_error::source_debug_info_unavailable; return; }
            auto const& source{source_modules_[static_cast<::std::size_t>(location.code_unit)]};
            if(source.dwarf_status != source_dwarf::error::none || !source.dwarf || !source.binding)
            { result.status = source.dwarf_status == source_dwarf::error::missing_sections ?
                  control_error::source_debug_info_unavailable : control_error::source_debug_info_invalid; return; }
            ::uwvm2::runtime::lib::llvm_jit_debug_source_activation_snapshot actual{};
            // ONE genuine stopped transaction jointly authenticates the private
            // event capture, source owner, real activation and exact Code PC.
            // User NAME/THREAD/STOP_ID is metadata selection, never read authority.
            if(!::uwvm2::runtime::lib::llvm_jit_debug_query_source_activation_host_api(record.activation_capture, source.binding, actual) ||
               !actual.source_available || actual.activation.participant != record.participant ||
               actual.activation.location.code_unit != location.code_unit || actual.activation.location.function != location.function ||
               actual.activation.location.offset != location.offset || actual.activation.location.code_generation != location.code_generation ||
               actual.source.module != location.code_unit || actual.source.function != location.function ||
               actual.source.runtime_epoch != location.code_generation || actual.source.function_generation != record.function_generation)
            { result.status = control_error::source_location_unmapped; return; }
            ::std::optional<::std::size_t> frame_anchor{};
            auto const frame_status{source_frame_anchor_locked(record, command, actual, source, frame_anchor, result)};
            if(frame_status != control_error::none) { result.status = frame_status; return; }
            // [owned command NAME bytes ... checked size] end
            // [safe                                    ] immutable bounded borrow;
            //  ^^ query returns copied metadata nodes, never a guest/native address.
            ::std::string_view expression{command.source_variable_name.data(), command.source_variable_name_size};
            source_dwarf::object_selector selector{};
            auto const parsed_selector{source_dwarf::parse_source_object_selector(expression, selector)};
            if(parsed_selector != source_dwarf::object_selector_error::none)
            { result.status = parsed_selector == source_dwarf::object_selector_error::limit_exceeded ? control_error::exhausted :
                  control_error::malformed; return; }
            // Select a real scoped ROOT first. Member/index selectors never
            // mint an address, follow a guest pointer or narrow the runtime
            // transaction to an unauthenticated independently computed slice.
            ::std::string_view name{selector.root_name};
            source_dwarf::variable_selection selected{};
            auto status{source_dwarf::query_named_variable(source.dwarf->scopes(), source.dwarf->types(), source.dwarf->variables(),
                actual.source.code_offset, name, selected, {}, frame_anchor)};
            if(status == source_dwarf::inline_query_error::none && command.kind == console_command_kind::source_value && parsed_expression.steps.empty() && selected.type < source.dwarf->types().size())
            {
                auto const& type{source.dwarf->types()[selected.type]};
                if((selected.location.kind == source_dwarf::plan_kind::composite_value && type.kind != source_dwarf::type_kind::scalar) ||
                   type.kind == source_dwarf::type_kind::pointer || type.name == "&str" || type.name == "&mut str" ||
                   (source_language_expression::details::go_language(type) && type.kind == source_dwarf::type_kind::structure &&
                    (type.name == "string" || ::std::string_view{type.name}.starts_with("[]"))))
                { source_expression_locked(record,command,parsed_expression,result); return; }
            }
            if(status == source_dwarf::inline_query_error::none && command.kind == console_command_kind::source_value)
            {
                auto const publish_constant_origin{[&]() noexcept
                {
                    if(!selected.location.direct_constant_attribute) { return true; }
                    auto const scopes{source.dwarf->scopes()}; auto const types{source.dwarf->types()};
                    if(selected.location.kind != source_dwarf::plan_kind::constant_value ||
                       selected.scope >= scopes.size() || selected.type >= types.size()) { return false; }
                    // [owned source scopes/types ... checked indices] end
                    // [safe                                        ] metadata borrow only;
                    //  ^^ bounds precede both identity reads. No address capability.
                    result.source_constant = {selected.identity, scopes[selected.scope].identity,
                        types[selected.type].identity, record.participant, actual.source.code_offset, true};
                    return true;
                }};
                // These fixed slots were copied synchronously from THIS record's
                // real before-park callback. The private capture query above
                // proves its original ticket, actual position and function epoch;
                // controller mutex_ excludes resume/replacement while consuming them.
                ::std::array<source_dwarf::copied_numeric_local, ::uwvm2::runtime::lib::llvm_jit_debug_max_captured_locals> copied{};
                bool const absolute_object{selected.location.kind == source_dwarf::plan_kind::absolute_guest_offset};
                bool const constant_object{selected.location.kind == source_dwarf::plan_kind::constant_value};
                if((!record.locals_available && !absolute_object && !constant_object) ||
                   record.captured_count > copied.size() || record.captured_count > record.total_count || !selected.location_available)
                { result.status = control_error::source_location_unmapped; return; }
                for(::std::size_t i{}; i != record.captured_count; ++i)
                {
                    // [owned fixed captured slots ... captured_count<=256] end
                    // [safe                                              ] equal checked indices;
                    //  ^^ numeric carriers are copied, never exposed as native references.
                    copied[i].wasm_type = record.locals[i].type;
                    copied[i].available = record.locals[i].available;
                    if(!copied[i].available) { continue; }
                    for(::std::size_t byte{}; byte != copied[i].bytes.size(); ++byte)
                    { copied[i].bytes[byte] = record.locals[i].bytes[byte]; }
                }
                if(command.kind == console_command_kind::source_value &&
                   (selected.location.storage != source_dwarf::wasm_location_space::local ||
                    ::std::any_of(selected.location.pieces.begin(),selected.location.pieces.end(),[](auto const& piece)
                        { return piece.atom.storage != source_dwarf::wasm_location_space::local; })))
                { source_expression_locked(record,command,parsed_expression,result); return; }
                if(selected.location.kind != source_dwarf::plan_kind::frame_relative_offset && !absolute_object)
                {
                    // A numeric captured slot is not a materialized aggregate
                    // and gives no pointer-following authority. Bare scalar
                    // names keep the existing precise direct-local query.
                    if(!selector.steps.empty()) { result.status = control_error::source_location_unmapped; return; }
                    if(constant_object && selected.global && selected.type < source.dwarf->types().size())
                    {
                        source_dwarf::numeric_variable value{}; value.identity = selected.identity;
                        value.name = ::fast_io::concat_std(name);
                        // [owned metadata types ... checked selected.type] end
                        // [safe                                         ] type < size BEFORE borrow.
                        auto const& type{source.dwarf->types()[selected.type]}; value.type_name = ::fast_io::concat_std(::std::string_view{type.name});
                        source_dwarf::value_details::copy_value(selected.location, type, {copied.data(), record.captured_count}, record.total_count, value);
                        if(value.kind == source_dwarf::numeric_kind::unavailable) { result.status = control_error::source_location_unmapped; return; }
                        if(!publish_constant_origin()) { result.status = control_error::source_debug_info_invalid; return; }
                        result.source_locals.push_back(::std::move(value)); result.source_locals_available = true; return;
                    }
                    status = source_dwarf::query_numeric_variables(source.dwarf->scopes(), source.dwarf->types(),
                        source.dwarf->variables(), actual.source.code_offset, {copied.data(), record.captured_count},
                        record.total_count, result.source_locals);
                    if(status == source_dwarf::inline_query_error::none)
                    {
                        ::std::erase_if(result.source_locals, [&](auto const& variable) { return variable.identity != selected.identity; });
                        if(result.source_locals.size() == 1u && result.source_locals[0u].kind != source_dwarf::numeric_kind::unavailable)
                        {
                            if(!publish_constant_origin())
                            { result.source_locals.clear(); result.status = control_error::source_debug_info_invalid; return; }
                            result.source_locals_available = true; return;
                        }
                    }
                    result.source_locals.clear(); result.status = control_error::source_location_unmapped; return;
                }
                auto const scopes{source.dwarf->scopes()};
                auto const resolve_offset{[&](source_dwarf::variable_selection const& variable, ::std::uint64_t pc, ::std::uint64_t& offset) noexcept
                {
                    if(variable.location.kind == source_dwarf::plan_kind::absolute_guest_offset)
                    {
                        // DW_OP_addr is only a guest offset. The subsequent
                        // private runtime transaction proves single local
                        // unshared memory and its actual address width, never
                        // resolves a host symbol or casts this number to a pointer.
                        return source_dwarf::resolve_absolute_guest_offset(variable.location, offset) == source_dwarf::object_location_error::none;
                    }
                    if(variable.physical_scope >= scopes.size()) { return false; }
                    // [owned source scopes ... physical_scope<size] end
                    // [safe                                       ] checked metadata borrow only;
                    //  ^^ no generated-frame or runtime address is dereferenced here.
                    return source_dwarf::resolve_frame_relative_offset(variable.location, scopes[variable.physical_scope].frame_base,
                        pc, {copied.data(), record.captured_count}, record.total_count, offset) == source_dwarf::object_location_error::none;
                }};
                ::std::uint64_t guest_offset{};
                if(!resolve_offset(selected, actual.source.code_offset, guest_offset))
                { result.status = control_error::source_location_unmapped; return; }
                status = source_dwarf::query_type_layout(source.dwarf->types(), selected.type, result.source_object_type);
                if(status != source_dwarf::inline_query_error::none || result.source_object_type.empty() ||
                   result.source_object_type[0u].reason != source_dwarf::object_unavailable_reason::none ||
                   result.source_object_type[0u].byte_size > 65536u)
                { result.source_object_type.clear(); result.status = control_error::source_location_unmapped; return; }
                auto const extent{static_cast<::std::size_t>(result.source_object_type[0u].byte_size)};
                ::uwvm2::runtime::lib::llvm_jit_debug_source_object_copy object{};
                // No bare read_memory_ call here. The runtime authenticates both
                // private owners and copies the actual single unshared memory
                // under ONE genuine all-stopped participant transaction.
                if(!::uwvm2::runtime::lib::llvm_jit_debug_copy_source_object_host_api(record.activation_capture, source.binding,
                       guest_offset, extent, selected.location.address_bytes, object) ||
                   !object.position.source_available || object.position.activation.participant != record.participant ||
                   object.position.activation.location.code_unit != location.code_unit ||
                   object.position.activation.location.function != location.function ||
                   object.position.activation.location.offset != location.offset ||
                   object.position.activation.location.code_generation != location.code_generation ||
                   object.position.source.module != actual.source.module || object.position.source.function != actual.source.function ||
                   object.position.source.code_offset != actual.source.code_offset || object.position.source.runtime_epoch != actual.source.runtime_epoch ||
                   object.position.source.function_generation != record.function_generation || object.guest_offset != guest_offset ||
                   object.address_bytes != selected.location.address_bytes || object.bytes.size() != extent)
                { result.source_object_type.clear(); result.status = control_error::source_location_unmapped; return; }
                // Repeat lexical selection at the ACTUAL position returned by
                // the same copy transaction. Metadata cannot swap an object or
                // frame-base expression between address planning and formatting.
                source_dwarf::variable_selection confirmed{};
                ::std::uint64_t confirmed_offset{};
                if(source_dwarf::query_named_variable(scopes, source.dwarf->types(), source.dwarf->variables(),
                       object.position.source.code_offset, name, confirmed, {}, frame_anchor) != source_dwarf::inline_query_error::none ||
                   confirmed.identity != selected.identity || confirmed.type != selected.type || !confirmed.location_available ||
                   confirmed.scope != selected.scope || confirmed.physical_scope != selected.physical_scope ||
                   confirmed.global != selected.global || confirmed.static_storage != selected.static_storage ||
                   !resolve_offset(confirmed, object.position.source.code_offset, confirmed_offset) || confirmed_offset != guest_offset ||
                   confirmed.location.address_bytes != object.address_bytes)
                { result.source_object_type.clear(); result.status = control_error::source_location_unmapped; return; }
                // [owned object bytes ... extent<=65536] end
                // [safe                               ] bounded immutable span;
                //  ^^ formatter sees copied guest bytes only, never a memory/native pointer.
                // Consume selectors only after the COMPLETE bounded root was
                // copied under its actual private ticket and re-selected above.
                // All subsequent offsets are confined to this owned byte image.
                status = source_dwarf::query_selected_object_value(source.dwarf->types(), confirmed.type,
                    {selector.steps.data(), selector.steps.size()}, {object.bytes.data(), object.bytes.size()}, result.source_object_type);
                if(status == source_dwarf::inline_query_error::none) { result.source_object_value_available = true; return; }
            }
            else if(status == source_dwarf::inline_query_error::none)
            {
                status = source_dwarf::query_selected_object_type(source.dwarf->types(), selected.type,
                    {selector.steps.data(), selector.steps.size()}, result.source_object_type);
            }
            if(status == source_dwarf::inline_query_error::none) { result.source_object_type_available = true; return; }
            result.source_object_type.clear();
            result.status = status == source_dwarf::inline_query_error::limit_exceeded ? control_error::exhausted :
                status == source_dwarf::inline_query_error::malformed ? control_error::source_debug_info_invalid :
                control_error::source_location_unmapped;
#else
            result.status = control_error::source_debug_info_unavailable;
#endif
        }

        // Caller owns ONLY controller mutex_. The runtime obtains the real
        // lease -> ONE domain guard -> publication guard for BOTH identities
        // and current Code-relative PC. Display traces supply no incarnation.
        [[nodiscard]] control_error source_step_position_locked(trace_record const& record,
            source_step::position& out, ::uwvm2::runtime::lib::llvm_jit_debug_activation_snapshot& activation) noexcept
        {
            out = {}; activation = {};
#if defined(UWVM_USE_LLVM_JIT) || defined(UWVM_USE_DEFAULT_JIT)
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
            try
            {
#endif
                if(native_owned_ || !record.activation_capture || !record.capture_ticket || record.participant == 0u)
                { return control_error::source_step_policy_unavailable; }
                auto const location{record.native_site_location};
                source_module const* source{};
                if(location.code_unit < source_modules_.size())
                {
                    // [controller-owned source modules] module_end
                    // [safe                           ] bounded immutable borrow
                    //  ^^ mutex_ retains the index through this cold policy query.
                    source = ::std::addressof(source_modules_[static_cast<::std::size_t>(location.code_unit)]);
                }
                ::uwvm2::runtime::lib::llvm_jit_debug_source_activation_snapshot current{};
                if(!::uwvm2::runtime::lib::llvm_jit_debug_query_source_activation_host_api(record.activation_capture,
                       source == nullptr ? ::uwvm2::runtime::lib::llvm_jit_debug_source_binding_owner{} : source->binding, current) ||
                   current.activation.participant != record.participant || current.activation.frames.empty() ||
                   current.activation.location.code_unit != location.code_unit || current.activation.location.function != location.function ||
                   current.activation.location.offset != location.offset || current.activation.location.code_generation != location.code_generation)
                { return control_error::source_step_policy_unavailable; }
                auto const& frame{current.activation.frames.back()};
                out.module = frame.module; out.function = frame.function; out.runtime_epoch = frame.runtime_epoch;
                out.function_generation = frame.function_generation; out.physical_depth = current.activation.frames.size();
                out.trace = source_step::trace_status::complete;
                out.activation = source_step::activation_relation::same;
                out.status = source_step::position_status::unmapped;
                activation = ::std::move(current.activation);
                if(!current.source_context_available || source == nullptr) { return control_error::none; }
                // The opaque owner is used only for bounded metadata equality.
                // Its real source/generation and current PC were checked jointly
                // above; no independent source query is combined with this chain.
                out.source_owner = source->binding;
                if(source->line_only)
                {
                    // Reuse ONLY this fresh joint runtime source+activation
                    // query. A map grants no memory access or inline selection.
                    out.mapping = source_step::mapping_kind::line_only;
                    out.metadata_ready = source->status == source_map_error::none;
                    if(!out.metadata_ready || !current.source_available) { return control_error::none; }
                    auto const line{source->lines.lookup(current.source.module, current.source.code_offset)};
                    if(!line) { return control_error::none; }
                    out.file = ::fast_io::concat_std(line->file); out.line = line->line; out.column = line->column;
                    out.discriminator = line->discriminator; out.is_statement = line->is_statement;
                    out.status = source_step::position_status::mapped; return control_error::none;
                }
                if(source->status == source_map_error::missing_debug_line ||
                   source->dwarf_status == source_dwarf::error::missing_sections) { return control_error::none; }
                if(source->status != source_map_error::none || source->dwarf_status != source_dwarf::error::none || !source->dwarf)
                { return control_error::source_debug_info_invalid; }
                out.metadata_ready = true;
                if(!current.source_available) { return control_error::none; }
                auto const line{source->lines.lookup(current.source.module, current.source.code_offset)};
                if(!line) { return control_error::none; }
                source_dwarf::concrete_scope_path path{};
                auto const selected{source_dwarf::query_concrete_scope_path(source->dwarf->scopes(), current.source.code_offset, path)};
                if(selected == source_dwarf::scope_path_error::unavailable || selected == source_dwarf::scope_path_error::ambiguous)
                {
                    // An overlapping compiler scope cannot supply a statement
                    // or inline path. Keep only the jointly authenticated guest
                    // context/activation chain already obtained above. over/out
                    // may skip a proven deeper helper; preparing a new source
                    // step here still fails because this position is unmapped.
                    return control_error::none;
                }
                if(selected != source_dwarf::scope_path_error::none)
                { return control_error::source_debug_info_invalid; }
                out.scope_path = ::std::move(path.physical_and_inline);
                out.file = ::fast_io::concat_std(line->file); out.line = line->line; out.column = line->column;
                out.discriminator = line->discriminator; out.is_statement = line->is_statement;
                out.status = source_step::position_status::mapped; return control_error::none;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
            }
            catch(...) { out = {}; activation = {}; return control_error::source_step_policy_unavailable; }
#endif
#else
            (void)record; return control_error::source_step_policy_unavailable;
#endif
        }
        [[nodiscard]] control_error source_destination_locked(source_step::position const& position,
            console_command const& command, source_step::origin& out) noexcept
        {
#if defined(UWVM_USE_LLVM_JIT) || defined(UWVM_USE_DEFAULT_JIT)
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
            try
            {
#endif
                if(position.module >= source_modules_.size()) { return control_error::source_location_unmapped; }
                auto const& source{source_modules_[static_cast<::std::size_t>(position.module)]};
                if(source.status != source_map_error::none || source.invalidated.size() != source.functions.size())
                { return control_error::source_debug_info_invalid; }
                auto const file{command.source_path_size == 0u ? ::fast_io::concat_std(::std::string_view{position.file}) :
                    source_map_details::normalized_path({command.source_path.data(),command.source_path_size})};
                bool found{};
                for(::std::size_t index{}; index != source.functions.size() && !found; ++index)
                {
                    auto const& function{source.functions[index]};
                    if(source.invalidated[index] || (command.source_run_to == source_run_to_policy::until && function.function != position.function)) { continue; }
                    auto const sites{::uwvm2::runtime::lib::llvm_jit_debug_safe_points_host_api(position.module,function.function)};
                    if(!sites || sites.expression_size != function.expression_size) { return control_error::source_debug_info_invalid; }
                    for(::std::size_t byte{}; byte != sites.byte_count && !found; ++byte)
                    {
                        auto const bits{sites.bits[byte]};
                        for(unsigned bit{}; bit != 8u && !found; ++bit)
                        {
                            if((bits & (1u << bit)) == 0u) { continue; }
                            if(byte > ((::std::numeric_limits<::std::uint64_t>::max)() - bit)/8u) { return control_error::source_debug_info_invalid; }
                            auto const offset{static_cast<::std::uint64_t>(byte)*8u+bit};
                            if(offset >= sites.expression_size) { break; }
                            if(function.expression_begin > (::std::numeric_limits<::std::uint64_t>::max)()-offset) { return control_error::source_debug_info_invalid; }
                            auto const pc{function.expression_begin+offset};
                            auto const line{source.lines.lookup(position.module,pc)};
                            if(!line || line->file != file || line->line != command.source_line || !line->is_statement || line->epilogue_begin) { continue; }
                            if(command.source_run_to == source_run_to_policy::until && position.mapping == source_step::mapping_kind::dwarf_scope)
                            {
                                if(!source.dwarf || source.dwarf_status != source_dwarf::error::none) { return control_error::source_debug_info_invalid; }
                                source_dwarf::concrete_scope_path path{};
                                if(source_dwarf::query_concrete_scope_path(source.dwarf->scopes(),pc,path) != source_dwarf::scope_path_error::none ||
                                   !source_step::details::prefix(position.scope_path,path.physical_and_inline)) { continue; }
                            }
                            found = true;
                        }
                    }
                }
                if(!found) { return control_error::source_location_unmapped; }
                return source_step_error(source_step::prepare_destination(position,
                    command.source_run_to == source_run_to_policy::until ? source_step::destination_policy::until : source_step::destination_policy::advance,
                    {file.data(),file.size()},command.source_line,out));
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
            }
            catch(...) { return control_error::source_step_policy_unavailable; }
#endif
#else
            (void)position; (void)command; (void)out; return control_error::source_step_policy_unavailable;
#endif
        }
        [[nodiscard]] control_error wasm_step_position_locked(trace_record const& record,
            ::uwvm2::runtime::lib::llvm_jit_debug_activation_snapshot& out) noexcept
        {
            out = {};
            auto const location{record.native_site_location};
            if(native_owned_ || !record.activation_capture || !record.capture_ticket || record.participant == 0u ||
               !::uwvm2::runtime::lib::llvm_jit_debug_query_activation_host_api(record.activation_capture, out) ||
               out.participant != record.participant || out.frames.empty() ||
               out.location.code_unit != location.code_unit || out.location.function != location.function ||
               out.location.offset != location.offset || out.location.code_generation != location.code_generation ||
               out.frames.back().function_generation != record.function_generation ||
               source_step::compare_event_chains(::std::span{::std::as_const(out.frames)},
                   ::std::span{::std::as_const(out.frames)}) != source_step::activation_relation::same)
            { out = {}; return control_error::wasm_step_policy_unavailable; }
            return control_error::none;
        }
        // GDB finish follows the SELECTED frame; next/step follow the
        // innermost execution frame. Selection remains DATA only. A caller
        // origin comes from the SAME full-cohort/closedhost/N/publication
        // transaction as its actual saved site and canonical source binding.
        [[nodiscard]] control_error source_finish_position_locked(trace_record const& record,
            console_command const& command, controller_reply& reply, source_step::position& out,
            ::uwvm2::runtime::lib::llvm_jit_debug_activation_snapshot& activation) noexcept
        {
            out = {}; activation = {};
#if defined(UWVM_USE_LLVM_JIT) || defined(UWVM_USE_DEFAULT_JIT)
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
            try
            {
#endif
                source_caller_selection chosen{};
                auto const route{source_caller_selection_locked(record, command, chosen, reply)};
                if(route == source_caller_route::rejected) { return reply.status; }
                if(route == source_caller_route::current)
                {
                    auto const status{source_step_position_locked(record, out, activation)};
                    if(status != control_error::none) { return status; }
                    if(source_frame_cursor_.bound && source_frame_cursor_.participant == record.participant)
                    {
                        // The selector above already revalidated this actual
                        // stop/top incarnation/source control block. Now match
                        // its concrete selected scope against THIS fresh path.
                        auto const ordinal{source_frame_cursor_.ordinal};
                        if(ordinal >= out.scope_path.size()) { out = {}; activation = {}; return control_error::source_location_unmapped; }
                        auto const count{out.scope_path.size() - static_cast<::std::size_t>(ordinal)};
                        // [owned true physical->inline keys ... count-1] end
                        // [safe] ordinal<size proves 0<count<=size BEFORE index.
                        if(out.scope_path[count - 1u] != source_frame_cursor_.scope)
                        { out = {}; activation = {}; return control_error::invalid_state; }
                        out.scope_path.resize(count); // bounded owned metadata truncation; no guest pointer/read.
                    }
                    return control_error::none;
                }
                if(chosen.source == nullptr || !chosen.source->binding || chosen.identity.incarnation == 0u)
                { return control_error::source_location_unmapped; }
                ::fast_io::array<::uwvm2::runtime::lib::llvm_jit_checkpoint_thread_capture_owner, 256u> captures{};
                ::std::size_t count{};
                if(!source_capture_cohort_locked(reply, record.participant, captures, count))
                { return control_error::source_location_unmapped; }
                struct context
                {
                    trace_record const& record;
                    source_caller_selection const& chosen;
                    source_step::position position{};
                    ::uwvm2::runtime::lib::llvm_jit_debug_activation_snapshot activation{};
                    control_error error{control_error::source_location_unmapped};
                } state{record, chosen};
                auto const callback{+[](void* opaque, ::uwvm2::runtime::lib::llvm_jit_debug_source_memory_view& reader) noexcept
                {
                    if(opaque == nullptr) { return false; }
                    // [THIS synchronous lexical management recipe] end
                    // [safe] runtime invokes only our bounded context; neither
                    // a guest address nor a numeric frame can forge this cast.
                    auto& state{*static_cast<context*>(opaque)};
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
                    try
                    {
#endif
                        auto const& identity{state.chosen.identity}; auto const& source{*state.chosen.source};
                        ::uwvm2::runtime::lib::llvm_jit_debug_source_activation_snapshot actual{};
                        if(!reader.copy_position(actual) || !actual.source_available || actual.activation.frames.empty() ||
                           actual.activation.participant != state.record.participant ||
                           actual.activation.frames.back().incarnation != identity.incarnation ||
                           actual.activation.frames.back().parent != identity.parent ||
                           actual.activation.frames.back().continuation != identity.continuation ||
                           actual.source.module != identity.module || actual.source.function != identity.function ||
                           actual.source.runtime_epoch != identity.runtime_epoch || actual.source.function_generation != identity.function_generation)
                        { return false; }
                        auto& position{state.position};
                        position.module = identity.module; position.function = identity.function;
                        position.runtime_epoch = identity.runtime_epoch; position.function_generation = identity.function_generation;
                        position.physical_depth = actual.activation.frames.size();
                        position.trace = source_step::trace_status::complete;
                        position.activation = source_step::activation_relation::same;
                        position.status = source_step::position_status::unmapped;
                        position.source_owner = source.binding; // comparison-only DATA owner; actual API already authenticated it.
                        state.activation = ::std::move(actual.activation); // genuine selected caller's own ancestor prefix, no native unwind.
                        if(source.status == source_map_error::missing_debug_line || source.dwarf_status == source_dwarf::error::missing_sections)
                        { state.error = control_error::none; return true; }
                        if(source.status != source_map_error::none || source.dwarf_status != source_dwarf::error::none || !source.dwarf)
                        { state.error = control_error::source_debug_info_invalid; return false; }
                        position.metadata_ready = true;
                        auto const line{source.lines.lookup(actual.source.module, actual.source.code_offset)};
                        if(!line) { state.error = control_error::none; return true; }
                        source_dwarf::concrete_scope_path path{};
                        auto const status{source_dwarf::query_concrete_scope_path(source.dwarf->scopes(), actual.source.code_offset, path)};
                        if(status == source_dwarf::scope_path_error::unavailable) { state.error = control_error::none; return true; }
                        if(status != source_dwarf::scope_path_error::none || path.physical_and_inline.empty())
                        { state.error = status == source_dwarf::scope_path_error::ambiguous ? control_error::source_location_unmapped : control_error::source_debug_info_invalid; return false; }
                        // Caller rows select the genuine PHYSICAL activation.
                        // Its call-site may itself be inside an inline DIE; do
                        // not finish that inline instance instead of this caller.
                        path.physical_and_inline.resize(1u); // nonempty owned path checked BEFORE truncation.
                        position.scope_path = ::std::move(path.physical_and_inline);
                        position.file = ::fast_io::concat_std(line->file); position.line = line->line; position.column = line->column;
                        position.discriminator = line->discriminator; position.is_statement = line->is_statement;
                        position.status = source_step::position_status::mapped; state.error = control_error::none; return true;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
                    }
                    catch(...) { state.error = control_error::exhausted; return false; }
#endif
                }};
                if(!::uwvm2::runtime::lib::llvm_jit_debug_with_source_frame_memory_host_api(pause_, {captures.data(), count},
                    record.activation_capture, chosen.source->binding, chosen.identity.incarnation, ::std::addressof(state), callback))
                { return state.error == control_error::none ? control_error::source_location_unmapped : state.error; }
                // Publish no tentative origin until the genuine manager has
                // returned success and released its coherent observation scope.
                out = ::std::move(state.position); activation = ::std::move(state.activation);
                return state.error;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
            }
            catch(...) { out = {}; activation = {}; return control_error::exhausted; }
#endif
#else
            (void)record; (void)command; (void)reply; return control_error::source_step_policy_unavailable;
#endif
        }
        [[nodiscard]] static control_error source_step_error(source_step::error value) noexcept
        {
            switch(value)
            {
                case source_step::error::none: return control_error::none;
                case source_step::error::unmapped: case source_step::error::ambiguous: return control_error::source_location_unmapped;
                case source_step::error::malformed_path: case source_step::error::malformed_position:
                    return control_error::source_debug_info_invalid;
                default: return control_error::source_step_policy_unavailable;
            }
        }

        [[nodiscard]] bool wait_native_phase(native_step::phase expected,
                                              clock_type::time_point deadline, management_wait_interrupt interrupt = {}) const noexcept
        {
            while(clock_type::now() < deadline)
            {
                // A real release ACK cannot be canceled by a host flag. Its
                // owner/cursor/event must survive until actual `released`.
                if(expected != native_step::phase::released && interrupt.pending()) { return false; }
                auto const state{native_session_.state.load(::std::memory_order_acquire)};
                if(state == expected) { return true; }
                if(expected != native_step::phase::released &&
                   (state == native_step::phase::failed || state == native_step::phase::released ||
                    state == native_step::phase::idle
#if defined(__linux__) && (defined(__x86_64__) || UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE)
                    || state == native_step::phase::continuation_cancel_pending
#endif
                   )) { return false; }
                ::std::this_thread::yield();
            }
            return native_session_.state.load(::std::memory_order_acquire) == expected;
        }
        // Caller owns mutex_. A signal park cannot be treated as a cooperative
        // stop after this call opens its gate. Never remove the fixed session
        // until the signal handler has published `released` and dropped TLS.
        [[nodiscard]] bool release_native_locked(
            ::std::optional<clock_type::time_point> deadline = {}) noexcept
        {
            if(!native_owned_) { native_code_capture_ = {}; return true; }
            if(native_external_parked_)
            {
                if(!domain_->external_unpark(pause_, native_participant_) && !domain_->is_closed()) { return false; }
                // A closed domain has already retired its external marker. It
                // grants no read authority, but the REAL backend session must
                // still drain its handler before its fixed storage/owners die.
                native_external_parked_ = false;
            }
            // clear and arm_at_return share the backend's transition guard.
            // A sampled ready state can become arming before clear acquires it,
            // especially when close wakes the guest before release_one succeeds.
            // Failed clear NEVER retires controller ownership of that borrower.
#if defined(__linux__) && (defined(__x86_64__) || UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE)
            if(native_call_continuation_count_ > native_call_continuation_owners_.size() ||
               native_call_continuation_events_.size() != native_call_continuation_owners_.size() ||
               native_return_continuation_count_ > native_return_continuation_owners_.size() ||
               native_return_continuation_events_.size() != native_return_continuation_owners_.size()) { ::fast_io::fast_terminate(); }
            bool const has_continuation_events{native_call_continuation_count_ != 0u ||
                native_return_continuation_count_ != 0u || native_continuation_event_.owns_descriptor()};
            if(has_continuation_events)
            {
                auto const state{native_session_.state.load(::std::memory_order_acquire)};
                if((state == native_step::phase::continuation_running || state == native_step::phase::continuation_cancel_pending) &&
                   !native_step::release(native_session_)) { return false; }
            }
            if(native_call_continuation_count_ != 0u)
            {
                for(::std::size_t index{}; index != native_call_continuation_count_; ++index)
                {
                    // [fixed retained event owners0 ... checked count<=256) end
                    // [safe] disable every actual plan BEFORE worker retirement;
                    //  ^^ no descriptor closes/reuses while its TLS may borrow it.
                    if(!native_call_continuation_events_[index].disable_retained()) { return false; }
                }
            }
            if(native_return_continuation_count_ != 0u)
            {
                for(::std::size_t index{}; index != native_return_continuation_count_; ++index)
                { if(!native_return_continuation_events_[index].disable_retained()) { return false; } }
            }
            if(native_continuation_event_.owns_descriptor())
            {
                if(!native_continuation_event_.disable_retained()) { return false; }
            }
            if(has_continuation_events)
            {
                // One worker/TLS retirement flag covers ALL retained event
                // families. A previous NI call must not acknowledge retirement
                // while the current return or bridge event is still enabled.
                // A failed disable above preserves the flag, FDs and owners.
                native_step::acknowledge_continuation_event_retired(native_session_);
            }
#endif
            bool const cleared_ready{native_session_.state.load(::std::memory_order_acquire) == native_step::phase::ready &&
                                     native_step::clear(native_session_)};
            if(!cleared_ready)
            {
                if(!native_step::release(native_session_) ||
                   !wait_native_phase(native_step::phase::released,
                       deadline ? *deadline : clock_type::now() + ::std::chrono::seconds{2}) ||
                   !native_step::clear(native_session_)) { return false; }
            }
            native_continuation_event_.disable_close();
            native_owned_ = false;
            native_participant_ = 0u;
            native_pc_ = 0u;
            native_location_ = {}; native_anchor_location_ = {};
            native_code_capture_ = {};
#if defined(__linux__) && (defined(__x86_64__) || UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE)
            // Backend clear has removed its provider borrow AFTER the real
            // worker ACK. Only now may the private cursor/context be destroyed.
            native_activation_cursor_ = {};
            if(native_call_continuation_count_ > native_call_continuation_owners_.size()) { ::fast_io::fast_terminate(); }
            for(::std::size_t index{}; index != native_call_continuation_count_; ++index)
            {
                // [actual fixed call-owner array 0 ... count<=256) owners_end
                // [safe] capacity was checked BEFORE insertion; release only
                //  ^^ AFTER this exact session's real worker ACK and clear.
                native_call_continuation_events_[index].disable_close();
                native_call_continuation_owners_[index] = {};
            }
            native_call_continuation_count_ = 0u;
            for(::std::size_t index{}; index != native_return_continuation_count_; ++index)
            {
                // Fixed bound checked before retirement; backend clear has
                // already removed every real signal/TLS provider borrow.
                native_return_continuation_events_[index].disable_close();
                native_return_continuation_owners_[index] = {};
            }
            native_return_continuation_count_ = 0u;
#endif
            return true;
        }
        void cancel_step_locked() noexcept
        {
            if(source_step_active_ || wasm_step_active_) { source_step_error_ = control_error::invalid_state; }
            step_thread_ = 0u; source_step_active_ = false; wasm_step_active_ = false;
            source_step_stop_count_ = 0u;
            source_step_origin_ = {}; source_step_origin_activation_ = {};
        }
        [[nodiscard]] bool resume_locked() noexcept
        {
            if(!pause_ || !release_native_locked() || !domain_->resume(pause_)) { return false; }
            pause_ = {};
            stopped_wasm_catchpoint_ = 0u; terminal_wasm_participant_ = 0u;
            pending_breakpoint_ = pending_breakpoint_participant_ = 0u; condition_error_ = control_error::none;
            clear_traces_locked();
            return true;
        }
        // Display projection of an already authenticated private owner copy.
        // Execution selection retains its separate complete-byte proof. Neither
        // this copied instruction nor its numeric PC grants a native read/write.
        template<typename Image, typename DisplayDecoder, typename SemanticDecoder>
        [[nodiscard]] static native_disassembly::instruction public_native_instruction(
            Image const& image, DisplayDecoder& display, SemanticDecoder& semantics,
            native_disassembly::instruction const& instruction) noexcept
        { return native_branch_display::project(image, display, semantics, instruction); }
        struct native_step_outcome
        {
            completion completed{completion::rejected};
            control_error error{control_error::invalid_state};
            ::std::uintptr_t from{}, to{};
            native_disassembly::instruction decoded{};
            native_next_policy::reason next_reason{native_next_policy::reason::none};
            bool timed_out{};
        };

        [[nodiscard]] native_step_outcome execute_native_finish(::std::uint_least64_t selected,
            clock_type::duration wait_budget, management_wait_interrupt interrupt)
        {
            native_step_outcome result{};
            result.error = control_error::unsupported_command;
            result.next_reason = native_next_policy::reason::caller_unwind_unavailable;
#if defined(__linux__) && (__SIZEOF_POINTER__ == 8 || (defined(__i386__) && __SIZEOF_POINTER__ == 4)) && \
    defined(UWVM2_ENABLE_DEBUG_NATIVE_CONTINUATION_LINUX_PRODUCT) && UWVM2_ENABLE_DEBUG_NATIVE_CONTINUATION_LINUX_PRODUCT == 1 && \
    ((defined(__i386__) && __SIZEOF_POINTER__ == 4) || (defined(__loongarch64) && __SIZEOF_POINTER__ == 8) || (defined(__riscv) && __riscv_xlen == 64) || (defined(__aarch64__) && defined(__AARCH64EL__) && __SIZEOF_POINTER__ == 8) || (defined(__x86_64__) && defined(PERF_ATTR_SIZE_VER7) && !defined(__ILP32__)) || (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips)))
            auto deadline{clock_type::now() + wait_budget};
            ::std::size_t event_index{};
            {
                ::std::lock_guard lock{mutex_};
                if(!pause_ || !native_owned_ || !native_external_parked_ || native_participant_ != selected ||
                   native_session_.abort_requested.load(::std::memory_order_acquire) ||
                   native_session_.state.load(::std::memory_order_acquire) != native_step::phase::trapped)
                { result.next_reason = native_next_policy::reason::current_native_trap_required; return result; }
                if(!::uwvm2::runtime::lib::llvm_jit_debug_native_activation_host_api(
                    native_activation_cursor_,::std::addressof(native_session_))) { return result; }
                if(interrupt.pending())
                { result.completed = completion::paused; result.error = control_error::none;
                  result.next_reason = native_next_policy::reason::none; return result; }
                if(native_return_continuation_count_ >= native_return_continuation_events_.size())
                { result.next_reason = native_next_policy::reason::return_continuation_capacity_exhausted; return result; }
                auto proof{::uwvm2::runtime::lib::llvm_jit_debug_mint_native_return_continuation_host_api(
                    native_activation_cursor_,::std::addressof(native_session_))};
                if(!proof) { return result; } // Root/host/unknown CFI never opens the gate.
                struct return_resume_context
                {
                    controller& manager;
                    clock_type::time_point& deadline;
                    clock_type::duration wait_budget;
                    ::std::size_t index;
                    bool woke{};
                } context{*this,deadline,wait_budget,native_return_continuation_count_};
                auto const from{native_pc_};
                bool const committed{::uwvm2::runtime::lib::llvm_jit_debug_resume_native_return_event_host_api(
                    proof,::std::addressof(native_session_),::std::addressof(context),
                    [](void* opaque, ::uwvm2::runtime::lib::llvm_jit_debug_native_return_event const& plan,
                       ::uwvm2::utils::thread::cooperative_pause_domain::external_resume_borrow& resume) noexcept
                {
                    // Exact synchronous host context; no wire address or display
                    // caller supplies a plan. The runtime holds domain/publication
                    // through the sealed backend installation and one-shot wake.
                    auto& context{*static_cast<return_resume_context*>(opaque)};
                    auto& manager{context.manager};
                    if(!plan.valid() || plan.thread() != manager.native_session_.target_thread ||
                       context.index != manager.native_return_continuation_count_ ||
                       context.index >= manager.native_return_continuation_events_.size()) { return; }
                    // [fixed arrays 0 ... checked index<256) end
                    // [safe] retain the entire sealed provider/proof BEFORE enable.
                    // Its strong origin chain must survive all later return hops
                    // until disable ALL -> real worker ACK -> backend clear.
                    auto& retained{manager.native_return_continuation_owners_[context.index]};
                    auto& event{manager.native_return_continuation_events_[context.index]};
                    retained = plan;
                    auto const cookie{mint_native_continuation_cookie()};
#if ((defined(__riscv) && __riscv_xlen == 64) || (defined(__aarch64__) && defined(__AARCH64EL__) && __SIZEOF_POINTER__ == 8) || (defined(__loongarch64) && __SIZEOF_POINTER__ == 8) || (defined(__i386__) && __SIZEOF_POINTER__ == 4) || (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips)))
                    if(cookie==0u) { retained={};return; }
#else
                    if(cookie == 0u || !event.open_disabled(plan.thread(),plan.event_pc(),cookie) || !event.enable())
                    { event.disable_close(); retained = {}; return; }
#endif
                    context.deadline = clock_type::now() + context.wait_budget;
                    if(!resume.commit([&]() noexcept
                    {
#if ((defined(__riscv) && __riscv_xlen == 64) || (defined(__aarch64__) && defined(__AARCH64EL__) && __SIZEOF_POINTER__ == 8) || (defined(__loongarch64) && __SIZEOF_POINTER__ == 8) || (defined(__i386__) && __SIZEOF_POINTER__ == 4) || (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips)))
                        return native_step::continue_to_return(manager.native_session_,plan,cookie,-1);
#else
                        return native_step::continue_to_return(manager.native_session_,plan,cookie,event.file_.native_handle());
#endif
                    }))
                    { event.disable_close(); retained = {}; return; }
                    manager.native_external_parked_ = false;
                    ++manager.native_return_continuation_count_;
                    context.woke = true;
                })};
                if(!committed || !context.woke)
                { result.next_reason = native_next_policy::reason::return_continuation_event_unavailable; return result; }
                event_index = context.index; result.from = from;
                clear_traces_locked(); // Departing child locals/source are no longer current.
            }
            auto const retain_cooperative_pause{[&]()
            {
                result.from = result.to = 0u; result.decoded = {};
                result.next_reason = native_next_policy::reason::none;
                result.timed_out = clock_type::now() >= deadline;
                // The execution deadline may already have expired. Give actual
                // event/TLS retirement and the subsequent all-participant Wasm
                // park ONE separate bounded drain budget. Never publish a fresh
                // stop merely because the old native capture was released.
                auto const drain_deadline{clock_type::now() + ::std::chrono::seconds{2}};
                result.error = control_error::invalid_state;
                {
                    ::std::lock_guard lock{mutex_};
                    cancel_step_locked(); source_step_error_ = control_error::none;
                    if(!release_native_locked(drain_deadline)) { return; } // Never drop owners before actual ACK.
                    reason_ = stop_reason::requested; changed_.notify_all();
                }
                if(wait_for_stop(drain_deadline, interrupt))
                {
                    ::std::lock_guard lock{mutex_};
                    if(exited_ || domain_->is_closed())
                    { result.completed = completion::inspected; result.error = control_error::none; }
                    else if(pause_ && domain_->capture(pause_).result ==
                            ::uwvm2::utils::thread::cooperative_pause_result::paused)
                    { reason_ = stop_reason::requested; advance_stop_identifier_locked();
                      result.completed = completion::paused; result.error = control_error::none; }
                }
                else { result.timed_out = clock_type::now() >= deadline; }
            }};
            if(!wait_native_phase(native_step::phase::trapped,deadline,interrupt) || interrupt.pending())
            {
                // Ctrl+C, nonreturning/tail/EH paths cannot become a fabricated
                // parent stop. Drain the real event and preserve the Wasm pause.
                retain_cooperative_pause(); return result;
            }
            bool published{};
            {
                ::std::lock_guard lock{mutex_};
                if(native_owned_ && !native_external_parked_ && pause_ && native_participant_ == selected &&
                   !native_session_.abort_requested.load(::std::memory_order_acquire) &&
                   event_index < native_return_continuation_count_)
                {
                    // Keep the ORIGINAL cooperative anchor for the runtime join.
                    // It is never rewritten to a fabricated parent safe point.
                    if(domain_->external_park(pause_,selected,native_anchor_location_))
                    {
                        native_external_parked_ = true;
                        auto const& event{native_return_continuation_owners_[event_index]};
                        ::uwvm2::runtime::lib::llvm_jit_debug_native_return_stop parent{};
                        ::uwvm2::runtime::lib::llvm_jit_debug_native_caller_view identity{};
                        ::uwvm2::runtime::lib::llvm_jit_debug_native_function_image actual{};
                        if(::uwvm2::runtime::lib::llvm_jit_debug_query_native_return_event_host_api(
                               event,::std::addressof(native_session_),identity) && identity.valid &&
                           ::uwvm2::runtime::lib::llvm_jit_debug_capture_native_return_stop_host_api(
                               event,::std::addressof(native_session_),parent) && parent.capture && parent.cursor &&
                           ::uwvm2::runtime::lib::llvm_jit_debug_native_activation_host_api(
                               parent.cursor,::std::addressof(native_session_)) &&
                           ::uwvm2::runtime::lib::llvm_jit_debug_copy_native_function_host_api(
                               parent.capture,::std::addressof(native_session_),actual) &&
                           actual.participant == selected && actual.stop_pc == native_session_.next_pc &&
                           actual.module == identity.module && actual.function == identity.function &&
                           actual.function_generation == identity.function_generation && actual.runtime_epoch == identity.runtime_epoch &&
                           actual.owner_begin == native_session_.owner_begin && actual.owner_end == native_session_.owner_end)
                        {
                            native_code_capture_ = parent.capture; native_activation_cursor_ = parent.cursor;
                            native_location_ = {actual.module,actual.function,0u,actual.runtime_epoch};
                            native_pc_ = actual.stop_pc; result.to = actual.stop_pc;
                            reason_ = stop_reason::native_step; advance_stop_identifier_locked(); clear_traces_locked();
                            published = true;
                        }
                    }
                }
            }
            if(!published) { retain_cooperative_pause(); return result; }
            result.completed = completion::stepped; result.error = control_error::none;
            result.next_reason = native_next_policy::reason::none;
#else
            (void)selected; (void)wait_budget; (void)interrupt;
#endif
            return result;
        }

        [[nodiscard]] static ::std::uint64_t mint_native_continuation_cookie() noexcept
        {
            static ::std::atomic<::std::uint32_t> next{1u};
            auto value{next.load(::std::memory_order_relaxed)};
            while(value != 0u && value != UINT32_MAX)
            {
                if(next.compare_exchange_weak(value, value + 1u, ::std::memory_order_relaxed))
                { return 0x554e544300000000ull | value; }
            }
            return 0u; // Never wrap/reuse an identity retained by kernel work.
        }
        [[nodiscard]] native_step_outcome execute_native_step(::std::uint_least64_t selected,
            controller_reply const& initial, clock_type::duration wait_budget, bool ordinary_next = false, management_wait_interrupt interrupt = {})
        {
            native_step_outcome result{};
            // Cold owned-code copying/MC boundary proof opens no guest gate.
            // The timeout bounds actual guest/trap waiting, so start it at the
            // authenticated wake after that preparation, including NI commits.
            auto deadline{clock_type::now() + wait_budget};
            if(interrupt.pending())
            {
                // No gate has opened. Re-prove the actual retained stop under
                // controller ownership; an earlier inspect() label is not a
                // capability if another host reset/close changed its ticket.
                ::std::lock_guard lock{mutex_};
                if(exited_ || domain_->is_closed())
                { result.completed = completion::inspected; result.error = control_error::none; return result; }
                if(!pause_ || domain_->capture(pause_).result !=
                    ::uwvm2::utils::thread::cooperative_pause_result::paused) { return result; }
                if(native_owned_)
                {
                    if(!native_external_parked_ || native_session_.abort_requested.load(::std::memory_order_acquire)) { return result; }
                    bool actual{};
                    if(!native_step::with_owned_trap(::std::addressof(native_session_),
                        [&](auto thread, auto pc, auto begin, auto end) noexcept
                        {
                            // Copies integer metadata only under the actual
                            // backend transition guard; no supplied pointer is read.
                            actual = thread == native_session_.target_thread && pc == native_pc_ &&
                                begin == native_session_.owner_begin && end == native_session_.owner_end;
                        }) || !actual) { return result; }
                }
                result.completed = completion::paused; result.error = control_error::none; return result;
            }
            if(!native_step::platform_available() ||
               (!ordinary_next && !::uwvm2::runtime::lib::llvm_jit_enable_debug_native_step_host_api()))
            { result.error = control_error::unsupported_command; return result; }
            auto const stopped{::std::find_if(initial.threads.begin(), initial.threads.end(),
                [&](auto const& item) { return item.identifier == selected; })};
            if(stopped == initial.threads.end()) { return result; }
            bool first_step{}, started_continuation{};
            ::std::uintptr_t first_successor{}, second_successor{};
            {
                ::std::lock_guard lock{mutex_};
                if(!pause_) { return result; }
                if(ordinary_next && !native_owned_)
                {
                    result.error = control_error::unsupported_command;
                    result.next_reason = native_next_policy::reason::current_native_trap_required;
                    return result; // A cooperative bridge return is not a physical next.
                }
                if(native_owned_)
                {
                    if(!native_external_parked_ || native_participant_ != selected ||
                       native_session_.state.load(::std::memory_order_acquire) != native_step::phase::trapped ||
                       native_pc_ < native_session_.owner_begin || native_pc_ >= native_session_.owner_end)
                    { return result; }
                    ::uwvm2::runtime::lib::llvm_jit_debug_native_function_image copied{};
                    if(!::uwvm2::runtime::lib::llvm_jit_debug_copy_native_function_host_api(native_code_capture_,
                           ::std::addressof(native_session_), copied) || copied.stop_pc != native_pc_ || copied.participant != selected ||
                       copied.module != native_location_.code_unit || copied.function != native_location_.function ||
                       copied.runtime_epoch != native_location_.code_generation || copied.function_generation == 0u ||
                       copied.owner_begin != native_session_.owner_begin || copied.owner_end != native_session_.owner_end ||
                       copied.size == 0u || !copied.complete_storage() ||
                       copied.owner_end <= copied.owner_begin || copied.owner_end - copied.owner_begin != copied.size)
                    { return result; }
                    native_disassembly::decoder display{copied.target};
                    native_owned_instruction_semantics::decoder semantics{copied.target};
                    // [ONE authenticated exact-sized owned function bytes] end
                    // [safe                                                   ] the
                    //  ^^ private trap + domain + lease/publication copied a complete
                    // function. MC never reads pc; all successors must remain exact
                    // decoded boundaries of THIS Wasm owner before gate mutation.
                    ::std::uintptr_t pending_npc{};
                    bool pending_delay_slot{};
#if UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE && defined(__sparc__) && defined(__arch64__)
                    bool same_kernel{};
                    if(!native_step::with_owned_registers_and_revision(::std::addressof(native_session_),
                        [&](auto thread,auto pc,auto begin,auto end,auto const& raw,auto,bool actual_delay_slot) noexcept
                        {
                            same_kernel = thread == native_session_.target_thread && pc == copied.stop_pc &&
                                begin == copied.owner_begin && end == copied.owner_end &&
                                raw.machine == native_registers::architecture::sparc64;
                            if(same_kernel)
                            { pending_npc = raw.values[33u];pending_delay_slot = actual_delay_slot; }
                        }) || !same_kernel) { return result; }
#endif
                    auto const permitted{native_wasm_step_boundary::prepare(display, semantics,
                        {copied.bytes.data(), copied.size}, copied.owner_begin, copied.owner_end, copied.stop_pc, ordinary_next,pending_npc,copied.instruction_code,pending_delay_slot)};
                    result.decoded = public_native_instruction(copied, display, semantics, permitted.decoded);
                    first_successor = permitted.first_successor;
                    second_successor = permitted.second_successor;
                    bool continuation_call{};
#if defined(__linux__) && defined(__x86_64__) && __SIZEOF_POINTER__ == 8 && \
    defined(UWVM2_ENABLE_DEBUG_NATIVE_CONTINUATION_LINUX_PRODUCT) && UWVM2_ENABLE_DEBUG_NATIVE_CONTINUATION_LINUX_PRODUCT == 1 && \
    defined(PERF_ATTR_SIZE_VER7) && !defined(__ILP32__)
                    if(!permitted && ordinary_next &&
                       permitted.unavailable_reason == native_next_policy::reason::call_continuation_unavailable)
                    {
                        if(native_call_continuation_count_ >= native_call_continuation_owners_.size())
                        {
                            result.error = control_error::unsupported_command;
                            result.next_reason = native_next_policy::reason::call_continuation_capacity_exhausted;
                            return result; // Exact trap retained; no new event or unpark.
                        }
                        // The issuer takes NO requested PC, target, register
                        // snapshot or decode DTO. It derives the actual near
                        // call from the same canonical physical trap, complete
                        // true-endpoint Wasm caller image and kernel trap. NI may
                        // run an opaque host/VM call with TF off; no callee view,
                        // code, register, stack or memory capability is issued.
                        auto const call{::uwvm2::runtime::lib::llvm_jit_debug_mint_native_call_continuation_host_api(
                            native_code_capture_, native_activation_cursor_, ::std::addressof(native_session_))};
                        if(!call)
                        {
                            result.error = control_error::unsupported_command;
                            result.next_reason = native_next_policy::reason::call_target_unavailable;
                            return result;
                        }
                        struct call_resume_context
                        {
                            controller& manager;
                            ::uwvm2::runtime::lib::llvm_jit_debug_native_function_image const& expected;
                            ::uwvm2::runtime::lib::llvm_jit_debug_native_call_continuation_owner const& call;
                            clock_type::time_point& deadline;
                            clock_type::duration wait_budget;
                            ::std::size_t index{};
                            ::std::uintptr_t continuation{};
                            native_next_policy::reason failed{native_next_policy::reason::call_target_unavailable};
                            bool woke{};
                        } context{*this, copied, call, deadline, wait_budget, native_call_continuation_count_};
                        // [complete host-stack context] exact synchronous borrow
                        // [safe] this opaque pointer is from addressof(context),
                        // never from Wasm/wire. It expires only AFTER the runtime
                        // returns from its ONE actual domain/publication commit.
                        bool const committed{::uwvm2::runtime::lib::llvm_jit_debug_resume_native_call_continuation_host_api(
                            call, ::std::addressof(native_session_), ::std::addressof(context),
                            [](void* opaque, ::uwvm2::runtime::lib::llvm_jit_debug_native_call_continuation_view const& actual,
                               ::uwvm2::utils::thread::cooperative_pause_domain::external_resume_borrow& resume) noexcept
                        {
                            // [same synchronous host-owned complete context] end
                            // [safe] only the caller above supplied this exact
                            // context; runtime invokes now under its closed true
                            // trap/current caller+callee/revision/publication.
                            auto& context{*static_cast<call_resume_context*>(opaque)};
                            auto& manager{context.manager}; auto const& expected{context.expected};
                            if(actual.thread != manager.native_session_.target_thread || actual.origin_pc != expected.stop_pc ||
                               actual.owner_begin != expected.owner_begin || actual.owner_end != expected.owner_end ||
                               actual.module != expected.module || actual.function != expected.function ||
                               actual.function_generation != expected.function_generation || actual.runtime_epoch != expected.runtime_epoch ||
                               actual.continuation_pc <= expected.stop_pc || actual.continuation_pc >= expected.owner_end ||
                               context.index != manager.native_call_continuation_count_ ||
                               context.index >= manager.native_call_continuation_owners_.size()) { return; }
                            // [fixed stable cap/event arrays0 ... checked index<256) end
                            // [safe] bounds precede BOTH owner selections. Strong
                            // ownership is installed BEFORE precise event enable;
                            // shared_ptr copy allocates nothing and cannot throw.
                            auto& event{manager.native_call_continuation_events_[context.index]};
                            auto& retained{manager.native_call_continuation_owners_[context.index]};
                            retained = context.call;
                            auto const cookie{mint_native_continuation_cookie()};
                            if(cookie == 0u || !event.open_disabled(actual.thread, actual.continuation_pc, cookie) || !event.enable())
                            {
                                // This new event was NEVER installed into the
                                // native TLS session and no worker gate opened.
                                event.disable_close(); retained = {};
                                context.failed = native_next_policy::reason::call_continuation_event_unavailable; return;
                            }
                            // Native snapshot guard ended, but the SAME actual
                            // domain + publication guards remain held throughout
                            // true flags/count removal AND the actual backend wake.
                            // This closes independent HOST replacement's old
                            // query -> unpark TOCTOU; scalar view is never detached
                            // execution permission. No recursive domain API call.
                            context.deadline = clock_type::now() + context.wait_budget;
                            if(!resume.commit([&]() noexcept
                            {
                                return native_step::continue_to_continuation(manager.native_session_, actual.continuation_pc,
                                    cookie, event.file_.native_handle());
                            }))
                            {
                                // A false backend leaves its existing trap/TLS
                                // unchanged; this uninstalled new descriptor has
                                // no execution borrower. The domain rolled back.
                                event.disable_close(); retained = {};
                                context.failed = native_next_policy::reason::call_continuation_event_unavailable; return;
                            }
                            manager.native_external_parked_ = false;
                            ++manager.native_call_continuation_count_; // index<256 above, fixed no-overflow insertion.
                            context.continuation = actual.continuation_pc;
                            context.woke = true;
                        })};
                        if(!committed || !context.woke)
                        {
                            result.error = control_error::unsupported_command;
                            result.next_reason = context.failed;
                            return result; // Original real trap remains parked if no actual commit.
                        }
                        first_successor = context.continuation; second_successor = 0u;
                        continuation_call = started_continuation = true;
                    }
#elif UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE
                    if(!permitted && ordinary_next &&
                       permitted.unavailable_reason==native_next_policy::reason::call_continuation_unavailable)
                    {
                        if(native_call_continuation_count_>=native_call_continuation_owners_.size())
                        { result.error=control_error::unsupported_command;result.next_reason=native_next_policy::reason::call_continuation_capacity_exhausted;return result; }
                        auto const call{::uwvm2::runtime::lib::llvm_jit_debug_mint_native_call_continuation_host_api(
                            native_code_capture_,native_activation_cursor_,::std::addressof(native_session_))};
                        if(!call)
                        { result.error=control_error::unsupported_command;result.next_reason=native_next_policy::reason::call_target_unavailable;return result; }
                        struct software_call_resume_context
                        {
                            controller& manager;
                            ::uwvm2::runtime::lib::llvm_jit_debug_native_function_image const& expected;
                            ::uwvm2::runtime::lib::llvm_jit_debug_native_call_continuation_owner const& call;
                            clock_type::time_point& deadline;
                            clock_type::duration wait_budget;
                            ::std::uintptr_t continuation{};
                            bool woke{};
                        } context{*this,copied,call,deadline,wait_budget};
                        bool const committed{::uwvm2::runtime::lib::llvm_jit_debug_resume_native_call_event_host_api(
                            call,::std::addressof(native_session_),::std::addressof(context),
                            [](void* opaque,::uwvm2::runtime::lib::llvm_jit_debug_native_call_event const& event,
                               ::uwvm2::runtime::lib::llvm_jit_debug_native_call_continuation_view const& actual,
                               ::uwvm2::utils::thread::cooperative_pause_domain::external_resume_borrow& resume) noexcept
                        {
                            auto& context{*static_cast<software_call_resume_context*>(opaque)};
                            auto& manager{context.manager};auto const& expected{context.expected};
                            auto const index{manager.native_call_continuation_count_};
                            if(index>=manager.native_call_continuation_owners_.size() ||
                               actual.thread!=manager.native_session_.target_thread || actual.origin_pc!=expected.stop_pc ||
                               actual.owner_begin!=expected.owner_begin || actual.owner_end!=expected.owner_end ||
                               actual.module!=expected.module || actual.function!=expected.function ||
                               actual.function_generation!=expected.function_generation || actual.runtime_epoch!=expected.runtime_epoch ||
                               actual.continuation_pc<=expected.stop_pc || actual.continuation_pc>=expected.owner_end) { return; }
                            auto& retained{manager.native_call_continuation_owners_[index]};retained=context.call;
                            context.deadline=clock_type::now()+context.wait_budget;
                            // Only the synchronous runtime-issued seal can patch
                            // the caller's return point under this actual domain
                            // and publication transaction. No callee is inspected.
                            if(!resume.commit([&]() noexcept
                            { return ::uwvm2::runtime::lib::llvm_jit_debug_continue_native_call_event_host_api(&manager.native_session_,event); }))
                            { retained={};return; }
                            manager.native_external_parked_=false;++manager.native_call_continuation_count_;
                            context.continuation=actual.continuation_pc;context.woke=true;
                        })};
                        if(!committed || !context.woke)
                        { result.error=control_error::unsupported_command;result.next_reason=native_next_policy::reason::call_continuation_event_unavailable;return result; }
                        first_successor=context.continuation;second_successor=0u;
                        continuation_call=started_continuation=true;
                    }
#endif
                    if(!permitted && !continuation_call)
                    {
                        result.error = control_error::unsupported_command;
                        result.next_reason = permitted.unavailable_reason;
                        return result; // No unpark, backend continue, TF or PC write.
                    }
                    result.from = native_pc_;
                    if(!continuation_call)
                    {
#if UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE
                        ::uwvm2::runtime::lib::llvm_jit_debug_native_breakpoint_plan plan{};
                        if(!::uwvm2::runtime::lib::llvm_jit_debug_native_breakpoint_plan_host_api(native_activation_cursor_,
                            ::std::addressof(native_session_),false,plan) || !native_step::stage_plan(native_session_,plan))
                        { result.error = control_error::unsupported_command; return result; }
#endif
                        deadline = clock_type::now() + wait_budget;
                        if(!domain_->external_unpark(pause_, selected)) { return result; }
                        native_external_parked_ = false;
                        if(!native_step::continue_from_trap(native_session_))
                        {
                            native_external_parked_ = domain_->external_park(pause_, selected, native_anchor_location_);
                            return result;
                        }
                    } // Genuine call continuation was already committed inside the ONE runtime transaction.
                }
                else
                {
                    auto const record{::std::find_if(traces_.begin(), traces_.end(),
                        [&](auto const& item) { return item.participant == selected; })};
                    if(record == traces_.end()) { return result; }
                    auto const site{record->native_site};
                    if(!site.valid || site.participant != selected || site.native_thread == 0u ||
                       site.return_pc < site.owner_begin || site.return_pc >= site.owner_end ||
                       stopped->location != record->native_site_location)
                    { return result; }
                    ::uwvm2::runtime::lib::llvm_jit_debug_native_function_image copied{};
                    if(!::uwvm2::runtime::lib::llvm_jit_debug_copy_native_function_host_api(record->activation_capture, nullptr, copied) ||
                       copied.stop_pc != site.return_pc || copied.participant != selected ||
                       copied.module != stopped->location.code_unit || copied.function != stopped->location.function ||
                       copied.runtime_epoch != stopped->location.code_generation || copied.function_generation == 0u ||
                       copied.function_generation != record->function_generation || copied.size == 0u ||
                       copied.owner_begin != site.owner_begin || copied.owner_end != site.owner_end ||
                       !copied.complete_storage() ||
                       copied.owner_end <= copied.owner_begin || copied.owner_end - copied.owner_begin != copied.size)
                    { return result; }
                    native_disassembly::decoder display{copied.target};
                    native_owned_instruction_semantics::decoder semantics{copied.target};
                    // [ONE complete exact-sized owned image] owned_end
                    // [safe                                   ] exact authenticated
                    //  ^^ range/participant/gen/epoch above. Before arming the
                    // first hardware trap, refuse every unknown/host successor.
                    auto const permitted{native_wasm_step_boundary::prepare(display, semantics,
                        {copied.bytes.data(), copied.size}, copied.owner_begin, copied.owner_end, copied.stop_pc, false,0u,copied.instruction_code)};
                    result.decoded = public_native_instruction(copied, display, semantics, permitted.decoded);
                    first_successor = permitted.first_successor;
                    second_successor = permitted.second_successor;
                    if(!permitted)
                    {
                        result.error = control_error::unsupported_command;
                        result.next_reason = permitted.unavailable_reason;
                        return result; // Preserve the original cooperative stop.
                    }
#if defined(__linux__) && (defined(__x86_64__) || UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE)
                    auto const cursor{::uwvm2::runtime::lib::llvm_jit_debug_mint_native_activation_host_api(
                        record->activation_capture, ::std::addressof(native_session_))};
                    ::uwvm2::runtime::lib::llvm_jit_debug_native_activation_provider provider{};
                    if(!cursor || !::uwvm2::runtime::lib::llvm_jit_debug_native_activation_provider_host_api(cursor, provider)) { return result; }
                    // Strong ownership precedes publication of the real session's
                    // borrowed callback. Failure leaves no active backend borrower.
                    native_activation_cursor_ = cursor;
# if UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE
                    ::uwvm2::runtime::lib::llvm_jit_debug_native_breakpoint_plan plan{};
                    if(!::uwvm2::runtime::lib::llvm_jit_debug_native_breakpoint_plan_host_api(cursor,
                        ::std::addressof(native_session_),true,plan)) { native_activation_cursor_ = {}; return result; }
                    bool installed{};
                    // Installing the entry trap and publishing its live session
                    // must finish while the real worker remains cooperatively
                    // parked. Concurrent close/resume cannot wake it into an
                    // unarmed breakpoint between patching and publication.
                    auto const held{domain_->with_stopped_participant(pause_,selected,[&](auto const& actual)
                    {
                        if(actual != stopped->location) { return; }
                        installed = native_step::request(native_session_,site.native_thread,
                            site.owner_begin,site.owner_end,site.return_pc,provider,plan);
                    })};
                    if(!held || !installed)
# else
                    if(!native_step::request(native_session_, site.native_thread,
                        site.owner_begin, site.owner_end, site.return_pc, provider))
# endif
                    { native_activation_cursor_ = {}; return result; }
#else
                    if(!native_step::request(native_session_, site.native_thread,
                        site.owner_begin, site.owner_end, site.return_pc)) { return result; }
#endif
                    native_owned_ = true;
                    native_code_capture_ = record->activation_capture;
                    native_participant_ = selected;
                    native_location_ = native_anchor_location_ = stopped->location;
                    deadline = clock_type::now() + wait_budget;
                    if(!domain_->release_one_for_native_step(pause_, selected))
                    {
                        // close may already have awakened the genuine guest and
                        // advanced ready -> arming. Retire ownership ONLY after
                        // actual backend release -> released -> clear succeeds;
                        // on a bounded-drain failure detach/destruction retains
                        // this same fixed session and must drain or fail closed.
                        static_cast<void>(release_native_locked());
                        return result;
                    }
                    first_step = true;
                }
            }
            auto const interrupt_session{[&]()
            {
                // Every cancellation entry, including one AFTER a genuine
                // continuation trap, returns to a cooperative Wasm stop. Clear
                // old physical instruction DATA BEFORE any early exit/ACK wait.
                result.from = result.to = 0u; result.decoded = {};
                result.next_reason = native_next_policy::reason::none;
                result.timed_out = clock_type::now() >= deadline;
                // The execution deadline may already have expired. Give actual
                // event/TLS retirement and the subsequent all-participant Wasm
                // park ONE separate bounded drain budget. Never publish a fresh
                // stop merely because the old native capture was released.
                auto const drain_deadline{clock_type::now() + ::std::chrono::seconds{2}};
                {
                    ::std::lock_guard lock{mutex_};
                    cancel_step_locked(); source_step_error_ = control_error::none;
                    // EXACT actual native owner/event retirement: external
                    // marker removal -> backend release -> released ACK ->
                    // clear. Never resume the cooperative pause ticket here.
                    if(!release_native_locked(drain_deadline)) { return; }
                    reason_ = stop_reason::requested; changed_.notify_all();
                }
                // The selected worker now executes with TF off until a REAL
                // Wasm safe point captures/parks under the retained request.
                // No native VM entry label or old locals become a new stop.
                if(wait_for_stop(drain_deadline, interrupt))
                {
                    ::std::lock_guard lock{mutex_};
                    if(exited_ || domain_->is_closed())
                    { result.completed = completion::inspected; result.error = control_error::none; }
                    else if(pause_ && domain_->capture(pause_).result ==
                        ::uwvm2::utils::thread::cooperative_pause_result::paused)
                    {
                        reason_ = stop_reason::requested; advance_stop_identifier_locked();
                        result.completed = completion::paused; result.error = control_error::none;
                    }
                }
                else { result.timed_out = clock_type::now() >= deadline; }
            }};
            auto const abort_session{[&]()
            {
                ::std::lock_guard lock{mutex_};
                static_cast<void>(resume_locked());
            }};
            if(first_step)
            {
                if(!wait_native_phase(native_step::phase::at_guest_pc, deadline, interrupt))
                {
                    if(interrupt.pending()) { interrupt_session(); }
                    else { result.timed_out = clock_type::now() >= deadline; abort_session(); }
                    return result;
                }
                ::std::lock_guard lock{mutex_};
                if(!native_owned_ || native_participant_ != selected || !pause_ ||
                   native_session_.state.load(::std::memory_order_acquire) != native_step::phase::at_guest_pc ||
                   native_session_.abort_requested.load(::std::memory_order_acquire)) { return result; }
                if(native_session_.first_pc != native_session_.expected_pc ||
                   !domain_->external_park(pause_, selected, native_anchor_location_))
                { static_cast<void>(resume_locked()); return result; }
                native_external_parked_ = true;
                result.from = native_session_.first_pc;
                if(!domain_->external_unpark(pause_, selected))
                { static_cast<void>(resume_locked()); return result; }
                native_external_parked_ = false;
                if(!native_step::continue_one(native_session_))
                { static_cast<void>(resume_locked()); return result; }
            }
            if(!wait_native_phase(native_step::phase::trapped, deadline, interrupt))
            {
                if(interrupt.pending() || started_continuation)
                {
                    // EH/host unwind, a nonreturning call or Ctrl+C abandons
                    // the exact native continuation. Disable and observe the
                    // genuine selected worker ACK before clearing its owners,
                    // then preserve the existing real Wasm pause request. A
                    // VM bridge/exception handler is NEVER a native stop. The
                    // old instruction/from/to DATA must not label the actual
                    // catch/safe-point stop as a completed machine instruction.
                    interrupt_session();
                }
                else { result.timed_out = clock_type::now() >= deadline; abort_session(); }
                return result;
            }
            if(interrupt.pending()) { interrupt_session(); return result; }
            {
                ::std::lock_guard lock{mutex_};
                // The execution gate was opened ONLY after proving every normal
                // successor in the same actual Wasm owner. Check completion too:
                // a signal/protocol/decoder disagreement must never be published
                // as a native JIT stop or authorize a VM/host register/code query.
                // This defensive check does not replace the pre-execution proof.
                // Waiting for a phase is not ownership: close/reset/detach may
                // have resumed or cleared it before mutex_ is reacquired. Prove
                // the ACTUAL still-trapped session before external park accounting.
                if(!native_owned_ || native_participant_ != selected || !pause_ || native_external_parked_ ||
                   native_session_.abort_requested.load(::std::memory_order_acquire)) { return result; }
                ::std::uintptr_t actual_pc{};
                bool const owned{native_step::with_owned_registers(::std::addressof(native_session_),
                    [&](auto thread, auto pc, auto begin, auto end, auto const& registers) noexcept
                {
                    if(thread == native_session_.target_thread && begin == native_session_.owner_begin &&
                       end == native_session_.owner_end && registers.pc() == pc && registers.sp() != 0u) { actual_pc = pc; }
                })};
                if(!owned || actual_pc == 0u) { return result; }
                // Native guard is already released. Preserve domain -> native
                // lock order for the joint runtime query below.

                if(first_successor == 0u ||
                   (actual_pc != first_successor && (second_successor == 0u || actual_pc != second_successor)))
                { static_cast<void>(resume_locked()); return result; }
                if(!domain_->external_park(pause_, selected, native_anchor_location_))
                { static_cast<void>(resume_locked()); return result; }
                native_external_parked_ = true;
#if defined(__linux__) && (defined(__x86_64__) || UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE)
                if(!::uwvm2::runtime::lib::llvm_jit_debug_native_activation_host_api(
                       native_activation_cursor_, ::std::addressof(native_session_)))
                { static_cast<void>(resume_locked()); return result; }
#endif
                ::uwvm2::runtime::lib::llvm_jit_debug_native_function_image actual{};
                if(!::uwvm2::runtime::lib::llvm_jit_debug_copy_native_function_host_api(native_code_capture_,
                       ::std::addressof(native_session_), actual) || actual.stop_pc != actual_pc ||
                   actual.participant != selected || actual.module != native_location_.code_unit ||
                   actual.function != native_location_.function || actual.runtime_epoch != native_location_.code_generation ||
                   actual.function_generation == 0u || actual.owner_begin != native_session_.owner_begin ||
                   actual.owner_end != native_session_.owner_end)
                { static_cast<void>(resume_locked()); return result; }
                native_pc_ = native_session_.next_pc;
                result.to = native_pc_;
                reason_ = stop_reason::native_step;
                advance_stop_identifier_locked();
                // Locals/backtraces were copied at the previous Wasm safe point.
                // One machine instruction may mutate either; never display a
                // stale snapshot as though it described this exact PC.
                clear_traces_locked();
            }
            result.completed = completion::stepped;
            result.error = control_error::none;
            return result;
        }
        // Caller owns mutex_. Numeric stop IDs only reject stale IDE requests;
        // the runtime independently authenticates the privately minted capture,
        // actual ticket/participant, publication/owner and real native trap.
        void disassemble_range_locked(console_command const& command, controller_reply& result)
        {
            if(!pause_ || stop_identifier_ == 0u || stop_identifier_ != command.disassembly_stop_identifier)
            { result.status = control_error::invalid_state; return; }
            ::uwvm2::runtime::lib::llvm_jit_debug_activation_capture_owner capture{};
            void const* session_identity{};
            if(native_owned_ && native_participant_ == command.requested_step_thread)
            {
                if(!native_external_parked_) { result.status = control_error::invalid_state; return; }
                capture = native_code_capture_;
                // [this controller's fixed native session] mutex_ pins identity
                // [safe                                 ] runtime compares its actual
                //  ^^ active private session before borrowing any native trap.
                session_identity = ::std::addressof(native_session_);
            }
            else
            {
                auto const record{::std::find_if(traces_.begin(), traces_.end(),
                    [&](auto const& item) { return item.participant == command.requested_step_thread; })};
                if(record == traces_.end()) { result.status = control_error::invalid_state; return; }
                capture = record->activation_capture;
            }
            ::uwvm2::runtime::lib::llvm_jit_debug_native_function_image copied{};
            // No outer domain mutex: the private runtime copy owns precisely
            // execution lease -> ONE domain ticket guard -> publication -> trap.
            if(!::uwvm2::runtime::lib::llvm_jit_debug_copy_native_function_host_api(capture, session_identity, copied) ||
               copied.participant != command.requested_step_thread || copied.size == 0u ||
               !copied.complete_storage() ||
               copied.owner_begin == 0u || copied.owner_end <= copied.owner_begin ||
               copied.owner_end - copied.owner_begin != copied.size || copied.stop_pc < copied.owner_begin ||
               copied.stop_pc >= copied.owner_end || copied.function_generation == 0u || copied.runtime_epoch == 0u ||
               copied.function_name_size > ::uwvm2::runtime::lib::llvm_jit_debug_max_native_function_name_bytes)
            { result.status = control_error::unsupported_command; return; }
            // [owned copied.bytes ... copied.size] end
            // [safe                             ] size equals both owned buffer extents above;
            //  ^^ span borrows only this local copied image after live owner guard ends.
            native_disassembly::decoder display_decoder{copied.target};
            auto const decoded{native_disassembly::decode_window_with(display_decoder, {copied.bytes.data(), copied.size},
                copied.owner_begin, copied.stop_pc, command.disassembly_byte_offset,
                command.disassembly_instruction_offset, static_cast<::std::size_t>(command.disassembly_count),copied.instruction_code)};
            if(!decoded.available || decoded.count != command.disassembly_count)
            { result.status = control_error::unsupported_command; return; }
            result.disassembly_stop_identifier = stop_identifier_;
            auto& identity{result.disassembly_code};
            identity.target = copied.target; identity.native_instruction_stop = copied.native_instruction_stop;
            identity.pc = copied.stop_pc; identity.participant = copied.participant;
            identity.module = copied.module; identity.function = copied.function;
            identity.function_generation = copied.function_generation; identity.runtime_epoch = copied.runtime_epoch;
            result.disassembly_owner_begin = copied.owner_begin; result.disassembly_owner_end = copied.owner_end;
            result.disassembly_count = decoded.count;
            // One cold semantic MC context for this whole owned display page.
            // A missing context leaves optional target DATA unavailable.
            native_owned_instruction_semantics::decoder semantics_decoder{copied.target};
            bool public_gap{};
            for(::std::size_t index{}; index != decoded.count; ++index)
            {
                // [fixed decoded page ... count <= 32] end
                // [safe                              ] both arrays share this capacity;
                //  ^^ only owned instruction values survive the copied-image lifetime.
                auto instruction{decoded.instructions[index]};
                if(!public_gap) { instruction = public_native_instruction(copied, display_decoder, semantics_decoder, instruction); }
                else { instruction = {}; }
                if(!instruction) { public_gap = true; }
                result.disassembly[index] = instruction;
                result.disassembly_destinations[index] = native_branch_display::decode(semantics_decoder, instruction);
            }
            if(command.disassembly_resolve_symbols && copied.function_name_size != 0u)
            {
                // [owned copied.function_name ... size<=256] [reply array capacity=256]
                // [safe                                   ] bounded full-length copy, no C string;
                //  ^^ no parser/name pointer survives the private runtime query.
                ::std::memcpy(result.disassembly_function_name.data(), copied.function_name, copied.function_name_size);
                result.disassembly_function_name_size = copied.function_name_size;
            }
        }

        // GPRs belong to frame zero at the current REAL native trap. A display
        // stop ID only rejects stale requests; it never authenticates the read.
        void registers_locked(console_command const& command, controller_reply& result)
        {
            if(!pause_ || stop_identifier_ == 0u || stop_identifier_ != command.disassembly_stop_identifier ||
               !native_owned_ || !native_external_parked_ || native_participant_ != command.requested_step_thread)
            { result.status = control_error::invalid_state; return; }
            ::uwvm2::runtime::lib::llvm_jit_debug_native_code_bytes code{};
            auto const* identity{::std::addressof(native_session_)};
            if(!::uwvm2::runtime::lib::llvm_jit_debug_copy_native_code_host_api(native_code_capture_, identity, code) ||
               code.participant != command.requested_step_thread || code.pc != native_pc_ || code.function_generation == 0u || code.runtime_epoch == 0u)
            { result.status = control_error::unsupported_command; return; }
            ::uwvm2::runtime::lib::llvm_jit_debug_native_position position{};
            if(!::uwvm2::runtime::lib::llvm_jit_debug_native_position_host_api(native_code_capture_, identity, position) ||
               position.pc != code.pc || position.participant != code.participant || position.module != code.module ||
               position.function != code.function || position.function_generation != code.function_generation ||
               position.runtime_epoch != code.runtime_epoch || position.numeric_location_count > 64u)
            { result.status = control_error::unsupported_command; return; }
            native_registers::numeric_location locations[64u]{};
            for(::std::size_t i{}; i != position.numeric_location_count; ++i)
            { locations[i] = {position.numeric_locations[i].dwarf_register, position.numeric_locations[i].bits}; }
            // The runtime copy above authenticates the actual private capture,
            // live publication/owner and originating pause ticket. The native
            // adapter independently holds its active trapped-session transition
            // guard while copying only immutable integer register values.
            bool const copied{native_step::with_owned_registers(identity,
                [&](auto thread, auto pc, auto begin, auto end, native_registers::snapshot const& registers) noexcept
                {
                    if(thread != native_session_.target_thread || pc != native_pc_ || pc != code.pc ||
                       begin == 0u || end <= begin || pc < begin || pc >= end || registers.pc() != pc || registers.size() == 0u)
                    { return false; }
                    result.registers = native_registers::project(registers, locations, position.numeric_location_count);
                    result.registers_stop_identifier = stop_identifier_;
                    result.disassembly_code = code; return true;
                })};
            if(!copied || result.registers_stop_identifier == 0u) { result.status = control_error::unsupported_command; }
        }

        void wasm_command_locked(wasm_events::request const& request, controller_reply& result)
        {
            using enum wasm_events::command_kind;
            if(!wasm_events::known_category(request.event)) { result.status = control_error::malformed; return; }
            switch(request.command)
            {
                case trace_enable:
                    if(wasm_trace_.exhausted()) { result.status = control_error::exhausted; return; }
                    wasm_trace_filter_ = request.event; wasm_trace_enabled_ = true; break;
                case trace_disable: wasm_trace_enabled_ = false; break;
                case trace_clear: wasm_trace_.clear(); break;
                case trace_read:
                {
                    if(request.maximum_records == 0u || request.maximum_records > wasm_events::maximum_trace_page_records)
                    { result.status = control_error::malformed; return; }
                    result.wasm_trace_page_available = true;
                    result.wasm_trace_next_sequence = request.after_sequence;
                    if(wasm_trace_.size() != 0u)
                    {
                        result.wasm_trace_oldest_sequence = wasm_trace_.at(0u).sequence;
                        result.wasm_trace_newest_sequence = wasm_trace_.at(wasm_trace_.size() - 1u).sequence;
                        result.wasm_trace_cursor_gap = request.after_sequence != 0u && result.wasm_trace_oldest_sequence > 1u &&
                            request.after_sequence < result.wasm_trace_oldest_sequence - 1u;
                    }
                    result.wasm_trace.reserve(static_cast<::std::size_t>(request.maximum_records));
                    for(::std::size_t i{}; i != wasm_trace_.size(); ++i)
                    {
                        // [fixed trace records ... i<size<=512] end
                        // [safe                              ] only copied integers/opcodes;
                        //  ^^ sequence labels cannot create a pause or native read capability.
                        auto const& entry{wasm_trace_.at(i)};
                        if(entry.sequence <= request.after_sequence) { continue; }
                        if(result.wasm_trace.size() == request.maximum_records) { ++result.wasm_trace_remaining; continue; }
                        result.wasm_trace.push_back(entry); result.wasm_trace_next_sequence = entry.sequence;
                    }
                    break;
                }
                case catch_set:
                {
                    if(request.module >= source_modules_.size()) { result.status = control_error::invalid_breakpoint_target; return; }
                    auto const& module{source_modules_[static_cast<::std::size_t>(request.module)]};
                    if(module.wasm_runtime_epoch == 0u) { result.status = control_error::unavailable_capability; return; }
                    bool available{};
                    for(::std::size_t i{}; i != module.wasm_functions.size(); ++i)
                    {
                        auto const& function{module.wasm_functions[i]};
                        if(i < module.invalidated.size() && function.function_generation != 0u && function.expression_size != 0u &&
                           (request.event == wasm_events::category::all || !function.expression_bytes.empty()) &&
                           (request.all_functions || function.function == request.function)) { available = true; break; }
                    }
                    if(!available) { result.status = control_error::breakpoint_not_executable; return; }
                    if(next_wasm_catchpoint_ == 0u) { result.status = control_error::exhausted; return; }
                    auto const slot{::std::find_if(wasm_catchpoints_.begin(), wasm_catchpoints_.end(),
                        [](auto const& point) { return point.identifier == 0u; })};
                    if(slot == wasm_catchpoints_.end()) { result.status = control_error::exhausted; return; }
                    *slot = {next_wasm_catchpoint_++, request.module, request.function, module.wasm_runtime_epoch,
                        request.event, request.all_functions, true};
                    ++active_wasm_catchpoints_;
                    result.wasm_catchpoint_identifier = slot->identifier; break;
                }
                case catch_delete: case catch_enable: case catch_disable:
                {
                    if(request.identifier == 0u) { result.status = control_error::malformed; return; }
                    auto const slot{::std::find_if(wasm_catchpoints_.begin(), wasm_catchpoints_.end(),
                        [&](auto const& point) { return point.identifier == request.identifier; })};
                    if(slot == wasm_catchpoints_.end()) { result.status = control_error::invalid_state; return; }
                    if(request.command == catch_delete)
                    { if(slot->enabled) { --active_wasm_catchpoints_; } *slot = {}; }
                    else
                    {
                        bool const enabled{request.command == catch_enable};
                        if(enabled != slot->enabled)
                        { if(enabled) { ++active_wasm_catchpoints_; } else { --active_wasm_catchpoints_; } }
                        slot->enabled = enabled;
                    }
                    break;
                }
                case catch_list:
                    for(auto const& point : wasm_catchpoints_) { if(point.identifier != 0u) { result.wasm_catchpoints.push_back(point); } }
                    break;
                default: result.status = control_error::malformed; return;
            }
            result.wasm_trace_enabled = wasm_trace_enabled_;
            result.wasm_trace_filter = wasm_trace_filter_;
            result.wasm_trace_overwritten = wasm_trace_.overwritten();
        }

        void disassemble_locked(console_command const& command, controller_reply& result)
        {
            // A suffix alone cannot prove a direct target's guest provenance.
            // ONE bounded complete private owner query serves both display forms;
            // the public output remains <=32 instructions / <=480 bytes on X86.
            auto view_command{command}; view_command.disassembly_resolve_symbols = false;
            disassemble_range_locked(view_command, result);
            if(result.status != control_error::none) { return; }
            auto last{result.disassembly_code.pc};
            for(::std::size_t index{}; index != result.disassembly_count; ++index)
            {
                auto& row{result.disassembly[index]};
                if(row)
                {
                    if(row.size > UINTPTR_MAX - row.pc) { result.status = control_error::unsupported_command; return; }
                    last = row.pc + row.size;
                }
                else { row.pc = last; }
            }
            // Retain authenticated bounds for mandatory target containment;
            // plain output requests no function name or symbolic resolution.
            result.disassembly_function_name_size = 0u;
        }
        // Manager-only condition evaluation after a genuine complete cohort
        // has parked. A condition string never grants capture/read authority.
        // False conditions retire the exact ticket; unavailable conditions keep
        // it stopped. Source expressions use their existing coherent reader.
        [[nodiscard]] bool filter_breakpoint_condition_locked()
        {
            if(reason_ != stop_reason::breakpoint || pending_breakpoint_ == 0u ||
               condition_checked_stop_ == stop_identifier_) { return true; }
            auto const cohort{domain_->capture(pause_)};
            if(cohort.result != ::uwvm2::utils::thread::cooperative_pause_result::paused) { return true; }
            condition_checked_stop_ = stop_identifier_; condition_error_ = control_error::none;
            auto const point{::std::find_if(breakpoints_.begin(),breakpoints_.end(),
                [&](auto const& item) { return item.identifier == pending_breakpoint_; })};
            if(point == breakpoints_.end() || point->condition_size == 0u) { return true; }
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
            try {
#endif
                auto const trace{::std::find_if(traces_.begin(),traces_.end(),
                    [&](auto const& item) { return item.participant == pending_breakpoint_participant_; })};
                if(trace == traces_.end() || point->condition_size > point->condition.size())
                { condition_error_ = control_error::source_location_unmapped; return true; }
                console_command query{}; query.kind = console_command_kind::source_value;
                query.requested_step_thread = trace->participant; query.disassembly_stop_identifier = stop_identifier_;
                query.source_frame_explicit = true; query.source_frame_ordinal = 0u;
                // Compare against Boolean false in the selected CU. C/C++
                // numeric conversion supplies scalar truth; Rust/Go/Zig keep
                // Boolean operands. Rust !!integer is bitwise, not Boolean.
                auto const text{::fast_io::concat_fast_io("(",::fast_io::string_view{point->condition.data(),point->condition_size},") != false")};
                if(text.size() > query.source_variable_name.size())
                { condition_error_ = control_error::exhausted; return true; }
                query.source_variable_name_size = text.size();
                for(::std::size_t i{}; i != text.size(); ++i) { query.source_variable_name[i] = text[i]; }
                controller_reply copied{};
                copied.execution = execution_status::stopped; copied.stop_identifier = stop_identifier_;
                for(auto const& member : cohort.participants) { copied.threads.push_back({member.id,member.location,{}}); }
                source_type_locked(*trace,query,copied);
                if(copied.status != control_error::none || !copied.source_object_value_available ||
                   copied.source_object_type.size() != 1u || !copied.source_object_type.front().value_available ||
                   (copied.source_object_type.front().scalar_kind != source_dwarf::numeric_kind::signed_integer &&
                    copied.source_object_type.front().scalar_kind != source_dwarf::numeric_kind::boolean))
                { condition_error_ = copied.status == control_error::none ? control_error::source_location_unmapped : copied.status; return true; }
                if(copied.source_object_type.front().bits != 0u)
                {
                    if(source_step_active_ || wasm_step_active_) { cancel_step_locked(); source_step_error_ = control_error::none; }
                    return true;
                }
                auto const retired_stop{stop_identifier_};
                if(!resume_locked()) { condition_error_ = control_error::invalid_state; return true; }
                condition_resumed_stop_ = retired_stop; return false;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
            } catch(...) { condition_error_ = control_error::exhausted; return true; }
#endif
        }
        void manage_conditions() noexcept
        {
            ::std::unique_lock lock{mutex_};
            for(;;)
            {
                if(condition_worker_stopping_) { return; }
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
                try {
#endif
                    if(pause_ && reason_ == stop_reason::breakpoint && pending_breakpoint_ != 0u &&
                       condition_checked_stop_ != stop_identifier_)
                    {
                        static_cast<void>(filter_breakpoint_condition_locked());
                        if(condition_error_ != control_error::none)
                        { cancel_step_locked(); source_step_error_ = control_error::none; }
                        changed_.notify_all();
                    }
                    // Poll only the cold actual pause; no guest callback may
                    // block waiting for another participant or evaluate a view.
                    changed_.wait_until(lock,clock_type::now()+::std::chrono::milliseconds{10});
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
                } catch(...)
                {
                    condition_error_ = control_error::exhausted;
                    condition_checked_stop_ = stop_identifier_; condition_worker_stopping_ = true;
                    changed_.notify_all(); return; // Keep the real pause intact.
                }
#endif
            }
        }
        [[nodiscard]] bool wait_for_stop(clock_type::time_point deadline, management_wait_interrupt interrupt = {})
        {
            for(;;)
            {
                // Same authenticated execute() manager, not a recursive wire
                // request or a callback from the OS handler. Ctrl+C cancels
                // source auto-resume and requests an ORDINARY cooperative
                // pause; only real all-participant park/capture admits success.
                if(interrupt.pending())
                {
                    ::std::lock_guard lock{mutex_};
                    auto const stepping{source_step_active_ || step_thread_ != 0u};
                    cancel_step_locked();
                    if(stepping) { source_step_error_ = control_error::none; }
                    if(reason_ == stop_reason::breakpoint && pending_breakpoint_ != 0u &&
                       condition_checked_stop_ != stop_identifier_) { reason_ = stop_reason::requested; }
                    if(!pause_ && !exited_ && !domain_->is_closed())
                    { pause_ = request_pause_locked(); reason_ = stop_reason::requested; }
                    else if(stepping && reason_ == stop_reason::step) { reason_ = stop_reason::requested; }
                    changed_.notify_all();
                }
                domain_type::pause_ticket ticket;
                ::std::uint64_t ticket_stop{};
                {
                    ::std::unique_lock lock{mutex_};
                    while(!pause_ && !exited_)
                    {
                        if(domain_->is_closed()) { return true; }
                        auto const now{clock_type::now()};
                        if(now >= deadline) { return false; }
                        changed_.wait_until(lock, (::std::min)(deadline, now + ::std::chrono::milliseconds{20}));
                        if(interrupt.pending()) { break; }
                    }
                    if(exited_) { return true; }
                    if(!pause_) { continue; } // service the host flag before forming a ticket
                    ticket = pause_; ticket_stop = stop_identifier_;
                }
                // Observers must finish minting their actual before-park copies;
                // never hold controller mutex while waiting for all participants.
                auto const wait_deadline{interrupt.requested == nullptr ? deadline :
                    (::std::min)(deadline, clock_type::now() + ::std::chrono::milliseconds{20})};
                auto const parked{domain_->wait_until_paused(ticket, wait_deadline)};
                if(parked != ::uwvm2::utils::thread::cooperative_pause_result::paused)
                {
                    if(parked == ::uwvm2::utils::thread::cooperative_pause_result::timeout && clock_type::now() < deadline) { continue; }
                    { ::std::lock_guard lock{mutex_};
                      if(ticket_stop != 0u && condition_resumed_stop_ >= ticket_stop) { continue; } }
                    return false; // never publish an incomplete or stale park as stopped
                }
                ::std::lock_guard lock{mutex_};
                if(ticket_stop != 0u && condition_resumed_stop_ >= ticket_stop) { continue; }
                if(interrupt.pending())
                {
                    auto const stepping{source_step_active_ || step_thread_ != 0u};
                    cancel_step_locked(); if(stepping) { source_step_error_ = control_error::none; }
                    if(stepping && reason_ == stop_reason::step) { reason_ = stop_reason::requested; }
                    if(reason_ == stop_reason::breakpoint && pending_breakpoint_ != 0u &&
                       condition_checked_stop_ != stop_identifier_) { reason_ = stop_reason::requested; }
                    return true; // actual ticket is already fully parked; no automatic next opcode
                }
                if(!filter_breakpoint_condition_locked())
                { if(clock_type::now() >= deadline) { return false; } continue; }
                if(condition_error_ != control_error::none)
                { cancel_step_locked(); source_step_error_ = control_error::none; return true; }
                if(!source_step_active_ && !wasm_step_active_) { return true; }
                if(source_step_stop_count_ == max_source_step_stops_)
                {
                    source_step_error_ = wasm_step_active_ ? control_error::wasm_step_policy_unavailable : control_error::source_step_policy_unavailable;
                    source_step_active_ = false; wasm_step_active_ = false; step_thread_ = 0u;
                    return true; // Retain the actual current stop, never a synthetic source label.
                }
                ++source_step_stop_count_;
                auto const found{::std::find_if(traces_.begin(), traces_.end(),
                    [&](auto const& record) { return record.participant == step_thread_; })};
                source_step::position current{};
                ::uwvm2::runtime::lib::llvm_jit_debug_activation_snapshot activation{};
                auto const unavailable{wasm_step_active_ ? control_error::wasm_step_policy_unavailable : control_error::source_step_policy_unavailable};
                source_step_error_ = found == traces_.end() ? unavailable :
                    wasm_step_active_ ? wasm_step_position_locked(*found, activation) : source_step_position_locked(*found, current, activation);
                source_step::outcome outcome{};
                if(source_step_error_ == control_error::none)
                {
                    if(activation.participant != source_step_origin_activation_.participant)
                    { source_step_error_ = unavailable; }
                    else
                    {
                        current.activation = source_step::compare_event_chains(
                            ::std::span{::std::as_const(source_step_origin_activation_.frames)}, ::std::span{::std::as_const(activation.frames)});
                        if(current.activation == source_step::activation_relation::unknown)
                        { source_step_error_ = unavailable; }
                        else
                        {
                            if(wasm_step_active_)
                            {
                                auto const policy{wasm_step_policy_ == wasm_step_policy::over ? source_step::policy::over :
                                    wasm_step_policy_ == wasm_step_policy::out ? source_step::policy::out : source_step::policy::into};
                                outcome.result = source_step::wasm_instruction_action(policy, current.activation);
                                if(outcome.result == source_step::action::decline) { source_step_error_ = unavailable; }
                            }
                            else
                            {
                                outcome = source_step::decide(source_step_origin_, current);
                                source_step_error_ = source_step_error(outcome.reason);
                                if(source_step_error_ == control_error::none && outcome.result == source_step::action::keep_running &&
                                   current.activation == source_step::activation_relation::returned &&
                                   source_step_origin_.requested != source_step::policy::into)
                                {
                                    source_step_error_ = source_step_error(source_step::observe_return(source_step_origin_,current));
                                    if(source_step_error_ == control_error::none)
                                    { source_step_origin_activation_ = ::std::move(activation); }
                                    // Rebase only on this authenticated returned ancestor. Fresh
                                    // sibling/recursive/tail identities still require the next
                                    // complete event-chain comparison; no depth/CFA guess is used.
                                }
                            }
                        }
                    }
                }
                if(source_step_error_ != control_error::none || outcome.result != source_step::action::keep_running)
                { source_step_active_ = false; wasm_step_active_ = false; step_thread_ = 0u; return true; }
                // Only an authenticated real next stop was used. Retire this
                // ticket/capture before running to another actual opcode. The
                // selected participant retains the step request across resumes.
                if(!resume_locked())
                { source_step_error_ = control_error::invalid_state; source_step_active_ = false; wasm_step_active_ = false; step_thread_ = 0u; return true; }
                if(clock_type::now() >= deadline) { return false; }
            }
        }
        [[nodiscard]] controller_reply snapshot()
        {
            ::std::lock_guard lock{mutex_};
            controller_reply result;
            result.reason = reason_;
            result.wasm_catchpoint_identifier = stopped_wasm_catchpoint_;
            result.wasm_terminal_participant = terminal_wasm_participant_;
            result.guest_exit_code = exit_code_;
            if(exited_) { result.execution = execution_status::exited; return result; }
            if(domain_->is_closed()) { result.execution = execution_status::closed; return result; }
            if(!pause_) { return result; }
            auto const stopped{domain_->capture(pause_)};
            if(stopped.result != ::uwvm2::utils::thread::cooperative_pause_result::paused)
            { result.execution = execution_status::stopping; return result; }
            if(!filter_breakpoint_condition_locked()) { result.execution = execution_status::running; return result; }
            if(condition_error_ != control_error::none) { cancel_step_locked(); source_step_error_ = control_error::none; }
            result.execution = execution_status::stopped;
            result.stop_identifier = stop_identifier_;
            result.breakpoint_condition_identifier = pending_breakpoint_;
            result.breakpoint_condition_error = condition_error_;
            for(auto const& member : stopped.participants)
            {
                stopped_thread saved{member.id, member.location, {}};
                if(native_owned_ && native_external_parked_ && member.id == native_participant_)
                {
                    saved.location = native_location_;
                    saved.native_pc = native_pc_; saved.source_inline_reason = source_inline_unavailable_reason::native_stop;
                    ::uwvm2::runtime::lib::llvm_jit_debug_native_position mapped{};
                    // The runtime owns ONE actual native-park transaction; the
                    // previous Wasm display location is never its PC authority.
                    // No outer while_stopped guard or native address request.
                    if(::uwvm2::runtime::lib::llvm_jit_debug_native_position_host_api(native_code_capture_,
                           ::std::addressof(native_session_), mapped) && mapped.pc == native_pc_ &&
                       mapped.participant == member.id && mapped.function_generation != 0u &&
                       mapped.runtime_epoch == saved.location.code_generation && mapped.owner_begin != 0u &&
                       mapped.owner_end > mapped.owner_begin && mapped.pc >= mapped.owner_begin && mapped.pc < mapped.owner_end)
                    {
                        saved.native_wasm_position = mapped;
                        // This is only display metadata for a qualified native
                        // provenance row. Original guest DWARF is usable solely
                        // for generation one; replacement keeps it retired.
                        // It grants no numeric locals, guest-memory or stepping authority.
                        if(mapped.status == ::uwvm2::runtime::lib::llvm_jit_debug_native_position_status::exact &&
                           mapped.function_generation == 1u)
                        { saved.source = find_source_locked({mapped.module, mapped.function, mapped.wasm_offset, mapped.runtime_epoch}).value; }
                    }
                }
                else { saved.source = find_source_locked(member.location).value; }
                for(auto const& trace : traces_)
                { if(trace.participant == member.id) { saved.trace = trace.trace; break; } }
#if defined(UWVM_USE_LLVM_JIT) || defined(UWVM_USE_DEFAULT_JIT)
                if(!saved.native_pc && (!saved.trace || saved.trace->frames().empty() ||
                   saved.trace->frames().front().module_id != member.location.code_unit ||
                   saved.trace->frames().front().function_index != member.location.function))
                { saved.source_inline_reason = source_inline_unavailable_reason::no_current_frame; }
                if(!saved.native_pc && saved.trace && !saved.trace->frames().empty() &&
                   saved.trace->frames().front().module_id == member.location.code_unit &&
                   saved.trace->frames().front().function_index == member.location.function &&
                   member.location.code_unit < source_modules_.size())
                {
                    auto const& source{source_modules_[static_cast<::std::size_t>(member.location.code_unit)]};
                    if(source.dwarf_status == source_dwarf::error::allocation_failure)
                    { saved.source_inline_reason = source_inline_unavailable_reason::allocation_failure; }
                    else if(source.dwarf_status == source_dwarf::error::limit_exceeded)
                    { saved.source_inline_reason = source_inline_unavailable_reason::metadata_limit; }
                    else if(source.dwarf_status != source_dwarf::error::none && source.dwarf_status != source_dwarf::error::missing_sections)
                    { saved.source_inline_reason = source_inline_unavailable_reason::invalid_metadata; }
                    else if(source.dwarf_status == source_dwarf::error::none && source.dwarf && source.binding)
                    { saved.source_inline_reason = source_inline_unavailable_reason::stale_generation_or_stop; }
                    ::uwvm2::runtime::lib::llvm_jit_debug_source_position position{};
                    // Runtime owns the ONE domain guard. Never wrap this call in
                    // while_stopped: its nonrecursive lock must not be nested.
                    // Actual location/generations come from the parked slot and
                    // live unique code publication, not this display snapshot.
                    if(source.dwarf_status == source_dwarf::error::none && source.dwarf && source.binding &&
                       ::uwvm2::runtime::lib::llvm_jit_debug_source_position_host_api(
                           source.binding, domain_, pause_, member.id, position) &&
                       position.module == member.location.code_unit && position.function == member.location.function &&
                       position.runtime_epoch == member.location.code_generation)
                    {
                        auto const queried{source_dwarf::query_inline_frames(source.dwarf->scopes(), position.code_offset, saved.source_inline_frames)};
                        saved.source_inline_available = queried == source_dwarf::inline_query_error::none;
                        switch(queried)
                        {
                            case source_dwarf::inline_query_error::none: saved.source_inline_reason = source_inline_unavailable_reason::none; break;
                            case source_dwarf::inline_query_error::unavailable: saved.source_inline_reason = source_inline_unavailable_reason::unmapped; break;
                            case source_dwarf::inline_query_error::ambiguous: saved.source_inline_reason = source_inline_unavailable_reason::ambiguous; break;
                            case source_dwarf::inline_query_error::limit_exceeded: saved.source_inline_reason = source_inline_unavailable_reason::metadata_limit; break;
                            case source_dwarf::inline_query_error::allocation_failure: saved.source_inline_reason = source_inline_unavailable_reason::allocation_failure; break;
                            case source_dwarf::inline_query_error::malformed: saved.source_inline_reason = source_inline_unavailable_reason::invalid_metadata; break;
                        }
                    }
                }
#endif
                result.threads.push_back(::std::move(saved));
            }
            return result;
        }
        void synchronize_session(controller_reply const& current) noexcept
        {
            // An observer's request is not quiescence. Only capture() above can
            // establish stopped; pending requests are never overwritten here.
            static_cast<void>(session_.confirm_execution_state(current.execution == execution_status::stopped ?
                ::uwvm2::utils::control::execution_state::stopped : ::uwvm2::utils::control::execution_state::running));
        }

    public:
        controller(controller const&) = delete;
        controller& operator=(controller const&) = delete;
        ~controller() noexcept
        {
            { ::std::lock_guard lock{mutex_}; condition_worker_stopping_ = true; changed_.notify_all(); }
            // Never join while owning mutex_: the coordinator borrows this
            // actual controller only until its joined native-thread retirement.
            if(condition_worker_.joinable()) { condition_worker_.join(); }
            // Last shared ownership has retired controller callbacks. Native
            // trap backends can still borrow native_session_ through TLS/VEH;
            // release -> observed released -> clear MUST precede destruction.
            // Never close a gate as a substitute for draining its borrower.
            if((native_owned_ && !release_native_locked()) ||
               native_session_.state.load(::std::memory_order_acquire) != native_step::phase::idle)
            { ::fast_io::fast_terminate(); }
        }
        // HOST LAUNCH ONLY. A console grant cannot be created by guest bytes.
        // An external transport will attach its own authenticated session and
        // use this same operation grammar; this factory opens no server/socket.
        [[nodiscard]] static ::std::shared_ptr<controller> create(::uwvm2::utils::control::launch_config config,
            ::std::size_t capacity = 256u, trace_ref (*capture)() noexcept =
                ::uwvm2::runtime::lib::llvm_jit_capture_debug_stack_host_api, memory_reader reader = nullptr)
        {
            if(capacity == 0u || capacity > 4096u || config.origin != ::uwvm2::utils::control::launch_origin::console ||
               ::uwvm2::utils::control::qualify(config) != control_error::none || !config.debug_enabled)
            { return {}; }
            // No actual execution has been paused yet; arm_initial_pause is the
            // separate host operation after prepare_debug_host_api succeeds.
            config.initial_execution = ::uwvm2::utils::control::execution_state::running;
            return ::std::shared_ptr<controller>{new controller{config, capacity, capture, reader}};
        }
        [[nodiscard]] control_error status() const noexcept { return admission_; }
        // Read-only HOST UI metadata. Names/indices are DATA, never native
        // addresses or breakpoint permission. Callers still submit a normal
        // breakpoint command to execute(), which authenticates the live target.
        // Completion cannot resurrect original DWARF after replacement.
        template<typename Emit>
        [[nodiscard]] bool visit_source_function_symbols(::std::uint64_t module_id,
            ::fast_io::string_view prefix, Emit emit)
        {
            ::std::lock_guard lock{mutex_};
#if defined(UWVM_USE_LLVM_JIT) || defined(UWVM_USE_DEFAULT_JIT)
            if(admission_ != control_error::none || exited_ || domain_->is_closed() ||
               module_id >= source_modules_.size()) { return true; }
            auto const& source{source_modules_[static_cast<::std::size_t>(module_id)]};
            if(!source.binding || !source.dwarf || source.dwarf_status != source_dwarf::error::none ||
               source.invalidated.size() != source.functions.size()) { return true; }
            ::std::size_t work{};
            for(auto const& scope : source.dwarf->scopes())
            {
                if(++work > 262144u) { return false; }
                ::fast_io::string_view name{scope.name.data(),scope.name.size()};
                if(scope.kind != source_dwarf::scope_kind::subprogram || !scope.concrete || name.empty() || !name.starts_with(prefix)) { continue; }
                for(::std::size_t i{}; i < source.functions.size(); ++i)
                {
                    if(++work > 262144u) { return false; }
                    if(source.invalidated[i]) { continue; }
                    auto const& function{source.functions[i]};
                    auto const sites{::uwvm2::runtime::lib::llvm_jit_debug_safe_points_host_api(module_id,function.function)};
                    if(!sites || sites.function_generation != 1u || sites.expression_size != function.expression_size) { continue; }
                    bool found{}; ::std::uint64_t offset{};
                    for(auto const& range : scope.ranges)
                    {
                        if(++work > 262144u) { return false; }
                        if(range.begin >= range.end || range.end <= function.expression_begin ||
                           (range.begin >= function.expression_begin && range.begin-function.expression_begin >= function.expression_size)) { continue; }
                        auto const first{range.begin > function.expression_begin ? range.begin-function.expression_begin : 0u};
                        auto const last{::std::min(range.end-function.expression_begin,function.expression_size)};
                        for(auto at{first}; at < last; ++at)
                        {
                            if(++work > 262144u) { return false; }
                            if(sites.contains(at)) { if(!found || at < offset) { offset=at; found=true; } break; }
                        }
                    }
                    if(found && !emit(name,function.function,offset)) { return false; }
                }
            }
#endif
            return true;
        }

        // Called only after LLVM-full precompilation has assigned module IDs
        // and before the first guest thread starts. DWARF errors are retained
        // per module so Wasm-level debugging remains available.
        [[nodiscard]] bool install_source_module(::std::uint64_t id,
            source_map_sections sections, ::std::vector<source_function_span> functions,
            bool invalid_metadata = false)
        {
            ::std::lock_guard lock{mutex_};
            if(source_installation_closed_ || pause_ || exited_ || id > 65536u) { return false; }
            if(id >= source_modules_.size()) { source_modules_.resize(static_cast<::std::size_t>(id + 1u)); }
            auto& target{source_modules_[static_cast<::std::size_t>(id)]};
            target.functions = ::std::move(functions);
            target.invalidated.assign(target.functions.size(), false);
            target.status = invalid_metadata ? source_map_error::malformed :
                source_map::parse(id, sections, target.lines);
            target.line_only = target.status == source_map_error::none &&
                sections.debug_line.empty() && sections.source_map_declared;
            if(target.status == source_map_error::none)
            {
                if(!::std::is_sorted(target.functions.begin(), target.functions.end(),
                    [](auto const& a, auto const& b) { return a.function < b.function; }) ||
                   ::std::adjacent_find(target.functions.begin(), target.functions.end(),
                    [](auto const& a, auto const& b) { return a.function == b.function; }) != target.functions.end())
                { target.status = source_map_error::malformed; }
            }
            return true;
        }
#if defined(UWVM_USE_LLVM_JIT) || defined(UWVM_USE_DEFAULT_JIT)
        // Native serialized setup only, before the first guest starts. The
        // image is copied by the actual runtime factory; an arbitrary file or
        // caller-constructed image is never accepted as source identity.
        [[nodiscard]] bool install_source_metadata(::std::uint64_t id,
            ::uwvm2::runtime::lib::llvm_jit_debug_source_binding_owner binding)
        {
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
            try
            {
#endif
            ::uwvm2::runtime::lib::llvm_jit_debug_source_image image{};
            if(!binding || !::uwvm2::runtime::lib::llvm_jit_debug_copy_source_image_host_api(binding, image)) { return false; }
            ::std::vector<source_dwarf::section> sections{};
            sections.reserve(image.sections.size());
            bool external{};
            for(auto const& entry : image.sections)
            {
                if(entry.name == ".gnu_debuglink") { external = true; }
                // [runtime-owned copied image payload] payload_end
                // [safe                             ] borrowed only during parse,
                //  ^^ index immediately makes its own immutable buffer copies.
                sections.push_back({entry.name, entry.payload});
            }
            ::std::unique_ptr<source_dwarf::index> metadata{};
            source_dwarf::image_input input{sections, image.code_section_content_size, 4u};
            auto status{external ? source_dwarf::error::unsupported_external : source_dwarf::index::parse(input, metadata)};
            if(status == source_dwarf::error::unsupported_dwarf && !external)
            {
                // Wasm64 compiler metadata can use eight-byte Code addresses.
                // Width is parsed/rejected by the same embedded-only preflight;
                // no guest memory address or runtime-value read is involved.
                input.address_bytes = 8u; status = source_dwarf::index::parse(input, metadata);
            }
            ::std::lock_guard lock{mutex_};
            if(source_installation_closed_ || pause_ || exited_ || id >= source_modules_.size()) { return false; }
            auto& target{source_modules_[static_cast<::std::size_t>(id)]};
            if(image.functions.size() != target.functions.size()) { return false; }
            for(::std::size_t i{}; i != image.functions.size(); ++i)
            {
                auto const& actual{image.functions[i]}; auto const& mapped{target.functions[i]};
                if(actual.function != mapped.function || actual.expression_begin != mapped.expression_begin ||
                   actual.expression_size != mapped.expression_size || actual.function_generation != 1u) { return false; }
            }
            if(status == source_dwarf::error::none && metadata && target.status == source_map_error::none && !target.line_only)
            {
                source_map_sections mapped_sections{};
                mapped_sections.code_section_content_size = image.code_section_content_size;
                for(auto const& entry : image.sections)
                {
                    if(entry.name == ".debug_line") { mapped_sections.debug_line = entry.payload; }
                    else if(entry.name == ".debug_line_str") { mapped_sections.debug_line_str = entry.payload; }
                    else if(entry.name == ".debug_str") { mapped_sections.debug_str = entry.payload; }
                }
                ::std::vector<source_line_compilation_directory> directories{};
                for(auto const& entry : metadata->line_directories())
                { directories.push_back({entry.line_unit_offset, entry.directory}); }
                mapped_sections.compilation_directories = directories;
                // Reparse only the runtime-bound copied module. Paths and CU
                // offsets grant no source file read or native PC authority.
                target.status = source_map::parse(id, mapped_sections, target.lines);
            }
            target.dwarf_status = status; target.dwarf = ::std::move(metadata); target.binding = ::std::move(binding);
            target.wasm_runtime_epoch = image.runtime_epoch;
            target.wasm_functions = ::std::move(image.functions);
            return true; // malformed/unavailable metadata preserves Wasm/native debugging.
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
            }
            catch(...)
            {
                ::std::lock_guard lock{mutex_};
                if(source_installation_closed_ || pause_ || exited_ || id >= source_modules_.size()) { return false; }
                auto& target{source_modules_[static_cast<::std::size_t>(id)]};
                target.dwarf.reset(); target.binding.reset(); target.dwarf_status = source_dwarf::error::allocation_failure;
                return true;
            }
#endif
        }
#endif
        [[nodiscard]] ::std::shared_ptr<domain_type> domain() const noexcept { return domain_; }
        // Normal HOST shutdown only. The runtime owner must already have
        // closed this actual domain before borrowing any native retirement.
        // `retired` proves only this controller's native backend/event/cursor
        // borrowers retired; it is NOT execution drain or physical thread join.
        enum class native_shutdown_retirement : unsigned char { retired, pending, invalid_state };
        [[nodiscard]] native_shutdown_retirement try_retire_native_for_shutdown(
            ::std::chrono::steady_clock::time_point deadline) noexcept
        {
            ::std::unique_lock lock{mutex_, ::std::try_to_lock};
            if(!lock.owns_lock()) { return native_shutdown_retirement::pending; }
            if(!domain_->is_closed()) { return native_shutdown_retirement::invalid_state; }
            cancel_step_locked();
            // release keeps all actual owner/event/cursor fields on a pending
            // real worker ACK. A deadline never supplies `released` itself.
            if(!release_native_locked(deadline) || native_owned_ ||
               native_session_.state.load(::std::memory_order_acquire) != native_step::phase::idle)
            { return native_shutdown_retirement::pending; }
            pause_ = {}; stopped_wasm_catchpoint_ = 0u; terminal_wasm_participant_ = 0u;
            clear_traces_locked(); changed_.notify_all();
            return native_shutdown_retirement::retired;
        }
        [[nodiscard]] ::uwvm2::runtime::lib::llvm_jit_debug_observer observer()
        { return {shared_from_this(), observe, before_park, retire_on_close, observe_trap, observe_uncaught}; }
        // Only during serialized host startup, before starting the guest worker.
        [[nodiscard]] bool arm_initial_pause()
        {
            if(admission_ != control_error::none) { return false; }
            { ::std::lock_guard lock{mutex_}; if(pause_ || exited_) { return false; }
              source_installation_closed_ = true;
              pause_ = request_pause_locked(); reason_ = stop_reason::initial; }
            auto const current{snapshot()};
            if(current.execution != execution_status::stopped) { return false; }
            synchronize_session(current);
            return true;
        }
        void notify_guest_exit(::std::int_least64_t code) noexcept
        {
            ::std::lock_guard lock{mutex_};
            exited_ = true; exit_code_ = code; step_thread_ = 0u; source_step_active_ = false; wasm_step_active_ = false;
            source_installation_closed_ = true;
            native_code_capture_ = {};
            source_step_origin_ = {}; source_step_origin_activation_ = {}; changed_.notify_all();
        }
        // The authenticated server's detach/peer-loss path promises that the
        // inferior continues. Release any kernel-trapped native step before
        // returning from the management thread; no signal handler enters here.
        [[nodiscard]] bool detach_resume() noexcept
        {
            ::std::lock_guard lock{mutex_};
            // Peer loss can occur BETWEEN the internal source-step pauses.
            // Cancel its selected-thread request even while currently running,
            // so the next opcode cannot re-park an already detached inferior.
            cancel_step_locked();
            if(!release_native_locked()) { return false; }
            if(exited_ || domain_->is_closed() || !pause_) { return true; }
            return resume_locked();
        }
        [[nodiscard]] controller_reply inspect()
        { auto result{snapshot()}; synchronize_session(result); return result; }

        [[nodiscard]] controller_reply execute(console_command const& command, management_wait_interrupt interrupt)
        { return execute(command, ::std::chrono::seconds{2}, interrupt); }

        [[nodiscard]] controller_reply execute(console_command const& input_command,
            ::std::chrono::milliseconds timeout = ::std::chrono::seconds{2}, management_wait_interrupt interrupt = {})
        {
            // Synchronous host-only observation. Once THIS command has seen
            // Ctrl+C, neither a consumed adapter flag nor a later timeout may
            // turn its pending cooperative pause into an implicit resume.
            management_wait_interrupt_observation observed_interrupt{interrupt};
            if(interrupt.requested != nullptr) { interrupt = observed_interrupt.borrow(); }
            auto command{input_command};
            auto initial{inspect()};
            if(admission_ != control_error::none) { initial.status = admission_; return initial; }
            if(command.kind == console_command_kind::assembly_next || command.kind == console_command_kind::assembly_finish)
            {
                // Current top native frame only: this command has no frame
                // ordinal/caller PC and never uses a cooperative saved trace.
                ::std::lock_guard lock{mutex_};
                if(!native_owned_ || !native_external_parked_ || !native_code_capture_ || native_participant_ == 0u ||
                   !pause_ || stop_identifier_ == 0u || domain_->is_closed() ||
                   native_session_.state.load(::std::memory_order_acquire) != native_step::phase::trapped ||
                   initial.execution != execution_status::stopped || initial.stop_identifier != stop_identifier_ ||
                   (command.requested_step_thread != 0u && command.requested_step_thread != native_participant_) ||
                   (command.disassembly_stop_identifier != 0u && command.disassembly_stop_identifier != stop_identifier_))
                {
                    initial.status = control_error::unsupported_command;
                    initial.native_next_reason = native_next_policy::reason::current_native_trap_required;
                    return initial; // Reject before backend enable/request or inferior mutation.
                }
                if(command.payload_size == 0u && command.requested_step_thread == 0u)
                {
                    command.requested_step_thread = native_participant_; command.payload_size = 8u;
                    ::uwvm2::utils::control::output_buffer output{command.payload};
                    ::fast_io::io::print(output, ::fast_io::mnp::le_put<64>(native_participant_));
                }
                command.disassembly_stop_identifier = stop_identifier_;
            }
            if(command.kind == console_command_kind::assembly_registers && command.payload_size == 0u &&
               command.requested_step_thread == 0u && command.disassembly_stop_identifier == 0u)
            {
                // GDB-style current-frame spelling. The controller, not a guest
                // number, selects its own actual externally parked native session.
                ::std::lock_guard lock{mutex_};
                if(!native_owned_ || !native_external_parked_ || native_participant_ == 0u || !pause_ || stop_identifier_ == 0u)
                { initial.status = control_error::invalid_state; return initial; }
                command.requested_step_thread = native_participant_; command.disassembly_stop_identifier = stop_identifier_;
                command.payload_size = 8u;
                ::uwvm2::utils::control::output_buffer output{command.payload};
                ::fast_io::io::print(output, ::fast_io::mnp::le_put<64>(native_participant_));
            }
            if((command.kind == console_command_kind::source_type || command.kind == console_command_kind::source_value ||
                command.kind == console_command_kind::source_frame) && command.payload_size == 0u &&
               command.requested_step_thread == 0u && command.disassembly_stop_identifier == 0u)
            {
                // Prefer the explicitly selected stopped participant. Selection
                // supplies a scalar label only: each data/frame query repeats the
                // genuine runtime source+activation proof before borrowing storage.
                ::std::lock_guard lock{mutex_};
                if(initial.execution != execution_status::stopped || !pause_ || native_owned_ ||
                   initial.stop_identifier == 0u || initial.stop_identifier != stop_identifier_)
                { initial.status = control_error::invalid_state; return initial; }
                ::std::uint64_t participant{};
                if(source_frame_cursor_.bound)
                {
                    if(source_frame_cursor_.stop != stop_identifier_)
                    { initial.status = control_error::invalid_state; return initial; }
                    participant = source_frame_cursor_.participant;
                }
                else
                {
                    if(initial.threads.size() != 1u)
                    { initial.status = control_error::invalid_state; return initial; }
                    // [owned stopped participant rows ... only row] end
                    // [safe                                      ] size == 1 BEFORE front().
                    participant = initial.threads.front().identifier;
                }
                auto const selected_thread{::std::find_if(initial.threads.begin(), initial.threads.end(),
                    [=](auto const& thread) { return thread.identifier == participant; })};
                if(selected_thread == initial.threads.end() || selected_thread->native_pc)
                { initial.status = control_error::invalid_state; return initial; }
                command.requested_step_thread = participant;
                command.disassembly_stop_identifier = initial.stop_identifier; command.payload_size = 8u;
                ::uwvm2::utils::control::output_buffer output{command.payload};
                ::fast_io::io::print(output, ::fast_io::mnp::le_put<64>(command.requested_step_thread));
            }
            // A confirmed fatal helper can only be inspected or continued to
            // termination. It is never a native step/replace/mutation landing.
            if((initial.reason == stop_reason::wasm_trap || initial.reason == stop_reason::wasm_uncaught) &&
               (command.operation == operation::step || command.kind == console_command_kind::source_step ||
                command.kind == console_command_kind::wasm_step || command.kind == console_command_kind::assembly_step ||
                command.kind == console_command_kind::assembly_next || command.kind == console_command_kind::assembly_finish ||
                command.kind == console_command_kind::replacement_file || command.kind == console_command_kind::wasm_mutation ||
                command.source_run_to != source_run_to_policy::none))
            { initial.status = control_error::invalid_state; return initial; }
            bool const breakpoint_control{command.kind == console_command_kind::breakpoint_control};
            bool const replacement_file{command.kind == console_command_kind::replacement_file};
            bool const source_step{command.kind == console_command_kind::source_step};
            bool const wasm_step{command.kind == console_command_kind::wasm_step};
            bool const source_breakpoint{command.kind == console_command_kind::source_breakpoint};
            bool const ordinary_next{command.kind == console_command_kind::assembly_next};
            bool const assembly_finish{command.kind == console_command_kind::assembly_finish};
            bool const assembly_step{command.kind == console_command_kind::assembly_step || ordinary_next || assembly_finish};
            bool const source_locals{command.kind == console_command_kind::source_locals};
            bool const disassembly_range{command.kind == console_command_kind::assembly_disassemble_range};
            bool const disassembly{command.kind == console_command_kind::assembly_disassemble || disassembly_range};
            bool const registers{command.kind == console_command_kind::assembly_registers};
            bool const wasm_event{command.kind == console_command_kind::wasm_event};
            bool const wasm_state_query{command.kind == console_command_kind::wasm_state};
            bool const wasm_mutation_query{command.kind == console_command_kind::wasm_mutation};
            bool const wasm_path_query{command.kind == console_command_kind::wasm_path};
            bool const wasip1_state_query{command.kind == console_command_kind::wasip1_state};
            bool const wasm_script{command.kind == console_command_kind::wasm_script};
            bool const source_value{command.kind == console_command_kind::source_value};
            bool const source_type{command.kind == console_command_kind::source_type || source_value};
            bool const source_frame{command.kind == console_command_kind::source_frame};
            if(command.source_run_to != source_run_to_policy::none &&
               (!source_step || command.source_policy != source_step_policy::over || command.source_selected_finish ||
                command.operation != operation::step || command.payload_size != 8u || command.requested_step_thread == 0u ||
                command.disassembly_stop_identifier == 0u || command.source_line == 0u ||
                command.source_path_size >= command.source_path.size() || command.wait_for_event ||
                (command.source_run_to != source_run_to_policy::until && command.source_run_to != source_run_to_policy::advance)))
            { initial.status = control_error::malformed; return initial; }
            if(command.source_selected_finish && (!source_step || command.source_policy != source_step_policy::out))
            { initial.status = control_error::malformed; return initial; }
            if(((command.breakpoint_initial_ignore || command.breakpoint_initial_condition) && (command.operation != operation::breakpoint_set ||
                   (command.kind != console_command_kind::protocol && !source_breakpoint))) ||
               (command.kind != console_command_kind::protocol && !replacement_file && !source_step && !wasm_step && !source_breakpoint && !assembly_step && !source_locals && !disassembly && !registers && !wasm_event && !wasm_script && !source_type && !source_frame && !wasm_state_query && !wasip1_state_query && !wasm_path_query && !wasm_mutation_query && !breakpoint_control) ||
               (breakpoint_control && (command.operation != operation::status || command.payload_size != 0u || command.wait_for_event ||
                    (command.breakpoint_policy != breakpoint_action::enable && command.breakpoint_policy != breakpoint_action::disable &&
                     command.breakpoint_policy != breakpoint_action::ignore && command.breakpoint_policy != breakpoint_action::condition) ||
                    ((command.breakpoint_policy == breakpoint_action::ignore || command.breakpoint_policy == breakpoint_action::condition) && command.breakpoint_id == 0u))) ||
               (replacement_file != (command.operation == operation::replace_function)) ||
               ((wasm_event || wasm_script) && (command.operation != operation::status || command.payload_size != 0u || command.wait_for_event)) ||
               (wasip1_state_query && (command.operation != operation::status || command.payload_size != 0u ||
                                      command.wait_for_event || !wasip1_state::valid(command.wasip1_state_request))) ||
               (wasm_path_query && (command.operation!=operation::status || command.payload_size!=0u ||
                                     command.wait_for_event || !wasm_path::valid(command.wasm_path_request))) ||
               (wasm_mutation_query && (command.operation!=operation::status || command.payload_size!=0u ||
                    command.wait_for_event || !wasm_mutation::valid(command.wasm_mutation_request))) ||
               (wasm_state_query && (command.operation != operation::status || command.payload_size != 0u ||
                                    command.wait_for_event || !wasm_state::valid(command.wasm_state_request))) ||
               (registers && (command.operation != operation::backtrace || command.payload_size != 8u || command.disassembly_stop_identifier == 0u ||
                              command.register_name_size > command.register_name.size())) ||
               (source_frame && (command.operation != operation::backtrace || command.payload_size != 8u ||
                                 command.disassembly_stop_identifier == 0u || command.requested_step_thread == 0u ||
                                 command.frame_page_count == 0u || command.frame_page_count > 128u)) ||
               (source_type && (command.operation != operation::locals || command.payload_size != 8u || command.disassembly_stop_identifier == 0u ||
                                command.source_variable_name_size == 0u || command.source_variable_name_size > command.source_variable_name.size())) ||
               ((ordinary_next || assembly_finish) && (command.operation != operation::step || command.payload_size != 8u ||
                                  command.requested_step_thread == 0u || command.wait_for_event)) ||
               (source_locals && (command.operation != operation::locals || command.payload_size != 8u ||
                                  (command.source_frame_explicit && (command.disassembly_stop_identifier == 0u || command.requested_step_thread == 0u)))) ||
               (disassembly && (command.operation != operation::backtrace || command.payload_size != 8u ||
                                command.disassembly_stop_identifier == 0u || command.disassembly_count == 0u ||
                                command.disassembly_count > 32u ||
                                (disassembly_range && (command.disassembly_byte_offset < -65536 || command.disassembly_byte_offset > 65536 ||
                                  command.disassembly_instruction_offset < -8704 || command.disassembly_instruction_offset > 8704)))) ||
               (source_breakpoint && (command.operation != operation::breakpoint_set || command.payload_size != 0u ||
                                      command.source_line == 0u || command.source_path_size == 0u ||
                                      command.source_path_size >= command.source_path.size())) ||
               (source_step && command.source_policy != source_step_policy::into &&
                               command.source_policy != source_step_policy::over &&
                               command.source_policy != source_step_policy::out) ||
               (wasm_step && (command.operation != operation::step || command.payload_size != 8u ||
                    command.requested_step_thread == 0u || command.wait_for_event ||
                    (command.wasm_policy != wasm_step_policy::into && command.wasm_policy != wasm_step_policy::over &&
                     command.wasm_policy != wasm_step_policy::out))) ||
               command.payload_size > command.payload.size() ||
               (command.wait_for_event && command.operation != operation::status))
            { initial.status = control_error::malformed; return initial; }
            if(command.breakpoint_condition_size > command.breakpoint_condition.size() ||
               (command.breakpoint_condition_size != 0u && !command.breakpoint_initial_condition &&
                !(breakpoint_control && command.breakpoint_policy == breakpoint_action::condition)))
            { initial.status = control_error::malformed; return initial; }
            if(command.breakpoint_initial_condition || (breakpoint_control && command.breakpoint_policy == breakpoint_action::condition))
            {
                if(command.breakpoint_initial_condition && command.breakpoint_condition_size == 0u)
                { initial.status = control_error::malformed; return initial; }
                for(::std::size_t i{}; i != command.breakpoint_condition_size; ++i)
                { auto const c{static_cast<unsigned char>(command.breakpoint_condition[i])};
                  if(c < 32u || c >= 127u) { initial.status = control_error::malformed; return initial; } }
                source_scalar_expression::program syntax{};
                auto const condition_text{::fast_io::concat_fast_io("(",::fast_io::string_view{
                    command.breakpoint_condition.data(),command.breakpoint_condition_size},") != false")};
                if(command.breakpoint_condition_size != 0u && source_scalar_expression::parse_admitted(
                    {condition_text.data(),condition_text.size()},syntax) != source_scalar_expression::error::none)
                { initial.status = control_error::malformed; return initial; }
            }
            if(command.operation == operation::detach)
            { initial.status = control_error::unsupported_command; return initial; }
            if(replacement_file && !config_.replacement_enabled)
            { initial.status = control_error::unavailable_capability; return initial; }
            if(replacement_file)
            {
                ::std::lock_guard lock{mutex_};
                // A native trap is not a Wasm safe point: its captured Wasm
                // trace is deliberately invalidated after one machine step.
                if(native_owned_) { initial.status = control_error::invalid_state; return initial; }
            }
            // Prevent ordinary console mistakes from poisoning the authenticated
            // stream. The wire session independently repeats all state checks.
            bool const stopped{initial.execution == execution_status::stopped};
            bool const running{initial.execution == execution_status::running || initial.execution == execution_status::stopping};
            if(((command.operation == operation::resume || command.operation == operation::step ||
                 command.operation == operation::backtrace || command.operation == operation::read_memory ||
                 command.operation == operation::locals || replacement_file || wasm_state_query ||
                 (wasip1_state_query && !wasip1_calls::is_trace(command.wasip1_state_request.operation)) || wasm_mutation_query ||
                 (wasm_path_query && command.wasm_path_request.action!=wasm_path::operation::clear)) && !stopped) ||
               (command.operation == operation::pause && !running))
            { initial.status = control_error::invalid_state; return initial; }
            if((command.operation == operation::step || command.operation == operation::backtrace ||
                command.operation == operation::locals) && command.payload_size == 8u)
            {
                ::std::uint64_t id{};
                ::uwvm2::utils::control::input_buffer input{command.payload};
                ::fast_io::io::scan(input, ::fast_io::mnp::le_get<64>(id));
                if((source_step || wasm_step || assembly_step || disassembly || registers || source_type || source_frame ||
                    (source_locals && command.requested_step_thread != 0u)) && id != command.requested_step_thread)
                { initial.status = control_error::malformed; return initial; }
                if(::std::none_of(initial.threads.begin(), initial.threads.end(), [=](auto const& thread) { return thread.identifier == id; }))
                { initial.status = control_error::invalid_state; return initial; }
            }
            ::uwvm2::uwvm::debugger::source_step::origin prepared_source_step{};
            ::uwvm2::runtime::lib::llvm_jit_debug_activation_snapshot prepared_activation{};
            if(wasm_step)
            {
                ::std::lock_guard lock{mutex_};
                auto const record{::std::find_if(traces_.begin(), traces_.end(),
                    [&](auto const& item) { return item.participant == command.requested_step_thread; })};
                initial.status = record == traces_.end() ? control_error::wasm_step_policy_unavailable :
                    wasm_step_position_locked(*record, prepared_activation);
                if(initial.status != control_error::none) { return initial; }
            }
            if(source_step)
            {
                ::std::lock_guard lock{mutex_};
                if(native_owned_ || initial.reason == stop_reason::initial ||
                   (command.source_run_to != source_run_to_policy::none &&
                    (!pause_ || stop_identifier_ != command.disassembly_stop_identifier)))
                { initial.status = control_error::invalid_state; return initial; }
                auto const record{::std::find_if(traces_.begin(), traces_.end(),
                    [&](auto const& item) { return item.participant == command.requested_step_thread; })};
                ::uwvm2::uwvm::debugger::source_step::position position{};
                // GDB step/next execute the innermost frame even after the
                // user selected a caller to inspect. Only finish targets the
                // selected frame through an explicit HOST command flag; ordinary
                // source-out/DAP stepOut still follows the innermost frame. A
                // selected finish requires a fresh genuine saved-site origin.
                initial.status = record == traces_.end() ? control_error::invalid_state :
                    command.source_selected_finish ?
                        source_finish_position_locked(*record, command, initial, position, prepared_activation) :
                        source_step_position_locked(*record, position, prepared_activation);
                if(initial.status != control_error::none) { return initial; }
                auto const policy{command.source_policy == source_step_policy::into ? ::uwvm2::uwvm::debugger::source_step::policy::into :
                    command.source_policy == source_step_policy::over ? ::uwvm2::uwvm::debugger::source_step::policy::over :
                                                                      ::uwvm2::uwvm::debugger::source_step::policy::out};
                if(command.source_run_to != source_run_to_policy::none)
                {
                    initial.status = source_destination_locked(position,command,prepared_source_step);
                }
                else { initial.status = source_step_error(::uwvm2::uwvm::debugger::source_step::prepare(position, policy, prepared_source_step)); }
                if(initial.status != control_error::none) { return initial; }
            }
            if(next_request_ == 0u) { initial.status = control_error::exhausted; return initial; }
            wasm_script_commands script{};
            if(wasm_script)
            {
                script = parse_wasm_script(command);
                if(!script.valid) { initial.status = control_error::malformed; return initial; }
            }
            ::std::vector<::uwvm2::utils::control::wire_byte> replacement_payload{};
            ::fast_io::array<::uwvm2::utils::control::wire_byte, 24u> source_breakpoint_payload{};
            ::std::uint64_t source_breakpoint_function{}, source_breakpoint_offset{};
            auto const* payload_begin{command.payload.data()};
            ::std::size_t payload_size{command.payload_size};
            if(source_breakpoint)
            {
                ::std::uint64_t resolved_function{}, resolved_offset{};
                bool matched{}; unsigned best_row{};
                ::std::uint64_t best_line{(::std::numeric_limits<::std::uint64_t>::max)()};
                {
                    ::std::lock_guard lock{mutex_};
                    if(command.source_module >= source_modules_.size())
                    { initial.status = control_error::invalid_breakpoint_target; return initial; }
                    auto const& source{source_modules_[static_cast<::std::size_t>(command.source_module)]};
                    if(source.status == source_map_error::missing_debug_line)
                    { initial.status = control_error::source_debug_info_unavailable; return initial; }
                    if(source.status != source_map_error::none)
                    { initial.status = control_error::source_debug_info_invalid; return initial; }
                    if(source.invalidated.size() != source.functions.size())
                    { initial.status = control_error::source_debug_info_invalid; return initial; }
                    auto const requested_path{source_map_details::normalized_path({command.source_path.data(), command.source_path_size})};
                    bool saw_invalidated{};
                    for(::std::size_t function_index{}; function_index != source.functions.size(); ++function_index)
                    {
                        if(source.invalidated[function_index]) { saw_invalidated = true; continue; }
                        auto const& function{source.functions[function_index]};
                        auto const sites{::uwvm2::runtime::lib::llvm_jit_debug_safe_points_host_api(
                            command.source_module, function.function)};
                        if(!sites || sites.expression_size != function.expression_size)
                        { initial.status = control_error::source_debug_info_invalid; return initial; }
                        for(::std::size_t byte_index{}; byte_index != sites.byte_count; ++byte_index)
                        {
                            // [published bitmap bytes ...] bitmap_end
                            // [safe                       ] unsafe (one-past)
                            //  ^^ byte_index < byte_count, view remains borrowed only under this management operation.
                            auto const bits{sites.bits[byte_index]};
                            if(bits == 0u) { continue; }
                            for(unsigned bit{}; bit != 8u; ++bit)
                            {
                                if((bits & (1u << bit)) == 0u) { continue; }
                                if(byte_index > ((::std::numeric_limits<::std::uint64_t>::max)() - bit) / 8u)
                                { initial.status = control_error::source_debug_info_invalid; return initial; }
                                auto const offset{static_cast<::std::uint64_t>(byte_index) * 8u + bit};
                                if(offset >= sites.expression_size) { break; }
                                if(function.expression_begin > (::std::numeric_limits<::std::uint64_t>::max)() - offset)
                                { initial.status = control_error::source_debug_info_invalid; return initial; }
                                // [validated function expression] expression_end
                                // [safe                         ] unsafe (one-past)
                                //  ^^ checked scalar Code offset locates one emitted opcode without pointer arithmetic.
                                auto const mapped{source.lines.lookup(command.source_module, function.expression_begin + offset)};
                                if(mapped && mapped->line >= command.source_line && mapped->file == requested_path && mapped->is_statement && !mapped->epilogue_begin)
                                {
                                    auto const rank{mapped->prologue_end ? 2u : 1u};
                                    // Match an emitted statement on this line, or the
                                    // nearest following line when optimization erased it.
                                    if(mapped->line < best_line || (mapped->line == best_line && rank > best_row))
                                    { resolved_function = function.function; resolved_offset = offset; matched = true; best_row = rank; best_line = mapped->line; }
                                }
                            }
                        }
                    }
                    if(!matched)
                    {
                        initial.status = saw_invalidated ? control_error::source_debug_info_unavailable :
                                                               control_error::source_location_unmapped;
                        return initial;
                    }
                }
                ::uwvm2::utils::control::output_buffer source_output{source_breakpoint_payload};
                ::fast_io::io::print(source_output, ::fast_io::mnp::le_put<64>(command.source_module),
                    ::fast_io::mnp::le_put<64>(resolved_function), ::fast_io::mnp::le_put<64>(resolved_offset));
                // [24 complete owned payload bytes] payload_end
                // [safe                          ] unsafe (one-past)
                //  ^^ borrowed pointer remains live through the immediately encoded frame.
                payload_begin = source_breakpoint_payload.data();
                payload_size = source_breakpoint_payload.size();
                source_breakpoint_function = resolved_function;
                source_breakpoint_offset = resolved_offset;
            }
            if(replacement_file)
            {
                ::std::vector<::std::byte> body{};
                auto const source_status{read_replacement_source(command, body)};
                if(source_status != control_error::none) { initial.status = source_status; return initial; }
                payload_size = 32u + body.size();
                replacement_payload.resize(payload_size);
                ::uwvm2::utils::control::output_buffer body_header{replacement_payload.data(), replacement_payload.data() + 32u};
                ::fast_io::io::print(body_header,
                    ::fast_io::mnp::le_put<64>(command.replacement_module),
                    ::fast_io::mnp::le_put<64>(command.replacement_function),
                    ::fast_io::mnp::le_put<64>(command.replacement_generation),
                    ::fast_io::mnp::le_put<32>(static_cast<::std::uint32_t>(body.size())),
                    ::fast_io::mnp::le_put<32>(::std::uint32_t{}));
                for(::std::size_t index{}; index != body.size(); ++index)
                {
                    // [32-byte header][bounded owned body] end
                    // [safe                            ] index < body.size() <= 65536.
                    //                   ^^ only a body byte, never an untrusted pointer, is copied.
                    replacement_payload[32u + index] = ::std::to_integer<::uwvm2::utils::control::wire_byte>(body[index]);
                }
                payload_begin = replacement_payload.data();
            }
            ::std::vector<::uwvm2::utils::control::wire_byte> frame(::uwvm2::utils::control::header_bytes + payload_size);
            ::uwvm2::utils::control::output_buffer output{frame.data(), frame.data() + frame.size()};
            auto const encoded{::uwvm2::utils::control::encode_frame(
                {command.operation, static_cast<::std::uint32_t>(payload_size), config_.instance, config_.generation, next_request_},
                {payload_begin, payload_begin + payload_size}, output)};
            if(encoded.status != control_error::none) { initial.status = encoded.status; return initial; }
            auto received{session_.receive({frame.data(), frame.data() + encoded.written})};
            if(received.status != control_error::none || !received.request)
            { initial.status = received.status; return initial; }
            ++next_request_;
            auto const& validated{received.request->get()};
            auto completed{completion::inspected};
            control_error error{};
            bool timed_out{};
            ::std::uint64_t breakpoint_id{};
            ::std::uint_least64_t replacement_generation{};
            ::std::uintptr_t native_step_from{}, native_step_to{};
            native_next_policy::reason native_next_reason{native_next_policy::reason::none};
            native_disassembly::instruction native_instruction{};
            ::std::vector<::std::byte> memory_bytes{};
            auto const deadline{clock_type::now() + timeout};
            using namespace ::uwvm2::utils::control;
            if(wasm_script)
            {
                // Complete the outer authenticated status ticket before issuing
                // another request on this single-session stream. Every child
                // command independently authenticates and checks current state.
                auto const acknowledged{session_.complete(*received.request, completion::inspected)};
                if(acknowledged != control_error::none) { initial.status = acknowledged; return initial; }
                controller_reply result{};
                result.script_commands.reserve(script.size); result.script_replies.reserve(script.size);
                for(::std::size_t i{}; i != script.size; ++i)
                {
                    if(interrupt.pending()) { break; } // do not start another mutation after host cancellation
                    auto reply{execute(script.commands[i], timeout, interrupt)};
                    auto const status{reply.status};
                    // Typed mutation failures are reported in their own result,
                    // even when the authenticated control request succeeded.
                    bool const mutation_failed{script.commands[i].kind == console_command_kind::wasm_mutation &&
                        !reply.wasm_mutation_value.applied};
                    result.script_commands.push_back(script.commands[i]); result.script_replies.push_back(::std::move(reply));
                    if(status != control_error::none || mutation_failed) { break; }
                }
                auto const current{inspect()}; result.execution = current.execution;
                result.reason = current.reason; result.stop_identifier = current.stop_identifier;
                return result;
            }
            if(breakpoint_control)
            {
                // Existing authenticated status admission permits a bounded
                // host breakpoint policy update. No stop/read capability is minted.
                auto const acknowledged{session_.complete(*received.request, completion::inspected)};
                auto result{inspect()};
                if(acknowledged != control_error::none) { result.status = acknowledged; return result; }
                ::std::lock_guard lock{mutex_};
                if(exited_ || domain_->is_closed()) { result.status = control_error::invalid_state; return result; }
                bool found{};
                for(auto& point : breakpoints_)
                {
                    if(point.identifier == 0u || (command.breakpoint_id != 0u && point.identifier != command.breakpoint_id)) { continue; }
                    found = true;
                    if(command.breakpoint_policy == breakpoint_action::ignore) { point.ignore_remaining = command.breakpoint_ignore; }
                    else if(command.breakpoint_policy == breakpoint_action::condition)
                    { point.condition = command.breakpoint_condition; point.condition_size = command.breakpoint_condition_size; }
                    else { point.enabled = command.breakpoint_policy == breakpoint_action::enable; }
                }
                if(!found && command.breakpoint_id != 0u) { result.status = control_error::invalid_breakpoint_target; }
                return result;
            }
            if(wasm_event)
            {
                // No new wire capability: the existing authenticated status
                // ticket admits a host-owned, bounded diagnostic policy change.
                auto const acknowledged{session_.complete(*received.request, completion::inspected)};
                auto result{inspect()};
                if(acknowledged != control_error::none) { result.status = acknowledged; return result; }
                ::std::lock_guard lock{mutex_}; wasm_command_locked(command.wasm_request, result); return result;
            }
            if(command.wait_for_event)
            {
                // Explicit wait only observes. A separate trusted host Ctrl+C
                // flag may request cooperative pause; only actual park/capture
                // admits stopped, and timeout never supplies stop authority.
                timed_out = !wait_for_stop(deadline, interrupt);
            }
            else if(::std::holds_alternative<pause_command>(validated))
            {
                { ::std::lock_guard lock{mutex_}; if(!pause_) { pause_ = request_pause_locked(); reason_ = stop_reason::requested; }
                  changed_.notify_all(); }
                if(wait_for_stop(deadline, interrupt)) { completed = completion::paused; }
                else
                {
                    ::std::lock_guard lock{mutex_};
                    // Timeout is rejection, never proof of a complete stop.
                    // Keep an observed Ctrl+C request pending for the actual
                    // participant still inside its uncooperative host call.
                    // A normal pause timeout retains the existing resume policy.
                    if(interrupt.pending())
                    {
                        cancel_step_locked(); source_step_error_ = control_error::none;
                        if(!pause_ && !exited_ && !domain_->is_closed())
                        { pause_ = request_pause_locked(); reason_ = stop_reason::requested; }
                    }
                    else { static_cast<void>(resume_locked()); }
                    timed_out = true; completed = completion::rejected;
                }
            }
            else if(::std::holds_alternative<resume_command>(validated))
            {
                ::std::lock_guard lock{mutex_};
                cancel_step_locked();
                if(resume_locked()) { completed = completion::resumed; }
                else { completed = completion::rejected; error = control_error::invalid_state; }
            }
            else if(assembly_step)
            {
                auto const* step{::std::get_if<step_command>(::std::addressof(validated))};
                if(step == nullptr) { completed = completion::rejected; error = control_error::malformed; }
                else
                {
                    auto const outcome{assembly_finish ? execute_native_finish(step->thread,timeout,interrupt) :
                        execute_native_step(step->thread, initial, timeout, ordinary_next, interrupt)};
                    completed = outcome.completed; error = outcome.error;
                    timed_out = outcome.timed_out;
                    native_step_from = outcome.from; native_step_to = outcome.to;
                    native_instruction = outcome.decoded; native_next_reason = outcome.next_reason;
                }
            }
            else if(auto const* step = ::std::get_if<step_command>(::std::addressof(validated)))
            {
                { ::std::lock_guard lock{mutex_}; step_thread_ = step->thread;
                  source_step_active_ = source_step;
                  wasm_step_active_ = wasm_step; wasm_step_policy_ = command.wasm_policy;
                  source_step_stop_count_ = 0u;
                  source_step_error_ = control_error::none;
                  source_step_origin_ = ::std::move(prepared_source_step);
                  source_step_origin_activation_ = ::std::move(prepared_activation);
                  if(!resume_locked()) { step_thread_ = 0u; source_step_active_ = false; wasm_step_active_ = false; error = control_error::invalid_state; } }
                if(error == control_error::none && wait_for_stop(deadline, interrupt))
                { completed = interrupt.pending() ? completion::paused : completion::stepped; ::std::lock_guard lock{mutex_}; error = source_step_error_; }
                else
                {
                    ::std::lock_guard lock{mutex_};
                    if(interrupt.pending())
                    {
                        // The selected source/Wasm step must not auto-resume
                        // after cancellation, even when its participant cannot
                        // park before deadline. Preserve/create the REAL domain
                        // ticket; snapshot() still reports stopping until actual
                        // all-participant capture proves a complete pause.
                        cancel_step_locked(); source_step_error_ = control_error::none;
                        if(!pause_ && !exited_ && !domain_->is_closed())
                        { pause_ = request_pause_locked(); reason_ = stop_reason::requested; }
                    }
                    else
                    { step_thread_ = 0u; source_step_active_ = false; wasm_step_active_ = false; static_cast<void>(resume_locked()); }
                    timed_out = error == control_error::none; completed = completion::rejected;
                }
            }
            else if(auto const* point = ::std::get_if<breakpoint_set_command>(::std::addressof(validated)))
            {
                ::std::lock_guard lock{mutex_};
                // The runtime view is borrowed only during this serialized
                // host operation. It refers to the currently published code
                // generation; a successful replacement purges old sites.
                auto const sites{::uwvm2::runtime::lib::llvm_jit_debug_safe_points_host_api(point->module, point->function)};
                completed = completion::rejected;
                if(!sites) { error = control_error::invalid_breakpoint_target; }
                else if(!sites.contains(point->offset)) { error = control_error::breakpoint_not_executable; }
                else
                {
                    error = control_error::exhausted;
                    for(auto& slot : breakpoints_)
                    {
                        if(slot.identifier != 0u && slot.module == point->module && slot.function == point->function && slot.offset == point->offset)
                        {
                            if(command.breakpoint_initial_ignore)
                            { slot.hits = 0u; slot.ignore_remaining = command.breakpoint_ignore; slot.enabled = true; }
                            if(command.breakpoint_initial_condition)
                            { slot.condition = command.breakpoint_condition; slot.condition_size = command.breakpoint_condition_size; }
                            breakpoint_id = slot.identifier; error = control_error::none; completed = completion::configured; break;
                        }
                    }
                    for(auto& slot : breakpoints_)
                    {
                        if(breakpoint_id != 0u) { break; }
                        if(slot.identifier == 0u && next_breakpoint_ != 0u)
                        { slot = {next_breakpoint_++, point->module, point->function, point->offset};
                          slot.ignore_remaining = command.breakpoint_initial_ignore ? command.breakpoint_ignore : 0u;
                          if(command.breakpoint_initial_condition)
                          { slot.condition = command.breakpoint_condition; slot.condition_size = command.breakpoint_condition_size; }
                          breakpoint_id = slot.identifier;
                          error = control_error::none; completed = completion::configured; break; }
                    }
                }
            }
            else if(auto const* point = ::std::get_if<breakpoint_clear_command>(::std::addressof(validated)))
            {
                ::std::lock_guard lock{mutex_};
                completed = completion::rejected; error = control_error::invalid_state;
                for(auto& slot : breakpoints_) { if(slot.identifier == point->identifier)
                    { slot = {}; error = control_error::none; completed = completion::configured; break; } }
            }
            else if(auto const* request = ::std::get_if<read_memory_command>(::std::addressof(validated)))
            {
                // session_ has checked the 32-byte payload, overflow and 64 KiB
                // bound. snapshot() confirmed all admitted guest threads parked.
                // The runtime performs a fresh memory-size check while copying.
                memory_bytes.resize(request->length);
                if(read_memory_ == nullptr || !read_memory_(request->module, request->memory,
                    request->offset, memory_bytes.data(), memory_bytes.size()))
                { memory_bytes.clear(); completed = completion::rejected; error = control_error::invalid_memory_range; }
            }
            else if(auto const* request = ::std::get_if<replace_function_command>(::std::addressof(validated)))
            {
                // Compilation is private while the guest remains stopped. The
                // runtime must not publish before every parked stack is checked.
                auto const prepared{::uwvm2::runtime::lib::llvm_jit_debug_prepare_function_replacement_host_api(
                    request->module, request->function, request->expected_generation,
                    request->body.data(), request->body.size())};
                auto replacement_result{::uwvm2::runtime::lib::llvm_jit_debug_replace_result{prepared.status, prepared.generation}};
                using enum ::uwvm2::runtime::lib::llvm_jit_debug_replace_status;
                bool active_target_frame{};
                bool complete_stopped_trace{true};
                bool complete_pause{};
                if(prepared.status == replaced && prepared.transaction != nullptr)
                {
                    {
                        // The controller lock precedes the domain lock, matching
                        // observer order. No observer can replace a parked trace
                        // before this pause is resumed by the management thread.
                        ::std::lock_guard lock{mutex_};
                        auto const stopped{domain_->capture(pause_)};
                        complete_pause = stopped.result == ::uwvm2::utils::thread::cooperative_pause_result::paused;
                        if(complete_pause)
                        {
                            for(auto const& member : stopped.participants)
                            {
                                auto const found{::std::find_if(traces_.begin(), traces_.end(),
                                    [&](auto const& record) { return record.participant == member.id; })};
                                ::uwvm2::runtime::lib::llvm_jit_debug_activation_snapshot actual{};
                                if(found == traces_.end() || !found->activation_capture ||
                                   !::uwvm2::runtime::lib::llvm_jit_debug_query_activation_host_api(found->activation_capture, actual) ||
                                   actual.participant != member.id || actual.frames.empty())
                                { complete_stopped_trace = false; break; }
                                for(auto const& frame : actual.frames)
                                {
                                    if(frame.module == request->module && frame.function == request->function)
                                    { active_target_frame = true; break; }
                                }
                                if(active_target_frame) { break; }
                            }
                            if(complete_stopped_trace && !active_target_frame)
                            {
                                // Keep the complete stopped set stable while the
                                // runtime publishes the precompiled entry target.
                                // Publication is bounded: this callback must
                                // not compile, wait, or re-enter the pause domain.
                                complete_pause = domain_->while_stopped(pause_, [&]
                                {
                                    replacement_result = ::uwvm2::runtime::lib::llvm_jit_debug_commit_function_replacement_host_api(
                                        prepared.transaction);
                                });
                            }
                        }
                    }
                    ::uwvm2::runtime::lib::llvm_jit_debug_discard_function_replacement_host_api(prepared.transaction);
                }
                else if(prepared.transaction != nullptr)
                { ::uwvm2::runtime::lib::llvm_jit_debug_discard_function_replacement_host_api(prepared.transaction); }
                if(prepared.status == replaced)
                {
                    if(!complete_pause || !complete_stopped_trace) { replacement_result.status = invalid_state; }
                    else if(active_target_frame) { replacement_result.status = invalid_state; }
                }
                completed = completion::rejected;
                switch(replacement_result.status)
                {
                    case replaced:
                        completed = completion::replaced; replacement_generation = replacement_result.generation;
                        { ::std::lock_guard lock{mutex_};
                          advance_stop_identifier_locked();
                          // A numeric or source line site from the old body can
                          // name unrelated bytes in the new generation. Remove
                          // it before the still-paused guest may resume.
                          for(auto& slot : breakpoints_)
                          {
                              if(slot.identifier != 0u && slot.module == request->module && slot.function == request->function)
                              { slot = {}; }
                          }
                          if(request->module < source_modules_.size())
                          {
                              auto& source{source_modules_[static_cast<::std::size_t>(request->module)]};
                              auto const found{::std::lower_bound(source.functions.begin(), source.functions.end(), request->function,
                                  [](source_function_span const& entry, ::std::uint64_t id) { return entry.function < id; })};
                              if(found != source.functions.end() && found->function == request->function)
                              {
                                  auto const index{static_cast<::std::size_t>(found - source.functions.begin())};
                                  source.invalidated[index] = true;
                                  if(index < source.wasm_functions.size())
                                  {
                                      auto& instruction_source{source.wasm_functions[index]};
                                      instruction_source.expression_bytes.clear(); instruction_source.instruction_safe_point_bits.clear();
                                      instruction_source.instruction_safe_points_complete = false; instruction_source.function_generation = 0u;
                                      // The authenticated payload was actually compiled and
                                      // committed with exact ABI. Query its newly published
                                      // safe-point metadata while every guest remains parked.
                                      // Keep old DWARF invalid; only Wasm opcode bytes refresh.
                                      auto const actual{::uwvm2::runtime::lib::llvm_jit_debug_safe_points_host_api(request->module, request->function)};
                                      if(actual && actual.function_generation == replacement_result.generation &&
                                         actual.expression_size != 0u && actual.expression_size <= request->body.size() &&
                                         replacement_payload.size() == 32u + request->body.size())
                                      {
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
                                          try
                                          {
#endif
                                          auto const offset{replacement_payload.size() - actual.expression_size};
                                          instruction_source.expression_bytes.resize(actual.expression_size);
                                          for(::std::size_t byte{}; byte != actual.expression_size; ++byte)
                                          {
                                              // [32-byte header][validated locals][new expression ...] end
                                              // [safe                                                ] unsafe (one-past)
                                              //                                       ^^ offset+byte < payload.size();
                                              // copied values are owned bytes, never decoded guest pointers.
                                              instruction_source.expression_bytes[byte] = static_cast<::std::byte>(replacement_payload[offset + byte]);
                                          }
                                          instruction_source.expression_size = actual.expression_size;
                                          instruction_source.function_generation = replacement_result.generation;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
                                          }
                                          catch(...)
                                          { instruction_source.expression_bytes.clear(); instruction_source.function_generation = 0u; }
#endif
                                      }
                                  }
                              }
                          } }
                        break;
                    case unsupported_mode: error = control_error::unsupported_command; break;
                    case invalid_state: error = control_error::invalid_state; break;
                    case invalid_target: error = control_error::invalid_replacement_target; break;
                    case stale_generation: error = control_error::stale_function_generation; break;
                    case invalid_body: error = control_error::invalid_function_body; break;
                    case abi_mismatch: error = control_error::function_abi_mismatch; break;
                    case compile_failure: error = control_error::replacement_compile_failure; break;
                    case exhausted: error = control_error::exhausted; break;
                }
                if(active_target_frame) { error = control_error::active_replacement_frame; }
            }
            auto const acknowledged{session_.complete(*received.request, completed)};
            auto result{inspect()};
            result.status = error != control_error::none ? error : acknowledged;
            result.timed_out = timed_out;
            result.breakpoint_identifier = breakpoint_id;
            result.source_breakpoint_function = source_breakpoint_function;
            result.source_breakpoint_offset = source_breakpoint_offset;
            result.replacement_generation = replacement_generation;
            result.native_next_reason = native_next_reason;
            result.native_step_from = native_step_from;
            result.native_step_to = native_step_to;
            result.native_instruction = native_instruction; // only compiler-qualified Wasm bytes survived the private projection
            result.memory = ::std::move(memory_bytes);
            if(::std::holds_alternative<breakpoint_list_command>(validated))
            { ::std::lock_guard lock{mutex_}; for(auto const& slot : breakpoints_) { if(slot.identifier != 0u) { result.breakpoints.push_back(slot); } } }
            if(source_frame)
            {
                ::std::lock_guard lock{mutex_}; bool found{};
                for(auto const& record : traces_)
                { if(record.participant == command.requested_step_thread) { source_frames_locked(record, command, result); found = true; break; } }
                if(!found) { result.status = control_error::invalid_state; }
            }
            // A source-frame query now inspects actual saved callers using the
            // COMPLETE current cohort. Backtrace thread selection is display
            // filtering only and must happen AFTER that coherent read.
            if(auto const* request = ::std::get_if<backtrace_command>(::std::addressof(validated)))
            {
                ::std::erase_if(result.threads, [&](auto const& thread) { return thread.identifier != request->thread; });
#if defined(__linux__) && (defined(__x86_64__) || UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE)
                if(command.kind == console_command_kind::protocol)
                {
                    ::std::lock_guard lock{mutex_};
                    if(native_owned_ && native_external_parked_ && native_participant_ == request->thread &&
                       native_activation_cursor_ && stop_identifier_ != 0u && result.stop_identifier == stop_identifier_)
                    {
                        if(command.disassembly_stop_identifier != 0u && command.disassembly_stop_identifier != stop_identifier_)
                        { result.status = control_error::unsupported_command; }
                        else
                        {
                            result.native_caller_stop_identifier = stop_identifier_;
                            ::uwvm2::runtime::lib::llvm_jit_debug_native_backtrace_view trace{};
                            if(::uwvm2::runtime::lib::llvm_jit_debug_native_backtrace_host_api(native_activation_cursor_,
                                ::std::addressof(native_session_), trace))
                            { result.native_backtrace = trace; result.native_caller = trace.frames[0u]; }
                        }
                    }
                }
#endif
            }
            if(disassembly)
            { ::std::lock_guard lock{mutex_}; if(disassembly_range) { disassemble_range_locked(command, result); }
              else { disassemble_locked(command, result); } }
            if(registers) { ::std::lock_guard lock{mutex_}; registers_locked(command, result); }
            if(auto const* request = ::std::get_if<locals_command>(::std::addressof(validated)))
            {
                // Source pointer inspection requires the complete actual current
                // roster for runtime cohort authentication. Filter display DATA
                // only AFTER this bounded selected-thread query has completed.
                ::std::lock_guard lock{mutex_};
                if(source_locals) { result.source_stop_identifier = pause_ ? stop_identifier_ : 0u; }
                for(auto const& record : traces_)
                {
                    if(record.participant != request->thread) { continue; }
                    if(source_type) { source_type_locked(record, command, result); break; }
                    if(source_locals) { source_locals_locked(record, command, result); break; }
                    if(!record.locals_available) { continue; }
                    result.total_local_count = record.total_count;
                    result.locals_available = true;
                    result.locals.reserve(record.captured_count);
                    for(::std::size_t i{}; i != record.captured_count; ++i) { result.locals.push_back(record.locals[i]); }
                    break;
                }
                ::std::erase_if(result.threads, [&](auto const& thread) { return thread.identifier != request->thread; });
            }
            if(wasip1_state_query && result.status == control_error::none)
            { ::std::lock_guard lock{mutex_}; query_wasip1_state_locked(command.wasip1_state_request, result, command.disassembly_stop_identifier); }
            if(wasm_path_query && result.status==control_error::none)
            { ::std::lock_guard lock{mutex_};query_wasm_path_locked(command.wasm_path_request,result); }
            if(wasm_mutation_query && result.status==control_error::none)
            { ::std::lock_guard lock{mutex_};mutate_wasm_state_locked(command.wasm_mutation_request,result); }
            if(wasm_state_query && result.status == control_error::none)
            { ::std::lock_guard lock{mutex_}; query_wasm_state_locked(command.wasm_state_request, result); }
            if(source_type && !result.source_object_type_available && !result.source_object_value_available &&
               !result.source_locals_available && result.status == control_error::none)
            { result.status = control_error::source_location_unmapped; }
            return result;
        }
    };
#endif
}
#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/pop_macros.h>
#endif
