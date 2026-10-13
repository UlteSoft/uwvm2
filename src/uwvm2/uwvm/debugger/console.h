/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/push_macros.h>
# if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
# include <fast_io_dsal/array.h>
# include <algorithm>
# include <bit>
# include <cstring>
# include <string>
# include <memory>
# include <cstdint>
# include <limits>
# include <chrono>
# include <utility>
# include <vector>
# include <fast_io_dsal/string_view.h>
# include <fast_io_dsal/string.h>
# include <fast_io.h>
# include <fast_io_unit/string.h>
# include "command.h"
# include "controller.h"
# include "managed_cli_shutdown.h"
# include "console_line_editor.h"
# include "console_help.h"
# include "console_keyboard.h"
# include "console_aliases.h"
# include "console_execution.h"
# include "console_completion.h"
# include "console_displays.h"
# include "console_tui.h"
# include "console_messages.h"
# include "console_module_help.h"
# include "wasm_state.h"
# include "wasm_mutation.h"
# include "wasip1_state.h"
# include <uwvm2/runtime/lib/uwvm_runtime.h>
# endif
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger
{
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    // shutdown_pending is only a retained HOST embedding result. The original
    // issuer must keep its helper, guest thread, controller and all VM owners
    // alive and retry later; it is never permission to destroy those owners.
    enum class console_exit { terminate_process, input_failure, managed_exit, shutdown_pending, restart };
    struct console_restart_request
    {
        bool pending{}, resume_new_launch{}, background{}, killed{};
        ::std::unique_ptr<console_tui::screen> screen{};
        ::std::unique_ptr<console_editing::editor> editor{};
        console_launch_action action{console_launch_action::run};
        ::fast_io::string program_name{}; // Owned host display label, never guest/launch authority.
        ::std::vector<breakpoint> breakpoints{}; // Cold settings, revalidated in the new instance.
    };
    struct console_io
    {
        // HOST-ONLY callbacks. read_byte returns 0..255, -1 EOF, -2 failure,
        // -3 canceled host input or a qualified OS keyboard interrupt. Native
        // key codes 256..260 are bounded editor controls, never VM operations.
        // Their context stays alive for the synchronous console call. Production
        // must isolate this input from every guest descriptor/preopen/import.
        void* context{};
        int (*read_byte)(void*) noexcept{};
        void (*write)(void*, ::fast_io::string_view) noexcept{};
        bool interactive{}; // native adapter alone enables verified terminal cursor controls
        bool (*interrupt_pending)(void*) noexcept{}; // synchronous host flag query, never a signal/controller callback
        bool foreground_execution{}; // HOST input policy, independent of terminal display/output
        bool (*consume_interrupt)(void*) noexcept{}; // normal management acknowledgement of a delivered HOST event
        void* display_context{};
        void (*display_poll)(void*){};
        bool (*input_ready)(void*) noexcept{}; // Nonblocking host queue observation for mouse batching.
    };
    enum class line_status { complete, end, oversized, failure, interrupted };
    struct console_line
    {
        ::fast_io::array<char, max_input_command_bytes> bytes{};
        ::std::size_t size{};
        line_status status{line_status::complete};
        bool physical_input_eof{}; // actual provider -1; terminal CtrlD is a distinct editor event
    };
    [[nodiscard]] inline console_line read_console_line(console_io io) noexcept
    {
        console_line line;
        bool oversized{};
        for(;;)
        {
            auto const ch{io.read_byte(io.context)};
            if(ch < -1 || ch > 255) { line.status = line_status::failure; return line; }
            if(ch == -1 || ch == '\n')
            {
                line.physical_input_eof = ch == -1;
                line.status = oversized ? line_status::oversized : ch == -1 && line.size == 0u ? line_status::end : line_status::complete;
                return line;
            }
            if(line.size == line.bytes.size()) { oversized = true; }
            else { line.bytes[line.size++] = static_cast<char>(ch); }
            // After overflow, drain to this line's delimiter without storing any
            // suffix as a new command. An oversized prefix can never execute.
        }
    }
    struct console_read_hooks
    {
        void* context{};
        console_editing::action (*complete)(void*, console_editing::editor&){};
        bool (*draw)(void*, console_editing::editor const&, console_editing::action){};
        void (*candidates)(void*,console_editing::editor const&,bool){};
        bool (*input)(void*,console_editing::editor&,int){};
    };
    [[nodiscard]] inline console_line read_console_line(console_io io, console_editing::editor& editor,
        console_read_hooks hooks = {})
    {
        static_assert(console_editing::maximum_bytes == max_input_command_bytes);
        editor.begin();
        bool const full_screen{hooks.draw != nullptr && hooks.draw(hooks.context, editor, console_editing::action::unchanged)};
        if(io.interactive && !full_screen)
        {
            auto const row{console_tui::command_input(editor, console_tui::terminal_dimensions().columns)};
            io.write(io.context, row.text.subview(0u));
        }
        for(;;)
        {
            auto const input_byte{io.read_byte(io.context)};
            bool const consumed{hooks.input!=nullptr && hooks.input(hooks.context,editor,input_byte)};
            auto action{consumed ? console_editing::action::changed : editor.feed(input_byte)};
            bool dynamic_candidates{};
            if(action == console_editing::action::completion)
            { action = hooks.complete == nullptr ? console_editing::action::unchanged : hooks.complete(hooks.context, editor);
              dynamic_candidates = action == console_editing::action::candidates; }
            bool const handled_candidates{action==console_editing::action::candidates && hooks.candidates!=nullptr && io.interactive};
            if(handled_candidates) { hooks.candidates(hooks.context,editor,dynamic_candidates); }
            bool const drawn{hooks.draw != nullptr && hooks.draw(hooks.context, editor, action)};
            if(io.interactive && !drawn && (action == console_editing::action::changed ||
                action == console_editing::action::redraw || action == console_editing::action::candidates))
            {
                if(action == console_editing::action::redraw) { io.write(io.context, "\x1b[2J\x1b[H"); }
                if(action == console_editing::action::candidates && !handled_candidates)
                {
                    io.write(io.context, "\n");
                    editor.display_candidates([&](::fast_io::string_view name)
                    { io.write(io.context, name); io.write(io.context, "\n"); });
                }
                auto const rendered{console_tui::command_line(editor, console_tui::terminal_dimensions().columns)};
                io.write(io.context, ::fast_io::string_view{rendered.data(), rendered.size()});
            }
            if(action == console_editing::action::changed || action == console_editing::action::unchanged || action==console_editing::action::idle ||
                action == console_editing::action::redraw || action == console_editing::action::candidates ||
                action == console_editing::action::tui_toggle || action == console_editing::action::tui_single ||
                action == console_editing::action::tui_split || action == console_editing::action::page_up ||
                action == console_editing::action::page_down || action == console_editing::action::mouse) { continue; }
            console_line line{};
            line.physical_input_eof = input_byte == -1;
            switch(action)
            {
                case console_editing::action::complete:
                    line.size = editor.line().size;
                    // [owned editor bytes][size <= max_input_command_bytes] end
                    // [safe                              ] copied into equal fixed extent.
                    for(::std::size_t index{}; index != line.size; ++index) { line.bytes[index] = editor.line().bytes[index]; }
                    if(io.interactive && !drawn) { io.write(io.context, "\n"); }
                    return line;
                case console_editing::action::end: line.status = line_status::end; return line;
                case console_editing::action::interrupted: line.status = line_status::interrupted; return line;
                case console_editing::action::oversized: line.status = line_status::oversized; return line;
                default: line.status = line_status::failure; return line;
            }
        }
    }
    namespace details
    {
        inline constexpr ::std::size_t maximum_formatted_reply_bytes{65536u};
        inline constexpr ::std::size_t reply_trailer_reserve{256u};
        inline void add_name(::std::string& text, ::std::u8string const& name)
        {
            ::fast_io::ostring_ref_std output{__builtin_addressof(text)};
            for(auto const byte : name)
            {
                auto const c{static_cast<unsigned char>(byte)};
                // Guest-provided names cannot emit terminal control sequences.
                if(::fast_io::char_category::is_c_cntrl(c))
                { ::fast_io::io::print(output, "\\x", ::fast_io::mnp::hex<false, true>(c)); }
                else { ::fast_io::io::print(output, ::fast_io::mnp::chvw(static_cast<char>(c))); }
            }
        }
        inline void add_source_path(::std::string& text, ::std::string_view path)
        {
            ::fast_io::ostring_ref_std output{__builtin_addressof(text)};
            for(auto const byte : path)
            {
                auto const c{static_cast<unsigned char>(byte)};
                if(::fast_io::char_category::is_c_cntrl(c))
                { ::fast_io::io::print(output, "\\x", ::fast_io::mnp::hex<false, true>(c)); }
                else { ::fast_io::io::print(output, ::fast_io::mnp::chvw(byte)); }
            }
        }
        [[nodiscard]] constexpr ::fast_io::string_view native_next_unavailable_message(native_next_policy::reason why) noexcept
        {
            using enum native_next_policy::reason;
            switch(why)
            {
                case current_native_trap_required:
                    return "a genuine current native top-frame trap is required; first use step asm THREAD";
                case call_continuation_unavailable: return "a qualified call continuation is unavailable; current stop retained";
                case call_target_unavailable: return "the current call has no proved Wasm typed target; current stop retained";
                case call_continuation_event_unavailable: return "the precise owned continuation event is unavailable; current stop retained";
                case call_continuation_capacity_exhausted: return "native continuation owner capacity is exhausted; use step wasm THREAD to retire this session; current stop retained";
                case return_continuation_event_unavailable: return "the precise owned parent return event is unavailable; current stop retained";
                case return_continuation_capacity_exhausted: return "native return owner capacity is exhausted; retire this native session before another finish; current stop retained";
                case caller_unwind_unavailable: return "native caller/unwind continuation is unavailable; current stop retained";
                case branch_continuation_unavailable: return "branch target continuation is unavailable; current stop retained";
                case trap_instruction: return "trap instruction is not an ordinary native next; current stop retained";
                case other_control_flow: return "control/system/prefix instruction is not supported; current stop retained";
                case inconsistent_descriptor: return "instruction is not a qualified ordinary fallthrough; current stop retained";
                case decoder_disagreement: return "native decoders disagree on the complete instruction length; current stop retained";
                case incomplete_decode: return "complete conservative native decode is unavailable; current stop retained";
                default: return "actual stopped code owner or qualified native backend is unavailable";
            }
        }
        [[nodiscard]] inline ::std::string format_reply(controller_reply const& reply, console_command const& command)
        {
            ::std::string text;
            ::fast_io::ostring_ref_std output{__builtin_addressof(text)};
            if(command.kind==console_command_kind::execution_relocate)
            {
                using result=::uwvm2::runtime::lib::llvm_jit_debug_relocation_status;
                if(reply.relocation_status==result::moved)
                { return ::fast_io::concat_std("Execution moved to ",::fast_io::mnp::hex0x(reply.native_step_to),"; still stopped.\n"); }
                auto reason{reply.relocation_status==result::invalid_target ? "target is outside the current JIT function or is not an instruction boundary" :
                    reply.relocation_status==result::incompatible_frame ? "target crosses a call, branch or incompatible native frame state" :
                    reply.relocation_status==result::restore_state_unavailable ? "target requires captured operand/control stacks and a resumable JIT landing" :
                    reply.relocation_status==result::current_stop_required ? "requires an exact => stop at the requested level; N> cannot be moved" : "native context relocation is unavailable"};
                return ::fast_io::concat_std("error: jump unavailable: ",::fast_io::mnp::os_c_str(reason),"; current stop retained\n");
            }
            if(reply.source_frame_out_of_range)
            { return ::fast_io::concat_std("error: frame ordinal is outside the current authenticated frame list; selection unchanged\n"); }
            if(reply.source_frame_caller_unavailable)
            { return ::fast_io::concat_std("error: source variables unavailable: selected caller has no authenticated captured locals or caller source PC\n"); }
            if(reply.source_frame_step_unavailable)
            { return ::fast_io::concat_std("error: selected outer-frame source stepping unavailable; select frame 0 before source step; current stop retained\n"); }
            if(command.kind == console_command_kind::source_frame &&
               (reply.status == ::uwvm2::utils::control::error::source_debug_info_unavailable ||
                reply.status == ::uwvm2::utils::control::error::source_debug_info_invalid ||
                reply.status == ::uwvm2::utils::control::error::source_location_unmapped))
            { return ::fast_io::concat_std("error: current source frames unavailable: valid embedded -g DWARF and a current private source/activation capture are required\n"); }
            if(reply.status != ::uwvm2::utils::control::error::none)
            {
                ::fast_io::io::print(output, "error: ");
                switch(reply.status)
                {
                    case ::uwvm2::utils::control::error::invalid_state: ::fast_io::io::print(output, "command or thread is not valid in the current execution state"); break;
                    case ::uwvm2::utils::control::error::invalid_memory_range: ::fast_io::io::print(output, "memory module, index or byte range unavailable"); break;
                    case ::uwvm2::utils::control::error::invalid_replacement_source: ::fast_io::io::print(output, "replacement body must be a stable regular file of 1..65536 bytes"); break;
                    case ::uwvm2::utils::control::error::invalid_replacement_target: ::fast_io::io::print(output, "replacement module or defined function unavailable"); break;
                    case ::uwvm2::utils::control::error::stale_function_generation: ::fast_io::io::print(output, "function generation changed; inspect again before replacing"); break;
                    case ::uwvm2::utils::control::error::invalid_function_body: ::fast_io::io::print(output, "replacement body failed WebAssembly validation"); break;
                    case ::uwvm2::utils::control::error::function_abi_mismatch: ::fast_io::io::print(output, "replacement function ABI differs from the published function"); break;
                    case ::uwvm2::utils::control::error::replacement_compile_failure: ::fast_io::io::print(output, "replacement native compilation failed; old function remains active"); break;
                    case ::uwvm2::utils::control::error::active_replacement_frame: ::fast_io::io::print(output, "replacement target is active on a stopped Wasm stack; resume past it before replacing"); break;
                    case ::uwvm2::utils::control::error::source_debug_info_unavailable: ::fast_io::io::print(output, "source stepping unavailable: this module lacks -g DWARF mapping or its function was replaced"); break;
                    case ::uwvm2::utils::control::error::source_debug_info_invalid: ::fast_io::io::print(output, "source stepping unavailable: DWARF or Code section boundary is invalid"); break;
                    case ::uwvm2::utils::control::error::source_location_unmapped:
                        if(command.source_run_to != source_run_to_policy::none)
                        { ::fast_io::io::print(output, "source target has no emitted statement in the permitted module/frame"); }
                        else if(command.kind == console_command_kind::source_value)
                        { ::fast_io::io::print(output, "source value unavailable: current captured location, layout or single unshared local-memory read is unavailable"); }
                        else { ::fast_io::io::print(output, "source stepping unavailable: stopped Wasm instruction has no source line"); }
                        break;
                    case ::uwvm2::utils::control::error::source_step_policy_unavailable: ::fast_io::io::print(output, "source stepping unavailable: actual full-JIT activation or current source mapping is unavailable"); break;
                    case ::uwvm2::utils::control::error::wasm_step_policy_unavailable: ::fast_io::io::print(output, "Wasm stepping unavailable: current complete full-JIT activation chain is unavailable"); break;
                    case ::uwvm2::utils::control::error::invalid_breakpoint_target: ::fast_io::io::print(output, "breakpoint target is not a defined LLVM-full Wasm function"); break;
                    case ::uwvm2::utils::control::error::breakpoint_not_executable: ::fast_io::io::print(output, "Wasm byte offset has no emitted executable debug safe point"); break;
                    case ::uwvm2::utils::control::error::exhausted: ::fast_io::io::print(output, "bounded controller capacity exhausted"); break;
                    case ::uwvm2::utils::control::error::unsupported_command:
                        if(command.kind == console_command_kind::assembly_step &&
                           reply.native_next_reason != native_next_policy::reason::none)
                        { ::fast_io::io::print(output, "native step unavailable: ", native_next_unavailable_message(reply.native_next_reason)); break; }
                        if(command.kind == console_command_kind::assembly_finish)
                        { ::fast_io::io::print(output, "native finish unavailable: ", native_next_unavailable_message(reply.native_next_reason)); break; }
                        if(command.kind == console_command_kind::assembly_next)
                        { ::fast_io::io::print(output, "native next unavailable: ", native_next_unavailable_message(reply.native_next_reason)); break; }
                        if(command.kind == console_command_kind::assembly_disassemble ||
                           command.kind == console_command_kind::assembly_disassemble_range)
                        { ::fast_io::io::print(output, "native disassembly unavailable: current private stop, live code owner or qualified native decoder is unavailable"); break; }
                        ::fast_io::io::print(output, ::fast_io::mnp::os_c_str(command.kind == console_command_kind::assembly_step ?
                            "assembly stepping unavailable: this host has no qualified LLVM-full native single-step backend" :
                            "unsupported command"));
                        break;
                    default:
                        ::fast_io::io::print(output, "control request rejected (code ", ::fast_io::mnp::dec(static_cast<unsigned>(reply.status)), ")");
                        break;
                }
                ::fast_io::io::print(output, ::fast_io::mnp::chvw('\n')); return text;
            }
            if(command.kind == console_command_kind::source_frame)
            {
                if(!reply.source_frames_available || reply.source_stop_identifier == 0u || reply.source_frame_thread == 0u ||
                   reply.source_frames.size() > 128u || reply.source_frame_total == 0u || reply.selected_source_frame >= reply.source_frame_total ||
                   reply.source_frame_first > reply.source_frame_total || reply.source_frames.size() > reply.source_frame_total - reply.source_frame_first)
                { return ::fast_io::concat_std("error: current source frame unavailable\n"); }
                ::fast_io::io::print(output, command.wasm_frames ? ::fast_io::string_view{"wasm-frames stop="} : ::fast_io::string_view{"source-frames stop="}, ::fast_io::mnp::dec(reply.source_stop_identifier),
                    " thread=", ::fast_io::mnp::dec(reply.source_frame_thread), " selected=", ::fast_io::mnp::dec(reply.selected_source_frame),
                    " count=", ::fast_io::mnp::dec(reply.source_frames.size()));
                if(command.frame_page_explicit || reply.source_frame_total > 128u)
                { ::fast_io::io::print(output, " total=", ::fast_io::mnp::dec(reply.source_frame_total), " first=", ::fast_io::mnp::dec(reply.source_frame_first),
                    " physical=", ::fast_io::mnp::dec(reply.source_frame_physical)); }
                ::fast_io::io::print(output, "\n");
                // A row has <=252 fixed/numeric bytes plus <=56*4 escaped
                // name bytes and <=17 truncation bytes. Header/end <=109 bytes:
                // 128*493+109 == 63213 < the broker's 65536-byte reply cap.
                // This bound is proved BEFORE producing any row; names cannot
                // force a valid activation list into a truncated wire response.
                for(::std::size_t i{}; i != reply.source_frames.size(); ++i)
                {
                    // [owned bounded frame views ... i ... end]
                    // [safe                                  ] i < size BEFORE immutable borrow.
                    auto const& frame{reply.source_frames[i]};
                    ::fast_io::io::print(output, "  frame ", ::fast_io::mnp::dec(reply.source_frame_first + i), " kind=",
                        ::fast_io::mnp::code_cvt(frame.kind == source_frame_kind::inline_scope ? ::std::string_view{"inline"} :
                            frame.kind == source_frame_kind::physical ? ::std::string_view{"physical"} : ::std::string_view{"caller"}),
                        " module=", ::fast_io::mnp::dec(frame.module), " function=", ::fast_io::mnp::dec(frame.function),
                        " generation=", ::fast_io::mnp::dec(frame.function_generation), " runtime-epoch=", ::fast_io::mnp::dec(frame.runtime_epoch),
                        " scope-unit=", ::fast_io::mnp::dec(frame.scope.unit), " scope-offset=", ::fast_io::mnp::dec(frame.scope.offset),
                        " variables=", ::fast_io::mnp::code_cvt(frame.variables_available ? ::std::string_view{"current"} : ::std::string_view{"caller-unavailable"}), " name=",
                        source_dwarf::escaped_metadata_text{::std::string_view{frame.name}.substr(0u, (::std::min)(frame.name.size(), ::std::size_t{56u}))},
                        frame.name.size() > 56u ? ::std::string_view{" [name truncated]\n"} : ::std::string_view{"\n"});
                }
                ::fast_io::io::print(output, command.wasm_frames ? ::fast_io::string_view{"wasm-frames end\n"} : ::fast_io::string_view{"source-frames end\n"}); return text;
            }
            if(command.kind == console_command_kind::wasm_script)
            {
                if(reply.script_replies.size() != reply.script_commands.size() || reply.script_replies.size() > 16u)
                { return ::fast_io::concat_std("error: invalid bounded script reply\n"); }
                ::std::size_t shown{};
                for(::std::size_t i{}; i != reply.script_replies.size(); ++i)
                {
                    if(reply.script_commands[i].kind == console_command_kind::wasm_script)
                    { return ::fast_io::concat_std("error: recursive script reply is forbidden\n"); }
                    auto const child{format_reply(reply.script_replies[i], reply.script_commands[i])};
                    auto const heading{::fast_io::concat_std("wasm-script command ", ::fast_io::mnp::dec(i + 1u), "\n")};
                    if(text.size() > maximum_formatted_reply_bytes - reply_trailer_reserve ||
                       heading.size() > maximum_formatted_reply_bytes - reply_trailer_reserve - text.size() ||
                       child.size() > maximum_formatted_reply_bytes - reply_trailer_reserve - text.size() - heading.size())
                    { break; }
                    // [owned complete heading/child ... checked combined reply bound] end
                    // [safe                                                          ] emit complete command records;
                    //  ^^ no byte truncation can turn an omitted command into success.
                    ::fast_io::io::print(output, ::std::string_view{heading}, ::std::string_view{child}); ++shown;
                }
                if(shown != reply.script_replies.size())
                {
                    auto const last_status{reply.script_replies.empty() ? ::uwvm2::utils::control::error::none : reply.script_replies.back().status};
                    ::fast_io::io::print(output, "wasm-script output-truncated shown=", ::fast_io::mnp::dec(shown),
                        " executed=", ::fast_io::mnp::dec(reply.script_replies.size()),
                        " last-command-status=", ::fast_io::mnp::dec(static_cast<unsigned>(last_status)), "\n");
                }
                ::fast_io::io::print(output, "wasm-script end\n"); return text;
            }
            if(command.kind == console_command_kind::wasm_event)
            {
                ::fast_io::io::print(output, "wasm-trace enabled=", ::fast_io::mnp::dec(static_cast<unsigned>(reply.wasm_trace_enabled)),
                    " filter=", wasm_events::name(reply.wasm_trace_filter), " overwritten=", ::fast_io::mnp::dec(reply.wasm_trace_overwritten), "\n");
                if(reply.wasm_trace_page_available)
                {
                    if(reply.wasm_trace.size() > wasm_events::maximum_trace_page_records)
                    { return ::fast_io::concat_std("error: invalid bounded Wasm trace page\n"); }
                    ::fast_io::io::print(output, "wasm-trace page oldest=", ::fast_io::mnp::dec(reply.wasm_trace_oldest_sequence),
                        " newest=", ::fast_io::mnp::dec(reply.wasm_trace_newest_sequence),
                        " next-after=", ::fast_io::mnp::dec(reply.wasm_trace_next_sequence),
                        " remaining=", ::fast_io::mnp::dec(reply.wasm_trace_remaining),
                        " cursor-gap=", ::fast_io::mnp::dec(static_cast<unsigned>(reply.wasm_trace_cursor_gap)), "\n");
                }
                if(reply.wasm_trace.size() > wasm_events::maximum_trace_page_records || reply.wasm_catchpoints.size() > 256u)
                { return ::fast_io::concat_std("error: invalid bounded Wasm event response\n"); }
                if(command.wasm_request.command == wasm_events::command_kind::catch_set)
                { ::fast_io::io::print(output, "wasm-catchpoint ", ::fast_io::mnp::dec(reply.wasm_catchpoint_identifier), ::fast_io::mnp::os_c_str(command.wasm_request.event == wasm_events::category::uncaught ? " after-throw; before-unwind\n" : command.wasm_request.event == wasm_events::category::trap ? " after-failure\n" : " before-instruction\n")); }
                for(auto const& point : reply.wasm_catchpoints)
                {
                    ::fast_io::io::print(output, "wasm-catchpoint ", ::fast_io::mnp::dec(point.identifier),
                        " enabled=", ::fast_io::mnp::dec(static_cast<unsigned>(point.enabled)), " event=", wasm_events::name(point.event),
                        " module=", ::fast_io::mnp::dec(point.module), " function=");
                    if(point.all_functions) { ::fast_io::io::print(output, "all"); } else { ::fast_io::io::print(output, ::fast_io::mnp::dec(point.function)); }
                    ::fast_io::io::print(output, " runtime-epoch=", ::fast_io::mnp::dec(point.runtime_epoch), ::fast_io::mnp::os_c_str(point.event == wasm_events::category::uncaught ? " after-throw; before-unwind\n" : point.event == wasm_events::category::trap ? " after-failure\n" : " before-instruction\n"));
                }
                for(auto const& entry : reply.wasm_trace)
                {
                    ::fast_io::io::print(output, "wasm-event sequence=", ::fast_io::mnp::dec(entry.sequence),
                        " thread=", ::fast_io::mnp::dec(entry.participant), " module=", ::fast_io::mnp::dec(entry.module),
                        " function=", ::fast_io::mnp::dec(entry.function), " byte-offset=", ::fast_io::mnp::dec(entry.offset),
                        " runtime-epoch=", ::fast_io::mnp::dec(entry.runtime_epoch), " function-generation=", ::fast_io::mnp::dec(entry.function_generation),
                        ::fast_io::mnp::os_c_str(entry.uncaught ? " unhandled-before-unwind opcode=" : entry.trapped ? " after-failure opcode=" : " before-instruction opcode="));
                    if(!entry.instruction.available) { ::fast_io::io::print(output, "unavailable"); }
                    else
                    {
                        ::fast_io::io::print(output, "0x", ::fast_io::mnp::hex<false, true>(entry.instruction.primary));
                        if(entry.instruction.prefixed) { ::fast_io::io::print(output, ":", ::fast_io::mnp::dec(entry.instruction.extended)); }
                    }
                    ::fast_io::io::print(output, ::fast_io::mnp::chvw('\n'));
                }
                ::fast_io::io::print(output, "wasm-events end\n"); return text;
            }
            if(command.kind == console_command_kind::assembly_registers)
            {
                if(reply.registers_stop_identifier == 0u || reply.registers.size() == 0u ||
                   (reply.registers.word_bits != 32u && reply.registers.word_bits != 64u) ||
                   (command.requested_step_thread != 0u && reply.disassembly_code.participant != command.requested_step_thread) ||
                   reply.registers.pc() != reply.disassembly_code.pc)
                { return ::fast_io::concat_std("error: current native frame-zero registers unavailable\n"); }
                char const* architecture_label{"unavailable"};
                switch(reply.registers.machine)
                {
                    case native_registers::architecture::x86_64: architecture_label = "x86_64"; break;
                    case native_registers::architecture::aarch64: architecture_label = "aarch64"; break;
                    case native_registers::architecture::i686: architecture_label = "i686"; break;
                    case native_registers::architecture::powerpc: architecture_label = "powerpc"; break;
                    case native_registers::architecture::mips64: architecture_label = "mips64"; break;
                    case native_registers::architecture::riscv64: architecture_label = "riscv64"; break;
                    case native_registers::architecture::loongarch64: architecture_label = "loongarch64"; break;
                    case native_registers::architecture::sparc64: architecture_label = "sparc64"; break;
                    case native_registers::architecture::s390x: architecture_label = "s390x"; break;
                    case native_registers::architecture::arm: architecture_label = "arm"; break;
                    default: break;
                }
                ::fast_io::io::print(output, "native-registers stop=", ::fast_io::mnp::dec(reply.registers_stop_identifier),
                    " thread=", ::fast_io::mnp::dec(reply.disassembly_code.participant), " frame=0 module=",
                    ::fast_io::mnp::dec(reply.disassembly_code.module), " function=", ::fast_io::mnp::dec(reply.disassembly_code.function),
                    " generation=", ::fast_io::mnp::dec(reply.disassembly_code.function_generation),
                    " epoch=", ::fast_io::mnp::dec(reply.disassembly_code.runtime_epoch), " architecture=",
                    ::fast_io::mnp::os_c_str(architecture_label), " word-bits=", ::fast_io::mnp::dec(reply.registers.word_bits), "\n");
                ::std::size_t selected{native_registers::max_registers}, selected_fp{native_registers::max_fp_registers};
                bool all_registers{};
                if(command.register_name_size != 0u)
                {
                    if(command.register_name_size > command.register_name.size()) { return ::fast_io::concat_std("error: invalid register name\n"); }
                    // [owned bounded register token ...] end
                    // [safe                            ] metadata name comparison;
                    //  ^^ no expression or numeric native address is evaluated.
                    ::fast_io::array<char8_t, 32u> token{};
                    for(::std::size_t i{}; i != command.register_name_size; ++i)
                    {
                        // [owned bounded ASCII register name][owned char8 token]
                        // [safe                            ][safe             ]
                        //  ^^ checked equal indices; copy avoids incompatible char/char8 aliasing.
                        token[i] = static_cast<char8_t>(command.register_name[i]);
                    }
                    auto const requested{::std::u8string_view{token.data(), command.register_name_size}};
                    all_registers = requested == u8"all";
                    selected = native_registers::index_of(reply.registers.machine, requested);
                    if(!all_registers && selected >= reply.registers.size())
                    {
                        selected_fp = native_registers::fp_index_of(reply.registers.machine,
                            ::std::u8string_view{token.data(), command.register_name_size});
                        if(selected_fp == native_registers::max_fp_registers)
                        { return ::fast_io::concat_std("error: register is unavailable on the current architecture\n"); }
                    }
                }
                for(::std::size_t i{}; i != reply.registers.size(); ++i)
                {
                    if(command.register_name_size != 0u && !all_registers && i != selected) { continue; }
                    auto const* name{native_registers::name(reply.registers.machine, i)};
                    if(name == nullptr) { return ::fast_io::concat_std("error: invalid native register snapshot\n"); }
                    ::fast_io::io::print(output, "  ", ::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(name)), "=");
                    auto const bits{reply.registers.known_bits[i]};
                    if(bits == 0u) { ::fast_io::io::print(output, "unavailable\n"); continue; }
                    if((bits != 32u && bits != 64u) || bits > reply.registers.word_bits)
                    { return ::fast_io::concat_std("error: invalid numeric register width\n"); }
                    ::fast_io::io::print(output, "0x");
                    for(::std::size_t byte{reply.registers.word_bits / 8u}; byte != 0u; --byte)
                    {
                        if(byte > bits / 8u) { ::fast_io::io::print(output, "??"); }
                        else { ::fast_io::io::print(output, ::fast_io::mnp::hex<false, true>(
                            static_cast<::std::uint8_t>(reply.registers.values[i] >> ((byte - 1u) * 8u)))); }
                    }
                    ::fast_io::io::print(output, "\n");
                }
                // Preserve the existing GPR-only default display. Wide values
                // are explicitly selected by name or GDB's info all-registers.
                if(native_registers::count(reply.registers.machine) != 0u && (all_registers || selected_fp != native_registers::max_fp_registers))
                {
                    for(::std::size_t i{}; i != native_registers::max_fp_registers; ++i)
                    {
                        if(!all_registers && i != selected_fp) { continue; }
                        auto const* name{native_registers::fp_name(reply.registers.machine, i)};
                        auto const& value{reply.registers.floating.values[i]};
                        auto const width{native_registers::fp_width(reply.registers.machine, i)};
                        if(name == nullptr && value.width == 0u) { continue; }
                        if(name == nullptr || width == 0u || width > value.bytes.size() || value.width > width ||
                           (value.width != 0u && ((reply.registers.machine == native_registers::architecture::x86_64 && i >= 16u) || (value.width != 4u && value.width != 8u && value.width != 16u))))
                        { return ::fast_io::concat_std("error: invalid native floating register snapshot\n"); }
                        ::fast_io::io::print(output, "  ", ::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(name)), "=");
                        if(value.width == 0u) { ::fast_io::io::print(output, "unavailable\n"); continue; }
                        ::fast_io::io::print(output, "0x");
                        for(::std::size_t part{width}; part != 0u; --part)
                        {
                            // [owned little-endian register bytes ... width<=16] end
                            // [safe                                             ] part>0
                            //  ^^ decrement-before-index is bounded by the checked
                            // width; FastIO formats each exact two-digit byte.
                            if(part > value.width) { ::fast_io::io::print(output, "??"); }
                            else { ::fast_io::io::print(output, ::fast_io::mnp::hex<false, true>(value.bytes[part - 1u])); }
                        }
                        ::fast_io::io::print(output, "\n");
                    }
                }
                ::fast_io::io::print(output, "native-registers end\n"); return text;
            }
            if(command.kind == console_command_kind::source_type ||
               (command.kind == console_command_kind::source_value && reply.source_object_value_available))
            {
                if((!reply.source_object_type_available && !reply.source_object_value_available) || reply.source_stop_identifier == 0u ||
                   reply.source_object_type.size() > 1024u)
                { return ::fast_io::concat_std("error: current scoped source type unavailable\n"); }
                ::fast_io::io::print(output, ::fast_io::mnp::os_c_str(command.kind == console_command_kind::source_value ? "source-value stop=" : "source-type stop="), ::fast_io::mnp::dec(reply.source_stop_identifier), " name=");
                if(command.source_variable_name_size > command.source_variable_name.size()) { return ::fast_io::concat_std("error: invalid source name\n"); }
                add_source_path(text, {command.source_variable_name.data(), command.source_variable_name_size}); ::fast_io::io::print(output, ::fast_io::mnp::chvw('\n'));
                // Validate the entire owned tree even when its display later
                // needs truncation; a bad omitted row is still a malformed reply.
                for(::std::size_t i{}; i != reply.source_object_type.size(); ++i)
                {
                    auto const& node{reply.source_object_type[i]};
                    if(node.depth > 32u || (node.parent != source_dwarf::no_record && node.parent >= i))
                    { return ::fast_io::concat_std("error: invalid source object tree\n"); }
                }
                ::std::size_t shown{};
                for(::std::size_t i{}; i != reply.source_object_type.size(); ++i)
                {
                    auto const& node{reply.source_object_type[i]};
                    ::std::string row;
                    ::fast_io::ostring_ref_std row_output{__builtin_addressof(row)};
                    if(command.kind == console_command_kind::source_value)
                    { ::fast_io::io::print(row_output, source_dwarf::object_details_of(node), "\n"); }
                    else
                    {
                        ::fast_io::io::print(row_output, "  type-node ", ::fast_io::mnp::dec(i), " parent=");
                        if(node.parent == source_dwarf::no_record) { ::fast_io::io::print(row_output, "none"); }
                        else { ::fast_io::io::print(row_output, ::fast_io::mnp::dec(node.parent)); }
                        // The shared metadata CPO escapes terminal/control/bidi
                        // bytes and marks long names truncated at 4096 bytes.
                        // A valid long producer name is not an invalid type.
                        ::fast_io::io::print(row_output, " depth=", ::fast_io::mnp::dec(node.depth), " name=",
                            source_dwarf::escaped_metadata_text{node.name}, " type=",
                            source_dwarf::escaped_metadata_text{node.type_name});
                        ::fast_io::io::print(row_output, " kind=", ::fast_io::mnp::code_cvt(source_dwarf::object_type_kind_text(node.kind)),
                            " byte-offset=", ::fast_io::mnp::dec(node.byte_offset), " byte-size=", ::fast_io::mnp::dec(node.byte_size));
                        if(node.inherited) { ::fast_io::io::print(row_output, " inherited"); }
                        if(node.bit_field) { ::fast_io::io::print(row_output, " bit-width=", ::fast_io::mnp::dec(node.bit_width)); }
                        if(node.omitted_children != 0u) { ::fast_io::io::print(row_output, " omitted=", ::fast_io::mnp::dec(node.omitted_children)); }
                        if(node.reason != source_dwarf::object_unavailable_reason::none)
                        { ::fast_io::io::print(row_output, " unavailable="); add_source_path(row, source_dwarf::object_reason_text(node.reason)); }
                        ::fast_io::io::print(row_output, ::fast_io::mnp::chvw('\n'));
                    }
                    if(text.size() > maximum_formatted_reply_bytes - reply_trailer_reserve ||
                       row.size() > maximum_formatted_reply_bytes - reply_trailer_reserve - text.size()) { break; }
                    // [owned complete object row ... checked remaining reply budget] end
                    // [safe                                                       ] no split escape/value;
                    //  ^^ append copied display data, never an address or executable expression.
                    ::fast_io::io::print(output, ::std::string_view{row}); ++shown;
                }
                if(shown != reply.source_object_type.size())
                { ::fast_io::io::print(output, "source-object output-truncated shown=", ::fast_io::mnp::dec(shown),
                    " total=", ::fast_io::mnp::dec(reply.source_object_type.size()), "\n"); }
                ::fast_io::io::print(output, ::fast_io::mnp::os_c_str(command.kind == console_command_kind::source_value ? "source-value end\n" : "source-type end\n")); return text;
            }
            if(command.kind == console_command_kind::assembly_disassemble_range)
            {
                auto const& code{reply.disassembly_code};
                if(reply.disassembly_stop_identifier == 0u || reply.disassembly_count == 0u ||
                   reply.disassembly_count > reply.disassembly.size() || code.participant == 0u ||
                   code.function_generation == 0u || code.runtime_epoch == 0u ||
                   reply.disassembly_owner_begin == 0u || reply.disassembly_owner_end <= reply.disassembly_owner_begin ||
                   code.pc < reply.disassembly_owner_begin || code.pc >= reply.disassembly_owner_end ||
                   reply.disassembly_function_name_size > reply.disassembly_function_name.size() ||
                   (!command.disassembly_resolve_symbols && reply.disassembly_function_name_size != 0u))
                { return ::fast_io::concat_std("error: native disassembly range is unavailable\n"); }
                native_owned_instruction_semantics::decoder public_decoder{code.target};
                for(::std::size_t index{}; index != reply.disassembly_count; ++index)
                {
                    auto const& instruction{reply.disassembly[index]};
                    if(!native_branch_display::matches(reply.disassembly_destinations[index], instruction) ||
                       !native_branch_display::displayable(public_decoder, instruction, reply.disassembly_destinations[index],
                           reply.disassembly_owner_begin, reply.disassembly_owner_end, true) ||
                       (!instruction && instruction.pc != 0u && (instruction.pc < reply.disassembly_owner_begin ||
                         instruction.pc >= reply.disassembly_owner_end)) ||
                       (instruction && (instruction.pc < reply.disassembly_owner_begin ||
                         instruction.pc >= reply.disassembly_owner_end || instruction.size > instruction.bytes.size() ||
                         instruction.size > reply.disassembly_owner_end - instruction.pc ||
                         ::std::memchr(instruction.text.data(), '\0', instruction.text.size()) == nullptr)))
                    { return ::fast_io::concat_std("error: native disassembly range is invalid\n"); }
                }
                ::fast_io::io::print(output, "native-disassembly-range stop=", ::fast_io::mnp::dec(reply.disassembly_stop_identifier),
                    " thread=", ::fast_io::mnp::dec(code.participant), " module=", ::fast_io::mnp::dec(code.module),
                    " function=", ::fast_io::mnp::dec(code.function), " function-generation=", ::fast_io::mnp::dec(code.function_generation),
                    " runtime-epoch=", ::fast_io::mnp::dec(code.runtime_epoch),
                    " origin=", ::fast_io::mnp::cond(code.native_instruction_stop, "native-instruction-stop", "safepoint-code-view"), " reference-pc=0x", ::fast_io::mnp::hex<false, true>(code.pc),
                    " owner-begin=0x", ::fast_io::mnp::hex<false, true>(reply.disassembly_owner_begin),
                    " owner-end=0x", ::fast_io::mnp::hex<false, true>(reply.disassembly_owner_end),
                    " byte-offset=", ::fast_io::mnp::dec(command.disassembly_byte_offset),
                    " instruction-offset=", ::fast_io::mnp::dec(command.disassembly_instruction_offset),
                    " resolve-symbols=", ::fast_io::mnp::dec(static_cast<unsigned>(command.disassembly_resolve_symbols)), "\n");
                if(reply.disassembly_function_name_size != 0u)
                {
                    ::fast_io::io::print(output, "native-function-name ");
                    // [owned name array ... bounded size<=256] end
                    // [safe                                 ] byte view has explicit extent;
                    //  ^^ never treat guest function metadata as a C string/address.
                    add_source_path(text, ::std::string_view{reinterpret_cast<char const*>(reply.disassembly_function_name.data()),
                        reply.disassembly_function_name_size});
                    ::fast_io::io::print(output, ::fast_io::mnp::chvw('\n'));
                }
                for(::std::size_t index{}; index != reply.disassembly_count; ++index)
                {
                    auto const& instruction{reply.disassembly[index]};
                    ::fast_io::io::print(output, "  instruction ", ::fast_io::mnp::dec(index));
                    if(!instruction) { ::fast_io::io::print(output, " unavailable\n"); continue; }
                    ::fast_io::io::print(output, " function-offset=", ::fast_io::mnp::dec(instruction.pc - reply.disassembly_owner_begin),
                        " pc=0x", ::fast_io::mnp::hex<false, true>(instruction.pc), " bytes=");
                    for(::std::size_t byte{}; byte != instruction.size; ++byte)
                    {
                        if(byte != 0u) { ::fast_io::io::print(output, ::fast_io::mnp::chvw(' ')); }
                        ::fast_io::io::print(output, ::fast_io::mnp::hex<false, true>(instruction.bytes[byte]));
                    }
                    ::fast_io::io::print(output, "  ");
                    // [fixed decoded text ... verified NUL] end
                    // [safe                               ] MC output is owned/terminated;
                    //  ^^ sanitize display bytes without any expression/symbol evaluation.
                    add_source_path(text, ::std::string_view{instruction.text.data()});
                    native_branch_display::print(output, reply.disassembly_destinations[index],
                        reply.disassembly_owner_begin, reply.disassembly_owner_end, command.disassembly_resolve_symbols,
                        code.module, code.function, code.function_generation);
                    ::fast_io::io::print(output, ::fast_io::mnp::chvw('\n'));
                }
                ::fast_io::io::print(output, "native-disassembly-end\n"); return text;
            }
            if(command.kind == console_command_kind::assembly_disassemble)
            {
                auto const& code{reply.disassembly_code};
                if(reply.disassembly_stop_identifier == 0u || reply.disassembly_count == 0u ||
                   reply.disassembly_count > reply.disassembly.size() || code.participant == 0u ||
                   code.pc == 0u || code.function_generation == 0u || code.runtime_epoch == 0u)
                { return ::fast_io::concat_std("error: native disassembly result is unavailable\n"); }
                native_owned_instruction_semantics::decoder public_decoder{code.target};
                for(::std::size_t index{}; index != reply.disassembly_count; ++index)
                {
                    auto const& instruction{reply.disassembly[index]};
                    if(!native_branch_display::matches(reply.disassembly_destinations[index], instruction) ||
                       !native_branch_display::displayable(public_decoder, instruction, reply.disassembly_destinations[index],
                           reply.disassembly_owner_begin, reply.disassembly_owner_end, true) ||
                       instruction.pc == 0u || instruction.size > instruction.bytes.size() ||
                       (instruction && ::std::memchr(instruction.text.data(), '\0', instruction.text.size()) == nullptr))
                    { return ::fast_io::concat_std("error: native disassembly result is invalid\n"); }
                }
                ::fast_io::io::print(output, "native-disassembly stop=", ::fast_io::mnp::dec(reply.disassembly_stop_identifier),
                    " thread=", ::fast_io::mnp::dec(code.participant), " module=", ::fast_io::mnp::dec(code.module),
                    " function=", ::fast_io::mnp::dec(code.function), " function-generation=", ::fast_io::mnp::dec(code.function_generation),
                    " runtime-epoch=", ::fast_io::mnp::dec(code.runtime_epoch),
                    " origin=", ::fast_io::mnp::cond(code.native_instruction_stop, "native-instruction-stop", "safepoint-code-view"), "\n");
                for(::std::size_t index{}; index != reply.disassembly_count; ++index)
                {
                    auto const& instruction{reply.disassembly[index]};
                    ::fast_io::io::print(output, "  instruction ", ::fast_io::mnp::dec(index), " pc=0x",
                        ::fast_io::mnp::hex<false, true>(instruction.pc));
                    if(!instruction) { ::fast_io::io::print(output, " unavailable\n"); continue; }
                    ::fast_io::io::print(output, " function-offset=", ::fast_io::mnp::dec(instruction.pc - reply.disassembly_owner_begin), " bytes=");
                    for(::std::size_t byte{}; byte != instruction.size; ++byte)
                    {
                        if(byte != 0u) { ::fast_io::io::print(output, ::fast_io::mnp::chvw(' ')); }
                        ::fast_io::io::print(output, ::fast_io::mnp::hex<false, true>(instruction.bytes[byte]));
                    }
                    ::fast_io::io::print(output, "  ");
                    // [fixed decoded text, verified NUL] end
                    // [safe                           ] bounded decoder validates its terminator;
                    //  ^^ sanitize the owned text so no terminal/protocol control can escape.
                    add_source_path(text, ::std::string_view{instruction.text.data()});
                    // Owner containment remains mandatory when naming is disabled.
                    native_branch_display::print(output, reply.disassembly_destinations[index],
                        reply.disassembly_owner_begin, reply.disassembly_owner_end, false,
                        code.module, code.function, code.function_generation);
                    ::fast_io::io::print(output, ::fast_io::mnp::chvw('\n'));
                }
                ::fast_io::io::print(output, "native-disassembly-end\n");
                return text;
            }
            if(command.operation == ::uwvm2::utils::control::operation::read_memory)
            {
                ::std::uint64_t module{},offset{};::std::uint32_t memory{},reserved{};
                ::uwvm2::utils::control::input_buffer fields{command.payload};
                ::fast_io::io::scan(fields,::fast_io::mnp::le_get<64>(module),::fast_io::mnp::le_get<32>(memory),
                    ::fast_io::mnp::le_get<32>(reserved),::fast_io::mnp::le_get<64>(offset));
                ::fast_io::io::print(output,"Wasm memory module=",module," index=",memory," offset=",offset," bytes=",reply.memory.size(),"\n",
                    console_memory::dump(reply.memory,offset)); return text;
            }
            if(command.operation == ::uwvm2::utils::control::operation::replace_function)
            {
                ::fast_io::io::print(output, "function replaced; generation ", ::fast_io::mnp::dec(reply.replacement_generation), "\n");
                return text;
            }
            if(reply.timed_out)
            {
                ::fast_io::io::print(output, ::fast_io::mnp::os_c_str(command.wait_for_event ?
                    "wait timed out; " : "pause/step timed out; "));
                if(reply.execution == execution_status::stopping)
                { ::fast_io::io::print(output, "cooperative pause remains pending; no complete stop established\n"); }
                else if(reply.execution == execution_status::running)
                { ::fast_io::io::print(output, "no complete stop established; VM is running\n"); }
                else { ::fast_io::io::print(output, "execution state below is the current actual snapshot\n"); }
            }
            if((command.kind == console_command_kind::assembly_step || command.kind == console_command_kind::assembly_next) &&
               reply.execution == execution_status::stopped && reply.native_step_from != 0u && reply.native_instruction)
            {
                if(reply.native_instruction.size > reply.native_instruction.bytes.size())
                { return ::fast_io::concat_std("error: invalid native instruction extent\n"); }
                auto const text_end{::std::find(reply.native_instruction.text.begin(), reply.native_instruction.text.end(), '\0')};
                if(text_end == reply.native_instruction.text.end()) { return ::fast_io::concat_std("error: invalid native instruction text\n"); }
                ::fast_io::io::print(output, "native instruction 0x", ::fast_io::mnp::hex<false, true>(reply.native_step_from),
                    " bytes=");
                for(::std::size_t index{}; index != reply.native_instruction.size; ++index)
                {
                    if(index != 0u) { ::fast_io::io::print(output, ::fast_io::mnp::chvw(' ')); }
                    auto const value{reply.native_instruction.bytes[index]};
                    ::fast_io::io::print(output, ::fast_io::mnp::hex<false, true>(value));
                }
                ::fast_io::io::print(output, "  ");
                // [owned decoded text ... checked NUL] end
                // [safe                             ] size determined within fixed array before span construction.
                add_source_path(text, {reply.native_instruction.text.data(), static_cast<::std::size_t>(text_end - reply.native_instruction.text.begin())});
                ::fast_io::io::print(output, " -> 0x", ::fast_io::mnp::hex<false, true>(reply.native_step_to), "\n");
            }
            if(command.kind == console_command_kind::assembly_finish &&
               reply.execution == execution_status::stopped && reply.native_step_from != 0u && reply.native_step_to != 0u)
            { ::fast_io::io::print(output,"native finish 0x",::fast_io::mnp::hex<false,true>(reply.native_step_from),
                " -> 0x",::fast_io::mnp::hex<false,true>(reply.native_step_to),"\n"); }
            if(reply.breakpoint_identifier != 0u)
            {
                if(command.kind == console_command_kind::assembly_breakpoint)
                { ::fast_io::io::print(output, "breakpoint ", ::fast_io::mnp::dec(reply.breakpoint_identifier),
                    " asm function-offset=", ::fast_io::mnp::dec(command.native_breakpoint_offset), "\n"); }
                else if(command.kind == console_command_kind::source_breakpoint)
                {
                    ::fast_io::io::print(output, "breakpoint ", ::fast_io::mnp::dec(reply.breakpoint_identifier),
                        " source=", ::fast_io::string_view{command.source_path.data(), command.source_path_size},
                        ":", ::fast_io::mnp::dec(command.source_line),
                        " function=", ::fast_io::mnp::dec(reply.source_breakpoint_function),
                        " byte-offset=", ::fast_io::mnp::dec(reply.source_breakpoint_offset), "\n");
                }
                else
                {
                    ::fast_io::io::print(output, "breakpoint ", ::fast_io::mnp::dec(reply.breakpoint_identifier),
                                        " registered at executable Wasm expression byte offset\n");
                }
                return text;
            }
            if(command.operation == ::uwvm2::utils::control::operation::breakpoint_clear)
            { return ::fast_io::concat_std("breakpoint deleted\n"); }
            if(command.kind == console_command_kind::breakpoint_control)
            { return ::fast_io::concat_std("breakpoint policy updated\n"); }
            if(command.operation == ::uwvm2::utils::control::operation::breakpoint_list)
            {
                if(reply.breakpoints.empty()) { return ::fast_io::concat_std("no breakpoints\n"); }
                for(auto const& point : reply.breakpoints)
                {
                    ::fast_io::io::print(output, "breakpoint ", ::fast_io::mnp::dec(point.identifier),
                                        " module=", ::fast_io::mnp::dec(point.module), " function=", ::fast_io::mnp::dec(point.function),
                                        " kind=", ::fast_io::mnp::cond(point.assembly, "asm", "wasm"),
                                        " byte-offset=", ::fast_io::mnp::dec(point.offset),
                                        " enabled=", ::fast_io::mnp::os_c_str(point.enabled ? "yes" : "no"), " hits=", ::fast_io::mnp::dec(point.hits),
                                        " ignore=", ::fast_io::mnp::dec(point.ignore_remaining));
                    if(point.condition_size != 0u && point.condition_size <= point.condition.size())
                    { ::fast_io::io::print(output," condition=",::fast_io::string_view{point.condition.data(),point.condition_size}); }
                    ::fast_io::io::print(output,"\n");
                }
                return text;
            }
            if(command.kind == console_command_kind::source_locals || command.kind == console_command_kind::source_value)
            {
                if(reply.source_stop_identifier != 0u)
                { ::fast_io::io::print(output, "source-stop ", ::fast_io::mnp::dec(reply.source_stop_identifier), "\n"); }
                if(!reply.source_locals_available)
                {
                    ::fast_io::io::print(output, "source locals unavailable: ");
                    switch(reply.source_locals_reason)
                    {
                        case source_inline_unavailable_reason::none: ::fast_io::io::print(output, "no current value metadata"); break;
                        case source_inline_unavailable_reason::no_bound_metadata: ::fast_io::io::print(output, "no embedded metadata bound to the live code owner"); break;
                        case source_inline_unavailable_reason::invalid_metadata: ::fast_io::io::print(output, "embedded DWARF metadata is invalid or unsupported"); break;
                        case source_inline_unavailable_reason::metadata_limit: ::fast_io::io::print(output, "metadata or value query limit exceeded"); break;
                        case source_inline_unavailable_reason::no_current_frame: ::fast_io::io::print(output, "no current cooperative local snapshot"); break;
                        case source_inline_unavailable_reason::stale_generation_or_stop: ::fast_io::io::print(output, "captured stop, source or function generation is stale"); break;
                        case source_inline_unavailable_reason::unmapped: ::fast_io::io::print(output, "no concrete source scope at this Wasm position"); break;
                        case source_inline_unavailable_reason::ambiguous: ::fast_io::io::print(output, "source scopes or locations are ambiguous"); break;
                        case source_inline_unavailable_reason::allocation_failure: ::fast_io::io::print(output, "metadata allocation failed"); break;
                        case source_inline_unavailable_reason::native_stop: ::fast_io::io::print(output, "native trap has no current cooperative local snapshot"); break;
                    }
                    ::fast_io::io::print(output, ::fast_io::mnp::chvw('\n')); return text;
                }
                if(reply.source_locals.empty()) { ::fast_io::io::print(output, "no active source variables\n"); return text; }
                // Independently verify the producer's finite result budget BEFORE
                // formatting any untrusted label. A single temporary row is bounded
                // by escaped_metadata_text's 4096-byte input cap, not metadata size.
                constexpr ::std::size_t source_reply_cap{63u * 1024u};
                ::std::size_t source_string_bytes{};
                if(reply.source_locals.size() > 1024u) { return ::fast_io::concat_std("error: source variable result exceeds bounded capacity\n"); }
                for(auto const& value : reply.source_locals)
                {
                    if(!source_dwarf::budget::charge(value.name.size(), 16384u, source_string_bytes) ||
                       !source_dwarf::budget::charge(value.type_name.size(), 16384u, source_string_bytes))
                    { return ::fast_io::concat_std("error: source variable labels exceed bounded capacity\n"); }
                }
                ::std::string origin{}; ::fast_io::ostring_ref_std origin_output{__builtin_addressof(origin)};
                if(command.kind == console_command_kind::source_value && reply.source_constant.available)
                {
                    auto const& receipt{reply.source_constant};
                    ::fast_io::io::print(origin_output, "source-origin stop=", ::fast_io::mnp::dec(reply.source_stop_identifier),
                        " thread=", ::fast_io::mnp::dec(receipt.participant), " code-offset=", ::fast_io::mnp::dec(receipt.code_offset),
                        " variable-unit=", ::fast_io::mnp::dec(receipt.variable.unit), " variable-offset=", ::fast_io::mnp::dec(receipt.variable.offset),
                        " scope-unit=", ::fast_io::mnp::dec(receipt.scope.unit), " scope-offset=", ::fast_io::mnp::dec(receipt.scope.offset),
                        " type-unit=", ::fast_io::mnp::dec(receipt.type.unit), " type-offset=", ::fast_io::mnp::dec(receipt.type.offset),
                        " kind=DW_AT_const_value\n");
                }
                if(text.size() > source_reply_cap || origin.size() > 512u ||
                   origin.size() > source_reply_cap - text.size() || reply_trailer_reserve > source_reply_cap - text.size() - origin.size())
                { return ::fast_io::concat_std("error: source variable reply header exceeds bounded capacity\n"); }
                ::std::size_t shown{};
                for(auto const& value : reply.source_locals)
                {
                    ::std::string row{}; ::fast_io::ostring_ref_std row_output{__builtin_addressof(row)};
                    ::fast_io::io::print(row_output, ::fast_io::mnp::os_c_str(value.parameter ? "source parameter " : "source local "));
                    ::fast_io::io::print(row_output, source_dwarf::escaped_metadata_text{::std::string_view{value.name}},
                        " type=", source_dwarf::escaped_metadata_text{::std::string_view{value.type_name}}, " = ");
                    switch(value.kind)
                    {
                        case source_dwarf::numeric_kind::unavailable:
                            ::fast_io::io::print(row_output, "unavailable ("); ::fast_io::io::print(row_output, source_dwarf::escaped_metadata_text{source_dwarf::numeric_reason_text(value.reason)}); ::fast_io::io::print(row_output, ::fast_io::mnp::chvw(')')); break;
                        case source_dwarf::numeric_kind::boolean:
                            ::fast_io::io::print(row_output, ::fast_io::mnp::os_c_str(value.bits == 0u ? "bool=false" : "bool=true")); break;
                        case source_dwarf::numeric_kind::signed_integer:
                            ::fast_io::io::print(row_output, "i", ::fast_io::mnp::dec(static_cast<unsigned>(value.byte_count) * 8u),
                                "=", ::fast_io::mnp::dec(source_dwarf::numeric_signed_value(value))); break;
                        case source_dwarf::numeric_kind::unsigned_integer:
                            ::fast_io::io::print(row_output, "u", ::fast_io::mnp::dec(static_cast<unsigned>(value.byte_count) * 8u),
                                "=", ::fast_io::mnp::dec(value.bits)); break;
                        case source_dwarf::numeric_kind::f32_bits:
                            ::fast_io::io::print(row_output, "f32 bits=0x", ::fast_io::mnp::hex<false, true>(static_cast<::std::uint32_t>(value.bits)), " value=", ::std::bit_cast<float>(static_cast<::std::uint32_t>(value.bits))); break;
                        case source_dwarf::numeric_kind::f64_bits:
                            ::fast_io::io::print(row_output, "f64 bits=0x", ::fast_io::mnp::hex<false, true>(value.bits), " value=", ::std::bit_cast<double>(value.bits)); break;
                    }
                    ::fast_io::io::print(row_output, ::fast_io::mnp::chvw('\n'));
                    // [owned complete row] row_end
                    // [safe              ] entire row checked BEFORE append;
                    //  ^^ subtract the header, origin and full truncation trailer.
                    if(row.size() > source_reply_cap - text.size() - origin.size() - reply_trailer_reserve) { break; }
                    ::fast_io::io::print(output, ::std::string_view{row}); ++shown;
                }
                ::fast_io::io::print(output, ::std::string_view{origin});
                if(shown != reply.source_locals.size())
                { ::fast_io::io::print(output, "source-variables shown=", ::fast_io::mnp::dec(shown),
                    " total=", ::fast_io::mnp::dec(reply.source_locals.size()), " truncated=true\n"); }
                return text;
            }
            if(command.kind == console_command_kind::wasip1_state)
            {
                ::std::string text;
                auto output{::fast_io::ostring_ref_std{::std::addressof(text)}};
                ::fast_io::io::print(output, "wasip1-stop ", ::fast_io::mnp::dec(reply.stop_identifier), "\n");
                if(wasip1_calls::is_trace(command.wasip1_state_request.operation)) { wasip1_calls::print(output, reply.wasip1_trace_values); }
                else
                { wasip1_state::print(output, reply.wasip1_state_values);
                  if(command.wasip1_state_request.operation==wasip1_state::action::descriptor_detail)
                  { ::fast_io::io::print(output,"Descriptor-number history (may include close/reuse); current path above is observed separately.\n");
                    wasip1_calls::print(output,reply.wasip1_trace_values); } }
                return text;
            }
            if(command.kind==console_command_kind::wasm_path)
            {
                auto const& path{reply.wasm_path_values};
                if(path.result!=wasm_state::status::available)
                { return ::fast_io::concat_std("Wasm path unavailable: ",wasm_state::status_text(path.result),"\n"); }
                if(path.cleared) { return ::fast_io::concat_std("Wasm paths cleared\n"); }
                return ::fast_io::concat_std("wasm-stop ",::fast_io::mnp::dec(reply.stop_identifier),"\n",
                    "Wasm path session=",::fast_io::mnp::dec(path.session)," handle=",::fast_io::mnp::dec(path.handle),
                    " depth=",::fast_io::mnp::dec(path.depth)," view=",::fast_io::mnp::dec(wasm_path::bound_view_version),
                    " protocol=",::fast_io::mnp::dec(wasm_path::protocol_version),"\n",
                    command.wasm_path_request.action==wasm_path::operation::members ? wasm_state::format(reply.wasm_state_values) : ::std::string{});
            }
            if(command.kind==console_command_kind::wasm_mutation)
            { return ::fast_io::concat_std("wasm-stop ",::fast_io::mnp::dec(reply.stop_identifier),"\n",
                wasm_mutation::format(reply.wasm_mutation_value)); }
            if(command.kind == console_command_kind::wasm_state)
            { return ::fast_io::concat_std("wasm-stop ", ::fast_io::mnp::dec(reply.stop_identifier),
                ::fast_io::mnp::chvw('\n'),
                reply.reason == stop_reason::wasm_uncaught && command.wasm_state_request.participant == reply.wasm_terminal_participant && command.wasm_state_request.selected == wasm_state::selection::operands ?
                    ::fast_io::string_view{"Note: Uncaught Wasm snapshot; stack has not unwound.\n"} :
                reply.reason == stop_reason::wasm_trap && command.wasm_state_request.participant == reply.wasm_terminal_participant && command.wasm_state_request.selected == wasm_state::selection::operands ?
                    ::fast_io::string_view{"Note: Pre-trap Wasm inputs; no instruction result.\n"} : ::fast_io::string_view{},
                wasm_state::format(reply.wasm_state_values)); }
            if(command.operation == ::uwvm2::utils::control::operation::locals)
            {
                if(!reply.locals_available) { return ::fast_io::concat_std("locals unavailable for this stop\n"); }
                if(reply.total_local_count == 0u) { return ::fast_io::concat_std("no locals\n"); }
                for(::std::size_t index{}; index != reply.locals.size(); ++index)
                {
                    auto const& local{reply.locals[index]};
                    ::fast_io::io::print(output, "local ", ::fast_io::mnp::dec(index), " ");
                    if(!local.available)
                    { ::fast_io::io::print(output, "type=0x", ::fast_io::mnp::hex<false, true>(local.type),
                        " value unavailable (local not proven initialized at this stop)\n"); continue; }
                    if(local.type == 0x7fu || local.type == 0x7du)
                    {
                        ::std::uint32_t bits{};
                        // [complete 16-byte copied local] end
                        // [safe                         ] exactly four bytes are read from a stopped-frame snapshot.
                        ::std::memcpy(::std::addressof(bits), local.bytes.data(), sizeof(bits));
                        if(local.type == 0x7fu)
                        { ::fast_io::io::print(output, "i32=", ::fast_io::mnp::dec(::std::bit_cast<::std::int32_t>(bits))); }
                        else { ::fast_io::io::print(output, "f32 bits=0x", ::fast_io::mnp::hex<false, true>(bits)); }
                    }
                    else if(local.type == 0x7eu || local.type == 0x7cu)
                    {
                        ::std::uint64_t bits{};
                        // [complete 16-byte copied local] end
                        // [safe                         ] exactly eight bytes are read from a stopped-frame snapshot.
                        ::std::memcpy(::std::addressof(bits), local.bytes.data(), sizeof(bits));
                        if(local.type == 0x7eu)
                        { ::fast_io::io::print(output, "i64=", ::fast_io::mnp::dec(::std::bit_cast<::std::int64_t>(bits))); }
                        else { ::fast_io::io::print(output, "f64 bits=0x", ::fast_io::mnp::hex<false, true>(bits)); }
                    }
                    else if(local.type == 0x7bu)
                    {
                        ::fast_io::io::print(output, "v128 bytes=");
                        for(auto const byte : local.bytes)
                        { ::fast_io::io::print(output, ::fast_io::mnp::hex<false, true>(::std::to_integer<unsigned char>(byte))); }
                    }
                    else if(local.type == 0x70u || local.type == 0x6fu)
                    {
                        // A reference handle may embed a host address. Only its
                        // actual nullness is captured; never print the handle.
                        // Legacy 0x70 also projects typed GC references: it
                        // cannot identify the function heap. Rich locals wasm
                        // uses the actual typed state/GC identity producer.
                        ::fast_io::io::print(output, "reference (exact heap unavailable)=");
                        if(::std::to_integer<unsigned char>(local.bytes[0]) != 0u) { ::fast_io::io::print(output, "null"); }
                        else { ::fast_io::io::print(output, "non-null"); }
                    }
                    else
                    { ::fast_io::io::print(output, "type=0x", ::fast_io::mnp::hex<false, true>(local.type), " value unavailable"); }
                    ::fast_io::io::print(output, ::fast_io::mnp::chvw('\n'));
                }
                if(reply.locals.size() < reply.total_local_count)
                { ::fast_io::io::print(output, "locals truncated: showing ", ::fast_io::mnp::dec(reply.locals.size()),
                                       " of ", ::fast_io::mnp::dec(reply.total_local_count), "\n"); }
                return text;
            }
            switch(reply.execution)
            {
                case execution_status::running: ::fast_io::io::print(output, "running\n"); break;
                case execution_status::stopping: ::fast_io::io::print(output, "pause requested; executions are not all stopped\n"); break;
                case execution_status::stopped:
                    ::fast_io::io::print(output, ::fast_io::mnp::os_c_str(reply.reason == stop_reason::initial ? "prepared; no Wasm instruction executed\n" :
                        reply.reason == stop_reason::breakpoint ? "stopped: breakpoint\n" :
                        reply.reason == stop_reason::native_breakpoint ? "stopped: native breakpoint\n" :
                        reply.reason == stop_reason::wasm_entry ? "stopped: first Wasm instruction\n" :
                        reply.reason == stop_reason::execution_relocated ? "stopped: execution relocated\n" :
                        reply.reason == stop_reason::native_entry ? "stopped: first Wasm instruction JIT entry\n" :
                        reply.reason == stop_reason::native_step ? "stopped: native instruction step\n" :
                        reply.reason == stop_reason::wasm_uncaught ? "stopped: Uncaught Wasm exception before unwind; continue propagates\n" :
                        reply.reason == stop_reason::wasm_trap ? "stopped: Wasm trap after failure; pre-trap operands; continue terminates\n" :
                        reply.reason == stop_reason::wasm_catchpoint ? "stopped: Wasm catchpoint before instruction\n" :
                        reply.reason == stop_reason::step ? "stopped: selected participant step\n" : "stopped: pause\n"));
                    if(reply.reason == stop_reason::wasm_uncaught && reply.wasm_terminal_participant != 0u)
                    { ::fast_io::io::print(output, "uncaught-thread ", ::fast_io::mnp::dec(reply.wasm_terminal_participant), "\n"); }
                    if(reply.native_breakpoint_hit != 0u)
                    { ::fast_io::io::print(output, "native-breakpoint ", ::fast_io::mnp::dec(reply.native_breakpoint_hit), "\n"); }
                    if(reply.stop_identifier != 0u)
                    { ::fast_io::io::print(output, "stop-id ", ::fast_io::mnp::dec(reply.stop_identifier), "\n"); }
                    if(reply.breakpoint_condition_error != ::uwvm2::utils::control::error::none)
                    { ::fast_io::io::print(output,"breakpoint-condition ",::fast_io::mnp::dec(reply.breakpoint_condition_identifier),
                        " unavailable code=",::fast_io::mnp::dec(static_cast<unsigned>(reply.breakpoint_condition_error)),
                        "; actual stop retained\n"); }
                    if(reply.reason == stop_reason::wasm_catchpoint || reply.reason == stop_reason::wasm_trap)
                    { ::fast_io::io::print(output, "wasm-catchpoint ", ::fast_io::mnp::dec(reply.wasm_catchpoint_identifier), "\n"); }
                    break;
                case execution_status::exited:
                    ::fast_io::io::print(output, "guest exited: ", ::fast_io::mnp::dec(reply.guest_exit_code), "\n");
                    break;
                case execution_status::closed: ::fast_io::io::print(output, "debug domain closed\n"); break;
            }
            for(auto const& thread : reply.threads)
            {
                ::fast_io::io::print(output, "thread ", ::fast_io::mnp::dec(thread.identifier),
                                    " module=", ::fast_io::mnp::dec(thread.location.code_unit),
                                    " function=", ::fast_io::mnp::dec(thread.location.function),
                                    " byte-offset=", ::fast_io::mnp::dec(thread.location.offset),
                                    " generation=", ::fast_io::mnp::dec(thread.location.code_generation), "\n");
                if(thread.native_pc)
                {
                    ::fast_io::io::print(output, "  native-pc=0x", ::fast_io::mnp::hex<false, true>(*thread.native_pc), "\n");
                    ::fast_io::io::print(output, "  cooperative-location=last-observed; current native provenance follows\n");
                    ::fast_io::io::print(output, "  ", wasm_state::operand_snapshot_notice, ::fast_io::mnp::chvw('\n'));
                    if(command.operation == ::uwvm2::utils::control::operation::backtrace && reply.native_caller_stop_identifier != 0u)
                    {
                        if(reply.native_caller_stop_identifier != reply.stop_identifier)
                        { return ::fast_io::concat_std("error: stale physical Wasm caller view\n"); }
                        if(reply.native_backtrace)
                        {
                            auto const& trace{*reply.native_backtrace};
                            if(trace.count == 0u || trace.count > trace.frames.size() || !reply.native_caller ||
                               reply.native_caller->incarnation != trace.frames[0u].incarnation ||
                               reply.native_caller->current_incarnation != trace.frames[0u].current_incarnation)
                            { return ::fast_io::concat_std("error: invalid physical Wasm backtrace view\n"); }
                            for(::std::size_t i{}; i != trace.count; ++i)
                            {
                                auto const& frame{trace.frames[i]};
                                if(!frame.valid || frame.function_generation == 0u || frame.runtime_epoch == 0u ||
                                   frame.incarnation == 0u || frame.current_incarnation == 0u || frame.incarnation == frame.current_incarnation ||
                                   (i != 0u && (frame.current_incarnation != trace.frames[i - 1u].incarnation ||
                                    frame.runtime_epoch != trace.frames[i - 1u].runtime_epoch)))
                                { return ::fast_io::concat_std("error: invalid physical Wasm backtrace chain\n"); }
                                ::fast_io::io::print(output, "  physical Wasm caller module=", ::fast_io::mnp::dec(frame.module),
                                    " function=", ::fast_io::mnp::dec(frame.function), " function-generation=", ::fast_io::mnp::dec(frame.function_generation),
                                    " runtime-epoch=", ::fast_io::mnp::dec(frame.runtime_epoch), " incarnation=", ::fast_io::mnp::dec(frame.incarnation),
                                    " (authenticated frame ", ::fast_io::mnp::dec(i + 1u), ")\n");
                            }
                            ::fast_io::io::print(output, ::fast_io::mnp::os_c_str(trace.complete ? "  physical Wasm backtrace complete to Wasm root\n" :
                                "  partial physical Wasm backtrace (unavailable parent or frame limit)\n"));
                        }
                        else if(!reply.native_caller) { ::fast_io::io::print(output, "  physical Wasm caller unavailable\n"); }
                        else
                        {
                            auto const& caller{*reply.native_caller};
                            if(!caller.valid || caller.function_generation == 0u || caller.runtime_epoch == 0u ||
                               caller.incarnation == 0u || caller.current_incarnation == 0u || caller.incarnation == caller.current_incarnation)
                            { return ::fast_io::concat_std("error: invalid physical Wasm caller view\n"); }
                            ::fast_io::io::print(output, "  physical Wasm caller module=", ::fast_io::mnp::dec(caller.module),
                                " function=", ::fast_io::mnp::dec(caller.function), " function-generation=", ::fast_io::mnp::dec(caller.function_generation),
                                " runtime-epoch=", ::fast_io::mnp::dec(caller.runtime_epoch), " incarnation=", ::fast_io::mnp::dec(caller.incarnation),
                                " (one authenticated frame)\n");
                        }
                    }
                    if(!thread.native_wasm_position) { ::fast_io::io::print(output, "  native-wasm provenance=unavailable\n"); }
                    else
                    {
                        auto const& mapped{*thread.native_wasm_position};
                        if(mapped.pc != *thread.native_pc || mapped.participant != thread.identifier || mapped.owner_begin == 0u ||
                           mapped.owner_end <= mapped.owner_begin || mapped.pc < mapped.owner_begin || mapped.pc >= mapped.owner_end ||
                           mapped.function_generation == 0u || mapped.runtime_epoch == 0u)
                        { return ::fast_io::concat_std("error: invalid native provenance identity\n"); }
                        ::fast_io::io::print(output, "  native-wasm provenance=");
                        using provenance = ::uwvm2::runtime::lib::llvm_jit_debug_native_position_status;
                        switch(mapped.status)
                        {
                            case provenance::unavailable: ::fast_io::io::print(output, "unavailable\n"); break;
                            case provenance::unknown: ::fast_io::io::print(output, "unknown\n"); break;
                            case provenance::ambiguous: ::fast_io::io::print(output, "ambiguous\n"); break;
                            case provenance::exact:
                                if(mapped.row_begin < mapped.owner_begin || mapped.row_end > mapped.owner_end ||
                                   mapped.row_end <= mapped.row_begin || mapped.pc < mapped.row_begin || mapped.pc >= mapped.row_end)
                                { return ::fast_io::concat_std("error: invalid native provenance row\n"); }
                                ::fast_io::io::print(output, "exact module=", ::fast_io::mnp::dec(mapped.module),
                                    " function=", ::fast_io::mnp::dec(mapped.function), " byte-offset=", ::fast_io::mnp::dec(mapped.wasm_offset),
                                    " function-generation=", ::fast_io::mnp::dec(mapped.function_generation),
                                    " runtime-epoch=", ::fast_io::mnp::dec(mapped.runtime_epoch), "\n");
                                break;
                            default: return ::fast_io::concat_std("error: invalid native provenance status\n");
                        }
                    }
                }
                if(thread.source)
                {
                    ::fast_io::io::print(output, "  source ");
                    add_source_path(text, thread.source->file);
                    ::fast_io::io::print(output, ":", ::fast_io::mnp::dec(thread.source->line),
                        ":", ::fast_io::mnp::dec(thread.source->column), "\n");
                }
                if(command.operation != ::uwvm2::utils::control::operation::backtrace) { continue; }
                if(thread.source_inline_available)
                {
                    for(auto const& frame : thread.source_inline_frames)
                    {
                        // Inline display records have no physical caller PC or
                        // runtime value/address authority. Keep the existing #N
                        // physical frame protocol unchanged for current bridges.
                        ::fast_io::io::print(output, "  inline "); add_source_path(text, frame.name);
                        if(!frame.call_file.empty())
                        {
                            ::fast_io::io::print(output, " call-site "); add_source_path(text, frame.call_file);
                            ::fast_io::io::print(output, ":", ::fast_io::mnp::dec(frame.call_line),
                                ":", ::fast_io::mnp::dec(frame.call_column));
                        }
                        ::fast_io::io::print(output, ::fast_io::mnp::chvw('\n'));
                    }
                }
                else
                {
                    ::fast_io::io::print(output, "  inline metadata unavailable: ");
                    switch(thread.source_inline_reason)
                    {
                        case source_inline_unavailable_reason::none: ::fast_io::io::print(output, "no current metadata"); break;
                        case source_inline_unavailable_reason::no_bound_metadata: ::fast_io::io::print(output, "no embedded metadata bound to this source"); break;
                        case source_inline_unavailable_reason::invalid_metadata: ::fast_io::io::print(output, "invalid or unsupported embedded metadata"); break;
                        case source_inline_unavailable_reason::metadata_limit: ::fast_io::io::print(output, "metadata budget exceeded"); break;
                        case source_inline_unavailable_reason::no_current_frame: ::fast_io::io::print(output, "no matching current physical Wasm frame"); break;
                        case source_inline_unavailable_reason::stale_generation_or_stop: ::fast_io::io::print(output, "source generation or stopped position is no longer current"); break;
                        case source_inline_unavailable_reason::unmapped: ::fast_io::io::print(output, "current Code position is unmapped"); break;
                        case source_inline_unavailable_reason::ambiguous: ::fast_io::io::print(output, "overlapping inline metadata is ambiguous"); break;
                        case source_inline_unavailable_reason::allocation_failure: ::fast_io::io::print(output, "metadata allocation failed"); break;
                        case source_inline_unavailable_reason::native_stop: ::fast_io::io::print(output, "native trap has no current Wasm source position"); break;
                    }
                    ::fast_io::io::print(output, ::fast_io::mnp::chvw('\n'));
                }
                if(!thread.trace || thread.trace->frames().empty()) { ::fast_io::io::print(output, "  backtrace unavailable for this stop\n"); continue; }
                ::std::uint64_t index{};
                for(auto const& frame : thread.trace->frames())
                {
                    ::fast_io::io::print(output, "  #", ::fast_io::mnp::dec(index++), " module=", ::fast_io::mnp::dec(frame.module_id),
                                        " function=", ::fast_io::mnp::dec(frame.function_index), " [");
                    add_name(text, frame.module_name); ::fast_io::io::print(output, "] "); add_name(text, frame.function_name); ::fast_io::io::print(output, ::fast_io::mnp::chvw('\n'));
                }
                if(thread.trace->truncated()) { ::fast_io::io::print(output, "  backtrace truncated\n"); }
                ::fast_io::io::print(output, "  caller source lines and byte offsets unavailable\n");
            }
            return text;
        }
    }
    namespace details
    {
        // The CLI presents execution events. The authenticated text transport
        // keeps format_reply's existing machine-readable status/exit grammar.
        [[nodiscard]] inline ::fast_io::string format_cli_reply(controller_reply const& reply, console_command const& command)
        {
            auto message{console_messages::error(reply, command)};
            if(!message.empty()) { return message; }
            auto const raw{format_reply(reply, command)};
            if(reply.status == ::uwvm2::utils::control::error::none)
            {
                auto const view{::fast_io::string_view{raw.data(), raw.size()}};
                auto native{console_messages::native_view(reply, command, view)}; if(!native.empty()) { return native; }
                auto source{console_messages::source_values(reply, command, view)}; if(!source.empty()) { return source; }
                auto stack{console_messages::backtrace(reply, command, view)}; if(!stack.empty()) { return stack; }
            }
            if(reply.status == ::uwvm2::utils::control::error::none && raw.starts_with("source-frames stop="))
            { auto selected{console_messages::frame(reply, command)}; if(!selected.empty()) { return selected; } }
            if(command.kind == console_command_kind::wasm_script && reply.status == ::uwvm2::utils::control::error::none &&
               !raw.starts_with("error:") && reply.script_replies.size() == reply.script_commands.size() && reply.script_replies.size() <= 16u)
            {
                ::fast_io::string script{}; ::std::size_t shown{};
                for(::std::size_t i{}; i < reply.script_replies.size(); ++i)
                {
                    if(reply.script_commands[i].kind == console_command_kind::wasm_script)
                    { return ::fast_io::concat_fast_io("error: Recursive scripts are not allowed. Type \"help wasm-script\".\n"); }
                    auto heading{::fast_io::concat_fast_io("wasm-script command ", ::fast_io::mnp::dec(i + 1u),
                        " (", console_messages::topic(reply.script_commands[i]), "):\n")};
                    auto child{format_cli_reply(reply.script_replies[i], reply.script_commands[i])};
                    constexpr auto budget{maximum_formatted_reply_bytes - reply_trailer_reserve};
                    if(script.size() > budget || heading.size() > budget - script.size() || child.size() > budget - script.size() - heading.size()) { break; }
                    script.append(heading); script.append(child); ++shown;
                }
                if(shown != reply.script_replies.size())
                { script.append(::fast_io::concat_fast_io("wasm-script output-truncated shown=", ::fast_io::mnp::dec(shown),
                    " executed=", ::fast_io::mnp::dec(reply.script_replies.size()), " last-command-status=",
                    ::fast_io::mnp::dec(static_cast<unsigned>(reply.script_replies.back().status)), "\n")); }
                script.append("wasm-script end\n"); return script;
            }
            if(reply.status == ::uwvm2::utils::control::error::none && !reply.timed_out)
            {
                if(command.kind == console_command_kind::protocol && command.operation == ::uwvm2::utils::control::operation::resume &&
                   (raw.starts_with("running\n") || raw.starts_with("pause requested; executions are not all stopped\n")))
                { return {}; }
                if(reply.execution == execution_status::exited && raw.starts_with("guest exited: "))
                {
                    if(reply.guest_exit_code == 0) { return ::fast_io::concat_fast_io("Program exited normally.\n"); }
                    // GDB displays ordinary exit codes in octal, prefixed by 0.
                    if(reply.guest_exit_code > 0)
                    { return ::fast_io::concat_fast_io("Program exited with code 0", ::fast_io::mnp::oct(reply.guest_exit_code), ".\n"); }
                    return ::fast_io::concat_fast_io("Program exited with code ", ::fast_io::mnp::dec(reply.guest_exit_code), ".\n");
                }
            }
            bool const needs_hint{reply.status != ::uwvm2::utils::control::error::none ||
                (command.kind == console_command_kind::wasm_state && reply.wasm_state_values.result != wasm_state::status::available) ||
                (command.kind == console_command_kind::wasm_mutation && !reply.wasm_mutation_value.applied) ||
                (command.kind == console_command_kind::wasip1_state && reply.wasip1_state_values.result != wasip1_state::status::ok &&
                 !(command.wasip1_state_request.operation >= wasip1_state::action::trace_enable &&
                   command.wasip1_state_request.operation <= wasip1_state::action::trace_read))};
            return ::fast_io::concat_fast_io(::fast_io::string_view{raw.data(), raw.size()},
                needs_hint ? ::fast_io::concat_fast_io("Type \"help ", console_messages::topic(command), "\" for usage and requirements.\n") : ::fast_io::string{});
        }
        [[nodiscard]] inline ::fast_io::string format_program_start(::fast_io::string_view program)
        {
            if(program.empty()) { return ::fast_io::concat_fast_io("Starting program.\n"); }
            auto const safe{console_tui::clean(program)};
            ::fast_io::string label{};
            for(char c : safe) { if(c == '\n') { label.append("\\n"); } else { label.push_back(c); } }
            return ::fast_io::concat_fast_io("Starting program: ", label, "\n");
        }
    }
    namespace console_inspection
    {
        struct register_value { ::fast_io::string type{},value{};bool available{}; };
        // Copied, already qualified public register data only. Unknown bits
        // remain unknown even when the backing scalar/byte array contains data.
        [[nodiscard]] inline register_value register_preview(native_registers::display_snapshot const& state,::fast_io::string_view name)
        {
            if(name.starts_with("$")) { name=name.subview(1u); }if(name.empty() || name.size()>31u) { return {}; }
            ::std::u8string token{};for(unsigned char c:name) { if(c>=128u) { return {}; }token.push_back(static_cast<char8_t>(c)); }
            unsigned requested{};
            if(state.machine==native_registers::architecture::aarch64)
            {
                if(token==u8"wzr" || token==u8"xzr") { return {::fast_io::concat_fast_io(token.front()==u8'w' ? ::fast_io::string_view{"u32"} : ::fast_io::string_view{"u64"}),::fast_io::concat_fast_io("0x0"),true}; }
                if(token==u8"wsp") { token=u8"sp";requested=32u; }
                else if(token.size()>1u && token.front()==u8'w' && token[1]>=u8'0' && token[1]<=u8'9') { token[0]=u8'x';requested=32u; }
            }
            if(state.machine==native_registers::architecture::x86_64)
            {
                constexpr ::std::u8string_view small[]{u8"eax",u8"ebx",u8"ecx",u8"edx",u8"esi",u8"edi",u8"ebp",u8"esp",u8"r8d",u8"r9d",u8"r10d",u8"r11d",u8"r12d",u8"r13d",u8"r14d",u8"r15d"};
                for(::std::size_t i{};i<16u;++i) { if(token==small[i]) { token=native_registers::name(state.machine,i);requested=32u;break; } }
            }
            auto const index{native_registers::index_of(state.machine,token)};
            if(index<state.size())
            {
                auto known{state.known_bits[index]};auto width{requested ? requested : state.word_bits};
                if((known!=32u && known!=64u) || (width!=32u && width!=64u) || known>state.word_bits) { return {}; }
                register_value result{::fast_io::concat_fast_io("u",width),::fast_io::concat_fast_io("0x"),true};
                for(unsigned byte{width/8u};byte;--byte) { if(byte>known/8u) { result.value.append("??"); }
                    else { result.value.append(::fast_io::concat_fast_io(::fast_io::mnp::hex<false,true>(static_cast<::std::uint8_t>(state.values[index]>>((byte-1u)*8u))))); } }
                return result;
            }
            unsigned float_width{};
            if(state.machine==native_registers::architecture::aarch64 && token.size()>1u && token[1]>=u8'0' && token[1]<=u8'9')
            { if(token[0]==u8's') { float_width=4u;token[0]=u8'v'; }else if(token[0]==u8'd') { float_width=8u;token[0]=u8'v'; }else if(token[0]==u8'q') { token[0]=u8'v'; } }
            auto const fp{native_registers::fp_index_of(state.machine,token)};
            if(!state.floating.available || fp>=native_registers::max_fp_registers) { return {}; }
            auto const& value{state.floating.values[fp]};auto width{float_width ? float_width : native_registers::fp_width(state.machine,fp)};
            if(value.width==0u || value.width>value.bytes.size() || width==0u || width>value.bytes.size()) { return {}; }
            if(float_width && value.width>=float_width)
            { ::std::uint64_t bits{};for(unsigned i{};i<float_width;++i) { bits|=static_cast<::std::uint64_t>(value.bytes[i])<<(i*8u); }
              return {::fast_io::concat_fast_io("f",float_width*8u),float_width==4u ? ::fast_io::concat_fast_io(::std::bit_cast<float>(static_cast<::std::uint32_t>(bits))) : ::fast_io::concat_fast_io(::std::bit_cast<double>(bits)),true}; }
            register_value result{::fast_io::concat_fast_io("bits",width*8u),::fast_io::concat_fast_io("0x"),true};
            for(auto byte{width};byte;--byte) { if(byte>value.width) { result.value.append("??"); }
                else { result.value.append(::fast_io::concat_fast_io(::fast_io::mnp::hex<false,true>(value.bytes[byte-1u]))); } }
            return result;
        }
        // Consume only the qualified public snapshot for this exact stop. PC
        // comes first so a small pane cannot hide all of its available values.
        template<typename Format>
        [[nodiscard]] inline ::fast_io::string register_panel(controller_reply const& reply,::std::uint64_t thread,
            ::std::uint64_t stop,::std::uint64_t pc,Format&& format)
        {
            auto const& state{reply.registers};auto const pc_index{native_registers::pc_index(state.machine)};
            if(!pc) { return ::fast_io::concat_fast_io("Wasm stop; use si or ni for native registers\n"); }
            if(reply.status!=::uwvm2::utils::control::error::none || !stop || reply.registers_stop_identifier!=stop ||
                reply.disassembly_code.participant!=thread || reply.disassembly_code.pc!=pc || state.pc()!=pc ||
                (state.word_bits!=32u && state.word_bits!=64u) || pc_index>=state.size() || state.known_bits[pc_index]!=state.word_bits)
            { return ::fast_io::concat_fast_io("Registers unavailable at this native stop\n"); }
            ::fast_io::string text{};::std::size_t count{};
            auto append{[&](char8_t const* label)
            {
                if(!label) { return; }
                auto name{::fast_io::concat_fast_io(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(label)))};
                auto value{register_preview(state,name.subview(0u))};if(!value.available) { return; }
                text.append(format(name.subview(0u),value.type.subview(0u),value.value.subview(0u),true));++count;
            }};
            append(native_registers::name(state.machine,pc_index));
            for(::std::size_t i{};i<state.size();++i) { if(i!=pc_index) { append(native_registers::name(state.machine,i)); } }
            for(::std::size_t i{};i<native_registers::max_fp_registers;++i) { append(native_registers::fp_name(state.machine,i)); }
            if(count==1u) { text.append("No other mapped values at this instruction\n"); }
            return text;
        }
    }
    // One normal HOST management session. UI/completion receives copied reply
    // DATA only. Every source/native/Wasm query still goes through execute().
    class console_visual_session final
    {
        controller& control_;
        console_io io_;
        console_tui::screen& screen_;
        console_completion::candidates candidates_{};
        ::std::uint64_t painted_stop_{};
        execution_status painted_execution_{execution_status::closed};
        bool refresh_needed_{true},deferred_mouse_frame_{};
        ::std::chrono::steady_clock::time_point mouse_batch_start_{};
        ::std::size_t mouse_batch_count_{};
        [[nodiscard]] controller_reply query(::fast_io::string_view input)
        { return control_.execute(parse_console_command(input)); }
        void panel_reply(console_tui::layout which, ::fast_io::string const& input) { panel_reply(which,input.subview(0u)); }
        void panel_reply(console_tui::layout which, ::fast_io::string_view input)
        {
            auto const command{parse_console_command(input)};
            auto const reply{control_.execute(command)};
            auto const text{details::format_reply(reply,command)};
            if(which==console_tui::layout::memory && reply.status==::uwvm2::utils::control::error::none)
            {
                ::std::uint64_t module{},offset{};::std::uint32_t memory{},reserved{};
                ::uwvm2::utils::control::input_buffer fields{command.payload};
                ::fast_io::io::scan(fields,::fast_io::mnp::le_get<64>(module),::fast_io::mnp::le_get<32>(memory),::fast_io::mnp::le_get<32>(reserved),::fast_io::mnp::le_get<64>(offset));
                screen_.memory_snapshot(reply.memory,offset);
            }
            if(which==console_tui::layout::memory && reply.status!=::uwvm2::utils::control::error::none) { screen_.memory_snapshot({},0u); }
            if(which == console_tui::layout::assembly && reply.status == ::uwvm2::utils::control::error::none &&
               reply.disassembly_stop_identifier != 0u && reply.disassembly_count <= reply.disassembly.size())
            {
                auto listing{::fast_io::concat_fast_io("JIT module=", reply.disassembly_code.module,
                    " function=", reply.disassembly_code.function, "\n")};
                native_owned_instruction_semantics::decoder decoder{reply.disassembly_code.target};
                for(::std::size_t i{}; i != reply.disassembly_count; ++i)
                {
                    auto const& instruction{reply.disassembly[i]};
                    if(instruction.pc < reply.disassembly_owner_begin || instruction.pc >= reply.disassembly_owner_end) { continue; }
                    auto const marker{instruction.pc == reply.disassembly_code.pc ? "=> " : "   "};
                    listing.append(::fast_io::concat_fast_io(::fast_io::mnp::os_c_str(marker),
                        ::fast_io::mnp::hex0x(instruction.pc), "  "));
                    if(instruction && native_branch_display::displayable(decoder, instruction, reply.disassembly_destinations[i],
                        reply.disassembly_owner_begin, reply.disassembly_owner_end, true))
                    { listing.append(console_tui::clean(::fast_io::string_view{instruction.text.data(),
                        static_cast<::std::size_t>(::std::find(instruction.text.begin(), instruction.text.end(), '\0') - instruction.text.begin())})); }
                    else { listing.append("<runtime boundary; instruction unavailable>"); }
                    listing.push_back('\n');
                }
                screen_.set_panel(which, listing);
            }
            else { screen_.set_panel(which,::fast_io::string_view{text.data(),text.size()}); }
        }
        void assembly_panel(::std::uint64_t thread,::std::uint64_t stop)
        {
            auto const read{[&](::std::int64_t first,::std::size_t count)
            { return query(::fast_io::concat_fast_io("disassemble-range ",thread," ",stop," ",count," 0 ",first," 0").subview(0u)); }};
            auto const valid{[&](controller_reply const& reply)
            { return reply.status==::uwvm2::utils::control::error::none && reply.disassembly_stop_identifier==stop &&
                reply.disassembly_count && reply.disassembly_count<=reply.disassembly.size(); }};
            auto const occupied{[](controller_reply const& reply)
            { return reply.disassembly[0].pc>=reply.disassembly_owner_begin && reply.disassembly[0].pc<reply.disassembly_owner_end && reply.disassembly[0].pc!=0u; }};
            auto first{screen_.assembly_first()};auto probe{read(first,1u)};
            if(!valid(probe)) { screen_.set_panel(console_tui::layout::assembly,"JIT instructions unavailable at this stop\n");return; }
            if(!occupied(probe))
            {
                // Search between the last real displayed row and the requested
                // page. This clamps large scrolls to the first/last instruction,
                // rather than presenting an endless sequence of empty pages.
                auto inside{screen_.assembly_valid_first()},outside{first};auto anchor{read(inside,1u)};
                if(!valid(anchor) || !occupied(anchor)) { return; }
                while(inside>outside ? inside-outside>1 : outside-inside>1)
                {
                    auto const middle{inside+(outside-inside)/2};auto candidate{read(middle,1u)};
                    if(!valid(candidate)) { return; }
                    if(occupied(candidate)) { inside=middle; }else { outside=middle; }
                }
                screen_.assembly_bound(first<inside,inside);first=inside;probe=read(first,1u);
                if(!valid(probe) || !occupied(probe)) { return; }
            }
            auto const symbol{control_.wasm_function_label(probe.disassembly_code.module,probe.disassembly_code.function)};
            auto listing{::fast_io::concat_fast_io(symbol.empty()?::fast_io::string_view{"JIT"}:symbol.subview(0u),
                " [",probe.disassembly_code.module,":",probe.disassembly_code.function,"]\n")};
            auto const body{screen_.pane_body_rows(console_tui::layout::assembly)};
            auto const count{body>1u ? body-1u : 1u};bool end{};
            native_owned_instruction_semantics::decoder decoder{probe.disassembly_code.target};
            for(::std::size_t shown{};shown<count && !end;)
            {
                auto const requested{::std::min(count-shown,::std::size_t{32u})};
                auto const page{read(first+static_cast<::std::int64_t>(shown),requested)};
                if(!valid(page) || page.disassembly_owner_begin!=probe.disassembly_owner_begin ||
                    page.disassembly_owner_end!=probe.disassembly_owner_end || page.disassembly_code.pc!=probe.disassembly_code.pc)
                { return; }
                for(::std::size_t i{};i<page.disassembly_count;++i)
                {
                    auto const& instruction{page.disassembly[i]};
                    if(instruction.pc<page.disassembly_owner_begin || instruction.pc>=page.disassembly_owner_end)
                    { screen_.assembly_bound(false,first+static_cast<::std::int64_t>(shown+i)-1);end=true;break; }
                    // Both a native trap PC and the authenticated cooperative
                    // JIT return edge are the selected frame's exact resume PC.
                    // This display marker never grants native register/jump access.
                    auto const marker{instruction.pc==page.disassembly_code.pc ? "=> " : "   "};
                    listing.append(::fast_io::concat_fast_io(::fast_io::mnp::os_c_str(marker),::fast_io::mnp::hex0x(instruction.pc),"  "));
                    if(instruction && native_branch_display::displayable(decoder,instruction,page.disassembly_destinations[i],page.disassembly_owner_begin,page.disassembly_owner_end,true))
                    { listing.append(console_tui::clean(::fast_io::string_view{instruction.text.data(),
                        static_cast<::std::size_t>(::std::find(instruction.text.begin(),instruction.text.end(),'\0')-instruction.text.begin())})); }
                    else { listing.append("<instruction unavailable>"); }
                    listing.push_back('\n');
                }
                shown+=requested;
            }
            screen_.assembly_position(first);screen_.set_panel(console_tui::layout::assembly,listing);
        }
        [[nodiscard]] static ::fast_io::string inspection_line(::fast_io::string_view name,::fast_io::string_view type,::fast_io::string_view value,bool available=true)
        {
            auto const clean_name{console_tui::clean(name,160u)},clean_type{console_tui::clean(type,120u)},clean_value{console_tui::clean(value,320u)};
            bool color{::uwvm2::uwvm::utils::ansies::put_color};using view=::fast_io::string_view;
            ::std::size_t indent{};while(indent<clean_name.size() && clean_name[indent]==' ') { ++indent; }
            return ::fast_io::concat_fast_io(clean_name.subview(0u,indent),color ? view{"\x1b[94m"} : view{},clean_type,
                clean_type.empty() ? view{} : view{" "},color ? view{"\x1b[96m"} : view{},clean_name.subview(indent),
                color ? view{"\x1b[0m"} : view{}," = ",color ? (available ? view{"\x1b[93m"} : view{"\x1b[90m"}) : view{},clean_value,
                color ? view{"\x1b[0m"} : view{},"\n");
        }
        [[nodiscard]] static ::fast_io::string wasm_value_text(wasm_state::value const& value)
        {
            if(!value.available) { return ::fast_io::concat_fast_io("unavailable"); }
            if(value.type.kind==wasm_state::value_kind::f32)
            { ::std::uint32_t bits{};if(wasm_state::details::read_numeric<32u>(value,bits)) { return ::fast_io::concat_fast_io(::std::bit_cast<float>(bits)); } }
            if(value.type.kind==wasm_state::value_kind::f64)
            { ::std::uint64_t bits{};if(wasm_state::details::read_numeric<64u>(value,bits)) { return ::fast_io::concat_fast_io(::std::bit_cast<double>(bits)); } }
            ::std::string row{};wasm_state::details::print_value(::fast_io::ostring_ref_std{::std::addressof(row)},value);
            auto separator{row.find(" = ")};return ::fast_io::concat_fast_io(::std::string_view{row}.substr(separator==row.npos?0u:separator+3u));
        }
        void wasm_values_panel(console_tui::layout pane,::std::uint64_t thread,bool native)
        {
            auto reply{query(::fast_io::concat_fast_io(pane==console_tui::layout::locals?::fast_io::string_view{"locals wasm "} : ::fast_io::string_view{"operands "},
                thread," 0 0 64").subview(0u))};
            auto const& values{reply.wasm_state_values};::fast_io::string text{};
            if(reply.status!=::uwvm2::utils::control::error::none || !wasm_state::valid(values) || values.result!=wasm_state::status::available)
            { text=::fast_io::concat_fast_io("Unavailable at this stop\n"); }
            else
            {
                if(native) { text.append("Snapshot at last Wasm boundary\n"); }
                for(auto const& row:values.rows)
                {
                    auto name{::fast_io::concat_fast_io("[",row.index,"]")};::std::string type{};
                    wasm_state::details::print_type(::fast_io::ostring_ref_std{::std::addressof(type)},row.data.type);
                    auto value{wasm_value_text(row.data)};
                    text.append(inspection_line(name.subview(0u),::fast_io::string_view{type.data(),type.size()},value.subview(0u),row.data.available));
                }
                if(values.rows.empty()) { text.append(pane==console_tui::layout::locals ? ::fast_io::string_view{"No locals\n"} : ::fast_io::string_view{"Empty stack\n"}); }
                if(values.rows.size()<values.total_values)
                { text.append(::fast_io::concat_fast_io(values.rows.size()," / ",values.total_values," values; use ",
                    pane==console_tui::layout::locals ? ::fast_io::string_view{"locals wasm"} : ::fast_io::string_view{"operands"}," for more\n")); }
            }
            screen_.set_value_panel(pane,text.subview(0u));
        }
        [[nodiscard]] static ::fast_io::string scalar_text(source_dwarf::numeric_kind kind,::std::uint64_t bits,unsigned bytes)
        {
            switch(kind)
            {
                case source_dwarf::numeric_kind::boolean: return ::fast_io::concat_fast_io(bits ? ::fast_io::string_view{"true"} : ::fast_io::string_view{"false"});
                case source_dwarf::numeric_kind::signed_integer:
                { if(bytes!=1u && bytes!=2u && bytes!=4u && bytes!=8u) { return ::fast_io::concat_fast_io("unavailable"); }
                  source_dwarf::numeric_variable value{};value.bits=bits;value.byte_count=static_cast<::std::uint8_t>(bytes);return ::fast_io::concat_fast_io(source_dwarf::numeric_signed_value(value)); }
                case source_dwarf::numeric_kind::unsigned_integer: return ::fast_io::concat_fast_io(bits);
                case source_dwarf::numeric_kind::f32_bits:return ::fast_io::concat_fast_io(::std::bit_cast<float>(static_cast<::std::uint32_t>(bits)));
                case source_dwarf::numeric_kind::f64_bits:return ::fast_io::concat_fast_io(::std::bit_cast<double>(bits));
                default:return ::fast_io::concat_fast_io("unavailable");
            }
        }
        [[nodiscard]] ::fast_io::string inspect_value(::fast_io::string_view input,bool hover=false)
        {
            auto line{[&](::fast_io::string_view name,::fast_io::string_view type,::fast_io::string_view value,bool available=true) { return hover && !available ? ::fast_io::string{} : inspection_line(name,type,value,available); }};
            auto const level{console_aliases::split(input)};auto const part{console_aliases::split(level.tail)};
            auto actual{control_.inspect()};
            if(actual.execution!=execution_status::stopped || actual.threads.size()!=1u)
            { return hover ? ::fast_io::string{} : ::fast_io::concat_fast_io("value unavailable: requires one stopped thread\n"); }
            auto const& thread{actual.threads.front()};::fast_io::string request{};
            if(level.word=="src" && !level.tail.empty()) { request=::fast_io::concat_fast_io("print ",level.tail); }
            else if(level.word=="wasm" && (part.word=="local" || part.word=="stack"))
            { ::std::uint64_t index{};auto token{part.tail};auto p{::fast_io::parse_by_scan(token.begin(),token.end(),::fast_io::mnp::dec_get<true,true>(index))};
              if(token.empty() || p.code!=::fast_io::parse_code::ok || p.iter!=token.end()) { return ::fast_io::concat_fast_io("error: inspect wasm local|stack INDEX\n"); }
              request=::fast_io::concat_fast_io(part.word=="local" ? ::fast_io::string_view{"locals wasm "} : ::fast_io::string_view{"operands "},thread.identifier," 0 ",index," 1"); }
            else if(level.word=="asm" && !part.word.empty() && part.tail.empty())
            { request=::fast_io::concat_fast_io("info registers ",thread.identifier," ",actual.stop_identifier); }
            else { return ::fast_io::concat_fast_io("error: inspect src EXPR | inspect wasm local|stack INDEX | inspect asm REGISTER\n"); }
            auto command{parse_console_command(request.subview(0u))};auto reply{control_.execute(command)};
            if(reply.status!=::uwvm2::utils::control::error::none)
            { return line(level.tail,{},"unavailable at this stop",false); }
            if(level.word=="src")
            {
                if(reply.source_stop_identifier!=actual.stop_identifier) { return line(level.tail,{},"unavailable at this stop",false); }
                if(reply.source_object_value_available && !reply.source_object_type.empty() && reply.source_object_type.size()<=1024u)
                {
                    ::fast_io::string text{};unsigned shown{};
                    for(auto const& node:reply.source_object_type)
                    {
                        if(node.depth>1u) { continue; }if(shown==8u) { text.append("…\n");break; }++shown;
                        ::fast_io::string value{};bool available{node.value_available || node.display_text_available};
                        if(node.display_text_available) { value=::fast_io::concat_fast_io("\"",::std::string_view{node.display_text},node.display_text_truncated ? ::fast_io::string_view{"…\""} : ::fast_io::string_view{"\""}); }
                        else if(node.reason!=source_dwarf::object_unavailable_reason::none) { value=::fast_io::concat_fast_io("unavailable");available=false; }
                        else if(node.value_available && node.kind==source_dwarf::type_kind::pointer) { value=::fast_io::concat_fast_io(::fast_io::mnp::hex0x(node.bits)); }
                        else if(node.value_available) { value=scalar_text(node.scalar_kind,node.bits,node.scalar_bytes);available=value.subview(0u)!="unavailable"; }
                        else { value=::fast_io::concat_fast_io("{…}");available=true; }
                        auto name{node.depth==0u ? ::fast_io::concat_fast_io(level.tail) : ::fast_io::concat_fast_io("  ",::std::string_view{node.name})};
                        text.append(line(name.subview(0u),::fast_io::string_view{node.type_name.data(),node.type_name.size()},value.subview(0u),available));
                    }
                    return text;
                }
                if(reply.source_locals_available && !reply.source_locals.empty())
                { auto const& value{reply.source_locals.front()};auto text{scalar_text(value.kind,value.bits,value.byte_count)};
                  return line(level.tail,::fast_io::string_view{value.type_name.data(),value.type_name.size()},text.subview(0u),value.kind!=source_dwarf::numeric_kind::unavailable); }
                return line(level.tail,{},"unavailable at this stop",false);
            }
            if(level.word=="wasm")
            {
                auto const& values{reply.wasm_state_values};auto name{::fast_io::concat_fast_io(part.word,"[",part.tail,"]")};
                if(!wasm_state::valid(values) || values.result!=wasm_state::status::available || values.rows.size()!=1u)
                { return line(name.subview(0u),{},"unavailable at this stop",false); }
                auto const& value{values.rows.front().data};::std::string type{};auto out{::fast_io::ostring_ref_std{::std::addressof(type)}};wasm_state::details::print_type(out,value.type);
                ::fast_io::string formatted{};
                if(!value.available) { formatted=::fast_io::concat_fast_io("unavailable"); }
                else if(value.type.kind==wasm_state::value_kind::i32 || value.type.kind==wasm_state::value_kind::f32)
                { ::std::uint32_t bits{};if(!wasm_state::details::read_numeric<32u>(value,bits)) { return line(name.subview(0u),{},"unavailable",false); }
                  formatted=scalar_text(value.type.kind==wasm_state::value_kind::i32 ? source_dwarf::numeric_kind::signed_integer : source_dwarf::numeric_kind::f32_bits,bits,4u); }
                else if(value.type.kind==wasm_state::value_kind::i64 || value.type.kind==wasm_state::value_kind::f64)
                { ::std::uint64_t bits{};if(!wasm_state::details::read_numeric<64u>(value,bits)) { return line(name.subview(0u),{},"unavailable",false); }
                  formatted=scalar_text(value.type.kind==wasm_state::value_kind::i64 ? source_dwarf::numeric_kind::signed_integer : source_dwarf::numeric_kind::f64_bits,bits,8u); }
                else
                { ::std::string row{};wasm_state::details::print_value(::fast_io::ostring_ref_std{::std::addressof(row)},value);
                  auto separator{row.find(" = ")};formatted=::fast_io::concat_fast_io(::std::string_view{row}.substr(separator==row.npos ? 0u : separator+3u)); }
                return line(name.subview(0u),::fast_io::string_view{type.data(),type.size()},formatted.subview(0u),value.available);
            }
            if(reply.registers_stop_identifier!=actual.stop_identifier) { return line(part.word,{},"unavailable at this stop",false); }
            auto value{console_inspection::register_preview(reply.registers,part.word)};
            return value.available ? line(part.word,value.type.subview(0u),value.value.subview(0u)) : line(part.word,{},"unavailable",false);
        }
    public:
        console_visual_session(controller& control, console_io io, console_tui::screen& screen)
            : control_{control}, io_{io}, screen_{screen} {}
        void output(::fast_io::string_view text)
        {
            if(io_.interactive && text.starts_with("error:"))
            { screen_.output(console_help::format_error(text, true, console_tui::terminal_dimensions().columns)); }
            else { screen_.output(text); }
        }
        [[nodiscard]] bool enabled() const noexcept { return screen_.enabled(); }
        void disable() noexcept { screen_.disable(); }
        void output(::fast_io::string const& text) { output(text.subview(0u)); }
        [[nodiscard]] bool command(::fast_io::string_view input)
        {
            auto parts{console_aliases::split(input)};
            if(parts.word=="inspect") { output(inspect_value(parts.tail));return true; }
            if(parts.word == "list")
            {
                auto level{console_aliases::split(parts.tail)};
                if(!level.tail.empty() || (level.word!="src" && level.word!="wasm" && level.word!="asm"))
                { output("error: list src|wasm|asm\n"); return true; }
                auto const actual{control_.inspect()};
                if(actual.execution!=execution_status::stopped || actual.threads.size()!=1u)
                { output("error: list requires one stopped thread\n"); return true; }
                auto const& thread{actual.threads.front()};
                if(level.word=="wasm") { output(control_.wasm_instruction_listing(thread.identifier,actual.stop_identifier)); }
                else if(level.word=="asm")
                {
                    auto const command{parse_console_command(::fast_io::concat_fast_io("disassemble ",thread.identifier," ",actual.stop_identifier," 16").subview(0u))};
                    auto const text{details::format_reply(control_.execute(command),command)};
                    output(::fast_io::string_view{text.data(),text.size()});
                }
                else if(thread.source)
                {
                    auto const& source{*thread.source};auto const path{screen_.source_override().empty() ?
                        ::fast_io::string_view{source.file.data(),source.file.size()} : screen_.source_override()};
                    screen_.prepare_source(path,source.line,actual.stop_identifier); ::std::uint64_t total{};
                    output(console_tui::source_text(path,source.line,nullptr,!actual.source_statement_stop,
                        screen_.source_first_line(),::std::addressof(total))); screen_.source_bounds(total);
                }
                else { output("source location unavailable at this stop\n"); }
                return true;
            }
            bool const handled{screen_.command(input)}; if(handled) { apply_pointer_query(); refresh_needed_=true; } return handled;
        }
        void apply_pointer_query()
        {
            auto const hover{screen_.take_hover_query()};
            if(!hover.empty()) { auto const text{inspect_value(hover.subview(0u),true)};screen_.hover_reply(text.subview(0u)); }
            auto const query{screen_.take_query()}; if(query.empty()) { return; }
            refresh_needed_=true;
            // Mouse and typed `info wasip1 fd` use exactly the same authenticated query.
            auto const command{parse_console_command(query.subview(0u))};
            auto const reply{control_.execute(command)}; auto const text{details::format_reply(reply,command)};
            if(command.operation==::uwvm2::utils::control::operation::read_memory)
            { if(reply.status==::uwvm2::utils::control::error::none) { ::std::uint64_t module{},offset{};::std::uint32_t memory{},reserved{};::uwvm2::utils::control::input_buffer fields{command.payload};
                ::fast_io::io::scan(fields,::fast_io::mnp::le_get<64>(module),::fast_io::mnp::le_get<32>(memory),::fast_io::mnp::le_get<32>(reserved),::fast_io::mnp::le_get<64>(offset));screen_.memory_snapshot(reply.memory,offset); }
              else { screen_.memory_snapshot({},0u); } }
            screen_.output(::fast_io::string_view{text.data(),text.size()});
        }
        [[nodiscard]] bool function_breakpoint(::fast_io::string_view input, console_command& result)
        {
            auto const request{console_completion::context(input,input.size())};
            if(request.type != console_completion::kind::function || request.prefix.empty()) { return false; }
            // Numeric breakwasm MODULE FUNCTION BYTE_OFFSET retains its exact grammar.
            if(parse_console_command(input).kind != console_command_kind::invalid) { return false; }
            ::std::uint64_t function{},offset{}; bool found{},ambiguous{};
            bool const exhaustive{control_.visit_source_function_symbols(request.module,request.prefix,
                [&](::fast_io::string_view name,::std::uint64_t f,::std::uint64_t at)
                {
                    if(name != request.prefix) { return true; }
                    if(found && (function != f || offset != at)) { ambiguous=true; }
                    else { function=f;offset=at;found=true; } return true;
                })};
            if(!found || ambiguous || !exhaustive)
            { output("error: function symbol unavailable or ambiguous; use breakwasm MODULE FUNCTION BYTE_OFFSET\n"); return true; }
            result=parse_console_command(::fast_io::concat_fast_io("breakwasm ",::fast_io::mnp::dec(request.module)," ",
                ::fast_io::mnp::dec(function)," ",::fast_io::mnp::dec(offset)).subview(0u)); return true;
        }
        void submitted(::fast_io::string_view input)
        { screen_.record(::fast_io::concat_fast_io("(uwvm-debug) ",input,"\n")); refresh_needed_=true; }
        void refresh()
        {
            screen_.drain_guest();
            if(!screen_.enabled()) { return; }
            auto const actual{control_.inspect()};
            if(!refresh_needed_ && actual.stop_identifier == painted_stop_ && actual.execution == painted_execution_) { return; }
            refresh_needed_=false; painted_stop_=actual.stop_identifier; painted_execution_=actual.execution;
            screen_.observe_stop(actual.execution==execution_status::stopped ? actual.stop_identifier : 0u,actual.threads.empty() ? 0u : actual.threads.front().location.code_generation);
            screen_.clear_panels();
            auto const phase{actual.execution == execution_status::stopped ? ::fast_io::string_view{"stopped"} :
                actual.execution == execution_status::running ? ::fast_io::string_view{"running"} :
                actual.execution == execution_status::stopping ? ::fast_io::string_view{"stopping"} : ::fast_io::string_view{"exited/closed"}};
            screen_.set_status(::fast_io::concat_fast_io(phase," | stop=",::fast_io::mnp::dec(actual.stop_identifier),
                " | threads=",::fast_io::mnp::dec(actual.threads.size())));
            if(actual.execution != execution_status::stopped || actual.stop_identifier == 0u || actual.threads.size() != 1u)
            {
                for(auto layout : {console_tui::layout::src,console_tui::layout::assembly,console_tui::layout::registers,
                    console_tui::layout::wasm,console_tui::layout::wasip1,console_tui::layout::locals,console_tui::layout::operands,console_tui::layout::memory})
                { screen_.set_panel(layout,"view unavailable: requires one currently stopped Wasm thread\n"); }
                return;
            }
            auto const& thread{actual.threads.front()}; auto const id{thread.identifier};

            auto const prefix{::fast_io::concat_fast_io("thread=",::fast_io::mnp::dec(id)," module=",
                ::fast_io::mnp::dec(thread.location.code_unit)," function=",::fast_io::mnp::dec(thread.location.function),
                " wasm-offset=",::fast_io::mnp::dec(thread.location.offset),"\n")};
            if(screen_.wants(console_tui::layout::src))
            {
                auto const command{parse_console_command("frame")};
                auto const frames{thread.native_pc ? controller_reply{} : control_.execute(command)};
                auto source{::fast_io::string{}};
                console_source_highlight::line_styles source_styles{};
                if(thread.source && (!frames.source_frames_available || frames.selected_source_frame == 0u))
                {
                    auto const& at{*thread.source};
                    source.append(::fast_io::concat_fast_io(console_tui::clean(::fast_io::string_view{at.file.data(),at.file.size()}),":",
                        ::fast_io::mnp::dec(at.line),"\n"));
                    auto const path{screen_.source_override().empty() ? ::fast_io::string_view{at.file.data(),at.file.size()} : screen_.source_override()};
                    screen_.prepare_source(path,at.line,actual.stop_identifier); ::std::uint64_t source_lines{};
                    source.append(console_tui::source_text(path,at.line,::std::addressof(source_styles),!actual.source_statement_stop,
                        screen_.source_first_line(),::std::addressof(source_lines))); screen_.source_bounds(source_lines);
                }
                else { source.append("source text unavailable at this selected stop/frame\n"); }
                if(!thread.source) { source.append(prefix); }
                screen_.set_panel(console_tui::layout::src,source);
                screen_.set_source_styles(::std::move(source_styles),1u);
            }
            if(screen_.wants(console_tui::layout::assembly))
            { assembly_panel(id,actual.stop_identifier); }
            if(screen_.wants(console_tui::layout::registers))
            { auto reply{thread.native_pc ? query(::fast_io::concat_fast_io("info registers ",id," ",actual.stop_identifier).subview(0u)) : controller_reply{}};
              auto text{console_inspection::register_panel(reply,id,actual.stop_identifier,thread.native_pc.value_or(0u),inspection_line)};
              screen_.set_value_panel(console_tui::layout::registers,text.subview(0u)); }
            if(screen_.wants(console_tui::layout::wasm))
            { screen_.set_panel(console_tui::layout::wasm,control_.wasm_instruction_listing(id,actual.stop_identifier)); }
            if(screen_.wants(console_tui::layout::memory))
            { if(screen_.memory_query().empty()) { panel_reply(console_tui::layout::memory,::fast_io::concat_fast_io("memory ",thread.location.code_unit," 0 0 128")); }
              else { panel_reply(console_tui::layout::memory,screen_.memory_query()); } }
            if(screen_.wants(console_tui::layout::locals)) { wasm_values_panel(console_tui::layout::locals,id,thread.native_pc.has_value()); }
            if(screen_.wants(console_tui::layout::operands)) { wasm_values_panel(console_tui::layout::operands,id,thread.native_pc.has_value()); }
            screen_.wasip1_module(thread.location.code_unit);
            if(screen_.wants(console_tui::layout::wasip1))
            { if(screen_.fd_query().empty()) { panel_reply(console_tui::layout::wasip1,::fast_io::concat_fast_io("info wasip1 fds ",::fast_io::mnp::dec(thread.location.code_unit)," 0 64")); }
              else { panel_reply(console_tui::layout::wasip1,screen_.fd_query()); } }
        }
        [[nodiscard]] console_editing::action complete(console_editing::editor& editor)
        {
            candidates_={};
            auto const request{console_completion::context(editor.line().view(),editor.cursor())};
            if(request.type == console_completion::kind::pane)
            { if(request.root=="direction")
              { for(auto name:{::fast_io::string_view{"left"},::fast_io::string_view{"right"},::fast_io::string_view{"above"},::fast_io::string_view{"below"}}) { static_cast<void>(candidates_.add(name,request.prefix)); } }
              else { for(auto name : console_tui::layout_names) { if(name!="split") { static_cast<void>(candidates_.add(name,request.prefix)); } }
                for(auto name:{::fast_io::string_view{"stack"},::fast_io::string_view{"register"},::fast_io::string_view{"registers"}})
                { static_cast<void>(candidates_.add(name,request.prefix)); } } }
            else if(request.type == console_completion::kind::module)
            {
                for(::std::uint64_t page{1u}; page<=32u; ++page)
                {
                    auto const modules{control_.source_module_page(page,8u)};
                    if(!modules.valid) { break; }
                    for(::std::size_t i{}; i<modules.size; ++i)
                    { auto name{::fast_io::concat_fast_io(::fast_io::mnp::dec(modules.rows[i].identifier))}; static_cast<void>(candidates_.add(name.subview(0u),request.prefix)); }
                    if(page>=modules.pages) { break; }
                    if(page==32u) { candidates_.truncated=true; }
                }
            }
            else if(request.type == console_completion::kind::path) { console_completion::paths(request,candidates_); }
            else if(request.type == console_completion::kind::function)
            {
                candidates_.truncated=!control_.visit_source_function_symbols(request.module,request.prefix,
                    [&](::fast_io::string_view name,::std::uint64_t,::std::uint64_t) { return candidates_.add(name,request.prefix); });
            }
            else if(request.type == console_completion::kind::symbol)
            {
                auto const actual{control_.inspect()};
                if(actual.execution != execution_status::stopped || actual.stop_identifier == 0u)
                { return console_editing::action::unchanged; }
                auto command{parse_console_command(::fast_io::concat_fast_io(request.explicit_frame ? ::fast_io::string_view{"ptype-frame "} : ::fast_io::string_view{"ptype "},request.selectors,
                    request.indirect ? ::fast_io::string_view{"*"} : ::fast_io::string_view{},
                    request.root.empty() ? ::fast_io::string_view{"_"} : request.root).subview(0u))};
                if(request.root.empty())
                {
                    // Auto-selection goes through the authenticated frame query;
                    // it cannot choose a thread by array ordinal.
                    if(command.requested_step_thread == 0u)
                    {
                        auto const frames{query("frame")};
                        if(!frames.source_frames_available || frames.source_frame_thread == 0u ||
                            frames.source_stop_identifier != actual.stop_identifier) { return console_editing::action::unchanged; }
                        command=parse_console_command(::fast_io::concat_fast_io("locals source ",::fast_io::mnp::dec(frames.source_frame_thread),
                            " ",::fast_io::mnp::dec(actual.stop_identifier)," ",::fast_io::mnp::dec(frames.selected_source_frame)).subview(0u));
                    }
                    else { command.kind=console_command_kind::source_locals; command.source_variable_name_size=0u; }
                }
                if(command.kind == console_command_kind::invalid) { return console_editing::action::unchanged; }
                auto const reply{control_.execute(command)};
                if(reply.status != ::uwvm2::utils::control::error::none || reply.source_stop_identifier != actual.stop_identifier)
                { return console_editing::action::unchanged; }
                if(request.root.empty() && reply.source_locals_available)
                {
                    for(auto const& variable : reply.source_locals)
                    { if(!candidates_.add(::fast_io::string_view{variable.name.data(),variable.name.size()},request.prefix)) { break; } }
                }
                else if(!request.root.empty() && reply.source_object_type_available)
                {
                    for(auto const& member : reply.source_object_type)
                    { if(member.depth == 1u && !candidates_.add(::fast_io::string_view{member.name.data(),member.name.size()},request.prefix)) { break; } }
                }
            }
            auto const action{candidates_.apply(editor,request)};
            if(candidates_.unavailable && action == console_editing::action::unchanged) { return console_editing::action::candidates; }
            return action;
        }
        void show_candidates(console_editing::editor const& editor,bool dynamic)
        {
            ::fast_io::string text{};
            if(!dynamic)
            { editor.display_candidates([&](::fast_io::string_view word) { if(!text.empty()) { text.push_back('\n'); } text.append(word); }); }
            else
            {
                if(candidates_.unavailable) { text.append("Path completion unavailable; edit the path and press Tab again."); }
                for(::std::size_t i{}; i < candidates_.size; ++i)
                { if(!text.empty()) { text.push_back('\n'); } text.append(candidates_.words[i]); }
                if(candidates_.truncated) { text.append("\n[completion truncated; type a longer prefix]"); }
            }
            screen_.show_candidates(text.subview(0u));
        }
        [[nodiscard]] bool draw(console_editing::editor const& editor, console_editing::action action)
        {
            bool const resize{screen_.needs_resize()};
            if(action==console_editing::action::idle && !resize && !deferred_mouse_frame_ && !screen_.drain_guest()) { return screen_.enabled(); }
            // Escape-sequence bytes do not change the editor or screen. In
            // particular, never redraw a full frame for each byte of SGR mouse.
            if(action==console_editing::action::unchanged && !resize && (!refresh_needed_ || deferred_mouse_frame_)) { return screen_.enabled(); }
            if(resize) { refresh_needed_=true; }
            auto const before{screen_.enabled()}; auto const previous{screen_.current_layout()}; screen_.key(action);
            if(action==console_editing::action::mouse) { screen_.mouse(editor.mouse()); apply_pointer_query(); }
            if(before != screen_.enabled() || previous != screen_.current_layout() || action == console_editing::action::redraw ||
               screen_.take_content_change() || action==console_editing::action::page_up || action==console_editing::action::page_down)
            { refresh_needed_=true; }
            if(action==console_editing::action::mouse && (editor.mouse().button&64u)!=0u && screen_.enabled())
            {
                auto const now{::std::chrono::steady_clock::now()};
                if(!deferred_mouse_frame_) { mouse_batch_start_=now;mouse_batch_count_=0u; }
                ++mouse_batch_count_;
                if(io_.input_ready!=nullptr && io_.input_ready(io_.context) && mouse_batch_count_<256u &&
                    now-mouse_batch_start_<::std::chrono::milliseconds{40})
                { deferred_mouse_frame_=true;return true; }
            }
            deferred_mouse_frame_=false;
            if(before && !screen_.enabled())
            {
                auto const rendered{console_tui::command_line(editor, console_tui::terminal_dimensions().columns)};
                io_.write(io_.context, rendered.subview(0u)); return true;
            }
            if(screen_.enabled()) { refresh(); screen_.render(editor); return true; }
            if(screen_.has_candidates()) { screen_.render(editor); return true; }
            return false;
        }
        [[nodiscard]] console_read_hooks hooks() noexcept
        {
            return {this,
                +[](void* self,console_editing::editor& editor) { return static_cast<console_visual_session*>(self)->complete(editor); },
                +[](void* self,console_editing::editor const& editor,console_editing::action action) { return static_cast<console_visual_session*>(self)->draw(editor,action); },
                +[](void* self,console_editing::editor const& editor,bool dynamic) { static_cast<console_visual_session*>(self)->show_candidates(editor,dynamic); },
                +[](void* self,console_editing::editor& editor,int key) { return static_cast<console_visual_session*>(self)->screen_.input_key(editor,key); }};
        }
    };
    // Normal CLI management only. No OS callback, guest address, synthetic
    // park or recursive controller request is used here. Each completed wait
    // ticket is acknowledged before the next synchronous wait starts.
    struct console_foreground_result
    {
        controller_reply reply{};
        bool interruption_timeout{};
    };
    [[nodiscard]] inline console_foreground_result wait_console_foreground(controller& control,
        console_io io, management_wait_interrupt_observation& interruption, bool& interrupt_acknowledged)
    {
        using clock_type = ::std::chrono::steady_clock;
        clock_type::time_point interruption_deadline{};
        auto const wait{parse_console_command("wait")};
        for(;;)
        {
            auto reply{control.execute(wait, ::std::chrono::milliseconds{50}, interruption.borrow())};
            if(io.display_poll!=nullptr) { io.display_poll(io.display_context); }
            if(interruption.observed())
            {
                if(interruption_deadline == clock_type::time_point{})
                { interruption_deadline = clock_type::now() + ::std::chrono::seconds{2}; }
                if(!interrupt_acknowledged)
                {
                    // execute(wait) already serviced the REAL delivered host
                    // event. Only acknowledge keyboard input, never a VM stop.
                    io.write(io.context, "^C\n");
                    if(io.consume_interrupt != nullptr) { static_cast<void>(io.consume_interrupt(io.context)); }
                    interrupt_acknowledged = true;
                }
            }
            if(reply.status != ::uwvm2::utils::control::error::none ||
               reply.execution == execution_status::exited || reply.execution == execution_status::closed ||
               (reply.execution == execution_status::stopped && reply.stop_identifier != 0u))
            { return {::std::move(reply), false}; }
            // A 50ms slice timeout is neither a completed run nor a stop. With
            // no cancellation, keep waiting for an actual breakpoint/exit.
            // An uncooperative host call may outlast the separate cancellation
            // deadline; expose an ERROR and its pending actual state, retain
            // the controller ticket/owners, and permit later status/pause.
            if(interruption_deadline != clock_type::time_point{} && clock_type::now() >= interruption_deadline)
            { reply.timed_out = true; return {::std::move(reply), true}; }
        }
    }
    // This formatter copies only host-owned phase/error values. It grants no
    // guest stop, stack, native-register or code-reset permission.
    inline void write_console_shutdown_result(console_io io, managed_cli_shutdown_result result)
    {
        if(result.phase == managed_cli_shutdown_phase::completed) { return; }
        auto const text{::fast_io::concat_fast_io("error: managed shutdown: ",
            managed_cli_shutdown_phase_text(result.phase), " native-error=", ::fast_io::mnp::dec(result.native_error),
            result.deadline_expired ? ::fast_io::string_view{"; deadline expired; owners retained\n"} : ::fast_io::string_view{"\n"})};
        io.write(io.context, text.subview(0u));
    }
    [[nodiscard]] inline console_exit run_console(controller& control, console_io io,
        managed_cli_shutdown* shutdown = nullptr, bool force_on_physical_eof = false,
        console_restart_request* restart = nullptr)
    {
        if(io.read_byte == nullptr || io.write == nullptr) { return console_exit::input_failure; }
        if(shutdown != nullptr && !shutdown->matches_controller(control))
        { io.write(io.context, "error: managed shutdown belongs to a different host controller\n"); return console_exit::input_failure; }
        struct paste_terminal_scope
        {
            console_io io;
            ~paste_terminal_scope() { if(io.interactive) { io.write(io.context, "\x1b[?2004l"); } }
        } const paste_scope{io};
        if(io.interactive) { io.write(io.context, "\x1b[?2004h"); }
        if(restart == nullptr || !restart->resume_new_launch)
        { io.write(io.context, "UWVM debugger. Type \"help\" for command classes, \"help run\" to begin.\n"); }
        // The bounded history/kill/undo buffers outlive each command. Keep
        // their owner off the management thread stack: at -O0 the editor plus
        // execute() exceeded the ordinary 1 MiB Windows thread stack.
        ::std::unique_ptr<console_editing::editor> local_editor{};
        ::std::unique_ptr<console_tui::screen> local_screen{};
        auto& editor_owner{restart == nullptr ? local_editor : restart->editor};
        auto& screen_owner{restart == nullptr ? local_screen : restart->screen};
        if(!editor_owner) { editor_owner=::std::make_unique<console_editing::editor>(); }
        if(!screen_owner) { screen_owner=::std::make_unique<console_tui::screen>(io.context,io.write,io.interactive); }
        auto& editor{*editor_owner};
        console_visual_session visual{control,io,*screen_owner};
        struct display_state { console_tui::screen& screen; console_editing::editor& editor; } display{*screen_owner,editor};
        io.display_context=::std::addressof(display);
        io.display_poll=+[](void* context)
        { auto& state{*static_cast<display_state*>(context)};
          if(state.screen.drain_guest() && state.screen.enabled()) { state.screen.render(state.editor); } };
        auto const output{[&](::fast_io::string_view text) { visual.output(text); }};
        auto launch_layout{[&](console_launch_action action)
        {
            if(!screen_owner->enabled() || screen_owner->custom_layout()) { return; }
            if(action == console_launch_action::start_wasm) { screen_owner->select(console_tui::layout::wasm); }
            else if(action == console_launch_action::start_instruction) { screen_owner->select(console_tui::layout::assembly); }
            else if(action == console_launch_action::start) { screen_owner->select(console_tui::layout::src); }
        }};
        if(restart != nullptr && restart->resume_new_launch) { launch_layout(restart->action); }
        breakpoint temporary_start{}; // Cold target labels, never a native address or read grant.
        auto const retire_start_hit{[&]
        {
            if(temporary_start.identifier == 0u) { return; }
            auto const actual{control.inspect()};
            if(actual.execution != execution_status::stopped || actual.reason != stop_reason::breakpoint) { return; }
            for(auto const& thread : actual.threads)
            {
                if(thread.location.code_unit != temporary_start.module || thread.location.function != temporary_start.function ||
                   thread.location.offset != temporary_start.offset) { continue; }
                // Delete only this CLI's temporary main breakpoint through the
                // ordinary current controller; user breakpoints remain intact.
                static_cast<void>(control.execute(parse_console_command(::fast_io::concat_fast_io("delete ",
                    ::fast_io::mnp::dec(temporary_start.identifier)).subview(0u))));
                temporary_start = {}; break;
            }
        }};
        auto const remember_breakpoints{[&]
        {
            auto points{control.copy_restart_breakpoints()};
            points.erase(::std::remove_if(points.begin(), points.end(), [&](auto const& point)
                { return point.identifier == temporary_start.identifier; }), points.end());
            restart->breakpoints = ::std::move(points);
        }};
        auto const arm_main{[&]() -> bool
        {
            ::std::uint64_t function{}, offset{}; bool found{}, ambiguous{};
            bool const exhaustive{control.visit_source_function_symbols(0u, "main",
                [&](::fast_io::string_view name, ::std::uint64_t f, ::std::uint64_t at)
                {
                    if(name != "main") { return true; }
                    if(found && function != f) { ambiguous = true; }
                    if(!found || at < offset) { function = f; offset = at; }
                    found = true; return true;
                })};
            if(!found || ambiguous || !exhaustive)
            { output("error: main is unavailable or ambiguous in module 0. Use startw, or set a breakpoint and run.\n"); return false; }
            if(temporary_start.identifier != 0u)
            {
                static_cast<void>(control.execute(parse_console_command(::fast_io::concat_fast_io("delete ",
                    ::fast_io::mnp::dec(temporary_start.identifier)).subview(0u))));
                temporary_start = {};
            }
            auto const previous{control.copy_restart_breakpoints()};
            bool const user_point{::std::any_of(previous.begin(), previous.end(), [&](auto const& point)
                { return point.module == 0u && point.function == function && point.offset == offset; })};
            auto point{parse_console_command(::fast_io::concat_fast_io("breakwasm 0 ",
                ::fast_io::mnp::dec(function), " ", ::fast_io::mnp::dec(offset)).subview(0u))};
            point.breakpoint_source_display=true;
            auto const installed{control.execute(point)};
            if(installed.status != ::uwvm2::utils::control::error::none)
            { output("error: Cannot set the temporary main breakpoint. Use startw or help breakwasm.\n"); return false; }
            auto const temporary_id{control.make_initial_temporary_breakpoint(installed.breakpoint_identifier, user_point)};
            if(temporary_id == 0u)
            {
                if(!user_point)
                { static_cast<void>(control.execute(parse_console_command(::fast_io::concat_fast_io("delete ",
                    ::fast_io::mnp::dec(installed.breakpoint_identifier)).subview(0u)))); }
                output("error: Cannot reserve a temporary main breakpoint at the initial stop. Use status and info breakpoints.\n"); return false;
            }
            temporary_start = {}; temporary_start.identifier = temporary_id;
            temporary_start.function = function; temporary_start.offset = offset;
            output(::fast_io::concat_fast_io("Temporary breakpoint ", ::fast_io::mnp::dec(temporary_id), " at main.\n").subview(0u));
            return true;
        }};
        auto const launch_first_instruction{[&](bool native)
        {
            output(details::format_program_start(restart == nullptr ? ::fast_io::string_view{} : restart->program_name.subview(0u)).subview(0u));
            management_wait_interrupt_observation interruption{{io.context, io.interrupt_pending}};
            auto const stopped{control.launch_first_instruction(native, ::std::chrono::seconds{2}, interruption.borrow())};
            if(stopped.status != ::uwvm2::utils::control::error::none)
            {
                output(native ? ::fast_io::string_view{"error: Cannot authenticate the initial JIT instruction trap. Use status, or startw for Wasm entry.\n"} :
                    ::fast_io::string_view{"error: The first Wasm instruction stop is unavailable. Use status to inspect the actual execution state.\n"});
            }
            auto const report{details::format_cli_reply(stopped, parse_console_command("wait"))};
            output(::fast_io::string_view{report.data(), report.size()});
        }};
        console_displays::session displays{};
        auto const show_displays{[&](controller_reply const& observed, ::std::uint64_t only = 0u)
        {
            // Backtrace/locals replies may filter a multithread cohort for
            // display. Select only from a fresh complete controller snapshot.
            auto const actual{control.inspect()};
            if(actual.stop_identifier != observed.stop_identifier) { return; }
            if(actual.execution != execution_status::stopped || actual.stop_identifier == 0u || actual.threads.size() != 1u) { return; }
            bool const fresh{displays.new_stop(actual.stop_identifier)};
            if(!fresh && only == 0u) { return; }
            for(auto const& entry : displays.records())
            {
                if(entry.identifier == 0u || !entry.enabled || (!fresh && only != 0u && only != entry.identifier)) { continue; }
                auto const query{parse_console_command(::fast_io::concat_fast_io("print ",
                    ::fast_io::mnp::dec(actual.threads.front().identifier)," ",::fast_io::mnp::dec(actual.stop_identifier),
                    " 0 ",entry.expression).subview(0u))};
                // execute rechecks this explicit stop/thread/frame on every
                // read. Text and a prior observed reply grant no read authority.
                auto const copied{control.execute(query)};
                output(::fast_io::concat_fast_io("display ",::fast_io::mnp::dec(entry.identifier)," stop=",
                    ::fast_io::mnp::dec(actual.stop_identifier)," expression=",entry.expression,"\n").subview(0u));
                auto const formatted{details::format_reply(copied,query)};
                output(::fast_io::string_view{formatted.data(),formatted.size()});
            }
        }};
        if(restart != nullptr && restart->resume_new_launch)
        {
            restart->resume_new_launch = false;
            restart->killed = false;
            auto const ready{control.execute(parse_console_command("wait"))};
            if(ready.status == ::uwvm2::utils::control::error::none &&
               ready.execution == execution_status::stopped && ready.reason == stop_reason::initial)
            {
                if(restart->action == console_launch_action::start_instruction || restart->action == console_launch_action::start_wasm)
                { launch_first_instruction(restart->action == console_launch_action::start_instruction); }
                else if(restart->action != console_launch_action::start || arm_main())
                {
                output(details::format_program_start(restart == nullptr ? ::fast_io::string_view{} : restart->program_name.subview(0u)).subview(0u));
                management_wait_interrupt_observation interruption{{io.context, io.interrupt_pending}};
                auto const resumed{control.execute(parse_console_command("continue"), interruption.borrow())};
                auto const text{details::format_cli_reply(resumed, parse_console_command("continue"))};
                output(::fast_io::string_view{text.data(), text.size()});
                bool acknowledged{};
                if(io.foreground_execution && !restart->background && resumed.status == ::uwvm2::utils::control::error::none)
                {
                    auto const foreground{wait_console_foreground(control, io, interruption, acknowledged)};
                    if(foreground.interruption_timeout)
                    { output("error: interruption deadline expired; complete pause is unavailable; actual request and owners retained\n"); }
                    auto const report{details::format_cli_reply(foreground.reply, parse_console_command("wait"))};
                    output(::fast_io::string_view{report.data(), report.size()});
                    show_displays(foreground.reply);
                }
                }
            }
            else { output("error: fresh initial stop is not ready; use status/wait then run\n"); }
        }
        for(;;)
        {
            retire_start_hit();
            if(!visual.enabled() && !io.interactive) { io.write(io.context,"(uwvm-debug) "); }
            auto const line{read_console_line(io, editor,visual.hooks())};
            if(line.status == line_status::failure) { return console_exit::input_failure; }
            if(line.status == line_status::end)
            {
                visual.disable();
                if(shutdown == nullptr) { return console_exit::terminate_process; }
                auto const result{shutdown->attempt_until(::std::chrono::steady_clock::now() + ::std::chrono::seconds{2})};
                write_console_shutdown_result(io, result);
                if(result.phase == managed_cli_shutdown_phase::completed) { return console_exit::managed_exit; }
                if(line.physical_input_eof)
                {
                    // Only an explicitly selected standalone HOST EOF policy
                    // permits process containment. Never return normally and
                    // destroy a still-joinable actual guest/code owner.
                    if(force_on_physical_eof)
                    {
                        output("physical EOF: preselected standalone process termination; cleanup incomplete\n");
                        return console_exit::terminate_process;
                    }
                    // An embedding caller retains the exact original helper/
                    // actual guest/VM owners across this explicitly pending
                    // return and may retry on the same host issuer. There is no
                    // unbounded console loop, normal-exit claim or detached work.
                    output("physical EOF: shutdown pending; host caller must retain all owners\n");
                    return console_exit::shutdown_pending;
                }
                continue; // terminal CtrlD can return to the retained prompt
            }
            if(line.status == line_status::interrupted)
            {
                output(io.interactive ? ::fast_io::string_view{"\r\x1b[2K^C\n"} : ::fast_io::string_view{"^C\n"});
                if(shutdown != nullptr && shutdown->requested())
                { write_console_shutdown_result(io, shutdown->inspect()); continue; }
                // Normal host-management context only. The callback provided no
                // stop/native/source capability; execute authenticates the same
                // ordinary pause request used by the explicit console command.
                auto const actual{control.inspect()};
                auto const command{parse_console_command(actual.execution == execution_status::running ||
                    actual.execution == execution_status::stopping ? ::fast_io::string_view{"pause"} : ::fast_io::string_view{"status"})};
                // read_byte() consumed the OS flag BEFORE returning the actual
                // trusted HOST -3 event. Retain that delivered cancellation in
                // this synchronous operation; it cannot fall back to ordinary
                // timeout resume. This fixed callback has no VM/frame authority.
                auto const delivered{management_wait_interrupt{nullptr, +[](void*) noexcept { return true; }}};
                auto const reply{command.operation == ::uwvm2::utils::control::operation::pause ?
                    control.execute(command, delivered) : actual};
                auto const text{details::format_cli_reply(reply, command)};
                output(::fast_io::string_view{text.data(), text.size()}); show_displays(reply); continue;
            }
            if(line.status == line_status::oversized)
            { editor.forget_repeat(); output("error: command exceeds 8448 input bytes or contains an unsupported escape; entire line discarded\n"); continue; }
            auto const input{::fast_io::string_view{line.bytes.data(), line.size}};
            auto const trimmed{console_execution::trim(input)};
            // History is what the user submitted, independent of parser or runtime success.
            if(!trimmed.empty()) { editor.remember(input,false); }
            visual.submitted(input);
            auto const documentation{console_aliases::normalize(trimmed)};
            auto const documentation_words{console_aliases::split(documentation.view())};
            if(auto const request{console_module_help::parse(trimmed)}; request.handled)
            {

                if(!request.valid)
                { output("error: MODULE_ID must be an unsigned decimal number, not a filename or module name.\nUse \"info modules\" to find its numeric ID.\n"); }
                else
                {
                    auto const size{console_tui::terminal_dimensions()};
                    auto page{request.selected && request.module == (::std::numeric_limits<::std::uint64_t>::max)() ? module_help_page{} :
                        control.source_module_page(request.selected ? request.module + 1u : 1u, request.selected ? 1u : 4u)};
                    output(console_module_help::format(page, io.interactive, io.interactive ? size.columns : 80u,
                        request.selected, request.source).subview(0u));
                }
                continue;
            }
            if(documentation.valid && documentation_words.word == "apropos")
            {
                auto const size{console_tui::terminal_dimensions()};
                auto const found{console_help::search(documentation_words.tail, io.interactive,
                    io.interactive ? size.columns : 80u,
                    io.interactive && size.rows < 23u ? (size.rows > 5u ? size.rows - 5u : 4u) : 18u)};
                output(found.subview(0u));  continue;
            }
            if(auto const menu{console_help::menu_request(trimmed)}; menu != nullptr)
            {
                auto const size{console_tui::terminal_dimensions()};
                auto const help{console_help::format(menu->topic, io.interactive, io.interactive ? size.columns : 80u,
                    io.interactive && size.rows < 23u ? (size.rows > 5u ? size.rows - 5u : 4u) : 18u)};
                output(help.subview(0u));  continue;
            }
            if(visual.command(trimmed)) {  continue; }
            if(documentation.valid && documentation.view() == "quit force")
            {
                editor.forget_repeat();
                if(shutdown == nullptr)
                { output("error: explicit force termination requires a managed standalone CLI owner\n"); continue; }
                output("explicit force termination: managed cleanup is not claimed\n");
                return console_exit::terminate_process;
            }
            if(shutdown != nullptr && shutdown->requested())
            {
                auto const safe_parse{console_aliases::parse(input, 0u, 0u)};
                auto const& safe{safe_parse.command};
                bool const retry_run{restart != nullptr && restart->pending && console_execution::classify_resume(input).from_entry};
                bool const retry_kill{restart != nullptr && restart->pending && safe.launch_action == console_launch_action::kill};
                if(restart != nullptr && restart->killed && safe.kind != console_command_kind::help &&
                   safe.kind != console_command_kind::quit && !retry_run)
                {
                    editor.forget_repeat();
                    if(safe.kind == console_command_kind::invalid && !safe_parse.current_thread_required)
                    { output(console_messages::syntax_error(trimmed).subview(0u)); }
                    else if(safe.operation == ::uwvm2::utils::control::operation::breakpoint_list)
                    {
                        controller_reply copied{}; copied.breakpoints = restart->breakpoints;
                        auto const text{details::format_cli_reply(copied, safe)};
                        output(::fast_io::string_view{text.data(), text.size()});
                    }
                    else { output("No program is running. Use startw, starti, start or run to launch it again.\n"); }
                    continue;
                }
                if(!retry_run && !retry_kill &&
                   safe.kind != console_command_kind::help && safe.kind != console_command_kind::quit)
                {
                    editor.forget_repeat();
                    if(safe.kind == console_command_kind::protocol && safe.operation == ::uwvm2::utils::control::operation::status && !safe.wait_for_event)
                    { write_console_shutdown_result(io, shutdown->inspect()); }
                    else { output("error: shutdown retains active owners; only status/help/quit retry/quit force or pending run retry are allowed\n"); }
                    continue;
                }
            }
            ::std::uint64_t current_thread{}, current_stop{};
            auto const display_request{console_displays::parse(trimmed)};
            if(display_request.operation != console_displays::action::none)
            {

                if(display_request.operation == console_displays::action::list)
                {
                    output("automatic source displays (frame0; current values are queried only at a genuine stop)\n");
                    for(auto const& entry : displays.records())
                    { if(entry.identifier != 0u) { output(::fast_io::concat_fast_io("display ",::fast_io::mnp::dec(entry.identifier),
                        entry.enabled ? ::fast_io::string_view{" enabled "} : ::fast_io::string_view{" disabled "},entry.expression,"\n").subview(0u)); } }
                }
                else if(display_request.operation == console_displays::action::add)
                {
                    source_scalar_expression::program syntax{}; ::std::uint64_t id{};
                    if(source_scalar_expression::parse_admitted({display_request.expression.data(),display_request.expression.size()},syntax) != source_scalar_expression::error::none)
                    { output("error: display requires a bounded read-only source expression\n"); }
                    else if(displays.add(display_request.expression,id) != console_displays::error::none)
                    { output("error: display limit is32 expressions of256 printable bytes\n"); }
                    else
                    {
                        output(::fast_io::concat_fast_io("display ",::fast_io::mnp::dec(id)," registered\n").subview(0u));
                        show_displays(control.inspect(),id);
                    }
                }
                else if(display_request.operation == console_displays::action::invalid ||
                    displays.change(display_request.operation,display_request.identifier) != console_displays::error::none)
                { output("error: invalid or unknown display identifier\n"); }
                else { output("automatic source display policy updated\n"); }
                continue;
            }
            if(console_aliases::uses_current_thread(input))
            {
                auto const actual{control.inspect()};
                if(actual.execution == execution_status::stopped && actual.stop_identifier != 0u && actual.threads.size() == 1u)
                { current_thread = actual.threads.front().identifier; current_stop = actual.stop_identifier; }
            }
            auto const resume_policy{console_execution::classify_resume(input)};
            // The ampersand is a CLI-only scheduling choice, never a new wire
            // operation. Controller/server/DAP resume grammar remains unchanged.
            auto const parsed{console_aliases::parse(resume_policy.recognized ?
                (resume_policy.from_entry ? console_execution::launch_word(resume_policy.launch_action) : ::fast_io::string_view{"continue"}) : input,
                current_thread, current_stop)};
            if(parsed.ambiguous)
            { editor.forget_repeat(); output(console_aliases::ambiguity_error(input).subview(0u)); continue; }
            if(parsed.current_thread_required)
            {
                editor.forget_repeat(); auto const actual{control.inspect()}; auto const message{console_messages::state_error(actual)};
                if(!message.empty()) { output(message.subview(0u)); }
                else { output("error: Select a stopped thread: use \"info threads\", then supply THREAD explicitly.\n"); }
                continue;
            }
            auto command{parsed.command};
            if(command.launch_action == console_launch_action::kill)
            {

                if(restart == nullptr || shutdown == nullptr)
                { output("error: kill requires the standalone owned LLVM-full launcher.\n"); continue; }
                if(!restart->pending) { remember_breakpoints(); restart->pending = true; }
                auto const retired{shutdown->attempt_until(::std::chrono::steady_clock::now() + ::std::chrono::seconds{2})};
                write_console_shutdown_result(io, retired);
                if(retired.phase != managed_cli_shutdown_phase::completed)
                { output("kill pending: old owners retained; retry kill after cleanup can complete.\n"); continue; }
                restart->killed = true; restart->resume_new_launch = false;
                temporary_start = {}; visual.disable();
                output("Program terminated.\n"); continue;
            }
            if(command.run_from_entry)
            {

                auto actual{control.inspect()};
                if(actual.execution == execution_status::stopping && actual.reason == stop_reason::initial)
                { actual = control.execute(parse_console_command("wait")); }
                launch_layout(command.launch_action);
                if((restart == nullptr || !restart->pending) && actual.execution == execution_status::stopped &&
                   actual.reason == stop_reason::initial)
                {
                    if(command.launch_action == console_launch_action::start_instruction || command.launch_action == console_launch_action::start_wasm)
                    { launch_first_instruction(command.launch_action == console_launch_action::start_instruction); continue; }
                    if(command.launch_action == console_launch_action::start && !arm_main()) { continue; }
                    command.run_from_entry = false; // Actual initial stop has executed no Wasm instruction.
                    command.launch_action = console_launch_action::none;
                    output(details::format_program_start(restart == nullptr ? ::fast_io::string_view{} : restart->program_name.subview(0u)).subview(0u));
                }
                else
                {
                    if(restart == nullptr || shutdown == nullptr)
                    { output("error: run restart requires the standalone owned LLVM-full launcher; use continue to resume\n"); continue; }
                    if(!restart->pending)
                    { remember_breakpoints(); restart->pending = true; }
                    auto const retired{shutdown->attempt_until(::std::chrono::steady_clock::now() + ::std::chrono::seconds{2})};
                    write_console_shutdown_result(io, retired);
                    if(retired.phase != managed_cli_shutdown_phase::completed)
                    { output("run restart pending: old owners retained; retry run after cleanup can complete\n"); continue; }
                    restart->resume_new_launch = true; restart->background = resume_policy.background;
                    restart->action = command.launch_action; launch_layout(command.launch_action);
                    return console_exit::restart;
                }
            }
            if(command.kind == console_command_kind::invalid && visual.function_breakpoint(trimmed,command) &&
                command.kind == console_command_kind::invalid) { editor.forget_repeat(); continue; }
            if(command.kind != console_command_kind::invalid && command.kind != console_command_kind::unsupported && command.kind != console_command_kind::empty)
            { editor.set_repeat(input, console_aliases::repeatable(command)); }
            else if(command.kind != console_command_kind::empty) { editor.forget_repeat(); }
            switch(command.kind)
            {
                case console_command_kind::module_list:
                {
                    auto const size{console_tui::terminal_dimensions()};
                    output(console_module_help::format(control.source_module_page(command.module_help_page),
                        io.interactive, io.interactive ? size.columns : 80u).subview(0u));
                    continue;
                }
                case console_command_kind::empty: continue;
                case console_command_kind::help:
                {
                    auto const size{console_tui::terminal_dimensions()};
                    auto const help_text{console_help::format(
                        ::fast_io::string_view{command.help_topic.data(), command.help_topic_size}, io.interactive,
                        io.interactive ? (visual.enabled() && size.columns > 5u ? size.columns-5u : size.columns) : 80u,
                        io.interactive && size.rows < 23u ? (size.rows > 5u ? size.rows - 5u : 4u) : 18u)};
                    output(help_text.subview(0u)); continue;
                }
                case console_command_kind::quit:
                {
                    visual.disable();
                    if(shutdown == nullptr) { return console_exit::terminate_process; }
                    auto const result{shutdown->attempt_until(::std::chrono::steady_clock::now() + ::std::chrono::seconds{2})};
                    write_console_shutdown_result(io, result);
                    if(result.phase == managed_cli_shutdown_phase::completed) { return console_exit::managed_exit; }
                    continue;
                }
                case console_command_kind::unsupported: output(unsupported_step_message(command)); continue;
                case console_command_kind::invalid: editor.forget_repeat(); output(console_messages::syntax_error(trimmed).subview(0u)); continue;
                case console_command_kind::source_step:
                case console_command_kind::wasm_step:
                case console_command_kind::source_breakpoint:
                case console_command_kind::source_locals:
                case console_command_kind::execution_relocate:
                case console_command_kind::assembly_breakpoint:
                case console_command_kind::assembly_step:
                case console_command_kind::assembly_next:
                case console_command_kind::assembly_finish:
                case console_command_kind::assembly_disassemble:
                case console_command_kind::assembly_disassemble_range:
                case console_command_kind::assembly_registers:
                case console_command_kind::wasm_state:
                case console_command_kind::wasm_mutation:
                case console_command_kind::wasm_path:
                case console_command_kind::wasip1_state:
                case console_command_kind::wasm_event:
                case console_command_kind::wasm_script:
                case console_command_kind::source_type:
                case console_command_kind::source_value:
                case console_command_kind::source_frame:
                case console_command_kind::breakpoint_control:
                case console_command_kind::replacement_file:
                case console_command_kind::protocol: break;
            }
            if(resume_policy.recognized && !resume_policy.from_entry)
            {
                auto const actual{control.inspect()};
                if(actual.execution == execution_status::stopped && actual.stop_identifier != 0u) { output("Continuing.\n"); }
            }
            management_wait_interrupt_observation interruption{{io.context, io.interrupt_pending}};
            auto const reply{control.execute(command, interruption.borrow())};
            bool interrupt_acknowledged{};
            if(interruption.observed())
            {
                output("^C\n");
                if(io.consume_interrupt != nullptr) { static_cast<void>(io.consume_interrupt(io.context)); }
                interrupt_acknowledged = true;
            }
            auto const text{details::format_cli_reply(reply, command)};
            output(::fast_io::string_view{text.data(), text.size()});
            show_displays(reply);
            if(io.foreground_execution && resume_policy.recognized && !resume_policy.background &&
               reply.status == ::uwvm2::utils::control::error::none &&
               (reply.execution == execution_status::running || reply.execution == execution_status::stopping))
            {
                auto const foreground{wait_console_foreground(control, io, interruption, interrupt_acknowledged)};
                if(foreground.interruption_timeout)
                { output("error: interruption deadline expired; complete pause is unavailable; actual request and owners retained\n"); }
                auto const report{details::format_cli_reply(foreground.reply, parse_console_command("wait"))};
                output(::fast_io::string_view{report.data(), report.size()});
                show_displays(foreground.reply);
            }
        }
    }
    // Explicit host adapter for tests/embedders that reserve stdin exclusively
    // for management. The production launcher must use isolated console_io when
    // guests have stdin: sharing stdin is not a secure command transport.
    // A standalone restart retains one physical terminal/signal owner while
    // retiring and recreating only the guest/controller session.
    [[nodiscard]] inline console_exit run_console(controller& control, console_keyboard::native_session& keyboard,
        managed_cli_shutdown* shutdown = nullptr, bool force_on_physical_eof = false,
        console_restart_request* restart = nullptr)
    {
        if(!keyboard.ready()) { return console_exit::input_failure; }
        // The caller retains the keyboard owner across console restarts. Its
        // scoped destructor restores terminal modes/handlers on final exit.
        // Guest retirement and full VM owner cleanup are separate launcher
        // obligations; no active guest may be joined indefinitely.
        return run_console(control, {::std::addressof(keyboard),
            [](void* context) noexcept { return static_cast<console_keyboard::native_session*>(context)->read_byte(); },
            [](void*, ::fast_io::string_view text) noexcept { ::fast_io::print(::fast_io::out(), text); },
            keyboard.interactive_display(),
            [](void* context) noexcept { return static_cast<console_keyboard::native_session*>(context)->interrupt_pending(); },
            keyboard.interactive(),
            [](void* context) noexcept { return static_cast<console_keyboard::native_session*>(context)->consume_interrupt(); },nullptr,nullptr,
            [](void* context) noexcept { return static_cast<console_keyboard::native_session*>(context)->input_ready(); }},
            shutdown, force_on_physical_eof, restart);
    }
    [[nodiscard]] inline console_exit run_console(controller& control,
        managed_cli_shutdown* shutdown = nullptr, bool force_on_physical_eof = false,
        console_restart_request* restart = nullptr)
    {
        console_keyboard::native_session keyboard{};
        return run_console(control, keyboard, shutdown, force_on_physical_eof, restart);
    }
#endif
}
#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/pop_macros.h>
#endif
