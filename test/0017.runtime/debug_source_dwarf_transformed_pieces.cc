// Portable owned-carrier checks. Genuine compiler location lists are tested separately.
#include <uwvm2/uwvm/debugger/source_dwarf_pieces.h>
#include <fast_io.h>
using namespace uwvm2::uwvm::debugger::source_dwarf;
using bytes = ::std::vector<::std::byte>;
static void check(bool v,char const* message)
{ if(!v) { ::fast_io::io::perrln("transformed pieces: ",::fast_io::mnp::os_c_str(message));::fast_io::fast_terminate(); } }
static void op(bytes& e,unsigned v) { e.push_back(::std::byte{static_cast<unsigned char>(v)}); }
static void local(bytes& e,unsigned index) { for(auto v:{0xedu,0u,index}) { op(e,v); } }
static void piece(bytes& e) { for(auto v:{0x9fu,0x93u,1u}) { op(e,v); } }
static bytes expression()
{
    bytes e{};local(e,0u);piece(e);
    local(e,0u);op(e,0x38u);op(e,0x25u);piece(e);
    local(e,1u);for(auto v:{0x31u,0x1au,0x30u,0x2eu,0x10u,1u,0x1au}) { op(e,v); }piece(e);
    local(e,2u);piece(e);return e;
}
static void carrier(copied_numeric_local& out,::std::uint64_t bits,bool wide)
{
    out.available=true;out.wasm_type=wide?0x7eu:0x7fu;
    if(wide) { ::std::memcpy(out.bytes.data(),::std::addressof(bits),sizeof(bits)); }
    else { auto narrow{static_cast<::std::uint32_t>(bits)};::std::memcpy(out.bytes.data(),::std::addressof(narrow),sizeof(narrow)); }
}
int main()
{
    for(unsigned address:{4u,8u}) for(bool wide:{false,true})
    {
        auto e{expression()};auto plan{decode_location_plan(e,static_cast<::std::uint8_t>(address))};
        check(plan.kind==plan_kind::composite_value&&plan.pieces.size()==4u&&plan.composite_bits==32u,"four original finite pieces decoded");
        ::std::array<copied_numeric_local,3u> locals{};
        carrier(locals[0],0xfedc12345aa5ull,wide);carrier(locals[2],0x123480u,wide);
        for(unsigned truth{};truth!=256u;++truth)
        {
            carrier(locals[1],0x8000u+truth,wide);composite_value out{};
            check(materialize_location_pieces(plan,locals,3u,{},4u,out)==piece_query_error::none&&out.fully_available,"complete transformed copied aggregate");
            check(out.bytes==bytes{::std::byte{165},::std::byte{90},::std::byte{static_cast<unsigned char>(truth%2u)},::std::byte{128}},"native complete carrier then logical shifts, mask, ne and source truncation");
        }
        locals[1].available=false;composite_value out{};
        check(materialize_location_pieces(plan,locals,3u,{},4u,out)==piece_query_error::none&&!out.fully_available&&
              out.known_bits==bytes{::std::byte{255},::std::byte{255},::std::byte{},::std::byte{255}},"uncaptured bool remains a hole beside independent known fields");
        carrier(locals[1],255u,wide);
        auto unresolved{plan};auto& converted{unresolved.pieces[3].atom};
        converted.integer_transform_count=1u;converted.integer_transforms[0].kind=integer_transform_kind::unsigned_convert;
        converted.integer_transforms[0].operand=43u;
        check(materialize_location_pieces(unresolved,locals,3u,{},4u,out)==piece_query_error::none&&!out.fully_available&&out.known_bits[3]==::std::byte{},"unresolved conversion cannot expose raw copied bits");
        converted.integer_transforms[0].resolved=true;converted.integer_transforms[0].bit_width=8u;
        check(materialize_location_pieces(unresolved,locals,3u,{},4u,out)==piece_query_error::none&&out.fully_available&&out.bytes[3]==::std::byte{128},"resolved unsigned conversion in atom actually applied");
        locals[0].wasm_type=wide?0x7cu:0x7du;
        check(materialize_location_pieces(plan,locals,3u,{},4u,out)==piece_query_error::none&&!out.fully_available&&out.known_bits[1]==::std::byte{},"float carrier cannot execute integer shift");
        carrier(locals[0],0x12345aa5u,wide);
        auto global{plan};global.pieces[1].atom.storage=wasm_location_space::global;
        check(materialize_location_pieces(global,locals,3u,{},4u,out)==piece_query_error::none&&out.known_bits[1]==::std::byte{},"no global read authority from transforms");
        auto shift{plan};shift.pieces[1].atom.integer_transforms[0].operand=wide?64u:32u;
        check(materialize_location_pieces(shift,locals,3u,{},4u,out)==piece_query_error::none&&out.known_bits[1]==::std::byte{},"carrier-width shift does not invoke C++ undefined behavior");
        // bit_piece is sliced AFTER the complete integer transform.
        bytes bit{};local(bit,0u);for(auto v:{0x38u,0x25u,0x9fu,0x9du,4u,4u}) { op(bit,v); }
        auto bitplan{decode_location_plan(bit,static_cast<::std::uint8_t>(address))};
        check(materialize_location_pieces(bitplan,locals,3u,{},1u,out)==piece_query_error::none&&out.bytes[0]==::std::byte{5}&&out.known_bits[0]==::std::byte{15},"source bit offset belongs to transformed value");
    }
    bytes rejected{};local(rejected,0u);op(rejected,0x31u);op(rejected,0x2eu);piece(rejected);
    check(decode_location_plan(rejected,4u).kind==plan_kind::unavailable,"arbitrary comparison is outside finite nonzero operation");
    rejected.clear();local(rejected,0u);for(unsigned i{};i!=5u;++i){op(rejected,0x31u);op(rejected,0x1au);}piece(rejected);
    check(decode_location_plan(rejected,4u).reason==unavailable_reason::expression_limit,"transform count independently bounded");
    bytes truncated{};local(truncated,0u);op(truncated,0x10u);op(truncated,0x80u);
    check(decode_location_plan(truncated,4u).reason==unavailable_reason::malformed_expression,"truncated operand not silently ignored");
    bytes converted{};local(converted,0u);for(auto v:{0xa8u,43u,0xa8u,47u}){op(converted,v);}piece(converted);
    check(decode_location_plan(converted,4u).pieces[0].atom.integer_transform_count==2u,"DWARF5 convert tails preserved separately in atom");
    ::fast_io::io::println("PASS portable transformed pieces: shift, boolean mask/ne, converts, holes, bit offsets and denied carriers");
}
