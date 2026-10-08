// Real LLVM parser and fresh compiler DWARF; no stopped runtime authority.
#include <uwvm2/uwvm/debugger/source_dwarf_index.h>
#include <uwvm2/uwvm/debugger/source_frames.h>
#include <uwvm2/uwvm/debugger/source_dwarf_query.h>
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
#include <fast_io.h>
namespace dwarf=uwvm2::uwvm::debugger::source_dwarf;
namespace frames=uwvm2::uwvm::debugger::source_frames;
namespace scalar=uwvm2::uwvm::debugger::source_scalar_expression;
using bytes=::std::vector<::std::byte>;
static ::std::size_t checks{};
static void check(bool ok,char const* why)
{ ++checks;if(!ok) { ::fast_io::io::perrln("language index FAIL: ",::fast_io::mnp::os_c_str(why));::fast_io::fast_terminate(); } }
static void fixed(bytes& out,::std::uint64_t value,unsigned count)
{ for(unsigned i{};i<count;++i) { out.push_back(::std::byte{static_cast<unsigned char>(value>>(i*8u))}); } }
static void uleb(bytes& out,::std::uint64_t value)
{ do { auto b{static_cast<unsigned char>(value&0x7fu)};value>>=7u;out.push_back(::std::byte{static_cast<unsigned char>(b|(value ? 0x80u : 0u))}); } while(value); }
static void string(bytes& out,::std::string_view text)
{ for(auto c:text) { out.push_back(::std::byte{static_cast<unsigned char>(c)}); }out.push_back(::std::byte{}); }
static void patch(bytes& out,::std::size_t at,::std::uint64_t value)
{ check(at<=out.size() && 4u<=out.size()-at,"patch bounds");for(unsigned i{};i<4u;++i) { out[at+i]=::std::byte{static_cast<unsigned char>(value>>(i*8u))}; } }
struct fixture { bytes info{},abbrev{}; };
static fixture metadata(unsigned lang,::std::string_view producer,bool bad_form=false,bool cross=false)
{
    using namespace ::llvm::dwarf;
    fixture f{};
    auto ab{[&](unsigned id,Tag tag,bool children,::std::initializer_list<::std::pair<Attribute,Form>> fields)
    { uleb(f.abbrev,id);uleb(f.abbrev,tag);fixed(f.abbrev,children,1u);for(auto [a,v]:fields) { uleb(f.abbrev,a);uleb(f.abbrev,v); }uleb(f.abbrev,0u);uleb(f.abbrev,0u); }};
    ab(1u,DW_TAG_compile_unit,true,{{DW_AT_name,DW_FORM_string},{DW_AT_language,bad_form ? DW_FORM_flag : DW_FORM_data2},{DW_AT_producer,DW_FORM_string}});
    ab(2u,DW_TAG_subprogram,false,{{DW_AT_name,DW_FORM_string},{DW_AT_low_pc,DW_FORM_addr},{DW_AT_high_pc,DW_FORM_data4}});
    ab(3u,DW_TAG_subprogram,false,{{DW_AT_name,DW_FORM_string},{DW_AT_low_pc,DW_FORM_addr},{DW_AT_high_pc,DW_FORM_data4},{DW_AT_abstract_origin,DW_FORM_ref_addr}});
    uleb(f.abbrev,0u);
    auto header{[&]() { auto at{f.info.size()};fixed(f.info,0u,4u);fixed(f.info,4u,2u);fixed(f.info,0u,4u);fixed(f.info,4u,1u);return at; }};
    auto at{header()};uleb(f.info,1u);string(f.info,"a.c");fixed(f.info,lang,bad_form ? 1u : 2u);string(f.info,producer);
    uleb(f.info,cross ? 3u : 2u);string(f.info,"function");fixed(f.info,10u,4u);fixed(f.info,10u,4u);
    ::std::size_t reference{};if(cross) { reference=f.info.size();fixed(f.info,0u,4u); }
    uleb(f.info,0u);patch(f.info,at,f.info.size()-at-4u);
    if(cross)
    { at=header();uleb(f.info,1u);string(f.info,"b.cpp");fixed(f.info,0x1au,2u);string(f.info,"clang");
      auto target{f.info.size()};uleb(f.info,2u);string(f.info,"origin");fixed(f.info,30u,4u);fixed(f.info,10u,4u);uleb(f.info,0u);
      patch(f.info,at,f.info.size()-at-4u);patch(f.info,reference,target); }
    return f;
}

