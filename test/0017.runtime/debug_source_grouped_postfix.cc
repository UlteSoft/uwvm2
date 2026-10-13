// Grouped reference postfix composition DATA; no live guest or pointer authority.
#include <fast_io.h>
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
namespace scalar=uwvm2::uwvm::debugger::source_scalar_expression;
namespace dwarf=uwvm2::uwvm::debugger::source_dwarf;
static ::std::size_t checks{};
static void check(bool ok,::std::string_view why)
{ ++checks;if(!ok){::fast_io::io::perrln("Grouped selector FAIL: ",why);::fast_io::fast_terminate();} }
#ifdef UWVM_GROUPED_SELECTOR_BASELINE
int main(int argc,char const* const* argv)
{
    check(argc==2,"baseline input");
    auto const text{::fast_io::string_view{::fast_io::mnp::os_c_str(argv[1])}};
    scalar::program code{};
    check(scalar::parse({text.data(),text.size()},code)==scalar::error::none,"new grouped postfix unavailable");
}
#else
static ::std::size_t reads{},queries{};
static ::std::uint64_t signature{};
static ::std::uint64_t path_signature(dwarf::source_expression const& ref)
{
    ::std::uint64_t value{2166136261u};
    auto const add{[&](::std::uint64_t byte){value=((value^byte)*16777619u)&0xffffffffu;}};
    for(unsigned char c:ref.root_name){add(c);}
    add(255u);
    for(auto const& step:ref.steps)
    {
        add(static_cast<unsigned>(step.kind));
        if(step.kind==dwarf::source_expression_step_kind::member)
        {for(unsigned char c:step.member){add(c);}add(254u);}
        else if(step.kind==dwarf::source_expression_step_kind::index)
        {auto bits{static_cast<::std::uint64_t>(step.index)};for(unsigned i{};i!=8u;++i){add(bits&255u);bits>>=8u;}}
    }
    return value;
}
static bool known(dwarf::source_expression const& ref)
{ return ref.root_name=="p" || ref.root_name=="packet" || ref.root_name=="array" || ref.root_name=="pair" || ref.root_name=="ns::packet"; }
static bool values(dwarf::source_expression const& ref,bool size,scalar::integer& out)
{
    ++reads;signature=path_signature(ref);
    if(!known(ref)){return false;}
    out={size ? 4u : 42u,32u,size,false,scalar::value_category::integer};return true;
}
static bool types(dwarf::source_expression const& ref,bool size,scalar::integer& out)
{
    ++queries;if(!known(ref)){return false;}
    out={size ? 4u : 0u,32u,size,false,scalar::value_category::integer};return true;
}
struct result {scalar::error parsed{},evaluated{};scalar::integer value{};};
static result run(::std::string_view text,unsigned guest)
{
    reads=queries=signature=0u;scalar::program code{};result out{};
    check(scalar::parse("1",code)==scalar::error::none,"seed old output");
    out.parsed=scalar::parse(text,code);
    if(out.parsed==scalar::error::none){out.evaluated=scalar::evaluate(code,values,out.value,guest,types);}
    else{check(code.nodes.empty(),"rejected syntax clears output");out.evaluated=scalar::error::unsupported;}
    return out;
}
int main(int argc,char const* const* argv)
{
    if(argc>1)
    {
        for(int i{1};i<argc;++i)
        {
            auto const text{::fast_io::string_view{::fast_io::mnp::os_c_str(argv[i])}};
            check(text.size()<=256u,"bounded bridge input");
            for(unsigned guest:{32u,64u})
            {
                auto const out{run({text.data(),text.size()},guest)};
                ::fast_io::io::println(i-1,"\t",guest,"\t",out.parsed==scalar::error::none ? 1u : 0u,
                    "\t",static_cast<unsigned>(out.evaluated),"\t",out.value.bits,"\t",out.value.width,
                    "\t",out.value.unsigned_value ? 1u : 0u,"\t",reads,"\t",queries,"\t",signature);
            }
        }
        return 0;
    }
    for(unsigned guest:{32u,64u})
    {
        for(auto text:{"(packet).field + 1","(*p).field + 1","((pair)).0 + 1","((p)).* + 1",
                      "((array)[1]).field + 1","((*p).array)[-2] + 1","(ns::packet)->field + 1"})
        {
            auto const view{::fast_io::string_view{::fast_io::mnp::os_c_str(text)}};
            auto const out{run({view.data(),view.size()},guest)};
            check(out.parsed==scalar::error::none && out.evaluated==scalar::error::none,"valid grouped postfix");
            check(out.value.bits==43u && out.value.width==32u && !out.value.unsigned_value,"numeric result");
            check(reads==1u && queries==0u,"one complete resolver plan");
        }
        for(auto text:{"(packet) - 1","(packet) + 1","(packet) * 2","(*p) + 1"})
        {
            auto const view{::fast_io::string_view{::fast_io::mnp::os_c_str(text)}};
            auto const out{run({view.data(),view.size()},guest)};
            check(out.parsed==scalar::error::none && out.evaluated==scalar::error::none,"group arithmetic unchanged");
            check(reads==1u,"one grouped value read");
        }
        for(auto text:{"(p+1)->field + 1","(42).field + 1","(int(p)).field + 1","(true).field + 1",
                      "(sizeof(p)).field + 1","(p as i32).field + 1","(1 ? p : p).field + 1","*(p+1)",
                      "(packet)[1+1] + 1","(packet). * + 1","(packet)-> + 1","(packet)[9223372036854775808] + 1"})
        {
            auto const view{::fast_io::string_view{::fast_io::mnp::os_c_str(text)}};
            auto const out{run({view.data(),view.size()},guest)};
            check(out.parsed!=scalar::error::none && reads==0u && queries==0u,"computed path or malformed suffix rejected");
        }
        auto const skipped{run("0 && ((*p).field + missing)",guest)};
        check(skipped.parsed==scalar::error::none && skipped.evaluated==scalar::error::none && reads==0u && queries==0u,"short circuit grants no read");
    }
    ::fast_io::io::println("debug_source_grouped_postfix: PASS checks=",checks," owned DATA only");
}
#endif
