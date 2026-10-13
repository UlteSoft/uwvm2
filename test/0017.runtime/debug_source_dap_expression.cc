// DAP/controller syntax bridge: owned DATA, no genuine runtime stop authority.
#include <fast_io.h>
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
#include <uwvm2/uwvm/debugger/command.h>
#include <uwvm2/uwvm/debugger/console_completion.h>
using namespace uwvm2::uwvm::debugger;
namespace scalar = source_scalar_expression;
static void check(bool ok)
{ if(!ok) { ::fast_io::io::perrln("DAP source expression bridge FAIL"); ::fast_io::fast_terminate(); } }
int main(int argc,char const* const* argv)
{
    for(auto text : {"value++1", "value--1", "++value", "--value", "0 && value++1", "1 || --value"})
    {
        scalar::program code{};auto const view{::fast_io::string_view{::fast_io::mnp::os_c_str(text)}};
        check(scalar::parse({view.data(),view.size()},code)==scalar::error::unsupported && code.nodes.empty());
    }
    for(auto text : {"print-frame 1 41 2 par", "ptype-frame 1 41 2 par"})
    {
        auto const line{::fast_io::string_view{::fast_io::mnp::os_c_str(text)}};
        auto const query{console_completion::context(line,line.size())};
        check(query.type==console_completion::kind::symbol && query.explicit_frame && query.prefix=="par" && query.selectors=="1 41 2 ");
    }
    for(auto text : {"print-frame par", "print-frame 1 41 par", "ptype-frame 1 par"})
    {
        auto const line{::fast_io::string_view{::fast_io::mnp::os_c_str(text)}};
        check(console_completion::context(line,line.size()).type==console_completion::kind::none);
    }
    for(auto text : {"print-frame", "print-frame 1 41", "print-frame 1 41 2", "print-frame 0 41 2 value",
                    "print-frame 1 0 2 value", "print-frame 1 41 -1 value", "print-frame 1 41 18446744073709551616 value",
                    "print-frame 1 41 bad value", "ptype-frame 1 41 2"})
    { check(parse_console_command(::fast_io::string_view{::fast_io::mnp::os_c_str(text)}).kind==console_command_kind::invalid); }
    for(int i{1};i<argc;++i)
    {
        auto const text{::fast_io::string_view{::fast_io::mnp::os_c_str(argv[i])}};
        check(!text.empty() && text.size()<=256u);
        auto const command_text{::fast_io::concat_fast_io("print-frame 1 41 2 ",text)};
        auto const command{parse_console_command(::fast_io::string_view{command_text.data(),command_text.size()})};
        check(command.kind==console_command_kind::source_value && command.source_frame_explicit &&
              command.requested_step_thread==1u && command.disassembly_stop_identifier==41u &&
              command.source_frame_ordinal==2u && command.source_variable_name_size==text.size() &&
              ::std::string_view{command.source_variable_name.data(),command.source_variable_name_size}==
              ::std::string_view{text.data(),text.size()});
        scalar::program code{};scalar::integer value{};::std::size_t reads{};
        check(scalar::parse({text.data(),text.size()},code)==scalar::error::none);
        auto const resolve{[&](source_dwarf::source_expression const& leaf,bool size,scalar::integer& out)
        {
            ++reads;
            if(leaf.steps.empty() && (leaf.root_name=="value" || leaf.root_name=="shadow"))
            { out={size ? 4u : 7u,32u,size};return true; }
            if(leaf.root_name=="packet" && size) { out={28u,32u,true};return true; }
            if(!size && leaf.steps.empty() && (leaf.root_name=="flag" || leaf.root_name=="off"))
            { out=scalar::from_dwarf_numeric(source_dwarf::numeric_kind::boolean,leaf.root_name=="flag" ? 2u : 0u,
                leaf.root_name=="flag" ? 32u : 8u);return true; }
            if(!size && leaf.root_name=="real32")
            { out={::std::bit_cast<::std::uint32_t>(3.5f),32u,false,true};return true; }
            if(!size && leaf.root_name=="real64")
            { out={::std::bit_cast<::std::uint64_t>(-2.5),64u,false,true};return true; }
            if(!size && leaf.root_name=="pair" && leaf.steps.size()==1u)
            {
                auto const& step{leaf.steps[0u]};
                if(step.kind!=source_dwarf::source_expression_step_kind::member) { return false; }
                if(step.member=="0") { out={0xfffffff7u,32u,false};return true; }
                if(step.member=="1") { out={42u,32u,true};return true; }
            }
            if(!size && leaf.root_name=="p" && leaf.steps.size()==1u &&
               leaf.steps[0u].kind==source_dwarf::source_expression_step_kind::dereference)
            { out={42u,32u,false};return true; }
            // Pointer-typed arithmetic and missing values cannot become guest
            // read authority. No memory or host address is accessed here.
            return false;
        }};
        auto const resolve_type{[](source_dwarf::source_expression const& leaf,bool size,scalar::integer& out)
        {
            // Independent metadata DATA, never invoking the value resolver.
            if(size)
            {
                if(leaf.root_name=="packet" && leaf.steps.empty()) { out={28u,32u,true};return true; }
                if(leaf.root_name=="p" && (leaf.steps.empty() || (leaf.steps.size()==1u &&
                    leaf.steps[0u].kind==source_dwarf::source_expression_step_kind::dereference))) { out={4u,32u,true};return true; }
                if(leaf.steps.empty() && (leaf.root_name=="value" || leaf.root_name=="shadow" || leaf.root_name=="real32" || leaf.root_name=="real64"))
                { out={leaf.root_name=="real64" ? 8u : 4u,32u,true};return true; }
                return false;
            }
            if(leaf.steps.empty() && (leaf.root_name=="value" || leaf.root_name=="shadow")) { out={0u,32u,false};return true; }
            if(leaf.steps.empty() && (leaf.root_name=="flag" || leaf.root_name=="off"))
            { out=scalar::from_dwarf_numeric(source_dwarf::numeric_kind::boolean,0u,leaf.root_name=="flag" ? 32u : 8u);return true; }
            if(leaf.steps.empty() && leaf.root_name=="real32") { out={0u,32u,false,true};return true; }
            if(leaf.steps.empty() && leaf.root_name=="real64") { out={0u,64u,false,true};return true; }
            if(leaf.root_name=="pair" && leaf.steps.size()==1u && leaf.steps[0u].kind==source_dwarf::source_expression_step_kind::member)
            {
                if(leaf.steps[0u].member=="0") { out={0u,32u,false};return true; }
                if(leaf.steps[0u].member=="1") { out={0u,32u,true};return true; }
            }
            if(leaf.root_name=="p" && leaf.steps.size()==1u && leaf.steps[0u].kind==source_dwarf::source_expression_step_kind::dereference)
            { out={0u,32u,false};return true; }
            return false;
        }};
        auto const status{scalar::evaluate(code,resolve,value,32u,resolve_type)};
        ::fast_io::io::println(i-1,"\t",static_cast<unsigned>(status),"\t",value.bits,"\t",
                              value.width,"\t",value.unsigned_value ? 1u : 0u,"\t",value.floating ? 1u : 0u,"\t",reads);
    }
}
