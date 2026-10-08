// Bounded grammar for the three debugger step levels. Source into is dispatched
// only after host authorization; over/out remain fail-closed. Assembly is
// dispatched through an independent authenticated native-step path.
#include <uwvm2/uwvm/debugger/command.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <string>
#include <string_view>

#define CHECK(condition) do { if(!(condition)) { std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #condition); std::abort(); } } while(false)
namespace debugger = ::uwvm2::uwvm::debugger;

[[nodiscard]] static debugger::console_command parse(::std::string_view line)
{ return debugger::parse_console_command(debugger::command_text_view{line.data(), line.size()}); }

[[nodiscard]] static ::std::string_view message(debugger::console_command const& command)
{
    auto const text{debugger::unsupported_step_message(command)};
    return {text.data(), text.size()};
}

int main()
{
    using debugger::console_command_kind;
    using debugger::source_step_policy;
    using debugger::unavailable_step_level;
    auto const legacy{parse("step 42")};
    auto const short_alias{parse("s 42")};
    auto const wasm{parse("step wasm 42")};
    CHECK(legacy.kind == console_command_kind::protocol);
    CHECK(wasm.kind == console_command_kind::protocol);
    CHECK(short_alias.kind == console_command_kind::protocol);
    CHECK(legacy.operation == ::uwvm2::utils::control::operation::step);
    CHECK(wasm.operation == legacy.operation && short_alias.operation == legacy.operation);
    CHECK(wasm.payload_size == 8u && legacy.payload_size == 8u && short_alias.payload_size == 8u);
    for(::std::size_t index{}; index != 8u; ++index)
    {
        CHECK(wasm.payload[index] == legacy.payload[index]);
        CHECK(short_alias.payload[index] == legacy.payload[index]);
    }
    auto const source_default{parse("step source 42")};
    CHECK(source_default.kind == console_command_kind::source_step);
    CHECK(source_default.source_policy == source_step_policy::into && source_default.requested_step_thread == 42u);
    CHECK(source_default.operation == ::uwvm2::utils::control::operation::step);
    CHECK(source_default.payload_size == 8u);
    for(auto const policy : {"into", "over", "out"})
    {
        auto const command{parse(::std::string{"step source 42 "} + policy)};
        CHECK(command.kind == console_command_kind::source_step);
        CHECK(command.source_policy == (::std::string_view{policy} == "over" ? source_step_policy::over :
                                        ::std::string_view{policy} == "out" ? source_step_policy::out : source_step_policy::into));
        CHECK(command.payload_size == 8u);
    }
    auto const assembly{parse("step asm 42")};
    CHECK(assembly.kind == console_command_kind::assembly_step);
    CHECK(assembly.operation == ::uwvm2::utils::control::operation::step);
    CHECK(assembly.requested_step_thread == 42u && assembly.payload_size == 8u);
    for(::std::size_t index{}; index != 8u; ++index) { CHECK(assembly.payload[index] == wasm.payload[index]); }
    auto const source_break{parse("break-source 7 C:\\work dir\\main.cpp:42")};
    CHECK(source_break.kind == console_command_kind::source_breakpoint);
    CHECK(source_break.operation == ::uwvm2::utils::control::operation::breakpoint_set);
    CHECK(source_break.source_module == 7u && source_break.source_line == 42u);
    CHECK((::std::string_view{source_break.source_path.data(), source_break.source_path_size} == "C:\\work dir\\main.cpp"));
    auto const utf8_break{parse("break-source 0 src/变量.rs:9")};
    CHECK(utf8_break.kind == console_command_kind::source_breakpoint);
    CHECK((::std::string_view{utf8_break.source_path.data(), utf8_break.source_path_size} == "src/变量.rs"));
    for(auto const invalid_source : {"break-source 0 main.c:0", "break-source x main.c:1",
                                     "break-source 0 main.c:-1", "break-source 0 :9",
                                     "break-source 0 main.c:18446744073709551616"})
    { CHECK(parse(invalid_source).kind == console_command_kind::invalid); }
    CHECK(message(parse("server")) == "error: command is unsupported\n");
    for(auto const invalid : {"step wasm", "step wasm 0", "step wasm -1", "step wasm 18446744073709551616",
                              "step wasm 42 over", "step source", "step source 0", "step source 42 next",
                              "step source 42 over extra", "step asm", "step asm 0", "step asm 42 into",
                              "step unknown 42", "step source 42;continue"})
    { CHECK(parse(invalid).kind == console_command_kind::invalid); }
    CHECK(parse(::std::string(513u, 'x')).kind == console_command_kind::invalid);
    std::puts("PASS bounded source-into and assembly dispatch; over/out fail-closed grammar");
}