static fixture base_metadata(bool atomic,::std::string_view alias_name,::std::string_view base_name,unsigned encoding,unsigned extent,unsigned lang,::std::string_view producer="bounded metadata fixture")
{
 using namespace ::llvm::dwarf;fixture f{};
 auto ab{[&](unsigned id,Tag tag,bool children,::std::initializer_list<::std::pair<Attribute,Form>> fields)
 { uleb(f.abbrev,id);uleb(f.abbrev,tag);fixed(f.abbrev,children,1u);for(auto [a,v]:fields) { uleb(f.abbrev,a);uleb(f.abbrev,v); }uleb(f.abbrev,0u);uleb(f.abbrev,0u); }};
 ab(1u,DW_TAG_compile_unit,true,{{DW_AT_name,DW_FORM_string},{DW_AT_language,DW_FORM_data2},{DW_AT_producer,DW_FORM_string}});
 ab(2u,DW_TAG_subprogram,true,{{DW_AT_name,DW_FORM_string},{DW_AT_low_pc,DW_FORM_addr},{DW_AT_high_pc,DW_FORM_data4}});
 ab(3u,DW_TAG_variable,false,{{DW_AT_name,DW_FORM_string},{DW_AT_type,DW_FORM_ref4}});
 ab(4u,DW_TAG_typedef,false,{{DW_AT_name,DW_FORM_string},{DW_AT_type,DW_FORM_ref4}});
 ab(5u,DW_TAG_atomic_type,false,{{DW_AT_type,DW_FORM_ref4}});
 ab(6u,DW_TAG_base_type,false,{{DW_AT_name,DW_FORM_string},{DW_AT_encoding,DW_FORM_data1},{DW_AT_byte_size,DW_FORM_data1}});uleb(f.abbrev,0u);
 fixed(f.info,0u,4u);fixed(f.info,4u,2u);fixed(f.info,0u,4u);fixed(f.info,4u,1u);
 uleb(f.info,1u);string(f.info,"base.cpp");fixed(f.info,lang,2u);string(f.info,producer);
 uleb(f.info,2u);string(f.info,"function");fixed(f.info,10u,4u);fixed(f.info,10u,4u);
 uleb(f.info,3u);string(f.info,"left");auto var_ref{f.info.size()};fixed(f.info,0u,4u);uleb(f.info,0u);
 auto alias{f.info.size()};uleb(f.info,4u);string(f.info,alias_name);auto alias_ref{f.info.size()};fixed(f.info,0u,4u);
 if(atomic)
 { auto wrap{f.info.size()};uleb(f.info,5u);auto atom_ref{f.info.size()};fixed(f.info,0u,4u);patch(f.info,alias_ref,wrap);patch(f.info,atom_ref,f.info.size()); }
 else { patch(f.info,alias_ref,f.info.size()); }
 uleb(f.info,6u);string(f.info,base_name);fixed(f.info,encoding,1u);fixed(f.info,extent,1u);uleb(f.info,0u);
 patch(f.info,var_ref,alias);patch(f.info,0u,f.info.size()-4u);return f;
}
static void base_identity_tests()
{
 for(bool atomic:{false,true}) { for(auto lang:{0x1au,0x1du,0x3eu,0x16u})
 {
  auto f{base_metadata(atomic,"char","short",5u,2u,lang)};
  ::std::array<dwarf::section,2u> sections{{{".debug_info",f.info},{".debug_abbrev",f.abbrev}}};::std::unique_ptr<dwarf::index> index{};
  check(dwarf::index::parse({sections,100u,4u},index)==dwarf::error::none && index,"actual LLVM alias/atomic base parser");
  dwarf::variable_selection selected{};check(dwarf::query_named_variable(index->scopes(),index->types(),index->variables(),12u,"left",selected)==dwarf::inline_query_error::none && selected.type<index->types().size(),"alias type selected without location read");
  auto const& type{index->types()[selected.type]};check(type.name=="char" && type.named_type_alias && type.atomic_scalar==atomic,"alias display and hidden atomic chain recorded separately");
  check(type.narrow_builtin==(!atomic && lang==0x1au ? scalar::narrow_builtin::signed_short : scalar::narrow_builtin::unknown),"actual base name; alias cannot assert char rank");
  auto value{scalar::from_dwarf_numeric(dwarf::numeric_kind::signed_integer,1u,16u)};
  check(atomic ? !scalar::attach_narrow_type(type,value) && !value.declaration_identity_known :
    scalar::attach_narrow_type(type,value) && value.declaration_identity_known,"atomic typedef cannot attach numeric identity");
  if(!atomic) { check(scalar::copied_type_name(value)==(lang==0x1au ? "short" : "signed integer"),"canonical primitive projection, not alias guessing"); }
 } }
 auto f{base_metadata(false,"signed char","char",5u,1u,0x1au)};
 ::std::array<dwarf::section,2u> sections{{{".debug_info",f.info},{".debug_abbrev",f.abbrev}}};::std::unique_ptr<dwarf::index> index{};
 check(dwarf::index::parse({sections,100u,4u},index)==dwarf::error::none && index,"mismatched char encoding metadata parsed");
 dwarf::variable_selection v{};check(dwarf::query_named_variable(index->scopes(),index->types(),index->variables(),12u,"left",v)==dwarf::inline_query_error::none && index->types()[v.type].narrow_builtin==scalar::narrow_builtin::unknown,"wrong base encoding cannot prove char identity");
}

