#pragma once
#ifndef UWVM_MODULE
# include <atomic>
# include <bit>
# include <concepts>
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <limits>
# include <memory>
# include <type_traits>
# include <utility>
# include <fast_io.h>
# include <uwvm2/validation/standard/wasm3/impl.h>
# include <uwvm2/uwvm/runtime/storage/impl.h>
# include <uwvm2/uwvm/runtime/checked_source/impl.h>
# include <uwvm2/runtime/lib/uwvm_runtime.h>
# include <uwvm2/runtime/compiler/uwvm_int/compile_all_from_uwvm/impl.h>
# include <uwvm2/runtime/compiler/uwvm_int/optable/impl.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::uwvm_int::compile_cu_from_lazy_validator
{
    // A native compile-time owner, never guest execution admission. Factory and
    // matches_current_source run under the existing serialized administration /
    // scheduler-generation contract: reset must drain actual workers first.
    // Neither the initializer serial nor actual generation observation substitutes
    // for that worker-drain/publication lifetime contract.
    class checked_integer_module_plan final
    {
        using source_owner = ::uwvm2::uwvm::runtime::full::full_source_instance::owner;
        using function_plan = ::uwvm2::validation::standard::wasm3::retained_integer_function_plan;
        // Strong source is destroyed last: module/recursive type IDs/parsed
        // closure remain real until every delayed native compiler drops its pin.
        source_owner source_{};
        ::uwvm2::uwvm::runtime::checked_source::parsed_integer_source_plan::owner parsed_{};
        ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_feature_parameter_storage_t features_{};
        ::uwvm2::utils::container::vector<function_plan::owner> functions_{};
        struct actual_function_source { void const* code{}; void const* signature{}; };
        ::uwvm2::utils::container::vector<actual_function_source> declarations_{};
        ::std::uint_least64_t initializer_serial_{};
        ::std::uint_least64_t runtime_generation_{};
        template<::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        [[nodiscard]] static auto const& parsed_codes(
            ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module) noexcept
        {
            return ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                ::uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<Fs...>>(module.sections).codes;
        }
        explicit checked_integer_module_plan(source_owner source)
            : source_{::std::move(source)}, features_{source_->file().wasm_parameter.binfmt1_para},
              initializer_serial_{::uwvm2::runtime::gc::published_initializer_serial.load(::std::memory_order_acquire)},
              runtime_generation_{::uwvm2::runtime::lib::observe_compiler_runtime_generation_host_api()} {}
    public:
        using owner = ::std::shared_ptr<checked_integer_module_plan const>;
        checked_integer_module_plan(checked_integer_module_plan const&) = delete;
        checked_integer_module_plan& operator=(checked_integer_module_plan const&) = delete;
        [[nodiscard]] static owner bind_after_actual_initializer(
            ::uwvm2::uwvm::runtime::checked_source::parsed_integer_source_plan::owner parsed,
            ::uwvm2::validation::error::code_validation_error_impl& error)
        {
            namespace full = ::uwvm2::uwvm::runtime::full;
            namespace compiler = ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm;
            if(!parsed || !parsed->complete_function_record_index() || !parsed->matches_selected_source() || !parsed->seal_actual_initializer() ||
               error.err_code != ::uwvm2::validation::error::code_validation_error_code::ok) { return {}; }
            auto source{parsed->source()};
            if(!full::full_source_instance::has_canonical_owner(source) || !source->initialized_from_actual_state() ||
               source->initialized_main_module() == nullptr || source->registry().size() != 1uz) { return {}; }
            ::std::unique_ptr<checked_integer_module_plan> pending{new checked_integer_module_plan{::std::move(source)}};
            pending->parsed_ = ::std::move(parsed);
            auto const* module{pending->source_->initialized_main_module()};
            if(!pending->matches_current_source(*module) || pending->parsed_->function_count() !=
               module->local_defined_function_vec_storage.size()) { return {}; }
            compiler::details::validate_runtime_module_storage(*module);
            // Constant/declaration metadata only; the actual typed parsing seal
            // preceded all initialization effects. No function body, raw opcode,
            // local declaration, LEB immediate or typed validation is replayed.
            compiler::details::require_runtime_module_declaration_policy(*module, pending->features_, error);
            for(::std::size_t index{}; index != module->local_defined_function_vec_storage.size(); ++index)
            {
                // [actual initialized local functions] end; index<size before borrow.
                // The parser-owned seal compares original code/signature identities;
                // it accepts no user-supplied ready bit or fabricated function data.
                auto const& actual{module->local_defined_function_vec_storage.index_unchecked(index)};
                if(!pending->parsed_->matches_actual_function(index, actual.wasm_code_ptr, actual.function_type_ptr)) { return {}; }
                auto original{pending->parsed_->function(index)};
                if(!original) { return {}; }
                pending->declarations_.push_back({actual.wasm_code_ptr, actual.function_type_ptr});
                pending->functions_.push_back(::std::move(original));
            }
            if(!pending->matches_current_source(*module) || !pending->parsed_->matches_selected_source()) { return {}; }
            return owner{pending.release()};
        }
        [[nodiscard]] bool matches_current_source(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& module) const noexcept
        {
            auto const selected{::uwvm2::uwvm::runtime::full::selected_full_source_owner_pin()};
            return source_ && selected && source_.get() == selected.get() &&
                !source_.owner_before(selected) && !selected.owner_before(source_) &&
                ::uwvm2::runtime::lib::observe_compiler_runtime_generation_host_api() == runtime_generation_ &&
                source_->initialized_main_module() == ::std::addressof(module) &&
                ::uwvm2::runtime::gc::published_initializer_serial.load(::std::memory_order_acquire) == initializer_serial_;
        }
        [[nodiscard]] bool matches_function_source(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& module,
            ::std::size_t index) const noexcept
        {
            if(!matches_current_source(module) || index >= declarations_.size() ||
               index >= module.local_defined_function_vec_storage.size()) { return false; }
            auto const& actual{module.local_defined_function_vec_storage.index_unchecked(index)};
            auto const& sealed{declarations_.index_unchecked(index)};
            return actual.wasm_code_ptr == sealed.code && actual.function_type_ptr == sealed.signature;
        }
        [[nodiscard]] ::std::size_t function_count() const noexcept { return functions_.size(); }
        [[nodiscard]] function_plan const* function(::std::size_t index) const noexcept
        {
            if(index >= functions_.size()) { return nullptr; }
            return functions_.index_unchecked(index).get();
        }
        [[nodiscard]] auto const& features() const noexcept { return features_; }
        [[nodiscard]] source_owner const& source() const noexcept { return source_; }
    };
    enum class checked_integer_lowering_status : unsigned
    {
        ok,
        missing_or_retired_source,
        unsupported_instruction,
        retention_budget_exceeded,
        unsupported_register_ring,
        invalid_index,
        representation_overflow
    };
    [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string_view
        checked_integer_lowering_status_name(checked_integer_lowering_status status) noexcept
    {
        switch(status)
        {
            case checked_integer_lowering_status::ok: return u8"ok";
            case checked_integer_lowering_status::missing_or_retired_source: return u8"missing or retired source generation";
            case checked_integer_lowering_status::unsupported_instruction: return u8"unsupported checked integer instruction";
            case checked_integer_lowering_status::retention_budget_exceeded: return u8"checked lowering record budget exceeded";
            case checked_integer_lowering_status::unsupported_register_ring: return u8"register-ring lowering has not been split";
            case checked_integer_lowering_status::invalid_index: return u8"invalid retained function or local index";
            case checked_integer_lowering_status::representation_overflow: return u8"retained representation size overflow";
        }
        return u8"unknown checked lowering status";
    }
    namespace checked_plan_details
    {
        using core_type = ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type;
        [[nodiscard]] inline constexpr ::std::size_t local_width(core_type value) noexcept
        {
            using kind = ::uwvm2::parser::wasm::standard::wasm3::type::value_kind;
            switch(value.kind)
            {
                case kind::i32: case kind::f32: return 4uz;
                case kind::i64: case kind::f64: return 8uz;
                case kind::v128: return sizeof(::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128);
                case kind::reference:
                    // The existing VM defines both reference carriers over the
                    // same global token. Preserve that target-specific ABI.
                    static_assert(sizeof(::uwvm2::object::global::wasm_funcref_t) ==
                                  sizeof(::uwvm2::object::global::wasm_externref_t));
                    return sizeof(::uwvm2::object::global::wasm_externref_t);
            }
            return 0uz;
        }
        template<typename Value>
        inline void append_native_slot(::uwvm2::runtime::compiler::uwvm_int::optable::interpreter_bytecode_vector& stream, Value value)
        {
            static_assert(::std::is_trivially_copyable_v<Value>);
            auto const offset{stream.size()};
            if(sizeof(Value) > (::std::numeric_limits<::std::size_t>::max)() - offset) { ::fast_io::fast_terminate(); }
            stream.resize(offset + sizeof(Value));
            // [stream.data(), stream.data()+stream.size()) new owned allocation
            // [safe: sizeof(Value) <= size-offset       ] output_end
            // ^^ data+offset forms a complete native slot only AFTER resize/range proof.
            ::fast_io::freestanding::my_memcpy(stream.data() + offset, ::std::addressof(value), sizeof(Value));
        }
    }
    template<::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption>
    [[nodiscard]] inline checked_integer_lowering_status lower_checked_integer_function(
        checked_integer_module_plan::owner const& plan,
        ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& actual_module,
        ::std::size_t local_index,
        ::uwvm2::runtime::compiler::uwvm_int::optable::local_func_storage_t& destination)
    {
        namespace optable = ::uwvm2::runtime::compiler::uwvm_int::optable;
        namespace translate = optable::translate;
        namespace full = ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm;
        using opcode = ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic;
        using kind = ::uwvm2::parser::wasm::standard::wasm3::type::value_kind;
        using i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
        using i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
        if(!plan || !plan->matches_current_source(actual_module)) { return checked_integer_lowering_status::missing_or_retired_source; }
        if(local_index >= actual_module.local_defined_function_vec_storage.size() || local_index >= plan->function_count())
        { return checked_integer_lowering_status::invalid_index; }
        if(!plan->matches_function_source(actual_module, local_index)) { return checked_integer_lowering_status::missing_or_retired_source; }
        auto const* function{plan->function(local_index)};
        if(function == nullptr) { return checked_integer_lowering_status::unsupported_instruction; }
        if(!function->integer_lowering_supported())
        {
            auto const reason{function->unavailability()};
            return reason != ::uwvm2::validation::standard::wasm3::retained_plan_unavailability::none &&
                reason != ::uwvm2::validation::standard::wasm3::retained_plan_unavailability::unsupported_instruction ?
                checked_integer_lowering_status::retention_budget_exceeded : checked_integer_lowering_status::unsupported_instruction;
        }
        if constexpr(CompileOption.i32_stack_top_begin_pos != SIZE_MAX || CompileOption.i32_stack_top_end_pos != SIZE_MAX ||
                     CompileOption.i64_stack_top_begin_pos != SIZE_MAX || CompileOption.i64_stack_top_end_pos != SIZE_MAX ||
                     CompileOption.f32_stack_top_begin_pos != SIZE_MAX || CompileOption.f32_stack_top_end_pos != SIZE_MAX ||
                     CompileOption.f64_stack_top_begin_pos != SIZE_MAX || CompileOption.f64_stack_top_end_pos != SIZE_MAX ||
                     CompileOption.v128_stack_top_begin_pos != SIZE_MAX || CompileOption.v128_stack_top_end_pos != SIZE_MAX)
        {
            // No slow spill-before-every-op workaround. Register-ring lowering
            // requires its real physical-state seam and remains explicit work.
            return checked_integer_lowering_status::unsupported_register_ring;
        }
        else
        {
            optable::local_func_storage_t pending{};
            struct local_layout_run
            {
                ::std::size_t first_index{};
                ::std::size_t count{};
                ::std::size_t first_byte{};
                checked_plan_details::core_type type{};
            };
            ::uwvm2::utils::container::vector<local_layout_run> layout{};
            ::std::size_t local_bytes{}, local_count{};
            auto const append_layout{[&](checked_plan_details::core_type type, ::std::size_t count) -> bool
            {
                if(count == 0uz) { return true; }
                auto const width{checked_plan_details::local_width(type)};
                if(width == 0uz || count > ((::std::numeric_limits<::std::size_t>::max)() - local_bytes) / width ||
                   count > (::std::numeric_limits<::std::size_t>::max)() - local_count) { return false; }
                layout.push_back({local_count, count, local_bytes, type});
                local_bytes += count * width; local_count += count;
                return true;
            }};
            for(auto const type: function->parameters())
            { if(!append_layout(type, 1uz)) { return checked_integer_lowering_status::representation_overflow; } }
            auto const parameter_bytes{local_bytes};
            for(auto const& run: function->local_runs())
            { if(!append_layout(run.type, run.count)) { return checked_integer_lowering_status::representation_overflow; } }
            if(local_count != function->local_count()) { return checked_integer_lowering_status::invalid_index; }
            constexpr auto temporary_bytes{sizeof(::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128)};
            if(local_bytes > (::std::numeric_limits<::std::size_t>::max)() - temporary_bytes ||
               local_count == (::std::numeric_limits<::std::size_t>::max)())
            { return checked_integer_lowering_status::representation_overflow; }
            pending.local_count = local_count + 1uz;
            // Match the full emitter: parameters occupy the initial prefix;
            // trailing locals never read by a retained local.get are not zeroed.
            pending.local_bytes_zeroinit_end = parameter_bytes;
            pending.local_bytes_max = local_bytes + temporary_bytes;
            pending.operand_stack_max = function->stack_max(); pending.operand_stack_byte_max = function->stack_bytes_max();
            constexpr auto tuple_size{full::details::interpreter_tuple_size<CompileOption>()};
            static constexpr auto tuple{full::details::make_interpreter_tuple<CompileOption>(::std::make_index_sequence<tuple_size>{})};
            optable::uwvm_interpreter_stacktop_currpos_t const stacktop{};
            auto const emit{[&](auto pointer)
            {
                static_assert(::std::same_as<decltype(pointer), full::details::interpreter_expected_opfunc_ptr_t<CompileOption>>);
                checked_plan_details::append_native_slot(pending.op.operands, pointer);
            }};
            for(auto const& operation: function->operations())
            {
                if(!operation.complete_payload) { return checked_integer_lowering_status::unsupported_instruction; }
                switch(operation.opcode)
                {
                    case opcode::nop: break;
                    case opcode::end: emit(translate::get_uwvmint_return_fptr_from_tuple<CompileOption>(stacktop, tuple)); break;
                    case opcode::i32_const:
                        emit(translate::get_uwvmint_i32_const_fptr_from_tuple<CompileOption>(stacktop, tuple));
                        checked_plan_details::append_native_slot(pending.op.operands,
                            ::std::bit_cast<i32>(static_cast<::std::uint_least32_t>(operation.immediate_bits))); break;
                    case opcode::i64_const:
                        emit(translate::get_uwvmint_i64_const_fptr_from_tuple<CompileOption>(stacktop, tuple));
                        checked_plan_details::append_native_slot(pending.op.operands, ::std::bit_cast<i64>(operation.immediate_bits)); break;
                    case opcode::local_get: case opcode::local_set: case opcode::local_tee:
                    {
                        auto const index{static_cast<::std::size_t>(operation.immediate_bits)};
                        if(index >= local_count || layout.empty()) { return checked_integer_lowering_status::invalid_index; }
                        // Binary search owns integer indices, not Wasm cursors.
                        // Each midpoint is proved < size before index_unchecked.
                        ::std::size_t first{}, last{layout.size()};
                        while(first != last)
                        {
                            auto const middle{first + (last - first) / 2uz};
                            if(layout.index_unchecked(middle).first_index <= index) { first = middle + 1uz; }
                            else { last = middle; }
                        }
                        if(first == 0uz) { return checked_integer_lowering_status::invalid_index; }
                        auto const& run{layout.index_unchecked(first - 1uz)};
                        auto const within{index - run.first_index};
                        if(within >= run.count) { return checked_integer_lowering_status::invalid_index; }
                        auto const type{run.type.kind};
                        auto const width{checked_plan_details::local_width(run.type)};
                        // append_layout proved the complete run's count*width
                        // extent. within<count therefore proves this slot end.
                        auto const offset{run.first_byte + within * width};
                        if(operation.opcode == opcode::local_get && offset + width > pending.local_bytes_zeroinit_end)
                        { pending.local_bytes_zeroinit_end = offset + width; }
                        if(type == kind::i32)
                        {
                            if(operation.opcode == opcode::local_get) { emit(translate::get_uwvmint_local_get_i32_fptr_from_tuple<CompileOption>(stacktop, tuple)); }
                            else if(operation.opcode == opcode::local_set) { emit(translate::get_uwvmint_local_set_i32_fptr_from_tuple<CompileOption>(stacktop, tuple)); }
                            else { emit(translate::get_uwvmint_local_tee_i32_fptr_from_tuple<CompileOption>(stacktop, tuple)); }
                        }
                        else if(type == kind::i64)
                        {
                            if(operation.opcode == opcode::local_get) { emit(translate::get_uwvmint_local_get_i64_fptr_from_tuple<CompileOption>(stacktop, tuple)); }
                            else if(operation.opcode == opcode::local_set) { emit(translate::get_uwvmint_local_set_i64_fptr_from_tuple<CompileOption>(stacktop, tuple)); }
                            else { emit(translate::get_uwvmint_local_tee_i64_fptr_from_tuple<CompileOption>(stacktop, tuple)); }
                        }
                        else { return checked_integer_lowering_status::unsupported_instruction; }
                        checked_plan_details::append_native_slot(pending.op.operands, offset); break;
                    }
                    case opcode::drop:
                        if(operation.operand_type.kind == kind::i32) { emit(translate::get_uwvmint_drop_i32_fptr_from_tuple<CompileOption>(stacktop, tuple)); }
                        else { emit(translate::get_uwvmint_drop_i64_fptr_from_tuple<CompileOption>(stacktop, tuple)); }
                        break;
                    case opcode::i32_add: emit(translate::get_uwvmint_i32_add_fptr_from_tuple<CompileOption>(stacktop, tuple)); break;
                    case opcode::i32_sub: emit(translate::get_uwvmint_i32_sub_fptr_from_tuple<CompileOption>(stacktop, tuple)); break;
                    case opcode::i32_mul: emit(translate::get_uwvmint_i32_mul_fptr_from_tuple<CompileOption>(stacktop, tuple)); break;
                    case opcode::i32_and: emit(translate::get_uwvmint_i32_and_fptr_from_tuple<CompileOption>(stacktop, tuple)); break;
                    case opcode::i32_or: emit(translate::get_uwvmint_i32_or_fptr_from_tuple<CompileOption>(stacktop, tuple)); break;
                    case opcode::i32_xor: emit(translate::get_uwvmint_i32_xor_fptr_from_tuple<CompileOption>(stacktop, tuple)); break;
                    case opcode::i64_add: emit(translate::get_uwvmint_i64_add_fptr_from_tuple<CompileOption>(stacktop, tuple)); break;
                    case opcode::i64_sub: emit(translate::get_uwvmint_i64_sub_fptr_from_tuple<CompileOption>(stacktop, tuple)); break;
                    case opcode::i64_mul: emit(translate::get_uwvmint_i64_mul_fptr_from_tuple<CompileOption>(stacktop, tuple)); break;
                    case opcode::i64_and: emit(translate::get_uwvmint_i64_and_fptr_from_tuple<CompileOption>(stacktop, tuple)); break;
                    case opcode::i64_or: emit(translate::get_uwvmint_i64_or_fptr_from_tuple<CompileOption>(stacktop, tuple)); break;
                    case opcode::i64_xor: emit(translate::get_uwvmint_i64_xor_fptr_from_tuple<CompileOption>(stacktop, tuple)); break;
                    default: return checked_integer_lowering_status::unsupported_instruction;
                }
            }
            if(!plan->matches_function_source(actual_module, local_index)) { return checked_integer_lowering_status::missing_or_retired_source; }
            // This is a complete owning output transaction. No raw Wasm body,
            // LEB scanner or type validator is accepted by this lowering API.
            destination = ::std::move(pending);
            return checked_integer_lowering_status::ok;
        }
    }
}
#endif
