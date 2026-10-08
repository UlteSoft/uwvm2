// Component metadata/grammar equivalence only; initialized-module mode parity is tested by the WAT corpus.
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <fast_io.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate.h>
namespace d = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
namespace v = ::uwvm2::validation::standard::wasm3;
namespace s = ::uwvm2::uwvm::runtime::storage;
namespace
{
    [[nodiscard]] bool same(d::runtime_block_signature_type const& a, d::runtime_block_signature_type const& b) noexcept
    {
        return a.params.begin==b.params.begin && a.params.end==b.params.end &&
               a.results.begin==b.results.begin && a.results.end==b.results.end &&
               a.type_index==b.type_index && a.singleton_result_witness==b.singleton_result_witness &&
               a.has_singleton_result_core_type==b.has_singleton_result_core_type &&
               a.singleton_result_core_type==b.singleton_result_core_type &&
               a.singleton_result_core_type.source_prefix==b.singleton_result_core_type.source_prefix;
    }
    [[nodiscard]] bool reference(::std::initializer_list<::std::uint8_t> bytes, s::wasm_module_storage_t const& module) noexcept
    {
        auto const* const begin{reinterpret_cast<::std::byte const*>(bytes.begin())};
        // [begin, begin+bytes.size()) is the synchronous initializer-list-owned buffer.
        // [safe                    ] unsafe (one-past)
        // ^^ first/raw cursor      ^^ end: the extent is checked before this endpoint construction.
        auto const* const end{begin+bytes.size()};
        auto first{begin}; auto raw{begin};
        v::recursive_type_context const empty{};
        auto const* const context{module.type_section_storage.core3_context_ptr};
        auto const decoded{v::scan_core3_value_carrier(first,end,true,d::get_runtime_type_section_count(module),
            context==nullptr?empty:*context,true,true)};
        d::runtime_block_signature_type a{},b{};
        return decoded.error==v::value_carrier_error::ok && first==end &&
               d::resolve_decoded_reference_block_signature(decoded.carrier,decoded.type,module,a) &&
               d::parse_wasm_block_signature_type(raw,end,module,b) && raw==end && same(a,b);
    }
    [[nodiscard]] bool malformed(::std::initializer_list<::std::uint8_t> bytes, s::wasm_module_storage_t const& module,
                                  ::std::size_t expected_consumed) noexcept
    {
        auto const* const begin{reinterpret_cast<::std::byte const*>(bytes.begin())};
        // Own the complete byte span for this call; endpoint addition stays within its checked extent.
        // [safe bytes ...] unsafe (one-past)
        // ^^ cursor       ^^ end
        auto cursor{begin}; auto const* const end{begin+bytes.size()};
        d::runtime_block_signature_type signature{}; signature.type_index=91uz;
        auto const before{signature};
        return !d::parse_wasm_block_signature_type(cursor,end,module,signature) &&
               static_cast<::std::size_t>(cursor-begin)==expected_consumed && same(signature,before);
    }
}
int main()
{
    ::std::size_t checks{};
    auto check{[&](bool value) noexcept { ++checks; return value; }};
    s::wasm_module_storage_t module{};
    constexpr auto no_index{(::std::numeric_limits<::std::size_t>::max)()};
    for(auto code : {-64,-1,-2,-3,-4,-5,-16,-17})
    {
        d::runtime_block_signature_type decoded{}; decoded.has_singleton_result_core_type=true; decoded.type_index=91uz;
        ::std::uint8_t byte{static_cast<::std::uint8_t>(code&0x7f)};
        auto const* const begin{reinterpret_cast<::std::byte const*>(::std::addressof(byte))}; auto raw{begin};
        // The scalar byte lives for this iteration. begin+1 is its checked one-past endpoint.
        // [safe byte] unsafe (one-past)
        // ^^ raw      ^^ end
        auto const* const end{begin+1uz}; d::runtime_block_signature_type scanned{};
        if(!check(d::resolve_decoded_block_signature(code,module,decoded) &&
                  d::parse_wasm_block_signature_type(raw,end,module,scanned) && raw==end && same(decoded,scanned) &&
                  decoded.has_singleton_result_core_type==(code==-16 || code==-17) && decoded.type_index==no_index)) { return 1; }
    }
    for(auto prefix : {0x69u,0x6au,0x6bu,0x6cu,0x6du,0x6eu,0x6fu,0x70u,0x71u,0x72u,0x73u,0x74u})
    { if(!check(reference({static_cast<::std::uint8_t>(prefix)},module))) { return 2; } }
    if(!check(reference({0x63u,0x69u},module)) || !check(reference({0x64u,0x6bu},module)) ||
       !check(reference({0x64u,0x70u},module)) || !check(reference({0x63u,0x74u},module))) { return 3; }
    // Synthetic owned tuples test resolver bounds, not initialized-module execution permission.
    s::wasm_binfmt1_final_function_type_t types[3]{};
    d::runtime_operand_stack_value_type params[]{d::runtime_operand_stack_value_type::i32};
    d::runtime_operand_stack_value_type results[]{d::runtime_operand_stack_value_type::i64,d::runtime_operand_stack_value_type::i32};
    // Checked local arrays own every endpoint for main's full lifetime; +N is exactly their one-past endpoint.
    // [types x3], [params x1], [results x2] safe allocations; endpoint borrows never outlive this function.
    types[1].parameter={params,params+1uz}; types[1].result={results,results+2uz};
    module.type_section_storage.type_section_begin=types;
    module.type_section_storage.type_section_end=types+3uz;
    module.type_section_storage.type_section_count=3uz;
    if(!check(reference({0x63u,0x01u},module)) || !check(reference({0x64u,0x81u,0x00u},module))) { return 4; }
    v::recursive_type_context context{}; context.records.resize(3uz);
    context.records.index_unchecked(0uz).kind=t::composite_kind::struct_;
    context.records.index_unchecked(1uz).kind=t::composite_kind::function;
    context.records.index_unchecked(2uz).kind=t::composite_kind::array;
    // [context] is owned for main's lifetime; borrow immutable records only after each helper's contains() proof.
    module.type_section_storage.core3_context_ptr=::std::addressof(context);
    module.type_section_storage.requires_gc=true;
    if(!check(reference({0x64u,0x00u},module)) || !check(reference({0x63u,0x01u},module)) ||
       !check(reference({0x63u,0x02u},module))) { return 5; }
    d::runtime_block_signature_type signature{};
    signature.has_singleton_result_core_type=true; signature.singleton_result_witness=91uz;
    if(!check(d::resolve_decoded_block_signature(1,module,signature) && signature.params.begin==params &&
              signature.params.end==params+1uz && signature.results.begin==results && signature.results.end==results+2uz &&
              signature.type_index==1uz && !signature.has_singleton_result_core_type && signature.singleton_result_witness==no_index)) { return 6; }
    auto const saved{signature};
    for(auto index : {0ll,2ll,3ll,-6ll,0x1'0000'0000ll})
    { if(!check(!d::resolve_decoded_block_signature(index,module,signature) && same(signature,saved))) { return 7; } }
    t::core_value_type value{.kind=t::value_kind::reference,.heap={3},.nullable=true,.source_prefix=0x63u};
    if(!check(!d::resolve_decoded_reference_block_signature(0x70u,value,module,signature) && same(signature,saved))) { return 8; }
    value.heap.code=0;
    if(!check(!d::resolve_decoded_reference_block_signature(0x6fu,value,module,signature) && same(signature,saved))) { return 9; }
    value.kind=t::value_kind::i32;
    if(!check(!d::resolve_decoded_reference_block_signature(0x70u,value,module,signature) && same(signature,saved))) { return 10; }
    if(!check(malformed({0x63u},module,0uz)) || !check(malformed({0x64u,0x80u},module,0uz)) ||
       !check(malformed({0x80u},module,0uz)) || !check(malformed({0xffu,0x7fu},module,2uz)) ||
       !check(malformed({0x80u,0x80u,0x80u,0x80u,0x80u,0x00u},module,6uz)) ||
       !check(malformed({0x00u},module,1uz)) || !check(malformed({0x03u},module,1uz))) { return 11; }
    ::fast_io::println("Core3 decoded block signature checks=",checks);
}
