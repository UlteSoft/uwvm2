// Copied sizeof extent DATA: no stopped frame or guest memory authority.
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
#include <fast_io.h>
namespace s = uwvm2::uwvm::debugger::source_scalar_expression;
namespace d = uwvm2::uwvm::debugger::source_dwarf;
using L = s::language_semantics;
static ::std::size_t checks{}, reads{}, extents{}, types{};
static void check(bool good, ::std::string_view message)
{
    ++checks;
    if(!good) { ::fast_io::io::perrln("size extent FAIL: ",message); ::fast_io::fast_terminate(); }
}
static s::integer extent(unsigned guest, unsigned fault)
{
    s::integer out{28u,guest,true};
    switch(fault)
    {
        case 1u: out.category = s::value_category::boolean; break;
        case 2u: out.width = guest == 32u ? 64u : 32u; break;
        case 3u: out.unsigned_value = false; break;
        case 4u: out.floating = true; break;
        case 5u: out.width = 16u; break;
        case 6u: out.category = static_cast<s::value_category>(99u); break;
        case 7u: out.wide_identity = s::wide_builtin::signed_long; break;
        case 8u: out.builtin_identity = s::narrow_builtin::unsigned_short; break;
        case 9u:
            out.category = s::value_category::integer;
            out.wide_identity = s::wide_builtin::unsigned_long;
            out.declaration_identity_known = true; // missing declaration offset
            break;
        case 10u:
            out.category = s::value_category::integer;
            out.wide_identity = s::wide_builtin::unsigned_long;
            out.declaration_identity = {7u,99u}; out.declaration_identity_known = true;
            break;
        case 12u: out.bits = ::std::uint64_t{1u} << 32u; break;
        case 13u: out.bits = guest == 32u ? 0xffffffffu : 0xffffffffffffffffu; break;
        case 14u: out.bits = 0u; break;
        default: break;
    }
    return out;
}
struct resolver
{
    unsigned guest{}, fault{}; bool type_only{};
    bool operator()(d::source_expression const& ref, bool size, s::integer& out) const
    {
        if(type_only) { ++types; }
        else if(size) { ++extents; }
        else { ++reads; }
        if(ref.root_name != "value" || !ref.steps.empty() || !size) { return false; }
        out = extent(guest,fault);
        return fault != 11u; // a failed callback cannot mint a result
    }
};
static s::error probe(::std::string_view text, unsigned guest, L language, unsigned type_fault,
    unsigned value_fault, s::integer& out)
{
    reads = extents = types = 0u;
    s::program code{}; auto status{s::parse(text,code)};
    return status == s::error::none ? s::evaluate(code,resolver{guest,value_fault,false},out,
        guest,resolver{guest,type_fault,true},language) : status;
}
static constexpr ::std::string_view contexts[]{
    "sizeof(sizeof(value))", "sizeof(sizeof(value)+1)",
    "0 && sizeof(value)", "1 || sizeof(value)",
    "1 ? sizeof(int) : sizeof(value)", "0 ? sizeof(value) : sizeof(int)"
};
static bool empty(s::integer const& out)
{
    return out.bits == 0u && !out.declaration_identity_known &&
        out.wide_identity == s::wide_builtin::unknown && out.builtin_identity == s::narrow_builtin::unknown;
}
int main(int argc, char** argv)
{
    if(argc == 2 && ::std::string_view{argv[1]} == "--witness")
    {
        for(unsigned fault : {1u,12u}) for(unsigned guest : {32u,64u})
            for(auto language : {L::c,L::cpp,L::c23})
            {
                if(fault == 12u && guest == 64u) { continue; }
                auto emit{[&](::std::string_view text)
                {
                    s::integer out{}; auto status{probe(text,guest,language,fault,fault,out)};
                    ::fast_io::io::println(guest,"\t",static_cast<unsigned>(language),"\t",text,"\t",
                        static_cast<unsigned>(status),"\t",out.bits,"\t",reads,"\t",extents,"\t",types);
                }};
                for(auto text : contexts) { emit(text); }
                if(fault == 12u) { emit("sizeof(value)"); }
            }
        return 0;
    }
    for(unsigned repeat{}; repeat != 128u; ++repeat)
        for(unsigned guest : {32u,64u}) for(auto language : {L::c,L::cpp,L::c23})
        {
            for(unsigned fault{1u}; fault <= 12u; ++fault)
            {
                if(fault == 10u || (fault == 12u && guest == 64u)) { continue; } // valid canonical extent
                for(auto text : contexts)
                {
                    s::integer out{999u,64u,true};
                    check(probe(text,guest,language,fault,0u,out) == s::error::unavailable,text);
                    check(empty(out),"failed inference publishes no value or operand DIE");
                    check(reads == 0u && extents == 0u && types == 1u,"type-only rejection; no operand or extent read");
                }
                s::integer out{};
                check(probe("sizeof(value)",guest,language,0u,fault,out) == s::error::unavailable && empty(out),
                    "direct extent uses the same malformed DATA contract");
                check(reads == 0u && extents == 1u && types == 0u,"direct sizeof calls only the extent resolver");
            }
            for(unsigned good : {0u,10u,13u,14u})
            {
                for(auto text : contexts)
                {
                    s::integer out{};
                    check(probe(text,guest,language,good,11u,out) == s::error::none,text);
                    auto const expected{text == contexts[0] || text == contexts[1] ? guest/8u :
                        text == contexts[2] ? 0u : text == contexts[3] ? 1u : 4u};
                    check(out.bits == expected && !out.declaration_identity_known,"valid inference discards operand identity and value bits");
                    check(reads == 0u && extents == 0u && types == 1u,"unevaluated or dead extent is never copied");
                }
                s::integer out{};
                check(probe("sizeof(value)",guest,language,11u,good,out) == s::error::none && out.bits == extent(guest,good).bits &&
                    out.width == guest && out.wide_identity == s::wide_builtin::unsigned_long && !out.declaration_identity_known,
                    "direct canonical extent is rebuilt as size_t without an operand DIE");
            }
            // Historical two-argument metadata callbacks describe the scalar
            // operand, not a guest-width extent. Signed/narrow/bool/f32 remain valid.
            for(auto operand : {s::from_dwarf_numeric(d::numeric_kind::signed_integer,0u,8u),
                    s::from_dwarf_numeric(d::numeric_kind::boolean,0u,8u),
                    s::from_dwarf_numeric(d::numeric_kind::f32_bits,0u,32u)})
            {
                s::program code{}; s::integer out{}; reads = extents = types = 0u;
                check(s::parse("sizeof(sizeof(value))",code) == s::error::none,"legacy type callback syntax");
                auto legacy{[&](d::source_expression const&, s::integer& v) { ++types; v = operand; return true; }};
                check(s::evaluate(code,resolver{guest,11u,false},out,guest,legacy,language) == s::error::none && out.bits == guest/8u,
                    "operand type is not confused with an extent");
                check(types == 1u && reads == 0u && extents == 0u,"legacy callback remains metadata only");
            }
        }
    for(unsigned guest : {32u,64u}) for(auto text : contexts)
    {
        s::integer out{};
        check(probe(text,guest,L::shared_numeric,1u,11u,out) == s::error::none && out.wide_identity == s::wide_builtin::unknown,
            "shared/TinyGo/Go legacy type-only contract is preserved");
        check(reads == 0u && extents == 0u && types == (text == contexts[2] || text == contexts[3] ? 0u : 1u),
            "shared short circuit retains its legacy no-type-query behavior");
    }
    {
        s::integer out{};
        check(probe("sizeof(value)",32u,L::shared_numeric,0u,12u,out) == s::error::none && out.bits == 0u,
            "legacy shared direct extent masking remains unchanged");
        check(probe("sizeof(sizeof(value))",32u,L::shared_numeric,12u,11u,out) == s::error::none && out.bits == 4u && reads == 0u && extents == 0u,
            "legacy shared metadata extent contract remains unchanged");
    }
    ::fast_io::io::println("debug_source_size_extent: PASS checks=",checks," declaration/value extent parity; copied DATA only");
}
