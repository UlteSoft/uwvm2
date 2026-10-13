// Shared Core 3 reference typing. The file interface validates the explicitly supported
// straight-line instruction subset; it is not the complete VM validator or an interpreter.
#include <uwvm2/validation/standard/wasm3/gc_validation.h>
#include <uwvm2/validation/standard/wasm3/reference_cast.h>
#include <uwvm2/validation/standard/wasm3/gc_immediate.h>
#include <uwvm2/validation/standard/wasm3/recursive_type_binary.h>
#include <uwvm2/validation/standard/wasm3/local_declarations.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>
namespace v3=uwvm2::validation::standard::wasm3;
namespace t3=uwvm2::parser::wasm::standard::wasm3::type;
using error=v3::core3_reference_error;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %u: %s\n",__LINE__,#x);std::abort();}}while(false)
using bytes=std::vector<std::byte>;
bytes raw(std::initializer_list<unsigned> values){bytes result;for(auto v:values)result.push_back(std::byte(v));return result;}
bytes read(char const* path){std::ifstream f(path,std::ios::binary);CHECK(f.good());std::vector<char> chars((std::istreambuf_iterator<char>(f)),{});bytes result(chars.size());if(chars.size())std::memcpy(result.data(),chars.data(),chars.size());return result;}
t3::recursive_type_section decode(bytes const& input,v3::recursive_type_context& context)
{
 auto p=static_cast<std::byte const*>(input.data());t3::recursive_type_section section;
 CHECK(v3::scan_core3_type_section(p,p+input.size(),section).error==v3::recursive_type_binary_error::ok);
 CHECK(v3::validate_core3_type_section(section,context).error==v3::recursive_type_validation_error::ok);return section;
}
int function(bytes const& input,unsigned index,bytes const& body,bytes const& segments={})
{
 v3::recursive_type_context ctx;auto section=decode(input,ctx);std::vector<t3::sub_type const*> definitions;
 for(auto const& group:section.groups)for(auto const& type:group.types)definitions.push_back(&type);
 std::uint_least32_t data_count{};std::vector<t3::core_value_type> elements;
 if(!segments.empty())
 {
  v3::recursive_binary_details::reader metadata{{segments.data(),segments.size()}};std::uint_least32_t count;
  CHECK(metadata.u32(data_count)&&metadata.u32(count));CHECK(count<=segments.size());
  for(unsigned i=0;i<count;++i){t3::core_value_type type;CHECK(metadata.value(type));CHECK(type.kind==t3::value_kind::reference);elements.push_back(type);}
  CHECK(metadata.position==metadata.input.size());
 }
 v3::core3_gc_environment gc{definitions,elements,data_count};
 CHECK(index<definitions.size());auto const& sig=*definitions[index];CHECK(sig.kind==t3::composite_kind::function);
 auto cursor=static_cast<std::byte const*>(body.data());auto end=cursor+body.size();v3::core3_local_declarations locals;
 if(v3::scan_core3_local_declarations(cursor,end,sig.parameters.size(),ctx,locals).error!=v3::core3_local_error::ok)return 1;
 v3::recursive_binary_details::reader code{{cursor,std::size_t(end-cursor)}};v3::core3_operand_stack stack;v3::core3_local_initialization init;
 auto results=std::span<t3::core_value_type const>(sig.results.data(),sig.results.size());
 while(code.position<code.input.size())
 {
  unsigned op;CHECK(code.byte(op));error status=error::ok;
  switch(op)
  {
   case 0:stack.make_unreachable();break;
   case 1:break;
   case 0x1a:{v3::core3_operand value;if(!stack.pop(value))status=error::stack_underflow;break;}
   case 0x20:case 0x21:case 0x22:
   {
    std::uint_least32_t local;CHECK(code.u32(local));t3::core_value_type type;bool initialized;
    if(local<sig.parameters.size()){type=sig.parameters.index_unchecked(local);initialized=true;}
    else {if(!locals.find(local,type))return 1;initialized=v3::core3_value_is_defaultable(type);}
    if(op==0x20){if(!init.is_initialized(local,initialized))return 1;stack.push(type);}
    else {status=stack.pop_expected(type,ctx);if(status==error::ok){init.initialize(local,initialized);if(op==0x22)stack.push(type);}}
    break;
   }
   case 0x41:{std::int_least64_t ignored;CHECK(code.s33(ignored));stack.push({t3::value_kind::i32});break;}
   case 0x44:{CHECK(code.input.size()-code.position>=8);code.position+=8;stack.push({t3::value_kind::f64});break;}
   case 0xd0:{t3::heap_type heap;CHECK(code.heap(heap));status=v3::validate_core3_ref_null(stack,heap,ctx);break;}
   case 0xd1:status=v3::validate_core3_ref_is_null(stack);break;
   case 0xd3:status=v3::validate_core3_ref_eq(stack,ctx,true);break;
   case 0xd4:status=v3::validate_core3_ref_as_non_null(stack);break;
   case 0x14:case 0x15:
   {
    std::uint_least32_t target;CHECK(code.u32(target));if(target>=definitions.size())return 1;
    status=v3::validate_core3_call_ref(stack,target,*definitions[target],ctx,op==0x15,results);break;
   }
   case 0xfb:
   {
    // code.position is in the nonempty body span after the checked 0xfb byte; equality with end is permitted.
    auto p=code.input.data()+code.position;auto const start=p;
    auto immediate=v3::scan_gc_instruction(p,code.input.data()+code.input.size());
    if(immediate.error!=v3::gc_immediate_error::ok)return 1;
    // The transactional scanner advanced p inside that same body allocation; commit its bounded consumed count.
    code.position+=std::size_t(p-start);auto opcode=immediate.opcode;
    if(opcode>=20&&opcode<=23)
    {status=v3::validate_core3_ref_cast(stack,immediate.to,ctx,opcode<22,true);break;}
    if(opcode==24||opcode==25)
    {
     if(immediate.first!=0)return 1; // only the function label in this fixture
     status=v3::validate_core3_branch_on_cast(stack,immediate.from,immediate.to,results,ctx,opcode==25,true);break;
    }
    if(opcode==26||opcode==27){status=v3::validate_core3_convert_reference(stack,ctx,opcode==26,true);break;}
    status=v3::validate_core3_gc_instruction(stack,opcode,immediate.first,immediate.second,gc,ctx,true);break;
   }
   case 11:
   {
    if(code.position!=code.input.size())return 1;
    for(std::size_t i=results.size();i;i--)if(stack.pop_expected(results[i-1],ctx)!=error::ok)return 1;
    return stack.size()==0?0:1;
   }
   default:std::fprintf(stderr,"unsupported fixture opcode %x\n",op);return 77;
  }
  if(status!=error::ok){std::printf("reference validation error %u\n",unsigned(status));return 1;}
 }
 return 1;
}
int main(int argc,char**argv)
{
 if(argc==4||argc==5)return function(read(argv[1]),unsigned(std::strtoul(argv[2],nullptr,10)),read(argv[3]),argc==5?read(argv[4]):bytes{});
 {
  v3::recursive_type_context context;auto section=decode(raw({1,0x5e,0x78,1}),context);
  t3::sub_type const* definition=&section.groups.index_unchecked(0).types.index_unchecked(0);
  v3::core3_gc_environment environment{{&definition,1},{},0};v3::core3_operand_stack stack;stack.make_unreachable();
  CHECK(v3::validate_core3_gc_instruction(stack,8,0,0xffffffffu,environment,context,true)==error::ok);
  CHECK(stack.size()==1); // Huge fixed count has O(actual stack) validation cost, not O(count).
  v3::core3_operand result;CHECK(stack.pop(result)&&!result.unknown&&result.type.heap.code==0&&!result.type.nullable);
  auto size=stack.size();CHECK(v3::validate_core3_gc_instruction(stack,8,0,0xffffffffu,environment,context,false)==error::feature_disabled&&stack.size()==size);
  CHECK(v3::validate_core3_ref_eq(stack,context,false)==error::feature_disabled&&stack.size()==size);
  stack.push({t3::value_kind::f64});CHECK(v3::validate_core3_gc_instruction(stack,8,0,0xffffffffu,environment,context,true)==error::type_mismatch);
  CHECK(v3::validate_core3_gc_instruction(stack,8,1,0,environment,context,true)==error::unknown_type);
  CHECK(v3::validate_core3_gc_instruction(stack,20,0,0,environment,context,true)==error::unsupported_gc_opcode);
  CHECK(stack.set_control_frame(0,false));CHECK(v3::validate_core3_gc_instruction(stack,8,0,1,environment,context,true)==error::stack_underflow);
 }

 v3::recursive_type_context ctx;auto section=decode(raw({4,0x60,0,1,0x7f,0x50,0,0x60,1,0x7f,1,0x6e,0x4f,1,1,0x60,1,0x7f,1,0x6d,0x5f,0}),ctx);
 auto const& f0=section.groups.index_unchecked(0).types.index_unchecked(0);
 auto const& f1=section.groups.index_unchecked(1).types.index_unchecked(0);
 auto const& f2=section.groups.index_unchecked(2).types.index_unchecked(0);
 t3::core_value_type i32{t3::value_kind::i32},f64{t3::value_kind::f64};
 t3::core_value_type externref{t3::value_kind::reference,{-17},true},nonnull=externref;nonnull.nullable=false;
 {
  v3::core3_operand_stack stack;CHECK(v3::validate_core3_ref_null(stack,{0},ctx)==error::ok);
  CHECK(v3::validate_core3_call_ref(stack,0,f0,ctx,false)==error::ok);CHECK(stack.pop_expected(i32,ctx)==error::ok);CHECK(stack.size()==0);
  CHECK(v3::validate_core3_ref_null(stack,{-13},ctx)==error::ok);CHECK(v3::validate_core3_call_ref(stack,0,f0,ctx,false)==error::ok);
 }
 {
  v3::core3_operand_stack stack;CHECK(v3::validate_core3_ref_null(stack,{-16},ctx)==error::ok);
  CHECK(v3::validate_core3_call_ref(stack,0,f0,ctx,false)==error::type_mismatch);
  CHECK(v3::validate_core3_ref_func_type(stack,3,ctx)==error::expected_function_type);
  CHECK(v3::validate_core3_ref_func_type(stack,4,ctx)==error::unknown_type);
  CHECK(v3::validate_core3_ref_null(stack,{t3::heap_type::bottom_code},ctx)==error::unknown_type);
 }
 {
  v3::core3_operand_stack stack;stack.push(i32);CHECK(v3::validate_core3_ref_func_type(stack,2,ctx)==error::ok);
  CHECK(v3::validate_core3_call_ref(stack,1,f1,ctx,false)==error::ok);
  v3::core3_operand result;CHECK(stack.pop(result)&&result.type.heap.code==-18); // declared call signature widens the result
  stack.push(i32);CHECK(v3::validate_core3_ref_func_type(stack,2,ctx)==error::ok);
  CHECK(v3::validate_core3_call_ref(stack,2,f2,ctx,true,{f1.results.data(),f1.results.size()})==error::ok);
  CHECK(stack.pop(result)&&result.unknown);
 }
 {
  v3::core3_operand_stack stack;CHECK(v3::validate_core3_call_ref(stack,1,f1,ctx,true,{f2.results.data(),f2.results.size()})==error::tail_result_mismatch);
  stack.push(i32);CHECK(v3::validate_core3_ref_as_non_null(stack)==error::expected_reference);
  CHECK(v3::validate_core3_ref_is_null(stack)==error::stack_underflow);
  stack.push(externref);CHECK(v3::validate_core3_ref_as_non_null(stack)==error::ok);CHECK(stack.pop_expected(nonnull,ctx)==error::ok);
 }
 {
  v3::core3_operand_stack stack;stack.push(i32);stack.push(externref);
  CHECK(v3::validate_core3_branch_on_null(stack,false,{&i32,1},ctx)==error::ok);
  CHECK(stack.pop_expected(nonnull,ctx)==error::ok&&stack.pop_expected(i32,ctx)==error::ok);
  t3::core_value_type label[]{i32,externref};stack.push(i32);stack.push(externref);
  CHECK(v3::validate_core3_branch_on_null(stack,true,label,ctx)==error::ok&&stack.size()==1);
  stack.push(externref);CHECK(v3::validate_core3_branch_on_null(stack,true,{&i32,1},ctx)==error::invalid_branch_signature);
 }
 {
  v3::core3_operand_stack stack;stack.make_unreachable();CHECK(v3::validate_core3_ref_as_non_null(stack)==error::ok);
  CHECK(stack.pop_expected(i32,ctx)==error::type_mismatch); // ref bottom is not numeric bottom
  CHECK(v3::validate_core3_ref_as_non_null(stack)==error::ok);CHECK(stack.pop_expected(nonnull,ctx)==error::ok);
  CHECK(v3::validate_core3_branch_on_null(stack,false,{&i32,1},ctx)==error::ok);
  CHECK(stack.pop_expected(nonnull,ctx)==error::ok&&stack.pop_expected(f64,ctx)==error::type_mismatch); // branch reifies i32
  t3::core_value_type label[]{externref,externref};CHECK(v3::validate_core3_branch_on_null(stack,true,label,ctx)==error::ok);
  CHECK(stack.pop_expected(i32,ctx)==error::type_mismatch);
 }
 {
  v3::core3_operand_stack stack;stack.push(f64);CHECK(stack.set_control_frame(1,false));v3::core3_operand value;
  CHECK(!stack.pop(value));stack.make_unreachable();CHECK(stack.pop(value)&&value.unknown);CHECK(stack.size()==1);
  CHECK(!stack.set_control_frame(2,false));CHECK(stack.set_control_frame(0,false)&&stack.pop_expected(f64,ctx)==error::ok);
 }
 {
  v3::core3_operand_stack stack;stack.push(i32);
  CHECK(v3::validate_core3_ref_cast(stack,externref,ctx,false,false)==error::feature_disabled&&stack.size()==1);
  CHECK(v3::validate_core3_convert_reference(stack,ctx,true,false)==error::feature_disabled&&stack.size()==1);
  CHECK(v3::validate_core3_branch_on_cast(stack,externref,externref,{&externref,1},ctx,false,false)==error::feature_disabled&&stack.size()==1);
 }
 std::puts("PASS Core 3 GC limits/gates, cast policies and reference typing: call_ref/return_call_ref, subtyping, null branches, frame bounds, principal reference bottom");
}
