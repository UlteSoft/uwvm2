// Owned metadata/copied DATA; no host standard-library objects are interpreted.
// Run only in the authorized SSH Linux 64 GiB cgroup, including QEMU hosts.
#include <uwvm2/uwvm/debugger/source_language_expression.h>
#include <cstdio>
#include <cstdlib>
namespace dwarf=::uwvm2::uwvm::debugger::source_dwarf;
namespace pretty=::uwvm2::uwvm::debugger::source_pretty_printer;
namespace expression=::uwvm2::uwvm::debugger::source_language_expression;
namespace
{
    void check(bool value,char const* text)
    { if(!value) { ::std::fprintf(stderr,"pretty_printer_sequences FAIL: %s\n",text);::std::exit(1); } }
    dwarf::type_record scalar(char const* name,unsigned width,unsigned encoding=7u)
    { dwarf::type_record out{};out.name=name;out.kind=dwarf::type_kind::scalar;out.language=0x21u;out.byte_size=width;
      out.byte_count=width;out.encoding=encoding;out.size_known=true;return out; }
    dwarf::type_record object(char const* name,unsigned size,::std::vector<dwarf::member_record> fields={})
    { dwarf::type_record out{};out.name=name;out.kind=dwarf::type_kind::structure;out.language=0x21u;out.byte_size=size;
      out.size_known=true;out.members=::std::move(fields);return out; }
    dwarf::member_record member(char const* name,unsigned type,unsigned offset,bool inherited=false)
    { dwarf::member_record out{};out.name=name;out.type=type;out.byte_offset=offset;out.offset_known=true;out.inherited=inherited;return out; }
    dwarf::type_record pointer(unsigned target,unsigned width)
    { auto out{object("",width)};out.kind=dwarf::type_kind::pointer;out.byte_count=width;out.referenced_type=target;return out; }
    void put(::std::vector<::std::byte>& out,unsigned at,unsigned width,::std::uint64_t value)
    { check(at<=out.size() && width<=out.size()-at && width<=8u,"bounded byte encoding");
      for(unsigned i{};i!=width;++i) { out[at+i]=static_cast<::std::byte>((value>>(i*8u))&255u); } }
    void put_bit(::std::vector<::std::byte>& out,unsigned bit,bool value)
    { check(bit/8u<out.size(),"bounded bit encoding");auto byte{::std::to_integer<unsigned char>(out[bit/8u])};
      byte=static_cast<unsigned char>((byte&~(1u<<(bit%8u)))|(value ? 1u<<(bit%8u) : 0u));out[bit/8u]=static_cast<::std::byte>(byte); }
    struct fixture
    {
        unsigned width{},word{},root{},calls{};bool fail{};
        ::std::vector<dwarf::type_record> types{};::std::vector<::std::byte> bytes{},known{},eligible{},memory{};
        ::std::vector<dwarf::object_node> tree{};
        fixture(unsigned w,unsigned word_width=0u):width(w),word(word_width==0u ? w : word_width),memory(4096u)
        { types={scalar("int",4u,5u),scalar("size_t",w),scalar("bool",1u,2u),scalar("unsigned word",word),pointer(0u,w),pointer(3u,w),pointer(4u,w)}; }
        unsigned add(dwarf::type_record type) { types.push_back(::std::move(type));return types.size()-1u; }
        void seal()
        {
            root=types.size()-1u;bytes.resize(types[root].byte_size);known.assign(bytes.size(),::std::byte{255u});eligible=known;
            for(::std::size_t i{};i!=types.size();++i) { types[i].identity={1u,i+1u};types[i].declaration_identity=types[i].identity; }
        }
        pretty::outcome format(pretty::limits const& cap={})
        {
            calls=0u;check(dwarf::query_object_value_with_known_bits(types,root,bytes,known,tree)==dwarf::inline_query_error::none,"copied raw tree");
            auto read{[&](::std::uint64_t at,::std::size_t count,::std::vector<::std::byte>& out)
            { ++calls;if(fail || at>memory.size() || count>memory.size()-at) { return false; }
              out.assign(memory.begin()+static_cast<::std::size_t>(at),memory.begin()+static_cast<::std::size_t>(at)+count);return true; }};
            return pretty::format(types,root,bytes,known,eligible,width,read,tree,cap);
        }
        ::std::vector<::std::uint64_t> values() const
        { ::std::vector<::std::uint64_t> out{};for(auto const& n:tree) if(n.detached_copy && n.parent==0u)
          { check(n.value_available,"copied synthetic child available");out.push_back(n.bits); }return out; }
        void span()
        {
            auto type{object("std::span<int,3>",width,{member("_M_ptr",4u,0u)})};type.template_types={0u};
            type.template_values={{1u,1u,3u,false,true}};add(::std::move(type));seal();put(bytes,0u,width,256u);
            for(unsigned i{};i!=3u;++i) { put(memory,256u+i*4u,4u,10u+i); }
        }
        void bitset(unsigned count)
        {
            ::std::vector<dwarf::member_record> fields{};
            if(count!=0u)
            {
                unsigned storage{3u};auto const words{(count-1u)/(word*8u)+1u};
                if(words>1u) { auto a{object("word[]",words*word)};a.kind=dwarf::type_kind::array;a.referenced_type=3u;a.dimensions={{0,words,true,true}};storage=add(::std::move(a)); }
                fields.push_back(member("_M_w",storage,0u));
            }
            auto type{object("std::bitset<N>",count==0u ? 1u : ((count-1u)/(word*8u)+1u)*word,fields)};
            type.template_values={{0u,1u,count,false,true}};add(::std::move(type));seal();
            for(unsigned i{};i<count;++i) { put_bit(bytes,i,i%3u==1u); }
        }
        unsigned bit_iterator_size{};
        void packed()
        {
            auto const offset_type{add(scalar("unsigned int",4u))};bit_iterator_size=(width+4u+width-1u)/width*width;
            auto const iterator{add(object("_Bit_iterator",bit_iterator_size,{member("_M_p",5u,0u),member("_M_offset",offset_type,width)}))};
            auto const impl{add(object("_Bvector_impl",bit_iterator_size*2u+width,
                {member("_M_start",iterator,0u),member("_M_finish",iterator,bit_iterator_size),member("_M_end_of_storage",5u,bit_iterator_size*2u)}))};
            auto type{object("std::vector<bool>",bit_iterator_size*2u+width,{member("_M_impl",impl,0u)})};type.template_types={2u};add(::std::move(type));seal();
            put(bytes,0u,width,256u);put(bytes,width,4u,3u);put(bytes,bit_iterator_size,width,256u+word);put(bytes,bit_iterator_size+width,4u,8u);
            put(bytes,bit_iterator_size*2u,width,256u+word*3u);
            for(unsigned i{};i!=word*8u+5u;++i) { put_bit(memory,256u*8u+3u+i,i%3u==1u); }
        }
        unsigned forward_node{};
        void forward()
        {
            auto const base_index{static_cast<unsigned>(types.size()+1u)};auto const ptr{add(pointer(base_index,width))};
            auto const base{add(object("_Fwd_list_node_base",width,{member("_M_next",ptr,0u)}))};
            auto storage_array{object("unsigned char[4]",4u)};storage_array.kind=dwarf::type_kind::array;
            auto const byte{add(scalar("unsigned char",1u))};storage_array.referenced_type=byte;storage_array.dimensions={{0,4u,true,true}};
            auto const array{add(::std::move(storage_array))};auto storage{object("__aligned_buffer<int>",4u,{member("_M_storage",array,0u)})};storage.template_types={0u};
            auto const stored{add(::std::move(storage))};auto node{object("std::_Fwd_list_node<int>",width+4u,
                {member("base",base,0u,true),member("_M_storage",stored,width)})};node.template_types={0u};forward_node=add(::std::move(node));
            auto const impl{add(object("_Fwd_list_impl",width,{member("_M_head",base,0u)}))};
            auto type{object("std::forward_list<int>",width,{member("_M_impl",impl,0u)})};type.template_types={0u};add(::std::move(type));seal();put(bytes,0u,width,256u);
            for(unsigned i{};i!=3u;++i) { put(memory,256u+i*256u,width,i==2u ? 0u : 512u+i*256u);put(memory,256u+i*256u+width,4u,(i+1u)*11u); }
        }
        void deque()
        {
            auto const iterator{add(object("_Deque_iterator",width*4u,
                {member("_M_cur",4u,0u),member("_M_first",4u,width),member("_M_last",4u,width*2u),member("_M_node",6u,width*3u)}))};
            auto const impl{add(object("_Deque_impl",width*10u,{member("_M_map",6u,0u),member("_M_map_size",1u,width),
                member("_M_start",iterator,width*2u),member("_M_finish",iterator,width*6u)}))};
            auto type{object("std::deque<int>",width*10u,{member("_M_impl",impl,0u)})};type.template_types={0u};add(::std::move(type));seal();
            put(bytes,0u,width,128u);put(bytes,width,width,4u);
            put(bytes,width*2u,width,260u);put(bytes,width*3u,width,256u);put(bytes,width*4u,width,268u);put(bytes,width*5u,width,128u);
            put(bytes,width*6u,width,520u);put(bytes,width*7u,width,512u);put(bytes,width*8u,width,524u);put(bytes,width*9u,width,128u+width);
            put(memory,128u,width,256u);put(memory,128u+width,width,512u);
            for(unsigned i{};i!=3u;++i) { put(memory,256u+i*4u,4u,10u+i);put(memory,512u+i*4u,4u,20u+i); }
        }
        unsigned compressed(unsigned first)
        {
            auto const extent{static_cast<unsigned>(types[first].byte_size)};
            auto const state{add(object("StatefulAllocator",4u,{member("state",0u,0u)}))};
            auto carrier{object("std::__1::__compressed_pair_elem<T,0,false>",extent,{member("__value_",first,0u)})};
            carrier.template_types={first};carrier.template_values={{1u,0u,0u,true,true}};auto const head{add(::std::move(carrier))};
            carrier=object("std::__1::__compressed_pair_elem<A,1,false>",4u,{member("__value_",state,0u)});
            carrier.template_types={state};carrier.template_values={{1u,0u,1u,true,true}};auto const tail{add(::std::move(carrier))};
            auto pair{object("std::__1::__compressed_pair<T,A>",(extent+4u+width-1u)/width*width,
                {member("",head,0u,true),member("",tail,extent,true)})};pair.template_types={first,state};return add(::std::move(pair));
        }
        void cpp_packed(bool old=false)
        {
            auto const cap{old ? compressed(1u) : 1u};
            auto type{object("std::__1::vector<bool>",width*2u+static_cast<unsigned>(types[cap].byte_size),
                {member("__begin_",5u,0u),member("__size_",1u,width),member(old ? "__cap_alloc_" : "__cap_",cap,width*2u)})};
            type.template_types={2u};add(::std::move(type));seal();put(bytes,0u,width,256u);put(bytes,width,width,word*8u+5u);put(bytes,width*2u,width,3u);
            for(unsigned i{};i!=word*8u+5u;++i) { put_bit(memory,256u*8u+i,i%3u==1u); }
        }
        unsigned cpp_iterator{};
        void cpp_deque(bool old=false)
        {
            memory.resize(16384u);auto const difference{add(scalar("ptrdiff_t",width,5u))};
            auto iterator{object("std::__1::__deque_iterator<int,int*,int&,int**,ptrdiff_t,1024>",width*2u)};
            iterator.template_types={0u,4u,4u,6u,difference};iterator.template_values={{5u,difference,1024u,true,true}};cpp_iterator=add(::std::move(iterator));
            auto const capacity{old ? compressed(6u) : 6u};auto const map_size{width*3u+static_cast<unsigned>(types[capacity].byte_size)};
            auto const map{add(object("__split_buffer<int*>",map_size,{member("__first_",6u,0u),member("__begin_",6u,width),
                member("__end_",6u,width*2u),member("__end_cap_",capacity,width*3u)}))};
            auto const size{old ? compressed(1u) : 1u};
            auto type{object("std::__1::deque<int>",map_size+width+static_cast<unsigned>(types[size].byte_size),
                {member("__map_",map,0u),member("__start_",1u,map_size),member("__size_",size,map_size+width)})};
            type.template_types={0u};type.static_constants={{"__block_size",difference,1024u,true,true}};add(::std::move(type));seal();
            put(bytes,0u,width,128u);put(bytes,width,width,128u);put(bytes,width*2u,width,128u+width*2u);put(bytes,width*3u,width,128u+width*2u);
            put(bytes,map_size,width,1023u);put(bytes,map_size+width,width,3u);
            put(memory,128u,width,512u);put(memory,128u+width,width,8192u);put(memory,512u+1023u*4u,4u,11u);put(memory,8192u,4u,20u);put(memory,8196u,4u,21u);
        }
    };
}
int main()
{
    for(unsigned width:{4u,8u})
    {
        fixture s{width};s.span();check(s.format()==pretty::outcome::formatted && s.values()==::std::vector<::std::uint64_t>{10u,11u,12u} && s.calls==1u,"static extent from integral template DIE");
        ::std::vector<dwarf::object_node> indexed{};
        auto select{[&](::std::int64_t index,bool type_only,expression::limits const& cap={})
        {
            s.calls=0u;dwarf::source_expression_step step{};step.kind=dwarf::source_expression_step_kind::index;step.index=index;
            ::std::span<dwarf::source_expression_step const> steps{::std::addressof(step),1u};
            auto read{[&](::std::uint64_t at,::std::size_t count,::std::vector<::std::byte>& out)
            { ++s.calls;if(at>s.memory.size() || count>s.memory.size()-at) { return false; }
              out.assign(s.memory.begin()+static_cast<::std::size_t>(at),s.memory.begin()+static_cast<::std::size_t>(at)+count);return true; }};
            return type_only ? expression::type(s.types,s.root,steps,width,indexed) :
                expression::value(s.types,s.root,steps,s.bytes,s.known,width,read,indexed,cap,s.eligible);
        }};
        s.known.assign(s.known.size(),::std::byte{});s.eligible=s.known;
        check(select(2,true)==dwarf::inline_query_error::none && indexed.size()==1u && indexed[0u].type==0u && s.calls==0u,"static span type index uses exact extent metadata only");
        check(select(3,true)==dwarf::inline_query_error::unavailable && s.calls==0u,"static span type index checks known extent");
        s.known.assign(s.known.size(),::std::byte{255u});s.eligible=s.known;
        check(select(2,false)==dwarf::inline_query_error::none && indexed.size()==1u && indexed[0u].bits==12u && s.calls==1u,"static span selected value reads one element");
        check(select(3,false)==dwarf::inline_query_error::unavailable && s.calls==0u &&
              select(-1,false)==dwarf::inline_query_error::unavailable && s.calls==0u,"static span out of range refuses before read");
        expression::limits no_reads{};no_reads.maximum_reads=0u;
        check(select(0,false,no_reads)==dwarf::inline_query_error::limit_exceeded && s.calls==0u,"static span honors zero read budget");
        s.eligible[0u]=::std::byte{254u};check(select(0,false)==dwarf::inline_query_error::unavailable && s.calls==0u,"static span pointer eligibility checked");s.eligible=s.known;
        put(s.bytes,0u,width,width==4u ? 0xffffffffull : ~::std::uint64_t{});
        check(select(0,false)==dwarf::inline_query_error::unavailable && s.calls==0u,"static span complete range overflow refuses before read");put(s.bytes,0u,width,256u);
        s.types[s.root].template_values[0u].available=false;
        check(select(0,true)!=dwarf::inline_query_error::none && select(0,false)!=dwarf::inline_query_error::none && s.calls==0u,"unavailable extent refuses type and value index");
        check(s.format()!=pretty::outcome::formatted && s.calls==0u,"unavailable extent never guessed from type name");
        s.types[s.root].template_values[0u].available=true;s.types[s.root].template_values[0u].bits=0u;put(s.bytes,0u,width,0u);
        check(s.format()==pretty::outcome::formatted && s.calls==0u && s.values().empty(),"empty static span no reads");
        check(select(0,true)==dwarf::inline_query_error::unavailable && select(0,false)==dwarf::inline_query_error::unavailable &&
            s.calls==0u,"zero extent has no element value or indexed type");
        for(unsigned word:{4u,8u})
        {
            fixture b{width,word};b.bitset(word*8u+3u);check(b.format()==pretty::outcome::formatted && b.calls==0u,"bitset uses owned real-width words");
            auto values{b.values()};check(values.size()==word*8u+3u,"bitset template count");
            for(::std::size_t i{};i!=values.size();++i) { check(values[i]==(i%3u==1u),"bitset bit order"); }
            b.known[0u]=::std::byte{254u};check(b.format()!=pretty::outcome::formatted && !b.tree[0u].pretty_available,"unknown bitset bit atomic rejection");
            fixture v{width,word};v.packed();check(v.format()==pretty::outcome::formatted && v.calls==1u,"vector bool crosses actual storage-word boundary");
            values=v.values();check(values.size()==word*8u+5u,"vector bool size");
            for(::std::size_t i{};i!=values.size();++i) { check(values[i]==(i%3u==1u),"vector bool nonzero start offset"); }
            pretty::limits cap{};cap.max_elements=5u;
            check(v.format(cap)==pretty::outcome::formatted && v.values().size()==5u && v.tree[0u].pretty_omitted==word*8u,"packed element budget");
            v.eligible.assign(v.eligible.size(),::std::byte{});check(v.format()!=pretty::outcome::formatted && v.calls==0u,"packed pointer needs eligible carrier");v.eligible=v.known;
            put(v.bytes,v.bit_iterator_size+width,4u,word*8u);check(v.format()!=pretty::outcome::formatted && v.calls==0u,"packed offset must fit real word");
            fixture cpp{width,word};cpp.cpp_packed();check(cpp.format()==pretty::outcome::formatted && cpp.calls==1u,"libc++ vector bool proven word capacity");
            values=cpp.values();check(values.size()==word*8u+5u,"libc++ packed size");
            for(::std::size_t i{};i!=values.size();++i) { check(values[i]==(i%3u==1u),"libc++ packed bit order"); }
            put(cpp.bytes,width*2u,width,1u);check(cpp.format()!=pretty::outcome::formatted && cpp.calls==0u,"libc++ packed size cannot exceed word capacity");
            fixture legacy{width,word};legacy.cpp_packed(true);pretty::details::field ambiguous{};
            check(!pretty::details::path(legacy.types,legacy.root,"__cap_alloc_.__value_",ambiguous),"generic lookup retains stateful pair ambiguity");
            check(legacy.format()==pretty::outcome::formatted && legacy.values()==values,"LLVM18 stateful compressed capacity selects ordinal zero");
            auto const pair_type{legacy.types[legacy.root].members.back().type};
            auto const carrier{legacy.types[pair_type].members[0u].type};legacy.types[carrier].template_values[0u].available=false;
            check(legacy.format()!=pretty::outcome::formatted && legacy.calls==0u,"unproven compressed ordinal never selects by first base order");
            legacy.types[carrier].template_values[0u].available=true;legacy.types[pair_type].members.push_back(legacy.types[pair_type].members[0u]);
            check(legacy.format()!=pretty::outcome::formatted && legacy.calls==0u,"duplicate compressed ordinal rejected");
        }
        fixture empty{width};empty.bitset(0u);check(empty.format()==pretty::outcome::formatted && empty.calls==0u,"bitset zero needs no storage");
        fixture f{width};f.forward();check(f.format()==pretty::outcome::formatted && f.values()==::std::vector<::std::uint64_t>{11u,22u,33u},"forward list typed nodes");
        pretty::limits cap{};cap.max_elements=1u;check(f.format(cap)==pretty::outcome::formatted && f.values().size()==1u && f.tree[0u].pretty_truncated,"forward prefix does not invent count");
        put(f.memory,768u,width,256u);check(f.format()!=pretty::outcome::formatted && !f.tree[0u].pretty_available,"forward list cycle atomic rejection");
        fixture d{width};d.deque();check(d.format()==pretty::outcome::formatted && d.values()==::std::vector<::std::uint64_t>{11u,12u,20u,21u},"deque crosses metadata-proven blocks");
        cap={};cap.max_elements=1u;check(d.format(cap)==pretty::outcome::formatted && d.values()==::std::vector<::std::uint64_t>{11u} && d.tree[0u].pretty_omitted==3u,"deque bounded prefix");
        cap={};cap.maximum_reads=1u;check(d.format(cap)==pretty::outcome::formatted && d.calls==1u && d.tree[0u].pretty_truncated,"deque read budget");
        put(d.memory,128u+width,width,768u);check(d.format()!=pretty::outcome::formatted && !d.tree[0u].pretty_available,"deque finish map slot verified");
        d.eligible.assign(d.eligible.size(),::std::byte{});check(d.format()!=pretty::outcome::formatted && d.calls==0u,"deque pointer eligibility checked before any map read");
        fixture cpp_deque{width};cpp_deque.cpp_deque();check(cpp_deque.format()==pretty::outcome::formatted &&
            cpp_deque.values()==::std::vector<::std::uint64_t>{11u,20u,21u},"libc++ deque root-owned static block size crosses map entries");
        cpp_deque.types[cpp_deque.cpp_iterator].template_values[0u].available=false;
        check(cpp_deque.format()==pretty::outcome::formatted,"unrelated iterator unavailable metadata does not override exact root constant");
        cpp_deque.types[cpp_deque.root].static_constants.clear();cpp_deque.types[cpp_deque.cpp_iterator].template_values[0u].available=true;
        check(cpp_deque.format()!=pretty::outcome::formatted && cpp_deque.calls==0u,"same-T iterator never proves deque root ABI");
        cpp_deque.types[cpp_deque.root].libcxx18_deque_blocks=true;
        check(cpp_deque.format()==pretty::outcome::formatted &&
              cpp_deque.values()==::std::vector<::std::uint64_t>{11u,20u,21u},
              "owned libc++18 ABI profile supplies omitted root constant and crosses block boundary");
        cpp_deque.types[cpp_deque.root].static_constants={{"__block_size",1u,4u,true,false}};
        check(cpp_deque.format()!=pretty::outcome::formatted && cpp_deque.calls==0u,
              "conflicting explicit constant cannot fall through to known ABI profile");
        cpp_deque.types[cpp_deque.root].static_constants.clear();cpp_deque.types[cpp_deque.root].atomic_scalar=true;
        check(cpp_deque.format()!=pretty::outcome::formatted && cpp_deque.calls==0u,
              "atomic wrapper never inherits library ABI fallback");
        cpp_deque.types[cpp_deque.root].atomic_scalar=false;cpp_deque.eligible.assign(cpp_deque.bytes.size(),::std::byte{});
        check(cpp_deque.format()!=pretty::outcome::formatted && cpp_deque.calls==0u,
              "known library ABI never supplies a guest pointer read capability");
        fixture small{width};small.cpp_deque();auto& small_owner{small.types[small.root]};
        small_owner.name="std::__2::deque<int>";small_owner.static_constants.clear();small_owner.libcxx23_abi2_small_deque_blocks=true;
        put(small.bytes,width*4u,width,127u);put(small.memory,512u+127u*4u,4u,11u);
        check(small.format()==pretty::outcome::formatted && small.values()==::std::vector<::std::uint64_t>{11u,20u,21u},
            "owned libc++23 ABI2 small block policy crosses correct map boundary");
        small.types[small.root].libcxx18_deque_blocks=true;
        check(small.format()!=pretty::outcome::formatted && small.calls==0u,"conflicting versioned block policies stay unavailable");
        small.types[small.root].libcxx18_deque_blocks=false;small.types[small.root].libcxx23_abi2_small_deque_blocks=false;
        check(small.format()!=pretty::outcome::formatted && small.calls==0u,"same small layout without own ABI evidence stays unavailable");
        fixture alias{width};alias.cpp_deque();auto aggregate{alias.types[alias.root]};
        alias.types[alias.root].name="DequeAlias";alias.types[alias.root].aggregate_name=aggregate.name;
        alias.types[alias.root].named_type_alias=true;alias.types[alias.root].static_constants.clear();
        alias.types[alias.root].identity.offset+=1000u;
        alias.types.push_back(aggregate);
        check(alias.format()==pretty::outcome::formatted && alias.values()==::std::vector<::std::uint64_t>{11u,20u,21u},
            "typedef deque block constant comes only from its exact aggregate declaration");
        alias.types.back().declaration_identity.unit=2u;
        check(alias.format()!=pretty::outcome::formatted && alias.calls==0u,"different-CU same-name aggregate cannot supply block constant");
        alias.types.back().declaration_identity=aggregate.declaration_identity;alias.types.back().declaration_identity.offset+=1000u;
        check(alias.format()!=pretty::outcome::formatted && alias.calls==0u,"same-CU same-name distinct aggregate cannot supply block constant");
        alias.types.back().declaration_identity=aggregate.declaration_identity;alias.types[alias.root].static_constants={{"__block_size",aggregate.static_constants[0u].type,1024u,true,false}};
        check(alias.format()!=pretty::outcome::formatted && alias.calls==0u,"explicit unavailable alias constant cannot fall through to aggregate");
        fixture cv{width};cv.cpp_deque();auto concrete{cv.types[cv.root]};
        cv.types[cv.root].display_qualifiers=1u;cv.types[cv.root].identity.offset+=2000u;cv.types[cv.root].static_constants.clear();
        cv.types.push_back(concrete);
        check(cv.format()==pretty::outcome::formatted && cv.values()==::std::vector<::std::uint64_t>{11u,20u,21u},
            "const deque wrapper uses only its exact unqualified aggregate constant");
        cv.types[cv.root].atomic_scalar=true;
        check(cv.format()!=pretty::outcome::formatted && cv.calls==0u,"atomic wrapper cannot use deque ABI fallback");
        fixture old_deque{width};old_deque.cpp_deque(true);
        check(old_deque.format()==pretty::outcome::formatted && old_deque.values()==::std::vector<::std::uint64_t>{11u,20u,21u},
            "LLVM18 stateful split buffer end capacity and deque size");
        old_deque.types[old_deque.root].static_constants[0u].available=false;
        check(old_deque.format()!=pretty::outcome::formatted && old_deque.calls==0u,"unavailable root block size remains unavailable");
        old_deque.types[old_deque.root].static_constants[0u].available=true;
        old_deque.types[old_deque.root].static_constants.push_back(old_deque.types[old_deque.root].static_constants[0u]);
        check(old_deque.format()!=pretty::outcome::formatted && old_deque.calls==0u,"ambiguous root constants rejected");
    }
    ::std::puts("{\"passed\":true,\"scope\":\"owned sequence DWARF schemas; not producer parity\"}");
}
