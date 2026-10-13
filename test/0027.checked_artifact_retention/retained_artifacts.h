// Test-owned artifact-retention prototype. This is DATA, not runtime admission,
// a completed int-lazy implementation, or guest execution authority.
#pragma once
#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/uwvm/runtime/storage/full.h>
#include <atomic>
#include <optional>
#include <span>
#include <utility>
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#endif

namespace uwvm2test::checked_artifacts
{
    namespace full = ::uwvm2::uwvm::runtime::full;
    using source_owner = full::full_source_instance::owner;
    using error = ::uwvm2::validation::error::code_validation_error_impl;
    using error_code = ::uwvm2::validation::error::code_validation_error_code;

    // Every producer and deferred handoff in this prototype runs under the
    // same externally serialized native administration contract as the actual
    // source loader. This observation is NOT a replacement for a production
    // generation execution/publication lease when concurrent reset is allowed.
    class source_cut
    {
        source_owner source_{};
        ::std::uint_least64_t initializer_serial_{};
        explicit source_cut(source_owner source) noexcept : source_{::std::move(source)},
            initializer_serial_{::uwvm2::runtime::gc::published_initializer_serial.load(::std::memory_order_acquire)} {}
    public:
        [[nodiscard]] static ::std::optional<source_cut> capture(source_owner source) noexcept
        {
            if(!full::full_source_instance::has_canonical_owner(source) || !source->initialized_from_actual_state() ||
               source->initialized_main_module() == nullptr) { return {}; }
            source_cut cut{::std::move(source)};
            if(!cut.current()) { return {}; }
            return cut;
        }
        [[nodiscard]] bool current() const noexcept
        {
            auto const selected{full::selected_full_source_owner_pin()};
            return source_ && selected && selected.get() == source_.get() &&
                !selected.owner_before(source_) && !source_.owner_before(selected) &&
                source_->initialized_from_actual_state() &&
                ::uwvm2::runtime::gc::published_initializer_serial.load(::std::memory_order_acquire) == initializer_serial_;
        }
        [[nodiscard]] source_owner const& source() const noexcept { return source_; }
    };

#if !defined(UWVM_DISABLE_INT) && !defined(UWVM2TEST_STRICT_NO_INTERPRETER)
    namespace int_compiler = ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm;
    namespace optable = ::uwvm2::runtime::compiler::uwvm_int::optable;
    template<optable::uwvm_interpreter_translate_option_t Options>
    class retained_int_artifact final
    {
        // Declared first and destroyed last: every borrowed type/module/code
        // descriptor embedded by the fused compiler stays genuinely owned.
        source_cut cut_;
        optable::uwvm_interpreter_full_function_symbol_t product_{};
        explicit retained_int_artifact(source_cut cut, optable::uwvm_interpreter_full_function_symbol_t product)
            : cut_{::std::move(cut)}, product_{::std::move(product)} {}
    public:
        using owner = ::std::shared_ptr<retained_int_artifact const>;
        retained_int_artifact(retained_int_artifact const&) = delete;
        retained_int_artifact& operator=(retained_int_artifact const&) = delete;
        class deferred_function
        {
            owner artifact_{};
            ::std::size_t index_{};
            friend class retained_int_artifact;
            explicit deferred_function(owner artifact, ::std::size_t index) noexcept
                : artifact_{::std::move(artifact)}, index_{index} {}
        public:
            [[nodiscard]] ::std::span<::std::byte const> program() const noexcept
            {
                if(!artifact_ || !artifact_->cut_.current() || index_ >= artifact_->product_.local_funcs.size()) { return {}; }
                // [actual retained fused products][index < count] product_end
                // [safe                                                    ] this owning view retains the product and source.
                auto const& bytes{artifact_->product_.local_funcs.index_unchecked(index_).op.operands};
                // [actual emitted interpreter stream ... bytes.size()] end
                // [safe                                                    ] the span borrows this exact vector; no cursor/LEB read.
                return {bytes.data(), bytes.size()};
            }
        };
        // A caller cannot submit an unchecked product or a ready bool. Only
        // this actual fused all-function compiler can reach the private ctor.
        [[nodiscard]] static owner compile_initial(source_owner source, error& failure)
        {
            auto cut{source_cut::capture(::std::move(source))};
            if(!cut || failure.err_code != error_code::ok) { return {}; }
            // [actual initialized member of the nonmoving retained source registry]
            // [safe] copy only: the strong cut owner outlives this borrowed module.
            auto const* module{cut->source()->initialized_main_module()};
            if(module == nullptr) { return {}; }
            optable::compile_option options{};
            auto const& policy{cut->source()->file().wasm_parameter.binfmt1_para};
            auto product{int_compiler::compile_all_from_uwvm<Options>(*module, options, failure, 0uz, {}, &policy)};
            if(failure.err_code != error_code::ok || product.local_funcs.size() != module->local_defined_function_vec_storage.size()) { return {}; }
            return owner{new retained_int_artifact{::std::move(*cut), ::std::move(product)}};
        }
        [[nodiscard]] static ::std::optional<deferred_function> defer_function(owner artifact, ::std::size_t index) noexcept
        {
            if(!artifact || !artifact->cut_.current() || index >= artifact->product_.local_funcs.size()) { return {}; }
            // Handoff uses only the already emitted, owned stream and its local
            // ordinal. It never receives a raw Wasm body, decoder or validator.
            return deferred_function{::std::move(artifact), index};
        }
        [[nodiscard]] ::std::size_t function_count() const noexcept { return product_.local_funcs.size(); }
        [[nodiscard]] bool current() const noexcept { return cut_.current(); }
        [[nodiscard]] source_owner const& source() const noexcept { return cut_.source(); }
    };
#endif

#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
    namespace llvm_compiler = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
    class retained_ir_artifact final
    {
        source_cut cut_;
        mutable llvm_compiler::full_function_symbol_t product_{};
        mutable ::std::atomic<bool> handed_off_{};
        explicit retained_ir_artifact(source_cut cut, llvm_compiler::full_function_symbol_t product)
            : cut_{::std::move(cut)}, product_{::std::move(product)} {}
    public:
        using owner = ::std::shared_ptr<retained_ir_artifact const>;
        retained_ir_artifact(retained_ir_artifact const&) = delete;
        retained_ir_artifact& operator=(retained_ir_artifact const&) = delete;
        class deferred_ir
        {
            // LLVM IR/function/type and original Wasm descriptor borrows cannot
            // outlive either the original source or the genuine LLVMContext.
            source_cut cut_;
            llvm_compiler::full_function_symbol_t product_{};
            friend class retained_ir_artifact;
            explicit deferred_ir(source_cut cut, llvm_compiler::full_function_symbol_t product)
                : cut_{::std::move(cut)}, product_{::std::move(product)} {}
        public:
            deferred_ir(deferred_ir const&) = delete;
            deferred_ir& operator=(deferred_ir const&) = delete;
            deferred_ir(deferred_ir&&) = default;
            deferred_ir& operator=(deferred_ir&&) = delete;
            [[nodiscard]] bool current() const noexcept { return cut_.current(); }
            [[nodiscard]] llvm_compiler::full_function_symbol_t const& data() const noexcept { return product_; }
        };
        [[nodiscard]] static owner compile_initial(source_owner source, error& failure)
        {
            auto cut{source_cut::capture(::std::move(source))};
            if(!cut || failure.err_code != error_code::ok) { return {}; }
            // [actual initialized member of the nonmoving retained source registry]
            // [safe] copy only: the strong cut owner outlives this borrowed module.
            auto const* module{cut->source()->initialized_main_module()};
            if(module == nullptr) { return {}; }
            llvm_compiler::compile_option options{};
            // [actual immutable parser/initializer feature tuple inside this source]
            // [safe] borrow only during this synchronous real fused traversal; no pointer advance.
            options.validator_feature_parameter = ::std::addressof(cut->source()->file().wasm_parameter.binfmt1_para);
            options.verify_llvm_jit_ir = true;
            options.compilation_mode = llvm_compiler::llvm_jit_compilation_mode::lazy;
            auto product{llvm_compiler::compile_all_from_uwvm(*module, options, failure, 0uz)};
            if(failure.err_code != error_code::ok || product.local_funcs.size() != module->local_defined_function_vec_storage.size() ||
               !product.llvm_jit_module.emitted || product.llvm_jit_module.llvm_module == nullptr ||
               product.llvm_jit_module.llvm_context_holder == nullptr) { return {}; }
            return owner{new retained_ir_artifact{::std::move(*cut), ::std::move(product)}};
        }
        [[nodiscard]] ::std::optional<deferred_ir> defer_lowering() const
        {
            if(!cut_.current()) { return {}; }
            bool expected{};
            if(!handed_off_.compare_exchange_strong(expected, true, ::std::memory_order_acq_rel)) { return {}; }
            // Single ownership transfer of the actual already validated IR and
            // context. There is NO raw Wasm decoder/checker call in this path.
            // Later LLVM IR verification/optimization/codegen is a separate
            // native-IR operation, never a repeated Wasm validation traversal.
            return deferred_ir{cut_, ::std::move(product_)};
        }
        [[nodiscard]] bool current() const noexcept { return cut_.current(); }
        [[nodiscard]] source_owner const& source() const noexcept { return cut_.source(); }
    };
#endif
}
