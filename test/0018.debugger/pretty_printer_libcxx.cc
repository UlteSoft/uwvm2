// Explicit libc++ bit-field layouts as owned DWARF/copied DATA. Qualification
// must run in the authorized SSH Linux 64 GiB cgroup, including BE hosts under
// QEMU: these bytes always model Wasm little-endian memory, not host objects.
#include <uwvm2/uwvm/debugger/source_language_expression.h>
#include <cstdio>
#include <cstdlib>

namespace dwarf = ::uwvm2::uwvm::debugger::source_dwarf;
namespace pretty = ::uwvm2::uwvm::debugger::source_pretty_printer;
namespace expression = ::uwvm2::uwvm::debugger::source_language_expression;
namespace
{
    void check(bool value,char const* message)
    { if(!value) { ::std::fprintf(stderr,"pretty_printer_libcxx FAIL: %s\n",message); ::std::exit(1); } }
    dwarf::type_record scalar(char const* name,unsigned bytes,unsigned encoding=7u)
    {
        dwarf::type_record out{};out.name=name;out.kind=dwarf::type_kind::scalar;out.language=0x21u;out.encoding=encoding;
        out.byte_size=bytes;out.byte_count=static_cast<::std::uint8_t>(bytes);out.size_known=true;return out;
    }
    dwarf::type_record object(char const* name,unsigned size,::std::vector<dwarf::member_record> members={},bool is_union=false)
    {
        dwarf::type_record out{};out.name=name;out.kind=is_union ? dwarf::type_kind::union_type : dwarf::type_kind::structure;
        out.language=0x21u;out.byte_size=size;out.size_known=true;out.members=::std::move(members);return out;
    }
    dwarf::member_record field(char const* name,::std::size_t type,unsigned offset)
    { dwarf::member_record out{};out.name=name;out.type=type;out.byte_offset=offset;out.offset_known=true;return out; }
    dwarf::member_record bit(char const* name,::std::size_t type,unsigned offset,unsigned width)
    { auto out{field(name,type,offset/8u)};out.bit_field=true;out.data_bit_offset=offset;out.bit_size=width;return out; }
    void put(::std::vector<::std::byte>& bytes,unsigned offset,unsigned width,::std::uint64_t value)
    {
        check(offset<=bytes.size() && width<=bytes.size()-offset && width<=8u,"bounded fixture scalar encoding");
        for(unsigned i{};i!=width;++i) { bytes[offset+i]=static_cast<::std::byte>((value>>(i*8u))&255u); }
    }
    void put_bit(::std::vector<::std::byte>& bytes,unsigned offset,unsigned width,::std::uint64_t value)
    {
        check(width<=64u && offset/8u<=bytes.size() && (offset%8u+width+7u)/8u<=bytes.size()-offset/8u,"bounded fixture bit encoding");
        for(unsigned i{};i!=width;++i)
        {
            auto const byte{(offset+i)/8u};auto const mask{1u<<((offset+i)%8u)};auto current{::std::to_integer<unsigned char>(bytes[byte])};
            current=static_cast<unsigned char>((current&~mask)|(((value>>i)&1u)!=0u ? mask : 0u));bytes[byte]=static_cast<::std::byte>(current);
        }
    }
    struct fixture
    {
        unsigned width{},unit{},root{},data_offset{},control_offset{},first_carrier{};bool alternate{};
        ::std::vector<dwarf::type_record> types{};
        ::std::vector<::std::byte> bytes{},known{},qualified{},memory{};
        ::std::vector<dwarf::object_node> tree{};unsigned calls{};bool selected_offset{};
        fixture(unsigned w,unsigned u,bool alt,unsigned wrapper,bool representation_wrapper=false,bool character_alias=false):width(w),unit(u),alternate(alt),memory(4096u)
        {
            types={scalar(u==1u ? "char" : u==2u ? "char16_t" : "char32_t",u,u==1u ? 6u : 0x10u),scalar("size_t",w),scalar("unsigned char",1u,8u)};
            auto pointer{object("",w)};pointer.kind=dwarf::type_kind::pointer;pointer.byte_count=w;pointer.referenced_type=0u;types.push_back(::std::move(pointer));
            auto array{object("",((w*3u-1u)/u)*u)};array.kind=dwarf::type_kind::array;array.referenced_type=0u;
            array.dimensions={{0,(w*3u-1u)/u,true,true}};types.push_back(::std::move(array));
            // Match modern libc++'s anonymous packed wrappers in default CSD.
            auto long_bits{object("",w,{bit("__is_long_",1u,0u,1u),bit("__cap_",1u,1u,w*8u-1u)})};types.push_back(::std::move(long_bits));
            auto short_bits{object("",1u,{bit("__is_long_",2u,0u,1u),bit("__size_",2u,1u,7u)})};types.push_back(::std::move(short_bits));
            auto long_type{object("__long",w*3u,alt ?
                ::std::vector<dwarf::member_record>{field("__data_",3u,0u),field("__size_",1u,w),bit("__cap_",1u,w*16u,w*8u-1u),bit("__is_long_",1u,w*24u-1u,1u)} :
                ::std::vector<dwarf::member_record>{field("",5u,0u),field("__size_",1u,w),field("__data_",3u,w*2u)})};types.push_back(::std::move(long_type));
            auto short_type{object("__short",w*3u,alt ?
                ::std::vector<dwarf::member_record>{field("__data_",4u,0u),bit("__size_",2u,(w*3u-1u)*8u,7u),bit("__is_long_",2u,w*24u-1u,1u)} :
                ::std::vector<dwarf::member_record>{field("",6u,0u),field("__data_",4u,u)})};types.push_back(::std::move(short_type));
            // Inactive long.__size_ precedes the short node deliberately. A
            // global name search used to select it and misprint short "a".
            types.push_back(object("__rep",w*3u,{field("__l",7u,0u),field("__s",8u,0u)},true));
            unsigned rep{9u};char const* name{"__rep_"};
            if(wrapper==3u)
            {
                auto const ordinal{types.size()};types.push_back(scalar("int",4u,5u));
                auto const allocator{types.size()};types.push_back(object("stateful_allocator",4u,{field("state",ordinal,0u)}));
                first_carrier=types.size();auto first{object("__compressed_pair_elem<rep, 0, false>",w*3u,{field("__value_",9u,0u)})};
                first.template_types={9u};first.template_values={{1u,ordinal,0u,true,true}};types.push_back(::std::move(first));
                auto const second_index{types.size()};auto second{object("__compressed_pair_elem<allocator, 1, false>",4u,{field("__value_",allocator,0u)})};
                second.template_types={allocator};second.template_values={{1u,ordinal,1u,true,true}};types.push_back(::std::move(second));
                auto left{field("",first_carrier,0u)};left.inherited=true;auto right{field("",second_index,w*3u)};right.inherited=true;
                rep=types.size();auto pair{object("__compressed_pair<rep, allocator>",w*4u,{left,right})};
                pair.template_types={9u,allocator};types.push_back(::std::move(pair));name="__r_";
            }
            else if(wrapper!=0u) { types.push_back(object("__compressed_pair",w*3u,{field("__value_",9u,0u)}));rep=10u;name=wrapper==1u ? "__r_" : "__rep_"; }
            types.push_back(object("std::__1::basic_string<char>",wrapper==3u ? w*4u : w*3u,{field(name,rep,0u)}));root=types.size()-1u;
            if(representation_wrapper)
            {
                auto actual_union{types[9u]};actual_union.name="";
                auto const inner{types.size()};types.push_back(::std::move(actual_union));
                types[9u].kind=dwarf::type_kind::structure;types[9u].members={field("",inner,0u)};
            }
            if(character_alias)
            {
                types[0u].identity=types[0u].declaration_identity={7u,10u};
                auto alias{types[0u]};alias.name="value_type";alias.named_type_alias=true;alias.identity={7u,20u};
                types[4u].referenced_type=types.size();types.push_back(::std::move(alias));
            }
            bytes.resize(types[root].byte_size);known.assign(bytes.size(),::std::byte{255u});qualified=known;
            data_offset=alt ? 0u : u;control_offset=alt ? w*3u-1u : 0u;
        }
        void short_string(unsigned count=1u)
        {
            bytes.assign(bytes.size(),::std::byte{});put_bit(bytes,control_offset*8u+(alternate ? 0u : 1u),7u,count);
            for(unsigned i{};i!=count && i<types[4u].dimensions[0u].count;++i) { put(bytes,data_offset+i*unit,unit,65u+i); }
        }
        void long_string()
        {
            bytes.assign(bytes.size(),::std::byte{});
            put_bit(bytes,alternate ? width*16u : 1u,width*8u-1u,alternate ? 32u : 16u);
            put_bit(bytes,control_offset*8u+(alternate ? 7u : 0u),1u,1u);
            put(bytes,width,width,3u);put(bytes,alternate ? 0u : width*2u,width,256u);
            for(unsigned i{};i!=3u;++i) { put(memory,256u+i*unit,unit,65u+i); }
        }
        dwarf::inline_query_error select(::std::int64_t index,bool type_only,::std::vector<dwarf::object_node>& out,
            expression::limits const& cap={},bool nested=false)
        {
            calls=0u;::std::vector<dwarf::source_expression_step> steps{};
            auto selected_root{root};auto selected_bytes{bytes};auto selected_known{known};auto selected_qualified{qualified};
            if(nested)
            {
                selected_root=types.size();types.push_back(object("holder",bytes.size()+16u,{field("text",root,16u)}));
                selected_bytes.insert(selected_bytes.begin(),16u,::std::byte{});
                selected_known.insert(selected_known.begin(),16u,::std::byte{255u});
                selected_qualified.insert(selected_qualified.begin(),16u,::std::byte{});
                dwarf::source_expression_step member{};member.kind=dwarf::source_expression_step_kind::member;member.member="text";steps.push_back(::std::move(member));
            }
            dwarf::source_expression_step item{};item.kind=dwarf::source_expression_step_kind::index;item.index=index;steps.push_back(item);
            auto read{[&](::std::uint64_t offset,::std::size_t count,::std::vector<::std::byte>& result)
            { ++calls;if(offset>memory.size() || count>memory.size()-offset) { return false; }
              result.assign(memory.begin()+static_cast<::std::size_t>(offset),memory.begin()+static_cast<::std::size_t>(offset)+count);return true; }};
            auto const status{type_only ? expression::type(types,selected_root,steps,width,out) :
                expression::value(types,selected_root,steps,::std::move(selected_bytes),::std::move(selected_known),width,read,out,cap,selected_qualified)};
            if(nested) { types.pop_back(); }return status;
        }
        pretty::outcome format(pretty::limits const& cap={})
        {
            calls=0u;check(dwarf::query_object_value_with_known_bits(types,root,bytes,known,tree)==dwarf::inline_query_error::none,"raw copied object query");
            if(selected_offset) { for(auto& node:tree) { node.byte_offset+=16u; } }
            auto read{[&](::std::uint64_t offset,::std::size_t count,::std::vector<::std::byte>& out)
            { ++calls;if(offset>memory.size() || count>memory.size()-offset) { return false; }
              out.assign(memory.begin()+static_cast<::std::size_t>(offset),memory.begin()+static_cast<::std::size_t>(offset)+count);return true; }};
            return pretty::format(types,root,bytes,known,qualified,width,read,tree,cap);
        }
    };
}
int main()
{
    unsigned cases{};
    for(unsigned width:{4u,8u}) for(unsigned unit:{1u,2u,4u}) for(bool alternate:{false,true}) for(unsigned wrapper:{0u,1u,2u,3u}) for(bool representation_wrapper:{false,true}) for(bool character_alias:{false,true})
    {
        fixture f{width,unit,alternate,wrapper,representation_wrapper,character_alias};f.short_string();
        if(wrapper==3u)
        { pretty::details::field ambiguous{};check(!pretty::details::path(f.types,f.root,"__r_.__value_",ambiguous),"generic stateful pair lookup stays ambiguous"); }
        check(f.format()==pretty::outcome::formatted && f.calls==0u && f.tree[0u].pretty_summary.find("string length=1")!=::std::string::npos,"exact SSO branch and code-unit count");
        if(unit==1u) { check(f.tree[0u].display_text=="A","SSO does not select inactive long length zero"); }
        else { check(f.tree[0u].pretty_summary.find(unit==2u ? "UTF16 code units: 65" : "UTF32 code units: 65")!=::std::string::npos,"wide SSO uses code units without UTF guesses"); }
        if(character_alias)
        {
            auto const alias_index{f.types[4u].referenced_type};auto const accepted{f.types[alias_index]};
            ::std::vector<dwarf::object_node> valid_layout{};
            check(dwarf::query_type_layout(f.types,f.root,valid_layout)==dwarf::inline_query_error::none,"accepted alias raw layout");
            auto reject{[&](char const* reason)
            {
                pretty::details::libcxx_string_layout rejected{};
                check(!pretty::details::libcxx_string_schema(f.types,f.root,valid_layout,width,rejected),reason);
                ::std::vector<dwarf::object_node> raw{};f.calls=0u;
                auto const status{dwarf::query_object_value_with_known_bits(f.types,f.root,f.bytes,f.known,raw)};
                if(status==dwarf::inline_query_error::none)
                {
                    auto no_read{[&](::std::uint64_t,::std::size_t,::std::vector<::std::byte>&)
                    { ++f.calls;return false; }};
                    check(pretty::format(f.types,f.root,f.bytes,f.known,f.qualified,width,no_read,raw)!=pretty::outcome::formatted,reason);
                }
                check(f.calls==0u,"invalid alias rejected before any guest reader call");f.types[alias_index]=accepted;
            }};
            f.types[alias_index].declaration_identity={7u,11u};reject("equal spelling/extent cannot replace declaration identity");
            f.types[alias_index].declaration_identity={8u,10u};reject("same DIE offset from another CU cannot bind character alias");
            f.types[alias_index].declaration_identity={};reject("missing character declaration identity is not a wildcard");
            f.types[alias_index].encoding=9u;reject("inconsistent alias encoding rejected before reads");
            f.types[alias_index].byte_size+=1u;reject("inconsistent alias size rejected before reads");
            f.types[alias_index].atomic_scalar=true;reject("atomic character alias cannot select ordinary string payload");
            f.types[alias_index].language=0x1cu;reject("non-C++ alias cannot select C++ string payload");
            f.types[alias_index].kind=dwarf::type_kind::enumeration;reject("same declaration key with inconsistent kind rejected");
            check(f.format()==pretty::outcome::formatted && f.calls==0u,"validated character alias restored");
        }
        pretty::limits summary_only{};summary_only.max_elements=0u;
        check(f.format(summary_only)==pretty::outcome::formatted && f.calls==0u && f.tree[0u].pretty_truncated &&
              f.tree[0u].pretty_omitted==1u && (unit!=1u || f.tree[0u].display_text.empty()),"SSO shares the same zero-element summary budget as long strings");
        f.selected_offset=true;
        check(f.format()==pretty::outcome::formatted,"selected object with nonzero outer raw offset");f.selected_offset=false;
        f.known[f.data_offset]=::std::byte{};
        check(f.format()!=pretty::outcome::formatted && !f.tree[0u].pretty_available && f.calls==0u,"unknown SSO payload never becomes copied display text");
        f.known.assign(f.known.size(),::std::byte{255u});f.short_string(0u);f.known.assign(f.known.size(),::std::byte{});f.known[f.control_offset]=::std::byte{255u};
        check(f.format()==pretty::outcome::formatted && f.calls==0u,"empty SSO does not require unknown unused payload");
        f.known.assign(f.known.size(),::std::byte{255u});f.short_string(static_cast<unsigned>(f.types[4u].dimensions[0u].count));
        check(f.format()!=pretty::outcome::formatted && f.calls==0u,"SSO length leaves room for terminator");
        f.long_string();check(f.format()==pretty::outcome::formatted && f.calls==1u && f.tree[0u].pretty_printer=="libc++ long basic_string","exact long branch and capacity decode");
        if(unit==1u) { check(f.tree[0u].display_text=="ABC","long payload copied once"); }
        f.qualified.assign(f.qualified.size(),::std::byte{});
        check(f.format()!=pretty::outcome::formatted && f.calls==0u,"long pointer requires eligible carrier bits");f.qualified=f.known;
        put(f.bytes,width,width,32u);
        check(f.format()!=pretty::outcome::formatted && f.calls==0u,"long size cannot consume terminator capacity");
        f.long_string();f.known[f.control_offset]=::std::byte{};
        check(f.format()!=pretty::outcome::formatted && f.calls==0u,"unknown exact mode cannot select either union branch");
        f.known.assign(f.known.size(),::std::byte{255u});f.short_string();
        auto const saved{f.types[8u].members};f.types[8u].members.push_back(f.types[8u].members.back());
        check(f.format()!=pretty::outcome::formatted && f.calls==0u,"duplicate exact branch field rejected");f.types[8u].members=saved;
        // Type and value queries use the renderer's SAME exact union schema.
        ::std::vector<dwarf::object_node> indexed{};expression::limits no_reads{};no_reads.maximum_reads=0u;
        f.short_string();f.known.assign(f.known.size(),::std::byte{});f.qualified=f.known;
        check(f.select(0,true,indexed)==dwarf::inline_query_error::none && indexed.size()==1u && indexed[0u].type==f.types[4u].referenced_type &&
            indexed[0u].byte_size==unit && f.calls==0u,"type query needs no mode/payload/pointer values");
        auto const selected_unit{unit==1u ? 65u : unit==2u ? 0xd834u : 0x80000041u};
        put(f.bytes,f.data_offset,unit,selected_unit);
        f.known[f.control_offset]=::std::byte{255u};
        for(unsigned i{};i!=unit;++i) { f.known[f.data_offset+i]=::std::byte{255u}; }
        check(f.select(0,false,indexed,no_reads)==dwarf::inline_query_error::none && indexed.size()==1u &&
            indexed[0u].value_available && indexed[0u].bits==selected_unit && f.calls==0u,"SSO selected code unit uses copied bytes only; inactive/unrelated bytes unknown");
        check(f.select(0,false,indexed,no_reads,true)==dwarf::inline_query_error::none && indexed[0u].bits==selected_unit &&
            f.calls==0u,"nested SSO uses container-relative copied offset, never guest address zero");
        f.known[f.data_offset]=::std::byte{};
        check(f.select(0,false,indexed)==dwarf::inline_query_error::unavailable && f.calls==0u,"SSO selected unknown code unit rejected before any read");
        f.known.assign(f.known.size(),::std::byte{255u});
        check(f.select(1,false,indexed)==dwarf::inline_query_error::unavailable && f.calls==0u,"SSO index at length rejected without reads");
        check(f.select(-1,false,indexed)==dwarf::inline_query_error::unavailable && f.calls==0u,"SSO negative index rejected without reads");
        f.known[f.control_offset]=::std::byte{};
        check(f.select(0,false,indexed)==dwarf::inline_query_error::unavailable && f.calls==0u,"unknown mode cannot select SSO or long storage");
        f.known.assign(f.known.size(),::std::byte{255u});f.qualified=f.known;f.long_string();
        auto const long_unit{unit==1u ? 67u : unit==2u ? 0xdc00u : 0xf0000043u};put(f.memory,256u+2u*unit,unit,long_unit);
        check(f.select(2,false,indexed)==dwarf::inline_query_error::none && indexed.size()==1u && indexed[0u].bits==long_unit &&
            indexed[0u].byte_size==unit && f.calls==1u,"long selected code unit copies one genuine reader extent");
        check(f.select(2,false,indexed,{},true)==dwarf::inline_query_error::none && indexed[0u].bits==long_unit &&
            f.calls==1u,"nested long field uses real guest pointer under canonical reader");
        check(f.select(2,false,indexed,no_reads)==dwarf::inline_query_error::limit_exceeded && f.calls==0u,"long index obeys shared read allowance");
        check(f.select(3,false,indexed)==dwarf::inline_query_error::unavailable && f.calls==0u,"long index at length rejected without reads");
        auto const pointer_offset{alternate ? 0u : width*2u};f.qualified[pointer_offset]=::std::byte{254u};
        check(f.select(0,false,indexed)==dwarf::inline_query_error::unavailable && f.calls==0u,"long index needs every eligible pointer bit");f.qualified=f.known;
        f.known[width]=::std::byte{};
        check(f.select(0,false,indexed)==dwarf::inline_query_error::unavailable && f.calls==0u,"long index needs exact available length");f.known=f.qualified;
        put(f.bytes,width,width,32u);
        check(f.select(0,false,indexed)==dwarf::inline_query_error::unavailable && f.calls==0u,"long index cannot bypass decoded capacity proof");
        f.long_string();put(f.bytes,pointer_offset,width,width==4u ? 0xffffffffull : ~::std::uint64_t{});
        check(f.select(0,false,indexed)==dwarf::inline_query_error::unavailable && f.calls==0u,"whole declared long range checked before selected copy");
        f.long_string();f.types[8u].members.push_back(f.types[8u].members.back());
        check(f.select(0,true,indexed)!=dwarf::inline_query_error::none && f.calls==0u &&
              f.select(0,false,indexed)!=dwarf::inline_query_error::none && f.calls==0u,"duplicate union field rejects both type and value indexing");f.types[8u].members=saved;
        if(wrapper==3u)
        {
            f.types[f.first_carrier].template_values[0u].bits=1u;
            check(f.select(0,true,indexed)!=dwarf::inline_query_error::none && f.calls==0u &&
                  f.select(0,false,indexed)!=dwarf::inline_query_error::none && f.calls==0u,"stateful string indexing cannot bypass ordinal proof");
            check(f.format()!=pretty::outcome::formatted && f.calls==0u,"wrong compressed-pair ordinal cannot select string storage");
        }
        ++cases;
    }
    ::std::printf("{\"passed\":true,\"libcxx_layout_cases\":%u,\"scope\":\"owned DWARF bit-field layouts, not producer parity\"}\n",cases);
}
