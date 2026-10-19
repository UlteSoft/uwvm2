// Actual LLVM decodes owned DWARF; this tests metadata, not a live VM stop.
#include <uwvm2/uwvm/debugger/source_dwarf_index.h>
#include <cstdio>
#include <cstdlib>
namespace dwarf=::uwvm2::uwvm::debugger::source_dwarf;
namespace dw=::llvm::dwarf;
static void check(bool good,char const* reason)
{ if(!good) { ::std::fprintf(stderr,"pretty metadata FAIL: %s\n",reason); ::std::exit(1); } }
static void number(::std::string& bytes,::std::uint64_t n,unsigned width)
{ for(unsigned i{};i!=width;++i) { bytes+=static_cast<char>(n>>(8u*i)); } }
static void uleb(::std::string& bytes,::std::uint64_t n)
{ do { auto c=static_cast<unsigned char>(n&127u);n>>=7u;bytes+=static_cast<char>(c|(n!=0u?128u:0u)); } while(n!=0u); }
static void string(::std::string& bytes,char const* text)
{ bytes+=text;bytes+='\0'; }
static void run(bool specification,bool instance)
{
    ::std::string abbreviations{},body{};
    auto abbreviation{[&](unsigned code,dw::Tag tag,bool children,
        ::std::initializer_list<::std::pair<dw::Attribute,dw::Form>> attributes)
    {
        uleb(abbreviations,code);uleb(abbreviations,tag);abbreviations+=children?'\1':'\0';
        for(auto [attribute,form]:attributes) {uleb(abbreviations,attribute);uleb(abbreviations,form);}
        uleb(abbreviations,0u);uleb(abbreviations,0u);
    }};
    abbreviation(1u,dw::DW_TAG_compile_unit,true,{{dw::DW_AT_name,dw::DW_FORM_string},{dw::DW_AT_language,dw::DW_FORM_data2}});
    abbreviation(2u,dw::DW_TAG_base_type,false,{{dw::DW_AT_name,dw::DW_FORM_string},{dw::DW_AT_encoding,dw::DW_FORM_data1},{dw::DW_AT_byte_size,dw::DW_FORM_data1}});
    abbreviation(3u,dw::DW_TAG_structure_type,true,{{dw::DW_AT_name,dw::DW_FORM_string},{dw::DW_AT_byte_size,dw::DW_FORM_data1}});
    if(specification)
    { abbreviation(4u,dw::DW_TAG_member,false,{{dw::DW_AT_name,dw::DW_FORM_string},{dw::DW_AT_type,dw::DW_FORM_ref4},{dw::DW_AT_declaration,dw::DW_FORM_flag}}); }
    else if(instance)
    { abbreviation(4u,dw::DW_TAG_member,false,{{dw::DW_AT_name,dw::DW_FORM_string},{dw::DW_AT_type,dw::DW_FORM_ref4},{dw::DW_AT_const_value,dw::DW_FORM_udata},{dw::DW_AT_data_member_location,dw::DW_FORM_data1}}); }
    else
    { abbreviation(4u,dw::DW_TAG_member,false,{{dw::DW_AT_name,dw::DW_FORM_string},{dw::DW_AT_type,dw::DW_FORM_ref4},{dw::DW_AT_const_value,dw::DW_FORM_udata}}); }
    abbreviation(5u,dw::DW_TAG_variable,false,{{dw::DW_AT_specification,dw::DW_FORM_ref4},{dw::DW_AT_const_value,dw::DW_FORM_udata}});
    abbreviation(6u,dw::DW_TAG_member,false,{{dw::DW_AT_name,dw::DW_FORM_string},{dw::DW_AT_type,dw::DW_FORM_ref4},{dw::DW_AT_data_member_location,dw::DW_FORM_data1}});
    abbreviation(7u,dw::DW_TAG_template_value_parameter,false,{{dw::DW_AT_name,dw::DW_FORM_string},{dw::DW_AT_type,dw::DW_FORM_ref4},{dw::DW_AT_const_value,dw::DW_FORM_flag}});
    abbreviation(8u,dw::DW_TAG_typedef,false,{{dw::DW_AT_name,dw::DW_FORM_string},{dw::DW_AT_type,dw::DW_FORM_ref4}});
    abbreviation(9u,dw::DW_TAG_variable,false,{{dw::DW_AT_name,dw::DW_FORM_string},{dw::DW_AT_type,dw::DW_FORM_ref4}});
    uleb(abbreviations,0u);
    number(body,4u,2u);number(body,0u,4u);number(body,4u,1u);
    uleb(body,1u);string(body,"pretty-metadata.cc");number(body,dw::DW_LANG_C_plus_plus_14,2u);
    auto const integer_offset=body.size()+4u;
    uleb(body,2u);string(body,"long");number(body,dw::DW_ATE_signed,1u);number(body,4u,1u);
    auto const deque_offset=body.size()+4u;
    uleb(body,3u);string(body,"deque<int>");number(body,12u,1u);
    auto const member_offset=body.size()+4u;
    uleb(body,4u);string(body,"__block_size");number(body,integer_offset,4u);
    if(specification) {number(body,1u,1u);} else {uleb(body,1024u);if(instance) {number(body,0u,1u);}}
    // DW_FORM_flag is not a numeric constant even though LLVM exposes its
    // unsigned flag value through getAsUnsignedConstant().
    uleb(body,7u);string(body,"invalid_numeric_form");number(body,integer_offset,4u);number(body,1u,1u);
    uleb(body,0u);
    // No variable references this complete aggregate: the bounded type pass
    // must still retain its exact DIE so linked-container nodes are available.
    uleb(body,3u);string(body,"orphan_node");number(body,4u,1u);
    uleb(body,6u);string(body,"value");number(body,integer_offset,4u);number(body,0u,1u);uleb(body,0u);
    auto const alias_offset=body.size()+4u;
    uleb(body,8u);string(body,"renamed_deque");number(body,deque_offset,4u);
    uleb(body,9u);string(body,"alias_object");number(body,alias_offset,4u);
    if(specification) {uleb(body,5u);number(body,member_offset,4u);uleb(body,1024u);}
    uleb(body,0u);
    ::std::string info{};number(info,body.size(),4u);info+=body;
    ::std::vector<dwarf::section> sections{{".debug_info",::std::as_bytes(::std::span{info.data(),info.size()})},
        {".debug_abbrev",::std::as_bytes(::std::span{abbreviations.data(),abbreviations.size()})}};
    ::std::unique_ptr<dwarf::index> index{};
    check(dwarf::index::parse({sections,100u,4u},index)==dwarf::error::none && index,"LLVM parses bounded actual attribute tables");
    bool deque{},orphan{},alias{};
    for(auto const& type:index->types())
    {
        if(type.name=="deque<int>")
        {
            deque=true;
            check(type.static_constants.size()==(instance?0u:1u),"instance constants cannot establish a static ABI value");
            if(!instance) {auto const& value=type.static_constants[0u];check(value.name=="__block_size" && value.available && value.bits==1024u,"constant belongs to exact class owner, including specification definition");}
            check(type.template_values.size()==1u && !type.template_values[0u].available,"non-numeric flag remains unavailable");
        }
        if(type.name=="orphan_node") {orphan=true;check(type.members.size()==1u,"unreferenced aggregate definition is retained");}
        if(type.name=="renamed_deque") {alias=true;check(type.named_type_alias && type.aggregate_name=="deque<int>" && type.declaration_identity.offset==deque_offset,
            "display alias keeps exact underlying aggregate name and DIE identity");}
    }
    check(deque && orphan && alias,"aggregate and alias identities retained");
}
int main()
{ run(false,false);run(true,false);run(false,true);::std::puts("PASS actual LLVM pretty metadata component"); }
