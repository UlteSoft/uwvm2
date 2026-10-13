// Finite expression parser and owned numeric DATA only; no VM stop authority.
#include <fast_io.h>
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
#include <uwvm2/uwvm/debugger/command.h>

using namespace uwvm2::uwvm::debugger;
namespace scalar = source_scalar_expression;
static ::std::size_t checks{};
static void check(bool ok, ::std::string_view reason)
{
    ++checks;
    if(!ok) { ::fast_io::io::perrln("character literal FAIL: ",reason); ::fast_io::fast_terminate(); }
}
static auto resolver(::std::size_t& reads)
{
    return [&reads](source_dwarf::source_expression const& leaf,bool size,scalar::integer& out)
    {
        ++reads;
        if(size || !leaf.steps.empty() || leaf.root_name != "value") { return false; }
        out = {7u,32u,false}; return true;
    };
}
static void accept(::std::string_view text, ::std::uint64_t expected, ::std::size_t expected_reads = 0u)
{
    for(auto bits : {32u,64u})
    {
        scalar::program code{}; scalar::integer value{}; ::std::size_t reads{};
        check(scalar::parse(text,code) == scalar::error::none,"supported literal syntax");
        check(scalar::evaluate(code,resolver(reads),value,bits) == scalar::error::none,"supported literal evaluation");
        check(value.bits == expected && value.width == 32u && !value.unsigned_value && !value.floating,"literal value and guest-independent scalar type");
        check(reads == expected_reads,"only reachable producer operands are resolved");
    }
    auto const line{::fast_io::concat_fast_io("print-frame 1 41 2 ",text)};
    auto const command{parse_console_command(::fast_io::string_view{line.data(),line.size()})};
    check(command.kind == console_command_kind::source_value && command.source_frame_explicit &&
          command.requested_step_thread == 1u && command.disassembly_stop_identifier == 41u &&
          command.source_frame_ordinal == 2u && command.source_variable_name_size == text.size() &&
          ::std::string_view{command.source_variable_name.data(),command.source_variable_name_size} == text,
          "console preserves the complete character literal");
}
static void reject(::std::string_view text, ::std::string_view reason)
{
    scalar::program code{}; scalar::integer value{123u,64u,true}; ::std::size_t reads{};
    check(scalar::parse("7",code) == scalar::error::none && !code.nodes.empty(),"reused program starts populated");
    check(scalar::parse(text,code) != scalar::error::none,reason);
    check(code.nodes.empty(),"rejected syntax clears the previous program");
    check(scalar::evaluate(code,resolver(reads),value) != scalar::error::none &&
          value.bits == 0u && reads == 0u,"rejected syntax yields no value or resolver access");
}
int main(int argc,char const* const* argv)
{
    auto const argument{argc > 1 ? ::fast_io::string_view{::fast_io::mnp::os_c_str(argv[1])} : ::fast_io::string_view{}};
    if(argument == "--probe")
    {
        for(int i{2};i<argc;++i)
        {
            auto const text{::fast_io::string_view{::fast_io::mnp::os_c_str(argv[i])}};
            scalar::program code{}; scalar::integer value{}; ::std::size_t reads{};
            auto const parsed{scalar::parse({text.data(),text.size()},code)};
            auto const evaluated{scalar::evaluate(code,resolver(reads),value)};
            ::fast_io::io::println(i-2,"\t",static_cast<unsigned>(parsed),"\t",static_cast<unsigned>(evaluated),
                                  "\t",value.bits,"\t",value.width,"\t",value.unsigned_value ? 1u : 0u,
                                  "\t",value.floating ? 1u : 0u,"\t",reads);
        }
        return 0;
    }
    bool const all{argument.empty()};
    check(all || argument == "space" || argument == "quote" || argument == "line","known regression selector");
    if(all || argument == "space") { reject("'a '","closing quote must not skip literal whitespace"); }
    if(all || argument == "quote") { reject("'''","unescaped apostrophe is not a character"); }
    if(all || argument == "line") { reject("'\n'","raw line break is not a character"); }
    if(!all) { return 0; }

    auto const exercise{[](::std::string_view literal, ::std::uint64_t value)
    {
        accept(literal,value);
        // Spaces outside a literal remain ordinary expression whitespace.
        accept(::fast_io::concat_std("(",literal," + 1)"),value+1u);
        accept(::fast_io::concat_std("value + ",literal),value+7u,1u);
        accept(::fast_io::concat_std("0 && ",literal),0u);
        accept(::fast_io::concat_std("1 || ",literal),1u);
        accept(::fast_io::concat_std("1 ? ",literal," : (1 / 0)"),value);
        for(auto white : {::std::string_view{" "},::std::string_view{"  "},::std::string_view{"\t"},
                          ::std::string_view{"\n"},::std::string_view{"\r"},::std::string_view{"\v"},::std::string_view{"\f"}})
        {
            auto const text{::fast_io::concat_std(literal.substr(0u,literal.size()-1u),white,"'")};
            reject(text,"additional characters are never silently stripped");
            reject(::fast_io::concat_std("0 && ",text),"short circuit cannot hide unsupported character syntax");
        }
    }};
    for(unsigned value{32u};value<127u;++value)
    {
        if(value == '\'' || value == '\\') { continue; }
        char const character{static_cast<char>(value)};
        exercise(::fast_io::concat_std("'",::std::string_view{&character,1u},"'"),value);
    }
    struct escape { ::std::string_view text; unsigned value; };
    for(auto const& item : {escape{"'\\n'",10u},escape{"'\\r'",13u},escape{"'\\t'",9u},
                           escape{"'\\0'",0u},escape{"'\\\\'",92u},escape{"'\\''",39u}})
    { exercise(item.text,item.value); }
    for(unsigned value{128u};value<256u;++value)
    {
        char const character{static_cast<char>(value)};
        reject(::fast_io::concat_std("'",::std::string_view{&character,1u},"'"),"non-ASCII literals remain unsupported");
    }
    for(auto text : {::std::string_view{"'"},::std::string_view{"''"},::std::string_view{"'a"},
                     ::std::string_view{"'\\"},::std::string_view{"'\\'"},::std::string_view{"'ab'"},
                     ::std::string_view{"'a''b'"},::std::string_view{"'\\n\\t'"},::std::string_view{"'\r'"},
                     ::std::string_view{"'\\x80'"},::std::string_view{"'\\u0041'"},::std::string_view{"u8'a'"}})
    { reject(text,"truncated, multi-character and unsupported literal forms are refused"); }
    // Repeated parsing must not retain state after alternating good/bad tokens.
    for(unsigned i{};i<256u;++i) { accept("' '",32u); reject("'  '","two spaces are not one space"); }
    ::fast_io::io::println("debug_source_character_literal: PASS checks=",checks," owned DATA only");
}
