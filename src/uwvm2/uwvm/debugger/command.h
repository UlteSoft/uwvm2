/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <fast_io_dsal/array.h>
# include <cstddef>
# include <cstdint>
# include <limits>
# include <fast_io.h>
# include <fast_io_dsal/string_view.h>
# include <uwvm2/utils/control/protocol.h>
# include "wasm_events.h"
# include "wasm_state.h"
# include "wasm_mutation.h"
# include "wasm_path.h"
# include "wasip1_state.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger
{
    inline constexpr ::std::size_t max_command_bytes{512};
    inline constexpr ::std::size_t max_input_command_bytes{wasip1_state::maximum_command_bytes}; // Long input is restricted to WASIp1 text edits; raw memory remains <=1024.
    using command_text_view = ::fast_io::string_view;
    enum class console_command_kind { protocol, source_step, source_breakpoint, source_locals, assembly_step, replacement_file, help, quit, unsupported, invalid, empty, assembly_disassemble, assembly_disassemble_range, wasm_event, wasm_script, assembly_registers, source_type, source_value, assembly_next, source_frame, wasm_state, wasip1_state, wasm_path, wasm_mutation, wasm_step, breakpoint_control, assembly_finish };
    enum class unavailable_step_level { none, source, assembly };
    enum class source_step_policy { into, over, out };
    enum class source_run_to_policy { none, until, advance };
    enum class wasm_step_policy { into, over, out };
    enum class source_frame_action { list, show, select, up, down };
    enum class breakpoint_action { enable, disable, ignore, condition };
    struct console_command
    {
        console_command_kind kind{console_command_kind::invalid};
        ::uwvm2::utils::control::operation operation{};
        ::fast_io::array<::uwvm2::utils::control::wire_byte, 32> payload{};
        ::std::size_t payload_size{};
        ::std::uint64_t replacement_module{}, replacement_function{}, replacement_generation{};
        ::fast_io::array<char, max_command_bytes> replacement_path{};
        ::std::size_t replacement_path_size{};
        ::std::uint64_t source_module{}, source_line{};
        ::fast_io::array<char, max_command_bytes> source_path{};
        ::std::size_t source_path_size{};
        bool wait_for_event{}; // Host observation using an authenticated status request.
        unavailable_step_level unavailable_step{unavailable_step_level::none};
        source_step_policy source_policy{source_step_policy::into};
        wasm_step_policy wasm_policy{wasm_step_policy::into};
        // HOST scheduling choice only. CLI finish/fin targets its selected
        // source frame; ordinary source out/DAP stepOut targets current execution.
        // This DATA flag grants no capture, read, frame or resume capability.
        bool source_selected_finish{};
        source_run_to_policy source_run_to{}; // bounded host scheduling DATA, not a frame credential.
        ::std::uint64_t requested_step_thread{};
        ::std::uint64_t disassembly_stop_identifier{}, disassembly_count{};
        ::std::int64_t disassembly_byte_offset{}, disassembly_instruction_offset{};
        bool disassembly_resolve_symbols{};
        wasm_events::request wasm_request{};
        wasm_state::request wasm_state_request{};
        wasm_mutation::request wasm_mutation_request{};
        wasm_path::request wasm_path_request{};
        wasip1_state::request wasip1_state_request{};
        ::fast_io::array<char, max_command_bytes> script_text{};
        ::std::size_t script_size{};
        ::fast_io::array<char, 32u> register_name{};
        ::std::size_t register_name_size{};
        ::fast_io::array<char, max_command_bytes> source_variable_name{};
        ::std::size_t source_variable_name_size{};
        source_frame_action frame_action{source_frame_action::show};
        ::std::uint64_t source_frame_ordinal{}, frame_page_first{}, frame_page_count{128u};
        bool frame_page_explicit{}, wasm_frames{};
        breakpoint_action breakpoint_policy{};
        ::std::uint64_t breakpoint_id{}, breakpoint_ignore{};
        bool breakpoint_initial_ignore{}; // Installed under the same lock as a new breakpoint.
        bool breakpoint_initial_condition{};
        ::fast_io::array<char, 256u> breakpoint_condition{};
        ::std::size_t breakpoint_condition_size{};
        bool source_frame_explicit{}; // per-request ordinal, never activation/read authority.
    };
    [[nodiscard]] inline constexpr ::fast_io::string_view unsupported_step_message(console_command const& command) noexcept
    {
        switch(command.unavailable_step)
        {
            case unavailable_step_level::source:
                return "error: source stepping unavailable: -g DWARF custom sections are not mapped to JIT safe points\n";
            case unavailable_step_level::assembly:
                return "error: assembly stepping unavailable: native single-step is not installed\n";
            case unavailable_step_level::none: return "error: command is unsupported\n";
        }
        return "error: command is unsupported\n";
    }
    namespace details
    {
        [[nodiscard]] constexpr bool parse_decimal(::fast_io::string_view text, ::std::uint64_t& value) noexcept
        {
            if(text.empty()) { return false; }
            ::std::uint64_t parsed{};
            // [safe bounded decimal bytes] unsafe (one-past)
            //                           ^^ end is formed from the string_view extent.
            auto const end{text.data() + text.size()};
            // Consume one complete unsigned decimal token; retain leading zeroes
            // while rejecting signs, whitespace, overflow and trailing characters.
            auto const result{::fast_io::parse_by_scan(text.data(), end, ::fast_io::mnp::dec_get<true, true>(parsed))};
            if(result.code != ::fast_io::parse_code::ok || result.iter != end) { return false; }
            value = parsed;
            return true;
        }
        [[nodiscard]] constexpr bool parse_signed_decimal(::fast_io::string_view text, ::std::int64_t& value) noexcept
        {
            if(text.empty()) { return false; }
            ::std::int64_t parsed{};
            // [safe bounded signed decimal bytes] unsafe (one-past)
            //                                  ^^ end derives only from string_view extent.
            auto const end{text.data() + text.size()};
            auto const result{::fast_io::parse_by_scan(text.data(), end, ::fast_io::mnp::dec_get<true, true>(parsed))};
            if(result.code != ::fast_io::parse_code::ok || result.iter != end) { return false; }
            value = parsed; return true;
        }
    }
    // This is a bounded host console grammar, not an evaluator. No expressions,
    // shell expansion, escape sequences, native addresses or arbitrary calls.
    [[nodiscard]] inline constexpr console_command parse_console_command(::fast_io::string_view line, bool allow_initial_ignore = true, bool allow_initial_condition = true) noexcept
    {
        if(line.size()>max_input_command_bytes) { return {}; }
        if(allow_initial_condition && line.size() <= max_command_bytes && (line.starts_with("break ") || line.starts_with("break-source ")))
        {
            constexpr ::fast_io::string_view marker{" if "};
            auto const at{line.rfind(marker)};
            if(at != ::fast_io::containers::npos)
            {
                auto const expression{line.subview(at+marker.size())};
                if(expression.empty() || expression.size() > 256u) { return {}; }
                auto result{parse_console_command(line.subview(0u,at),allow_initial_ignore,false)};
                if(result.operation != ::uwvm2::utils::control::operation::breakpoint_set ||
                   (result.kind != console_command_kind::protocol && result.kind != console_command_kind::source_breakpoint)) { return {}; }
                result.breakpoint_initial_condition = true; result.breakpoint_condition_size = expression.size();
                for(::std::size_t i{}; i != expression.size(); ++i) { result.breakpoint_condition[i] = expression[i]; }
                return result;
            }
        }
        if(line.starts_with("condition "))
        {
            auto tail{line.subview(10u)};
            while(!tail.empty() && tail.front() == ' ') { tail = tail.subview(1u); }
            ::std::size_t split{}; while(split != tail.size() && tail[split] != ' ') { ++split; }
            console_command result{};
            if(!details::parse_decimal(tail.subview(0u,split),result.breakpoint_id) || result.breakpoint_id == 0u) { return {}; }
            auto expression{tail.subview(split)};
            while(!expression.empty() && expression.front() == ' ') { expression = expression.subview(1u); }
            if(expression.size() > result.breakpoint_condition.size()) { return {}; }
            result.kind = console_command_kind::breakpoint_control;
            result.operation = ::uwvm2::utils::control::operation::status;
            result.breakpoint_policy = breakpoint_action::condition;
            result.breakpoint_condition_size = expression.size();
            for(::std::size_t i{}; i != expression.size(); ++i) { result.breakpoint_condition[i] = expression[i]; }
            return result;
        }
        if(allow_initial_ignore && line.size() <= max_command_bytes && (line.starts_with("break ") || line.starts_with("break-source ")))
        {
            constexpr ::fast_io::string_view marker{" ignore "};
            auto const at{line.rfind(marker)}; ::std::uint64_t count{};
            if(at != ::fast_io::containers::npos && details::parse_decimal(line.subview(at+marker.size()),count))
            {
                auto result{parse_console_command(line.subview(0u,at),false,false)};
                if(result.operation != ::uwvm2::utils::control::operation::breakpoint_set ||
                   (result.kind != console_command_kind::protocol && result.kind != console_command_kind::source_breakpoint)) { return {}; }
                result.breakpoint_initial_ignore = true; result.breakpoint_ignore = count; return result;
            }
        }
        if(line.size()>max_command_bytes)
        {
            constexpr ::fast_io::string_view raw_memory_prefix{"set wasm memory "};
            constexpr ::fast_io::string_view wasip1_prefix{"set wasip1 "};
            bool const wasip1{line.size() >= wasip1_prefix.size() && line.subview(0u, wasip1_prefix.size()) == wasip1_prefix};
            bool const memory{line.size() <= 1024u && line.size() >= raw_memory_prefix.size() && line.subview(0u, raw_memory_prefix.size()) == raw_memory_prefix};
            if(!wasip1 && !memory) { return {}; }
        }
        // WASIp1 is a separate host-environment debugger level. The parser owns
        // opaque argument/environment bytes; it grants no guest descriptor or
        // native file authority. Actual complete-cohort admission is repeated
        // by the runtime before reading or changing any selected environment.
        for(auto prefix : ::fast_io::array<::fast_io::string_view, 4u>{"info wasip1 ", "set wasip1 ", "unset wasip1 ", "trace wasip1 "})
        {
            if(line.size() >= prefix.size() && line.subview(0u, prefix.size()) == prefix)
            {
                console_command result{console_command_kind::wasip1_state};
                // Optional stop selector is DATA checked under the controller
                // lock immediately before the ordinary coherent admission.
                auto selected{line};
                constexpr ::fast_io::string_view guard{" if-stop "};
                auto const marker{line.find(guard)};
                if(marker != ::fast_io::containers::npos)
                {
                    if(!details::parse_decimal(line.subview(marker + guard.size()), result.disassembly_stop_identifier) ||
                       result.disassembly_stop_identifier == 0u) { return {}; }
                    selected = line.subview(0u, marker);
                }
                if(!wasip1_state::parse(selected, result.wasip1_state_request) ||
                   (result.disassembly_stop_identifier != 0u && !wasip1_state::is_mutation(result.wasip1_state_request.operation))) { return {}; }
                result.operation = ::uwvm2::utils::control::operation::status;
                return result;
            }
        }
        constexpr ::fast_io::string_view mutation_prefix{"set wasm "};
        if(line.size()>=mutation_prefix.size() && line.subview(0u,mutation_prefix.size())==mutation_prefix)
        {
            console_command result{console_command_kind::wasm_mutation};
            if(!wasm_mutation::parse(line,result.wasm_mutation_request)) { return {}; }
            result.operation=::uwvm2::utils::control::operation::status;return result;
        }
        constexpr ::fast_io::string_view path_prefix{"path "};
        if(line.size()>=path_prefix.size() && line.subview(0u,path_prefix.size())==path_prefix)
        {
            ::fast_io::array<::fast_io::string_view,25u> tokens{};::std::size_t cursor{},count{};
            while(cursor<line.size())
            {
                if(::fast_io::char_category::is_c_blank(line[cursor])) { ++cursor;continue; }
                auto const begin{cursor};
                while(cursor<line.size() && !::fast_io::char_category::is_c_blank(line[cursor])) { ++cursor; }
                if(count==tokens.size()) { return {}; }
                tokens[count++]=line.subview(begin,cursor-begin); // bounded complete source range before subview.
            }
            if(count<2u) { return {}; }
            console_command result{console_command_kind::wasm_path};result.operation=::uwvm2::utils::control::operation::status;
            auto& query{result.wasm_path_request};::std::size_t suffix_begin{};
            if(tokens[1]=="clear") { if(count!=2u) { return {}; }query.action=wasm_path::operation::clear; }
            else if(tokens[1]=="create")
            {
                if(count<8u || count>24u) { return {}; }
                query.action=wasm_path::operation::create;auto& root{query.root};root.count=root.member_count=1u;
                if(tokens[2]=="locals") { root.selected=wasm_state::selection::locals; }
                else if(tokens[2]=="operands") { root.selected=wasm_state::selection::operands; }
                else if(tokens[2]=="saved") { root.selected=wasm_state::selection::saved_parameters; }
                else if(tokens[2]=="globals") { root.selected=wasm_state::selection::globals; }
                else if(tokens[2]=="table") { root.selected=wasm_state::selection::table; }else{ return {}; }
                if(!details::parse_decimal(tokens[3],root.participant) || !details::parse_decimal(tokens[4],root.module) ||
                   !details::parse_decimal(tokens[5],root.frame) || !details::parse_decimal(tokens[6],root.index) ||
                   !details::parse_decimal(tokens[7],root.first)) { return {}; }
                suffix_begin=8u;
            }
            else if(tokens[1]=="extend")
            {
                if(count<5u || count>20u) { return {}; }
                query.action=wasm_path::operation::extend;
                if(!details::parse_decimal(tokens[2],query.session) || !details::parse_decimal(tokens[3],query.handle)) { return {}; }
                suffix_begin=4u;
            }
            else if(tokens[1]=="members")
            {
                if(count!=6u) { return {}; }query.action=wasm_path::operation::members;
                if(!details::parse_decimal(tokens[2],query.session) || !details::parse_decimal(tokens[3],query.handle) ||
                   !details::parse_decimal(tokens[4],query.first) || !details::parse_decimal(tokens[5],query.count)) { return {}; }
            }
            else { return {}; }
            if(suffix_begin!=0u)
            {
                query.suffix_size=static_cast<unsigned char>(count-suffix_begin); // checked exact token range <=16 BEFORE narrowing.
                for(::std::size_t i{};i!=query.suffix_size;++i)
                {
                    // [bounded suffix_begin..count][owned suffix0..16] end
                    // [safe] i<count-begin<=16 BEFORE both source/dest indexes.
                    if(!details::parse_decimal(tokens[suffix_begin+i],query.suffix[i])) { return {}; }
                }
            }
            if(!wasm_path::valid(query)) { return {}; }return result;
        }
        constexpr ::fast_io::string_view members_prefix{"members "};
        if(line.size() >= members_prefix.size() && line.subview(0u, members_prefix.size()) == members_prefix)
        {
            // Fixed root locus followed by at most16 ORIGINAL member indices.
            // Dense display object IDs and native tokens have no input syntax.
            ::fast_io::array<::fast_io::string_view, 26u> tokens{};
            ::std::size_t cursor{}, count{};
            while(cursor < line.size())
            {
                if(::fast_io::char_category::is_c_blank(line[cursor])) { ++cursor; continue; }
                auto const begin{cursor};
                while(cursor < line.size() && !::fast_io::char_category::is_c_blank(line[cursor])) { ++cursor; }
                if(count == tokens.size()) { return {}; }
                tokens[count++] = line.subview(begin, cursor - begin); // Both bounded integers before view formation.
            }
            if(count < 9u || count > 25u) { return {}; }
            console_command result{console_command_kind::wasm_state};
            auto& query{result.wasm_state_request}; query.count = 1u;
            if(tokens[1] == "locals") { query.selected = wasm_state::selection::locals; }
            else if(tokens[1] == "operands") { query.selected = wasm_state::selection::operands; }
            else if(tokens[1] == "saved") { query.selected = wasm_state::selection::saved_parameters; }
            else if(tokens[1] == "globals") { query.selected = wasm_state::selection::globals; }
            else if(tokens[1] == "table") { query.selected = wasm_state::selection::table; }
            else { return {}; }
            if(!details::parse_decimal(tokens[2], query.participant) || !details::parse_decimal(tokens[3], query.module) ||
               !details::parse_decimal(tokens[4], query.frame) || !details::parse_decimal(tokens[5], query.index) ||
               !details::parse_decimal(tokens[6], query.first) || !details::parse_decimal(tokens[7], query.member_first) ||
               !details::parse_decimal(tokens[8], query.member_count)) { return {}; }
            query.path_size = static_cast<unsigned char>(count - 9u); // count9..25 bounds narrowing BEFORE assignment.
            for(::std::size_t index{}; index != query.path_size; ++index)
            {
                // [bounded tokens9..count][fixed path0..16] end
                // [safe] index<path_size=count-9<=16 before BOTH array accesses.
                if(!details::parse_decimal(tokens[9u + index], query.path[index])) { return {}; }
            }
            // `members` always denotes a nonempty member page. Zero is the
            // ordinary root-query sentinel, not a valid member page size.
            if(query.member_count == 0u || !wasm_state::valid(query)) { return {}; }
            result.operation = ::uwvm2::utils::control::operation::status;
            return result;
        }
        constexpr ::fast_io::string_view script_prefix{"wasm-script "};
        if(line.size() > script_prefix.size() && line.subview(0u, script_prefix.size()) == script_prefix)
        {
            console_command result{console_command_kind::wasm_script};
            result.operation = ::uwvm2::utils::control::operation::status;
            result.script_size = line.size() - script_prefix.size();
            for(::std::size_t index{}; index != result.script_size; ++index)
            {
                // [bounded host command ... script prefix][script bytes] end
                // [safe                                              ] unsafe (one-past)
                //  ^^ source index < line.size(), destination < fixed command capacity.
                result.script_text[index] = line[script_prefix.size() + index];
            }
            return result;
        }
        // Source paths are opaque DWARF bytes and may contain spaces, non-ASCII
        // bytes, or a Windows drive colon. Parse from the final :LINE without
        // sending a path over UWC1 or opening a source file.
        constexpr ::fast_io::string_view break_prefix{"break-source "}, until_prefix{"until "}, advance_prefix{"advance "};
        bool const until{line.starts_with(until_prefix)}, advance{line.starts_with(advance_prefix)};
        auto const source_prefix{until ? until_prefix : advance ? advance_prefix : break_prefix};
        if(line.size() >= source_prefix.size() && line.subview(0u, source_prefix.size()) == source_prefix)
        {
            ::std::size_t cursor{source_prefix.size()};
            while(cursor < line.size() && line[cursor] == ' ')
            {
                // [bounded command bytes ...] command_end
                // [safe                     ] unsafe (one-past)
                //                           ^^ cursor advances only while < size.
                ++cursor;
            }
            auto const module_begin{cursor};
            while(cursor < line.size() && line[cursor] != ' ')
            {
                // [bounded command bytes ...] command_end
                // [safe                     ] unsafe (one-past)
                //                           ^^ cursor advances only while < size.
                ++cursor;
            }
            ::std::uint64_t module{};
            if(module_begin == cursor || !details::parse_decimal(line.subview(module_begin, cursor - module_begin), module)) { return {}; }
            while(cursor < line.size() && line[cursor] == ' ')
            {
                // [bounded command bytes ...] command_end
                // [safe                     ] unsafe (one-past)
                //                           ^^ cursor advances only while < size.
                ++cursor;
            }
            ::std::uint64_t stop{};
            if(until || advance)
            {
                auto const stop_begin{cursor};
                while(cursor < line.size() && line[cursor] != ' ') { ++cursor; }
                if(module == 0u || !details::parse_decimal(line.subview(stop_begin,cursor-stop_begin),stop) || stop == 0u) { return {}; }
                while(cursor < line.size() && line[cursor] == ' ') { ++cursor; }
            }
            auto const path_begin{cursor};
            ::std::size_t colon{line.size()};
            for(::std::size_t index{path_begin}; index != line.size(); ++index)
            {
                // [bounded command bytes ...] command_end
                // [safe                     ] unsafe (one-past)
                //  ^^ index advances to size only after the preceding checked read.
                auto const c{static_cast<unsigned char>(line[index])};
                if(::fast_io::char_category::is_c_cntrl(c)) { return {}; }
                if(c == ':') { colon = index; }
            }
            ::std::uint64_t source_line{};
            // A bare decimal line requests the genuine current captured file.
            // Only the controller resolves it after authenticating THREAD/STOP.
            bool const current_file{(until || advance) && details::parse_decimal(line.subview(path_begin),source_line) && source_line != 0u};
            if(!current_file && (colon == line.size() || colon == path_begin || colon + 1u == line.size() ||
               !details::parse_decimal(line.subview(colon + 1u), source_line) || source_line == 0u ||
               colon - path_begin >= max_command_bytes)) { return {}; }
            console_command result{console_command_kind::source_breakpoint};
            result.operation = ::uwvm2::utils::control::operation::breakpoint_set;
            result.source_module = module;
            if(until || advance)
            {
                result.kind = console_command_kind::source_step;
                result.operation = ::uwvm2::utils::control::operation::step;
                result.source_policy = source_step_policy::over;
                result.source_run_to = until ? source_run_to_policy::until : source_run_to_policy::advance;
                result.requested_step_thread = module; result.disassembly_stop_identifier = stop;
                result.payload_size = 8u;
                ::uwvm2::utils::control::output_buffer output{result.payload};
                ::fast_io::io::print(output,::fast_io::mnp::le_put<64>(module));
            }
            result.source_line = source_line;
            result.source_path_size = current_file ? 0u : colon - path_begin;
            for(::std::size_t index{}; index != result.source_path_size; ++index)
            {
                // [validated path bytes ...] path_end
                // [safe                   ] unsafe (one-past)
                //  ^^ both source and destination indices are bounded above.
                result.source_path[index] = line[path_begin + index];
            }
            return result;
        }
        // Keep the complete expression, including whitespace. Optional numeric
        // THREAD STOP [FRAME] selectors remain launch/control metadata only.
        ::std::size_t name_end{};
        while(name_end < line.size() && !::fast_io::char_category::is_c_space(line[name_end])) { ++name_end; }
        auto const expression_command{line.subview(0u, name_end)};
        if(expression_command == "print" || expression_command == "p" || expression_command == "source-value" ||
           expression_command == "ptype" || expression_command == "source-type" ||
           expression_command == "print-frame" || expression_command == "ptype-frame")
        {
            bool const explicit_frame{expression_command == "print-frame" || expression_command == "ptype-frame"};
            console_command result{expression_command == "ptype" || expression_command == "source-type" || expression_command == "ptype-frame" ?
                console_command_kind::source_type : console_command_kind::source_value};
            result.operation = ::uwvm2::utils::control::operation::locals;
            auto cursor{name_end}; auto const skip{[&]() noexcept
            { while(cursor < line.size() && ::fast_io::char_category::is_c_space(line[cursor])) { ++cursor; } }};
            skip(); auto expression_begin{cursor};
            auto const word{[&]() noexcept
            { auto const begin{cursor}; while(cursor < line.size() && !::fast_io::char_category::is_c_space(line[cursor])) { ++cursor; }
              auto const token{line.subview(begin,cursor-begin)}; skip(); return token; }};
            ::std::uint64_t thread{},stop{};
            auto const first{word()}; auto const second{word()};
            if(explicit_frame && (!details::parse_decimal(first,thread) || !details::parse_decimal(second,stop) || cursor==line.size()))
            { return {}; }
            if(details::parse_decimal(first,thread) && details::parse_decimal(second,stop) && cursor < line.size())
            {
                if(thread == 0u || stop == 0u) { return {}; }
                result.requested_step_thread = thread; result.disassembly_stop_identifier = stop;
                result.payload_size = 8u; ::uwvm2::utils::control::output_buffer output{result.payload};
                ::fast_io::io::print(output, ::fast_io::mnp::le_put<64>(thread));
                expression_begin = cursor; auto const frame_token{word()}; ::std::uint64_t frame{};
                if(explicit_frame)
                {
                    if(!details::parse_decimal(frame_token,frame) || cursor==line.size()) { return {}; }
                    result.source_frame_explicit=true;result.source_frame_ordinal=frame;expression_begin=cursor;
                }
                // A leading literal followed by an operator belongs to the
                // expression ("1 || missing"), not the legacy frame prefix.
                else if(details::parse_decimal(frame_token,frame) && cursor < line.size() &&
                   (::fast_io::char_category::is_c_alnum(line[cursor]) || line[cursor] == '_' || line[cursor] == '('))
                { result.source_frame_explicit = true; result.source_frame_ordinal = frame; expression_begin = cursor; }
            }
            auto const expression{line.subview(expression_begin)};
            if(expression.empty() || expression.size() > result.source_variable_name.size()) { return {}; }
            for(::std::size_t i{}; i != expression.size(); ++i)
            { if(::fast_io::char_category::is_c_cntrl(expression[i]) && expression[i] != '\t') { return {}; }
              result.source_variable_name[i] = expression[i]; }
            result.source_variable_name_size = expression.size(); return result;
        }
        ::fast_io::array<::fast_io::string_view, 7> words{};
        ::std::size_t count{}, cursor{};
        while(cursor != line.size())
        {
            if(::fast_io::char_category::is_c_blank(line[cursor]) || line[cursor] == '\r') { ++cursor; continue; }
            if(count == words.size()) { return {}; }
            auto const begin{cursor};
            while(cursor != line.size() && !::fast_io::char_category::is_c_blank(line[cursor]) && line[cursor] != '\r')
            {
                auto const c{static_cast<unsigned char>(line[cursor])};
                if(!::fast_io::char_category::is_c_graph(c)) { return {}; }
                ++cursor; // cursor <= line.size(); no pointer formed beyond this checked span.
            }
            words[count++] = line.subview(begin, cursor - begin);
        }
        if(count == 0u) { return {console_command_kind::empty}; }
        auto const name{words[0]};
        console_command result{console_command_kind::protocol};
        using enum ::uwvm2::utils::control::operation;
        ::uwvm2::utils::control::output_buffer output{result.payload};
        // State selectors are Wasm indices only. The authenticated STATUS
        // request does not mint a stop/root capability; the cold runtime reader
        // rechecks the real cohort, capture owners, generation and store borrow.
        bool const state_info{name == "info" && count >= 2u &&
            (words[1] == "globals" || words[1] == "table" || words[1] == "operands" || words[1] == "saved" ||
             words[1] == "controls" || words[1] == "handlers" || words[1] == "control-params" || words[1] == "control-results" || words[1] == "handler-params")};
        bool const state_locals{name == "locals" && count >= 2u && words[1] == "wasm"};
        if(name == "globals" || name == "table" || name == "operands" || name == "saved" || name == "controls" || name == "handlers" ||
           name == "control-params" || name == "control-results" || name == "handler-params" || state_info || state_locals)
        {
            auto const first_word{state_info || state_locals ? 2u : 1u};
            auto const state_name{state_info ? words[1] : state_locals ? ::fast_io::string_view{"locals"} : name};
            auto& query{result.wasm_state_request};
            query.selected = state_name == "globals" ? wasm_state::selection::globals :
                state_name == "table" ? wasm_state::selection::table :
                state_name == "locals" ? wasm_state::selection::locals : state_name == "saved" ? wasm_state::selection::saved_parameters :
                state_name == "controls" ? wasm_state::selection::controls : state_name == "handlers" ? wasm_state::selection::handlers :
                state_name == "control-params" ? wasm_state::selection::control_parameters : state_name == "control-results" ? wasm_state::selection::control_results :
                state_name == "handler-params" ? wasm_state::selection::handler_parameters : wasm_state::selection::operands;
            if(count <= first_word || !details::parse_decimal(words[first_word], query.participant)) { return {}; }
            auto next_word{first_word + 1u};
            if(query.selected == wasm_state::selection::globals || query.selected == wasm_state::selection::table)
            {
                if(count <= next_word || !details::parse_decimal(words[next_word], query.module)) { return {}; }
                ++next_word; // bounded by the preceding token-count check.
                if(query.selected == wasm_state::selection::table)
                {
                    if(count <= next_word || !details::parse_decimal(words[next_word], query.index)) { return {}; }
                    ++next_word; // one actual module-local table index, not a host address.
                }
            }
            else if(count != next_word)
            {
                if(!details::parse_decimal(words[next_word], query.frame)) { return {}; }
                ++next_word; // count > next_word before the original-index read.
            }
            if(wasm_state::declaration_selection(query.selected))
            {
                if(count <= next_word || !details::parse_decimal(words[next_word],query.index)) { return {}; }
                ++next_word; // complete token count proved BEFORE original tuple selector read.
            }
            if(count != next_word)
            {
                if(count - next_word != 2u || !details::parse_decimal(words[next_word], query.first) ||
                    !details::parse_decimal(words[next_word + 1u], query.count)) { return {}; }
            }
            if(!wasm_state::valid(query)) { return {}; }
            result.kind = console_command_kind::wasm_state;
            result.operation = status; result.payload_size = 0u;
            return result;
        }
        if(name == "trace" && count >= 3u && count <= 5u && words[1] == "wasm")
        {
            result.kind = console_command_kind::wasm_event; result.operation = status;
            if(words[2] == "on" && (count == 3u || (count == 4u && wasm_events::parse_category(words[3], result.wasm_request.event))))
            { result.wasm_request.command = wasm_events::command_kind::trace_enable; }
            else if(count == 3u && words[2] == "off") { result.wasm_request.command = wasm_events::command_kind::trace_disable; }
            else if(count == 3u && words[2] == "clear") { result.wasm_request.command = wasm_events::command_kind::trace_clear; }
            else if(words[2] == "read")
            {
                result.wasm_request.command = wasm_events::command_kind::trace_read;
                if((count >= 4u && !details::parse_decimal(words[3], result.wasm_request.after_sequence)) ||
                   (count == 5u && (!details::parse_decimal(words[4], result.wasm_request.maximum_records) ||
                       result.wasm_request.maximum_records == 0u || result.wasm_request.maximum_records > wasm_events::maximum_trace_page_records)))
                { return {}; }
            }
            else { return {}; }
            return result;
        }
        if(name == "catch" && count == 5u && words[1] == "wasm")
        {
            result.kind = console_command_kind::wasm_event; result.operation = status;
            result.wasm_request.command = wasm_events::command_kind::catch_set;
            if(!wasm_events::parse_category(words[2], result.wasm_request.event) ||
               !details::parse_decimal(words[3], result.wasm_request.module)) { return {}; }
            result.wasm_request.all_functions = words[4] == "all";
            if(!result.wasm_request.all_functions && !details::parse_decimal(words[4], result.wasm_request.function)) { return {}; }
            return result;
        }
        if(name == "info" && count == 2u && words[1] == "wasm-events")
        {
            result.kind = console_command_kind::wasm_event; result.operation = status;
            result.wasm_request.command = wasm_events::command_kind::catch_list; return result;
        }
        if((name == "delete" || name == "enable" || name == "disable") && count == 3u && words[1] == "wasm-event")
        {
            result.kind = console_command_kind::wasm_event; result.operation = status;
            result.wasm_request.command = name == "delete" ? wasm_events::command_kind::catch_delete :
                name == "enable" ? wasm_events::command_kind::catch_enable : wasm_events::command_kind::catch_disable;
            if(!details::parse_decimal(words[2], result.wasm_request.identifier) || result.wasm_request.identifier == 0u) { return {}; }
            return result;
        }
        if(name == "info" && count >= 2u && count <= 5u && (words[1] == "registers" || words[1] == "all-registers"))
        {
            result.kind = console_command_kind::assembly_registers;
            result.operation = backtrace;
            if(words[1] == "all-registers")
            {
                if(count != 2u) { return {}; }
                // [fixed host spelling "all"][owned bounded register selector]
                // [safe                     ][safe                            ]
                //  ^^ value-only display selector; no guest/native address or
                // new wire capability is introduced by this GDB spelling.
                result.register_name[0u] = 'a'; result.register_name[1u] = 'l'; result.register_name[2u] = 'l';
                result.register_name_size = 3u;
            }
            if(count >= 4u)
            {
                if(!details::parse_decimal(words[2], result.requested_step_thread) || result.requested_step_thread == 0u ||
                   !details::parse_decimal(words[3], result.disassembly_stop_identifier) || result.disassembly_stop_identifier == 0u) { return {}; }
                result.payload_size = 8u;
                ::fast_io::io::print(output, ::fast_io::mnp::le_put<64>(result.requested_step_thread));
            }
            if(count == 3u || count == 5u)
            {
                auto const requested{words[count - 1u]};
                if(requested.empty() || requested.size() > result.register_name.size()) { return {}; }
                result.register_name_size = requested.size();
                for(::std::size_t i{}; i != requested.size(); ++i)
                {
                    // [owned ASCII token ... size<=32] end
                    // [safe                          ] unsafe (one-past)
                    //  ^^ checked equal source/destination index; no pointer survives parsing.
                    result.register_name[i] = requested[i];
                }
            }
            return result;
        }
        if(name == "frames" || name == "frame" || name == "up" || name == "down")
        {
            bool const list{name == "frames"};
            bool const wasm{list && count == 6u && words[1] == "wasm"};
            bool const paged{list && (count == 5u || wasm)};
            if((list && count != 1u && count != 3u && !paged) || (!list && count != 1u && count != 2u && count != 4u)) { return {}; }
            result.kind = console_command_kind::source_frame; result.operation = backtrace;
            result.frame_action = list ? source_frame_action::list : name == "up" ? source_frame_action::up :
                name == "down" ? source_frame_action::down : count == 1u ? source_frame_action::show : source_frame_action::select;
            result.source_frame_ordinal = name == "up" || name == "down" ? 1u : 0u;
            result.wasm_frames = wasm; result.frame_page_explicit = paged;
            if(count >= 3u)
            {
                auto const shift{wasm ? 1u : 0u};
                if(!details::parse_decimal(words[1u + shift], result.requested_step_thread) || result.requested_step_thread == 0u ||
                   !details::parse_decimal(words[2u + shift], result.disassembly_stop_identifier) || result.disassembly_stop_identifier == 0u) { return {}; }
                result.payload_size = 8u;
                ::fast_io::io::print(output, ::fast_io::mnp::le_put<64>(result.requested_step_thread));
            }
            if(paged)
            {
                if(!details::parse_decimal(words[count - 2u], result.frame_page_first) ||
                   !details::parse_decimal(words[count - 1u], result.frame_page_count) ||
                   result.frame_page_count == 0u || result.frame_page_count > 128u) { return {}; }
            }
            else if((!list && count == 2u) || count == 4u)
            { if(!details::parse_decimal(words[count - 1u], result.source_frame_ordinal)) { return {}; } }
            return result;
        }
        if((name == "ptype" || name == "source-type" || name == "print" || name == "p" || name == "source-value") && (count == 2u || count == 4u || count == 5u))
        {
            result.kind = name == "ptype" || name == "source-type" ? console_command_kind::source_type : console_command_kind::source_value;
            result.operation = locals;
            if(count == 4u || count == 5u)
            {
                if(!details::parse_decimal(words[1], result.requested_step_thread) || result.requested_step_thread == 0u ||
                   !details::parse_decimal(words[2], result.disassembly_stop_identifier) || result.disassembly_stop_identifier == 0u) { return {}; }
                result.payload_size = 8u;
                ::fast_io::io::print(output, ::fast_io::mnp::le_put<64>(result.requested_step_thread));
            }
            if(count == 5u)
            { result.source_frame_explicit = true; if(!details::parse_decimal(words[3], result.source_frame_ordinal)) { return {}; } }
            auto const requested{words[count - 1u]};
            if(requested.empty() || requested.size() > result.source_variable_name.size()) { return {}; }
            result.source_variable_name_size = requested.size();
            for(::std::size_t i{}; i != requested.size(); ++i)
            {
                // [bounded ASCII metadata name ...] end
                // [safe                           ] unsafe (one-past)
                //  ^^ copied into command-owned storage; no input pointer is retained.
                result.source_variable_name[i] = requested[i];
            }
            return result;
        }
        if((name == "help" || name == "h") && count == 1u) { return {console_command_kind::help}; }
        if((name == "quit" || name == "q") && count == 1u) { return {console_command_kind::quit}; }
        if(name == "server") { return {console_command_kind::unsupported}; }
        if(name == "replace" && count == 5u)
        {
            if(!details::parse_decimal(words[1], result.replacement_module) ||
               !details::parse_decimal(words[2], result.replacement_function) ||
               !details::parse_decimal(words[3], result.replacement_generation) ||
               result.replacement_generation == 0u || words[4].empty() ||
               words[4].size() >= result.replacement_path.size()) { return {}; }
            result.kind = console_command_kind::replacement_file;
            result.operation = replace_function;
            result.replacement_path_size = words[4].size();
            for(::std::size_t index{}; index != result.replacement_path_size; ++index)
            {
                // [validated ASCII path token] index < size < fixed capacity.
                // [safe                      ] copy into owned command storage.
                //  ^^ no pointer into the caller's line survives parsing.
                result.replacement_path[index] = words[4][index];
            }
            result.replacement_path[result.replacement_path_size] = '\0';
            return result;
        }
        if(((name == "enable" || name == "disable") && (count == 1u || count == 2u)) || (name == "ignore" && count == 3u))
        {
            result.kind = console_command_kind::breakpoint_control; result.operation = status;
            result.breakpoint_policy = name == "enable" ? breakpoint_action::enable : name == "disable" ? breakpoint_action::disable : breakpoint_action::ignore;
            if(count > 1u && (!details::parse_decimal(words[1u],result.breakpoint_id) || result.breakpoint_id == 0u)) { return {}; }
            if(name == "ignore" && !details::parse_decimal(words[2u],result.breakpoint_ignore)) { return {}; }
            return result;
        }
        if((name == "continue" || name == "c") && count == 1u) { result.operation = resume; }
        else if(name == "wait" && count == 1u) { result.operation = status; result.wait_for_event = true; }
        else if(name == "pause" && count == 1u) { result.operation = pause; }
        else if((name == "status" && count == 1u) || (name == "info" && count == 2u && words[1] == "threads"))
        { result.operation = status; }
        else if(name == "info" && count == 2u && words[1] == "breakpoints") { result.operation = breakpoint_list; }
        else if((name == "break" || name == "b") && count == 4u)
        {
            result.operation = breakpoint_set;
            result.payload_size = 24u;
            for(::std::size_t i{}; i != 3u; ++i)
            {
                ::std::uint64_t value{};
                if(!details::parse_decimal(words[i + 1u], value)) { return {}; }
                ::fast_io::io::print(output, ::fast_io::mnp::le_put<64>(value));
            }
        }
        else if((name == "ni" || name == "nexti") && (count == 1u || count == 2u))
        {
            result.kind = console_command_kind::assembly_next; result.operation = step;
            // No frame/address/expression is accepted. An absent thread is
            // resolved only from the controller's genuine current native trap.
            if(count == 2u)
            {
                if(!details::parse_decimal(words[1], result.requested_step_thread) || result.requested_step_thread == 0u)
                { return {}; }
                result.payload_size = 8u;
                ::fast_io::io::print(output, ::fast_io::mnp::le_put<64>(result.requested_step_thread));
            }
        }
        else if((name == "finish" || name == "fin") && (count == 2u || count == 3u) && words[1] == "asm")
        {
            result.kind = console_command_kind::assembly_finish; result.operation = step;
            if(count == 3u)
            {
                if(!details::parse_decimal(words[2],result.requested_step_thread) || result.requested_step_thread == 0u) { return {}; }
                result.payload_size = 8u;
                ::fast_io::io::print(output,::fast_io::mnp::le_put<64>(result.requested_step_thread));
            }
        }
        else if(name == "disassemble" && count == 4u)
        {
            if(!details::parse_decimal(words[1], result.requested_step_thread) || result.requested_step_thread == 0u ||
               !details::parse_decimal(words[2], result.disassembly_stop_identifier) || result.disassembly_stop_identifier == 0u ||
               !details::parse_decimal(words[3], result.disassembly_count) || result.disassembly_count == 0u ||
               result.disassembly_count > 32u) { return {}; }
            result.kind = console_command_kind::assembly_disassemble;
            // Reuse the authenticated participant-only backtrace admission;
            // the cold runtime copy independently requires a private live owner.
            result.operation = backtrace; result.payload_size = 8u;
            ::fast_io::io::print(output, ::fast_io::mnp::le_put<64>(result.requested_step_thread));
        }
        else if(name == "disassemble-range" && count == 7u)
        {
            ::std::uint64_t symbols{};
            if(!details::parse_decimal(words[1], result.requested_step_thread) || result.requested_step_thread == 0u ||
               !details::parse_decimal(words[2], result.disassembly_stop_identifier) || result.disassembly_stop_identifier == 0u ||
               !details::parse_decimal(words[3], result.disassembly_count) || result.disassembly_count == 0u || result.disassembly_count > 32u ||
               !details::parse_signed_decimal(words[4], result.disassembly_byte_offset) ||
               result.disassembly_byte_offset < -65536 || result.disassembly_byte_offset > 65536 ||
               !details::parse_signed_decimal(words[5], result.disassembly_instruction_offset) ||
               result.disassembly_instruction_offset < -8704 || result.disassembly_instruction_offset > 8704 ||
               !details::parse_decimal(words[6], symbols) || symbols > 1u) { return {}; }
            result.disassembly_resolve_symbols = symbols != 0u;
            result.kind = console_command_kind::assembly_disassemble_range;
            result.operation = backtrace; result.payload_size = 8u;
            ::fast_io::io::print(output, ::fast_io::mnp::le_put<64>(result.requested_step_thread));
        }
        else if(name == "locals" && (count == 3u || count == 5u) && words[1] == "source")
        {
            ::std::uint64_t value{};
            if(!details::parse_decimal(words[2], value) || value == 0u) { return {}; }
            result.kind = console_command_kind::source_locals;
            result.requested_step_thread = value;
            if(count == 5u)
            {
                result.source_frame_explicit = true;
                if(!details::parse_decimal(words[3], result.disassembly_stop_identifier) || result.disassembly_stop_identifier == 0u ||
                   !details::parse_decimal(words[4], result.source_frame_ordinal)) { return {}; }
            }
            result.operation = locals; result.payload_size = 8u;
            ::fast_io::io::print(output, ::fast_io::mnp::le_put<64>(value));
        }
        else if((name == "finish" || name == "fin") && count == 2u)
        {
            ::std::uint64_t thread{};
            if(!details::parse_decimal(words[1], thread) || thread == 0u) { return {}; }
            result.kind = console_command_kind::source_step; result.operation = step;
            result.source_policy = source_step_policy::out; result.source_selected_finish = true;
            result.requested_step_thread = thread; result.payload_size = 8u;
            ::fast_io::io::print(output, ::fast_io::mnp::le_put<64>(thread));
        }
        else if((name == "delete" || name == "step" || name == "s" || name == "bt" || name == "locals") && count == 2u)
        {
            ::std::uint64_t value{};
            if(!details::parse_decimal(words[1], value) || value == 0u) { return {}; }
            result.operation = name == "delete" ? breakpoint_clear : name == "bt" ? backtrace : name == "locals" ? locals : step;
            result.payload_size = 8u;
            ::fast_io::io::print(output, ::fast_io::mnp::le_put<64>(value));
        }
        else if(name == "step" && (count == 3u || count == 4u))
        {
            ::std::uint64_t thread{};
            if(!details::parse_decimal(words[2], thread) || thread == 0u) { return {}; }
            result.requested_step_thread = thread;
            if(words[1] == "wasm" && (count == 3u ||
               (count == 4u && (words[3] == "into" || words[3] == "over" || words[3] == "out"))))
            {
                result.operation = step;
                result.payload_size = 8u;
                ::fast_io::io::print(output, ::fast_io::mnp::le_put<64>(thread));
                if(count == 4u && words[3] != "into")
                {
                    result.kind = console_command_kind::wasm_step;
                    result.wasm_policy = words[3] == "over" ? wasm_step_policy::over : wasm_step_policy::out;
                }
            }
            else if(words[1] == "source" && (count == 3u ||
                    (count == 4u && (words[3] == "into" || words[3] == "over" || words[3] == "out"))))
            {
                result.kind = console_command_kind::source_step;
                result.operation = step;
                result.payload_size = 8u;
                ::fast_io::io::print(output, ::fast_io::mnp::le_put<64>(thread));
                if(count == 4u)
                {
                    result.source_policy = words[3] == "over" ? source_step_policy::over :
                                           words[3] == "out" ? source_step_policy::out : source_step_policy::into;
                }
            }
            else if(words[1] == "asm" && count == 3u)
            {
                result.kind = console_command_kind::assembly_step;
                result.operation = step;
                result.payload_size = 8u;
                ::fast_io::io::print(output, ::fast_io::mnp::le_put<64>(thread));
            }
            else { return {}; }
        }
        else if(name == "memory" && count == 5u)
        {
            ::fast_io::array<::std::uint64_t, 4> fields{};
            for(::std::size_t i{}; i != fields.size(); ++i)
            { if(!details::parse_decimal(words[i + 1u], fields[i])) { return {}; } }
            // Console output is deliberately small; protocol clients retain the
            // independent 64 KiB maximum. Both reserved fields remain zero.
            if(fields[1] > (::std::numeric_limits<::std::uint32_t>::max)() ||
               fields[3] > 256u || fields[2] > (::std::numeric_limits<::std::uint64_t>::max)() - fields[3]) { return {}; }
            result.operation = read_memory;
            result.payload_size = 32u;
            ::fast_io::io::print(output, ::fast_io::mnp::le_put<64>(fields[0]),
                                ::fast_io::mnp::le_put<32>(static_cast<::std::uint32_t>(fields[1])), ::fast_io::mnp::le_put<32>(::std::uint32_t{}),
                                ::fast_io::mnp::le_put<64>(fields[2]),
                                ::fast_io::mnp::le_put<32>(static_cast<::std::uint32_t>(fields[3])), ::fast_io::mnp::le_put<32>(::std::uint32_t{}));
        }
        else { return {}; }
        return result;
    }
    struct wasm_script_commands
    {
        ::fast_io::array<console_command, 16u> commands{};
        ::std::size_t size{};
        bool valid{};
    };
    // A script is a bounded host command list, never a host/guest evaluator.
    // Parse the COMPLETE list before executing its first command. No nesting,
    // generic filesystem commands, resume/step, wait, arbitrary calls or shell
    // syntax. Replacement may read ONLY through its existing sealed body-file
    // command and still needs stopped-frame, exact ABI and generation checks.
    [[nodiscard]] inline constexpr wasm_script_commands parse_wasm_script(console_command const& script) noexcept
    {
        wasm_script_commands result{};
        if(script.kind != console_command_kind::wasm_script || script.script_size == 0u ||
           script.script_size > script.script_text.size()) { return result; }
        ::fast_io::string_view text{script.script_text.data(), script.script_size};
        ::std::size_t begin{};
        for(::std::size_t index{}; index <= text.size(); ++index)
        {
            // [owned bounded script bytes ...] end
            // [safe                          ] unsafe (one-past)
            //  ^^ index==size is only a delimiter; never dereference one-past.
            if(index != text.size() && text[index] != ';') { continue; }
            if(result.size == result.commands.size() || index == begin) { return {}; }
            auto command{parse_console_command(text.subview(begin, index - begin))};
            bool permitted{command.kind == console_command_kind::wasm_state || command.kind == console_command_kind::wasm_path || command.kind == console_command_kind::wasm_mutation || command.kind == console_command_kind::wasip1_state ||
                command.kind == console_command_kind::wasm_event || command.kind == console_command_kind::replacement_file ||
                command.kind == console_command_kind::source_type || command.kind == console_command_kind::source_value || command.kind == console_command_kind::source_locals ||
                command.kind == console_command_kind::assembly_registers};
            if(command.kind == console_command_kind::protocol && !command.wait_for_event)
            {
                using enum ::uwvm2::utils::control::operation;
                permitted = command.operation == status || command.operation == backtrace || command.operation == locals ||
                    command.operation == read_memory || command.operation == breakpoint_set ||
                    command.operation == breakpoint_clear || command.operation == breakpoint_list;
            }
            if(!permitted) { return {}; }
            result.commands[result.size++] = command;
            // [complete current command][next command ...] end
            // [safe                                    ] unsafe (one-past)
            //                            ^^ index<size before forming next offset;
            // final delimiter at size is never incremented into a borrowed view.
            if(index != text.size()) { begin = index + 1u; }
        }
        result.valid = result.size != 0u; return result;
    }
}
