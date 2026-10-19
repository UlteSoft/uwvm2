#pragma once
#include "contract.h"
#include <algorithm>
#include <bit>
#include <cstring>
#include <limits>
#include <vector>
#include <uwvm2/uwvm/runtime/storage/impl.h>
#include <uwvm2/validation/error/impl.h>
#include <uwvm2/validation/standard/wasm3/reference_policy.h>
#include <uwvm2/validation/standard/wasm3/exception_immediate.h>

#if defined(__linux__) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS) && !defined(UWVM_MODULE)
namespace uwvm2::runtime::compiler::uwvm_int::threaded
{
    namespace storage = ::uwvm2::uwvm::runtime::storage;
    using signature = storage::wasm_binfmt1_final_function_type_t;
    using value_type = storage::wasm_binfmt1_final_value_type_t;
    namespace core = ::uwvm2::parser::wasm::standard::wasm3::type;
    struct catch_clause { unsigned kind{}; ::std::uint32_t tag{}, label{}; };
    struct instruction
    {
        unsigned opcode{};
        ::std::uint64_t immediate{};
        ::std::size_t offset{}, target{}, alternate{}, parameters{}, results{};
        ::std::vector<::std::uint32_t> labels{};
        ::std::vector<catch_clause> catches{};
    };
    struct function_program
    {
        signature const* type{};
        ::std::size_t locals{};
        ::std::vector<instruction> instructions{};
    };
    struct module_program
    {
        storage::wasm_module_storage_t const* instance{};
        ::std::vector<function_program> functions{};
    };
    namespace detail
    {
        [[nodiscard]] inline bool numeric_type(value_type t) noexcept
        { return t==value_type::i32 || t==value_type::i64 || t==value_type::f32 || t==value_type::f64; }
        [[nodiscard]] inline bool value_type_supported(core::core_value_type t) noexcept
        {
            return t.kind<=core::value_kind::f64 || (t.kind==core::value_kind::reference &&
                (t.heap.code==static_cast<::std::int64_t>(core::abstract_heap_type::exn) ||
                 t.heap.code==static_cast<::std::int64_t>(core::abstract_heap_type::noexn)));
        }
        [[nodiscard]] inline bool carrier_type(value_type t) noexcept
        { return numeric_type(t) || t==value_type::externref; }
        [[nodiscard]] inline ::std::size_t abi_bytes(value_type t) noexcept
        { return t==value_type::i32 || t==value_type::f32 ? 4uz : 8uz; }
        [[nodiscard]] inline bool executable_signature(signature const* t) noexcept
        {
            if(!t) { return false; }
            for(auto i{t->parameter.begin}; i != t->parameter.end; ++i) { if(!carrier_type(*i)) { return false; } }
            for(auto i{t->result.begin}; i != t->result.end; ++i) { if(!carrier_type(*i)) { return false; } }
            return true;
        }
        [[nodiscard]] inline bool equal_signature(signature const* a, signature const* b) noexcept
        {
            if(!executable_signature(a) || !executable_signature(b)) { return false; }
            for(auto i{a->parameter.begin};i!=a->parameter.end;++i) { if(!numeric_type(*i)) { return false; } }
            for(auto i{a->result.begin};i!=a->result.end;++i) { if(!numeric_type(*i)) { return false; } }
            for(auto i{b->parameter.begin};i!=b->parameter.end;++i) { if(!numeric_type(*i)) { return false; } }
            for(auto i{b->result.begin};i!=b->result.end;++i) { if(!numeric_type(*i)) { return false; } }
            return (a->parameter.end-a->parameter.begin)==(b->parameter.end-b->parameter.begin) &&
                   (a->result.end-a->result.begin)==(b->result.end-b->result.begin) &&
                   ::std::equal(a->parameter.begin,a->parameter.end,b->parameter.begin) &&
                   ::std::equal(a->result.begin,a->result.end,b->result.begin);
        }
        struct source_cursor
        {
            ::std::byte const* begin{};
            ::std::byte const* current{};
            ::std::byte const* end{};
            bool valid{true};
            [[nodiscard]] unsigned byte() noexcept
            {
                if(current==end) { valid=false;return 0; }
                return ::std::to_integer<unsigned>(*current++);
            }
            [[nodiscard]] ::std::uint64_t leb(unsigned width, bool signed_value=false) noexcept
            {
                ::std::uint64_t out{};unsigned shift{}, last{};
                for(unsigned n{}; n != (width+6u)/7u; ++n)
                {
                    last=byte();if(!valid) { return 0; }
                    unsigned const available{width-shift};
                    unsigned const mask{available>=7u?127u:((1u<<available)-1u)};
                    unsigned const unused{(last&127u)&~mask};
                    if(available<7u && ((!signed_value && unused) ||
                        (signed_value && unused!=0u && unused!=(127u&~mask)))) { valid=false;return 0; }
                    out|=static_cast<::std::uint64_t>(last&mask)<<shift;
                    shift+=available<7u?available:7u;
                    if(!(last&128u))
                    {
                        if(signed_value && shift<64u && (last&64u)) { out|=(~::std::uint64_t{})<<shift; }
                        return out;
                    }
                }
                valid=false;return 0;
            }
        };
        [[nodiscard]] inline bool supported_plain(unsigned op) noexcept
        {
            return op<=0x05u || op==0x08u || op==0x0au || (op>=0x0bu&&op<=0x11u) || (op>=0x1au && op<=0x1cu) || op==0x1fu || (op>=0x20u && op<=0x24u) ||
                   (op>=0x28u && op<=0x40u) || (op>=0x41u && op<=0xc4u) ||
                   op==0xd0u || op==0xd1u || (op>=0xd4u && op<=0xd6u);
        }
        // The caller retains a nonforgeable World/source validation certificate
        // from the actual immutable validated file and unchanged feature object.
        // Checked lowering still rejects every unsupported source shape.
        [[nodiscard]] inline status lower(storage::wasm_module_storage_t const& module,
            storage::local_defined_function_storage_t const& source, function_program& out,
            limits const& budget, ::std::size_t& charged_bytes, ::std::uintptr_t source_begin)
        {
            out.type=source.function_type_ptr;
            if(!executable_signature(out.type) || !source.wasm_code_ptr) { return status::unsupported_profile; }
            out.locals=static_cast<::std::size_t>(out.type->parameter.end-out.type->parameter.begin);
            if(out.locals>budget.values) { return status::resource_limit; }
            for(auto const& run: source.wasm_code_ptr->locals)
            {
                if(!(run.has_core_type ? value_type_supported(run.core_type) : numeric_type(run.type)) || run.count>budget.values-out.locals) { return status::unsupported_profile; }
                out.locals+=run.count;
            }
            auto const& body{source.wasm_code_ptr->body};
            source_cursor input{reinterpret_cast<::std::byte const*>(body.expr_begin),
                reinterpret_cast<::std::byte const*>(body.expr_begin),reinterpret_cast<::std::byte const*>(body.code_end)};
            auto const bytes{static_cast<::std::size_t>(input.end-input.begin)};
            // Conservative exact-capacity reserve. No vector growth can exceed
            // the charged instruction bytes while lowering this function.
            if(charged_bytes>budget.instruction_bytes || bytes>(budget.instruction_bytes-charged_bytes)/sizeof(instruction))
            { return status::resource_limit; }
            charged_bytes+=bytes*sizeof(instruction);out.instructions.reserve(bytes);
            ::std::vector<::std::size_t> nesting{};nesting.reserve(::std::min(bytes,budget.controls));
            while(input.current!=input.end && input.valid)
            {
                instruction ins{};ins.offset=static_cast<::std::size_t>(reinterpret_cast<::std::uintptr_t>(input.current)-source_begin);ins.opcode=input.byte();
                auto const position{out.instructions.size()};
                auto const op{ins.opcode};
                if(op==2u || op==3u || op==4u || op==0x1fu)
                {
                    auto const block{static_cast<::std::int64_t>(input.leb(33u,true))};
                    if(block==-64) { }
                    else if(block>=-4 && block<=-1) { ins.results=1; }
                    else if(block==-23 || block==-12) { ins.results=1; }
                    else if(block==-29 || block==-28)
                    {
                        auto const heap{static_cast<::std::int64_t>(input.leb(33u,true))};
                        if(heap!=-23 && heap!=-12) { return status::unsupported_profile; }
                        ins.results=1;
                    }
                    else if(block>=0 && static_cast<::std::size_t>(block)<module.type_section_storage.type_section_count)
                    {
                        auto const* t{module.type_section_storage.type_section_begin+block};
                        if(!executable_signature(t)) { return status::unsupported_profile; }
                        ins.parameters=static_cast<::std::size_t>(t->parameter.end-t->parameter.begin);
                        ins.results=static_cast<::std::size_t>(t->result.end-t->result.begin);
                    }
                    else { return status::unsupported_profile; }
                    if(op==0x1fu)
                    {
                        auto const count{input.leb(32u)};
                        if(count>budget.controls || count>(budget.instruction_bytes-charged_bytes)/sizeof(catch_clause)) { return status::resource_limit; }
                        charged_bytes+=count*sizeof(catch_clause);ins.catches.reserve(count);
                        for(::std::uint64_t n{};n<count;++n)
                        {
                            catch_clause clause{};clause.kind=input.byte();if(clause.kind>3u) { return status::validation_failed; }
                            if(clause.kind<2u) { clause.tag=static_cast<::std::uint32_t>(input.leb(32u)); }
                            clause.label=static_cast<::std::uint32_t>(input.leb(32u));ins.catches.push_back(clause);
                        }
                    }
                    if(nesting.size()==budget.controls) { return status::resource_limit; }
                    nesting.push_back(position);
                }
                else if(op==5u)
                {
                    if(nesting.empty() || out.instructions[nesting.back()].opcode!=4u) { return status::validation_failed; }
                    out.instructions[nesting.back()].alternate=position+1uz;
                    ins.target=nesting.back();
                }
                else if(op==0x0bu)
                {
                    if(!nesting.empty())
                    {
                        auto const block{nesting.back()};nesting.pop_back();out.instructions[block].target=position;
                        if(!out.instructions[block].alternate) { out.instructions[block].alternate=position; }
                        if(out.instructions[block].opcode==4u && out.instructions[block].alternate!=position)
                        { out.instructions[out.instructions[block].alternate-1uz].target=position; }
                    }
                    else if(input.current!=input.end) { return status::validation_failed; }
                }
                else if(op==0x0eu)
                {
                    auto const n{input.leb(32u)};
                    if(n>budget.controls || n+1u>(budget.instruction_bytes-charged_bytes)/sizeof(::std::uint32_t))
                    { return status::resource_limit; }
                    charged_bytes+=(n+1u)*sizeof(::std::uint32_t);ins.labels.reserve(n+1u);
                    for(::std::uint64_t i{};i<=n;++i) { ins.labels.push_back(static_cast<::std::uint32_t>(input.leb(32u))); }
                }
                else if(op==0x0cu || op==0x0du || op==0x10u || op==0x08u || (op>=0x20u&&op<=0x24u) || (op==0x3fu||op==0x40u) || op==0xd5u || op==0xd6u)
                { ins.immediate=input.leb(32u); }
                else if(op==0x11u) { ins.immediate=input.leb(32u);ins.target=input.leb(32u); }
                else if(op==0x41u || op==0x42u) { ins.immediate=input.leb(op==0x41u?32u:64u,true); }
                else if(op==0x43u || op==0x44u)
                { for(unsigned n{};n!=(op==0x43u?4u:8u);++n) { ins.immediate|=static_cast<::std::uint64_t>(input.byte())<<(8u*n); } }
                else if(op==0x1cu)
                {
                    if(input.leb(32u)!=1u) { return status::validation_failed; }
                    auto const t{static_cast<::std::int64_t>(input.leb(33u,true))};
                    if(t==-29 || t==-28)
                    { auto const heap{static_cast<::std::int64_t>(input.leb(33u,true))};if(heap!=-23&&heap!=-12) { return status::unsupported_profile; } }
                    else if((t<-4||t>-1)&&t!=-23&&t!=-12) { return status::unsupported_profile; }
                }
                else if(op==0xd0u)
                { ins.immediate=input.leb(33u,true);if(static_cast<::std::int64_t>(ins.immediate)!=-23 && static_cast<::std::int64_t>(ins.immediate)!=-12) { return status::unsupported_profile; } }
                else if(op==0xfcu)
                {
                    auto const sub{input.leb(32u)};
                    if(sub<=7u) { ins.opcode=0x200u+static_cast<unsigned>(sub); }
                    else if(sub<=11u)
                    {
                        ins.opcode=0x300u+static_cast<unsigned>(sub);
                        ins.immediate=input.leb(32u);
                        if(sub==8u || sub==10u) { ins.target=input.leb(32u); }
                    }
                    else { return status::unsupported_profile; }
                }
                else if((op>=0x28u&&op<=0x3eu) || op==0xfeu)
                {
                    if(op==0xfeu)
                    {
                        auto const sub{input.leb(32u)};
                        if(sub>0x4eu || (sub>3u&&sub<0x10u)) { return status::unsupported_profile; }
                        ins.opcode=0x100u+static_cast<unsigned>(sub);
                        if(sub==3u) { if(input.byte()!=0u) { return status::validation_failed; } }
                    }
                    if(ins.opcode!=0x103u)
                    {
                        auto const alignment{input.leb(32u)};
                        // This finite profile is wasm32, one imported shared
                        // memory. Multi-memory/memory64 are not silently erased.
                        if(alignment&~63u) { return status::unsupported_profile; }
                        ins.immediate=input.leb(32u);
                    }
                }
                else if(!supported_plain(op)) { return status::unsupported_profile; }
                // GC/SIMD, call_ref/tail calls and bulk table remain cold
                // unsupported. Atomic subcodes and numeric opcodes are decoded
                // exactly; no guest feature defaults are rewritten.
                out.instructions.push_back(::std::move(ins));
            }
            return input.valid && nesting.empty() && !out.instructions.empty() &&
                out.instructions.back().opcode==0x0bu ? status::ok : status::validation_failed;
        }
    }
}

#endif // Linux native-thread, C++ exceptions, non-module threaded source profile
