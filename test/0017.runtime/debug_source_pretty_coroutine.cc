// Owned metadata/copied-byte qualification only. This fixture has no runtime
// reader capability, native pointer, executable function pointer or coroutine lifetime
// authority. Run it only in the authorized SSH Linux 64 GiB cgroup.
#include <fast_io.h>
#include <uwvm2/uwvm/debugger/source_pretty_printer.h>

namespace dwarf = uwvm2::uwvm::debugger::source_dwarf;
namespace coroutine = uwvm2::uwvm::debugger::source_pretty_printer::coroutine_details;
static void check(bool accepted, char const* message)
{
    if(!accepted)
    { ::fast_io::io::perrln("debug_source_pretty_coroutine: ",::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); }
}
static dwarf::type_record scalar(::std::string name, unsigned width, unsigned encoding)
{
    dwarf::type_record out{};out.name=::std::move(name);out.kind=dwarf::type_kind::scalar;
    out.language=0x002au;out.byte_size=width;out.byte_count=static_cast<::std::uint8_t>(width);out.encoding=encoding;out.size_known=true;return out;
}
static dwarf::type_record object(::std::string name, unsigned size)
{
    dwarf::type_record out{};out.name=::std::move(name);out.kind=dwarf::type_kind::structure;
    out.language=0x002au;out.byte_size=size;out.size_known=true;return out;
}
static dwarf::type_record pointer(unsigned width, ::std::size_t target=dwarf::no_record)
{
    auto out{object("",width)};out.kind=dwarf::type_kind::pointer;out.byte_count=static_cast<::std::uint8_t>(width);out.referenced_type=target;return out;
}
static dwarf::member_record field(::std::string name, ::std::size_t type, unsigned offset)
{
    dwarf::member_record out{};out.name=::std::move(name);out.type=type;out.byte_offset=offset;out.offset_known=true;return out;
}
static void put(::std::vector<::std::byte>& bytes, unsigned offset, unsigned width, ::std::uint64_t value)
{
    check(width<=8u && offset<=bytes.size() && width<=bytes.size()-offset,"bounded fixture byte write");
    for(unsigned i{};i!=width;++i) { bytes[offset+i]=static_cast<::std::byte>((value>>(i*8u))&255u); }
}
static ::std::vector<dwarf::type_record> metadata(unsigned width)
{
    ::std::vector<dwarf::type_record> types{scalar("unsigned char",1u,8u),scalar("unsigned short",2u,7u),scalar("unsigned int",4u,7u),pointer(width)};
    auto promise{object("generator::promise_type",4u)};promise.members={field("current_value",2u,0u)};types.push_back(::std::move(promise));
    auto clang{object("_Z9generatorv.coro_frame_ty",width*2u+8u)};
    clang.members={field("__resume_fn",3u,0u),field("__destroy_fn",3u,width),field("__promise",4u,width*2u),field("__coro_index",0u,width*2u+4u)};
    types.push_back(::std::move(clang));
    auto handle{object("std::coroutine_handle<generator::promise_type>",width)};
    handle.members={field("__handle",3u,0u)};types.push_back(::std::move(handle));
    auto gcc{object("_Z9generatorv.Frame",width*2u+8u)};
    gcc.members={field("_Coro_resume_fn",9u,0u),field("_Coro_destroy_fn",9u,width),field("_Coro_promise",4u,width*2u),field("_Coro_resume_index",1u,width*2u+4u)};
    types.push_back(::std::move(gcc));
    dwarf::type_record signature{};signature.kind=dwarf::type_kind::subroutine;signature.language=0x002au;
    signature.signature_complete=true;signature.parameter_types={10u};types.push_back(::std::move(signature));
    types.push_back(pointer(width,8u));types.push_back(pointer(width,7u));return types;
}
static ::std::vector<dwarf::object_node> copied(::std::span<dwarf::type_record const> types, ::std::size_t root,
    ::std::vector<::std::byte> const& bytes)
{
    ::std::vector<dwarf::object_node> out{};
    check(dwarf::query_object_value(types,root,bytes,out)==dwarf::inline_query_error::none && !out.empty(),"owned copied object query");return out;
}
static void cases(unsigned width)
{
    auto types{metadata(width)};::std::vector<::std::byte> bytes(width*2u+8u);
    // These scalar integers represent Wasm table indices only. No host
    // executable address is manufactured or invoked by this fixture.
    put(bytes,0u,width,11u);put(bytes,width,width,12u);put(bytes,width*2u,4u,42u);put(bytes,width*2u+4u,1u,3u);
    auto nodes{copied(types,5u,bytes)};auto const raw{nodes};
    check(coroutine::summarize(types,nodes) && nodes[0u].pretty_available &&
        nodes[0u].pretty_printer=="cxx.coroutine_frame.clang" && nodes[0u].pretty_summary.find("suspended/running not proven")!=::std::string::npos &&
        nodes[0u].pretty_summary.find("suspension-index=3")!=::std::string::npos &&
        nodes[0u].pretty_summary.find("promise=raw copied child")!=::std::string::npos,"Clang copied frame conservative summary and promise");
    check(nodes.size()==raw.size(),"printer adds no synthetic nodes or reads");
    for(::std::size_t i{};i!=nodes.size();++i)
    { check(nodes[i].parent==raw[i].parent && nodes[i].type==raw[i].type && nodes[i].bits==raw[i].bits && nodes[i].byte_offset==raw[i].byte_offset &&
        nodes[i].byte_size==raw[i].byte_size && nodes[i].name==raw[i].name,"raw copied object remains intact"); }
    put(bytes,0u,width,0u);nodes=copied(types,5u,bytes);
    check(coroutine::summarize(types,nodes) && nodes[0u].pretty_summary.find("state=done")!=::std::string::npos &&
        nodes[0u].pretty_summary.find("lifetime not proven")!=::std::string::npos,"null resume yields only final-suspend marker");
    put(bytes,width,width,0u);nodes=copied(types,5u,bytes);
    check(coroutine::summarize(types,nodes) && nodes[0u].pretty_summary.find("state=unknown")!=::std::string::npos,"zero function values do not prove destruction");
    ::std::vector<::std::byte> known(bytes.size(),::std::byte{255u});known[0u]=::std::byte{};
    check(dwarf::query_object_value_with_known_bits(types,5u,bytes,known,nodes)==dwarf::inline_query_error::none &&
        coroutine::summarize(types,nodes) && nodes[0u].pretty_summary.find("state=unknown")!=::std::string::npos,"partial unavailable pointer bits never infer done");
    auto bad{types};bad[5u].name="UserObject";nodes=copied(bad,5u,bytes);
    check(!coroutine::summarize(bad,nodes),"member names without compiler frame type are insufficient");
    bad=types;bad[5u].members[1u].byte_offset=0u;nodes=copied(bad,5u,bytes);
    check(!coroutine::summarize(bad,nodes),"overlapping resume and destroy fields reject schema");
    bad=types;bad[3u].address_class_known=true;bad[3u].address_class=1u;nodes=copied(bad,5u,bytes);
    check(!coroutine::summarize(bad,nodes),"nondefault pointer address class rejected");
    bad=types;bad[5u].members.push_back(bad[5u].members[0u]);nodes=copied(bad,5u,bytes);
    check(!coroutine::summarize(bad,nodes),"duplicate semantic fields reject schema");
    bad=types;bad[0u].encoding=6u;nodes=copied(bad,5u,bytes);
    check(!coroutine::summarize(bad,nodes),"signed index rejected");
    nodes=copied(types,5u,bytes);nodes[1u].parent=nodes.size();
    check(!coroutine::summarize(types,nodes) && !nodes[0u].pretty_available,"malformed copied tree rejected before mutation");
    put(bytes,0u,width,11u);put(bytes,width,width,12u);put(bytes,width*2u+4u,2u,6u);nodes=copied(types,7u,bytes);
    check(coroutine::summarize(types,nodes) && nodes[0u].pretty_printer=="cxx.coroutine_frame.gcc" &&
        nodes[0u].pretty_summary.find("suspension-index=6")!=::std::string::npos,"GCC exact void(Frame*) signature and ushort index");
    put(bytes,width*2u+4u,2u,7u);nodes=copied(types,7u,bytes);
    check(coroutine::summarize(types,nodes) && nodes[0u].pretty_summary.find("destroy path; destruction not proven")!=::std::string::npos,"GCC odd index is destroy path rather than destroyed lifetime");
    bad=types;bad[8u].parameter_types[0u]=3u;nodes=copied(bad,7u,bytes);
    check(!coroutine::summarize(bad,nodes),"erased GCC function parameter cannot establish self-frame ABI");
    bad=types;bad[8u].variadic=true;nodes=copied(bad,7u,bytes);
    check(!coroutine::summarize(bad,nodes),"variadic GCC function does not match frame ABI");
    ::std::vector<::std::byte> handle_bytes(width);put(handle_bytes,0u,width,0x1234u);nodes=copied(types,6u,handle_bytes);
    check(coroutine::summarize(types,nodes) && nodes[0u].pretty_printer=="cxx.coroutine_handle" &&
        nodes[0u].pretty_summary.find("frame=guest:0x1234")!=::std::string::npos &&
        nodes[0u].pretty_summary.find("frame ABI unavailable")!=::std::string::npos && nodes.size()==2u,"erased handle never guesses or follows frame layout");
    put(handle_bytes,0u,width,0u);nodes=copied(types,6u,handle_bytes);
    check(coroutine::summarize(types,nodes) && nodes[0u].pretty_summary.find("state=empty")!=::std::string::npos,"null handle is empty without frame read");
    bad=types;bad[6u].name="std::__1::coroutine_handle<void>";nodes=copied(bad,6u,handle_bytes);
    check(coroutine::summarize(bad,nodes),"libc++ inline namespace erased handle");
    bad=types;bad[6u].name="coroutine_handle<generator::promise_type>";nodes=copied(bad,6u,handle_bytes);
    check(coroutine::summarize(bad,nodes),"raw unqualified DW_AT_name retains explicit unavailable frame ABI");
    bad=types;bad[6u].language=0x001cu;nodes=copied(bad,6u,handle_bytes);
    check(!coroutine::summarize(bad,nodes),"standard spelling in another source language grants no C++ ABI");
    bad=types;bad[6u].members.push_back(field("extra",3u,0u));nodes=copied(bad,6u,handle_bytes);
    check(!coroutine::summarize(bad,nodes),"handle requires exact one-pointer schema");
    // A real frame can also be a member of an already copied outer object.
    auto outer{object("Holder",static_cast<unsigned>(types[5u].byte_size)+4u)};outer.members={field("frame",5u,4u)};types.push_back(::std::move(outer));
    ::std::vector<::std::byte> outer_bytes(bytes.size()+4u);
    for(::std::size_t i{};i!=bytes.size();++i) { outer_bytes[i+4u]=bytes[i]; }
    nodes=copied(types,11u,outer_bytes);
    check(coroutine::summarize(types,nodes) && !nodes[0u].pretty_available && nodes[1u].pretty_available,"nested raw frame retains actual parent-relative extent");
}
static void indirect_locations(unsigned width)
{
    // Owned DATA reader only. A real pause/read/source capability is supplied
    // solely by the production controller and tested with original Wasm guests.
    ::std::vector<::std::byte> expression{::std::byte{0x91},::std::byte{44},::std::byte{0x06},::std::byte{0x23},::std::byte{8}};
    auto plan{dwarf::decode_location_plan(expression,static_cast<::std::uint8_t>(width))};
    check(plan.kind==dwarf::plan_kind::frame_relative_indirect_offset && plan.displacement==44 &&
        plan.indirect_addend==8 && plan.reason==dwarf::unavailable_reason::none,"finite actual Clang coroutine location recipe");
    dwarf::location_plan base{};base.kind=dwarf::plan_kind::wasm_local_frame_base;base.reason=dwarf::unavailable_reason::none;base.address_bytes=width;
    ::std::vector<dwarf::location_record> frames{{{},base}};
    ::std::array<dwarf::copied_numeric_local,1> locals{};locals[0].available=true;locals[0].wasm_type=width==4u ? 0x7fu : 0x7eu;
    auto native_base{[&](::std::uint64_t value)
    { if(width==4u){auto narrow=static_cast<::std::uint32_t>(value);::std::memcpy(locals[0].bytes.data(),::std::addressof(narrow),4u);}
      else {::std::memcpy(locals[0].bytes.data(),::std::addressof(value),8u);} }};
    native_base(128u);::std::vector<::std::byte> word(width);put(word,0u,width,512u);
    unsigned calls{};bool allowed=true,short_read=false;
    auto read{[&](::std::uint64_t offset,::std::size_t bytes,::std::vector<::std::byte>& out)
    { ++calls;check(offset==172u && bytes==width,"indirect read uses exact checked slot and guest ABI width");
      if(!allowed)return false;out=word;if(short_read)out.pop_back();return true; }};
    ::std::uint64_t offset{999u};
    auto resolve{[&]{calls=0u;offset=999u;return dwarf::resolve_indirect_frame_offset(plan,frames,2u,locals,1u,read,offset);}};
    check(resolve()==dwarf::object_location_error::none && calls==1u && offset==520u,"one little-endian guest pointer read then checked addend");
    allowed=false;check(resolve()!=dwarf::object_location_error::none && calls==1u && offset==0u,"reader denial produces no offset");allowed=true;
    short_read=true;check(resolve()!=dwarf::object_location_error::none && calls==1u && offset==0u,"partial pointer bytes cannot become an offset");short_read=false;
    put(word,0u,width,0u);check(resolve()!=dwarf::object_location_error::none && calls==1u && offset==0u,"null pointer plus nonzero addend stays unavailable");
    auto maximum{width==4u ? ::std::uint64_t{0xffffffffu} : ~::std::uint64_t{}};
    put(word,0u,width,maximum-3u);check(resolve()==dwarf::object_location_error::address_overflow && calls==1u && offset==0u,"target addition cannot wrap guest address width");
    put(word,0u,width,512u);
    locals[0].available=false;check(resolve()!=dwarf::object_location_error::none && calls==0u && offset==0u,"uncaptured frame base cannot authorize an intermediate read");locals[0].available=true;
    auto tag=locals[0].wasm_type;locals[0].wasm_type=0x7du;check(resolve()!=dwarf::object_location_error::none && calls==0u,"float carrier is not a guest stack base");locals[0].wasm_type=tag;
    native_base(maximum-20u);check(resolve()==dwarf::object_location_error::address_overflow && calls==0u && offset==0u,"slot arithmetic checked before any guest read");native_base(128u);
    plan.pieces.resize(1u);check(resolve()==dwarf::object_location_error::malformed_metadata && calls==0u,"composite/nested pointer recipes are not admitted");plan.pieces.clear();
    plan.integer_transform_count=1u;check(resolve()==dwarf::object_location_error::malformed_metadata && calls==0u,"integer transforms cannot add another interpretation");plan.integer_transform_count=0u;
    frames.push_back(frames[0]);check(resolve()!=dwarf::object_location_error::none && calls==0u,"ambiguous frame bases refuse intermediate reads");frames.pop_back();
    ::std::vector<::std::byte> sized{::std::byte{0x91},::std::byte{44},::std::byte{0x94},static_cast<::std::byte>(width),::std::byte{0x23},::std::byte{8}};
    check(dwarf::decode_location_plan(sized,static_cast<::std::uint8_t>(width)).kind==dwarf::plan_kind::frame_relative_indirect_offset,
        "explicit address-width deref_size describes the same finite operation");
    sized[3]=::std::byte{1};check(dwarf::decode_location_plan(sized,static_cast<::std::uint8_t>(width)).kind==dwarf::plan_kind::unavailable,"partial-width loads cannot fabricate a pointer");
    for(auto tail:{::std::byte{0x06},::std::byte{0x9f},::std::byte{0x96}})
    { auto bad=expression;bad.push_back(tail);check(dwarf::decode_location_plan(bad,static_cast<::std::uint8_t>(width)).kind==dwarf::plan_kind::unavailable,
        "extra indirection, stack_value or trailing operations stay unsupported"); }
    auto truncated=expression;truncated.pop_back();check(dwarf::decode_location_plan(truncated,static_cast<::std::uint8_t>(width)).kind==dwarf::plan_kind::unavailable,"missing addend is not interpreted as zero");
    check(dwarf::decode_location_plan(expression,static_cast<::std::uint8_t>(width),{},dwarf::position_role::frame_base).kind==dwarf::plan_kind::unavailable,
        "a frame-base attribute cannot introduce a recursive guest pointer load");
    dwarf::limits cap{};cap.max_expression_bytes=expression.size()-1u;
    check(dwarf::decode_location_plan(expression,static_cast<::std::uint8_t>(width),cap).reason==dwarf::unavailable_reason::expression_limit,
        "expression byte cap retains precedence");
}
static void clang18_abi()
{
    auto legacy{[]()
    {
        auto types=metadata(4u);
        for(::std::size_t i{};i!=types.size();++i)types[i].identity=types[i].declaration_identity={7u,100u+i};
        types[5u].members[1u].byte_offset=8u;
        types[5u].clang18_wasm32_coro_dwarf=true;
        return types;
    }};
    auto types=legacy();
    check(dwarf::normalize_clang18_wasm32_coroutine(types,5u) && types[5u].members[1u].byte_offset==4u &&
        types[5u].original_coro_destroy_offset==8u,"exact known compiler metadata retains original offset and target ABI position");
    check(dwarf::normalize_clang18_wasm32_coroutine(types,5u),"owned normalization is idempotent");
    ::std::vector<::std::byte> bytes(types[5u].byte_size);put(bytes,0u,4u,9u);put(bytes,4u,4u,17u);put(bytes,8u,4u,23u);put(bytes,12u,1u,2u);
    auto nodes=copied(types,5u,bytes);
    check(coroutine::summarize(types,nodes,4u) && nodes[0].pretty_summary.find("compiler ABI correction: Clang18 Wasm32")!=::std::string::npos,
        "normalized real ABI fields are visibly distinguished from original DWARF");
    types=legacy();types[5u].clang18_wasm32_coro_dwarf=false;
    check(!dwarf::normalize_clang18_wasm32_coroutine(types,5u) && types[5u].members[1].byte_offset==8u,"matching spelling alone cannot repair unknown compiler metadata");
    types=legacy();types[5u].members.push_back(types[5u].members[1]);
    check(!dwarf::normalize_clang18_wasm32_coroutine(types,5u) && types[5u].members[1].byte_offset==8u,"ambiguous compiler header never normalizes");
    types=legacy();types[3u].atomic_scalar=true;
    check(!dwarf::normalize_clang18_wasm32_coroutine(types,5u),"atomic pointer descriptor cannot establish a coroutine ABI");
    types=legacy();types[5u].members[2].byte_offset=12u;
    check(!dwarf::normalize_clang18_wasm32_coroutine(types,5u),"promise offset must prove this exact legacy header");
    types=legacy();types[4u].declaration_identity.unit=8u;
    check(!dwarf::normalize_clang18_wasm32_coroutine(types,5u),"foreign promise declaration cannot establish compiler-frame evidence");
    types=legacy();types[5u].members[1].offset_known=false;
    check(!dwarf::normalize_clang18_wasm32_coroutine(types,5u),"unavailable original offset cannot be replaced by a layout guess");
    types=legacy();types[5u].language=0x1cu;
    check(!dwarf::normalize_clang18_wasm32_coroutine(types,5u),"another language cannot borrow the Clang C++ compatibility profile");
    types=legacy();types[5u].name="user::Frame";
    check(!dwarf::normalize_clang18_wasm32_coroutine(types,5u),"ordinary user frames cannot borrow a compiler-generated schema");
    types=metadata(8u);bytes.assign(types[5u].byte_size,::std::byte{});nodes=copied(types,5u,bytes);
    check(!coroutine::summarize(types,nodes,4u),"actual Wasm32 context refuses a pointer-width-eight coroutine schema");
    types=legacy();types[5u].byte_size=8u;
    check(!dwarf::normalize_clang18_wasm32_coroutine(types,5u),"all compiler members remain within the original frame extent");
}

