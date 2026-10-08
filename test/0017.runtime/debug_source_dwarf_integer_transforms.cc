// Finite owned metadata/copies; genuine O1 producer witnesses are separate.
#include <uwvm2/uwvm/debugger/source_dwarf_index.h>
#include <uwvm2/uwvm/debugger/source_dwarf_values.h>
#include <fast_io.h>
using namespace uwvm2::uwvm::debugger::source_dwarf;
using byte_vector = ::std::vector<::std::byte>;
static void check(bool value, char const* message)
{ if(!value) { ::fast_io::io::perrln("debug_source_dwarf_integer_transforms: ",::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
static void fixed(byte_vector& out, ::std::uint64_t value, unsigned width)
{ for(unsigned i{};i!=width;++i) { out.push_back(::std::byte{static_cast<unsigned char>(value>>(8u*i))}); } }
static void leb(byte_vector& out, ::std::uint64_t value)
{ do { auto part{static_cast<unsigned char>(value&127u)};value>>=7u;out.push_back(::std::byte{static_cast<unsigned char>(part|(value?128u:0u))}); } while(value); }
static void string(byte_vector& out, ::std::string_view value)
{ for(auto c:value) { out.push_back(::std::byte{static_cast<unsigned char>(c)}); } out.push_back(::std::byte{}); }
struct fixture { byte_vector info{},abbrev{}; ::std::uint64_t base{},function{}; };
static fixture make_fixture(unsigned version, unsigned address, unsigned encoding=7u, unsigned bytes=1u, unsigned bits=1u, unsigned bit_offset=0u, unsigned reference=0u, bool composite=false)
{
    using namespace ::llvm::dwarf;
    fixture f{};
    auto abbreviation{[&](unsigned code,Tag tag,bool children,::std::initializer_list<::std::pair<Attribute,Form>> fields)
    { leb(f.abbrev,code);leb(f.abbrev,tag);fixed(f.abbrev,children,1u);for(auto [a,b]:fields){leb(f.abbrev,a);leb(f.abbrev,b);}leb(f.abbrev,0u);leb(f.abbrev,0u); }};
    abbreviation(1u,DW_TAG_compile_unit,true,{{DW_AT_name,DW_FORM_string},{DW_AT_low_pc,DW_FORM_addr},{DW_AT_high_pc,DW_FORM_data4}});
    abbreviation(2u,DW_TAG_base_type,false,{{DW_AT_name,DW_FORM_string},{DW_AT_encoding,DW_FORM_data1},{DW_AT_byte_size,DW_FORM_data1},{DW_AT_bit_size,DW_FORM_data1},{DW_AT_bit_offset,DW_FORM_data1}});
    abbreviation(3u,DW_TAG_subprogram,true,{{DW_AT_name,DW_FORM_string},{DW_AT_low_pc,DW_FORM_addr},{DW_AT_high_pc,DW_FORM_data4}});
    abbreviation(4u,DW_TAG_formal_parameter,false,{{DW_AT_name,DW_FORM_string},{DW_AT_type,DW_FORM_ref4},{DW_AT_location,DW_FORM_exprloc}});
    leb(f.abbrev,0u);
    byte_vector body{};fixed(body,version,2u);
    if(version==5u){fixed(body,1u,1u);fixed(body,address,1u);fixed(body,0u,4u);}
    else {fixed(body,0u,4u);fixed(body,address,1u);}
    leb(body,1u);string(body,"transform.c");fixed(body,10u,address);fixed(body,20u,4u);
    f.base=body.size()+4u;
    leb(body,2u);string(body,"name_does_not_set_width");fixed(body,encoding,1u);fixed(body,bytes,1u);fixed(body,bits,1u);fixed(body,bit_offset,1u);
    f.function=body.size()+4u;leb(body,3u);string(body,"probe");fixed(body,10u,address);fixed(body,20u,4u);
    leb(body,4u);string(body,"value");fixed(body,f.base,4u);
    byte_vector expression{};fixed(expression,0xedu,1u);leb(expression,0u);leb(expression,0u);
    fixed(expression,0xa8u,1u);leb(expression,reference==1u?f.function:reference==2u?f.base+1u:reference==3u?0xffffu:f.base);
    fixed(expression,0x9fu,1u);
    if(composite) { for(unsigned v:{0x93u,1u,0x37u,0x9fu,0x93u,1u}) { fixed(expression,v,1u); } }
    leb(body,expression.size());body.insert(body.end(),expression.begin(),expression.end());
    leb(body,0u);leb(body,0u);fixed(f.info,body.size(),4u);f.info.insert(f.info.end(),body.begin(),body.end());return f;
}
static error parse(fixture const& f, ::std::unique_ptr<uwvm2::uwvm::debugger::source_dwarf::index>& out)
{
    ::std::array<section,2u> sections{{{".debug_info",f.info},{".debug_abbrev",f.abbrev}}};
    return uwvm2::uwvm::debugger::source_dwarf::index::parse({sections,100u,4u},out);
}
int main()
{
    for(unsigned address:{4u,8u})
    {
        ::std::array<::std::byte,7u> mask{::std::byte{0xed},::std::byte{0},::std::byte{0},::std::byte{0x10},::std::byte{1},::std::byte{0x1a},::std::byte{0x9f}};
        auto plan{decode_location_plan(mask,static_cast<::std::uint8_t>(address))};
        check(plan.kind==plan_kind::wasm_local_value && plan.integer_transform_count==1u,"finite DWARF4 mask admitted");
        ::std::uint64_t value{254u};check(transform_copied_integer(plan,value)&&value==0u,"mask is evaluated, not ignored");
        value=255u;check(transform_copied_integer(plan,value)&&value==1u,"odd copied local preserves low bit");
        check(decode_location_plan(mask,static_cast<::std::uint8_t>(address),{},position_role::frame_base).kind==plan_kind::unavailable,"no frame-base capability from transforms");
        auto bad{mask};bad[1]=::std::byte{1};check(decode_location_plan(bad,static_cast<::std::uint8_t>(address)).kind==plan_kind::unavailable,"no global access");
        bad=mask;bad[5]=::std::byte{0x22};check(decode_location_plan(bad,static_cast<::std::uint8_t>(address)).kind==plan_kind::unavailable,"no general arithmetic evaluation");
        bad=mask;bad[6]=::std::byte{0x06};check(decode_location_plan(bad,static_cast<::std::uint8_t>(address)).kind==plan_kind::unavailable,"no dereference");
        ::std::array<copied_numeric_local,1u> locals{};locals[0].wasm_type=0x7fu;locals[0].available=true;
        ::std::uint32_t carrier{254u};::std::memcpy(locals[0].bytes.data(),::std::addressof(carrier),sizeof(carrier));
        type_record type{};type.kind=type_kind::scalar;type.encoding=2u;type.byte_count=1u;
        numeric_variable result{};value_details::copy_value(plan,type,locals,1u,result);
        check(result.kind==numeric_kind::boolean&&result.bits==0u,"runtime copier evaluates transform before source truncation");
        locals[0].available=false;result={};value_details::copy_value(plan,type,locals,1u,result);
        check(result.reason==numeric_unavailable_reason::local_unavailable,"transforms cannot manufacture initialization");
        locals[0].available=true;type.encoding=4u;type.byte_count=4u;locals[0].wasm_type=0x7du;result={};
        value_details::copy_value(plan,type,locals,1u,result);check(result.kind==numeric_kind::unavailable,"no float reinterpretation");
    }
    for(unsigned version:{4u,5u}) for(unsigned width:{1u,8u,64u})
    {
        auto f{make_fixture(version,4u,7u,width==64u?8u:1u,width)};
        ::std::unique_ptr<uwvm2::uwvm::debugger::source_dwarf::index> index{};
        check(parse(f,index)==error::none&&index,"actual immutable parser resolves exact base DIE");
        auto plan{index->variables()[0].locations[0].plan};
        check(plan.kind==plan_kind::wasm_local_value && plan.integer_transforms[0].resolved && plan.integer_transforms[0].bit_width==width &&
              plan.integer_transforms[0].type_identity.offset==f.base,"bit width from attributes, never spelling");
        ::std::uint64_t value{~::std::uint64_t{0}};check(transform_copied_integer(plan,value)&&value==(width==64u?~::std::uint64_t{0}:(::std::uint64_t{1}<<width)-1u),"bounded unsigned conversion");
        auto second{f.info};f.info.insert(f.info.end(),second.begin(),second.end());check(parse(f,index)==error::none&&index,"two CUs with identical relative offsets");
        check(index->variables()[1].locations[0].plan.integer_transforms[0].type_identity.unit==second.size(),"conversion reference remains in current CU");
        plan.integer_transforms[0].resolved=false;value=255u;check(!transform_copied_integer(plan,value),"unresolved plan never usable");
    }
    for(auto config : {::std::array<unsigned,4u>{5u,1u,1u,0u}, {4u,4u,32u,0u},{7u,0u,1u,0u},{7u,9u,1u,0u},{7u,1u,0u,0u},{7u,1u,9u,0u},{7u,1u,1u,1u}})
    {
        auto f{make_fixture(5u,4u,config[0],config[1],config[2],config[3])};
        ::std::unique_ptr<uwvm2::uwvm::debugger::source_dwarf::index> index{};
        check(parse(f,index)==error::none&&index&&index->variables()[0].locations[0].plan.kind==plan_kind::unavailable,"unsupported target encoding/extent/offset stays unavailable");
    }
    for(unsigned reference:{1u,2u,3u})
    {
        auto f{make_fixture(5u,4u,7u,1u,1u,0u,reference)};
        ::std::unique_ptr<uwvm2::uwvm::debugger::source_dwarf::index> index{};auto result{parse(f,index)};
        check(reference==1u ? result==error::none&&index&&index->variables()[0].locations[0].plan.kind==plan_kind::unavailable : result==error::malformed&&!index,"wrong DIE/middle/out-of-CU references rejected");
    }
    for(unsigned version:{4u,5u}) for(unsigned encoding:{7u,4u})
    {
        auto f{make_fixture(version,4u,encoding,1u,1u,0u,0u,true)};
        ::std::unique_ptr<uwvm2::uwvm::debugger::source_dwarf::index> index{};
        check(parse(f,index)==error::none&&index,"composite CU parse");
        auto const& plan{index->variables()[0].locations[0].plan};
        check(plan.kind==plan_kind::composite_value&&plan.pieces.size()==2u,"base DIE resolution preserves aggregate");
        auto const& atom{plan.pieces[0].atom};
        check(encoding==7u ? atom.integer_transforms[0].resolved&&atom.integer_transforms[0].type_identity.offset==f.base :
              atom.kind==plan_kind::unavailable,"exact per-atom conversion or independent unsupported hole");
        ::std::array<copied_numeric_local,1u> locals{};locals[0].available=true;locals[0].wasm_type=0x7fu;
        ::std::uint32_t bits{255u};::std::memcpy(locals[0].bytes.data(),::std::addressof(bits),sizeof(bits));composite_value out{};
        check(materialize_location_pieces(plan,locals,1u,{},2u,out)==piece_query_error::none&&out.bytes[1]==::std::byte{7}&&
              out.known_bits[1]==::std::byte{255}&&(encoding==7u ? out.fully_available&&out.bytes[0]==::std::byte{1} : out.known_bits[0]==::std::byte{}),
              "atom conversion evaluated or quarantined without losing other fragments");
        auto second{f.info};f.info.insert(f.info.end(),second.begin(),second.end());
        check(parse(f,index)==error::none&&index,"two CU composite parse");
        if(encoding==7u) { check(index->variables()[1].locations[0].plan.pieces[0].atom.integer_transforms[0].type_identity.unit==second.size(),
              "piece conversion remains in current CU"); }
    }
    ::fast_io::io::println("PASS bounded integer transforms; exact CU type identity, width, initialization and capability rejection");
}