static void wide_base_identity_tests()
{
 using K=scalar::wide_builtin;
 for(bool atomic:{false,true}) { for(auto lang:{0x1au,0x1du,0x3eu,0x0cu,0x16u})
 { for(auto k:{K::signed_int,K::unsigned_int,K::signed_long,K::unsigned_long,K::signed_long_long,K::unsigned_long_long})
 {
  bool const uns{dwarf::c_wide_unsigned(k)};unsigned const bytes{dwarf::c_wide_rank(k)==5u?8u:4u};
  auto name{k==K::signed_int?::std::string_view{"int"}:k==K::unsigned_int?::std::string_view{"unsigned int"}:k==K::signed_long?::std::string_view{"long int"}:
    k==K::unsigned_long?::std::string_view{"long unsigned int"}:k==K::signed_long_long?::std::string_view{"long long int"} : ::std::string_view{"long long unsigned int"}};
  auto f{base_metadata(atomic,"int",name,uns?7u:5u,bytes,lang,lang==0x0cu?"TinyGo":"clang")};
  ::std::array<dwarf::section,2u> sections{{{".debug_info",f.info},{".debug_abbrev",f.abbrev}}};::std::unique_ptr<dwarf::index> index{};
  check(dwarf::index::parse({sections,100u,4u},index)==dwarf::error::none && index,"actual LLVM wide alias/atomic parser");
  dwarf::variable_selection selected{};check(dwarf::query_named_variable(index->scopes(),index->types(),index->variables(),12u,"left",selected)==dwarf::inline_query_error::none,"wide metadata selected without location evaluation");
  auto const& type{index->types()[selected.type]};bool const valid{!atomic && lang!=0x16u && lang!=0x0cu};
  check(type.name=="int" && type.wide_builtin==(valid?k:K::unknown),"alias display cannot override actual standard rank; TinyGo/Go excluded");
  auto value{scalar::from_dwarf_numeric(uns?dwarf::numeric_kind::unsigned_integer:dwarf::numeric_kind::signed_integer,1u,bytes*8u)};
  check(scalar::attach_standard_integer_type(type,value,32u)==valid && value.declaration_identity_known==valid,"production helper tracks atomic exclusion and native rank");
 } } }
 auto f{base_metadata(false,"long","long",5u,8u,0x1au)};::std::array<dwarf::section,2u> sections{{{".debug_info",f.info},{".debug_abbrev",f.abbrev}}};::std::unique_ptr<dwarf::index> index{};
 check(dwarf::index::parse({sections,100u,4u},index)==dwarf::error::none && index,"wrong guest long extent remains readable metadata");
 dwarf::variable_selection v{};check(dwarf::query_named_variable(index->scopes(),index->types(),index->variables(),12u,"left",v)==dwarf::inline_query_error::none && index->types()[v.type].wide_builtin==K::unknown,"long extent must match guest ABI");
}

