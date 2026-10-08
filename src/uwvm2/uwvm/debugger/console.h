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
# include <chrono>
# include <utility>
# include <fast_io_dsal/string_view.h>
# include <fast_io_dsal/string.h>
# include <fast_io.h>
# include <fast_io_unit/string.h>
# include "command.h"
# include "controller.h"
# include "managed_cli_shutdown.h"
# include "console_line_editor.h"
# include "console_keyboard.h"
# include "console_aliases.h"
# include "console_execution.h"
# include "console_completion.h"
# include "console_displays.h"
# include "console_tui.h"
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
    enum class console_exit { terminate_process, input_failure, managed_exit, shutdown_pending };
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
    };
    [[nodiscard]] inline console_line read_console_line(console_io io, console_editing::editor& editor,
        console_read_hooks hooks = {})
    {
        static_assert(console_editing::maximum_bytes == max_input_command_bytes);
        editor.begin();
        bool const full_screen{hooks.draw != nullptr && hooks.draw(hooks.context, editor, console_editing::action::unchanged)};
        if(io.interactive && !full_screen && editor.line().size != 0u) { io.write(io.context, editor.line().view()); }
        for(;;)
        {
            auto const input_byte{io.read_byte(io.context)};
            auto action{editor.feed(input_byte)};
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
                auto const text{editor.line().view()}; auto const cursor{editor.cursor()};
                // [owned bounded edited command ... cursor ... size] end
                // [safe                                             ] cursor <= size;
                // subviews stay inside this synchronous editor-owned buffer.
                auto const prompt{editor.searching() ? ::fast_io::concat_fast_io(
                    editor.searching_forward()
                        ? (editor.search_matched() ? ::fast_io::string_view{"(i-search)`"} : ::fast_io::string_view{"(failed i-search)`"})
                        : (editor.search_matched() ? ::fast_io::string_view{"(reverse-i-search)`"} : ::fast_io::string_view{"(failed reverse-i-search)`"}),
                    editor.search_query(), "': ") : ::fast_io::concat_fast_io("(uwvm-debug) ")};
                auto const rendered{::fast_io::concat_fast_io("\r\x1b[2K", prompt, text.subview(0u, cursor),
                    "\x1b[s", text.subview(cursor), "\x1b[u")};
                io.write(io.context, ::fast_io::string_view{rendered.data(), rendered.size()});
            }
            if(action == console_editing::action::changed || action == console_editing::action::unchanged ||
                action == console_editing::action::redraw || action == console_editing::action::candidates ||
                action == console_editing::action::tui_toggle || action == console_editing::action::tui_single ||
                action == console_editing::action::tui_split || action == console_editing::action::page_up ||
                action == console_editing::action::page_down) { continue; }
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
                           reply.disassembly_owner_begin, reply.disassembly_owner_end) ||
                       (!instruction && instruction.pc != 0u) ||
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
                    ::fast_io::io::print(output, " pc=0x", ::fast_io::mnp::hex<false, true>(instruction.pc), " bytes=");
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
                           reply.disassembly_owner_begin, reply.disassembly_owner_end) ||
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
                    ::fast_io::io::print(output, " bytes=");
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
                ::fast_io::io::print(output, "memory:");
                if(reply.memory.empty()) { ::fast_io::io::print(output, " <empty>"); }
                for(auto const byte : reply.memory)
                {
                    auto const value{::std::to_integer<unsigned char>(byte)};
                    ::fast_io::io::print(output, " ", ::fast_io::mnp::hex<false, true>(value));
                }
                ::fast_io::io::print(output, ::fast_io::mnp::chvw('\n')); return text;
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
                if(command.kind == console_command_kind::source_breakpoint)
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
                else { wasip1_state::print(output, reply.wasip1_state_values); }
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
                        reply.reason == stop_reason::native_step ? "stopped: native instruction step\n" :
                        reply.reason == stop_reason::wasm_uncaught ? "stopped: Uncaught Wasm exception before unwind; continue propagates\n" :
                        reply.reason == stop_reason::wasm_trap ? "stopped: Wasm trap after failure; pre-trap operands; continue terminates\n" :
                        reply.reason == stop_reason::wasm_catchpoint ? "stopped: Wasm catchpoint before instruction\n" :
                        reply.reason == stop_reason::step ? "stopped: selected participant step\n" : "stopped: pause\n"));
                    if(reply.reason == stop_reason::wasm_uncaught && reply.wasm_terminal_participant != 0u)
                    { ::fast_io::io::print(output, "uncaught-thread ", ::fast_io::mnp::dec(reply.wasm_terminal_participant), "\n"); }
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
    // One normal HOST management session. UI/completion receives copied reply
    // DATA only. Every source/native/Wasm query still goes through execute().
    class console_visual_session final
    {
        controller& control_;
        console_io io_;
        console_tui::screen screen_;
        console_completion::candidates candidates_{};
        ::std::uint64_t painted_stop_{};
        execution_status painted_execution_{execution_status::closed};
        bool refresh_needed_{true};
        [[nodiscard]] controller_reply query(::fast_io::string_view input)
        { return control_.execute(parse_console_command(input)); }
        void panel_reply(console_tui::layout which, ::fast_io::string const& input) { panel_reply(which,input.subview(0u)); }
        void panel_reply(console_tui::layout which, ::fast_io::string_view input)
        {
            auto const command{parse_console_command(input)};
            auto const reply{control_.execute(command)};
            auto const text{details::format_reply(reply,command)};
            if(which == console_tui::layout::registers && reply.registers_stop_identifier == 0u)
            { screen_.set_panel(which,::fast_io::concat_fast_io(
                "native registers unavailable: requires an authenticated Wasm JIT trap\n",
                ::fast_io::string_view{text.data(),text.size()})); }
            else { screen_.set_panel(which,::fast_io::string_view{text.data(),text.size()}); }
        }
    public:
        console_visual_session(controller& control, console_io io)
            : control_{control}, io_{io}, screen_{io.context,io.write,io.interactive} {}
        void output(::fast_io::string_view text) { screen_.output(text); }
        [[nodiscard]] bool enabled() const noexcept { return screen_.enabled(); }
        void disable() noexcept { screen_.disable(); }
        void output(::fast_io::string const& text) { output(text.subview(0u)); }
        [[nodiscard]] bool command(::fast_io::string_view input)
        { bool const handled{screen_.command(input)}; if(handled) { refresh_needed_=true; } return handled; }
        [[nodiscard]] bool function_breakpoint(::fast_io::string_view input, console_command& result)
        {
            auto const request{console_completion::context(input,input.size())};
            if(request.type != console_completion::kind::function || request.prefix.empty()) { return false; }
            // Numeric break MODULE FUNCTION BYTE_OFFSET retains its exact grammar.
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
            { output("error: function symbol unavailable or ambiguous; use break MODULE FUNCTION BYTE_OFFSET\n"); return true; }
            result=parse_console_command(::fast_io::concat_fast_io("break ",::fast_io::mnp::dec(request.module)," ",
                ::fast_io::mnp::dec(function)," ",::fast_io::mnp::dec(offset)).subview(0u)); return true;
        }
        void submitted(::fast_io::string_view input)
        { screen_.record(::fast_io::concat_fast_io("(uwvm-debug) ",input,"\n")); refresh_needed_=true; }
        void refresh()
        {
            if(!screen_.enabled()) { return; }
            auto const actual{control_.inspect()};
            if(!refresh_needed_ && actual.stop_identifier == painted_stop_ && actual.execution == painted_execution_) { return; }
            refresh_needed_=false; painted_stop_=actual.stop_identifier; painted_execution_=actual.execution;
            screen_.clear_panels();
            auto const phase{actual.execution == execution_status::stopped ? ::fast_io::string_view{"stopped"} :
                actual.execution == execution_status::running ? ::fast_io::string_view{"running"} :
                actual.execution == execution_status::stopping ? ::fast_io::string_view{"stopping"} : ::fast_io::string_view{"exited/closed"}};
            screen_.set_status(::fast_io::concat_fast_io(phase," | stop=",::fast_io::mnp::dec(actual.stop_identifier),
                " | threads=",::fast_io::mnp::dec(actual.threads.size())));
            if(actual.execution != execution_status::stopped || actual.stop_identifier == 0u || actual.threads.size() != 1u)
            {
                for(auto layout : {console_tui::layout::src,console_tui::layout::assembly,console_tui::layout::registers,
                    console_tui::layout::wasm,console_tui::layout::wasip1})
                { screen_.set_panel(layout,"view unavailable: requires one currently stopped Wasm thread\n"); }
                return;
            }
            auto const& thread{actual.threads.front()}; auto const id{thread.identifier};
            auto const layout{screen_.current_layout()};
            auto const prefix{::fast_io::concat_fast_io("thread=",::fast_io::mnp::dec(id)," module=",
                ::fast_io::mnp::dec(thread.location.code_unit)," function=",::fast_io::mnp::dec(thread.location.function),
                " wasm-offset=",::fast_io::mnp::dec(thread.location.offset),"\n")};
            if(layout == console_tui::layout::src || layout == console_tui::layout::split)
            {
                auto const command{parse_console_command("frame")}; auto const frames{control_.execute(command)};
                auto const text{details::format_reply(frames,command)};
                auto source{::fast_io::string{}};
                if(thread.source && (!frames.source_frames_available || frames.selected_source_frame == 0u))
                {
                    auto const& at{*thread.source};
                    source.append(::fast_io::concat_fast_io(console_tui::clean(::fast_io::string_view{at.file.data(),at.file.size()}),":",
                        ::fast_io::mnp::dec(at.line),"\n"));
                    auto const path{screen_.source_override().empty() ? ::fast_io::string_view{at.file.data(),at.file.size()} : screen_.source_override()};
                    source.append(console_tui::source_text(path,at.line));
                }
                else { source.append("source text unavailable at this selected stop/frame\n"); }
                source.append(prefix); source.append(::fast_io::string_view{text.data(),text.size()});
                screen_.set_panel(console_tui::layout::src,source);
            }
            if(layout == console_tui::layout::assembly || layout == console_tui::layout::split || layout == console_tui::layout::registers)
            { panel_reply(console_tui::layout::assembly,::fast_io::concat_fast_io("disassemble ",::fast_io::mnp::dec(id),
                " ",::fast_io::mnp::dec(actual.stop_identifier)," 16")); }
            if(layout == console_tui::layout::registers)
            { panel_reply(console_tui::layout::registers,::fast_io::concat_fast_io("info registers ",::fast_io::mnp::dec(id),
                " ",::fast_io::mnp::dec(actual.stop_identifier))); }
            if(layout == console_tui::layout::wasm)
            {
                auto const locals_command{parse_console_command(::fast_io::concat_fast_io("locals wasm ",::fast_io::mnp::dec(id)," 0 0 16").subview(0u))};
                auto const operands_command{parse_console_command(::fast_io::concat_fast_io("operands ",::fast_io::mnp::dec(id)," 0 0 16").subview(0u))};
                auto const locals{control_.execute(locals_command)}, operands{control_.execute(operands_command)};
                auto const l{details::format_reply(locals,locals_command)}, o{details::format_reply(operands,operands_command)};
                screen_.set_panel(layout,::fast_io::concat_fast_io(prefix,"[locals]\n",::fast_io::string_view{l.data(),l.size()},
                    "[operands]\n",::fast_io::string_view{o.data(),o.size()}));
            }
            if(layout == console_tui::layout::wasip1)
            { panel_reply(layout,::fast_io::concat_fast_io("info wasip1 fds ",::fast_io::mnp::dec(thread.location.code_unit)," 0 16")); }
        }
        [[nodiscard]] console_editing::action complete(console_editing::editor& editor)
        {
            candidates_={};
            auto const request{console_completion::context(editor.line().view(),editor.cursor())};
            if(request.type == console_completion::kind::path) { console_completion::paths(request,candidates_); }
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
            if(candidates_.unavailable && action == console_editing::action::unchanged) { output("\ncompletion unavailable for this path\n"); }
            return action;
        }
        void show_candidates(console_editing::editor const& editor,bool dynamic)
        {
            output("\n");
            if(!dynamic)
            { editor.display_candidates([&](::fast_io::string_view text) { output(text);output("\n"); });return; }
            for(::std::size_t i{}; i < candidates_.size; ++i) { output(candidates_.words[i].subview(0u)); output("\n"); }
            if(candidates_.truncated) { output("[completion truncated; type a longer prefix]\n"); }
        }
        [[nodiscard]] bool draw(console_editing::editor const& editor, console_editing::action action)
        {
            auto const before{screen_.enabled()}; auto const previous{screen_.current_layout()}; screen_.key(action);
            if(before != screen_.enabled() || previous != screen_.current_layout() || action == console_editing::action::redraw)
            { refresh_needed_=true; }
            if(before && !screen_.enabled())
            { io_.write(io_.context,"(uwvm-debug) "); io_.write(io_.context,editor.line().view()); }
            if(screen_.enabled()) { refresh(); screen_.render(editor); return true; }
            return false;
        }
        [[nodiscard]] console_read_hooks hooks() noexcept
        {
            return {this,
                +[](void* self,console_editing::editor& editor) { return static_cast<console_visual_session*>(self)->complete(editor); },
                +[](void* self,console_editing::editor const& editor,console_editing::action action) { return static_cast<console_visual_session*>(self)->draw(editor,action); },
                +[](void* self,console_editing::editor const& editor,bool dynamic) { static_cast<console_visual_session*>(self)->show_candidates(editor,dynamic); }};
        }
    };
    inline constexpr ::fast_io::string_view console_help{
        "tui enable|disable | tui source PATH | layout src|asm|split|regs|wasm|wasip1|next | focus next|prev|cmd|code\n"
        "Ctrl+X A toggles TUI; Ctrl+X 1/2 selects source/split; PgUp/PgDn scrolls the focused pane; Ctrl+L refreshes/resizes.\n"
        "TUI uses current authenticated replies; asm/register views never open VM/runtime code. Source text is bounded local file data.\n"
        "break MODULE SYMBOL | b MODULE SYMBOL (exact unambiguous live DWARF function; Tab completes names)\n"
        "break MODULE FUNCTION BYTE_OFFSET | break-source MODULE FILE:LINE | delete BREAKPOINT | enable [BREAKPOINT] | disable [BREAKPOINT] | ignore BREAKPOINT COUNT | info breakpoints\n"
        "break/break-source ... ignore COUNT installs the initial hit policy atomically; no unconditional registration window.\n"
        "break/break-source ... [ignore COUNT] if EXPR | condition BREAKPOINT [EXPR] (bounded read-only source condition; omitted EXPR clears)\n"
        "Conditions evaluate at a genuine complete cooperative stop, frame0 of the actual hitting thread; false resumes, unavailable retains the stop.\n"
        "continue | wait | pause | step wasm THREAD [into|over|out] | status | info threads | bt THREAD | locals THREAD\n"
        "step source THREAD [into|over|out] | step asm THREAD | locals source THREAD\n"
        "globals THREAD MODULE [FIRST COUNT] | info globals THREAD MODULE [FIRST COUNT]\n"
        "table THREAD MODULE TABLE [FIRST COUNT] | info table THREAD MODULE TABLE [FIRST COUNT]\n"
        "operands THREAD [FRAME [FIRST COUNT]] | info operands THREAD [FRAME [FIRST COUNT]]\n"
        "locals wasm THREAD [FRAME [FIRST COUNT]] (COUNT 1..64; actual typed stop, no host addresses)\n"
        "saved THREAD [FRAME [FIRST COUNT]] | controls THREAD [FRAME [FIRST COUNT]] | handlers THREAD [FRAME [FIRST COUNT]]\n"
        "control-params|control-results|handler-params THREAD FRAME CONTROL_OR_CLAUSE [FIRST COUNT]\n"
        "  Controls and handlers are lexical Wasm layouts; saved values use actual if parameters, not native catch state.\n"
        "members locals|operands|saved|globals|table THREAD MODULE FRAME TABLE ROOT FIRST COUNT [PATH...]\n"
        "path create locals|operands|saved|globals|table THREAD MODULE FRAME TABLE ROOT [PATH...]\n"
        "path extend SESSION HANDLE [PATH...] (returns new handle; retires parent)\n"
        "path members SESSION HANDLE FIRST COUNT | path clear\n"
        "  FRAME/TABLE zero where unused; ROOT/PATH are original indices, at most 16 edges and 64 members.\n"
        "info wasip1 args|env|fds|preopens MODULE [FIRST COUNT]\n"
        "trace wasip1 on [NAME|all] | off | clear | read [AFTER COUNT]\n"
        "set wasip1 file MODULE HEXBYTES | set wasip1 fd-dup MODULE FD OLD_BASE OLD_INHERIT\n"
        "unset wasip1 fd MODULE FD OLD_BASE OLD_INHERIT\n"
        "set wasip1 checkpoint MODULE SLOT | set wasip1 restore MODULE SLOT bindings|strict\n"
        "unset wasip1 checkpoint MODULE SLOT (SLOT 0..7; live environment only)\n"
        "set wasip1 export MODULE HEXPATH | set wasip1 import MODULE HEXPATH [RESOURCE=TARGET_FD,...]\n"
                "set wasip1 export-group HEXPATH MODULE... [if-stop N]\n"
                "set wasip1 import-group HEXPATH MODULE[:RESOURCE=FD,...]... [if-stop N]\n"
        "portable WASIp1: content external; configure target mnt with matching guest names; pair with Wasm checkpoint\n"
        "Checkpoint Wasm and WASIp1 together at the same cooperative stop. External IO is not rolled back.\n"
        "set wasm memory MODULE MEMORY THREAD OFFSET bytes HEX (1..256 bytes; guest address order)\n"
        "set wasm global MODULE GLOBAL THREAD bits i32|i64|f32|f64|v128 HEX [HEX]\n"
        "set wasm global MODULE GLOBAL THREAD null|i31 VALUE|function MODULE FUNCTION\n"
        "set wasm table MODULE TABLE ELEMENT THREAD null|i31 VALUE|function MODULE FUNCTION\n"
        "set wasm global|table ... from locals|operands|saved FRAME INDEX [path EDGES...]\n"
        "set wasm global|table ... from globals MODULE INDEX | from table MODULE TABLE ELEMENT\n"
        "set wasm global|table ... from handle SESSION HANDLE [path EDGES...]\n"
        "set wasm member THREAD locals|operands|saved FRAME INDEX [path EDGES...] at MEMBER SOURCE\n"
        "set wasm member THREAD globals MODULE INDEX | table MODULE TABLE ELEMENT [path EDGES...] at MEMBER SOURCE\n"
        "set wasm member THREAD handle SESSION HANDLE [path EDGES...] at MEMBER SOURCE\n"
        "set wasip1 arg|arg-insert MODULE INDEX HEX | unset wasip1 arg MODULE INDEX\n"
        "set wasip1 env MODULE HEXKEY HEXVALUE\n"
        "WASIp1 mutations may append: if-stop STOP_ID (rejects a different pause)\n"
        "unset wasip1 env MODULE HEXKEY | set wasip1 rights MODULE FD OLD_BASE OLD_INHERIT NEW_BASE NEW_INHERIT\n"
        "WASIp1 text uses hex bytes (- means empty), up to 4096 raw bytes; rights accept decimal or 0x hex and may only decrease.\n"
        "ni [THREAD] | nexti [THREAD] (current Wasm native trap; qualified instructions, same-owner direct branches and near-call continuations)\n"
        "ni requires a proved Wasm caller return point; host/VM callees run normally without exposing their code, registers, stack or memory. Unavailable continuations and unsupported returns retain the stop. First establish step asm THREAD.\n"
        "finish asm [THREAD] | fin asm [THREAD] (current native top frame; authenticated Wasm parent only)\n"
        "Native finish stops at a proved normal parent return. Root/host/unknown callers are refused; Ctrl+C/EH retain a real cooperative Wasm pause. Parent locals/source snapshots are unavailable.\n"
        "disassemble THREAD STOP_ID COUNT (1..32 instructions; no address or offset input)\n"
        "disassemble-range THREAD STOP_ID COUNT BYTE_OFFSET INSTRUCTION_OFFSET SYMBOLS (bounded owner; COUNT 1..32, SYMBOLS 0|1)\n"
        "memory MODULE MEMORY OFFSET LENGTH | replace MODULE FUNCTION GENERATION BODY_FILE | help | quit\n"
        "catch wasm EVENT MODULE FUNCTION|all | info wasm-events | delete|enable|disable wasm-event ID\n"
        "catch wasm trap MODULE FUNCTION|all (after failure; pre-trap operands; continue terminates)\n"
        "catch wasm uncaught MODULE FUNCTION|all (actual throw; before unwind; continue propagates)\n"
        "trace wasm on [EVENT] | trace wasm off|clear | trace wasm read [AFTER_SEQUENCE [COUNT]] (COUNT=1..128)\n"
        "wasm-script COMMAND; COMMAND (up to 16 diagnostic/policy commands, including ABI-checked replace; no resume/step or host evaluation)\n"
        "info registers [REGISTER] | info registers THREAD STOP_ID [REGISTER] (real native trap, top frame only)\n"
        "ptype NAME | ptype THREAD STOP_ID NAME (scoped C/C++/Rust type layout)\n"
        "print EXPR | print THREAD STOP_ID EXPR (bounded copied guest selectors, integer/f32/f64 arithmetic, builtin numeric casts and sizeof; no calls)\n"
        "print-frame THREAD STOP_ID FRAME EXPR | ptype-frame THREAD STOP_ID FRAME EXPR (explicit source frame, any expression prefix)\n"
        "Rust tuple fields: pair.0 / pair.1; Zig conventional pointer dereference: p.* / p.*.field (owned guest bytes only).\n"
        "display EXPR | info display | undisplay [ID] | enable|disable|delete display [ID]\n"
        "Displays: at most32 printable expressions of256 bytes; evaluate source frame0 at each new genuine sole-thread stop. Unavailable values retain the entry; no saved pointer/value is reused.\n"
        "Wasm EVENT: all|throw|exception|gc|memory|table|atomic|call|reference|simd|control|trap|uncaught. Opcode events are BEFORE instructions; trap/uncaught require actual runtime witnesses.\n"
        "Trace holds the newest 512 observations, reports overwritten entries, and never invokes scripts on a guest thread.\n"
        "wait observes a stop/guest exit for at most two seconds; Ctrl+C separately requests a cooperative pause.\n"
        "IDs and byte offsets are decimal; offsets are relative to the function expression and must name an emitted safe point.\n"
        "break-source compares embedded DWARF or source-map paths exactly and opens no source text file.\n"
        "locals source shows only current cooperative numeric direct-local/constant values; unsupported locations are unavailable.\n"
        "bt THREAD at a real native stop inspects up to 32 authenticated physical Wasm parents; complete/partial status is explicit, and no native stack bytes or registers are exposed.\n"
        "frames [THREAD STOP_ID [FIRST COUNT]], frame [N|THREAD STOP_ID N], up/down [COUNT|THREAD STOP_ID COUNT] select real source frames.\n"
        "frames wasm THREAD STOP_ID FIRST COUNT lists canonical Wasm frames without DWARF; pages contain at most128 rows and report the full total.\n"
        "Caller values require their authenticated saved source PC and typed packet; unavailable locations are reported; resume/replacement/native stepping retire selection.\n"
        "print/ptype THREAD STOP_ID FRAME NAME and locals source THREAD STOP_ID FRAME query a frame atomically without changing selection.\n"
        "step wasm (also step THREAD or s THREAD) resumes all participants and stops at the next emitted\n"
        "Wasm instruction safe point; other participants may advance before parking.\n"
        "Wasm over skips child/tail-successor activations; out follows this activation through tail calls to its caller or guest exit.\n"
        "Both use actual Wasm activation identities without DWARF; breakpoints and catchpoints interrupt over/out.\n"
        "source into/over execute the innermost activation; out follows a selected DWARF frame or the current physical source-map frame. Actual full-JIT activation identities and DWARF scopes or Source Map v3 coordinates are required.\n"
        "asm executes exactly one native instruction on a qualified LLVM-full JIT thread; other guests stay stopped.\n"
        "memory reads at most 256 bytes and requires all guest threads stopped.\n"
        "locals shows up to 256 typed values from the selected stopped thread.\n"
        "replacement requires an owner-controlled regular body file, exact function ABI, and a stopped guest without an active target frame.\n"
        "Use tools/debug/secure_server.py for an authorized server launched before guest entry.\n"
        "Interactive c/continue waits for an actual stop; c&/continue& runs in the background.\n"
        "Piped/script input keeps asynchronous continue; controller/server/DAP resume is unchanged.\n"
        "Ctrl+C cancels partial input and requests a cooperative pause; a blocked host timeout keeps its request pending.\n"
        "Up/Down or Ctrl+P/Ctrl+N recall 32 commands; Ctrl+A/E/B/F, Backspace, Ctrl+U/K/W edit the line.\nCtrl+_ or Ctrl+X Ctrl+U undo current input edits; Alt+F/B move by word and Alt+D/Alt+Backspace kill words.\n"
        "Ctrl+Y restores killed input text; the command still needs Enter and stays within 8448 input bytes (WASIp1 text4096, legacy grammar512, raw memory256 bytes).\n"
        "Ctrl+T transposes characters; Ctrl+V quotes printable input (newline/tab become spaces); Ctrl+O accepts and recalls the next history line.\n"
        "Ctrl/Alt+Left/Right move by word; Alt+< / Alt+> select first history / current draft; Alt+digits repeat bounded input edits.\n"
        "Bracketed paste treats newlines as spaces and requires Enter after its closing marker; overflow or incomplete markers reject the whole line.\n"
        "Ctrl+L redraws; Tab completes commands, replace/tui source paths and current source variables/members and break MODULE function symbols; Ctrl+R/S search history backward/forward, Ctrl+G restores the search draft. Alt+Y cycles the immediate prior yank.\n"
        "Empty Enter repeats eligible step/query commands; replacement/scripts/resume are never implicitly repeated.\n"
        "s/step, n/next, finish select source into/over/out; si/stepi, bt/where, disas use the sole current stopped thread.\n"
        "until/u FILE:LINE|LINE | advance/adv FILE:LINE|LINE (sole stopped thread, innermost frame); explicit until/advance THREAD STOP FILE:LINE|LINE.\n"
        "Exact embedded statement in the origin module; until skips recursive calls, advance may enter them; both stop after return.\n"
        "Explicit s THREAD/step THREAD retain Wasm stepping. i r, i b, i threads are info aliases.\n"
        "Ctrl+D deletes under the cursor; on empty input it behaves as EOF.\n"
        "Unique command prefixes (cont, hel, info reg) and GDB aliases (backtrace, f, d/del) are accepted; ambiguous prefixes are rejected.\n"
        "LLDB aliases: thread step-in/step-over/step-out/step-inst/step-inst-over, thread backtrace/list, process continue/interrupt/status, frame variable/select, register read, breakpoint list/set/delete.\n"
        "Breakpoints use explicit Wasm identities: b MODULE FUNCTION OFFSET or b MODULE FILE:LINE; unique published function symbols are supported; arbitrary native addresses and VM/host debugging remain unavailable.\n"
        "Managed CLI quit requests bounded real cleanup; pending keeps owners and permits status/help/quit retry.\n"
        "quit force explicitly terminates this process without claiming finalizers. CtrlD may retain a pending prompt.\n"
        "A standalone launcher may preselect process termination after actual pipe EOF cannot complete cleanup.\n"
        "Without an installed managed cleanup owner, quit/EOF retains the launcher's process termination policy.\n"};
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
        auto const text{::fast_io::concat_std(
            result.phase == managed_cli_shutdown_phase::completed ?
                ::fast_io::string_view{"managed shutdown complete: "} : ::fast_io::string_view{"error: managed shutdown: "},
            managed_cli_shutdown_phase_text(result.phase), " native-error=", ::fast_io::mnp::dec(result.native_error),
            result.deadline_expired ? ::fast_io::string_view{"; deadline expired; owners retained\n"} : ::fast_io::string_view{"\n"})};
        io.write(io.context, ::fast_io::string_view{text.data(), text.size()});
    }
    [[nodiscard]] inline console_exit run_console(controller& control, console_io io,
        managed_cli_shutdown* shutdown = nullptr, bool force_on_physical_eof = false)
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
        io.write(io.context, "UWVM LLVM full debugger. Type help for commands.\n");
        // The bounded history/kill/undo buffers outlive each command. Keep
        // their owner off the management thread stack: at -O0 the editor plus
        // execute() exceeded the ordinary 1 MiB Windows thread stack.
        auto editor_owner{::std::make_unique<console_editing::editor>()};
        auto& editor{*editor_owner};
        console_visual_session visual{control,io};
        auto const output{[&](::fast_io::string_view text) { visual.output(text); }};
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
        for(;;)
        {
            if(!visual.enabled()) { io.write(io.context,"(uwvm-debug) "); }
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
                auto const text{details::format_reply(reply, command)};
                output(::fast_io::string_view{text.data(), text.size()}); show_displays(reply); continue;
            }
            if(line.status == line_status::oversized)
            { editor.forget_repeat(); output("error: command exceeds 8448 input bytes or contains an unsupported escape; entire line discarded\n"); continue; }
            auto const input{::fast_io::string_view{line.bytes.data(), line.size}};
            auto const trimmed{console_execution::trim(input)};
            visual.submitted(input);
            if(visual.command(trimmed)) { editor.remember(input,false); continue; }
            if(trimmed == "quit force" || trimmed == "q force")
            {
                editor.forget_repeat();
                if(shutdown == nullptr)
                { output("error: explicit force termination requires a managed standalone CLI owner\n"); continue; }
                output("explicit force termination: managed cleanup is not claimed\n");
                return console_exit::terminate_process;
            }
            if(shutdown != nullptr && shutdown->requested())
            {
                auto const safe{console_aliases::parse(input, 0u, 0u).command};
                if(safe.kind != console_command_kind::help && safe.kind != console_command_kind::quit)
                {
                    editor.forget_repeat();
                    if(safe.kind == console_command_kind::protocol && safe.operation == ::uwvm2::utils::control::operation::status && !safe.wait_for_event)
                    { write_console_shutdown_result(io, shutdown->inspect()); }
                    else { output("error: shutdown retains active owners; only status/help/quit retry/quit force are allowed\n"); }
                    continue;
                }
            }
            ::std::uint64_t current_thread{}, current_stop{};
            auto const display_request{console_displays::parse(trimmed)};
            if(display_request.operation != console_displays::action::none)
            {
                editor.remember(input,false);
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
            auto const parsed{console_aliases::parse(resume_policy.recognized ? ::fast_io::string_view{"continue"} : input,
                current_thread, current_stop)};
            if(parsed.ambiguous)
            { editor.forget_repeat(); output("error: ambiguous command abbreviation; type the full command or use Tab\n"); continue; }
            if(parsed.current_thread_required)
            { editor.forget_repeat(); output("error: shorthand requires one currently stopped Wasm thread; use an explicit THREAD\n"); continue; }
            auto command{parsed.command};
            if(command.kind == console_command_kind::invalid && visual.function_breakpoint(trimmed,command) &&
                command.kind == console_command_kind::invalid) { editor.forget_repeat(); continue; }
            if(command.kind != console_command_kind::invalid && command.kind != console_command_kind::unsupported && command.kind != console_command_kind::empty)
            { editor.remember(input, console_aliases::repeatable(command)); }
            else if(command.kind != console_command_kind::empty) { editor.forget_repeat(); }
            switch(command.kind)
            {
                case console_command_kind::empty: continue;
                case console_command_kind::help: output(console_help); continue;
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
                case console_command_kind::invalid: output("error: invalid command; type help\n"); continue;
                case console_command_kind::source_step:
                case console_command_kind::wasm_step:
                case console_command_kind::source_breakpoint:
                case console_command_kind::source_locals:
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
            management_wait_interrupt_observation interruption{{io.context, io.interrupt_pending}};
            auto const reply{control.execute(command, interruption.borrow())};
            bool interrupt_acknowledged{};
            if(interruption.observed())
            {
                output("^C\n");
                if(io.consume_interrupt != nullptr) { static_cast<void>(io.consume_interrupt(io.context)); }
                interrupt_acknowledged = true;
            }
            auto const text{details::format_reply(reply, command)};
            output(::fast_io::string_view{text.data(), text.size()});
            show_displays(reply);
            if(io.foreground_execution && resume_policy.recognized && !resume_policy.background &&
               reply.status == ::uwvm2::utils::control::error::none &&
               (reply.execution == execution_status::running || reply.execution == execution_status::stopping))
            {
                auto const foreground{wait_console_foreground(control, io, interruption, interrupt_acknowledged)};
                if(foreground.interruption_timeout)
                { output("error: interruption deadline expired; complete pause is unavailable; actual request and owners retained\n"); }
                auto const report{details::format_reply(foreground.reply, parse_console_command("wait"))};
                output(::fast_io::string_view{report.data(), report.size()});
                show_displays(foreground.reply);
            }
        }
    }
    // Explicit host adapter for tests/embedders that reserve stdin exclusively
    // for management. The production launcher must use isolated console_io when
    // guests have stdin: sharing stdin is not a secure command transport.
    [[nodiscard]] inline console_exit run_console(controller& control,
        managed_cli_shutdown* shutdown = nullptr, bool force_on_physical_eof = false)
    {
        console_keyboard::native_session keyboard{};
        if(!keyboard.ready()) { return console_exit::input_failure; }
        // The adapter's scoped destructor restores terminal modes/handlers on
        // console exit. Guest retirement and full VM owner cleanup are separate
        // launcher obligations; no active guest may be joined indefinitely.
        return run_console(control, {::std::addressof(keyboard),
            [](void* context) noexcept { return static_cast<console_keyboard::native_session*>(context)->read_byte(); },
            [](void*, ::fast_io::string_view text) noexcept { ::fast_io::print(::fast_io::out(), text); },
            keyboard.interactive_display(),
            [](void* context) noexcept { return static_cast<console_keyboard::native_session*>(context)->interrupt_pending(); },
            keyboard.interactive(),
            [](void* context) noexcept { return static_cast<console_keyboard::native_session*>(context)->consume_interrupt(); }},
            shutdown, force_on_physical_eof);
    }
#endif
}
#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/pop_macros.h>
#endif
