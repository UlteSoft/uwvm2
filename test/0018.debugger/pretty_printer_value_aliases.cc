// Owned, compiler-shaped DATA. Run only in the authorized SSH Linux cgroup.
#include <uwvm2/uwvm/debugger/source_pretty_printer.h>
#include <cstdio>
#include <cstdlib>
namespace d=uwvm2::uwvm::debugger::source_dwarf;
namespace p=uwvm2::uwvm::debugger::source_pretty_printer;
static void check(bool ok,char const* why){if(!ok){std::fprintf(stderr,"value aliases: %s\n",why);std::exit(1);}}
static d::type_record scalar(char const* name,unsigned size,unsigned encoding){d::type_record t{};t.name=name;t.language=0x21u;t.kind=d::type_kind::scalar;t.byte_size=t.byte_count=size;t.size_known=true;t.encoding=encoding;return t;}
static d::member_record member(char const* name,std::size_t type,unsigned at,bool base=false){d::member_record m{};m.name=name;m.type=type;m.byte_offset=at;m.offset_known=true;m.inherited=base;return m;}
static d::type_record object(char const* name,unsigned size,std::vector<d::member_record> fields={},bool is_union=false){d::type_record t{};t.name=name;t.language=0x21u;t.kind=is_union?d::type_kind::union_type:d::type_kind::structure;t.byte_size=size;t.size_known=true;t.members=std::move(fields);return t;}
struct fixture{
 unsigned width,reads{};std::size_t root{};std::vector<d::type_record> types;std::vector<std::byte> bytes,known,eligible,memory;std::vector<d::object_node> tree;
 explicit fixture(unsigned w):width(w),types{scalar("int",4,5),scalar("bool",1,2),scalar("unsigned char",1,8),scalar("long",w,5)},memory(1024){}
 std::size_t add(d::type_record t){auto i=types.size();types.push_back(std::move(t));return i;}
 std::size_t alias(std::size_t i){types[i].identity=types[i].declaration_identity={17,100+i};auto t=types[i];t.identity={17,10000+types.size()};if(t.kind==d::type_kind::structure&&t.aggregate_name.empty())t.aggregate_name=t.name;t.name="library_value_type";t.named_type_alias=true;return add(std::move(t));}
 std::size_t pointer(std::size_t target){auto t=object("pointer",width);t.kind=d::type_kind::pointer;t.byte_count=width;t.referenced_type=target;return add(std::move(t));}
 void seal(std::size_t i){root=i;bytes.assign(types[root].byte_size,std::byte{});known.assign(bytes.size(),std::byte{255});eligible=known;}
 static void put(std::vector<std::byte>& b,unsigned at,unsigned n,std::uint64_t v){check(at<=b.size()&&n<=b.size()-at,"fixture extent");for(unsigned i=0;i<n;++i)b[at+i]=std::byte((v>>(i*8))&255);}
 p::outcome run(p::limits const& cap={}){reads=0;check(d::query_object_value_with_known_bits(types,root,bytes,known,tree)==d::inline_query_error::none,"well-formed raw object");auto reader=[&](std::uint64_t at,std::size_t n,std::vector<std::byte>& out){++reads;if(at>memory.size()||n>memory.size()-at)return false;out.assign(memory.begin()+at,memory.begin()+at+n);return true;};return p::format(types,root,bytes,known,eligible,width,reader,tree,cap);}
 d::object_node const* value(){for(auto const& n:tree)if(n.detached_copy&&n.parent==0)return &n;return nullptr;}
};
static void optional(unsigned width){
 fixture f(width);auto a=f.alias(0);auto storage=f.add(object("",4,{member("__null_state_",2,0),member("__val_",a,0)},true));
 auto base=f.add(object("__optional_destruct_base",8,{member("",storage,0),member("__engaged_",1,4)}));
 for(unsigned i=0;i<7;++i)base=f.add(object("__optional_base",8,{member("",base,0,true)}));
 auto t=object("std::__2::optional<int>",8,{member("",base,0,true)});t.template_types={0};f.seal(f.add(std::move(t)));
 fixture::put(f.bytes,0,4,7);fixture::put(f.bytes,4,1,1);
 check(f.run()==p::outcome::formatted&&f.reads==0&&f.value()&&f.value()->type==a&&f.value()->bits==7,"deep inherited optional preserves active alias value");
 ++f.types[a].declaration_identity.unit;check(f.run()!=p::outcome::formatted&&f.reads==0,"cross-CU equal-size optional alias rejected");--f.types[a].declaration_identity.unit;
 ++f.types[a].declaration_identity.offset;check(f.run()!=p::outcome::formatted&&f.reads==0,"different declaration optional alias rejected");--f.types[a].declaration_identity.offset;
 fixture::put(f.bytes,4,1,0);f.known[0]=std::byte{};check(f.run()==p::outcome::formatted&&f.reads==0&&!f.value()&&f.tree[0].pretty_summary=="nullopt","empty optional ignores inactive payload");
 f.known[4]=std::byte{};check(f.run()!=p::outcome::formatted&&f.reads==0,"unknown optional discriminator cannot select payload");
}
static void variant(unsigned width){
 fixture f(width);auto a=f.alias(0),b=f.alias(1);auto first=f.add(object("__alt<0,int>",4,{member("__value",a,0)}));auto second=f.add(object("__alt<1,bool>",1,{member("__value",b,0)}));
 auto tail=f.add(object("__union<bool>",1,{member("__head",second,0)},true));auto data=f.add(object("__union<int,bool>",4,{member("__head",first,0),member("__tail",tail,0)},true));
 auto base=f.add(object("__base",8,{member("__data",data,0),member("__index",2,4)}));for(unsigned i=0;i<7;++i)base=f.add(object("__variant_base",8,{member("",base,0,true)}));
 auto t=object("std::__2::variant<int,bool>",8,{member("__impl_",base,0)});t.template_types={0,1};f.seal(f.add(std::move(t)));
 fixture::put(f.bytes,0,4,42);check(f.run()==p::outcome::formatted&&f.reads==0&&f.value()&&f.value()->type==a&&f.value()->bits==42,"variant first alias follows active index");
 fixture::put(f.bytes,0,4,1);fixture::put(f.bytes,4,1,1);check(f.run()==p::outcome::formatted&&f.value()&&f.value()->type==b&&f.value()->bits==1,"variant second alias follows active index");
 ++f.types[b].declaration_identity.offset;check(f.run()!=p::outcome::formatted&&f.reads==0,"variant alias spelling cannot replace declaration proof");--f.types[b].declaration_identity.offset;
 fixture::put(f.bytes,4,1,255);check(f.run()==p::outcome::formatted&&!f.value()&&f.reads==0,"valueless variant never reads stale alias payload");
 fixture::put(f.bytes,4,1,2);check(f.run()!=p::outcome::formatted&&f.reads==0,"out-of-range alternative remains unavailable");
}
static void shared(unsigned width){
 fixture f(width);auto a=f.alias(0),ptr=f.pointer(a);auto count=f.add(object("__shared_count",width*2,{member("__shared_owners_",3,width)}));auto control=f.add(object("__shared_weak_count",width*3,{member("",count,0,true),member("__shared_weak_owners_",3,width*2)}));auto cptr=f.pointer(control);
 auto t=object("std::__2::shared_ptr<int>",width*2,{member("__ptr_",ptr,0),member("__cntrl_",cptr,width)});t.template_types={0};f.seal(f.add(std::move(t)));fixture::put(f.bytes,0,width,256);fixture::put(f.bytes,width,width,512);fixture::put(f.memory,256,4,23);
 check(f.run()==p::outcome::formatted&&f.reads==2&&f.value()&&f.value()->bits==23&&f.tree[0].pretty_summary.find("use_count=1")!=std::string::npos,"shared alias retains independent owner and pointee copies");
 ++f.types[a].declaration_identity.unit;check(f.run()!=p::outcome::formatted&&f.reads==0,"foreign pointee alias rejected before control read");--f.types[a].declaration_identity.unit;
 // Metadata completion preserves the original eligible pointer carrier.
 f.types[control].declaration_identity={17,900};f.types[control].qualified_type_name="std::__2::__shared_weak_count";f.types[control].completion_anchors={"ABI release","ABI weak","ABI count"};
 auto declaration=f.types[control];declaration.size_known=false;declaration.byte_size=0;declaration.members.clear();declaration.declaration_identity={17,901};auto incomplete=f.add(std::move(declaration));f.types[cptr].referenced_type=incomplete;
 check(f.run()==p::outcome::formatted&&f.reads==2&&f.value()&&f.value()->bits==23,"exact owned qualified class and ABI anchors complete the control layout");
 f.types[incomplete].completion_anchors[0]="other ABI";check(f.run()!=p::outcome::formatted&&f.reads==0,"control class spelling cannot replace exact ABI anchor");f.types[incomplete].completion_anchors[0]="ABI release";
 f.types[incomplete].qualified_type_name="application::__shared_weak_count";check(f.run()!=p::outcome::formatted&&f.reads==0,"another namespace cannot supply control layout");f.types[incomplete].qualified_type_name=f.types[control].qualified_type_name;
 auto duplicate=f.types[control];duplicate.declaration_identity={18,900};auto duplicate_at=f.add(std::move(duplicate));check(f.run()!=p::outcome::formatted&&f.reads==0,"ambiguous independent class definitions remain unavailable");f.types[duplicate_at].completion_anchors[0]="unrelated";
 check(f.run()==p::outcome::formatted&&f.reads==2,"unique matching control definition restored");
 // Actual libc++ 23 EH versioned declarations versus legacy inline exports.
 std::array<std::string,3> versioned{
 "_ZNSt3__219__shared_weak_count16__release_sharedB9nqe230100Ev",
 "_ZNSt3__219__shared_weak_count10__add_weakB9nqe230100Ev",
 "_ZNKSt3__219__shared_weak_count9use_countB9nqe230100Ev"};
 auto legacy=versioned;legacy[0]="_ZNSt3__219__shared_weak_count16__release_sharedEv";legacy[1]="_ZNSt3__219__shared_weak_count10__add_weakEv";
 f.types[incomplete].completion_anchors=versioned;f.types[control].completion_anchors=legacy;
 check(f.run()==p::outcome::formatted&&f.reads==2,"exact use_count ABI and owned legacy exports complete control");
 f.types[incomplete].completion_anchors=legacy;f.types[control].completion_anchors=versioned;
 check(f.run()==p::outcome::formatted&&f.reads==2,"legacy completion is independent of declaration direction");
 f.types[incomplete].completion_anchors=versioned;f.types[control].completion_anchors=legacy;
 f.types[control].completion_anchors[2]="_ZNKSt3__219__shared_weak_count9use_countB9nqn230100Ev";
 check(f.run()!=p::outcome::formatted&&f.reads==0,"distinct exception ABI tags cannot share a control definition");
 f.types[control].completion_anchors=legacy;f.types[control].completion_anchors[0]="_ZNSt3__119__shared_weak_count16__release_sharedEv";
 check(f.run()!=p::outcome::formatted&&f.reads==0,"legacy method owner must match exact class namespace");
 f.types[control].completion_anchors=legacy;f.types[control].completion_anchors[0]="_ZNSt3__219__shared_weak_count16__release_sharedB9nqn230100Ev";
 check(f.run()!=p::outcome::formatted&&f.reads==0,"only exact third-anchor tag may qualify legacy methods");
 f.types[control].completion_anchors=legacy;f.types[control].atomic_scalar=true;
 check(f.run()!=p::outcome::formatted&&f.reads==0,"atomic control definition cannot complete a non-atomic carrier");f.types[control].atomic_scalar=false;
 auto invalid=versioned;invalid[2]="_ZNKSt3__219__shared_weak_count9use_countB99shortEv";
 f.types[control].completion_anchors=invalid;f.types[incomplete].completion_anchors=invalid;f.types[control].completion_anchors[0]=legacy[0];
 check(f.run()!=p::outcome::formatted&&f.reads==0,"malformed ABI tag lengths cannot enable legacy completion");
 f.types[control].completion_anchors=legacy;f.types[incomplete].completion_anchors=versioned;
 f.types[f.root].name="std::__2::weak_ptr<int>";check(f.run()==p::outcome::formatted&&f.reads==1&&!f.value(),"weak alias never reads pointee");
 fixture::put(f.memory,512+width,width,d::numeric_mask(width));check(f.run()==p::outcome::formatted&&f.reads==1&&!f.value()&&f.tree[0].pretty_summary.find("expired")!=std::string::npos,"expired weak alias remains non-dereferencing");
 f.eligible[width]=std::byte{};check(f.run()!=p::outcome::formatted&&f.reads==0,"alias proof never grants control pointer eligibility");
}
static void values(unsigned width){
 for(auto name:{"std::__2::optional<int>","std::expected<int,int>"}){
  fixture f(width);auto a=f.alias(0);bool opt=std::string_view(name).find("optional")!=std::string_view::npos;auto t=object(name,8,{member(opt?"__val_":"_M_val",a,0),member("_M_unex",a,0),member(opt?"__engaged_":"_M_has_value",1,4)});t.template_types={0,0};f.seal(f.add(std::move(t)));fixture::put(f.bytes,0,4,17);fixture::put(f.bytes,4,1,1);check(f.run()==p::outcome::formatted&&f.value()&&f.value()->bits==17&&f.reads==0,"active sum payload accepts exact owned alias");
 }
 fixture f(width);auto a=f.alias(0);auto t=object("std::__2::chrono::duration<int>",4,{member("__rep_",a,0)});t.template_types={0};auto duration=f.add(std::move(t));auto da=f.alias(duration);auto point=object("std::__2::chrono::time_point<clock,duration>",4,{member("__d_",da,0)});point.template_types={0,duration};f.seal(f.add(std::move(point)));fixture::put(f.bytes,0,4,31);check(f.run()==p::outcome::formatted&&f.reads==0&&f.tree[0].pretty_summary.find("duration ticks = 31")!=std::string::npos,"chrono nested aliases preserve canonical aggregate and scalar identity");
}
static void counted_vector(unsigned width){
 fixture f(width);auto element=f.alias(0),ptr=f.pointer(element);auto count=f.add(scalar("size_t",width,7));
 auto allocator=f.add(object("std::__2::allocator<int>",1));auto layout=object("std::__2::__vector_layout<int,allocator<int>>",width*3,
 {member("__begin_",ptr,0),member("__size_",count,width),member("__capacity_",count,width*2)});layout.template_types={element,allocator};auto storage=f.add(std::move(layout));
 auto owner=object("std::__2::vector<int,allocator<int>>",width*3,{member("__layout_",storage,0)});owner.template_types={0,allocator};f.seal(f.add(std::move(owner)));
 fixture::put(f.bytes,0,width,256);fixture::put(f.bytes,width,width,3);fixture::put(f.bytes,width*2,width,4);for(unsigned i=0;i<3;++i)fixture::put(f.memory,256+i*4,4,17+i);
 check(f.run()==p::outcome::formatted&&f.reads==1,"counted vector reads one bounded contiguous copy");
 unsigned children=0;for(auto const& n:f.tree)if(n.detached_copy&&n.parent==0){check(n.type==element&&n.bits==17+children,"counted vector preserves alias element values");++children;}check(children==3,"counted vector emits all elements");
 p::details::indexed_sequence plan{};check(p::details::index_plan(f.types,f.root,f.bytes,f.known,f.eligible,width,plan)&&plan.element==element&&plan.address==256&&plan.count==3,"counted vector indexing shares scalar extent proof");
 std::size_t indexed{};std::uint64_t extent{};bool bounded{};check(p::details::index_element_type(f.types,f.root,width,indexed,extent,bounded)&&indexed==element&&!bounded,"counted vector type query requires no guest reads");
 fixture::put(f.bytes,width,width,5);check(f.run()!=p::outcome::formatted&&f.reads==0,"counted length beyond capacity rejected before reads");fixture::put(f.bytes,width,width,3);
 f.known[0]=std::byte{};check(f.run()!=p::outcome::formatted&&f.reads==0,"eligible but unknown pointer cannot authorize a read");f.known[0]=std::byte{255};
 f.eligible[0]=std::byte{};check(f.run()!=p::outcome::formatted&&f.reads==0,"known pointer needs independent read eligibility");f.eligible[0]=std::byte{255};
 ++f.types[element].declaration_identity.unit;check(f.run()!=p::outcome::formatted&&f.reads==0,"counted vector element cannot borrow foreign-CU layout");--f.types[element].declaration_identity.unit;
 f.types[count].atomic_scalar=true;check(f.run()!=p::outcome::formatted&&f.reads==0,"atomic counted size is unavailable");f.types[count].atomic_scalar=false;
 fixture::put(f.bytes,width*2,width,d::numeric_mask(width));check(f.run()!=p::outcome::formatted&&f.reads==0,"overflowing allocated range rejected before prefix copy");fixture::put(f.bytes,width*2,width,4);
 f.types[storage].members[1].byte_offset=0;check(f.run()!=p::outcome::formatted&&f.reads==0,"overlapping counted vector fields rejected");f.types[storage].members[1].byte_offset=width;
 f.types[storage].template_types[1]=0;check(f.run()!=p::outcome::formatted&&f.reads==0,"counted storage allocator must belong to this vector");
}
int main(){for(unsigned width:{4u,8u}){optional(width);variant(width);shared(width);values(width);counted_vector(width);}std::puts("{\"passed\":true,\"scope\":\"owned alias declarations, inherited active sums and ownership; DATA only\"}");}
