/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <array>
# include <cstddef>
# include <memory>
# include <string>
# include <string_view>
# include <utility>
# include <vector>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::source_pretty_registry
{
    inline constexpr ::std::size_t maximum_profiles{64u}, maximum_pattern_bytes{512u}, maximum_path_bytes{128u};
    inline constexpr ::std::size_t maximum_name_bytes{128u}, maximum_type_bytes{4096u}, maximum_command_bytes{4096u};
    enum class profile_kind { sequence, string, pointer, optional };
    struct profile
    {
        ::std::string name{}, type_pattern{}, data_path{}, size_path{}, capacity_path{}, engaged_path{}, value_path{};
        profile_kind kind{profile_kind::sequence};
        bool enabled{true};
    };
    struct registry
    {
        ::std::vector<profile> profiles{};
        bool builtins_enabled{true};
    };
    enum class action { invalid, install, erase, enable, disable, list };
    struct request { action operation{}; profile value{}; ::std::string name{}; };
    enum class error { none, invalid_request, invalid_registry, not_found, capacity, reserved_name, allocation_failure };
    [[nodiscard]] inline constexpr bool identifier_start(char c) noexcept
    { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
    [[nodiscard]] inline constexpr bool identifier_byte(char c) noexcept
    { return identifier_start(c) || (c >= '0' && c <= '9'); }
    [[nodiscard]] inline constexpr bool valid_name(::std::string_view name) noexcept
    {
        if(name.empty() || name.size() > maximum_name_bytes || !identifier_start(name.front())) { return false; }
        for(char c : name) { if(!identifier_byte(c) && c != '-' && c != '.') { return false; } }
        return true;
    }
    // Paths identify already emitted fields only. They cannot contain an
    // expression, array selector, dereference, function call or host address.
    [[nodiscard]] inline constexpr bool valid_path(::std::string_view path) noexcept
    {
        if(path.empty() || path.size() > maximum_path_bytes) { return false; }
        bool start{true};
        for(char c : path)
        {
            if(c == '.') { if(start) { return false; } start = true; }
            else if(start) { if(!identifier_start(c)) { return false; } start = false; }
            else if(!identifier_byte(c)) { return false; }
        }
        return !start;
    }
    [[nodiscard]] inline constexpr bool valid_pattern(::std::string_view pattern) noexcept
    {
        if(pattern.empty() || pattern.size() > maximum_pattern_bytes) { return false; }
        for(unsigned char c : pattern) { if(c < 0x20u || c >= 0x7fu) { return false; } }
        return true;
    }
    [[nodiscard]] inline bool valid_profile(profile const& value) noexcept
    {
        if(!valid_name(value.name) || value.name == "builtin" || !valid_pattern(value.type_pattern)) { return false; }
        auto const unused{value.engaged_path.empty() && value.value_path.empty()};
        switch(value.kind)
        {
            case profile_kind::sequence: case profile_kind::string:
                return valid_path(value.data_path) && valid_path(value.size_path) &&
                    (value.capacity_path.empty() || valid_path(value.capacity_path)) && unused;
            case profile_kind::pointer:
                return valid_path(value.data_path) && value.size_path.empty() && value.capacity_path.empty() && unused;
            case profile_kind::optional:
                return value.data_path.empty() && value.size_path.empty() && value.capacity_path.empty() &&
                    valid_path(value.engaged_path) && valid_path(value.value_path);
        }
        return false;
    }
    [[nodiscard]] inline bool valid(registry const& values) noexcept
    {
        if(values.profiles.size() > maximum_profiles) { return false; }
        for(::std::size_t i{}; i != values.profiles.size(); ++i)
        {
            if(!valid_profile(values.profiles[i])) { return false; }
            for(::std::size_t j{}; j != i; ++j) { if(values.profiles[i].name == values.profiles[j].name) { return false; } }
        }
        return true;
    }
    // Full-string glob with only * and ?. The finite pattern/type budgets also
    // bound backtracking; matching supplies no DWARF schema or read authority.
    [[nodiscard]] inline constexpr bool matches(::std::string_view pattern, ::std::string_view type) noexcept
    {
        if(!valid_pattern(pattern) || type.size() > maximum_type_bytes) { return false; }
        if(pattern == type) { return true; }
        ::std::size_t p{}, t{}, star{::std::string_view::npos}, retry{};
        while(t < type.size())
        {
            if(p < pattern.size() && pattern[p] == '*') { star = p++; retry = t; }
            else if(p < pattern.size() && (pattern[p] == '?' || pattern[p] == type[t])) { ++p; ++t; }
            else if(star != ::std::string_view::npos) { p = star+1u; t = ++retry; }
            else { return false; }
        }
        while(p < pattern.size() && pattern[p] == '*') { ++p; }
        return p == pattern.size();
    }
    [[nodiscard]] inline profile const* find(registry const& values, ::std::string_view type) noexcept
    {
        if(!valid(values) || type.size() > maximum_type_bytes) { return nullptr; }
        for(auto const& value : values.profiles)
        { if(value.enabled && valid_profile(value) && value.type_pattern == type) { return ::std::addressof(value); } }
        for(auto const& value : values.profiles)
        { if(value.enabled && valid_profile(value) && matches(value.type_pattern,type)) { return ::std::addressof(value); } }
        return nullptr;
    }
    [[nodiscard]] inline constexpr ::std::string_view kind_text(profile_kind kind) noexcept
    {
        switch(kind)
        { case profile_kind::sequence: return "sequence"; case profile_kind::string: return "string";
          case profile_kind::pointer: return "pointer"; case profile_kind::optional: return "optional"; }
        return "invalid";
    }
    [[nodiscard]] inline constexpr ::std::string_view error_text(error value) noexcept
    {
        switch(value)
        {
            case error::none: return "none"; case error::invalid_request: return "invalid pretty-printer command or field path";
            case error::invalid_registry: return "invalid pretty-printer registry"; case error::not_found: return "unknown pretty-printer name";
            case error::capacity: return "pretty-printer registry limit is 64 profiles"; case error::reserved_name: return "builtin is reserved for enable/disable";
            case error::allocation_failure: return "pretty-printer registry allocation failed";
        }
        return "invalid pretty-printer result";
    }
    [[nodiscard]] inline request parse_host_command(::std::string_view line) noexcept
    {
        if(line.size() > maximum_command_bytes) { return {}; }
        for(unsigned char c : line) { if(c < 0x20u && c != '\t') { return {}; } if(c >= 0x7fu) { return {}; } }
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        try
        {
#endif
            ::std::array<::std::string,9u> words{}; ::std::size_t count{}, cursor{};
            while(cursor < line.size())
            {
                while(cursor < line.size() && (line[cursor] == ' ' || line[cursor] == '\t')) { ++cursor; }
                if(cursor == line.size()) { break; }
                if(count == words.size()) { return {}; }
                auto& word{words[count++]}; char quote{};
                if(line[cursor] == '\'' || line[cursor] == '"') { quote = line[cursor++]; }
                bool closed{quote == 0};
                while(cursor < line.size())
                {
                    char c{line[cursor++]};
                    if(quote != 0)
                    {
                        if(c == quote) { closed = true; break; }
                        if(c == '\\')
                        { if(cursor == line.size() || (line[cursor] != quote && line[cursor] != '\\')) { return {}; } c = line[cursor++]; }
                    }
                    else if(c == ' ' || c == '\t') { break; }
                    else if(c == '\'' || c == '"' || c == '\\') { return {}; }
                    if(word.size() == maximum_pattern_bytes) { return {}; }
                    word.push_back(c);
                }
                if(!closed || (quote != 0 && cursor < line.size() && line[cursor] != ' ' && line[cursor] != '\t')) { return {}; }
            }
            if(count < 2u || words[0u] != "pretty-printer") { return {}; }
            request result{};
            if(words[1u] == "list") { if(count != 2u) { return {}; } result.operation = action::list; return result; }
            if(words[1u] == "delete" || words[1u] == "enable" || words[1u] == "disable")
            {
                if(count != 3u || !valid_name(words[2u])) { return {}; }
                result.operation = words[1u] == "delete" ? action::erase : words[1u] == "enable" ? action::enable : action::disable;
                result.name = ::std::move(words[2u]); return result;
            }
            if(count < 5u) { return {}; }
            result.operation = action::install;
            auto& value{result.value}; value.name = ::std::move(words[2u]); value.type_pattern = ::std::move(words[3u]);
            if(words[1u] == "sequence" || words[1u] == "string")
            {
                if(count != 6u && count != 7u) { return {}; }
                value.kind = words[1u] == "sequence" ? profile_kind::sequence : profile_kind::string;
                value.data_path = ::std::move(words[4u]); value.size_path = ::std::move(words[5u]);
                if(count == 7u) { value.capacity_path = ::std::move(words[6u]); }
            }
            else if(words[1u] == "pointer")
            { if(count != 5u) { return {}; } value.kind = profile_kind::pointer; value.data_path = ::std::move(words[4u]); }
            else if(words[1u] == "optional")
            { if(count != 6u) { return {}; } value.kind = profile_kind::optional; value.engaged_path = ::std::move(words[4u]); value.value_path = ::std::move(words[5u]); }
            else { return {}; }
            return valid_profile(value) ? result : request{};
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        }
        catch(...) { return {}; }
#endif
    }
    // Publish only a complete validated immutable snapshot. A failed request
    // leaves both the caller's current snapshot and result reference unchanged.
    [[nodiscard]] inline error apply_request(registry const* current, request const& command,
        ::std::shared_ptr<registry const>& result) noexcept
    {
        if(command.operation != action::install && command.operation != action::erase && command.operation != action::enable &&
           command.operation != action::disable && command.operation != action::list) { return error::invalid_request; }
        if(current != nullptr && !valid(*current)) { return error::invalid_registry; }
        if(command.operation == action::list) { return error::none; }
        if(command.name == "builtin" && command.operation == action::erase) { return error::reserved_name; }
        if(command.operation == action::install && !valid_profile(command.value)) { return error::invalid_request; }
        if(command.operation != action::install && !valid_name(command.name)) { return error::invalid_request; }
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        try
        {
#endif
            auto pending{current == nullptr ? ::std::make_shared<registry>() : ::std::make_shared<registry>(*current)};
            if(command.operation != action::install && command.name == "builtin")
            { pending->builtins_enabled = command.operation == action::enable; result = ::std::move(pending); return error::none; }
            ::std::size_t index{};
            auto const& name{command.operation == action::install ? command.value.name : command.name};
            while(index != pending->profiles.size() && pending->profiles[index].name != name) { ++index; }
            if(command.operation == action::install)
            {
                if(index != pending->profiles.size()) { pending->profiles[index] = command.value; }
                else if(pending->profiles.size() == maximum_profiles) { return error::capacity; }
                else { pending->profiles.push_back(command.value); }
            }
            else
            {
                if(index == pending->profiles.size()) { return error::not_found; }
                if(command.operation == action::erase) { pending->profiles.erase(pending->profiles.begin()+index); }
                else if(command.operation == action::enable || command.operation == action::disable)
                { pending->profiles[index].enabled = command.operation == action::enable; }
                else { return error::invalid_request; }
            }
            result = ::std::move(pending); return error::none;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        }
        catch(...) { return error::allocation_failure; }
#endif
    }
    inline constexpr ::std::size_t maximum_description_bytes{61440u};
    [[nodiscard]] inline ::std::string acknowledge(request const& command)
    {
        auto const name{command.operation == action::install ? ::std::string_view{command.value.name} : ::std::string_view{command.name}};
        if((command.operation != action::install && command.operation != action::erase &&
            command.operation != action::enable && command.operation != action::disable) || !valid_name(name))
        { return "error: invalid pretty-printer acknowledgement\n"; }
        ::std::string text{"Pretty-printer "};text += name;
        text += command.operation == action::install ? " registered.\n" : command.operation == action::erase ? " deleted.\n" :
            command.operation == action::enable ? " enabled.\n" : " disabled.\n";
        return text;
    }
    [[nodiscard]] inline ::std::string describe(registry const* values)
    {
        if(values != nullptr && !valid(*values)) { return "error: invalid pretty-printer registry\n"; }
        ::std::string text{"builtin "}; text += values == nullptr || values->builtins_enabled ? "enabled\n" : "disabled\n";
        auto const total{values == nullptr ? 0u : values->profiles.size()};::std::size_t shown{};
        if(total == 0u) { text += "No custom pretty-printers registered.\n"; }
        else for(auto const& value : values->profiles)
        {
            ::std::string row{};
            auto quoted{[&](::std::string_view field)
            { row += '"'; for(char c : field) { if(c == '\\' || c == '"') { row += '\\'; } row += c; } row += '"'; }};
            row += value.name; row += value.enabled ? " enabled " : " disabled "; row += kind_text(value.kind); row += " type="; quoted(value.type_pattern);
            if(!value.data_path.empty()) { row += " data="; row += value.data_path; }
            if(!value.size_path.empty()) { row += " length="; row += value.size_path; }
            if(!value.capacity_path.empty()) { row += " capacity="; row += value.capacity_path; }
            if(!value.engaged_path.empty()) { row += " engaged="; row += value.engaged_path; }
            if(!value.value_path.empty()) { row += " value="; row += value.value_path; }
            row += '\n';
            // Reserve the complete trailer and append complete escaped rows only.
            // This stays below every platform's existing 65536-byte reply limit.
            if(text.size() > maximum_description_bytes-128u || row.size() > maximum_description_bytes-128u-text.size()) { break; }
            text += row;++shown;
        }
        text += "pretty-printers shown=";text += ::std::to_string(shown);text += " total=";text += ::std::to_string(total);
        text += shown == total ? " truncated=false\n" : " truncated=true\n";
        return text;
    }
}
