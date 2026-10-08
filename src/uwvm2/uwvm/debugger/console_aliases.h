/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstdint>
# include <fast_io_dsal/string.h>
# include <fast_io.h>
# include <fast_io_unit/string.h>
# include <fast_io_dsal/string_view.h>
# include <fast_io_dsal/array.h>
# include "command.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::console_aliases
{
    struct result { console_command command{}; bool current_thread_required{}, ambiguous{}; };
    [[nodiscard]] inline constexpr ::fast_io::string_view trim(::fast_io::string_view text) noexcept
    {
        while(!text.empty() && (text.front() == ' ' || text.front() == '\t')) { text = text.subview(1u); }
        while(!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r')) { text = text.subview(0u, text.size() - 1u); }
        return text;
    }
    struct normalized_input
    {
        ::fast_io::string text{};
        bool valid{true}, ambiguous{};
        [[nodiscard]] ::fast_io::string_view view() const noexcept { return ::fast_io::string_view{text.data(), text.size()}; }
    };
    struct word_tail { ::fast_io::string_view word{}, tail{}; };
    [[nodiscard]] inline constexpr word_tail split(::fast_io::string_view text) noexcept
    {
        text = trim(text);
        ::std::size_t end{};
        while(end < text.size() && text[end] != ' ' && text[end] != '\t') { ++end; }
        return {text.subview(0u, end), trim(text.subview(end))};
    }
    template<::std::size_t N>
    [[nodiscard]] inline constexpr ::fast_io::string_view resolve(::fast_io::string_view word,
        ::fast_io::array<::fast_io::string_view, N> const& names, bool& ambiguous) noexcept
    {
        if(word.empty()) { return {}; }
        for(auto name : names) { if(word == name) { return name; } }
        ::fast_io::string_view found{};
        for(auto name : names)
        {
            if(!name.starts_with(word)) { continue; }
            if(!found.empty()) { ambiguous = true; return {}; }
            found = name;
        }
        return found;
    }
    // Only command words are expanded. Argument bytes, file paths, register
    // names and expressions are never prefix matched or evaluated here.
    [[nodiscard]] inline normalized_input normalize(::fast_io::string_view input)
    {
        normalized_input result{};
        input = trim(input);
        if(input.size() > max_input_command_bytes) { result.valid = false; return result; }
        auto [name, tail]{split(input)};
        if(name.empty()) { return result; }
        if(name == "c") { name = "continue"; }
        else if(name == "s") { name = "step"; }
        else if(name == "n") { name = "next"; }
        else if(name == "u") { name = "until"; }
        else if(name == "adv") { name = "advance"; }
        else if(name == "si" || name == "stepi") { name = "si"; }
        else if(name == "ni") { name = "nexti"; }
        else if(name == "b") { name = "break"; }
        else if(name == "d" || name == "del") { name = "delete"; }
        else if(name == "f" || name == "fr") { name = "frame"; }
        else if(name == "i") { name = "info"; }
        else if(name == "h") { name = "help"; }
        else if(name == "q") { name = "quit"; }
        else if(name == "p") { name = "print"; }
        else if(name == "br") { name = "breakpoint"; }
        else if(name == "disas") { name = "disassemble"; }
        else if(name == "where" || name == "backtrace") { name = "bt"; }
        else
        {
            constexpr ::fast_io::array<::fast_io::string_view, 46u> names{{
                "advance", "until", "break", "break-source", "breakpoint", "bt", "catch", "continue", "delete", "disable", "disassemble",
                "disassemble-range", "down", "enable", "finish", "frame", "frames", "globals", "help", "info", "locals",
                "members", "memory", "next", "nexti", "operands", "pause", "print", "process", "ptype", "quit", "register",
                "replace", "saved", "server", "set", "si", "source-type", "source-value", "status", "step", "table",
                "thread", "trace", "unset", "up"}};
            // Exact product commands outside the compatibility vocabulary keep
            // their existing grammar, including bounded scripts and checkpoints.
            bool exact_extra{name == "wait" || name == "wasm-script" || name == "wasm-path" || name == "wasm-set" ||
                name == "controls" || name == "handlers" || name == "control-params" || name == "control-results" || name == "handler-params"};
            if(!exact_extra)
            {
                name = resolve(name, names, result.ambiguous);
                if(name.empty())
                {
                    if(!result.ambiguous && parse_console_command(input).kind != console_command_kind::invalid)
                    { result.text = ::fast_io::concat_fast_io(input); return result; }
                    result.valid = false; return result;
                }
            }
        }
        if(name == "info")
        {
            auto [sub, rest]{split(tail)};
            if(sub == "r") { sub = "registers"; }
            else if(sub == "b") { sub = "breakpoints"; }
            else
            {
                constexpr ::fast_io::array<::fast_io::string_view, 15u> names{{"all-registers", "breakpoints", "globals", "locals",
                    "operands", "registers", "saved", "table", "threads", "wasm-events", "wasip1", "controls", "handlers",
                    "control-params", "control-results"}};
                sub = resolve(sub, names, result.ambiguous);
                if(sub.empty())
                {
                    if(!result.ambiguous && parse_console_command(input).kind != console_command_kind::invalid)
                    { result.text = ::fast_io::concat_fast_io(input); return result; }
                    result.valid = false; return result;
                }
            }
            result.text = rest.empty() ? ::fast_io::concat_fast_io("info ", sub) : ::fast_io::concat_fast_io("info ", sub, " ", rest);
        }
        else if(name == "thread" || name == "process" || name == "breakpoint" || name == "register" || name == "frame")
        {
            auto [sub, rest]{split(tail)};
            if(name == "thread")
            {
                constexpr ::fast_io::array<::fast_io::string_view, 7u> verbs{{"backtrace", "list", "step-in", "step-over", "step-out", "step-inst", "step-inst-over"}};
                sub = resolve(sub, verbs, result.ambiguous);
                if(sub == "list" && rest.empty()) { name = "info threads"; }
                else if(sub == "backtrace") { name = "bt"; }
                else if(sub == "step-in" && rest.empty()) { name = "step"; }
                else if(sub == "step-over" && rest.empty()) { name = "next"; }
                else if(sub == "step-out" && rest.empty()) { name = "finish"; }
                else if(sub == "step-inst" && rest.empty()) { name = "si"; }
                else if(sub == "step-inst-over" && rest.empty()) { name = "nexti"; }
                else { result.valid = false; return result; }
                tail = rest;
            }
            else if(name == "process")
            {
                constexpr ::fast_io::array<::fast_io::string_view, 3u> verbs{{"continue", "interrupt", "status"}};
                sub = resolve(sub, verbs, result.ambiguous);
                if(sub.empty() || !rest.empty()) { result.valid = false; return result; }
                name = sub == "interrupt" ? ::fast_io::string_view{"pause"} : sub;
                tail = {};
            }
            else if(name == "breakpoint")
            {
                constexpr ::fast_io::array<::fast_io::string_view, 5u> verbs{{"delete", "disable", "enable", "list", "set"}};
                sub = resolve(sub, verbs, result.ambiguous);
                if(sub.empty() || (sub == "list" && !rest.empty())) { result.valid = false; return result; }
                name = sub == "list" ? ::fast_io::string_view{"info breakpoints"} : sub == "set" ? ::fast_io::string_view{"break"} : sub;
                tail = rest;
            }
            else if(name == "register")
            {
                constexpr ::fast_io::array<::fast_io::string_view, 1u> verbs{{"read"}};
                sub = resolve(sub, verbs, result.ambiguous);
                if(sub.empty()) { result.valid = false; return result; }
                name = "info registers"; tail = rest;
            }
            else if(!sub.empty() && (sub == "v" || ::fast_io::string_view{"variable"}.starts_with(sub)))
            {
                name = rest.empty() ? ::fast_io::string_view{"info locals"} : ::fast_io::string_view{"print"}; tail = rest;
            }
            else if(sub == "select") { tail = rest; }
            if(result.text.empty()) { result.text = tail.empty() ? ::fast_io::concat_fast_io(name) : ::fast_io::concat_fast_io(name, " ", tail); }
        }
        else { result.text = tail.empty() ? ::fast_io::concat_fast_io(name) : ::fast_io::concat_fast_io(name, " ", tail); }
        if(result.view().starts_with("break "))
        {
            auto [module, location]{split(result.view().subview(6u))};
            ::std::uint64_t index{};
            if(!module.empty() && !location.empty() && location.find(":") != ::fast_io::containers::npos &&
                ::fast_io::char_category::is_c_digit(module.front()))
            {
                auto const scan{::fast_io::parse_by_scan(module.cbegin(), module.cend(), ::fast_io::mnp::dec_get<true, true>(index))};
                if(scan.code == ::fast_io::parse_code::ok && scan.iter == module.cend())
                { result.text = ::fast_io::concat_fast_io("break-source ", ::fast_io::mnp::dec(index), " ", location); }
            }
        }
        if(result.text.size() > max_input_command_bytes) { result.valid = false; }
        return result;
    }
    [[nodiscard]] inline bool uses_current_thread(::fast_io::string_view input)
    {
        auto const normalized{normalize(input)};
        if(!normalized.valid) { return false; }
        auto const text{normalized.view()};
        return text == "step" || text == "next" || text == "finish" || text == "si" || text == "bt" ||
            text == "info locals" || text == "disassemble" ||
            ((text.starts_with("until ") || text.starts_with("advance ")) &&
             parse_console_command(text).kind == console_command_kind::invalid);
    }
    // Labels supplied ONLY by a fresh manager inspect(), never copied from
    // guest memory or guessed from thread ordinal. Resolving an alias grants no
    // stop/source/native authority: controller.execute independently admits it.
    [[nodiscard]] inline result parse(::fast_io::string_view text, ::std::uint64_t sole_stopped_thread,
                                     ::std::uint64_t current_stop)
    {
        auto normalized{normalize(text)};
        if(!normalized.valid) { return {{}, false, normalized.ambiguous}; }
        text = normalized.view();
        if((text.starts_with("until ") || text.starts_with("advance ")) &&
           parse_console_command(text).kind == console_command_kind::invalid)
        {
            if(sole_stopped_thread == 0u || current_stop == 0u) { return {{},true}; }
            auto const parts{split(text)};
            auto const expanded{::fast_io::concat_fast_io(parts.word," ",::fast_io::mnp::dec(sole_stopped_thread),
                " ",::fast_io::mnp::dec(current_stop)," ",parts.tail)};
            return {parse_console_command(::fast_io::string_view{expanded.data(),expanded.size()}),false};
        }
        bool const source_into{text == "step"}, source_over{text == "next"}, source_out{text == "finish"};
        bool const native_into{text == "si"}, backtrace{text == "bt"}, locals{text == "info locals"}, disassemble{text == "disassemble"};
        if(source_into || source_over || source_out || native_into || backtrace || locals || disassemble)
        {
            if(sole_stopped_thread == 0u || current_stop == 0u) { return {{}, true}; }
            ::fast_io::string expanded;
            if(source_into || source_over || source_out)
            { expanded = ::fast_io::concat_fast_io("step source ", ::fast_io::mnp::dec(sole_stopped_thread),
                source_over ? ::fast_io::string_view{" over"} : source_out ? ::fast_io::string_view{" out"} : ::fast_io::string_view{" into"}); }
            else if(native_into) { expanded = ::fast_io::concat_fast_io("step asm ", ::fast_io::mnp::dec(sole_stopped_thread)); }
            else if(backtrace) { expanded = ::fast_io::concat_fast_io("bt ", ::fast_io::mnp::dec(sole_stopped_thread)); }
            else if(locals) { expanded = ::fast_io::concat_fast_io("locals source ", ::fast_io::mnp::dec(sole_stopped_thread)); }
            else { expanded = ::fast_io::concat_fast_io("disassemble ", ::fast_io::mnp::dec(sole_stopped_thread), " ", ::fast_io::mnp::dec(current_stop), " 32"); }
            auto command{parse_console_command(::fast_io::string_view{expanded.data(), expanded.size()})};
            command.source_selected_finish = source_out;
            return {command, false};
        }
        return {parse_console_command(text), false}; // explicit step THREAD retains Wasm grammar
    }
    [[nodiscard]] constexpr bool repeatable(console_command const& command) noexcept
    {
        using enum console_command_kind;
        switch(command.kind)
        {
            case source_step: case wasm_step: case source_locals: case source_type: case source_value:
            case assembly_step: case assembly_next: case assembly_finish: case assembly_disassemble:
            case assembly_disassemble_range: case assembly_registers: return true;
            case source_frame:
                return command.frame_action == source_frame_action::list || command.frame_action == source_frame_action::show;
            case protocol:
                using enum ::uwvm2::utils::control::operation;
                return command.operation == status || command.operation == pause || command.operation == step ||
                       command.operation == backtrace || command.operation == locals || command.operation == read_memory ||
                       command.operation == breakpoint_list;
            default: return false; // no implicit repeat of mutations/replacement/script/resume/quit
        }
    }
}
