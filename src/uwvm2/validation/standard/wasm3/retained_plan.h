#pragma once
#ifndef UWVM_MODULE
# include <bit>
# include <cstddef>
# include <cstdint>
# include <limits>
# include <memory>
# include <span>
# include <utility>
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include <uwvm2/parser/wasm/standard/wasm3/type/recursive_type.h>
# include <uwvm2/validation/error/impl.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm3
{
    // This first representation deliberately covers the straight-line integer
    // lowering slice only. Other instructions remain fully validated by the
    // same Core 3 validator but cannot be lowered from this incomplete record.
    // An unsupported record is never a permission to replay raw Wasm bytes.

    // Compiler-owned record limits. These bound retained DATA, never Wasm
    // validity or guest execution permission. A limit hit discards lowering
    // payload while the SAME authoritative validator continues all bodies.
    struct retained_plan_limits
    {
        ::std::size_t function_operations{65536uz};
        ::std::size_t function_metadata_bytes{8uz * 1024uz * 1024uz};
        ::std::size_t module_operations{1048576uz};
        ::std::size_t module_metadata_bytes{128uz * 1024uz * 1024uz};
    };
    enum class retained_plan_unavailability : unsigned
    {
        none,
        unsupported_instruction,
        function_operations_limit,
        function_metadata_bytes_limit,
        module_operations_limit,
        module_metadata_bytes_limit
    };
    struct retained_module_record_budget
    {
        retained_plan_limits limits{};
        ::std::size_t operations{};
        ::std::size_t metadata_bytes{};
        // Byte accounting conservatively charges twice each vector payload
        // item plus fixed owner/descriptor DATA. Native allocator headers and
        // authoritative validator scratch are separate existing resources,
        // not a claim to measure the whole process's resident memory.
        [[nodiscard]] bool claim_fixed_metadata(::std::size_t bytes) noexcept
        {
            if(metadata_bytes > limits.module_metadata_bytes || bytes > limits.module_metadata_bytes - metadata_bytes)
            { return false; }
            metadata_bytes += bytes;
            return true;
        }
    };
    struct retained_integer_operation
    {
        ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic opcode{};
        ::std::size_t source_offset{};
        ::std::size_t source_bytes{};
        ::std::size_t stack_before{};
        ::std::size_t stack_after{};
        ::std::uint_least64_t immediate_bits{};
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type operand_type{};
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type result_type{};
        ::std::uint_least8_t popped{};
        ::std::uint_least8_t pushed{};
        bool complete_payload{};
    };
    struct retained_local_declaration_run
    {
        ::std::uint_least32_t count{};
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type type{};
    };
    namespace details { class retained_integer_plan_builder; }
    class retained_integer_function_plan final
    {
        friend class details::retained_integer_plan_builder;
        ::uwvm2::utils::container::vector<retained_integer_operation> operations_{};
        ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm3::type::core_value_type> parameter_types_{};
        ::uwvm2::utils::container::vector<retained_local_declaration_run> local_runs_{};
        ::std::size_t local_count_{};
        ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm3::type::core_value_type> results_{};
        ::std::size_t function_index_{};
        ::std::size_t parameters_{};
        ::std::size_t expression_bytes_{};
        ::std::size_t stack_max_{};
        ::std::size_t stack_bytes_max_{};
        retained_plan_unavailability unavailability_{};
        ::std::size_t retained_metadata_bytes_{};
        bool supported_{true};
        bool sealed_{};
        retained_integer_function_plan() = default;
        retained_integer_function_plan(retained_integer_function_plan&&) = default;
    public:
        using owner = ::std::shared_ptr<retained_integer_function_plan const>;
        retained_integer_function_plan(retained_integer_function_plan const&) = delete;
        retained_integer_function_plan& operator=(retained_integer_function_plan const&) = delete;
        [[nodiscard]] ::std::span<retained_integer_operation const> operations() const noexcept
        { return {operations_.data(), operations_.size()}; }
        [[nodiscard]] auto parameters() const noexcept
        { return ::std::span<::uwvm2::parser::wasm::standard::wasm3::type::core_value_type const>{parameter_types_.data(), parameter_types_.size()}; }
        [[nodiscard]] auto local_runs() const noexcept
        { return ::std::span<retained_local_declaration_run const>{local_runs_.data(), local_runs_.size()}; }
        [[nodiscard]] ::std::size_t local_count() const noexcept { return local_count_; }
        [[nodiscard]] bool local_type_at(::std::size_t index,
            ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type& out) const noexcept
        {
            if(index >= local_count_) { return false; }
            if(index < parameter_types_.size()) { out = parameter_types_.index_unchecked(index); return true; }
            auto remaining{index - parameter_types_.size()};
            for(auto const& run: local_runs_)
            {
                if(remaining < run.count) { out = run.type; return true; }
                remaining -= run.count;
            }
            return false;
        }
        [[nodiscard]] auto results() const noexcept
        { return ::std::span<::uwvm2::parser::wasm::standard::wasm3::type::core_value_type const>{results_.data(), results_.size()}; }
        [[nodiscard]] ::std::size_t function_index() const noexcept { return function_index_; }
        [[nodiscard]] ::std::size_t parameter_count() const noexcept { return parameters_; }
        [[nodiscard]] ::std::size_t expression_bytes() const noexcept { return expression_bytes_; }
        [[nodiscard]] ::std::size_t stack_max() const noexcept { return stack_max_; }
        [[nodiscard]] ::std::size_t stack_bytes_max() const noexcept { return stack_bytes_max_; }
        [[nodiscard]] bool integer_lowering_supported() const noexcept { return sealed_ && supported_; }
        [[nodiscard]] bool validation_sealed() const noexcept { return sealed_; }
        [[nodiscard]] retained_plan_unavailability unavailability() const noexcept { return unavailability_; }
        [[nodiscard]] ::std::size_t retained_metadata_bytes() const noexcept { return retained_metadata_bytes_; }
    };
    namespace details
    {
        struct discard_validated_operations { static constexpr bool retains_operations{false}; };
        class retained_integer_plan_builder final
        {
            retained_integer_function_plan plan_{};
            retained_integer_operation current_{};
            ::std::size_t current_bytes_{};
            bool terminal_{};

            retained_module_record_budget* budget_{};
            ::std::size_t retained_bytes_{};
            ::std::size_t retained_operations_{};
            explicit retained_integer_plan_builder(retained_module_record_budget& budget, bool retain) noexcept
                : budget_{::std::addressof(budget)}
            {
                if(!retain) { discard_payload(retained_plan_unavailability::module_metadata_bytes_limit); }
            }
            void discard_payload(retained_plan_unavailability reason) noexcept
            {
                if(plan_.unavailability_ == retained_plan_unavailability::none) { plan_.unavailability_ = reason; }
                plan_.supported_ = false;
                plan_.operations_.clear_destroy();
                plan_.parameter_types_.clear_destroy();
                plan_.local_runs_.clear_destroy();
                plan_.results_.clear_destroy();
                // Only charges made by this builder are refunded. Fixed module
                // owners and already completed functions remain accounted.
                if(budget_ != nullptr)
                {
                    if(retained_bytes_ > budget_->metadata_bytes || retained_operations_ > budget_->operations)
                    { ::fast_io::fast_terminate(); }
                    budget_->metadata_bytes -= retained_bytes_;
                    budget_->operations -= retained_operations_;
                }
                retained_bytes_ = 0uz;
                retained_operations_ = 0uz;
                plan_.retained_metadata_bytes_ = 0uz;
            }
            [[nodiscard]] bool claim_payload(::std::size_t bytes) noexcept
            {
                if(!plan_.supported_) { return false; }
                if(retained_bytes_ > budget_->limits.function_metadata_bytes ||
                   bytes > budget_->limits.function_metadata_bytes - retained_bytes_)
                { discard_payload(retained_plan_unavailability::function_metadata_bytes_limit); return false; }
                if(!budget_->claim_fixed_metadata(bytes))
                { discard_payload(retained_plan_unavailability::module_metadata_bytes_limit); return false; }
                retained_bytes_ += bytes;
                plan_.retained_metadata_bytes_ = retained_bytes_;
                return true;
            }
            [[nodiscard]] bool claim_operation() noexcept
            {
                if(retained_operations_ >= budget_->limits.function_operations)
                { discard_payload(retained_plan_unavailability::function_operations_limit); return false; }
                if(budget_->operations >= budget_->limits.module_operations)
                { discard_payload(retained_plan_unavailability::module_operations_limit); return false; }
                if(!claim_payload(2uz * sizeof(retained_integer_operation))) { return false; }
                ++retained_operations_;
                ++budget_->operations;
                return true;
            }

            static constexpr bool integer_type(::uwvm2::parser::wasm::standard::wasm3::type::core_value_type value) noexcept
            {
                using kind = ::uwvm2::parser::wasm::standard::wasm3::type::value_kind;
                return value.kind == kind::i32 || value.kind == kind::i64;
            }
            static constexpr ::std::size_t integer_width(::uwvm2::parser::wasm::standard::wasm3::type::core_value_type value) noexcept
            { return value.kind == ::uwvm2::parser::wasm::standard::wasm3::type::value_kind::i64 ? 8uz : 4uz; }
        public:
            static constexpr bool retains_operations{true};
            // Only this factory can construct a builder or mint an immutable
            // plan; its definition calls the actual typed validator once.
            template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
            [[nodiscard]] static retained_integer_function_plan::owner validate(
                ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module,
                ::std::size_t function_index,
                ::uwvm2::validation::error::code_validation_error_impl& error,
                ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& features,
                retained_module_record_budget* module_budget = nullptr, bool retain = true);
            template<typename ParameterAt, typename LocalRuns, typename RunType, typename ResultAt>
            void begin_function(::std::size_t index, ::std::size_t parameters, ::std::size_t local_count,
                                ::std::size_t results, ::std::size_t expression_bytes, ParameterAt parameter_at,
                                LocalRuns const& local_runs, RunType run_type, ResultAt result_at)
            {
                plan_.function_index_ = index; plan_.parameters_ = parameters; plan_.expression_bytes_ = expression_bytes;
                plan_.local_count_ = local_count;
                // Preserve the parser's compressed declaration runs. Flattening
                // every local of every unused function would defeat lazy memory
                // behavior and turn repeated run lookups into quadratic work.
                for(::std::size_t i{}; i != parameters; ++i)
                { if(claim_payload(2uz * sizeof(parameter_at(i)))) { plan_.parameter_types_.push_back(parameter_at(i)); } }
                for(auto const& run: local_runs)
                { if(claim_payload(2uz * sizeof(retained_local_declaration_run))) { plan_.local_runs_.push_back({run.count, run_type(run)}); } }
                for(::std::size_t i{}; i != results; ++i)
                {
                    auto const value{result_at(i)};
                    if(!integer_type(value)) { discard_payload(retained_plan_unavailability::unsupported_instruction); }
                    if(claim_payload(2uz * sizeof(value))) { plan_.results_.push_back(value); }
                }
            }
            void begin_instruction(::std::uint_least8_t opcode, ::std::size_t offset, ::std::size_t stack,
                                   ::std::size_t control_depth, bool polymorphic,
                                   ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type top)
            {
                current_ = {}; current_.opcode = static_cast<decltype(current_.opcode)>(opcode);
                current_.source_offset = offset; current_.stack_before = stack; current_.operand_type = top;
                if(control_depth != 1uz || polymorphic) { discard_payload(retained_plan_unavailability::unsupported_instruction); }
            }
            void immediate_u32(::std::uint_least32_t value) noexcept { current_.immediate_bits = value; }
            void immediate_i32(::std::int_least32_t value) noexcept
            { current_.immediate_bits = ::std::bit_cast<::std::uint_least32_t>(value); }
            void immediate_i64(::std::int_least64_t value) noexcept
            { current_.immediate_bits = ::std::bit_cast<::std::uint_least64_t>(value); }
            void finish_instruction(::std::size_t cursor_offset, ::std::size_t stack,
                                    ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type top)
            {
                using opcode = ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic;
                // Unavailable means no further payload copies, never stopping
                // the actual typed validator or skipping its terminal checks.
                if(!plan_.supported_) { return; }
                if(cursor_offset < current_.source_offset || cursor_offset > plan_.expression_bytes_) { ::fast_io::fast_terminate(); }
                current_.source_bytes = cursor_offset - current_.source_offset;
                current_.stack_after = stack; current_.result_type = top;
                current_.complete_payload = true;
                switch(current_.opcode)
                {
                    case opcode::nop: case opcode::end: break;
                    case opcode::i32_const: case opcode::i64_const: case opcode::local_get: current_.pushed = 1u; break;
                    case opcode::local_set: case opcode::drop: current_.popped = 1u; break;
                    case opcode::local_tee: current_.popped = 1u; current_.pushed = 1u; break;
                    case opcode::i32_add: case opcode::i32_sub: case opcode::i32_mul:
                    case opcode::i32_and: case opcode::i32_or: case opcode::i32_xor:
                    case opcode::i64_add: case opcode::i64_sub: case opcode::i64_mul:
                    case opcode::i64_and: case opcode::i64_or: case opcode::i64_xor:
                        current_.popped = 2u; current_.pushed = 1u; break;
                    default: current_.complete_payload = false; discard_payload(retained_plan_unavailability::unsupported_instruction); break;
                }
                // These checks concern the retained representation, not a
                // replay of Wasm typing: the authoritative validator supplied
                // both heights and actual effective value types in this pass.
                if(current_.complete_payload && plan_.supported_)
                {
                    if(current_.stack_before < current_.popped || current_.stack_before - current_.popped + current_.pushed != stack ||
                       (current_.popped != 0u && !integer_type(current_.operand_type)) ||
                       (current_.pushed != 0u && !integer_type(current_.result_type))) { discard_payload(retained_plan_unavailability::unsupported_instruction); }
                    else
                    {
                        auto const popped{current_.popped * integer_width(current_.operand_type)};
                        auto const pushed{current_.pushed * integer_width(current_.result_type)};
                        if(popped > current_bytes_ || pushed > (::std::numeric_limits<::std::size_t>::max)() - (current_bytes_ - popped))
                        { ::fast_io::fast_terminate(); }
                        current_bytes_ = current_bytes_ - popped + pushed;
                        if(stack > plan_.stack_max_) { plan_.stack_max_ = stack; }
                        if(current_bytes_ > plan_.stack_bytes_max_) { plan_.stack_bytes_max_ = current_bytes_; }
                    }
                }
                if(plan_.supported_ && claim_operation()) { plan_.operations_.push_back(current_); }
            }
            void finish_function() noexcept { terminal_ = true; }
        };
    }
}
