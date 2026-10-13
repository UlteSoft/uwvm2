// Owned metadata and copied carriers only; actual producer stops are tested separately.
#include <uwvm2/uwvm/debugger/source_dwarf_values.h>
#include <fast_io.h>
using namespace uwvm2::uwvm::debugger::source_dwarf;
static void check(bool v,char const* text)
{ if(!v) { ::fast_io::io::perrln("qualified numeric values: ",::fast_io::mnp::os_c_str(text));::fast_io::fast_terminate(); } }
template<class T> static copied_numeric_local copied(unsigned type,T bits)
{
    copied_numeric_local r{};r.available=true;r.wasm_type=static_cast<::std::uint8_t>(type);
    ::std::memcpy(r.bytes.data(),::std::addressof(bits),sizeof bits);return r;
}
int main()
{
    ::std::vector<scope_record> scopes(2);
    scopes[0].kind=scope_kind::compile_unit;scopes[1].parent=0;
    scopes[1].kind=scope_kind::subprogram;scopes[1].concrete=true;scopes[1].ranges={{10,20}};
    type_record integer{};integer.name="int";integer.kind=type_kind::scalar;integer.language=0x0c;
    integer.encoding=5;integer.byte_count=4;integer.display_qualifiers=1;
    type_record boolean{integer};boolean.name="_Bool";boolean.encoding=2;boolean.byte_count=1;
    type_record single{integer};single.name="float";single.encoding=4;
    type_record wide{single};wide.name="double";wide.byte_count=8;
    type_record alias{integer};alias.name="Ticket";alias.named_type_alias=true;
    type_record barrier{integer};barrier.display_qualifiers=3;
    ::std::vector<type_record> types{integer,boolean,single,wide,alias,barrier};
    ::std::vector<copied_numeric_local> locals{copied(0x7f,::std::uint32_t{0xffffff9d}),
        copied(0x7f,::std::uint32_t{1}),copied(0x7d,::std::uint32_t{0x3fa00000}),
        copied(0x7c,::std::uint64_t{0xc004000000000000}),copied(0x7f,::std::uint32_t{123}),
        copied(0x7f,::std::uint32_t{17})};
    ::std::vector<variable_record> variables;
    for(::std::size_t i{};i!=types.size();++i)
    {
        variable_record v{};v.identity={1,i+1};v.name="v";v.scope=1;v.type=i;v.parameter=true;
        location_plan p{};p.kind=plan_kind::wasm_local_value;p.local_index=i;v.locations.push_back({{},p});
        variables.push_back(::std::move(v));
    }
    ::std::vector<numeric_variable> out{};
    auto query{[&](numeric_query_limits const& cap={})
    { return query_numeric_variables(scopes,types,variables,15,locals,locals.size(),out,cap); }};
    check(query()==inline_query_error::none&&out.size()==6,"typed copied scalar inventory");
    ::std::array<::std::string_view,6> expected{"const int","const _Bool","const float","const double","const Ticket","const volatile int"};
    for(::std::size_t i{};i!=out.size();++i)
    { check(out[i].type_name==expected[i]&&out[i].reason==numeric_unavailable_reason::none,"original qualifiers and named alias"); }
    check(numeric_signed_value(out[0])==-99&&out[1].kind==numeric_kind::boolean&&out[1].bits==1&&
        out[2].kind==numeric_kind::f32_bits&&out[2].bits==0x3fa00000&&
        out[3].kind==numeric_kind::f64_bits&&out[3].bits==0xc004000000000000&&out[4].bits==123,
        "formatting never changes actual native carrier bits or scalar classification");
    // Generated prefix bytes, not just the original base name, share one output budget.
    variables.resize(1);numeric_query_limits cap{};cap.max_result_string_bytes=10;
    check(query(cap)==inline_query_error::none&&out[0].type_name=="const int","exact name plus generated type boundary");
    cap.max_result_string_bytes=9;
    check(query(cap)==inline_query_error::limit_exceeded&&out.empty(),"generated qualifier budget clears all output");
    variables.push_back(variables[0]);cap.max_result_string_bytes=19;
    check(query(cap)==inline_query_error::limit_exceeded&&out.empty(),"late shared budget failure publishes no partial list");
    variables.resize(1);types[0].display_qualifiers=16;
    check(query()==inline_query_error::malformed&&out.empty(),"malformed C qualifier cannot become a label");
    types[0]=integer;types[0].language=0x1c;types[0].name="i32";
    check(query()==inline_query_error::none&&out[0].type_name=="i32"&&numeric_signed_value(out[0])==-99,
        "Rust does not acquire C spelling from modifier bits");
    types[0]=integer;types[0].tinygo_producer=true;
    check(query()==inline_query_error::none&&out[0].type_name=="int","TinyGo producer overrides C-like language metadata");
    types[0].tinygo_producer=false;types[0].zig_producer=true;
    check(query()==inline_query_error::none&&out[0].type_name=="int","Zig producer retains its own spelling");
    types[0]=integer;types[0].language=0;
    check(query()==inline_query_error::none&&out[0].type_name=="int","unknown CU language is not guessed from int");
    types[0]=integer;locals[0].available=false;
    check(query()==inline_query_error::none&&out[0].type_name=="const int"&&
        out[0].kind==numeric_kind::unavailable&&out[0].reason==numeric_unavailable_reason::local_unavailable,
        "qualified display never authorizes a missing capture");
    locals[0].available=true;variables[0].locations.clear();
    check(query()==inline_query_error::none&&out[0].reason==numeric_unavailable_reason::no_location&&
        out[0].type_name=="const int","qualified display never synthesizes a missing location");
    // Finite constant-plan DATA, separately from LLVM decoder/real producer
    // qualification. No locals or memory reads; WebAssembly bytes stay little
    // endian even on big-endian hosts. These plans match the decoder's owned
    // implicit scalar representation, never a host floating pointer.
    types = {single, wide}; variables.clear(); locals.clear();
    ::std::array<::std::uint64_t,4u> constant_bits{0x80000000u,0x7fc01234u,
        0x7ff0000000000000ULL,0x7ff8000012345678ULL};
    for(::std::size_t i{};i!=constant_bits.size();++i)
    {
        variable_record v{};v.identity={1,i+1};v.name="constant";v.scope=1;v.type=i<2u?0u:1u;
        location_plan p{};p.kind=plan_kind::constant_value;p.reason=unavailable_reason::none;
        p.implicit_constant=true;p.direct_constant_attribute=true;p.byte_count=i<2u?4u:8u;p.address_bytes=8u;
        for(::std::size_t j{};j!=p.byte_count;++j)
        { p.implicit_bytes[j]=static_cast<::std::byte>((constant_bits[i]>>(8u*j))&0xffu); }
        v.locations.push_back({{},p});variables.push_back(::std::move(v));
    }
    check(query()==inline_query_error::none&&out.size()==4u,"constant plans require no native local or guest memory");
    for(::std::size_t i{};i!=out.size();++i)
    {
        check(out[i].bits==constant_bits[i]&&out[i].reason==numeric_unavailable_reason::none&&
              out[i].kind==(i<2u?numeric_kind::f32_bits:numeric_kind::f64_bits)&&
              out[i].type_name==(i<2u?"const float":"const double"),
              "negative zero, infinities and NaN payloads retain exact target bits and qualifiers");
    }
    variables[0].locations[0].plan.byte_count=3u;
    check(query()==inline_query_error::none&&out[0].reason==numeric_unavailable_reason::implicit_width_mismatch,
          "mismatched constant width cannot be padded into a value");
    variables[0].locations[0].plan.implicit_constant=false;
    check(query()==inline_query_error::none&&out[0].reason==numeric_unavailable_reason::unsupported_plan,
          "integer DW_OP constants cannot infer floating conversions");
    // Finite UTF direct constant plans: decoder admission and genuine product
    // provenance are qualified separately in native LLVM and CLI/DAP recipes.
    types.clear(); variables.clear();
    ::std::array<::std::uint64_t,4u> utf_bits{0x80u,0x3bbu,0x1f642u,0xd83du};
    for(::std::size_t i{};i!=utf_bits.size();++i)
    {
        type_record t{integer}; t.encoding=0x10u;
        t.byte_count=i==0u?1u:i==2u?4u:2u;
        t.byte_size=t.byte_count; t.size_known=true;
        t.name=i==0u?"char8_t":i==2u?"char32_t":"char16_t";
        types.push_back(t);
        variable_record v{};v.identity={2,i+1};v.name="unit";v.scope=1;v.type=i;
        location_plan p{};p.kind=plan_kind::constant_value;p.reason=unavailable_reason::none;
        p.constant_bits=utf_bits[i];p.byte_count=t.byte_count;p.address_bytes=4u;p.direct_constant_attribute=true;
        v.locations.push_back({{},p});variables.push_back(::std::move(v));
    }
    check(query()==inline_query_error::none&&out.size()==4u,"UTF constant plans need no carrier or guest memory");
    for(::std::size_t i{};i!=out.size();++i)
    { check(out[i].bits==utf_bits[i]&&out[i].kind==numeric_kind::unsigned_integer&&
            out[i].reason==numeric_unavailable_reason::none&&out[i].type_name==
            (i==0u?"const char8_t":i==2u?"const char32_t":"const char16_t"),
            "UTF code-unit target bits and type labels are identical on every host"); }
    types[0].byte_count=8u;types[0].byte_size=8u;
    check(query()==inline_query_error::none&&out[0].reason==numeric_unavailable_reason::unsupported_type&&
          out[1].bits==0x3bbu,"unsupported UTF width does not erase unrelated constants");
    types[0].byte_count=1u;types[0].byte_size=1u;types[0].kind=type_kind::pointer;
    check(query()==inline_query_error::none&&out[0].reason==numeric_unavailable_reason::unsupported_type,
          "direct constant metadata grants no pointer access");
    ::fast_io::io::println("PASS qualified numeric values: producer language, modifiers, aliases, native carriers, budgets and unavailable captures");
}
