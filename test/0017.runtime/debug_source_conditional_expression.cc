// Finite arithmetic syntax/type/value DATA; no guest pause or memory authority.
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
#include <uwvm2/uwvm/debugger/command.h>
#include <uwvm2/uwvm/debugger/source_language_expression.h>
#include <fast_io.h>
namespace scalar=uwvm2::uwvm::debugger::source_scalar_expression;
namespace dwarf=uwvm2::uwvm::debugger::source_dwarf;
static void check(bool value,char const* why)
{ if(!value) { ::fast_io::io::perrln("conditional expression FAIL: ",::fast_io::mnp::os_c_str(why)); ::fast_io::fast_terminate(); } }
int main()
{
    ::std::size_t reads{},type_queries{},dead_reads{};
    auto const types{[&](dwarf::source_expression const& leaf,scalar::integer& out)
    {
        ++type_queries; if(!leaf.steps.empty()) { return false; }
        if(leaf.root_name=="value" || leaf.root_name=="dead") { out={0u,32u,false};return true; }
        if(leaf.root_name=="wide") { out={0u,64u,false};return true; }
        if(leaf.root_name=="real") { out={0u,32u,false,true};return true; }
        if(leaf.root_name=="mismatch") { out={0u,32u,false};return true; }
        return false;
    }};
    auto const values{[&](dwarf::source_expression const& leaf,bool size,scalar::integer& out)
    {
        ++reads;
        if(leaf.root_name=="dead") { ++dead_reads;return false; }
        if(leaf.root_name=="value") { out={size ? 4u : 7u,32u,size};return true; }
        if(leaf.root_name=="wide") { out={size ? 8u : 99u,64u,size};return true; }
        if(leaf.root_name=="real") { out={::std::bit_cast<::std::uint32_t>(3.5f),32u,false,true};return !size; }
        if(leaf.root_name=="mismatch") { out={7u,64u,false};return true; }
        return false;
    }};
    auto const verify{[&](::std::string_view text,scalar::integer expected,::std::size_t expected_reads,unsigned guest=32u)
    {
        scalar::program code{};scalar::integer result{};
        check(scalar::parse(text,code)==scalar::error::none,"parse");auto const before{reads};
        check(scalar::evaluate(code,values,result,guest,types)==scalar::error::none,"evaluate");
        check(result.bits==expected.bits && result.width==expected.width && result.unsigned_value==expected.unsigned_value &&
              result.floating==expected.floating && reads-before==expected_reads,"value/type/read count");
        check(dead_reads==0u,"unselected values are never read");
    }};
    verify("1 ? 42 : 1 / 0",{42u,32u,false},0u);
    verify("0 ? 1 / 0 : 7",{7u,32u,false},0u);
    verify("1 ? 7 : 2147483647 + 1",{7u,32u,false},0u);
    verify("1 ? 0 ? 2 : 3 : 4",{3u,32u,false},0u);
    verify("0 ? 1 : 0 ? 2 : 3",{3u,32u,false},0u);
    verify("0 || 1 ? 2 + 3 * 4 : 5",{14u,32u,false},0u);
    verify("1 + (0 ? 2 : 3) * 4",{13u,32u,false},0u);
    verify("1 ? -1 : 0u",{0xffffffffu,32u,true},0u);
    verify("0 ? 0u : -1",{0xffffffffu,32u,true},0u);
    verify("1 ? -1LL : 0u",{~::std::uint64_t{},64u,false},0u);
    verify("1 ? 1L : 0u",{1u,32u,true},0u,32u);
    verify("1 ? 1L : 0u",{1u,64u,false},0u,64u);
    verify("1 ? 7 : 0.0",{::std::bit_cast<::std::uint64_t>(7.0),64u,false,true},0u);
    verify("1 ? 7 : 0.0f",{::std::bit_cast<::std::uint32_t>(7.0f),32u,false,true},0u);
    verify("!-0.0 ? -0.0f : 1.0f",{::std::bit_cast<::std::uint32_t>(-0.0f),32u,false,true},0u);
    verify("1 ? '\\n' : ':'",{10u,32u,false},0u);
    verify("value > 0 ? value : dead",{7u,32u,false},2u);
    verify("0 ? dead / 0 : value + 1",{8u,32u,false},1u);
    verify("1 ? value : wide",{7u,64u,false},1u);
    verify("0 ? real : value",{::std::bit_cast<::std::uint32_t>(7.0f),32u,false,true},1u);
    verify("1 ? value : sizeof(dead)",{7u,32u,true},1u);
    verify("value > 0 ? (true ? value : 2) : dead",{7u,32u,false},2u);
    for(auto text : {"1 ? 1/0 : 2","0 ? 2 : 1/0"})
    { scalar::program code{};scalar::integer result{};check(scalar::parse(text,code)==scalar::error::none,"selected arithmetic parse");
      check(scalar::evaluate(code,values,result,32u,types)==scalar::error::arithmetic && result.bits==0u,"selected arithmetic error remains"); }
    for(auto text : {"1 ? value : absent","1 ? mismatch : 0","1 ? value : sizeof(absent)"})
    { scalar::program code{};scalar::integer result{};check(scalar::parse(text,code)==scalar::error::none,"missing/mismatched metadata parse");
      check(scalar::evaluate(code,values,result,32u,types)==scalar::error::unavailable && result.bits==0u,"no guessed result type"); }
    for(auto text : {"1 ? (char)1 : (char)2","1 ? true : false","1 ? 42 : 1.0 & 1"})
    { scalar::program code{};scalar::integer result{};check(scalar::parse(text,code)==scalar::error::none,"unsupported numeric category parse");
      auto const before{reads};check(scalar::evaluate(code,values,result,32u,types)==scalar::error::unsupported && reads==before,"unsupported type before values"); }
    for(auto text : {"1 ?","1 ? 2", "1 ? : 2", "1 ? 2 :", "1 ? 2 : 3 : 4", "1 ?: 2", "1 ? value++ : 2",
                    "1 ? 2 : run()", "1 ? 2 : value = 8", "1 ? 2 : &value", "1 ? 2 : (int*)value", "1 ? 2,3 : 4"})
    { scalar::program code{};check(scalar::parse(text,code)!=scalar::error::none && code.nodes.empty(),"both arms must be bounded read-only syntax"); }
    {
        scalar::program code{};scalar::integer result{};check(scalar::parse("1 ? value : 0",code)==scalar::error::none,"legacy API syntax");
        auto const before{reads};check(scalar::evaluate(code,values,result)==scalar::error::unavailable && reads==before,"no value-resolver fallback for missing type resolver");
    }
    {
        // A fabricated shared-node DATA graph cannot amplify type work beyond
        // the 4096 visits. Production parses an owned tree with <=128 nodes.
        scalar::program code{};scalar::node literal{};literal.op=scalar::operation::literal;literal.literal=1u;code.nodes.push_back(literal);
        for(::std::size_t i{1u};i!=18u;++i)
        { scalar::node n{};n.op=scalar::operation::add;n.lhs=i-1u;n.rhs=i-1u;code.nodes.push_back(n); }
        scalar::node n{};n.op=scalar::operation::conditional;n.lhs=0u;n.rhs=17u;n.third=0u;code.nodes.push_back(n);code.root=18u;
        scalar::integer result{};auto const before{reads};check(scalar::evaluate(code,values,result,32u,types)==scalar::error::limit_exceeded && reads==before,"bounded type traversal");
    }
    for(auto text : {"print 1 41 value > 0 ? value : 0", "print-frame 1 41 2 1 ? 7 : 1/0"})
    { auto const cmd{uwvm2::uwvm::debugger::parse_console_command(::fast_io::string_view{::fast_io::mnp::os_c_str(text)})};
      check(cmd.kind==uwvm2::uwvm::debugger::console_command_kind::source_value,"original authenticated expression command"); }
    {
        // Real production type traversal; no guest bytes or memory reader are
        // even available to this metadata callback. The dead pointer can be
        // null or unavailable without changing its DWARF pointee type.
        for(::std::uint8_t guest_bytes : {4u,8u})
        {
            ::std::vector<dwarf::type_record> types(2u);
            types[0u].kind=dwarf::type_kind::scalar;types[0u].encoding=5u;
            types[0u].byte_count=4u;types[0u].byte_size=4u;types[0u].size_known=true;
            types[1u].kind=dwarf::type_kind::pointer;types[1u].referenced_type=0u;
            types[1u].byte_count=guest_bytes;types[1u].byte_size=guest_bytes;types[1u].size_known=true;
            auto metadata{[&](dwarf::source_expression const& leaf,scalar::integer& out)
            {
                if(leaf.root_name!="nullp") { return false; }
                ::std::vector<dwarf::object_node> layout{};
                if(uwvm2::uwvm::debugger::source_language_expression::type(types,1u,leaf.steps,guest_bytes,layout)!=
                    dwarf::inline_query_error::none || layout.size()!=1u || layout[0u].kind!=dwarf::type_kind::scalar ||
                    layout[0u].scalar_kind!=dwarf::numeric_kind::signed_integer || layout[0u].scalar_bytes!=4u) { return false; }
                out={0u,32u,false};return true;
            }};
            scalar::program code{};scalar::integer result{};auto const before{reads};
            check(scalar::parse("1 ? 42 : *nullp",code)==scalar::error::none,"dead dereference grammar");
            check(scalar::evaluate(code,values,result,guest_bytes*8u,metadata)==scalar::error::none &&
                result.bits==42u && result.width==32u && reads==before,"type traversal cannot read dead pointer");
        }
    }
    for(unsigned guest_bits : {32u,64u})
    {
        // Real type traversal for aggregates, pointer extents and a bit-field.
        // Neither a guest memory reader nor object value bytes exist here.
        ::std::vector<dwarf::type_record> metadata_types(3u);
        auto& number{metadata_types[0u]};number.kind=dwarf::type_kind::scalar;
        number.encoding=7u;number.byte_count=4u;number.byte_size=4u;number.size_known=true;
        auto& packet{metadata_types[1u]};packet.kind=dwarf::type_kind::structure;packet.byte_size=28u;packet.size_known=true;
        dwarf::member_record member{};member.name="field";member.type=0u;member.offset_known=true;packet.members.push_back(member);
        member.name="flags";member.bit_field=true;member.bit_size=3u;member.data_bit_offset=32u;member.byte_offset=4u;packet.members.push_back(member);
        auto& pointer{metadata_types[2u]};pointer.kind=dwarf::type_kind::pointer;pointer.referenced_type=1u;
        pointer.byte_count=guest_bits/8u;pointer.byte_size=guest_bits/8u;pointer.size_known=true;
        auto metadata{[&](dwarf::source_expression const& leaf,bool size,scalar::integer& out)
        {
            if(leaf.root_name!="packet" && leaf.root_name!="nullp") { return false; }
            ::std::vector<dwarf::object_node> layout{};
            if(uwvm2::uwvm::debugger::source_language_expression::type(metadata_types,leaf.root_name=="packet" ? 1u : 2u,
                leaf.steps,static_cast<::std::uint8_t>(guest_bits/8u),layout)!=dwarf::inline_query_error::none || layout.empty()) { return false; }
            auto const& node{layout[0u]};
            if(node.reason!=dwarf::object_unavailable_reason::none || node.bit_field) { return false; }
            if(size) { out={node.byte_size,guest_bits,true};return true; }
            if(layout.size()!=1u || node.kind!=dwarf::type_kind::scalar || node.scalar_bytes!=4u) { return false; }
            out={0u,32u,true};return true;
        }};
        for(auto text : {"1 ? 7 : sizeof(packet)","1 ? 7 : sizeof(nullp)","1 ? 7 : sizeof(*nullp)",
                        "1 ? 7 : sizeof(packet.field)","1 ? 7 : (0 ? 1 : sizeof(*nullp))"})
        {
            scalar::program code{};scalar::integer result{};auto const before{reads};
            check(scalar::parse(text,code)==scalar::error::none,"aggregate sizeof grammar");
            check(scalar::evaluate(code,values,result,guest_bits,metadata)==scalar::error::none &&
                result.bits==7u && result.width==guest_bits && result.unsigned_value && reads==before,"dead sizeof uses extent metadata only");
        }
        for(auto text : {"1 ? 7 : sizeof(packet.flags)","1 ? 7 : packet","1 ? 7 : nullp","1 ? 7 : sizeof(absent)"})
        {
            scalar::program code{};scalar::integer result{};auto const before{reads};
            check(scalar::parse(text,code)==scalar::error::none,"unsupported sizeof operand grammar");
            check(scalar::evaluate(code,values,result,guest_bits,metadata)==scalar::error::unavailable && reads==before,
                "no guessed bit-field, aggregate value, pointer arithmetic or missing declaration");
        }
        for(auto text : {"0 ? 7 : sizeof(packet)","0 ? 7 : sizeof(nullp)","0 ? 7 : sizeof(*nullp)"})
        {
            ::std::size_t size_queries{};
            auto selected_size{[&](dwarf::source_expression const& leaf,bool size,scalar::integer& out)
            { ++size_queries;return size && metadata(leaf,true,out); }};
            scalar::program code{};scalar::integer result{};
            check(scalar::parse(text,code)==scalar::error::none,"selected sizeof grammar");
            check(scalar::evaluate(code,selected_size,result,guest_bits,metadata)==scalar::error::none && size_queries==1u &&
                result.bits==(text==::std::string_view{"0 ? 7 : sizeof(nullp)"} ? guest_bits/8u : 28u) &&
                result.width==guest_bits && result.unsigned_value,"selected sizeof retains guest extent and width");
        }
        scalar::program code{};scalar::integer result{};check(scalar::parse("1 ? 7 : sizeof(packet)",code)==scalar::error::none,"bad metadata syntax");
        auto wrong_size_type{[](dwarf::source_expression const&,bool,scalar::integer& out) { out={28u,32u,false};return true; }};
        check(scalar::evaluate(code,values,result,guest_bits,wrong_size_type)==scalar::error::unavailable,"sizeof metadata must be guest size_t");
    }
    check(type_queries>0u,"metadata-only resolver exercised");
    ::fast_io::io::println("debug_source_conditional_expression: PASS");
}