static void synthetic()
{
    for(auto lang:{0x0cu,0x1du,0x1au,0x2au,0x3eu,0x16u,0x27u})
    { for(auto producer:{::std::string_view{"clang"},::std::string_view{"TinyGo"}})
    {
        auto f{metadata(lang,producer)};::std::array<dwarf::section,2u> sections{{{".debug_info",f.info},{".debug_abbrev",f.abbrev}}};
        ::std::unique_ptr<dwarf::index> index{};check(dwarf::index::parse({sections,100u,4u},index)==dwarf::error::none && index,"bounded actual LLVM parser");
        frames::language_context context{};check(frames::current_language(index->scopes(),12u,0u,context)==frames::error::none &&
            context.language==lang && context.tinygo_producer==(lang==0x0cu && producer=="TinyGo"),"actual CU language and producer parsed");
        if(context.tinygo_producer) { check(scalar::language_from_dwarf(context.language,true)==scalar::language_semantics::shared_numeric,"actual TinyGo CU adaptation"); }
        if(lang==0x3eu && !context.tinygo_producer)
        {
            auto mode{scalar::language_from_dwarf(context.language,false)};scalar::program code{};scalar::integer value{};
            auto resolve{[](dwarf::source_expression const&,bool,scalar::integer&) { return false; }};
            check(mode==scalar::language_semantics::c23 && scalar::parse("true",code)==scalar::error::none &&
                scalar::evaluate(code,resolve,value,32u,scalar::details::no_type_resolver{},mode)==scalar::error::none &&
                value.width==8u && value.category==scalar::value_category::boolean,"actual C23 CU predefined constant type");
        }
    } }
    for(bool cross:{false,true})
    {
        auto f{metadata(0x1du,"clang",false,cross)};::std::array<dwarf::section,2u> sections{{{".debug_info",f.info},{".debug_abbrev",f.abbrev}}};
        ::std::unique_ptr<dwarf::index> index{};check(dwarf::index::parse({sections,100u,4u},index)==dwarf::error::none,"cross-CU metadata structure accepted");
        frames::language_context context{};auto status{frames::current_language(index->scopes(),12u,0u,context)};
        check(cross ? status==frames::error::unavailable && context.language==0u : status==frames::error::none && context.language==0x1du,
            "different origin CU cannot choose selected frame language");
    }
    auto bad{metadata(1u,"clang",true)};::std::array<dwarf::section,2u> sections{{{".debug_info",bad.info},{".debug_abbrev",bad.abbrev}}};
    ::std::unique_ptr<dwarf::index> rejected{};check(dwarf::index::parse({sections,100u,4u},rejected)==dwarf::error::malformed && !rejected,"language flag is not a constant language ID");
}
int main(int argc,char** argv)
{
    synthetic();base_identity_tests();wide_base_identity_tests();::std::size_t canonical_proofs{},bridge_proofs{},C_exclusions{},wide_proofs{},sizeof_proofs{},unary_sizeof_proofs{},escape_proofs{},logical_proofs{};check(argc>=3 && argc%2==1,"path and native C/CPP semantic pairs");
    for(int i{1};i<argc;i+=2)
    {
        ::fast_io::native_file_loader file{::fast_io::mnp::os_c_str(argv[i]),::fast_io::open_mode::in};
        check(file.size()>=8u,"Wasm header size");auto bytes{::std::span<::std::byte const>{reinterpret_cast<::std::byte const*>(file.data()),file.size()}};
        ::std::array<unsigned char,8u> magic{0u,0x61u,0x73u,0x6du,1u,0u,0u,0u};for(::std::size_t p{};p<8u;++p) { check(::std::to_integer<unsigned char>(bytes[p])==magic[p],"Wasm header"); }
        dwarf::details::reader module{bytes,8u};::std::vector<dwarf::section> sections{};::std::uint64_t code_size{};
        while(module.cursor!=module.bytes.size())
        {
            ::std::uint8_t id{};::std::uint32_t length{};check(module.byte(id) && module.leb(length) && length<=module.bytes.size()-module.cursor,"section bounds");
            auto payload{module.bytes.subspan(module.cursor,length)};module.cursor+=length;
            if(id==10u) { check(code_size==0u,"one Code section");code_size=length; }if(id!=0u) { continue; }
            dwarf::details::reader custom{payload};::std::uint32_t name_length{};check(custom.leb(name_length) && name_length<=payload.size()-custom.cursor,"custom name bounds");
            ::std::string_view name{reinterpret_cast<char const*>(payload.data()+custom.cursor),name_length};custom.cursor+=name_length;
            if(name.starts_with(".debug_")) { sections.push_back({name,payload.subspan(custom.cursor)}); }
        }
        check(code_size!=0u,"fresh compiler Code");::std::unique_ptr<dwarf::index> parsed{};
        auto status{dwarf::index::parse({sections,code_size,4u},parsed)};
        if(status!=dwarf::error::none) { ::fast_io::io::perrln("index status=",static_cast<unsigned>(status)); }
        check(status==dwarf::error::none && parsed,"fresh embedded compiler DWARF");
        auto expected{::std::string_view{argv[i+1]}=="cpp" ? scalar::language_semantics::cpp : scalar::language_semantics::c};::std::size_t active{};
        for(auto const& scope:parsed->scopes())
        {
            if(scope.kind!=dwarf::scope_kind::subprogram || !scope.concrete || scope.ranges.empty()) { continue; }
            frames::language_context context{};
            if(frames::current_language(parsed->scopes(),scope.ranges[0].begin,0u,context)!=frames::error::none) { continue; }
            auto mode{scalar::language_from_dwarf(context.language,context.tinygo_producer)};
            check(expected==scalar::language_semantics::cpp ? mode==expected : mode==scalar::language_semantics::c || mode==scalar::language_semantics::c23,"fresh producer language selects semantic profile");
            ::fast_io::io::println("producer=",::fast_io::mnp::os_c_str(argv[i])," CU-language=",context.language," mode=",static_cast<unsigned>(mode));
            if(mode==scalar::language_semantics::cpp && scope.name=="conditional_narrow")
            {
                dwarf::variable_selection a{},b{};
                if(dwarf::query_named_variable(parsed->scopes(),parsed->types(),parsed->variables(),scope.ranges[0].begin,"a",a)==dwarf::inline_query_error::none &&
                   dwarf::query_named_variable(parsed->scopes(),parsed->types(),parsed->variables(),scope.ranges[0].begin,"b",b)==dwarf::inline_query_error::none)
                {
                    check(a.type<parsed->types().size() && b.type<parsed->types().size(),"real variable type indices");
                    auto const& x{parsed->types()[a.type]};auto const& y{parsed->types()[b.type]};
                    check(x.kind==dwarf::type_kind::scalar && y.kind==dwarf::type_kind::scalar && x.byte_count==1u && y.byte_count==1u &&
                        x.declaration_identity.offset!=0u && x.declaration_identity==y.declaration_identity,"real volatile C++ narrow canonical base DIE");
                    auto type{scalar::from_dwarf_numeric(dwarf::numeric_kind::signed_integer,0u,8u)};
                    type.declaration_identity=x.declaration_identity;type.declaration_identity_known=true;
                    ::std::size_t reads{};auto types{[&](dwarf::source_expression const& e,scalar::integer& out)
                    { if(e.root_name!="left" && e.root_name!="right") { return false; }out=type;return true; }};
                    auto values{[&](dwarf::source_expression const& e,bool size,scalar::integer& out)
                    { ++reads;if(size || e.root_name!="left") { return false; }out=type;out.bits=255u;return true; }};
                    scalar::program code{};scalar::integer out{};
                    check(scalar::parse("1 ? left : right",code)==scalar::error::none && scalar::evaluate(code,values,out,32u,types,mode)==scalar::error::none &&
                        out.width==8u && out.bits==255u && out.declaration_identity==x.declaration_identity && reads==1u,"real metadata canonical identity drives selected-copy narrow result");
                    ++canonical_proofs;
                }
            }

            if(scope.name.starts_with("bridge_"))
            {
                dwarf::variable_selection left{},right{};
                check(dwarf::query_named_variable(parsed->scopes(),parsed->types(),parsed->variables(),scope.ranges[0].begin,"a",left)==dwarf::inline_query_error::none &&
                    dwarf::query_named_variable(parsed->scopes(),parsed->types(),parsed->variables(),scope.ranges[0].begin,"b",right)==dwarf::inline_query_error::none &&
                    left.type<parsed->types().size() && right.type<parsed->types().size(),"real primitive producer variables");
                auto const& t{parsed->types()[left.type]};auto const& other{parsed->types()[right.type]};
                auto kind{scope.name=="bridge_char" ? scalar::narrow_builtin::plain_char : scope.name=="bridge_schar" ? scalar::narrow_builtin::signed_char :
                    scope.name=="bridge_uchar" ? scalar::narrow_builtin::unsigned_char : scope.name=="bridge_ushort" ? scalar::narrow_builtin::unsigned_short : scalar::narrow_builtin::signed_short};
                bool const uns{kind==scalar::narrow_builtin::unsigned_char || kind==scalar::narrow_builtin::unsigned_short};
                unsigned const width{kind==scalar::narrow_builtin::signed_short || kind==scalar::narrow_builtin::unsigned_short ? 16u : 8u};
                check(t.byte_count*8u==width && t.declaration_identity==other.declaration_identity && !t.atomic_scalar,"real narrow producer extent and canonical base");
                check(t.narrow_builtin==(mode==scalar::language_semantics::cpp ? kind : scalar::narrow_builtin::unknown),"actual C++ base identity with other-language exclusion");
                auto initial{scalar::from_dwarf_numeric(uns ? dwarf::numeric_kind::unsigned_integer : dwarf::numeric_kind::signed_integer,
                    kind==scalar::narrow_builtin::plain_char ? 97u : scalar::details::mask(width),width)};
                check(scalar::attach_narrow_type(t,initial),"real production helper attaches selected type");
                auto builtin_name{kind==scalar::narrow_builtin::plain_char ? ::std::string_view{"char"} : kind==scalar::narrow_builtin::signed_char ? ::std::string_view{"signed char"} :
                    kind==scalar::narrow_builtin::unsigned_char ? ::std::string_view{"unsigned char"} : kind==scalar::narrow_builtin::unsigned_short ? ::std::string_view{"unsigned short"} : ::std::string_view{"short"}};
                auto text{::fast_io::concat_std("1 ? left : (",builtin_name,")2")};scalar::program expression{};scalar::integer result{};::std::size_t reads{},queries{};
                auto type_query{[&](dwarf::source_expression const& e,scalar::integer& value)
                { ++queries;if(e.root_name!="left") { return false; }value=initial;value.bits=0u;return true; }};
                auto value_copy{[&](dwarf::source_expression const& e,bool size,scalar::integer& value)
                { ++reads;if(size || e.root_name!="left") { return false; }value=initial;return true; }};
                check(scalar::parse(text,expression)==scalar::error::none && scalar::evaluate(expression,value_copy,result,32u,type_query,mode)==scalar::error::none &&
                    reads==1u && queries==1u,"real metadata allows variable and builtin cast; selected copy only");
                if(mode==scalar::language_semantics::cpp)
                { check(result.width==width && result.bits==initial.bits && result.unsigned_value==uns && scalar::copied_type_name(result)==builtin_name && !result.declaration_identity_known,"real producer canonical primitive result name");++bridge_proofs; }
                else
                { check(result.width==32u && !result.unsigned_value && result.bits==((uns ? initial.bits : static_cast<::std::uint64_t>(scalar::details::signed_bits(initial)))&0xffffffffu),"real C metadata retains int promotion");++C_exclusions; }
                if(scope.name=="bridge_alias") { check(t.named_type_alias && t.name=="uwvm_short_alias","real named typedef retained separately from underlying short"); }
            }
            if(scope.name.starts_with("rank_"))
            {
                using K=scalar::wide_builtin;dwarf::variable_selection left{},right{};
                check(dwarf::query_named_variable(parsed->scopes(),parsed->types(),parsed->variables(),scope.ranges[0].begin,"a",left)==dwarf::inline_query_error::none &&
                    dwarf::query_named_variable(parsed->scopes(),parsed->types(),parsed->variables(),scope.ranges[0].begin,"b",right)==dwarf::inline_query_error::none,"real standard wide declarations");
                auto const& t{parsed->types()[left.type]};auto const& other{parsed->types()[right.type]};
                auto k{scope.name=="rank_int"?K::signed_int:scope.name=="rank_uint"?K::unsigned_int:scope.name=="rank_ulong"?K::unsigned_long:
                    scope.name=="rank_ll"?K::signed_long_long:scope.name=="rank_ull"?K::unsigned_long_long:K::signed_long};
                bool const uns{dwarf::c_wide_unsigned(k)};unsigned const width{dwarf::c_wide_rank(k)==5u?64u:32u};
                check(t.wide_builtin==k && t.byte_count*8u==width && t.declaration_identity==other.declaration_identity,"actual embedded standard rank and guest extent");
                auto initial{scalar::from_dwarf_numeric(uns?dwarf::numeric_kind::unsigned_integer:dwarf::numeric_kind::signed_integer,1u,width)};
                check(scalar::attach_standard_integer_type(t,initial,32u),"real selected standard integer helper");
                auto builtin_name{scalar::copied_type_name(initial)};auto text{::fast_io::concat_std("1 ? left : (",builtin_name,")2")};scalar::program expression{};scalar::integer result{};::std::size_t reads{},queries{};
                auto type_query{[&](dwarf::source_expression const& e,scalar::integer& value) { ++queries;if(e.root_name!="left") { return false; }value=initial;value.bits=0u;return true; }};
                auto value_copy{[&](dwarf::source_expression const& e,bool z,scalar::integer& value) { ++reads;if(z || e.root_name!="left") { return false; }value=initial;return true; }};
                check(scalar::parse(text,expression)==scalar::error::none && scalar::evaluate(expression,value_copy,result,32u,type_query,mode)==scalar::error::none &&
                    result.wide_identity==k && scalar::copied_type_name(result)==builtin_name && result.width==width && result.bits==1u && reads==1u && queries==1u && !result.declaration_identity_known,"real metadata variable/cast standard rank and copied result name");
                if(scope.name=="rank_alias") { check(t.named_type_alias && t.name=="uwvm_long_alias","real long alias display remains separate from rank proof"); }
                ++wide_proofs;
                for(auto text : {::std::string_view{"0 && left"},::std::string_view{"1 || left"},::std::string_view{"left && 1"},::std::string_view{"left || 0"}})
                {
                    reads=queries=0u;
                    check(scalar::parse(text,expression)==scalar::error::none && scalar::evaluate(expression,value_copy,result,32u,type_query,mode)==scalar::error::none &&
                        result.bits==(text=="0 && left"?0u:1u) && reads==(text.starts_with("left")?1u:0u) && queries==1u &&
                        result.width==(mode==scalar::language_semantics::cpp?8u:32u),"real declaration checked even when value copy is skipped");
                    ++logical_proofs;
                }
                for(auto text : {::std::string_view{"0 && (left%1.0)"},::std::string_view{"1 || (left<<1.0)"},::std::string_view{"0 && (left|1.0)"},::std::string_view{"1 || ~(left+1.0)"}})
                {
                    reads=queries=0u;
                    check(scalar::parse(text,expression)==scalar::error::none && scalar::evaluate(expression,value_copy,result,32u,type_query,mode)==scalar::error::unsupported &&
                        reads==0u && queries==1u && result.bits==0u,"real declaration rejects invalid dead operator before any value copy");
                    ++logical_proofs;
                }
                for(auto operand : {::std::string_view{"left+left"},::std::string_view{"left/0"},::std::string_view{"left<<-1"}})
                {
                    auto size_text{::fast_io::concat_std("sizeof(",operand,")")};queries=0u;reads=0u;
                    check(scalar::parse(size_text,expression)==scalar::error::none && scalar::evaluate(expression,value_copy,result,32u,type_query,mode)==scalar::error::none &&
                        result.bits==width/8u && result.width==32u && result.unsigned_value && !result.floating &&
                        result.wide_identity==K::unsigned_long && !result.declaration_identity_known && reads==0u && queries==(operand=="left+left"?2u:1u),"real metadata sizeof expression uses declarations without value/arithmetic evaluation");
                    ++sizeof_proofs;
                }
                for(auto operand : {::std::string_view{"+left"},::std::string_view{"-left"},::std::string_view{"sizeof +left"}})
                {
                    auto size_text{::fast_io::concat_std("sizeof ",operand)};queries=0u;reads=0u;
                    check(scalar::parse(size_text,expression)==scalar::error::none && scalar::evaluate(expression,value_copy,result,32u,type_query,mode)==scalar::error::none &&
                        result.bits==(operand=="sizeof +left"?4u:width/8u) && result.width==32u && result.unsigned_value && !result.floating &&
                        result.wide_identity==K::unsigned_long && !result.declaration_identity_known && reads==0u && queries==1u,"real metadata unary sizeof owns one type-only operand");
                    ++unary_sizeof_proofs;
                }
                for(auto token : {::std::string_view{"'\\x41'"},::std::string_view{"'\\101'"},::std::string_view{"'\\a'"}})
                {
                    auto size_text{::fast_io::concat_std("sizeof(",token," + left)")};queries=0u;reads=0u;
                    check(scalar::parse(size_text,expression)==scalar::error::none && scalar::evaluate(expression,value_copy,result,32u,type_query,mode)==scalar::error::none &&
                        result.bits==width/8u && result.width==32u && result.unsigned_value && reads==0u && queries==1u,"real metadata escaped character participates in unevaluated integer promotion");
                    ++escape_proofs;
                }
            }
            scalar::program code{};scalar::integer value{};auto resolver{[](dwarf::source_expression const&,bool,scalar::integer&) { return false; }};
            check(scalar::parse("1 ? (bool)1 : (bool)0",code)==scalar::error::none,"producer profile expression syntax");
            check(scalar::evaluate(code,resolver,value,32u,scalar::details::no_type_resolver{},mode)==scalar::error::none && value.bits==1u &&
                value.width==(expected==scalar::language_semantics::cpp ? 8u : 32u),"actual parsed CU drives native conditional result width");++active;
        }
        check(active!=0u,"at least one real active subprogram classified");
        if(expected==scalar::language_semantics::cpp) { check(canonical_proofs!=0u,"real C++ metadata identity must be exercised"); }
    }
    ::fast_io::io::println("debug_source_language_index: PASS checks=",checks," producer_modules=",(argc-1)/2," canonical_CPP_proofs=",canonical_proofs," primitive_CPP_bridge_proofs=",bridge_proofs," C_base_exclusions=",C_exclusions," standard_wide_proofs=",wide_proofs," unevaluated_sizeof_proofs=",sizeof_proofs," unary_sizeof_proofs=",unary_sizeof_proofs," character_escape_proofs=",escape_proofs," logical_preflight_proofs=",logical_proofs," no live stop qualification");
}
