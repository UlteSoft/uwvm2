/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <fast_io_dsal/string_view.h>
# include "console_aliases.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::console_execution
{
    struct resume_policy
    { bool recognized{}, background{}, from_entry{}; console_launch_action launch_action{}; };
    [[nodiscard]] inline constexpr ::fast_io::string_view launch_word(console_launch_action action) noexcept
    {
        switch(action)
        { case console_launch_action::start: return "start";
          case console_launch_action::start_wasm: return "startw";
          case console_launch_action::start_instruction: return "starti";
          default: return "run"; }
    }
    [[nodiscard]] inline constexpr ::fast_io::string_view trim(::fast_io::string_view text) noexcept
    {
        // Every subview follows the nonempty extent check; these are borrowed
        // HOST input bytes and never a Wasm/native memory address or permission.
        while(!text.empty() && (text.front() == ' ' || text.front() == '\t')) { text = text.subview(1u); }
        while(!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r'))
        { text = text.subview(0u, text.size() - 1u); }
        return text;
    }
    [[nodiscard]] inline resume_policy classify_resume(::fast_io::string_view input)
    {
        auto text{trim(input)};
        // Do not classify multiline/control input as a scheduling shortcut,
        // even when alias lookup normalizes whitespace. Horizontal tabs remain.
        for(unsigned char c : text) { if(c < 0x20u && c != '\t') { return {}; } }
        bool const background{!text.empty() && text.back() == '&'};
        if(background) { text = trim(text.subview(0u, text.size() - 1u)); }
        auto const normalized{::uwvm2::uwvm::debugger::console_aliases::normalize(text)};
        if(!normalized.valid) { return {}; }
        auto const word{normalized.view()};
        if(word == "continue") { return {true, background, false, console_launch_action::none}; }
        if(word == "run") { return {true, background, true, console_launch_action::run}; }
        // An entry stop is synchronous. No '&' or argument tail can silently
        // convert start/startw/starti into an ordinary run or a command script.
        if(!background && word == "start") { return {true, false, true, console_launch_action::start}; }
        if(!background && word == "startw") { return {true, false, true, console_launch_action::start_wasm}; }
        if(!background && word == "starti") { return {true, false, true, console_launch_action::start_instruction}; }
        return {};
    }
}
