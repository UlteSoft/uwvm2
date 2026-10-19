// Human CLI presentation only. These copied replies and scalar request labels
// never admit a stop, a memory read, a frame selection or a native operation.
#pragma once
#ifndef UWVM_MODULE
# include <algorithm>
# include <bit>
# include <memory>
# include <span>
# include <string_view>
# include <uwvm2/utils/macro/push_macros.h>
# include <fast_io.h>
# include <fast_io_dsal/string.h>
# include "command.h"
# include "controller.h"
# include "console_help.h"
# include "console_tui.h"
# include "console_aliases.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::console_messages
{
    using view = ::fast_io::string_view;
    [[nodiscard]] constexpr view topic(console_command const& command) noexcept
    {
        switch(command.launch_action)
        { case console_launch_action::run: return "run"; case console_launch_action::start: return "start";
          case console_launch_action::start_wasm: return "startw";
          case console_launch_action::start_instruction: return "starti"; case console_launch_action::kill: return "kill";
          default: break; }
        using enum console_command_kind;
        switch(command.kind)
        {
            case module_list: return "info modules";
            case execution_relocate: return "jump";
            case source_frame:
                if(command.wasm_frames) { return "wasm frames"; }
                switch(command.frame_action)
                { case source_frame_action::up: return "up"; case source_frame_action::down: return "down";
                  case source_frame_action::list: return "frames"; default: return "frame"; }
            case source_step: return command.source_run_to == source_run_to_policy::until ? view{"until"} :
                command.source_run_to == source_run_to_policy::advance ? view{"advance"} :
                command.source_policy == source_step_policy::over ? view{"next"} :
                command.source_policy == source_step_policy::out ? view{"finish"} : view{"step"};
            case source_value: return "print"; case source_type: return "ptype";
            case source_locals: return "info locals"; case source_breakpoint: return "breaksource"; case assembly_breakpoint: return "breakasm";
            case assembly_step: return "si"; case assembly_next: return "ni"; case assembly_finish: return "asm finish";
            case assembly_registers: return "info registers"; case assembly_disassemble: return "disas";
            case assembly_disassemble_range: return "disassemble-range";
            case wasm_state:
                if(command.wasm_state_request.member_page()) { return "members"; }
                switch(command.wasm_state_request.selected)
                {
                    case wasm_state::selection::locals: return "locals wasm";
                    case wasm_state::selection::operands: return "operands";
                    case wasm_state::selection::globals: return "globals";
                    case wasm_state::selection::table: return "table";
                    case wasm_state::selection::saved_parameters: return "saved";
                    case wasm_state::selection::controls: return "controls";
                    case wasm_state::selection::handlers: return "handlers";
                    case wasm_state::selection::control_parameters: return "control-params";
                    case wasm_state::selection::control_results: return "control-results";
                    case wasm_state::selection::handler_parameters: return "handler-params";
                }
                return "wasm values";
            case wasm_mutation:
                switch(command.wasm_mutation_request.target)
                {
                    case wasm_mutation::destination::global: return "set wasm global";
                    case wasm_mutation::destination::table: return "set wasm table";
                    case wasm_mutation::destination::member: case wasm_mutation::destination::member_path: return "set wasm member";
                    case wasm_mutation::destination::memory: return "set wasm memory";
                }
                return "wasm set";
            case wasm_path: return "path"; case wasm_event: return "wasm events";
            case wasm_script: return "wasm-script";
            case wasip1_state:
                switch(command.wasip1_state_request.operation)
                {
                    case wasip1_state::action::arguments: return "info wasip1 args";
                    case wasip1_state::action::environment: return "info wasip1 env";
                    case wasip1_state::action::descriptors: return "info wasip1 fds";
                    case wasip1_state::action::preopens: return "info wasip1 preopens";
                    case wasip1_state::action::replace_argument: return "set wasip1 arg";
                    case wasip1_state::action::insert_argument: return "set wasip1 arg-insert";
                    case wasip1_state::action::remove_argument: return "unset wasip1 arg";
                    case wasip1_state::action::set_environment: return "set wasip1 env";
                    case wasip1_state::action::remove_environment: return "unset wasip1 env";
                    case wasip1_state::action::reduce_rights: return "set wasip1 rights";
                    case wasip1_state::action::create_file: return "set wasip1 file";
                    case wasip1_state::action::duplicate_descriptor: return "set wasip1 fd-dup";
                    case wasip1_state::action::close_descriptor: return "unset wasip1 fd";
                    case wasip1_state::action::checkpoint_save: return "set wasip1 checkpoint";
                    case wasip1_state::action::checkpoint_restore: return "set wasip1 restore";
                    case wasip1_state::action::checkpoint_drop: return "unset wasip1 checkpoint";
                    case wasip1_state::action::portable_export: return "set wasip1 export";
                    case wasip1_state::action::portable_import: return "set wasip1 import";
                    case wasip1_state::action::portable_export_group: return "set wasip1 export-group";
                    case wasip1_state::action::portable_import_group: return "set wasip1 import-group";
                    default: return "trace wasip1";
                }
            case wasm_step: return "wasm step"; case replacement_file: return "replace";
            case breakpoint_control: return "breakpoints"; default: break;
        }
        using enum ::uwvm2::utils::control::operation;
        switch(command.operation)
        { case resume: return "continue"; case step: return "wasm step"; case read_memory: return "memory";
          case backtrace: return "bt"; case locals: return "locals"; case pause: return "pause";
          case breakpoint_set: case breakpoint_clear: case breakpoint_list: return "breakpoints";
          case replace_function: return "replace"; default: return "status"; }
    }
    [[nodiscard]] inline ::fast_io::string state_error(controller_reply const& reply)
    {
        switch(reply.execution)
        {
            case execution_status::exited: return ::fast_io::concat_fast_io("error: The program has exited. Use \"run\" to start again.\n");
            case execution_status::closed: return ::fast_io::concat_fast_io("error: The debugging session is closed. Restart the debugger.\n");
            case execution_status::running: return ::fast_io::concat_fast_io("error: The program is running. Press Ctrl+C or use \"pause\", then retry.\n");
            case execution_status::stopping: return ::fast_io::concat_fast_io("error: A pause is pending. Use \"wait\" until the program stops, then retry.\n");
            case execution_status::stopped:
                if(reply.reason == stop_reason::initial)
                { return ::fast_io::concat_fast_io("error: No stack yet: the program has not started.\nSet a breakpoint, then use \"run\". Type \"help break\" for an example.\n"); }
                break;
        }
        return {};
    }
    [[nodiscard]] constexpr bool source_query(console_command const& command) noexcept
    { return command.kind == console_command_kind::source_frame || command.kind == console_command_kind::source_value ||
        command.kind == console_command_kind::source_type || command.kind == console_command_kind::source_locals ||
        command.kind == console_command_kind::source_step; }
    [[nodiscard]] inline ::fast_io::string error(controller_reply const& reply, console_command const& command)
    {
        if(reply.source_frame_out_of_range)
        {
            if(command.frame_action == source_frame_action::up && command.source_frame_ordinal == 1u)
            { return ::fast_io::concat_fast_io("error: Already at the outermost frame. Use \"down\" to return toward frame #0.\n"); }
            if(command.frame_action == source_frame_action::down && command.source_frame_ordinal == 1u)
            { return ::fast_io::concat_fast_io("error: Already at the innermost frame (#0). Use \"up\" to view its caller.\n"); }
            return ::fast_io::concat_fast_io("error: Requested frame movement/number is out of range. Selection unchanged.\nUse \"frames\" to list the available frames; type \"help ", topic(command), "\" for usage.\n");
        }
        if(reply.source_frame_step_unavailable)
        { return ::fast_io::concat_fast_io("error: Cannot step from this selected outer frame. Use \"frame 0\", then retry.\nUse \"finish\" to run until the selected source frame returns.\n"); }
        if(reply.source_frame_caller_unavailable)
        { return ::fast_io::concat_fast_io("error: This caller frame has no captured source variables.\nUse \"frame 0\" to inspect the current function, or \"bt\" to inspect the call stack.\n"); }
        using enum ::uwvm2::utils::control::error;
        if(command.kind == console_command_kind::assembly_breakpoint)
        {
            if(reply.status == invalid_state)
            { return ::fast_io::concat_fast_io("error: breakasm requires the current real Wasm JIT native trap.\nSet bw/bs, run, then si; use disas to choose a function-offset.\n"); }
            if(reply.status == breakpoint_not_executable)
            { return ::fast_io::concat_fast_io("error: This offset is not a verified visible Wasm JIT instruction.\nCopy a decimal function-offset from disas. VM/adapter bytes, instruction\ninteriors and offsets outside the current function are rejected.\n"); }
        }
        if(reply.status == invalid_state)
        {
            auto state{state_error(reply)}; if(!state.empty()) { return state; }
            if(command.disassembly_stop_identifier != 0u && command.disassembly_stop_identifier != reply.stop_identifier)
            { return ::fast_io::concat_fast_io("error: Stop ", ::fast_io::mnp::dec(command.disassembly_stop_identifier),
                " is no longer current. Use \"status\" and retry with its current THREAD/STOP labels.\n"); }
            auto thread{command.requested_step_thread};
            if(command.kind == console_command_kind::wasm_state) { thread = command.wasm_state_request.participant; }
            if(command.kind == console_command_kind::wasm_mutation) { thread = command.wasm_mutation_request.participant; }
            if(thread != 0u)
            {
                bool found{}; for(auto const& row : reply.threads) { found = found || row.identifier == thread; }
                if(!found) { return ::fast_io::concat_fast_io("error: Thread ", ::fast_io::mnp::dec(thread),
                    " is not in the current stopped program. Use \"info threads\" to choose a current thread.\n"); }
            }
            if(source_query(command) && (reply.reason == stop_reason::native_step || reply.reason == stop_reason::native_entry || reply.reason == stop_reason::native_breakpoint))
            { return ::fast_io::concat_fast_io("error: Source frame inspection is unavailable at this native instruction stop.\nUse \"disas\" or \"info registers\" here; continue to a Wasm/source breakpoint for source frames.\n"); }
            if(command.kind == console_command_kind::assembly_registers)
            { return ::fast_io::concat_fast_io("error: Registers require a native instruction stop. Use \"si\", then \"info registers\".\nOnly registers belonging to the stopped Wasm JIT context are available.\n"); }
            if(thread == 0u && reply.threads.size() > 1u)
            { return ::fast_io::concat_fast_io("error: More than one thread is stopped. Use \"info threads\" and supply THREAD explicitly.\nType \"help ", topic(command), " full\" for the explicit form.\n"); }
            return ::fast_io::concat_fast_io("error: This command cannot use the current stop or selection.\nUse \"status\" to check the state and current THREAD/STOP labels; type \"help ", topic(command), "\" for usage.\n");
        }
        if(command.kind == console_command_kind::source_breakpoint)
        {
            if(reply.status == source_debug_info_unavailable || reply.source_breakpoint_no_points)
            { return ::fast_io::concat_fast_io("error: Module ", ::fast_io::mnp::dec(command.source_module),
                " has no usable source breakpoint locations.\nIt may lack -g (or equivalent source debug information), have stripped symbols,\nno executable mapped statements, or a retired mapping after hot replacement.\nUse \"bs ", ::fast_io::mnp::dec(command.source_module), "\" to inspect the module; type \"help bs\" for usage.\n"); }
            if(reply.status == source_debug_info_invalid)
            { return ::fast_io::concat_fast_io("error: Module ", ::fast_io::mnp::dec(command.source_module),
                " has invalid or unsupported source debug information.\nCheck its DWARF/source map; use \"info modules\" to confirm the numeric module ID.\n"); }
            if(reply.status == source_location_unmapped)
            { return ::fast_io::concat_fast_io("error: No executable source statement matches this FILE:LINE in module ",
                ::fast_io::mnp::dec(command.source_module), ".\nUse the path recorded in the Wasm debug information. \"set source\" changes display only.\nUse \"info modules\" to confirm the numeric module ID; type \"help bs\" for usage.\n"); }
        }
        if(reply.status == source_debug_info_unavailable || reply.status == source_debug_info_invalid ||
           (command.kind == console_command_kind::source_frame && reply.status == source_location_unmapped))
        { return ::fast_io::concat_fast_io("error: No usable source debug information for this frame.\nCompile the Wasm program with -g; use \"bt\" for the Wasm stack or \"help wasm step\" to step without source lines.\n"); }
        if(reply.status == busy || reply.status == would_block)
        { return ::fast_io::concat_fast_io("error: Another debugger operation is still pending. Use \"wait\" or \"status\", then retry.\n"); }
        if(reply.status == unsupported_command && command.kind == console_command_kind::assembly_registers)
        { return ::fast_io::concat_fast_io("error: This native stop has no safe Wasm register snapshot.\nUse \"disas\" or continue to a new Wasm breakpoint, then try \"si\". Type \"help info registers\" for requirements.\n"); }
        if(reply.status == unsupported_backend || reply.status == unsupported_mode || reply.status == disabled)
        { return ::fast_io::concat_fast_io("error: This command requires an LLVM-full debugger session.\nStart a build that supports LLVM JIT with -Rdbg; type \"help ", topic(command), "\" for requirements.\n"); }
        if(reply.status == closed || reply.status == revoked || reply.status == wrong_instance || reply.status == stale_generation ||
           reply.status == unauthorized || reply.status == wrong_peer || reply.status == same_process)
        { return ::fast_io::concat_fast_io("error: This debugger connection/session is no longer valid. Reconnect the debugger client.\n"); }
        if(reply.status == none)
        {
            if(command.kind == console_command_kind::wasm_mutation && !reply.wasm_mutation_value.applied)
            {
                auto why{reply.wasm_mutation_value.status == wasm_state::status::unavailable_foreign_host_state ?
                    view{"the required guest and host state could not be confirmed stable"} :
                    wasm_state::status_text(reply.wasm_mutation_value.status)};
                switch(reply.wasm_mutation_value.reason)
                {
                    case wasm_mutation::refusal::immutable_global: why = "the selected global is immutable"; break;
                    case wasm_mutation::refusal::immutable_member: why = "the selected field is immutable"; break;
                    case wasm_mutation::refusal::type_mismatch: why = "the supplied value has a different Wasm type"; break;
                    case wasm_mutation::refusal::unavailable_source: why = "the selected source value is unavailable at this stop"; break;
                    case wasm_mutation::refusal::retention_failed: why = "the replacement reference could not be retained"; break;
                    case wasm_mutation::refusal::none:
                        if(reply.wasm_mutation_value.status == wasm_state::status::available) { why = "the edit was not applied"; }
                        break;
                }
                return ::fast_io::concat_fast_io("error: Wasm edit refused: ", why, ".\nNo change was made. Type \"help ", topic(command), "\" for requirements.\n");
            }
            if(command.kind == console_command_kind::wasm_state && reply.wasm_state_values.result != wasm_state::status::available)
            { return ::fast_io::concat_fast_io("error: Wasm state unavailable: ", wasm_state::status_text(reply.wasm_state_values.result),
                ".\nType \"help ", topic(command), "\" for requirements.\n"); }
            if(command.kind == console_command_kind::wasm_path && reply.wasm_path_values.result != wasm_state::status::available)
            { return ::fast_io::concat_fast_io("error: Wasm path unavailable: ", wasm_state::status_text(reply.wasm_path_values.result),
                ".\nUse \"path clear\" to discard old handles; type \"help path\" for requirements.\n"); }
            if(command.kind == console_command_kind::wasip1_state && reply.wasip1_state_values.result != wasip1_state::status::ok &&
               !(command.wasip1_state_request.operation >= wasip1_state::action::trace_enable &&
                 command.wasip1_state_request.operation <= wasip1_state::action::trace_read))
            { return ::fast_io::concat_fast_io("error: WASIp1 command did not complete: ", wasip1_state::status_text(reply.wasip1_state_values.result),
                ".\nType \"help ", topic(command), "\" for requirements.\n"); }
        }
        return {};
    }
    // Only call after the ordinary formatter has validated the bounded copied
    // reply. No new code/register reads or wider publication are introduced.
    [[nodiscard]] inline ::fast_io::string native_view(controller_reply const& reply, console_command const& command, view raw)
    {
        if((command.kind == console_command_kind::assembly_disassemble || command.kind == console_command_kind::assembly_disassemble_range) &&
           (raw.starts_with("native-disassembly stop=") || raw.starts_with("native-disassembly-range stop=")) &&
           reply.disassembly_count != 0u && reply.disassembly_count <= reply.disassembly.size())
        {
            bool available{};
            for(::std::size_t i{}; i < reply.disassembly_count; ++i) { available = available || static_cast<bool>(reply.disassembly[i]); }
            if(!available)
            { return ::fast_io::concat_fast_io("error: No instruction in this window could be safely displayed as Wasm JIT code.\nUse \"si\" or \"ni\" to reach another native stop; type \"help disas\" for requirements.\n"); }
        }
        if(command.kind == console_command_kind::assembly_registers && command.register_name_size == 0u && raw.starts_with("native-registers stop="))
        {
            bool numeric{}; auto const pc{native_registers::pc_index(reply.registers.machine)};
            for(::std::size_t i{}; i < reply.registers.size(); ++i) { numeric = numeric || (i != pc && reply.registers.known_bits[i] != 0u); }
            constexpr view note{"Only the Wasm program counter is shown in this register view. Other\ndisplayed registers lack a verified Wasm value mapping. Type \"help info registers\" for details.\n"};
            if(!numeric && raw.size() <= 65536u - note.size()) { return ::fast_io::concat_fast_io(raw, note); }
        }
        return {};
    }
    inline constexpr ::std::size_t human_output_budget{63u * 1024u};
    [[nodiscard]] inline bool append_row(::fast_io::string& text, ::fast_io::string const& row)
    {
        constexpr auto capacity{human_output_budget - 256u};
        if(text.size() > capacity || row.size() > capacity - text.size()) { return false; }
        text.append(row); return true;
    }
    inline void truncated(::fast_io::string& text, console_command const& command)
    { text.append(::fast_io::concat_fast_io("Output truncated. Type \"help ", topic(command), " full\" for detailed queries.\n")); }
    // Only display already copied scalar bits. A guest pointer stays explicitly
    // marked guest; this formatter never dereferences it or obtains more state.
    template<typename stream_type>
    inline void numeric(stream_type stream, source_dwarf::numeric_kind kind, ::std::uint64_t bits, ::std::uint8_t width)
    {
        if(width != 1u && width != 2u && width != 4u && width != 8u)
        { ::fast_io::io::print(stream, "<unavailable: invalid scalar width>"); return; }
        switch(kind)
        {
            case source_dwarf::numeric_kind::boolean: ::fast_io::io::print(stream, bits != 0u ? view{"true"} : view{"false"}); break;
            case source_dwarf::numeric_kind::signed_integer:
            {
                source_dwarf::numeric_variable value{}; value.bits = bits; value.byte_count = width;
                ::fast_io::io::print(stream, ::fast_io::mnp::dec(source_dwarf::numeric_signed_value(value))); break;
            }
            case source_dwarf::numeric_kind::unsigned_integer: ::fast_io::io::print(stream, ::fast_io::mnp::dec(bits)); break;
            case source_dwarf::numeric_kind::f32_bits:
                if(width == 4u) { ::fast_io::io::print(stream, ::std::bit_cast<float>(static_cast<::std::uint32_t>(bits))); }
                else { ::fast_io::io::print(stream, "<unavailable: invalid float width>"); } break;
            case source_dwarf::numeric_kind::f64_bits:
                if(width == 8u) { ::fast_io::io::print(stream, ::std::bit_cast<double>(bits)); }
                else { ::fast_io::io::print(stream, "<unavailable: invalid float width>"); } break;
            default: ::fast_io::io::print(stream, "<unavailable: unsupported scalar>"); break;
        }
    }
    [[nodiscard]] inline bool pretty_visible(::std::span<source_dwarf::object_node const> nodes,::std::size_t index) noexcept
    {
        for(unsigned depth{};depth!=33u;++depth)
        {
            if(index >= nodes.size()) { return false; }
            auto const parent{nodes[index].parent};
            if(parent == source_dwarf::no_record) { return true; }
            if(parent >= index || (nodes[parent].pretty_available && !nodes[index].detached_copy)) { return false; }
            index = parent;
        }
        return false;
    }
    [[nodiscard]] inline ::fast_io::string source_values(controller_reply const& reply, console_command const& command, view raw)
    {
        bool const expression{command.kind == console_command_kind::source_value || command.kind == console_command_kind::source_type};
        bool const locals{command.kind == console_command_kind::source_locals && command.concise_source_locals};
        if((!expression && !locals) || raw.starts_with("error:") || reply.source_stop_identifier == 0u ||
           (expression && (command.requested_step_thread != 0u || command.disassembly_stop_identifier != 0u || command.source_frame_explicit)) ||
           command.source_variable_name_size > command.source_variable_name.size()) { return {}; }
        ::fast_io::string text{};
        bool const object{command.kind == console_command_kind::source_type || reply.source_object_value_available};
        if(object)
        {
            if(reply.source_object_type.empty() || reply.source_object_type.size() > 1024u ||
               !(raw.starts_with("source-value stop=") || raw.starts_with("source-type stop="))) { return {}; }
            for(::std::size_t i{}; i < reply.source_object_type.size(); ++i)
            {
                auto const& node{reply.source_object_type[i]};
                if(node.depth > 32u || (node.parent != source_dwarf::no_record && node.parent >= i)) { return {}; }
                if(command.kind == console_command_kind::source_value && !pretty_visible(reply.source_object_type,i)) { continue; }
                ::fast_io::string row{}; ::fast_io::ostring_ref_fast_io stream{::std::addressof(row)};
                for(::std::size_t depth{}; depth < node.depth; ++depth) { ::fast_io::io::print(stream, "  "); }
                auto const type{node.type_name.empty() ? source_dwarf::object_type_kind_text(node.kind) : ::std::string_view{node.type_name}};
                if(command.kind == console_command_kind::source_type)
                {
                    if(i == 0u) { ::fast_io::io::print(stream, "type = "); }
                    else { ::fast_io::io::print(stream, source_dwarf::escaped_metadata_text{node.name.empty() ? ::std::string_view{"<unnamed>"} : ::std::string_view{node.name}}, ": "); }
                    ::fast_io::io::print(stream, source_dwarf::escaped_metadata_text{type});
                    if(i != 0u)
                    {
                        ::fast_io::io::print(stream, " (offset ");
                        if(node.reason == source_dwarf::object_unavailable_reason::unknown_member_offset) { ::fast_io::io::print(stream, "unavailable"); }
                        else { ::fast_io::io::print(stream, ::fast_io::mnp::dec(node.byte_offset)); }
                        if(node.reason == source_dwarf::object_unavailable_reason::unknown_size) { ::fast_io::io::print(stream, ", size unavailable)"); }
                        else { ::fast_io::io::print(stream, ", ", ::fast_io::mnp::dec(node.byte_size), " bytes)"); }
                    }
                    if(node.bit_field) { ::fast_io::io::print(stream, " [", ::fast_io::mnp::dec(node.bit_width), " bit field]"); }
                    if(node.inherited) { ::fast_io::io::print(stream, " [base]"); }
                    if(node.reason != source_dwarf::object_unavailable_reason::none)
                    { ::fast_io::io::print(stream, " <unavailable: ", source_dwarf::object_reason_text(node.reason), ">"); }
                }
                else
                {
                    auto const name{i == 0u ? ::std::string_view{command.source_variable_name.data(), command.source_variable_name_size} :
                        node.name.empty() ? ::std::string_view{"<unnamed>"} : ::std::string_view{node.name}};
                    ::fast_io::io::print(stream, source_dwarf::escaped_metadata_text{name}, " = ");
                    if(node.pretty_available)
                    { ::fast_io::io::print(stream, source_dwarf::readable_metadata_text{node.pretty_summary}); }
                    else if(node.reason != source_dwarf::object_unavailable_reason::none)
                    { ::fast_io::io::print(stream, "<unavailable: ", source_dwarf::object_reason_text(node.reason), ">"); }
                    else if(node.value_available)
                    {
                        if(node.kind == source_dwarf::type_kind::pointer) { ::fast_io::io::print(stream, "guest:", ::fast_io::mnp::hex0x(node.bits)); }
                        else { numeric(stream, node.scalar_kind, node.bits, node.scalar_bytes); }
                        if(!node.enumerator.empty()) { ::fast_io::io::print(stream, " (", source_dwarf::escaped_metadata_text{node.enumerator}, ")"); }
                    }
                    else if(node.kind == source_dwarf::type_kind::structure || node.kind == source_dwarf::type_kind::class_type ||
                            node.kind == source_dwarf::type_kind::union_type || node.kind == source_dwarf::type_kind::array || node.variant_part || node.variant_case)
                    { ::fast_io::io::print(stream, "(", source_dwarf::escaped_metadata_text{type}, ")"); }
                    else { ::fast_io::io::print(stream, "<unavailable: value was not captured>"); }
                    if(node.display_text_available && !node.pretty_available)
                    { ::fast_io::io::print(stream, " [text: ", source_dwarf::escaped_metadata_text{node.display_text}, node.display_text_truncated ? view{"; truncated]"} : view{"]"}); }
                    if(node.pretty_available && node.pretty_truncated && node.pretty_omitted != 0u)
                    { ::fast_io::io::print(stream, " [", ::fast_io::mnp::dec(node.pretty_omitted), " pretty-printer elements omitted]"); }
                    if(node.variant_case) { ::fast_io::io::print(stream, node.active_variant ? view{" [active variant]"} : view{" [variant]"}); }
                }
                if(node.omitted_children != 0u) { ::fast_io::io::print(stream, " [", ::fast_io::mnp::dec(node.omitted_children), " elements omitted]"); }
                ::fast_io::io::print(stream, "\n");
                if(!append_row(text, row)) { truncated(text, command); break; }
            }
            return text;
        }
        if(!reply.source_locals_available)
        {
            constexpr view prefix{"source locals unavailable: "}; auto const at{raw.find(prefix)};
            if(at == ::fast_io::containers::npos) { return {}; }
            return ::fast_io::concat_fast_io("error: Source variables unavailable: ", raw.subview(at + prefix.size()),
                "Type \"help ", topic(command), "\" for requirements.\n");
        }
        if(reply.source_locals.size() > 1024u) { return {}; }
        if(reply.source_locals.empty()) { return ::fast_io::concat_fast_io("No source variables in the current scope.\n"); }
        bool object_preview{};
        for(auto const& value : reply.source_locals)
        {
            ::fast_io::string row{}; ::fast_io::ostring_ref_fast_io stream{::std::addressof(row)};
            auto const name{expression ? ::std::string_view{command.source_variable_name.data(), command.source_variable_name_size} : ::std::string_view{value.name}};
            ::fast_io::io::print(stream, source_dwarf::escaped_metadata_text{name}, " = ");
            if(value.kind == source_dwarf::numeric_kind::unavailable)
            {
                ::fast_io::io::print(stream, "<unavailable: ", source_dwarf::numeric_reason_text(value.reason), ">");
                object_preview = object_preview || value.reason == source_dwarf::numeric_unavailable_reason::unsupported_type;
            }
            else { numeric(stream, value.kind, value.bits, value.byte_count); }
            if(locals && value.parameter) { ::fast_io::io::print(stream, " [parameter]"); }
            ::fast_io::io::print(stream, "\n");
            if(!append_row(text, row)) { truncated(text, command); break; }
        }
        if(locals && object_preview)
        { text.append("Some entries have no scalar preview. Use \"print NAME\" for an object view or \"ptype NAME\" for its type.\n"); }
        return text;
    }
    [[nodiscard]] inline ::fast_io::string backtrace(controller_reply const& reply, console_command const& command, view raw)
    {
        if(!command.concise_backtrace || command.operation != ::uwvm2::utils::control::operation::backtrace ||
           !raw.starts_with("stopped:")) { return {}; }
        for(auto const& thread : reply.threads)
        {
            if(thread.identifier != command.requested_step_thread || !thread.trace || thread.trace->frames().empty()) { continue; }
            ::fast_io::string text{}; ::std::size_t index{};
            for(auto const& frame : thread.trace->frames())
            {
                if(index == 128u) { truncated(text, command); return text; }
                ::fast_io::string row{}; ::fast_io::ostring_ref_fast_io stream{::std::addressof(row)};
                ::fast_io::io::print(stream, "#", ::fast_io::mnp::dec(index), " ");
                if(frame.function_name.empty()) { ::fast_io::io::print(stream, "<unnamed>"); }
                else { ::fast_io::io::print(stream, source_dwarf::escaped_metadata_text{::std::string_view{reinterpret_cast<char const*>(frame.function_name.data()), frame.function_name.size()}}); }
                if(index == 0u && thread.source)
                { ::fast_io::io::print(stream, " at ", source_dwarf::escaped_metadata_text{thread.source->file}, ":", ::fast_io::mnp::dec(thread.source->line)); }
                ::fast_io::io::print(stream, " (Wasm module ", ::fast_io::mnp::dec(frame.module_id), ", function ", ::fast_io::mnp::dec(frame.function_index), ")\n");
                if(!append_row(text, row)) { truncated(text, command); return text; }
                ++index;
            }
            if(thread.trace->truncated()) { text.append("Partial stack: additional Wasm frames were not captured.\n"); }
            if(thread.source_inline_available && !thread.source_inline_frames.empty())
            { text.append("Inline source entries are available in the detailed \"bt THREAD\" query.\n"); }
            return text;
        }
        return ::fast_io::concat_fast_io("No Wasm call stack was captured at this stop. Type \"help bt\" for requirements.\n");
    }
    // Emit only the selected copied frame for GDB-style shorthand. Explicit
    // THREAD/STOP queries retain the bounded, detailed transport-like listing.
    [[nodiscard]] inline ::fast_io::string frame(controller_reply const& reply, console_command const& command)
    {
        if(command.kind != console_command_kind::source_frame || command.frame_action == source_frame_action::list ||
           command.payload_size != 0u || reply.selected_source_frame < reply.source_frame_first) { return {}; }
        auto const index{reply.selected_source_frame - reply.source_frame_first};
        if(index >= reply.source_frames.size()) { return {}; }
        auto const& row{reply.source_frames[static_cast<::std::size_t>(index)]};
        auto const name{console_tui::clean(view{row.name.data(), (::std::min)(row.name.size(), ::std::size_t{256u})})};
        ::fast_io::string single{}; for(char c : name) { if(c == '\n') { single.append("\\n"); } else { single.push_back(c); } }
        return ::fast_io::concat_fast_io("#", ::fast_io::mnp::dec(reply.selected_source_frame), " ", single,
            " (Wasm module ", ::fast_io::mnp::dec(row.module), ", function ", ::fast_io::mnp::dec(row.function), ")\n");
    }
    [[nodiscard]] inline bool single_typo(view word, view candidate) noexcept
    { return console_help::single_typo(word, candidate); }
    [[nodiscard]] inline ::fast_io::string undefined_command(view input)
    {
        auto const parts{console_aliases::split(input)};
        ::fast_io::array<view, 3u> suggestions{}; ::std::size_t count{};
        for(auto const& alias : console_help::aliases)
        {
            if(alias.name.find(" ") != ::fast_io::containers::npos || !single_typo(parts.word, alias.name)) { continue; }
            bool seen{}; for(::std::size_t i{}; i < count; ++i) { seen = seen || suggestions[i] == alias.name; }
            if(seen) { continue; }
            suggestions[count++] = alias.name; if(count == suggestions.size()) { break; }
        }
        auto text{::fast_io::concat_fast_io("error: Undefined command.")};
        if(count != 0u)
        {
            text.append(" Did you mean ");
            for(::std::size_t i{}; i < count; ++i)
            { if(i != 0u) { text.append(" or "); } text.push_back('"'); text.append(suggestions[i]); text.push_back('"'); }
            text.append("? No command was executed.");
        }
        text.append(" Type \"help\" for command classes.\n"); return text;
    }
    // Suggest only static direct children of the nearest command menu. The
    // misspelled word and following arguments are never echoed or evaluated.
    [[nodiscard]] inline ::fast_io::string subcommand_error(view input, console_help::prefix_menu const& menu)
    {
        auto const word{console_aliases::split(input.subview(menu.command.size())).word};
        ::fast_io::array<view, 3u> suggestions{}; ::std::size_t count{};
        for(auto const& alias : console_help::aliases)
        {
            if(alias.target.empty() || !alias.name.starts_with(menu.command) || alias.name.size() <= menu.command.size() ||
               alias.name[menu.command.size()] != ' ') { continue; }
            auto const child{alias.name.subview(menu.command.size() + 1u)};
            if(child.find(" ") != ::fast_io::containers::npos || !single_typo(word, child)) { continue; }
            bool seen{}; for(::std::size_t i{}; i < count; ++i) { seen = seen || suggestions[i] == alias.name; }
            if(!seen) { suggestions[count++] = alias.name; if(count == suggestions.size()) { break; } }
        }
        auto text{::fast_io::concat_fast_io("error: Unknown or incomplete subcommand.")};
        if(count != 0u)
        {
            text.append("\nDid you mean ");
            for(::std::size_t i{}; i < count; ++i)
            { if(i != 0u) { text.append(" or "); } text.push_back('"'); text.append(suggestions[i]); text.push_back('"'); }
            text.append("?\nNo command was executed.\n");
        }
        else { text.push_back(' '); }
        if(count == 1u)
        { text.append(::fast_io::concat_fast_io("Type \"help ", suggestions[0u], "\" for an explanation and example.\n")); }
        else
        { text.append(::fast_io::concat_fast_io("Type \"help ", menu.command,
            "\" to choose a command, then \"help COMMAND\" for its arguments.\n")); }
        return text;
    }
    [[nodiscard]] inline ::fast_io::string syntax_error(view input)
    {
        auto const normalized{console_aliases::normalize(input)};
        if(normalized.ambiguous) { return console_aliases::ambiguity_error(input); }
        if(normalized.valid && (normalized.head == "breaksource" || normalized.head == "breakwasm"))
        {
            auto const module{console_aliases::split(console_aliases::split(normalized.view()).tail).word};
            ::std::uint64_t id{};
            auto const scan{::fast_io::parse_by_scan(module.cbegin(), module.cend(), ::fast_io::mnp::dec_get<true, true>(id))};
            bool digits{!module.empty()}; for(char c : module) { digits = digits && c >= '0' && c <= '9'; }
            if(!digits || scan.code != ::fast_io::parse_code::ok || scan.iter != module.cend())
            { return ::fast_io::concat_fast_io("error: MODULE_ID must be an unsigned decimal number, not a filename or module name.\nUse \"info modules\" to find its numeric ID. Type \"help bw\" or \"help bs\" for usage.\n"); }
        }
        ::fast_io::string spelling{};
        if(normalized.valid && !normalized.view().empty()) { input = normalized.view(); }
        else if(!normalized.head.empty())
        {
            // A recognized root can still have invalid child arguments (e.g.
            // "thr list extra"). Keep that root's canonical spelling for the
            // documentation lookup instead of misreporting an unknown verb.
            auto const tail{console_aliases::split(input).tail};
            spelling = ::fast_io::concat_fast_io(normalized.head, tail.empty() ? view{} : view{" "}, tail);
            input = spelling.subview(0u);
        }
        console_help::entry const* selected{}; ::std::size_t longest{};
        for(auto const& alias : console_help::aliases)
        {
            if(alias.name.empty() || alias.target.empty() || alias.name.size() <= longest ||
               details::command_prefix_end(input, alias.name) == ::fast_io::containers::npos) { continue; }
            auto const item{console_help::find(alias.target)};
            if(item != nullptr && !item->group()) { selected = item; longest = alias.name.size(); }
        }
        if(selected == nullptr)
        {
            if(auto const menu{console_help::menu_context(input)}; menu != nullptr)
            { return subcommand_error(input, *menu); }
            return undefined_command(input);
        }
        return ::fast_io::concat_fast_io("error: Invalid arguments. Usage: ", console_help::primary_usage(selected->usage),
            "\nType \"help ", selected->key, "\" for an explanation and example.\n");
    }
}
#endif
#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/pop_macros.h>
#endif
