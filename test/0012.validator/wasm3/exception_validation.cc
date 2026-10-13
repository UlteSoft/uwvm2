// Normative Core 3 catch/throw typing, independent of native unwind ABI and backend emission.
#include <uwvm2/validation/standard/wasm3/exception_validation.h>
#include <cstdio>
#include <cstdlib>
#include <array>
#include <limits>
#include <vector>
namespace v=uwvm2::validation::standard::wasm3;namespace t=uwvm2::parser::wasm::standard::wasm3::type;
using e=v::core3_exception_error;unsigned checks{};
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL %u: %s\n",__LINE__,#x);std::abort();}}while(false)
t::core_value_type ref(t::abstract_heap_type h,bool nullable){return {t::value_kind::reference,{static_cast<std::int_least64_t>(h)},nullable};}
int main(){v::recursive_type_context ctx;
 std::array values{t::core_value_type{t::value_kind::i32},t::core_value_type{t::value_kind::i64},t::core_value_type{t::value_kind::f32},t::core_value_type{t::value_kind::f64},t::core_value_type{t::value_kind::v128},ref(t::abstract_heap_type::func,true),ref(t::abstract_heap_type::extern_,false),ref(t::abstract_heap_type::exn,true)};
 for(unsigned arity=0;arity<=values.size();++arity)for(unsigned kind=0;kind<4;++kind){
  t::sub_type tag{};for(unsigned i=0;i<arity;++i)tag.parameters.push_back(values[i]);t::sub_type const* tags[]{&tag,&tag};std::vector<t::core_value_type> label;
  if(kind<2)for(unsigned i=0;i<arity;++i)label.push_back(values[i]);if(kind&1)label.push_back(ref(t::abstract_heap_type::exn,false));
  std::span<t::core_value_type const> labels[]{label,{}};v::core3_exception_environment env{tags,labels};v::exception_catch_clause clause{static_cast<v::exception_catch_kind>(kind),0,0};
  CHECK(v::validate_core3_exception_catch(clause,env,ctx,true)==e::ok);CHECK(v::validate_core3_exception_catch(clause,env,ctx,false)==e::feature_disabled);
  clause.tag_index=1;CHECK(v::validate_core3_exception_catch(clause,env,ctx,true)==e::ok);clause.tag_index=0xffffffffu;CHECK(v::validate_core3_exception_catch(clause,env,ctx,true)==(kind<2?e::unknown_tag:e::ok));clause.tag_index=0;
  clause.label_index=2;CHECK(v::validate_core3_exception_catch(clause,env,ctx,true)==e::unknown_label);clause.label_index=0;
  // Empty label, additional label value, and equal-arity but wrong payload type are distinct invalid cases.
  auto old=labels[0];labels[0]={};CHECK(v::validate_core3_exception_catch(clause,env,ctx,true)==(old.empty()?e::ok:e::label_type_mismatch));labels[0]=old;
  label.push_back({t::value_kind::i32});labels[0]=label;CHECK(v::validate_core3_exception_catch(clause,env,ctx,true)==e::label_type_mismatch);label.pop_back();labels[0]=label;
  if(kind<2&&arity){auto save=label[0];label[0]={t::value_kind::f64};CHECK(v::validate_core3_exception_catch(clause,env,ctx,true)==e::label_type_mismatch);label[0]=save;}
  if(kind&1){label.back()=ref(t::abstract_heap_type::exn,true);CHECK(v::validate_core3_exception_catch(clause,env,ctx,true)==e::ok);label.back()=ref(t::abstract_heap_type::noexn,true);CHECK(v::validate_core3_exception_catch(clause,env,ctx,true)==e::label_type_mismatch);label.back()=ref(t::abstract_heap_type::extern_,true);CHECK(v::validate_core3_exception_catch(clause,env,ctx,true)==e::label_type_mismatch);label.back()=ref(t::abstract_heap_type::exn,false);}
  // Signature validation does not impose unique tags or catch-all-last: ordered dispatch handles both.
  for(unsigned repeated=0;repeated<3;++repeated)CHECK(v::validate_core3_exception_catch(clause,env,ctx,true)==e::ok);
 }
 for(unsigned arity=0;arity<=values.size();++arity){
  t::sub_type tag{};for(unsigned i=0;i<arity;++i)tag.parameters.push_back(values[i]);t::sub_type const* tags[]{&tag};v::core3_exception_environment env{tags,{}};
  v::core3_operand_stack stack;stack.push({t::value_kind::i64});CHECK(stack.set_control_frame(1,false));for(unsigned i=0;i<arity;++i)stack.push(values[i]);auto size=stack.size();
  CHECK(v::validate_core3_throw(stack,0,env,ctx,false)==e::feature_disabled&&stack.size()==size);CHECK(v::validate_core3_throw(stack,1,env,ctx,true)==e::unknown_tag&&stack.size()==size);
  CHECK(v::validate_core3_throw(stack,0,env,ctx,true)==e::ok&&stack.size()==1);v::core3_operand value;CHECK(stack.pop(value)&&value.unknown); // stack-polymorphic above the preserved outer base
  CHECK(v::validate_core3_throw(stack,0,env,ctx,true)==e::ok&&stack.size()==1);
  v::core3_operand_stack underflow;CHECK(v::validate_core3_throw(underflow,0,env,ctx,true)==(arity?e::stack_underflow:e::ok));
  if(arity){v::core3_operand_stack wrong;for(unsigned i=0;i<arity;++i)wrong.push(i+1==arity?t::core_value_type{t::value_kind::f32}:values[i]);if(values[arity-1].kind==t::value_kind::f32){v::core3_operand tmp;CHECK(wrong.pop(tmp));wrong.push({t::value_kind::i64});}CHECK(v::validate_core3_throw(wrong,0,env,ctx,true)==e::operand_type_mismatch);}
 }
 for(auto heap:{t::abstract_heap_type::exn,t::abstract_heap_type::noexn,t::abstract_heap_type::func,t::abstract_heap_type::extern_,t::abstract_heap_type::any})for(bool nullable:{false,true}){
  v::core3_operand_stack stack;stack.push(ref(heap,nullable));CHECK(v::validate_core3_throw_ref(stack,ctx,false)==e::feature_disabled&&stack.size()==1);
  CHECK(v::validate_core3_throw_ref(stack,ctx,true)==((heap==t::abstract_heap_type::exn||heap==t::abstract_heap_type::noexn)?e::ok:e::operand_type_mismatch));
 }
 for(auto type:values)if(type.kind!=t::value_kind::reference){v::core3_operand_stack s;s.push(type);CHECK(v::validate_core3_throw_ref(s,ctx,true)==e::operand_type_mismatch);}
 {v::core3_operand_stack s;CHECK(v::validate_core3_throw_ref(s,ctx,true)==e::stack_underflow);s.make_unreachable();CHECK(v::validate_core3_throw_ref(s,ctx,true)==e::ok);}
 {t::sub_type invalid;invalid.results.push_back({t::value_kind::i32});t::sub_type const* tags[]{&invalid,nullptr};std::span<t::core_value_type const> label;v::core3_exception_environment env{tags,{&label,1}};
  for(unsigned i:{0,1}){v::core3_operand_stack s;CHECK(v::validate_core3_throw(s,i,env,ctx,true)==e::invalid_tag_type);CHECK(v::validate_core3_exception_catch({v::exception_catch_kind::tagged,i,0},env,ctx,true)==e::invalid_tag_type);}
  invalid.results.clear();invalid.kind=t::composite_kind::struct_;v::core3_operand_stack s;CHECK(v::validate_core3_throw(s,0,env,ctx,true)==e::invalid_tag_type);CHECK(v::validate_core3_exception_catch({static_cast<v::exception_catch_kind>(4),0,0},env,ctx,true)==e::invalid_catch_kind);
 }
 // Handler signatures allow covariant reference payloads, never inverse nullability/heap matches.
 {t::sub_type tag;tag.parameters.push_back(ref(t::abstract_heap_type::i31,false));t::sub_type const* tags[]{&tag};std::array payload{ref(t::abstract_heap_type::eq,true)};std::span<t::core_value_type const> labels[]{payload};v::core3_exception_environment env{tags,labels};CHECK(v::validate_core3_exception_catch({},env,ctx,true)==e::ok);payload[0]=ref(t::abstract_heap_type::struct_,true);CHECK(v::validate_core3_exception_catch({},env,ctx,true)==e::label_type_mismatch);}
 // Oversized metadata must fail arity before invoking either callback or adding one to SIZE_MAX.
 {auto unreachable=[](std::size_t)->t::core_value_type{std::abort();};CHECK(v::validate_exception_catch_signature(v::exception_catch_kind::tagged_ref,std::numeric_limits<std::size_t>::max(),unreachable,0,unreachable,ctx)==e::label_type_mismatch);}
 std::printf("PASS Core 3 exception signature/throw typing: %u checks\n",checks);
}