static void handle_abi(bool current=false)
{
    auto types{metadata(4u)};
    for(::std::size_t i{};i!=types.size();++i)
    {types[i].identity={7u,100u+i};types[i].declaration_identity=types[i].identity;}
    auto& handle{types[6]};handle.clang18_wasi32_coro_handle=!current;handle.clang23_wasi32_coro_handle=current;
    handle.qualified_type_name="std::__2::coroutine_handle<generator::promise_type>";
    handle.members[0].name="__handle_";handle.template_types={4u};
    types[5].clang18_wasm32_coro_dwarf=!current;types[5].original_coro_destroy_offset=current ? 0u : 8u;
    types[5].clang23_wasm32_coro_dwarf=current;
    if(current)
    {
        check(dwarf::validate_clang23_wasm32_coroutine(types,5u),"exact modern frame schema validates without correction");
        check(types[5].members[1].byte_offset==4u && types[5].original_coro_destroy_offset==0u,"modern original offsets remain intact");
        auto malformed{types};malformed[5].members[1].byte_offset=8u;
        check(!dwarf::validate_clang23_wasm32_coroutine(malformed,5u) && malformed[5].members[1].byte_offset==8u,"modern compiler cannot borrow legacy offset correction");
    }
    ::std::vector<::std::byte> address(4u),frame(16u);put(address,0u,4u,256u);
    put(frame,0u,4u,11u);put(frame,4u,4u,12u);put(frame,8u,4u,42u);
    unsigned reads{};bool short_header{},denied{};
    auto reader{[&](::std::uint64_t offset,::std::size_t count,::std::vector<::std::byte>& out)
    {
        ++reads;if(denied)return false;
        check((offset==256u && count==8u) || (offset==264u && count==4u),"handle uses only bounded ABI header and owned promise reads");
        auto start{static_cast<::std::size_t>(offset-256u)};
        out.assign(frame.begin()+start,frame.begin()+start+count);
        if(short_header && count==8u)out.pop_back();return true;
    }};
    auto run{[&](auto const& selected,auto const& value,auto const& known,auto const& eligible,
        uwvm2::uwvm::debugger::source_pretty_printer::limits cap={})
    {
        reads=0u;auto nodes{copied(selected,6u,value)};
        (void)uwvm2::uwvm::debugger::source_pretty_printer::format(selected,6u,value,known,eligible,4u,reader,nodes,cap);
        return nodes;
    }};
    ::std::vector<::std::byte> all(4u,::std::byte{255u}),none(4u);
    auto nodes{run(types,address,all,all)};
    check(reads==2u && nodes[0].pretty_summary.find("resume-table-index=11")!=::std::string::npos &&
        nodes[0].pretty_summary.find("suspended/running not proven")!=::std::string::npos &&
        nodes[0].pretty_summary.find("promise=raw copied child")!=::std::string::npos,"versioned handle header and typed promise");
    check(nodes[0].pretty_summary.find(current ? "ABI=LLVM23.1.0/libc++23.1/Wasm32" : "ABI=LLVM18.1.2/libc++18.1/Wasm32")!=::std::string::npos,"exact versioned ABI is visible");
    bool promise{};for(auto const& node:nodes)promise|=node.name=="current_value" && node.bits==42u && node.value_available;
    check(promise && nodes.size()==4u && nodes[2].detached_copy && nodes[2].name=="[promise]","typed promise is an independently copied read-only child");
    auto bad{types};bad[6].clang18_wasi32_coro_handle=false;bad[6].clang23_wasi32_coro_handle=false;nodes=run(bad,address,all,all);
    check(reads==0u && nodes[0].pretty_summary.find("frame ABI unavailable")!=::std::string::npos,"unknown compiler never follows a handle");
    bad=types;bad[6].clang18_wasi32_coro_handle=true;bad[6].clang23_wasi32_coro_handle=true;nodes=run(bad,address,all,all);
    check(reads==0u,"ambiguous compiler profiles never authorize reads");
    bad=types;bad[5].clang18_wasm32_coro_dwarf=current;bad[5].clang23_wasm32_coro_dwarf=!current;nodes=run(bad,address,all,all);
    check(reads==1u && nodes.size()==2u,"a handle cannot borrow a different compiler promise layout");
    nodes=run(types,address,all,none);check(reads==0u,"unqualified pointer carrier cannot authorize ABI reads");
    nodes=run(types,address,none,all);check(reads==0u,"unknown pointer bits cannot authorize ABI reads");
    bad=types;bad[6].declaration_identity.unit=8u;nodes=run(bad,address,all,all);check(reads==0u,"foreign owning CU rejects handle ABI");
    bad=types;bad[6].qualified_type_name="user::coroutine_handle<generator::promise_type>";nodes=run(bad,address,all,all);
    check(reads==0u,"user namespace does not borrow the standard handle ABI");
    short_header=true;nodes=run(types,address,all,all);short_header=false;
    check(reads==1u && nodes[0].pretty_summary.find("frame ABI unavailable")!=::std::string::npos,"short header read retains conservative raw summary");
    denied=true;nodes=run(types,address,all,all);denied=false;
    check(reads==1u && nodes.size()==2u,"reader denial never fabricates frame fields");
    uwvm2::uwvm::debugger::source_pretty_printer::limits cap{};cap.maximum_reads=1u;
    nodes=run(types,address,all,all,cap);check(reads==1u && nodes.size()==2u && nodes[0].pretty_truncated,"header and promise share session read budget");
    cap={};cap.max_elements=0u;nodes=run(types,address,all,all,cap);check(reads==1u && nodes.size()==2u && nodes[0].pretty_truncated,"zero element limit suppresses promise copy");
    bad=types;bad[4].declaration_identity.unit=8u;nodes=run(bad,address,all,all);
    check(reads==1u && nodes.size()==2u,"foreign promise identity cannot select an owned frame offset");
    bad=types;bad[6].template_types={dwarf::no_record};nodes=run(bad,address,all,all);
    check(reads==1u && nodes[0].pretty_summary.find("promise=erased")!=::std::string::npos,"erased handle has a header but no inferred promise");
    bad=types;bad.push_back(bad[4]);bad.back().identity.offset+=500u;bad[5].members[2].type=bad.size()-1u;
    nodes=run(bad,address,all,all);
    check(reads==2u && nodes.size()==4u,"promise aliases use the same final owned declaration DIE");
    ++bad.back().declaration_identity.offset;nodes=run(bad,address,all,all);
    check(reads==1u && nodes.size()==2u,"independent same-name same-size promise declarations remain unavailable");
    put(frame,0u,4u,0u);nodes=run(types,address,all,all);
    check(nodes[0].pretty_summary.find("done (final-suspend marker; lifetime not proven)")!=::std::string::npos,"final marker does not claim a live coroutine");
    put(frame,4u,4u,0u);nodes=run(types,address,all,all);
    check(nodes[0].pretty_summary.find("destroyed/uninitialized/unavailable not distinguished")!=::std::string::npos,"zero header cannot prove destruction");
    put(address,0u,4u,0u);nodes=run(types,address,all,all);
    check(reads==0u && nodes[0].pretty_summary.find("state=empty")!=::std::string::npos,"null versioned handle never reads memory");
}

int main()
{
    cases(4u);cases(8u);indirect_locations(4u);indirect_locations(8u);clang18_abi();handle_abi();handle_abi(true);
    ::fast_io::io::println("debug_source_pretty_coroutine: copied wasm32/wasm64 schemas passed");
}
