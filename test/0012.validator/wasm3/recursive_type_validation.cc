#include <uwvm2/validation/standard/wasm3/recursive_type_binary.h>
#include <uwvm2/validation/standard/wasm3/recursive_type_validation.h>
#include <uwvm2/validation/standard/wasm3/recursive_type_registry.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <vector>
#include <initializer_list>
#include <string_view>
namespace v3=uwvm2::validation::standard::wasm3;
namespace t3=uwvm2::parser::wasm::standard::wasm3::type;
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"FAIL %u: %s\n",__LINE__,#x); std::abort(); } } while(false)
using bytes=std::vector<std::byte>;
unsigned checks{};
bytes raw(std::initializer_list<unsigned> values){bytes out;for(auto value:values)out.push_back(std::byte(value));return out;}
t3::recursive_type_section parse(bytes const& data)
{
 std::byte const* p=data.data();t3::recursive_type_section result;
 auto status=v3::scan_core3_type_section(p,p+data.size(),result);CHECK(status.error==v3::recursive_type_binary_error::ok);return result;
}
v3::recursive_type_context valid(bytes const& data,v3::recursive_type_validation_error error=v3::recursive_type_validation_error::ok)
{
 auto types=parse(data);v3::recursive_type_context output;output.records.resize(1);output.records.index_unchecked(0).canonical=12345;
 auto status=v3::validate_core3_type_section(types,output);CHECK(status.error==error);
 if(error!=v3::recursive_type_validation_error::ok)
 {CHECK(output.records.size()==1&&output.records.index_unchecked(0).canonical==12345);CHECK(status.binary_offset<data.size());}
 ++checks;return output;
}
bool match(v3::recursive_type_context const& ctx,std::int64_t a,std::int64_t b){++checks;return ctx.matches(t3::heap_type{a},t3::heap_type{b});}
int main(int argc,char**argv)
{
 if(argc==6&&std::string_view(argv[1])=="--match")
 {
  v3::recursive_type_registry registry;uwvm2::utils::container::vector<std::size_t> ids[2];
  for(unsigned i=0;i<2;++i)
  {
   std::ifstream file(argv[2+i*2],std::ios::binary);CHECK(file.good());std::vector<char> input((std::istreambuf_iterator<char>(file)),{});
   auto begin=reinterpret_cast<std::byte const*>(input.data());auto end=begin+input.size();t3::recursive_type_section types;
   CHECK(v3::scan_core3_type_section(begin,end,types).error==v3::recursive_type_binary_error::ok);
   CHECK(registry.validate_and_intern(types,ids[i]).error==v3::recursive_type_validation_error::ok);
  }
  auto a=std::strtoull(argv[3],nullptr,10),b=std::strtoull(argv[5],nullptr,10);CHECK(a<ids[0].size()&&b<ids[1].size());
  bool compatible=registry.matches(ids[0].index_unchecked(a),ids[1].index_unchecked(b));
  std::puts(compatible?"compatible":"incompatible");return compatible?0:1;
 }
 if(argc==2)
 {
  std::ifstream file(argv[1],std::ios::binary);CHECK(file.good());std::vector<char> input((std::istreambuf_iterator<char>(file)),{});
  auto p=reinterpret_cast<std::byte const*>(input.data());auto end=p+input.size();t3::recursive_type_section types;
  auto binary=v3::scan_core3_type_section(p,end,types);
  if(binary.error!=v3::recursive_type_binary_error::ok){std::printf("binary %u offset %zu\n",unsigned(binary.error),binary.error_offset);return 2;}
  v3::recursive_type_context ctx;auto result=v3::validate_core3_type_section(types,ctx);
  if(result.error!=v3::recursive_type_validation_error::ok){std::printf("validation %u type %llu offset %zu\n",unsigned(result.error),(unsigned long long)result.type_index,result.binary_offset);return 3;}
  v3::recursive_type_registry registry;uwvm2::utils::container::vector<std::size_t> ids;
  CHECK(registry.validate_and_intern(types,ids).error==v3::recursive_type_validation_error::ok);
  CHECK(ids.size()==ctx.records.size());
  std::printf("valid %zu\n",ctx.records.size());return 0;
 }
 using e=v3::recursive_type_validation_error;
 auto empty=valid(raw({0}));CHECK(empty.records.empty());CHECK(!match(empty,0,0));
 // Exact abstract heap lattice, including the four separate bottom hierarchies.
 bool lattice[12][12]{};for(unsigned i=0;i<12;++i)lattice[i][i]=true;
 auto edge=[&](int a,int b){lattice[a+23][b+23]=true;};
 edge(-15,-20);edge(-15,-21);edge(-15,-22);edge(-15,-19);edge(-20,-19);edge(-21,-19);edge(-22,-19);edge(-19,-18);
 edge(-13,-16);edge(-14,-17);edge(-12,-23);
 for(unsigned k=0;k<12;++k)for(unsigned i=0;i<12;++i)for(unsigned j=0;j<12;++j)lattice[i][j]|=lattice[i][k]&&lattice[k][j];
 for(int a=-23;a<=-12;++a)for(int b=-23;b<=-12;++b)CHECK(match(empty,a,b)==lattice[a+23][b+23]);
 CHECK(!match(empty,-24,-24));CHECK(!match(empty,-11,-18));
 auto funcs=valid(raw({3,0x60,0,1,0x7f,0x60,0,1,0x7f,0x60,0,1,0x7e}));
 CHECK(match(funcs,0,1)&&match(funcs,1,0)&&!match(funcs,0,2));CHECK(match(funcs,0,-16)&&match(funcs,-13,0)&&!match(funcs,-15,0));
 // Alpha-equivalent self-recursive groups at different module indices are the same closed defined type.
 auto self=valid(raw({2,0x5f,1,0x63,0,0,0x5f,1,0x63,1,0}));CHECK(match(self,0,1)&&match(self,1,0));
 auto mutual=valid(raw({2,0x4e,2,0x5f,1,0x63,1,0,0x5f,1,0x63,0,0,
                       0x4e,2,0x5f,1,0x63,3,0,0x5f,1,0x63,2,0}));
 CHECK(match(mutual,0,2)&&match(mutual,1,3)&&!match(mutual,0,1));
 // Identical individual shapes in differently sized groups must not collapse.
 auto projection=valid(raw({2,0x5f,0,0x4e,2,0x5f,0,0x5f,0}));CHECK(!match(projection,0,1)&&!match(projection,1,2));
 auto closed=valid(raw({4,0x5f,0,0x5f,0,0x5e,0x63,0,0,0x5e,0x63,1,0}));CHECK(match(closed,2,3));
 auto immutable=valid(raw({2,0x50,0,0x5f,1,0x6e,0,0x4f,1,0,0x5f,2,0x6c,0,0x7e,1}));
 CHECK(match(immutable,1,0)&&!match(immutable,0,1)&&match(immutable,-15,1)&&match(immutable,1,-19)&&match(immutable,1,-18));
 valid(raw({2,0x50,0,0x5f,1,0x6e,1,0x4f,1,0,0x5f,1,0x6c,1}),e::incompatible_supertype);
 valid(raw({2,0x50,0,0x5f,1,0x7f,0,0x4f,1,0,0x5f,1,0x7f,1}),e::incompatible_supertype);
 valid(raw({2,0x50,0,0x5e,0x78,0,0x4f,1,0,0x5e,0x77,0}),e::incompatible_supertype);
 auto contravariant=valid(raw({2,0x50,0,0x60,1,0x6d,1,0x6e,0x4f,1,0,0x60,1,0x6e,1,0x6d}));CHECK(match(contravariant,1,0));
 valid(raw({2,0x50,0,0x60,1,0x6e,1,0x6d,0x4f,1,0,0x60,1,0x6d,1,0x6e}),e::incompatible_supertype);
 valid(raw({2,0x50,0,0x60,0,0,0x4f,1,0,0x60,1,0x7f,0}),e::incompatible_supertype);
 auto recursive_sub=valid(raw({1,0x4e,2,0x50,0,0x5f,1,0x63,0,0,0x4f,1,0,0x5f,1,0x63,1,0}));CHECK(match(recursive_sub,1,0));
 // Canonically equal defined references in mutable storage are invariant-compatible.
 valid(raw({4,0x5f,0,0x5f,0,0x50,0,0x5e,0x63,0,1,0x4f,1,2,0x5e,0x63,1,1}));
 valid(raw({2,0x5f,0,0x4f,1,0,0x5f,0}),e::final_supertype);
 valid(raw({1,0x50,1,0,0x5f,0}),e::forward_supertype);
 valid(raw({1,0x50,2,0,1,0x5f,0}),e::multiple_supertypes);
 valid(raw({2,0x50,0,0x5f,0,0x4f,1,0,0x60,0,0}),e::incompatible_supertype);
 valid(raw({2,0x60,1,0x63,1,0,0x60,0,0}),e::unknown_type); // next group is not in scope
 valid(raw({1,0x60,1,0x63,0xff,0xff,0xff,0xff,0x0f,0}),e::unknown_type);
 auto reference=t3::core_value_type{t3::value_kind::reference,{-21},false};auto nullable=reference;nullable.nullable=true;
 CHECK(empty.matches(reference,nullable)&&!empty.matches(nullable,reference));
 // Large valid type depth exercises O(1) ancestry queries without native recursive traversal or per-type ancestor lists.
 t3::recursive_type_section chain;constexpr unsigned depth=50000;chain.type_count=depth;
 for(unsigned i=0;i<depth;++i){t3::recursive_group group;group.first_type_index=i;t3::sub_type type;type.final_=false;type.kind=t3::composite_kind::struct_;
  if(i)type.supertypes.push_back(i-1);group.types.push_back(std::move(type));chain.groups.push_back(std::move(group));}
 v3::recursive_type_context deep;CHECK(v3::validate_core3_type_section(chain,deep).error==e::ok);
 for(unsigned i=0;i<depth;++i)CHECK(match(deep,depth-1,i));CHECK(!match(deep,0,depth-1));
 chain.groups.index_unchecked(depth-1).first_type_index=1;
 CHECK(v3::validate_core3_type_section(chain,deep).error==e::inconsistent_section);CHECK(deep.records.size()==depth);
 // Cross-module identities include closed external references and recursive group projection positions.
 v3::recursive_type_registry registry;uwvm2::utils::container::vector<std::size_t> first_ids,second_ids;
 auto first_module=parse(raw({2,0x5f,1,0x63,0,0,0x5e,0x63,0,1}));
 auto second_module=parse(raw({3,0x60,0,0,0x5f,1,0x63,1,0,0x5e,0x63,1,1}));
 CHECK(registry.validate_and_intern(first_module,first_ids).error==e::ok);
 CHECK(registry.validate_and_intern(second_module,second_ids).error==e::ok);
 CHECK(first_ids.index_unchecked(0)==second_ids.index_unchecked(1));
 CHECK(first_ids.index_unchecked(1)==second_ids.index_unchecked(2));
 CHECK(registry.size()==3&&!registry.matches(first_ids.index_unchecked(0),second_ids.index_unchecked(0)));
 auto old_size=registry.size();auto old_id=second_ids.index_unchecked(0);
 second_module.groups.index_unchecked(1).types.index_unchecked(0).supertypes.push_back(2);
 CHECK(registry.validate_and_intern(second_module,second_ids).error==e::forward_supertype);
 CHECK(registry.size()==old_size&&second_ids.size()==3&&second_ids.index_unchecked(0)==old_id);
 // Existing IDs remain stable when later modules extend the same non-final type hierarchy.
 chain.groups.index_unchecked(depth-1).first_type_index=depth-1;
 CHECK(registry.validate_and_intern(chain,first_ids).error==e::ok);
 CHECK(registry.validate_and_intern(chain,second_ids).error==e::ok);
 for(unsigned i=0;i<depth;++i)
 {
  CHECK(first_ids.index_unchecked(i)==second_ids.index_unchecked(i));
  CHECK(registry.matches(first_ids.index_unchecked(depth-1),second_ids.index_unchecked(i)));
  CHECK(registry.matches(first_ids.index_unchecked(i),second_ids.index_unchecked(depth-1))==(i==depth-1));
 }
 // Diverse ancestor depths exercise both low-bit skip and parent fallback branches.
 std::uint64_t random=1;
 for(unsigned i=0;i<100000;++i)
 {
  random=random*6364136223846793005ull+1;auto a=unsigned(random>>32)%depth;
  random=random*6364136223846793005ull+1;auto b=unsigned(random>>32)%depth;
  CHECK(registry.matches(first_ids.index_unchecked(a),second_ids.index_unchecked(b))==(a>=b));
 }
 CHECK(!registry.matches(registry.size(),0));
 std::puts("PASS recursive type registry: cross-module closure, rollback, stable IDs, 50000-deep subtyping, 100000 randomized ancestor pairs");
 std::printf("PASS recursive type validation: %u matching/rule checks; deep chain=%u\n",checks,depth);
}
