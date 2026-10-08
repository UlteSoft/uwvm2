#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
#include <fast_io.h>
namespace se=uwvm2::uwvm::debugger::source_scalar_expression;
static void require(bool yes)
{ if(!yes){::fast_io::io::perrln("condition truth semantics failed");::fast_io::fast_terminate();} }
int main()
{
    unsigned checks{};
    for(auto lang:{se::language_semantics::c,se::language_semantics::c23,se::language_semantics::cpp,
                   se::language_semantics::rust,se::language_semantics::go,se::language_semantics::zig})
    {
        auto run=[&](char const* expression,int expected)
        {
            auto owned{::fast_io::concat_fast_io("(",::fast_io::mnp::os_c_str(expression),") != false")};
            se::program program{};se::integer value{};
            auto status=se::parse({owned.data(),owned.size()},program,lang);
            if(status==se::error::none)
            { status=se::evaluate(program,[](auto const&,bool,se::integer&){return false;},value,32u,
                                 [](auto const&,bool,se::integer&){return false;},lang); }
            if(expected<0){require(status!=se::error::none);}
            else
            {
                require(status==se::error::none && !value.floating && value.bits==static_cast<unsigned>(expected));
                require(se::copied_type_name(value)==(lang==se::language_semantics::c || lang==se::language_semantics::c23 ?
                        "int":"bool"));
            }
            ++checks;
        };
        run("false",0);run("true",1);run("1 == 0",0);run("1 == 1",1);
        run(lang==se::language_semantics::zig?"false and (1 / 0 == 0)":"false && (1 / 0 == 0)",0);
        run(lang==se::language_semantics::zig?"true and (1 / 0 == 0)":"true && (1 / 0 == 0)",-1);
        run("missing_condition_variable",-1);run(lang==se::language_semantics::zig?"false and missing_condition_variable":"false && missing_condition_variable",-1);
        bool const native{lang==se::language_semantics::c || lang==se::language_semantics::c23 ||
                          lang==se::language_semantics::cpp};
        run("23",native?1:-1);run("-31",native?1:-1);run("-0.0",native?0:-1);
        run("1.25",native?1:-1);run("0.0",native?0:-1);
        run("1.25 > 0.0",1);run("0.0 == -0.0",1);run("1 / 0",-1);
    }
    ::fast_io::io::println("PASS condition logical truth semantics: ",checks,
                          " finite DATA checks; manager live stops tested separately");
}
