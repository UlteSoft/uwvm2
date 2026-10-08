#include <fast_io.h>
#include <uwvm2/uwvm/debugger/source_language_expression.h>
using namespace uwvm2::uwvm::debugger;
static ::std::size_t checks{};
static void require(bool value,char const* message)
{
    ++checks;
    if(!value) { ::fast_io::io::perrln("Go collection failure: ",::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); }
}
static void put(::std::vector<::std::byte>& data,::std::size_t offset,::std::uint64_t value,unsigned width)
{
    ::fast_io::basic_obuffer_view<char> sink{reinterpret_cast<char*>(data.data()+offset),reinterpret_cast<char*>(data.data()+offset+width)};
    if(width==4u) { ::fast_io::io::print(sink,::fast_io::mnp::le_put<32u>(value)); }
    else { ::fast_io::io::print(sink,::fast_io::mnp::le_put<64u>(value)); }
}
int main()
{
    using K=source_dwarf::type_kind;
    for(::std::uint8_t width : {4u,8u}) for(bool channel : {false,true}) for(bool slots : {false,true})
    {
        if(channel && slots) { continue; }
        ::std::vector<source_dwarf::type_record> types(9u);
        for(auto& t:types) { t.language=0x0cu;t.tinygo_producer=true;t.size_known=true; }
        types[0].kind=K::scalar;types[0].encoding=7u;types[0].byte_size=types[0].byte_count=width;
        types[1].kind=K::scalar;types[1].encoding=2u;types[1].byte_size=types[1].byte_count=1u;
        types[2]=types[1];types[2].encoding=7u;
        types[3].kind=K::pointer;types[3].byte_size=types[3].byte_count=width;types[3].referenced_type=2u;
        types[4].kind=K::structure;types[4].byte_size=2u*width;
        types[5].kind=K::structure;types[5].byte_size=width;
        types[6].kind=K::structure;
        auto& object=types[7];object.kind=K::structure;object.name=::fast_io::concat_std(channel ? "runtime.channel" : "runtime.hashmap");
        types[8]=types[3];types[8].referenced_type=7u;types[8].name=::fast_io::concat_std("main.NamedContainer");
        auto member=[&](char const* name,unsigned index,::std::size_t type,unsigned extra=0u)
        {
            source_dwarf::member_record m{};m.name=::fast_io::concat_std(::fast_io::mnp::os_c_str(name));m.type=type;m.offset_known=true;m.byte_offset=index*width+extra;object.members.push_back(::std::move(m));
        };
        if(channel)
        {
            object.byte_size=9u*width;member("closed",0u,1u);member("selectLocked",0u,1u,1u);
            member("elementSize",1u,0u);member("bufCap",2u,0u);member("bufLen",3u,0u);member("bufHead",4u,0u);member("bufTail",5u,0u);
            member("senders",6u,5u);member("receivers",7u,5u);member("lock",8u,6u);member("buf",8u,3u);
        }
        else
        {
            unsigned const tail=slots ? 7u : 5u;object.byte_size=(tail+5u)*width;
            member("buckets",0u,3u);member("seed",1u,0u);member("count",2u,0u);member("keySize",3u,0u);member("valueSize",4u,0u);
            if(slots) { member("keySlotSize",5u,0u);member("valueSlotSize",6u,0u); }
            member("bucketBits",tail,2u);member("flags",tail,2u,1u);member("keyEqual",tail+1u,4u);member("keyHash",tail+3u,4u);
        }
        auto original=types;::std::vector<::std::byte> root(width),known(width,::std::byte{0xffu}),qualified=known,header(object.byte_size);
        put(root,0u,0x100u,width);put(header,2u*width,channel ? 5u : 3u,width);if(channel) { put(header,3u*width,2u,width); }
        ::std::size_t reads{};bool read_ok=true,short_copy=false;
        auto reader=[&](::std::uint64_t address,::std::size_t size,auto& copied)
        {
            ++reads;require(address==0x100u && size==header.size(),"reader escaped exact ordinary container header");
            copied=header;if(short_copy) { copied.pop_back(); }return read_ok;
        };
        auto query=[&](char const* text,bool type=false,::std::size_t budget=32u)
        {
            reads=0u;source_dwarf::source_expression parsed{};require(source_dwarf::parse_source_expression(text,parsed)==source_dwarf::object_selector_error::none,"collection builtin grammar");
            ::std::vector<source_dwarf::object_node> out{};
            auto status=type ? source_language_expression::type(types,8u,parsed.steps,width,out) :
                source_language_expression::value(types,8u,parsed.steps,root,known,width,reader,out,{budget},qualified);
            return ::std::pair{status,out};
        };
        auto expect=[&](char const* text,::std::uint64_t expected,::std::size_t copies)
        {
            auto [status,out]=query(text);require(status==source_dwarf::inline_query_error::none && out.size()==1u && out[0].value_available &&
                out[0].bits==expected && out[0].scalar_bytes==width && out[0].scalar_kind==source_dwarf::numeric_kind::signed_integer,"collection int value");
            require(reads==copies,"container query followed extra pointers");
            auto [typed,nodes]=query(text,true);require(typed==source_dwarf::inline_query_error::none && nodes.size()==1u && !nodes[0].value_available && nodes[0].byte_size==width && reads==0u,"container type query read values");
        };
        expect("len(c)",channel ? 2u : 3u,1u);
        if(channel) { expect("cap(c)",5u,1u); }
        else { require(query("cap(c)").first==source_dwarf::inline_query_error::unavailable && reads==0u,"map capacity accepted");require(query("cap(c)",true).first==source_dwarf::inline_query_error::unavailable,"map capacity type accepted"); }
        require(query("len(c)",false,0u).first==source_dwarf::inline_query_error::limit_exceeded && reads==0u,"read budget not charged first");
        expect("len(c)",channel ? 2u : 3u,1u);
        read_ok=false;require(query("len(c)").first==source_dwarf::inline_query_error::unavailable && reads==1u,"failed guest read fabricated a count");read_ok=true;
        short_copy=true;require(query("len(c)").first==source_dwarf::inline_query_error::unavailable && reads==1u,"short header accepted");short_copy=false;
        known[0]=::std::byte{};require(query("len(c)").first==source_dwarf::inline_query_error::unavailable && reads==0u,"unknown pointer read");known[0]=::std::byte{0xffu};
        qualified[0]=::std::byte{};require(query("len(c)").first==source_dwarf::inline_query_error::unavailable && reads==0u,"unqualified float carrier read");qualified[0]=::std::byte{0xffu};
        put(root,0u,0u,width);expect("len(c)",0u,0u);if(channel) { expect("cap(c)",0u,0u); }
        known[0]=::std::byte{};require(query("len(c)").first==source_dwarf::inline_query_error::unavailable && reads==0u,"unknown zero-filled pointer treated as nil");known[0]=::std::byte{0xffu};
        put(root,0u,width==4u ? 0xffffffffull : ~::std::uint64_t{},width);require(query("len(c)").first==source_dwarf::inline_query_error::unavailable && reads==0u,"header extent crossed guest width");put(root,0u,0x100u,width);
        auto const maximum=width==4u ? 0x7fffffffull : 0x7fffffffffffffffull;
        put(header,channel ? 3u*width : 2u*width,maximum+1u,width);require(query("len(c)").first==source_dwarf::inline_query_error::unavailable,"count overflow accepted");
        put(header,channel ? 3u*width : 2u*width,channel ? 2u : 3u,width);
        if(channel)
        {
            put(header,2u*width,1u,width);require(query("len(c)").first==source_dwarf::inline_query_error::unavailable,"channel len greater than cap");require(query("cap(c)").first==source_dwarf::inline_query_error::unavailable,"channel contradictory capacity");put(header,2u*width,5u,width);
        }
        // Every field of either qualified ABI must have the exact owned
        // offset/type and may not be a bit-field or ambiguous duplicate.
        for(::std::size_t i{};i<original[7].members.size();++i)
        {
            for(unsigned fault{};fault<4u;++fault)
            {
                types=original;
                if(fault==0u) { types[7].members[i].byte_offset+=1u; }
                if(fault==1u) { types[7].members[i].offset_known=false; }
                if(fault==2u) { types[7].members[i].bit_field=true; }
                if(fault==3u) { types[7].members[i].type=99u; }
                require(query("len(c)").first!=source_dwarf::inline_query_error::none && reads==0u,"invalid ABI field acquired guest read");
                require(query("len(c)",true).first!=source_dwarf::inline_query_error::none,"invalid ABI field acquired type result");
            }
        }
        for(unsigned fault{};fault<12u;++fault)
        {
            types=original;
            if(fault==0u) { types[8].language=0x1cu; }
            if(fault==1u) { types[8].tinygo_producer=false; }
            if(fault==2u) { types[7].tinygo_producer=false; }
            if(fault==3u) { types[7].byte_size-=1u; }
            if(fault==4u) { types[8].reference_type=true; }
            if(fault==5u) { types[8].rvalue_reference_type=true; }
            if(fault==6u) { types[8].address_class_known=true;types[8].address_class=1u; }
            if(fault==7u) { types[8].byte_count=width==4u ? 8u : 4u; }
            if(fault==8u) { types[7].name=::fast_io::concat_std("user.lookalike"); }
            if(fault==9u) { types[7].members[0]=types[7].members[1]; }
            if(fault==10u) { types[0].encoding=5u; }
            if(fault==11u) { types[0].tinygo_producer=false; }
            require(query("len(c)").first!=source_dwarf::inline_query_error::none && reads==0u,"foreign/contradictory container acquired read");
        }
    }
    ::fast_io::io::println("debug_source_go_collections: PASS ",checks," assertions");
}
