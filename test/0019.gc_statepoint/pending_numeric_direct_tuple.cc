#include <uwvm2/runtime/exception/pending_numeric_bridge.h>
#include <uwvm2/uwvm/runtime/storage/tag_instance_identity.h>
#include <array>
#include <vector>
#include <cstring>
#include <cstdio>
#include <cstdlib>
namespace e=uwvm2::runtime::exception;namespace p=e::pending_experiment;namespace b=p::numeric_guest_bridge;using word=b::word;
unsigned checks{};void ck(bool x){++checks;if(!x)std::abort();}word addr(void const* p){return reinterpret_cast<word>(p);}
int main(){
 std::array<std::byte,16> fresh{};for(size_t j=0;j<fresh.size();++j)fresh[j]=std::byte(j*19);
 auto field=e::payload_field::numeric(e::payload_kind::v128,fresh);ck(bool(field));auto original=*field;
 ck(!field->assign_numeric_unowned(static_cast<e::payload_kind>(255),fresh));ck(field->kind()==original.kind()&&std::memcmp(field->bits().data(),original.bits().data(),16)==0);
 ck(!field->assign_numeric_unowned(e::payload_kind::i64,fresh));ck(field->kind()==original.kind()&&std::memcmp(field->bits().data(),original.bits().data(),16)==0);
 ck(!field->assign_numeric_unowned(e::payload_kind::reference,{fresh.data(),sizeof(void*)}));ck(field->kind()==original.kind());
 auto owner=std::make_shared<int>(7);auto rooted=e::payload_field::rooted_reference(owner);ck(bool(rooted));auto count=owner.use_count();auto reference_bits=std::vector<std::byte>(rooted->bits().begin(),rooted->bits().end());
 ck(!rooted->assign_numeric_unowned(e::payload_kind::i32,{fresh.data(),4}));ck(rooted->kind()==e::payload_kind::reference&&owner.use_count()==count&&std::memcmp(rooted->bits().data(),reference_bits.data(),reference_bits.size())==0);
 ck(field->assign_numeric_unowned(e::payload_kind::i32,{fresh.data(),4}));ck(field->kind()==e::payload_kind::i32&&std::memcmp(field->bits().data(),fresh.data(),4)==0);
 for(size_t fields:std::array<size_t,16>{0,1,2,4,8,16,32,64,65,96,127,128,129,192,255,256}){
 std::vector<e::payload_kind> sig(fields);std::vector<std::byte> bits;for(size_t i=0;i<fields;++i){sig[i]=std::array{e::payload_kind::i32,e::payload_kind::i64,e::payload_kind::f32,e::payload_kind::f64,e::payload_kind::v128}[i%5];auto w=e::payload_width(sig[i]);for(size_t j=0;j<w;++j)bits.push_back(std::byte((i*17+j*13)&255));}
 auto identity=std::make_shared<uwvm2::uwvm::runtime::storage::tag_instance_identity>();std::array tags{p::admitted_tag{identity,sig}};auto reg=p::prepared_registry::prepare(tags,{});ck(bool(reg));p::native_island_chain chain;p::pending_context context{reg};p::execution_island island{chain,context};ck(island.admission()==p::status::ok);b::numeric_header header{context};ck(header.activated_on_owner());std::vector<std::byte> out(bits.size());auto h=addr(&header);
 for(unsigned epoch=0;epoch<128;++epoch){ck(b::uwvm2_pending_numeric_publish_r2(h,1,addr(bits.data()),bits.size())==b::result(p::status::invalid_tag));ck(!header.pending_on_owner());ck(b::uwvm2_pending_numeric_publish_r2(h,0,addr(bits.data()),bits.size())==0);ck(header.pending_on_owner());
 p::numeric_leaf_state packed{};ck(context.borrow_numeric_state_for_bridge(packed)==p::status::ok);ck(packed.packed&&packed.fields.empty()&&packed.packed_tuple.size()==bits.size());ck(bits.empty()||std::memcmp(packed.packed_tuple.data(),bits.data(),bits.size())==0);
 unsigned roots{};auto visits=island.visit_quiescent_roots([&](p::reference) noexcept {++roots;return true;});ck(visits.result==p::status::ok&&roots==0);
 if(epoch==0){ck(b::uwvm2_pending_numeric_copy_r2(h,0,addr(out.data()),out.size())==0);ck(out==bits&&header.pending_on_owner());p::numeric_leaf_state still_packed{};ck(context.borrow_numeric_state_for_bridge(still_packed)==p::status::ok);ck(still_packed.packed&&still_packed.fields.empty());p::numeric_leaf_state legacy{};ck(context.borrow_numeric_state(legacy)==p::status::ok);ck(!legacy.packed&&legacy.fields.size()==fields);}
 ck(b::uwvm2_pending_numeric_take_r3(h,0,addr(out.data()),out.size()+1)==b::result(p::status::invalid_signature));ck(header.pending_on_owner());ck(b::uwvm2_pending_numeric_take_r3(h,0,addr(out.data()),out.size())==0);ck(out==bits&&!header.pending_on_owner());}
 }std::printf("PASS direct numeric tuples checks=%u shapes=16 epochs=128 mixed=all_numeric_kinds\n",checks);}
