#pragma once

#include <string>
#include <string_view>

namespace uwvm2test::native_unwind
{
    // Match whole compiler-log fields: "unwind" must not accidentally match "unwind-uncheck".
    [[nodiscard]] inline bool has_field(::std::string_view log, ::std::string_view name, ::std::string_view value)
    {
        auto const needle{::std::string{name} + "=" + ::std::string{value}};
        ::std::size_t pos{};
        while((pos = log.find(needle, pos)) != ::std::string_view::npos)
        {
            auto const end{pos + needle.size()};
            if(end == log.size() || log[end] == ' ' || log[end] == '\n' || log[end] == '\r' || log[end] == ',' || log[end] == ';') { return true; }
            pos = end;
        }
        return false;
    }

    [[nodiscard]] inline bool has_native_replacement(::std::string_view log)
    {
        return (has_field(log, "unwind_backend", "unwind.h") || has_field(log, "unwind_backend", "win64-seh")) &&
               has_field(log, "unwind_replace_frames", "yes") &&
               has_field(log, "call_stack_frames", "omit") && !has_field(log, "call_stack_frames", "emit");
    }

    [[nodiscard]] inline bool checked_native_policy(::std::string_view log)
    {
        return has_native_replacement(log) && has_field(log, "unwind_check", "live");
    }

    [[nodiscard]] inline bool matches_policy(::std::string_view log, ::std::string_view requested)
    {
        if(requested == "instruction")
        {
            return has_field(log, "call_stack", "instruction") &&
                   has_field(log, "call_stack_frames", "emit") && !has_field(log, "call_stack_frames", "omit");
        }
        if(requested == "unwind")
        {
            return has_field(log, "call_stack", "unwind") && checked_native_policy(log);
        }
        if(requested == "unwind-uncheck" || requested == "unwind-unchecked")
        {
            return has_field(log, "call_stack", "unwind-uncheck") &&
                   has_native_replacement(log) &&
                   (has_field(log, "unwind_check", "off") || has_field(log, "unwind_check", "unchecked"));
        }
        if(requested == "auto")
        {
            // A failed native self-check may select instruction only before compilation. Either effective policy must
            // agree with actual frame emission; native output supplemented by hidden logical frames is not a pass.
            return matches_policy(log, "instruction") || matches_policy(log, "unwind");
        }
        if(requested == "none")
        {
            return has_field(log, "call_stack", "none") &&
                   has_field(log, "call_stack_frames", "omit") && !has_field(log, "call_stack_frames", "emit");
        }
        return false;
    }
}
