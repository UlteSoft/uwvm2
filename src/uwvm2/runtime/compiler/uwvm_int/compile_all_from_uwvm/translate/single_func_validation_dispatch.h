// block type
using value_type_enum = curr_operand_stack_value_type;
// This include fragment drives validation and bytecode emission for one function body. It is kept in
// the caller's scope so opcode case files can share the same code cursor, validation stacks, runtime
// storage references, and emission helpers without packaging them into a large mutable context object.
[[maybe_unused]] static constexpr value_type_enum i32_result_arr[1u]{value_type_enum::i32};
[[maybe_unused]] static constexpr value_type_enum i64_result_arr[1u]{value_type_enum::i64};
[[maybe_unused]] static constexpr value_type_enum f32_result_arr[1u]{value_type_enum::f32};
[[maybe_unused]] static constexpr value_type_enum f64_result_arr[1u]{value_type_enum::f64};
[[maybe_unused]] static constexpr value_type_enum v128_result_arr[1u]{value_type_enum::v128};
[[maybe_unused]] static constexpr value_type_enum funcref_result_arr[1u]{value_type_enum::funcref};
[[maybe_unused]] static constexpr value_type_enum externref_result_arr[1u]{value_type_enum::externref};

// The function itself is modeled as the outermost control frame. This lets `return` share the same
// stack-shape repair machinery as structured branches to a block end.
auto const func_end_label_id{new_label(false)};

// function block (label/result type is the function result)
control_flow_stack.push_back({
    .label = {.begin = curr_func_type.result.begin, .end = curr_func_type.result.end},
    .start = {},
    .result = {.begin = curr_func_type.result.begin, .end = curr_func_type.result.end},
    .operand_stack_base = 0uz,
    .type = block_type::function,
    .polymorphic_base = false,
    .then_polymorphic_end = false,
    .start_label_id = SIZE_MAX,
    .end_label_id = func_end_label_id,
    .else_label_id = SIZE_MAX
});

// start parse the code
auto code_curr{code_begin};

using wasm_value_type_u = curr_operand_stack_value_type;
using wasm_error_value_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type;
using wasm1p1_code = ::uwvm2::parser::wasm::standard::wasm1p1::opcode::op_basic;
using wasm1p1_numeric_code = ::uwvm2::parser::wasm::standard::wasm1p1::opcode::op_numeric;
using wasm1p1_simd_code = ::uwvm2::parser::wasm::standard::wasm1p1::opcode::op_simd;

auto const to_wasm1_value_type{[](curr_operand_stack_value_type type) constexpr noexcept -> wasm_error_value_type
                               { return static_cast<wasm_error_value_type>(type); }};

auto const opcode_byte{[](wasm1p1_code opcode) constexpr noexcept -> wasm_byte { return static_cast<wasm_byte>(opcode); }};
auto const opcode_u32{[&](wasm1p1_code opcode) constexpr noexcept -> wasm_u32 { return static_cast<wasm_u32>(opcode_byte(opcode)); }};

auto const is_reference_value_type{[](curr_operand_stack_value_type type) constexpr noexcept -> bool
                                   { return type == curr_operand_stack_value_type::funcref || type == curr_operand_stack_value_type::externref; }};

auto const is_untyped_select_value_type{[&](curr_operand_stack_value_type type) constexpr noexcept -> bool
                                        {
                                            if(type == curr_operand_stack_value_type::i32 || type == curr_operand_stack_value_type::i64 ||
                                               type == curr_operand_stack_value_type::f32 || type == curr_operand_stack_value_type::f64)
                                            {
                                                return true;
                                            }
                                            return type == curr_operand_stack_value_type::v128 && !wasm1p1_para.disable_simd;
                                        }};

auto const fail_wasm1p1_feature_required{
    [&](::std::byte const* const op_begin,
        wasm_u32 const value,
        ::uwvm2::parser::wasm::base::wasm1p1_feature_kind const feature,
        ::uwvm2::parser::wasm::base::wasm1p1_error_subject const subject) constexpr UWVM_THROWS -> void
    {
        err.err_curr = op_begin;
        err.err_selectable.wasm1p1_feature_required.value = value;
        err.err_selectable.wasm1p1_feature_required.feature = feature;
        err.err_selectable.wasm1p1_feature_required.subject = subject;
        err.err_code = code_validation_error_code::wasm1p1_feature_required;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }};

auto const fail_invalid_immediate{
    [&](::std::byte const* const op_begin,
        ::uwvm2::utils::container::u8string_view op_name,
        ::fast_io::parse_code pc = ::fast_io::parse_code::invalid) constexpr UWVM_THROWS -> void
    {
        err.err_curr = op_begin;
        err.err_selectable.invalid_const_immediate.op_code_name = op_name;
        err.err_code = code_validation_error_code::invalid_const_immediate;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(pc);
    }};

auto const read_leb128{
    [&]<typename T>(::std::byte const*& curr,
                    ::std::byte const* const end,
                    ::std::byte const* const op_begin,
                    ::uwvm2::utils::container::u8string_view op_name) constexpr UWVM_THROWS -> T
    {
        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

        T value{};
        auto const [next, perr]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(curr),
                                                         reinterpret_cast<char8_t_const_may_alias_ptr>(end),
                                                         ::fast_io::mnp::leb128_get(value))};
        if(perr != ::fast_io::parse_code::ok) [[unlikely]] { fail_invalid_immediate(op_begin, op_name, perr); }

        curr = reinterpret_cast<::std::byte const*>(next);
        return value;
    }};

auto const skip_immediate_bytes{
    [&](::std::byte const*& curr,
        ::std::byte const* const end,
        ::std::byte const* const op_begin,
        ::std::size_t const bytes,
        ::uwvm2::utils::container::u8string_view op_name) constexpr UWVM_THROWS
    {
        if(static_cast<::std::size_t>(end - curr) < bytes) [[unlikely]] { fail_invalid_immediate(op_begin, op_name); }
        curr += bytes;
    }};

auto const read_u8_immediate{
    [&](::std::byte const*& curr,
        ::std::byte const* const end,
        ::std::byte const* const op_begin,
        ::uwvm2::utils::container::u8string_view op_name) constexpr UWVM_THROWS -> wasm_byte
    {
        auto const imm_pos{curr};
        skip_immediate_bytes(curr, end, op_begin, 1uz, op_name);

        wasm_byte value{};
        ::std::memcpy(::std::addressof(value), imm_pos, sizeof(value));
#if CHAR_BIT > 8
        value = static_cast<wasm_byte>(static_cast<::std::uint_least8_t>(value) & 0xFFu);
#endif
        return value;
    }};

auto const ensure_wasm1p1_value_type_enabled{
    [&](::std::byte const* op_begin,
        curr_operand_stack_value_type type,
        ::uwvm2::parser::wasm::base::wasm1p1_error_subject subject) constexpr UWVM_THROWS
    {
        auto const vt{static_cast<::uwvm2::parser::wasm::standard::wasm1p1::type::value_type>(type)};
        if(!::uwvm2::parser::wasm::standard::wasm1p1::type::is_valid_value_type(vt)) [[unlikely]]
        {
            err.err_curr = op_begin;
            err.err_selectable.wasm1p1_invalid_reference_type.value = static_cast<wasm_byte>(type);
            err.err_code = code_validation_error_code::wasm1p1_invalid_reference_type;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        if(!::uwvm2::parser::wasm::standard::wasm1p1::features::value_type_enabled(vt, effective_wasm_feature_parameter)) [[unlikely]]
        {
            auto const feature{vt == ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128
                                   ? ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::simd
                                   : ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types};
            fail_wasm1p1_feature_required(op_begin, static_cast<wasm_byte>(type), feature, subject);
        }
    }};

auto const parse_block_type{
    [&](::std::byte const* op_begin, ::uwvm2::utils::container::u8string_view op_name) constexpr UWVM_THROWS -> block_signature_t
    {
        if(code_curr == code_end) [[unlikely]]
        {
            err.err_curr = op_begin;
            err.err_code = code_validation_error_code::missing_block_type;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::end_of_file);
        }

        auto const blocktype_begin{code_curr};
        auto const blocktype{read_leb128.template operator()<wasm_i64>(code_curr, code_end, op_begin, op_name)};
        auto const blocktype_encoded_size{static_cast<::std::size_t>(code_curr - blocktype_begin)};

        if(blocktype_encoded_size > 5uz) [[unlikely]]
        {
            wasm_byte first_blocktype_byte{};
            ::std::memcpy(::std::addressof(first_blocktype_byte), blocktype_begin, sizeof(first_blocktype_byte));
#if CHAR_BIT > 8
            first_blocktype_byte = static_cast<wasm_byte>(static_cast<::std::uint_least8_t>(first_blocktype_byte) & 0xFFu);
#endif
            err.err_curr = op_begin;
            err.err_selectable.u8 = first_blocktype_byte;
            err.err_code = code_validation_error_code::illegal_block_type;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        switch(blocktype)
        {
            case -64:
            {
                return {};
            }
            case -1:
            {
                return {.result = {i32_result_arr, i32_result_arr + 1u}};
            }
            case -2:
            {
                return {.result = {i64_result_arr, i64_result_arr + 1u}};
            }
            case -3:
            {
                return {.result = {f32_result_arr, f32_result_arr + 1u}};
            }
            case -4:
            {
                return {.result = {f64_result_arr, f64_result_arr + 1u}};
            }
            case -5:
            {
                ensure_wasm1p1_value_type_enabled(
                    op_begin, curr_operand_stack_value_type::v128, ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                return {.result = {v128_result_arr, v128_result_arr + 1u}};
            }
            case -16:
            {
                ensure_wasm1p1_value_type_enabled(
                    op_begin, curr_operand_stack_value_type::funcref, ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                return {.result = {funcref_result_arr, funcref_result_arr + 1u}};
            }
            case -17:
            {
                ensure_wasm1p1_value_type_enabled(
                    op_begin, curr_operand_stack_value_type::externref, ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                return {.result = {externref_result_arr, externref_result_arr + 1u}};
            }
            default:
            {
                break;
            }
        }

        if(blocktype >= 0)
        {
            if(wasm1p1_para.disable_multi_value) [[unlikely]]
            {
                fail_wasm1p1_feature_required(op_begin,
                                              static_cast<wasm_u32>(blocktype),
                                              ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::multi_value,
                                              ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
            }

            auto const types_begin{curr_module.type_section_storage.type_section_begin};
            auto const types_end{curr_module.type_section_storage.type_section_end};
            auto const all_type_count_uz{
                (types_begin == nullptr || types_end == nullptr) ? 0uz : static_cast<::std::size_t>(types_end - types_begin)};
            if(static_cast<::std::uint_least64_t>(blocktype) > ::std::numeric_limits<wasm_u32>::max() ||
               static_cast<::std::size_t>(blocktype) >= all_type_count_uz) [[unlikely]]
            {
                err.err_curr = op_begin;
                err.err_selectable.illegal_type_index.type_index = blocktype > 0 ? static_cast<wasm_u32>(blocktype) : 0u;
                err.err_selectable.illegal_type_index.all_type_count = static_cast<wasm_u32>(all_type_count_uz);
                err.err_code = code_validation_error_code::illegal_type_index;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            auto const& block_func_type{types_begin[static_cast<::std::size_t>(blocktype)]};
            return {.start = {block_func_type.parameter.begin, block_func_type.parameter.end},
                    .result = {block_func_type.result.begin, block_func_type.result.end}};
        }

        wasm_byte first_blocktype_byte{};
        ::std::memcpy(::std::addressof(first_blocktype_byte), blocktype_begin, sizeof(first_blocktype_byte));
#if CHAR_BIT > 8
        first_blocktype_byte = static_cast<wasm_byte>(static_cast<::std::uint_least8_t>(first_blocktype_byte) & 0xFFu);
#endif
        err.err_curr = op_begin;
        err.err_selectable.u8 = first_blocktype_byte;
        err.err_code = code_validation_error_code::illegal_block_type;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        return {};
    }};

auto const validate_numeric_unary_stack_effect{
    [&](::std::byte const* op_begin,
        ::uwvm2::utils::container::u8string_view op_name,
        curr_operand_stack_value_type expected_operand_type,
        curr_operand_stack_value_type result_type) constexpr UWVM_THROWS
    {
        if(!is_polymorphic && concrete_operand_count() == 0uz) [[unlikely]] { report_operand_stack_underflow(op_begin, op_name, 1uz); }

        auto const operand{try_pop_concrete_operand()};
        if(!operand_type_matches(operand, expected_operand_type)) [[unlikely]]
        {
            err.err_curr = op_begin;
            err.err_selectable.numeric_operand_type_mismatch.op_code_name = op_name;
            err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_value_type(expected_operand_type);
            err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_value_type(operand.type);
            err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        operand_stack_push(result_type);
    }};

[[maybe_unused]] auto const pop_expected_operands{
    [&](::std::byte const* op_begin, ::uwvm2::utils::container::u8string_view op_name, block_result_type expected) constexpr UWVM_THROWS
    {
        auto const expected_count{static_cast<::std::size_t>(expected.end - expected.begin)};
        if(!is_polymorphic && concrete_operand_count() < expected_count) [[unlikely]] { report_operand_stack_underflow(op_begin, op_name, expected_count); }

        auto const stack_size{operand_stack.size()};
        auto const available_count{concrete_operand_count()};
        auto const concrete_to_check{available_count < expected_count ? available_count : expected_count};

        for(::std::size_t i{}; i != concrete_to_check; ++i)
        {
            auto const expected_type{expected.begin[expected_count - 1uz - i]};
            auto const actual_operand{operand_stack.index_unchecked(stack_size - 1uz - i)};
            if(!stack_entry_type_matches(actual_operand, expected_type)) [[unlikely]]
            {
                err.err_curr = op_begin;
                err.err_selectable.br_value_type_mismatch.op_code_name = op_name;
                err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(expected_type);
                err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(actual_operand.type);
                err.err_code = code_validation_error_code::br_value_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }

        pop_available_concrete_operands(expected_count);
    }};

auto const enter_control_frame{
    [&](::std::byte const* op_begin,
        ::uwvm2::utils::container::u8string_view op_name,
        block_type type,
        block_signature_t signature,
        ::std::size_t start_label_id,
        ::std::size_t end_label_id,
        ::std::size_t else_label_id,
        ::std::byte const* wasm_code_curr_at_start_label = nullptr) constexpr UWVM_THROWS -> ::std::size_t
    {
        pop_expected_operands(op_begin, op_name, signature.start);

        auto const base{operand_stack.size()};
        auto const label{type == block_type::loop ? signature.start : signature.result};
        for(auto curr{signature.start.begin}; curr != signature.start.end; ++curr) { operand_stack_push(*curr); }

        control_flow_stack.push_back({.label = label,
                                      .start = signature.start,
                                      .result = signature.result,
                                      .operand_stack_base = base,
                                      .type = type,
                                      .polymorphic_base = is_polymorphic,
                                      .then_polymorphic_end = false,
                                      .start_label_id = start_label_id,
                                      .end_label_id = end_label_id,
                                      .else_label_id = else_label_id,
                                      .wasm_code_curr_at_start_label = wasm_code_curr_at_start_label});

        is_polymorphic = false;
        return base;
    }};

auto const validate_i32_operands{
    [&](::std::byte const* op_begin, ::uwvm2::utils::container::u8string_view op_name, ::std::size_t count) constexpr UWVM_THROWS
    {
        if(!is_polymorphic && concrete_operand_count() < count) [[unlikely]] { report_operand_stack_underflow(op_begin, op_name, count); }

        auto const concrete_to_check{concrete_operand_count() < count ? concrete_operand_count() : count};
        for(::std::size_t i{}; i != concrete_to_check; ++i)
        {
            auto const actual_operand{operand_stack.index_unchecked(operand_stack.size() - 1uz - i)};
            if(!stack_entry_type_matches(actual_operand, curr_operand_stack_value_type::i32)) [[unlikely]]
            {
                err.err_curr = op_begin;
                err.err_selectable.numeric_operand_type_mismatch.op_code_name = op_name;
                err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_value_type(curr_operand_stack_value_type::i32);
                err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_value_type(actual_operand.type);
                err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }

        pop_available_concrete_operands(count);
    }};

auto const check_table_index{[&](::std::byte const* op_begin, wasm_u32 table_index) constexpr UWVM_THROWS
                             {
                                 if(table_index >= all_table_count) [[unlikely]]
                                 {
                                     err.err_curr = op_begin;
                                     err.err_selectable.illegal_table_index.table_index = table_index;
                                     err.err_selectable.illegal_table_index.all_table_count = all_table_count;
                                     err.err_code = code_validation_error_code::illegal_table_index;
                                     ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                 }
                             }};

auto const check_memory_index_zero{[&](::std::byte const* op_begin, wasm_u32 memory_index, ::uwvm2::utils::container::u8string_view op_name) constexpr UWVM_THROWS
                                   {
                                       if(all_memory_count == 0u) [[unlikely]]
                                       {
                                           err.err_curr = op_begin;
                                           err.err_selectable.no_memory.op_code_name = op_name;
                                           err.err_selectable.no_memory.align = 0u;
                                           err.err_selectable.no_memory.offset = 0u;
                                           err.err_code = code_validation_error_code::no_memory;
                                           ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                       }

                                       if(memory_index != 0u) [[unlikely]]
                                       {
                                           err.err_curr = op_begin;
                                           err.err_selectable.illegal_memory_index.memory_index = memory_index;
                                           err.err_selectable.illegal_memory_index.all_memory_count = all_memory_count;
                                           err.err_code = code_validation_error_code::illegal_memory_index;
                                           ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                       }
                                   }};

auto const check_data_index{[&](::std::byte const* op_begin, wasm_u32 data_index) constexpr UWVM_THROWS
                            {
                                auto const all_data_count{static_cast<wasm_u32>(curr_module.local_defined_data_vec_storage.size())};
                                if(data_index >= all_data_count) [[unlikely]]
                                {
                                    err.err_curr = op_begin;
                                    err.err_selectable.illegal_data_index.data_index = data_index;
                                    err.err_selectable.illegal_data_index.all_data_count = all_data_count;
                                    err.err_code = code_validation_error_code::illegal_data_index;
                                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                }
                            }};

auto const check_element_index{[&](::std::byte const* op_begin, wasm_u32 element_index) constexpr UWVM_THROWS
                               {
                                   auto const all_element_count{static_cast<wasm_u32>(curr_module.local_defined_element_vec_storage.size())};
                                   if(element_index >= all_element_count) [[unlikely]]
                                   {
                                       err.err_curr = op_begin;
                                       err.err_selectable.illegal_element_index.element_index = element_index;
                                       err.err_selectable.illegal_element_index.all_element_count = all_element_count;
                                       err.err_code = code_validation_error_code::illegal_element_index;
                                       ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                   }
                               }};

auto const get_table_value_type{[&](wasm_u32 table_index) constexpr noexcept -> curr_operand_stack_value_type
                                {
                                    if(table_index < imported_table_count)
                                    {
                                        auto const& imported_table{curr_module.imported_table_vec_storage.index_unchecked(table_index)};
                                        auto const import_type_ptr{imported_table.import_type_ptr};
                                        if(import_type_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                                        return static_cast<curr_operand_stack_value_type>(
                                            ::uwvm2::parser::wasm::standard::wasm1p1::features::to_value_type(
                                                import_type_ptr->imports.storage.table.reftype));
                                    }

                                    auto const local_table_index{static_cast<::std::size_t>(table_index - imported_table_count)};
                                    auto const& local_table{curr_module.local_defined_table_vec_storage.index_unchecked(local_table_index)};
                                    auto const table_type_ptr{local_table.table_type_ptr};
                                    if(table_type_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                                    return static_cast<curr_operand_stack_value_type>(
                                        ::uwvm2::parser::wasm::standard::wasm1p1::features::to_value_type(table_type_ptr->reftype));
                                }};

[[maybe_unused]] auto const resolve_runtime_table{
    [&](wasm_u32 table_index) constexpr noexcept -> ::uwvm2::uwvm::runtime::storage::local_defined_table_storage_t*
    {
        if(table_index < imported_table_count)
        {
            auto t{::std::addressof(curr_module.imported_table_vec_storage.index_unchecked(table_index))};
            for(;;)
            {
                if(t == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                using link_kind_t = ::uwvm2::uwvm::runtime::storage::imported_table_storage_t::imported_table_link_kind;
                switch(t->link_kind)
                {
                    case link_kind_t::defined:
                    {
                        if(t->target.defined_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                        return t->target.defined_ptr;
                    }
                    case link_kind_t::imported:
                    {
                        t = t->target.imported_ptr;
                        continue;
                    }
                    case link_kind_t::unresolved: [[fallthrough]];
                    [[unlikely]] default:
                    {
                        ::fast_io::fast_terminate();
                    }
                }
            }
        }

        auto const local_table_index{static_cast<::std::size_t>(table_index - imported_table_count)};
        return const_cast<::uwvm2::uwvm::runtime::storage::local_defined_table_storage_t*>(
            ::std::addressof(curr_module.local_defined_table_vec_storage.index_unchecked(local_table_index)));
    }};

auto const check_ref_func_index{[&](::std::byte const* op_begin, wasm_u32 func_index) constexpr UWVM_THROWS
                                {
                                    auto const all_function_size{import_func_count + local_func_count};
                                    if(static_cast<::std::size_t>(func_index) >= all_function_size) [[unlikely]]
                                    {
                                        err.err_curr = op_begin;
                                        err.err_selectable.invalid_function_index.function_index = func_index;
                                        err.err_selectable.invalid_function_index.all_function_size = all_function_size;
                                        err.err_code = code_validation_error_code::invalid_function_index;
                                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                    }

                                    bool declared{};
                                    for(auto const declared_index: curr_module.declared_ref_funcidx_vec_storage)
                                    {
                                        if(declared_index == func_index)
                                        {
                                            declared = true;
                                            break;
                                        }
                                    }
                                    if(!declared) [[unlikely]]
                                    {
                                        err.err_curr = op_begin;
                                        err.err_selectable.wasm1p1_undeclared_ref_func.function_index = func_index;
                                        err.err_code = code_validation_error_code::wasm1p1_undeclared_ref_func;
                                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                    }
                                }};

// Numeric validators advance the Wasm cursor, enforce typed-stack rules, and update the abstract
// operand stack before opcode-specific emission runs. Keeping this shared avoids subtly different
// validation behavior between integer, float, and conversion opcode files.
auto const validate_numeric_unary{[&](::uwvm2::utils::container::u8string_view op_name,
                                      curr_operand_stack_value_type expected_operand_type,
                                      curr_operand_stack_value_type result_type) constexpr UWVM_THROWS
                                  {
                                      // op_name ...
                                      // [safe] unsafe (could be the section_end)
                                      // ^^ code_curr

                                      auto const op_begin{code_curr};

                                      // op_name ...
                                      // [safe] unsafe (could be the section_end)
                                      // ^^ op_begin

                                      ++code_curr;

                                      // op_name ...
                                      // [safe]  unsafe (could be the section_end)
                                      //         ^^ code_curr

                                      if(!is_polymorphic && concrete_operand_count() == 0uz) [[unlikely]]
                                      {
                                          report_operand_stack_underflow(op_begin, op_name, 1uz);
                                      }

                                      auto const operand{try_pop_concrete_operand()};

                                      if(!operand_type_matches(operand, expected_operand_type)) [[unlikely]]
                                      {
                                          err.err_curr = op_begin;
                                          err.err_selectable.numeric_operand_type_mismatch.op_code_name = op_name;
                                          err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_value_type(expected_operand_type);
                                          err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_value_type(operand.type);
                                          err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                                          ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                      }

                                      operand_stack_push(result_type);
                                  }};

// Binary numeric validation pops RHS first because that is the physical top of the Wasm operand
// stack; error reporting still names the opcode and expected homogeneous operand type.
auto const validate_numeric_binary{[&](::uwvm2::utils::container::u8string_view op_name,
                                       curr_operand_stack_value_type expected_operand_type,
                                       curr_operand_stack_value_type result_type) constexpr UWVM_THROWS
                                   {
                                       // op_name ...
                                       // [safe] unsafe (could be the section_end)
                                       // ^^ code_curr

                                       auto const op_begin{code_curr};

                                       // op_name ...
                                       // [safe] unsafe (could be the section_end)
                                       // ^^ op_begin

                                       ++code_curr;

                                       // op_name ...
                                       // [safe ] unsafe (could be the section_end)
                                       //         ^^ code_curr

                                       if(!is_polymorphic && concrete_operand_count() < 2uz) [[unlikely]]
                                       {
                                           report_operand_stack_underflow(op_begin, op_name, 2uz);
                                       }

                                       auto const rhs{try_pop_concrete_operand()};
                                       if(!operand_type_matches(rhs, expected_operand_type)) [[unlikely]]
                                       {
                                           err.err_curr = op_begin;
                                           err.err_selectable.numeric_operand_type_mismatch.op_code_name = op_name;
                                           err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_value_type(expected_operand_type);
                                           err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_value_type(rhs.type);
                                           err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                                           ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                       }

                                       auto const lhs{try_pop_concrete_operand()};
                                       if(!operand_type_matches(lhs, expected_operand_type)) [[unlikely]]
                                       {
                                           err.err_curr = op_begin;
                                           err.err_selectable.numeric_operand_type_mismatch.op_code_name = op_name;
                                           err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_value_type(expected_operand_type);
                                           err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_value_type(lhs.type);
                                           err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                                           ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                       }

                                       operand_stack_push(result_type);
                                   }};

// Memory load validation returns only the static offset. Alignment, memory existence, address type,
// and result stack effect are fully checked here so memory opcode cases can focus on choosing the
// fastest helper/fusion form.
auto const validate_mem_load{[&](::uwvm2::utils::container::u8string_view op_name,
                                 wasm_u32 const max_align,
                                 curr_operand_stack_value_type const result_type) constexpr UWVM_THROWS -> wasm_u32
                             {
                                 // i32.load align offset ...
                                 // [ safe ] unsafe (could be the section_end)
                                 // ^^ code_curr

                                 auto const op_begin{code_curr};

                                 // i32.load align offset ...
                                 // [ safe ] unsafe (could be the section_end)
                                 // ^^ op_begin

                                 ++code_curr;

                                 // i32.load align offset ...
                                 // [ safe ] unsafe (could be the section_end)
                                 //          ^^ code_curr

                                 wasm_u32 align;   // No initialization necessary
                                 wasm_u32 offset;  // No initialization necessary

                                 using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                                 auto const [align_next, align_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                             reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                             ::fast_io::mnp::leb128_get(align))};
                                 if(align_err != ::fast_io::parse_code::ok) [[unlikely]]
                                 {
                                     err.err_curr = op_begin;
                                     err.err_code = code_validation_error_code::invalid_memarg_align;
                                     ::uwvm2::parser::wasm::base::throw_wasm_parse_code(align_err);
                                 }

                                 // i32.load align offset ...
                                 // [    safe    ] unsafe (could be the section_end)
                                 //          ^^ code_curr

                                 code_curr = reinterpret_cast<::std::byte const*>(align_next);

                                 // i32.load align offset ...
                                 // [    safe    ] unsafe (could be the section_end)
                                 //                ^^ code_curr

                                 auto const [offset_next, offset_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                               reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                               ::fast_io::mnp::leb128_get(offset))};
                                 if(offset_err != ::fast_io::parse_code::ok) [[unlikely]]
                                 {
                                     err.err_curr = op_begin;
                                     err.err_code = code_validation_error_code::invalid_memarg_offset;
                                     ::uwvm2::parser::wasm::base::throw_wasm_parse_code(offset_err);
                                 }

                                 // i32.load align offset ...
                                 // [        safe       ] unsafe (could be the section_end)
                                 //                ^^ code_curr

                                 code_curr = reinterpret_cast<::std::byte const*>(offset_next);

                                 // i32.load align offset ...
                                 // [        safe       ] unsafe (could be the section_end)
                                 //                       ^^ code_curr

                                 if(all_memory_count == 0u) [[unlikely]]
                                 {
                                     err.err_curr = op_begin;
                                     err.err_selectable.no_memory.op_code_name = op_name;
                                     err.err_selectable.no_memory.align = align;
                                     err.err_selectable.no_memory.offset = offset;
                                     err.err_code = code_validation_error_code::no_memory;
                                     ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                 }

                                 if(align > max_align) [[unlikely]]
                                 {
                                     err.err_curr = op_begin;
                                     err.err_selectable.illegal_memarg_alignment.op_code_name = op_name;
                                     err.err_selectable.illegal_memarg_alignment.align = align;
                                     err.err_selectable.illegal_memarg_alignment.max_align = max_align;
                                     err.err_code = code_validation_error_code::illegal_memarg_alignment;
                                     ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                 }

                                 if(!is_polymorphic && concrete_operand_count() == 0uz) [[unlikely]] { report_operand_stack_underflow(op_begin, op_name, 1uz); }

                                 if(auto const addr{try_pop_concrete_operand()}; !operand_type_matches(addr, wasm_value_type_u::i32)) [[unlikely]]
                                 {
                                     err.err_curr = op_begin;
                                     err.err_selectable.memarg_address_type_not_i32.op_code_name = op_name;
                                     err.err_selectable.memarg_address_type_not_i32.addr_type = to_wasm1_value_type(addr.type);
                                     err.err_code = code_validation_error_code::memarg_address_type_not_i32;
                                     ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                 }

                                 operand_stack_push(result_type);

                                 return offset;
                             }};

// Store validation consumes value then address, matching Wasm stack order. The emitted helper later
// receives the static offset, but all typed-stack and memory-index errors are reported here.
auto const validate_mem_store{[&](::uwvm2::utils::container::u8string_view op_name,
                                  wasm_u32 const max_align,
                                  curr_operand_stack_value_type const expected_value_type) constexpr UWVM_THROWS -> wasm_u32
                              {
                                  // i32.store align offset ...
                                  // [ safe  ] unsafe (could be the section_end)
                                  // ^^ code_curr

                                  auto const op_begin{code_curr};

                                  // i32.store align offset ...
                                  // [ safe  ] unsafe (could be the section_end)
                                  // ^^ op_begin

                                  ++code_curr;

                                  // i32.store align offset ...
                                  // [ safe  ] unsafe (could be the section_end)
                                  //           ^^ code_curr

                                  wasm_u32 align;   // No initialization necessary
                                  wasm_u32 offset;  // No initialization necessary

                                  using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                                  auto const [align_next, align_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                              ::fast_io::mnp::leb128_get(align))};
                                  if(align_err != ::fast_io::parse_code::ok) [[unlikely]]
                                  {
                                      err.err_curr = op_begin;
                                      err.err_code = code_validation_error_code::invalid_memarg_align;
                                      ::uwvm2::parser::wasm::base::throw_wasm_parse_code(align_err);
                                  }

                                  // i32.store align offset ...
                                  // [      safe   ] unsafe (could be the section_end)
                                  //           ^^ code_curr

                                  code_curr = reinterpret_cast<::std::byte const*>(align_next);

                                  // i32.store align offset ...
                                  // [      safe   ] unsafe (could be the section_end)
                                  //                 ^^ code_curr

                                  auto const [offset_next, offset_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                                ::fast_io::mnp::leb128_get(offset))};
                                  if(offset_err != ::fast_io::parse_code::ok) [[unlikely]]
                                  {
                                      err.err_curr = op_begin;
                                      err.err_code = code_validation_error_code::invalid_memarg_offset;
                                      ::uwvm2::parser::wasm::base::throw_wasm_parse_code(offset_err);
                                  }

                                  // i32.store align offset ...
                                  // [      safe          ] unsafe (could be the section_end)
                                  //                 ^^ code_curr

                                  code_curr = reinterpret_cast<::std::byte const*>(offset_next);

                                  // i32.store align offset ...
                                  // [      safe          ] unsafe (could be the section_end)
                                  //                        ^^ code_curr

                                  // MVP memory instructions implicitly target memory 0. If the module has no imported/defined memory, any load/store is
                                  // invalid.
                                  if(all_memory_count == 0u) [[unlikely]]
                                  {
                                      err.err_curr = op_begin;
                                      err.err_selectable.no_memory.op_code_name = op_name;
                                      err.err_selectable.no_memory.align = align;
                                      err.err_selectable.no_memory.offset = offset;
                                      err.err_code = code_validation_error_code::no_memory;
                                      ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                  }

                                  if(align > max_align) [[unlikely]]
                                  {
                                      err.err_curr = op_begin;
                                      err.err_selectable.illegal_memarg_alignment.op_code_name = op_name;
                                      err.err_selectable.illegal_memarg_alignment.align = align;
                                      err.err_selectable.illegal_memarg_alignment.max_align = max_align;
                                      err.err_code = code_validation_error_code::illegal_memarg_alignment;
                                      ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                  }

                                  if(!is_polymorphic && concrete_operand_count() < 2uz) [[unlikely]] { report_operand_stack_underflow(op_begin, op_name, 2uz); }

                                  auto const value{try_pop_concrete_operand()};
                                  auto const addr{try_pop_concrete_operand()};

                                  if(!operand_type_matches(addr, wasm_value_type_u::i32)) [[unlikely]]
                                  {
                                      err.err_curr = op_begin;
                                      err.err_selectable.memarg_address_type_not_i32.op_code_name = op_name;
                                      err.err_selectable.memarg_address_type_not_i32.addr_type = to_wasm1_value_type(addr.type);
                                      err.err_code = code_validation_error_code::memarg_address_type_not_i32;
                                      ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                  }

                                  if(!operand_type_matches(value, expected_value_type)) [[unlikely]]
                                  {
                                      err.err_curr = op_begin;
                                      err.err_selectable.store_value_type_mismatch.op_code_name = op_name;
                                      err.err_selectable.store_value_type_mismatch.expected_type = to_wasm1_value_type(expected_value_type);
                                      err.err_selectable.store_value_type_mismatch.actual_type = to_wasm1_value_type(value.type);
                                      err.err_code = code_validation_error_code::store_value_type_mismatch;
                                      ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                  }

                                  return offset;
                              }};

// Resolve memory/global runtime pointers for translation.
// NOTE: The UWVM-int optable currently encodes memory ops against `native_memory_t*` and global ops against `wasm_global_storage_t*`.
// Local-imported (host) memories/globals do not expose these runtime objects, so they are not supported by this compiler backend yet.

using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;
using wasm_global_storage_t = ::uwvm2::object::global::wasm_global_storage_t;

struct resolved_memory0_t
{
    native_memory_t* memory_p{};
    ::std::size_t max_limit_memory_length{};
    bool resolved{};
};

resolved_memory0_t resolved_memory0{};

// Memory resolution is lazy because many functions never touch memory. Once a memory opcode appears,
// cache the resolved native pointer and effective max limit so every following memory helper can use
// compact immediates instead of rewalking import chains.
auto const ensure_memory0_resolved{
    [&]() constexpr UWVM_THROWS
    {
        if(resolved_memory0.resolved) { return; }

        // Convert Wasm memory max pages into a byte-length bound for `native_memory_t::grow_*`.
        auto const max_limit_from_limits{[&](auto const& limits) constexpr noexcept -> ::std::size_t
                                         {
                                             if(!limits.present_max) { return ::std::numeric_limits<::std::size_t>::max(); }
                                             ::std::size_t const max_pages{static_cast<::std::size_t>(limits.max)};
                                             constexpr ::std::size_t wasm_page_bytes{65536uz};
                                             if(max_pages > (::std::numeric_limits<::std::size_t>::max() / wasm_page_bytes))
                                             {
                                                 return ::std::numeric_limits<::std::size_t>::max();
                                             }
                                             return max_pages * wasm_page_bytes;
                                         }};

        auto const backend_max_limit_from_memory{
            [&](native_memory_t const& memory) constexpr noexcept -> ::std::size_t
            {
#if defined(UWVM_SUPPORT_MMAP)
                if constexpr(native_memory_t::can_mmap)
                {
                    if constexpr(sizeof(::std::size_t) >= sizeof(::std::uint_least64_t))
                    {
                        switch(memory.status)
                        {
                            case ::uwvm2::object::memory::linear::mmap_memory_status_t::wasm32:
                            {
                                constexpr auto max_full_protection_wasm32_length_half{::uwvm2::object::memory::linear::max_full_protection_wasm32_length / 2u};
                                return static_cast<::std::size_t>(max_full_protection_wasm32_length_half);
                            }
                            case ::uwvm2::object::memory::linear::mmap_memory_status_t::wasm64:
                            {
                                return static_cast<::std::size_t>(::uwvm2::object::memory::linear::max_partial_protection_wasm64_length);
                            }
                            [[unlikely]] default:
                            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                                return 0uz;
                            }
                        }
                    }
                    else
                    {
                        return static_cast<::std::size_t>(::uwvm2::object::memory::linear::max_partial_protection_wasm32_length);
                    }
                }
                else
                {
                    static_cast<void>(memory);
                    return ::std::numeric_limits<::std::size_t>::max();
                }
#else
                static_cast<void>(memory);
                return ::std::numeric_limits<::std::size_t>::max();
#endif
            }};

        auto const refine_max_limit_for_memory{[&](native_memory_t const& memory, ::std::size_t max_limit_memory_length) constexpr noexcept -> ::std::size_t
                                               {
                                                   auto const backend_max_limit_length{backend_max_limit_from_memory(memory)};
                                                   return backend_max_limit_length < max_limit_memory_length ? backend_max_limit_length
                                                                                                             : max_limit_memory_length;
                                               }};

        if(all_memory_count == 0u) [[unlikely]]
        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
            ::fast_io::fast_terminate();
        }

        if(imported_memory_count != 0u)
        {
            using imported_memory_storage_t = ::uwvm2::uwvm::runtime::storage::imported_memory_storage_t;
            using memory_link_kind = imported_memory_storage_t::imported_memory_link_kind;

            auto const& imported_mem0{curr_module.imported_memory_vec_storage.index_unchecked(0uz)};
            auto const import_type_ptr{imported_mem0.import_type_ptr};
            if(import_type_ptr == nullptr) [[unlikely]]
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                ::fast_io::fast_terminate();
            }

            resolved_memory0.max_limit_memory_length = max_limit_from_limits(imported_mem0.effective_limits);

            // Follow import -> import -> ... chains until we reach a defined memory storage.
            imported_memory_storage_t const* curr{::std::addressof(imported_mem0)};
            for(;;)
            {
                if(curr == nullptr) [[unlikely]]
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::fast_io::fast_terminate();
                }

                switch(curr->link_kind)
                {
                    case memory_link_kind::imported:
                    {
                        curr = curr->target.imported_ptr;
                        continue;
                    }
                    case memory_link_kind::defined:
                    {
                        auto def{curr->target.defined_ptr};
                        if(def == nullptr) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }
                        resolved_memory0.memory_p = ::std::addressof(def->memory);
                        resolved_memory0.max_limit_memory_length = max_limit_from_limits(def->effective_limits);
                        resolved_memory0.max_limit_memory_length = refine_max_limit_for_memory(def->memory, resolved_memory0.max_limit_memory_length);
                        resolved_memory0.resolved = true;
                        return;
                    }
                    case memory_link_kind::local_imported:
                        [[fallthrough]];
                    [[unlikely]] default:
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }
                }
            }
        }
        else
        {
            // Wasm1 MVP: at most one memory. This backend compiles memory index 0.
            auto& local_mem0{curr_module.local_defined_memory_vec_storage.index_unchecked(0uz)};
            resolved_memory0.memory_p = const_cast<native_memory_t*>(::std::addressof(local_mem0.memory));
            auto const mem_type_ptr{local_mem0.memory_type_ptr};
            if(mem_type_ptr == nullptr) [[unlikely]]
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                ::fast_io::fast_terminate();
            }
            resolved_memory0.max_limit_memory_length = max_limit_from_limits(local_mem0.effective_limits);
            resolved_memory0.max_limit_memory_length = refine_max_limit_for_memory(local_mem0.memory, resolved_memory0.max_limit_memory_length);
            resolved_memory0.resolved = true;
        }
    }};

// Global helpers emit direct storage pointers. This resolver follows imported-global forwarding
// chains once at translation time so runtime global get/set opfuncs stay simple and branch-free.
auto const resolve_global_storage_ptr{[&](wasm_u32 global_index) constexpr UWVM_THROWS -> wasm_global_storage_t*
                                      {
                                          using imported_global_storage_t = ::uwvm2::uwvm::runtime::storage::imported_global_storage_t;
                                          using global_link_kind = imported_global_storage_t::imported_global_link_kind;

                                          if(global_index < imported_global_count)
                                          {
                                              imported_global_storage_t const* curr{::std::addressof(
                                                  curr_module.imported_global_vec_storage.index_unchecked(static_cast<::std::size_t>(global_index)))};

                                              for(;;)
                                              {
                                                  if(curr == nullptr) [[unlikely]]
                                                  {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                                      ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                                                      ::fast_io::fast_terminate();
                                                  }

                                                  switch(curr->link_kind)
                                                  {
                                                      case global_link_kind::imported:
                                                      {
                                                          curr = curr->target.imported_ptr;
                                                          continue;
                                                      }
                                                      case global_link_kind::defined:
                                                      {
                                                          auto def{curr->target.defined_ptr};
                                                          if(def == nullptr) [[unlikely]]
                                                          {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                                              ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                                                              ::fast_io::fast_terminate();
                                                          }
                                                          return const_cast<wasm_global_storage_t*>(::std::addressof(def->global));
                                                      }
                                                      case global_link_kind::local_imported:
                                                          [[fallthrough]];
                                                      [[unlikely]] default:
                                                      {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                                          ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                                                          ::fast_io::fast_terminate();
                                                      }
                                                  }
                                              }
                                          }
                                          else
                                          {
                                              auto const local_idx{static_cast<::std::size_t>(global_index - imported_global_count)};
                                              auto& local_global{curr_module.local_defined_global_vec_storage.index_unchecked(local_idx)};
                                              return const_cast<wasm_global_storage_t*>(::std::addressof(local_global.global));
                                          }
                                      }};

// [before_section ... ] | opbase opextent
// [        safe       ] | unsafe (could be the section_end)
//                         ^^ code_curr

// a WebAssembly function with type '() -> ()' (often written as returning “nil”) can have no meaningful code, but it still must have a valid
// instruction sequence—at minimum an end.

using wasm1_code = ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code;

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
// `br_if` fusion state records a compare/eqz site that may be patched only if the immediately next
// opcode is `br_if`. This keeps speculative branch fusion local and prevents stale comparisons from
// crossing unrelated side effects.
enum class br_if_fuse_kind : unsigned
{
    none,

    // cmp/eqz/and ; br_if
    local_tee_nz,
    i32_eqz,
    i32_eq,
    i32_ne,
    i32_lt_s,
    i32_lt_u,
    i32_gt_u,
    i32_ge_s,
    i32_ge_u,
    i32_le_u,
    i32_gt_s,
    i32_le_s,
    i32_and_nz,

    i64_eqz,
    i64_eq,
    i64_ne,
    i64_lt_s,
    i64_gt_u,
    i64_lt_u,
    i64_gt_s,
    i64_ge_s,
    i64_ge_u,
    i64_le_s,
    i64_le_u,

# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
    f32_eq,
    f32_ne,
    f32_eq_localget_rhs,
    f32_ne_localget_rhs,
    f32_lt,
    f32_gt,
    f32_lt_localget_rhs,
    f32_gt_localget_rhs,
    f32_le,
    f32_ge,
    f32_le_localget_rhs,
    f32_ge_localget_rhs,
    f64_eq,
    f64_ne,
    f64_eq_localget_rhs,
    f64_ne_localget_rhs,
    f64_lt,
    f64_gt,
    f64_le,
    f64_ge,
    f64_lt_localget_rhs,
    f64_gt_localget_rhs,
    f64_le_localget_rhs,
    f64_ge_localget_rhs,
    f64_lt_eqz,
# endif
};

// Pending combine state is a tiny single-pass peephole machine. It delays emission only for patterns
// whose next opcode can prove a cheaper helper is correct; unsupported continuations are flushed by
// the dispatch loop before normal opcode handling.
enum class conbine_pending_kind : unsigned
{
    none,
    local_get,
    local_get2,
    local_get_i32_localget,  // off1=addr(i32), off2=value(vt)
    local_get2_const_i32,
    local_get2_const_i64,
    local_get2_const_i32_mul,
    local_get2_const_i32_shl,
    // common: integer update_local on the shared i32/i64 scalar ring
    i32_add_2localget_local_set,
    i32_add_2localget_local_tee,
    i32_add_imm_local_settee_same,
    i32_sub_imm_local_settee_same,
    i32_mul_imm_local_settee_same,
    i32_and_imm_local_settee_same,
    i32_or_imm_local_settee_same,
    i32_xor_imm_local_settee_same,
    i32_shl_imm_local_settee_same,
    i32_shr_s_imm_local_settee_same,
    i32_shr_u_imm_local_settee_same,
    i32_rotl_imm_local_settee_same,
    i32_rotr_imm_local_settee_same,
    i64_add_2localget_local_set,
    i64_add_2localget_local_tee,
    i64_add_imm_local_settee_same,
    i64_sub_imm_local_settee_same,
    i64_mul_imm_local_settee_same,
    i64_and_imm_local_settee_same,
    i64_or_imm_local_settee_same,
    i64_xor_imm_local_settee_same,
    i64_shl_imm_local_settee_same,
    i64_shr_s_imm_local_settee_same,
    i64_shr_u_imm_local_settee_same,
    i64_rotl_imm_local_settee_same,
    i64_rotr_imm_local_settee_same,
    local_get_eqz_i32,
    local_get_eqz_i64,
    local_get_const_i32,
    local_get_const_i64,
    const_i32,
    const_i64,
    local_get_const_i32_cmp_brif,
    local_get_const_i64_cmp_brif,
    local_get_const_i32_add,
    local_get_const_i32_add_localget,

# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
    local_get_const_f32,
    local_get_const_f64,

    // heavy: update_local (local = local (binop) imm)  (local.set/local.tee same local)
    f32_add_imm_local_settee_same,  // off1=local, imm_f32
    f32_mul_imm_local_settee_same,  // off1=local, imm_f32
    f32_sub_imm_local_settee_same,  // off1=local, imm_f32
    f64_add_imm_local_settee_same,  // off1=local, imm_f64
    f64_mul_imm_local_settee_same,  // off1=local, imm_f64
    f64_sub_imm_local_settee_same,  // off1=local, imm_f64

    // heavy: update_local (acc += f.unop(local.get x) -> acc)  (local.set acc)
    f32_acc_add_floor_localget_wait_add,        // off1=acc, off2=x
    f32_acc_add_ceil_localget_wait_add,         // off1=acc, off2=x
    f32_acc_add_trunc_localget_wait_add,        // off1=acc, off2=x
    f32_acc_add_nearest_localget_wait_add,      // off1=acc, off2=x
    f32_acc_add_abs_localget_wait_add,          // off1=acc, off2=x
    f32_acc_add_negabs_localget_wait_const,     // off1=acc, off2=x (next: f32.const -1.0)
    f32_acc_add_negabs_localget_wait_copysign,  // off1=acc, off2=x (next: f32.copysign)
    f32_acc_add_negabs_localget_wait_add,       // off1=acc, off2=x (next: f32.add)

    f32_acc_add_floor_localget_set_acc,    // off1=acc, off2=x
    f32_acc_add_ceil_localget_set_acc,     // off1=acc, off2=x
    f32_acc_add_trunc_localget_set_acc,    // off1=acc, off2=x
    f32_acc_add_nearest_localget_set_acc,  // off1=acc, off2=x
    f32_acc_add_abs_localget_set_acc,      // off1=acc, off2=x
    f32_acc_add_negabs_localget_set_acc,   // off1=acc, off2=x

    f64_acc_add_floor_localget_wait_add,        // off1=acc, off2=x
    f64_acc_add_ceil_localget_wait_add,         // off1=acc, off2=x
    f64_acc_add_trunc_localget_wait_add,        // off1=acc, off2=x
    f64_acc_add_nearest_localget_wait_add,      // off1=acc, off2=x
    f64_acc_add_abs_localget_wait_add,          // off1=acc, off2=x
    f64_acc_add_negabs_localget_wait_const,     // off1=acc, off2=x (next: f64.const -1.0)
    f64_acc_add_negabs_localget_wait_copysign,  // off1=acc, off2=x (next: f64.copysign)
    f64_acc_add_negabs_localget_wait_add,       // off1=acc, off2=x (next: f64.add)

    f64_acc_add_floor_localget_set_acc,    // off1=acc, off2=x
    f64_acc_add_ceil_localget_set_acc,     // off1=acc, off2=x
    f64_acc_add_trunc_localget_set_acc,    // off1=acc, off2=x
    f64_acc_add_nearest_localget_set_acc,  // off1=acc, off2=x
    f64_acc_add_abs_localget_set_acc,      // off1=acc, off2=x
    f64_acc_add_negabs_localget_set_acc,   // off1=acc, off2=x

    // heavy: const-first / imm-stack float patterns
    const_f32,                       // imm_f32
    const_f64,                       // imm_f64
    const_f32_localget,              // off1=src, imm_f32
    const_f64_localget,              // off1=src, imm_f64
    f32_div_from_imm_localtee_wait,  // off1=src, imm_f32
    f64_div_from_imm_localtee_wait,  // off1=src, imm_f64

    // heavy: float mul-add/sub and 2mul patterns
    float_mul_2localget,          // off1, off2, vt={f32|f64}
    float_mul_2localget_local3,   // off1, off2, off3, vt={f32|f64}
    float_2mul_wait_second_mul,   // off1, off2, off3, off4, vt={f32|f64}
    float_2mul_after_second_mul,  // off1, off2, off3, off4, vt={f32|f64}

    // heavy: select(local.get a,b,cond)
    select_localget3,     // off1=a, off2=b, off3=cond, vt={i32|i64|f32|f64}
    select_after_select,  // waiting local.set/local.tee

    // heavy: mac local accumulator (acc + x*y -> acc)
    mac_localget3,  // off1=acc, off2=x, off3=y, vt={i32|i64|f32|f64}
    mac_after_mul,
    mac_after_add,

    // heavy: update_local add (dst = a + b)
    f32_add_2localget_local_set,  // off1, off2, off3=dst
    f32_add_2localget_local_tee,  // off1, off2, off3=dst
    f64_add_2localget_local_set,  // off1, off2, off3=dst
    f64_add_2localget_local_tee,  // off1, off2, off3=dst

    // heavy: br_if fuse (local-based)
    i32_rem_u_2localget_wait_eqz,
    i32_rem_u_eqz_2localget_wait_brif,

#  ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
    // extra-heavy: loop fuse
    for_i32_inc_after_tee,
    for_i32_inc_after_end_const,
    for_i32_inc_after_cmp,
    for_ptr_inc_after_tee,
    for_ptr_inc_after_pend_get,
    for_ptr_inc_after_cmp,
#  endif

    // heavy: f64 loop condition (LLVM loop pattern; e.g. test8 hot loop)
    for_i32_inc_f64_lt_u_eqz_after_gets,
    for_i32_inc_f64_lt_u_eqz_after_step_const,
    for_i32_inc_f64_lt_u_eqz_after_add,
    for_i32_inc_f64_lt_u_eqz_after_tee,
    for_i32_inc_f64_lt_u_eqz_after_convert,
    for_i32_inc_f64_lt_u_eqz_after_cmp,
    for_i32_inc_f64_lt_u_eqz_after_eqz,

    // heavy: bit-mix
    xorshift_pre_shr,                 // off1=x, imm_i32=a
    xorshift_after_shr,               // off1=x, imm_i32=a
    xorshift_after_xor1,              // off1=x, imm_i32=a
    xorshift_after_xor1_getx,         // off1=x, imm_i32=a
    xorshift_after_xor1_getx_constb,  // off1=x, imm_i32=a, imm_i32_2=b
    xorshift_after_shl,               // off1=x, imm_i32=a, imm_i32_2=b

    rot_xor_add_after_rotl,        // off1=x, imm_i32=r
    rot_xor_add_after_gety,        // off1=x, off2=y, imm_i32=r
    rot_xor_add_after_xor,         // off1=x, off2=y, imm_i32=r
    rot_xor_add_after_xor_constc,  // off1=x, off2=y, imm_i32=r, imm_i32_2=c

    rot_xor_add_i64_after_rotl,        // off1=x, imm_i64=r
    rot_xor_add_i64_after_gety,        // off1=x, off2=y, imm_i64=r
    rot_xor_add_i64_after_xor,         // off1=x, off2=y, imm_i64=r
    rot_xor_add_i64_after_xor_constc,  // off1=x, off2=y, imm_i64=r, imm_i64_2=c

    rotl_xor_local_set_after_rotl,  // off1=y, off2=x, imm_i32=r
    rotl_xor_local_set_after_xor,   // off1=y, off2=x, imm_i32=r

    rotl_xor_local_set_i64_after_rotl,  // off1=y, off2=x, imm_i64=r
    rotl_xor_local_set_i64_after_xor,   // off1=y, off2=x, imm_i64=r

    // heavy: compound mem
    u16_copy_scaled_index_after_shl,  // off1=dst, off2=idx, imm_i32=sh
    u16_copy_scaled_index_after_load  // off1=dst, off2=idx, imm_i32=sh, imm_u32=src_off
# endif
};

enum class conbine_brif_cmp_kind : unsigned
{
    none,
    i32_eq,
    i32_ne,
    i32_lt_s,
    i32_lt_u,
    i32_gt_s,
    i32_gt_u,
    i32_le_s,
    i32_le_u,
    i32_ge_s,
    i32_ge_u,
    i64_eq,
    i64_ne,
    i64_lt_s,
    i64_lt_u,
    i64_gt_s,
    i64_gt_u,
    i64_le_s,
    i64_le_u,
    i64_ge_s,
    i64_ge_u,
};

// The payload is intentionally broad because different patterns reuse the same pending state object:
// offsets name compiled-frame locals, immediates preserve parsed constants, and `vt` guards typed
// helper selection.
struct conbine_pending_t
{
    conbine_pending_kind kind{conbine_pending_kind::none};
    conbine_brif_cmp_kind brif_cmp{conbine_brif_cmp_kind::none};
    curr_operand_stack_value_type vt{};
    local_offset_t off1{};
    local_offset_t off2{};
    local_offset_t off3{};
    local_offset_t off4{};
    wasm_i32 imm_i32{};
    wasm_i32 imm_i32_2{};
    wasm_u32 imm_u32{};
    wasm_u32 imm_u32_2{};
    wasm_i64 imm_i64{};
    wasm_i64 imm_i64_2{};

# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
    wasm_f32 imm_f32{};
    wasm_f64 imm_f64{};
# endif
};

struct br_if_fuse_state_t
{
    br_if_fuse_kind kind{br_if_fuse_kind::none};
    ::std::size_t site{SIZE_MAX};  // byte offset within `bytecode` to patch
    ::std::size_t end{SIZE_MAX};   // byte offset at end of the candidate op within `bytecode` (SIZE_MAX => op has no immediates)

    // Stack-top snapshot at the candidate-op entry (used to select the correct fused `br_if_*` specialization).
    ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_currpos_t stacktop_currpos_at_site{};
};

br_if_fuse_state_t br_if_fuse{};
conbine_pending_t conbine_pending{};

auto const runtime_log_conbine_kind_name{[]([[maybe_unused]] conbine_pending_kind k) constexpr noexcept -> ::uwvm2::utils::container::u8string_view
                                         {
                                             switch(k)
                                             {
                                                 case conbine_pending_kind::none: return u8"none";
                                                 case conbine_pending_kind::local_get: return u8"local_get";
                                                 case conbine_pending_kind::local_get2: return u8"local_get2";
                                                 case conbine_pending_kind::local_get_i32_localget: return u8"local_get_i32_localget";
                                                 case conbine_pending_kind::local_get2_const_i32: return u8"local_get2_const_i32";
                                                 case conbine_pending_kind::local_get2_const_i64: return u8"local_get2_const_i64";
                                                 case conbine_pending_kind::local_get2_const_i32_mul: return u8"local_get2_const_i32_mul";
                                                 case conbine_pending_kind::local_get2_const_i32_shl: return u8"local_get2_const_i32_shl";
                                                 case conbine_pending_kind::i32_add_2localget_local_set: return u8"i32_add_2localget_local_set";
                                                 case conbine_pending_kind::i32_add_2localget_local_tee: return u8"i32_add_2localget_local_tee";
                                                 case conbine_pending_kind::i32_add_imm_local_settee_same: return u8"i32_add_imm_local_settee_same";
                                                 case conbine_pending_kind::i32_sub_imm_local_settee_same: return u8"i32_sub_imm_local_settee_same";
                                                 case conbine_pending_kind::i32_mul_imm_local_settee_same: return u8"i32_mul_imm_local_settee_same";
                                                 case conbine_pending_kind::i32_and_imm_local_settee_same: return u8"i32_and_imm_local_settee_same";
                                                 case conbine_pending_kind::i32_or_imm_local_settee_same: return u8"i32_or_imm_local_settee_same";
                                                 case conbine_pending_kind::i32_xor_imm_local_settee_same: return u8"i32_xor_imm_local_settee_same";
                                                 case conbine_pending_kind::i32_shl_imm_local_settee_same: return u8"i32_shl_imm_local_settee_same";
                                                 case conbine_pending_kind::i32_shr_s_imm_local_settee_same: return u8"i32_shr_s_imm_local_settee_same";
                                                 case conbine_pending_kind::i32_shr_u_imm_local_settee_same: return u8"i32_shr_u_imm_local_settee_same";
                                                 case conbine_pending_kind::i32_rotl_imm_local_settee_same: return u8"i32_rotl_imm_local_settee_same";
                                                 case conbine_pending_kind::i32_rotr_imm_local_settee_same: return u8"i32_rotr_imm_local_settee_same";
                                                 case conbine_pending_kind::i64_add_2localget_local_set: return u8"i64_add_2localget_local_set";
                                                 case conbine_pending_kind::i64_add_2localget_local_tee: return u8"i64_add_2localget_local_tee";
                                                 case conbine_pending_kind::i64_add_imm_local_settee_same: return u8"i64_add_imm_local_settee_same";
                                                 case conbine_pending_kind::i64_sub_imm_local_settee_same: return u8"i64_sub_imm_local_settee_same";
                                                 case conbine_pending_kind::i64_mul_imm_local_settee_same: return u8"i64_mul_imm_local_settee_same";
                                                 case conbine_pending_kind::i64_and_imm_local_settee_same: return u8"i64_and_imm_local_settee_same";
                                                 case conbine_pending_kind::i64_or_imm_local_settee_same: return u8"i64_or_imm_local_settee_same";
                                                 case conbine_pending_kind::i64_xor_imm_local_settee_same: return u8"i64_xor_imm_local_settee_same";
                                                 case conbine_pending_kind::i64_shl_imm_local_settee_same: return u8"i64_shl_imm_local_settee_same";
                                                 case conbine_pending_kind::i64_shr_s_imm_local_settee_same: return u8"i64_shr_s_imm_local_settee_same";
                                                 case conbine_pending_kind::i64_shr_u_imm_local_settee_same: return u8"i64_shr_u_imm_local_settee_same";
                                                 case conbine_pending_kind::i64_rotl_imm_local_settee_same: return u8"i64_rotl_imm_local_settee_same";
                                                 case conbine_pending_kind::i64_rotr_imm_local_settee_same: return u8"i64_rotr_imm_local_settee_same";
                                                 case conbine_pending_kind::local_get_eqz_i32: return u8"local_get_eqz_i32";
                                                 case conbine_pending_kind::local_get_eqz_i64: return u8"local_get_eqz_i64";
                                                 case conbine_pending_kind::local_get_const_i32: return u8"local_get_const_i32";
                                                 case conbine_pending_kind::local_get_const_i64: return u8"local_get_const_i64";
                                                 case conbine_pending_kind::const_i32: return u8"const_i32";
                                                 case conbine_pending_kind::const_i64: return u8"const_i64";
                                                 case conbine_pending_kind::local_get_const_i32_cmp_brif: return u8"local_get_const_i32_cmp_brif";
                                                 case conbine_pending_kind::local_get_const_i64_cmp_brif: return u8"local_get_const_i64_cmp_brif";
                                                 case conbine_pending_kind::local_get_const_i32_add: return u8"local_get_const_i32_add";
                                                 case conbine_pending_kind::local_get_const_i32_add_localget: return u8"local_get_const_i32_add_localget";
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
                                                 case conbine_pending_kind::local_get_const_f32: return u8"local_get_const_f32";
                                                 case conbine_pending_kind::local_get_const_f64: return u8"local_get_const_f64";
                                                 case conbine_pending_kind::f32_add_imm_local_settee_same: return u8"f32_add_imm_local_settee_same";
                                                 case conbine_pending_kind::f32_mul_imm_local_settee_same: return u8"f32_mul_imm_local_settee_same";
                                                 case conbine_pending_kind::f32_sub_imm_local_settee_same: return u8"f32_sub_imm_local_settee_same";
                                                 case conbine_pending_kind::f64_add_imm_local_settee_same: return u8"f64_add_imm_local_settee_same";
                                                 case conbine_pending_kind::f64_mul_imm_local_settee_same: return u8"f64_mul_imm_local_settee_same";
                                                 case conbine_pending_kind::f64_sub_imm_local_settee_same: return u8"f64_sub_imm_local_settee_same";
                                                 case conbine_pending_kind::f32_acc_add_floor_localget_wait_add: return u8"f32_acc_add_floor_localget_wait_add";
                                                 case conbine_pending_kind::f32_acc_add_ceil_localget_wait_add: return u8"f32_acc_add_ceil_localget_wait_add";
                                                 case conbine_pending_kind::f32_acc_add_trunc_localget_wait_add: return u8"f32_acc_add_trunc_localget_wait_add";
                                                 case conbine_pending_kind::f32_acc_add_nearest_localget_wait_add:
                                                     return u8"f32_acc_add_nearest_localget_wait_add";
                                                 case conbine_pending_kind::f32_acc_add_abs_localget_wait_add: return u8"f32_acc_add_abs_localget_wait_add";
                                                 case conbine_pending_kind::f32_acc_add_negabs_localget_wait_const:
                                                     return u8"f32_acc_add_negabs_localget_wait_const";
                                                 case conbine_pending_kind::f32_acc_add_negabs_localget_wait_copysign:
                                                     return u8"f32_acc_add_negabs_localget_wait_copysign";
                                                 case conbine_pending_kind::f32_acc_add_negabs_localget_wait_add:
                                                     return u8"f32_acc_add_negabs_localget_wait_add";
                                                 case conbine_pending_kind::f32_acc_add_floor_localget_set_acc: return u8"f32_acc_add_floor_localget_set_acc";
                                                 case conbine_pending_kind::f32_acc_add_ceil_localget_set_acc: return u8"f32_acc_add_ceil_localget_set_acc";
                                                 case conbine_pending_kind::f32_acc_add_trunc_localget_set_acc: return u8"f32_acc_add_trunc_localget_set_acc";
                                                 case conbine_pending_kind::f32_acc_add_nearest_localget_set_acc:
                                                     return u8"f32_acc_add_nearest_localget_set_acc";
                                                 case conbine_pending_kind::f32_acc_add_abs_localget_set_acc: return u8"f32_acc_add_abs_localget_set_acc";
                                                 case conbine_pending_kind::f32_acc_add_negabs_localget_set_acc: return u8"f32_acc_add_negabs_localget_set_acc";
                                                 case conbine_pending_kind::f64_acc_add_floor_localget_wait_add: return u8"f64_acc_add_floor_localget_wait_add";
                                                 case conbine_pending_kind::f64_acc_add_ceil_localget_wait_add: return u8"f64_acc_add_ceil_localget_wait_add";
                                                 case conbine_pending_kind::f64_acc_add_trunc_localget_wait_add: return u8"f64_acc_add_trunc_localget_wait_add";
                                                 case conbine_pending_kind::f64_acc_add_nearest_localget_wait_add:
                                                     return u8"f64_acc_add_nearest_localget_wait_add";
                                                 case conbine_pending_kind::f64_acc_add_abs_localget_wait_add: return u8"f64_acc_add_abs_localget_wait_add";
                                                 case conbine_pending_kind::f64_acc_add_negabs_localget_wait_const:
                                                     return u8"f64_acc_add_negabs_localget_wait_const";
                                                 case conbine_pending_kind::f64_acc_add_negabs_localget_wait_copysign:
                                                     return u8"f64_acc_add_negabs_localget_wait_copysign";
                                                 case conbine_pending_kind::f64_acc_add_negabs_localget_wait_add:
                                                     return u8"f64_acc_add_negabs_localget_wait_add";
                                                 case conbine_pending_kind::f64_acc_add_floor_localget_set_acc: return u8"f64_acc_add_floor_localget_set_acc";
                                                 case conbine_pending_kind::f64_acc_add_ceil_localget_set_acc: return u8"f64_acc_add_ceil_localget_set_acc";
                                                 case conbine_pending_kind::f64_acc_add_trunc_localget_set_acc: return u8"f64_acc_add_trunc_localget_set_acc";
                                                 case conbine_pending_kind::f64_acc_add_nearest_localget_set_acc:
                                                     return u8"f64_acc_add_nearest_localget_set_acc";
                                                 case conbine_pending_kind::f64_acc_add_abs_localget_set_acc: return u8"f64_acc_add_abs_localget_set_acc";
                                                 case conbine_pending_kind::f64_acc_add_negabs_localget_set_acc: return u8"f64_acc_add_negabs_localget_set_acc";
                                                 case conbine_pending_kind::const_f32: return u8"const_f32";
                                                 case conbine_pending_kind::const_f64: return u8"const_f64";
                                                 case conbine_pending_kind::const_f32_localget: return u8"const_f32_localget";
                                                 case conbine_pending_kind::const_f64_localget: return u8"const_f64_localget";
                                                 case conbine_pending_kind::f32_div_from_imm_localtee_wait: return u8"f32_div_from_imm_localtee_wait";
                                                 case conbine_pending_kind::f64_div_from_imm_localtee_wait: return u8"f64_div_from_imm_localtee_wait";
                                                 case conbine_pending_kind::float_mul_2localget: return u8"float_mul_2localget";
                                                 case conbine_pending_kind::float_mul_2localget_local3: return u8"float_mul_2localget_local3";
                                                 case conbine_pending_kind::float_2mul_wait_second_mul: return u8"float_2mul_wait_second_mul";
                                                 case conbine_pending_kind::float_2mul_after_second_mul: return u8"float_2mul_after_second_mul";
                                                 case conbine_pending_kind::select_localget3: return u8"select_localget3";
                                                 case conbine_pending_kind::select_after_select: return u8"select_after_select";
                                                 case conbine_pending_kind::mac_localget3: return u8"mac_localget3";
                                                 case conbine_pending_kind::mac_after_mul: return u8"mac_after_mul";
                                                 case conbine_pending_kind::mac_after_add: return u8"mac_after_add";
                                                 case conbine_pending_kind::f32_add_2localget_local_set: return u8"f32_add_2localget_local_set";
                                                 case conbine_pending_kind::f32_add_2localget_local_tee: return u8"f32_add_2localget_local_tee";
                                                 case conbine_pending_kind::f64_add_2localget_local_set: return u8"f64_add_2localget_local_set";
                                                 case conbine_pending_kind::f64_add_2localget_local_tee: return u8"f64_add_2localget_local_tee";
                                                 case conbine_pending_kind::i32_rem_u_2localget_wait_eqz: return u8"i32_rem_u_2localget_wait_eqz";
                                                 case conbine_pending_kind::i32_rem_u_eqz_2localget_wait_brif: return u8"i32_rem_u_eqz_2localget_wait_brif";
#  ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
                                                 case conbine_pending_kind::for_i32_inc_after_tee: return u8"for_i32_inc_after_tee";
                                                 case conbine_pending_kind::for_i32_inc_after_end_const: return u8"for_i32_inc_after_end_const";
                                                 case conbine_pending_kind::for_i32_inc_after_cmp: return u8"for_i32_inc_after_cmp";
                                                 case conbine_pending_kind::for_ptr_inc_after_tee: return u8"for_ptr_inc_after_tee";
                                                 case conbine_pending_kind::for_ptr_inc_after_pend_get: return u8"for_ptr_inc_after_pend_get";
                                                 case conbine_pending_kind::for_ptr_inc_after_cmp: return u8"for_ptr_inc_after_cmp";
#  endif
                                                 case conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_gets: return u8"for_i32_inc_f64_lt_u_eqz_after_gets";
                                                 case conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_step_const:
                                                     return u8"for_i32_inc_f64_lt_u_eqz_after_step_const";
                                                 case conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_add: return u8"for_i32_inc_f64_lt_u_eqz_after_add";
                                                 case conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_tee: return u8"for_i32_inc_f64_lt_u_eqz_after_tee";
                                                 case conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_convert:
                                                     return u8"for_i32_inc_f64_lt_u_eqz_after_convert";
                                                 case conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_cmp: return u8"for_i32_inc_f64_lt_u_eqz_after_cmp";
                                                 case conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_eqz: return u8"for_i32_inc_f64_lt_u_eqz_after_eqz";
                                                 case conbine_pending_kind::xorshift_pre_shr: return u8"xorshift_pre_shr";
                                                 case conbine_pending_kind::xorshift_after_shr: return u8"xorshift_after_shr";
                                                 case conbine_pending_kind::xorshift_after_xor1: return u8"xorshift_after_xor1";
                                                 case conbine_pending_kind::xorshift_after_xor1_getx: return u8"xorshift_after_xor1_getx";
                                                 case conbine_pending_kind::xorshift_after_xor1_getx_constb: return u8"xorshift_after_xor1_getx_constb";
                                                 case conbine_pending_kind::xorshift_after_shl: return u8"xorshift_after_shl";
                                                 case conbine_pending_kind::rot_xor_add_after_rotl: return u8"rot_xor_add_after_rotl";
                                                 case conbine_pending_kind::rot_xor_add_after_gety: return u8"rot_xor_add_after_gety";
                                                 case conbine_pending_kind::rot_xor_add_after_xor: return u8"rot_xor_add_after_xor";
                                                 case conbine_pending_kind::rot_xor_add_after_xor_constc: return u8"rot_xor_add_after_xor_constc";
                                                 case conbine_pending_kind::rot_xor_add_i64_after_rotl: return u8"rot_xor_add_i64_after_rotl";
                                                 case conbine_pending_kind::rot_xor_add_i64_after_gety: return u8"rot_xor_add_i64_after_gety";
                                                 case conbine_pending_kind::rot_xor_add_i64_after_xor: return u8"rot_xor_add_i64_after_xor";
                                                 case conbine_pending_kind::rot_xor_add_i64_after_xor_constc: return u8"rot_xor_add_i64_after_xor_constc";
                                                 case conbine_pending_kind::rotl_xor_local_set_after_rotl: return u8"rotl_xor_local_set_after_rotl";
                                                 case conbine_pending_kind::rotl_xor_local_set_after_xor: return u8"rotl_xor_local_set_after_xor";
                                                 case conbine_pending_kind::rotl_xor_local_set_i64_after_rotl: return u8"rotl_xor_local_set_i64_after_rotl";
                                                 case conbine_pending_kind::rotl_xor_local_set_i64_after_xor: return u8"rotl_xor_local_set_i64_after_xor";
                                                 case conbine_pending_kind::u16_copy_scaled_index_after_shl: return u8"u16_copy_scaled_index_after_shl";
                                                 case conbine_pending_kind::u16_copy_scaled_index_after_load:
                                                     return u8"u16_copy_scaled_index_after_load";
# endif
                                                 [[unlikely]] default:
                                                     return u8"?";
                                             }
                                         }};

auto const runtime_log_conbine_brif_cmp_name{[]([[maybe_unused]] conbine_brif_cmp_kind k) constexpr noexcept -> ::uwvm2::utils::container::u8string_view
                                             {
                                                 switch(k)
                                                 {
                                                     case conbine_brif_cmp_kind::none: return u8"none";
                                                     case conbine_brif_cmp_kind::i32_eq: return u8"i32_eq";
                                                     case conbine_brif_cmp_kind::i32_ne: return u8"i32_ne";
                                                     case conbine_brif_cmp_kind::i32_lt_s: return u8"i32_lt_s";
                                                     case conbine_brif_cmp_kind::i32_lt_u: return u8"i32_lt_u";
                                                     case conbine_brif_cmp_kind::i32_gt_s: return u8"i32_gt_s";
                                                     case conbine_brif_cmp_kind::i32_gt_u: return u8"i32_gt_u";
                                                     case conbine_brif_cmp_kind::i32_le_s: return u8"i32_le_s";
                                                     case conbine_brif_cmp_kind::i32_le_u: return u8"i32_le_u";
                                                     case conbine_brif_cmp_kind::i32_ge_s: return u8"i32_ge_s";
                                                     case conbine_brif_cmp_kind::i32_ge_u: return u8"i32_ge_u";
                                                     case conbine_brif_cmp_kind::i64_eq: return u8"i64_eq";
                                                     case conbine_brif_cmp_kind::i64_ne: return u8"i64_ne";
                                                     case conbine_brif_cmp_kind::i64_lt_s: return u8"i64_lt_s";
                                                     case conbine_brif_cmp_kind::i64_lt_u: return u8"i64_lt_u";
                                                     case conbine_brif_cmp_kind::i64_gt_s: return u8"i64_gt_s";
                                                     case conbine_brif_cmp_kind::i64_gt_u: return u8"i64_gt_u";
                                                     case conbine_brif_cmp_kind::i64_le_s: return u8"i64_le_s";
                                                     case conbine_brif_cmp_kind::i64_le_u: return u8"i64_le_u";
                                                     case conbine_brif_cmp_kind::i64_ge_s: return u8"i64_ge_s";
                                                     case conbine_brif_cmp_kind::i64_ge_u:
                                                         return u8"i64_ge_u";
                                                     [[unlikely]] default:
                                                         return u8"?";
                                                 }
                                             }};

auto const flush_conbine_pending{
    [&]() constexpr UWVM_THROWS
    {
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

        auto const kind_before{conbine_pending.kind};
        auto const bc_before{bytecode.size()};
        auto const thunks_before{thunks.size()};

        if(runtime_log_on && runtime_log_emit_conbine && kind_before != conbine_pending_kind::none) [[unlikely]]
        {
            wasm1_code next_op{};
            ::std::uint_least8_t next_op_u8{};
            bool const has_next_op{code_curr != code_end};
            if(has_next_op)
            {
                ::std::memcpy(::std::addressof(next_op), code_curr, sizeof(wasm1_code));
                next_op_u8 = static_cast<::std::uint_least8_t>(next_op);
            }

            ::fast_io::io::print(::uwvm2::uwvm::io::u8runtime_log_output,
                                 u8"[uwvm-int-translator] fn=",
                                 function_index,
                                 u8" ip=",
                                 static_cast<::std::size_t>(code_curr - code_begin),
                                 u8" event=conbine.flush.before | kind=",
                                 runtime_log_conbine_kind_name(kind_before),
                                 u8" next_op=",
                                 (has_next_op ? runtime_log_op_name(next_op) : u8"<eof>"),
                                 u8"(",
                                 static_cast<::std::size_t>(next_op_u8),
                                 u8")",
                                 u8" brif=",
                                 runtime_log_conbine_brif_cmp_name(conbine_pending.brif_cmp),
                                 u8" vt=",
                                 runtime_log_vt_name(conbine_pending.vt),
                                 u8" off{",
                                 conbine_pending.off1,
                                 u8",",
                                 conbine_pending.off2,
                                 u8",",
                                 conbine_pending.off3,
                                 u8",",
                                 conbine_pending.off4,
                                 u8"} imm{i32=",
                                 conbine_pending.imm_i32,
                                 u8",i32_2=",
                                 conbine_pending.imm_i32_2,
                                 u8",u32=",
                                 conbine_pending.imm_u32,
                                 u8",u32_2=",
                                 conbine_pending.imm_u32_2,
                                 u8",i64=",
                                 conbine_pending.imm_i64,
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
                                 u8",f32=",
                                 conbine_pending.imm_f32,
                                 u8",f64=",
                                 conbine_pending.imm_f64,
# endif
                                 u8"} bytecode{main=",
                                 bc_before,
                                 u8",thunk=",
                                 thunks_before,
                                 u8"}\n");
        }

        switch(conbine_pending.kind)
        {
            case conbine_pending_kind::none:
            {
                return;
            }
            case conbine_pending_kind::local_get:
            {
                emit_local_get_typed_to(bytecode, conbine_pending.vt, conbine_pending.off1);
                break;
            }
            case conbine_pending_kind::local_get2:
            {
                emit_local_get_typed_to(bytecode, conbine_pending.vt, conbine_pending.off1);
                emit_local_get_typed_to(bytecode, conbine_pending.vt, conbine_pending.off2);
                break;
            }
            case conbine_pending_kind::local_get_i32_localget:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::i32, conbine_pending.off1);
                emit_local_get_typed_to(bytecode, conbine_pending.vt, conbine_pending.off2);
                break;
            }
# ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
            case conbine_pending_kind::const_i32:
            {
                emit_const_i32_to(bytecode, conbine_pending.imm_i32);
                break;
            }
            case conbine_pending_kind::const_i64:
            {
                emit_const_i64_to(bytecode, conbine_pending.imm_i64);
                break;
            }
#  ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
            case conbine_pending_kind::i32_add_2localget_local_set: [[fallthrough]];
            case conbine_pending_kind::i32_add_2localget_local_tee:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_i32_add_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.off2);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
                break;
            }
            case conbine_pending_kind::i64_add_2localget_local_set: [[fallthrough]];
            case conbine_pending_kind::i64_add_2localget_local_tee:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_i64_add_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.off2);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64); }
                break;
            }
#  endif
            case conbine_pending_kind::local_get2_const_i32:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::i32, conbine_pending.off1);
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::i32, conbine_pending.off2);
                emit_const_i32_to(bytecode, conbine_pending.imm_i32);
                break;
            }
            case conbine_pending_kind::local_get2_const_i64:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::i64, conbine_pending.off1);
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::i64, conbine_pending.off2);
                emit_const_i64_to(bytecode, conbine_pending.imm_i64);
                break;
            }
            case conbine_pending_kind::local_get2_const_i32_mul:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::i32, conbine_pending.off1);
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::i32, conbine_pending.off2);
                emit_const_i32_to(bytecode, conbine_pending.imm_i32);
                emit_opfunc_to(bytecode, translate::get_uwvmint_i32_mul_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                if constexpr(stacktop_enabled) { stacktop_after_pop_n_if_reachable(bytecode, 1uz); }
                break;
            }
            case conbine_pending_kind::local_get2_const_i32_shl:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::i32, conbine_pending.off1);
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::i32, conbine_pending.off2);
                emit_const_i32_to(bytecode, conbine_pending.imm_i32);
                emit_opfunc_to(bytecode, translate::get_uwvmint_i32_shl_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                if constexpr(stacktop_enabled) { stacktop_after_pop_n_if_reachable(bytecode, 1uz); }
                break;
            }
            case conbine_pending_kind::i32_add_imm_local_settee_same:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_i32_add_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.imm_i32);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
                break;
            }
            case conbine_pending_kind::i32_sub_imm_local_settee_same:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32); }
                emit_opfunc_to(bytecode,
                               translate::get_uwvmint_i32_binop_imm_localget_fptr_from_tuple<
                                   CompileOption,
                                   ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::sub>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.imm_i32);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
                break;
            }
            case conbine_pending_kind::i32_mul_imm_local_settee_same:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32); }
                emit_opfunc_to(bytecode,
                               translate::get_uwvmint_i32_binop_imm_localget_fptr_from_tuple<
                                   CompileOption,
                                   ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::mul>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.imm_i32);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
                break;
            }
            case conbine_pending_kind::i32_and_imm_local_settee_same:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32); }
                emit_opfunc_to(bytecode,
                               translate::get_uwvmint_i32_binop_imm_localget_fptr_from_tuple<
                                   CompileOption,
                                   ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::and_>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.imm_i32);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
                break;
            }
            case conbine_pending_kind::i32_or_imm_local_settee_same:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32); }
                emit_opfunc_to(bytecode,
                               translate::get_uwvmint_i32_binop_imm_localget_fptr_from_tuple<
                                   CompileOption,
                                   ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::or_>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.imm_i32);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
                break;
            }
            case conbine_pending_kind::i32_xor_imm_local_settee_same:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32); }
                emit_opfunc_to(bytecode,
                               translate::get_uwvmint_i32_binop_imm_localget_fptr_from_tuple<
                                   CompileOption,
                                   ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::xor_>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.imm_i32);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
                break;
            }
            case conbine_pending_kind::i32_shl_imm_local_settee_same:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32); }
                emit_opfunc_to(bytecode,
                               translate::get_uwvmint_i32_binop_imm_localget_fptr_from_tuple<
                                   CompileOption,
                                   ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::shl>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.imm_i32);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
                break;
            }
            case conbine_pending_kind::i32_shr_s_imm_local_settee_same:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32); }
                emit_opfunc_to(bytecode,
                               translate::get_uwvmint_i32_binop_imm_localget_fptr_from_tuple<
                                   CompileOption,
                                   ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::shr_s>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.imm_i32);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
                break;
            }
            case conbine_pending_kind::i32_shr_u_imm_local_settee_same:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32); }
                emit_opfunc_to(bytecode,
                               translate::get_uwvmint_i32_binop_imm_localget_fptr_from_tuple<
                                   CompileOption,
                                   ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::shr_u>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.imm_i32);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
                break;
            }
            case conbine_pending_kind::i32_rotl_imm_local_settee_same:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32); }
                emit_opfunc_to(bytecode,
                               translate::get_uwvmint_i32_binop_imm_localget_fptr_from_tuple<
                                   CompileOption,
                                   ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::rotl>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.imm_i32);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
                break;
            }
            case conbine_pending_kind::i32_rotr_imm_local_settee_same:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32); }
                emit_opfunc_to(bytecode,
                               translate::get_uwvmint_i32_binop_imm_localget_fptr_from_tuple<
                                   CompileOption,
                                   ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::rotr>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.imm_i32);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
                break;
            }
            case conbine_pending_kind::i64_add_imm_local_settee_same:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_i64_add_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.imm_i64);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64); }
                break;
            }
            case conbine_pending_kind::i64_sub_imm_local_settee_same:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64); }
                emit_opfunc_to(bytecode,
                               translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<
                                   CompileOption,
                                   ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::sub>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.imm_i64);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64); }
                break;
            }
            case conbine_pending_kind::i64_mul_imm_local_settee_same:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64); }
                emit_opfunc_to(bytecode,
                               translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<
                                   CompileOption,
                                   ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::mul>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.imm_i64);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64); }
                break;
            }
            case conbine_pending_kind::i64_and_imm_local_settee_same:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64); }
                emit_opfunc_to(bytecode,
                               translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<
                                   CompileOption,
                                   ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::and_>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.imm_i64);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64); }
                break;
            }
            case conbine_pending_kind::i64_or_imm_local_settee_same:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64); }
                emit_opfunc_to(bytecode,
                               translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<
                                   CompileOption,
                                   ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::or_>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.imm_i64);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64); }
                break;
            }
            case conbine_pending_kind::i64_xor_imm_local_settee_same:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64); }
                emit_opfunc_to(bytecode,
                               translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<
                                   CompileOption,
                                   ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::xor_>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.imm_i64);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64); }
                break;
            }
            case conbine_pending_kind::i64_shl_imm_local_settee_same:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64); }
                emit_opfunc_to(bytecode,
                               translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<
                                   CompileOption,
                                   ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::shl>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.imm_i64);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64); }
                break;
            }
            case conbine_pending_kind::i64_shr_s_imm_local_settee_same:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64); }
                emit_opfunc_to(bytecode,
                               translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<
                                   CompileOption,
                                   ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::shr_s>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.imm_i64);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64); }
                break;
            }
            case conbine_pending_kind::i64_shr_u_imm_local_settee_same:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64); }
                emit_opfunc_to(bytecode,
                               translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<
                                   CompileOption,
                                   ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::shr_u>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.imm_i64);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64); }
                break;
            }
            case conbine_pending_kind::i64_rotl_imm_local_settee_same:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64); }
                emit_opfunc_to(bytecode,
                               translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<
                                   CompileOption,
                                   ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::rotl>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.imm_i64);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64); }
                break;
            }
            case conbine_pending_kind::i64_rotr_imm_local_settee_same:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64); }
                emit_opfunc_to(bytecode,
                               translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<
                                   CompileOption,
                                   ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::rotr>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.imm_i64);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64); }
                break;
            }
            case conbine_pending_kind::local_get_eqz_i32:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_i32_eqz_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
                break;
            }
            case conbine_pending_kind::local_get_eqz_i64:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_i64_eqz_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
                break;
            }
            case conbine_pending_kind::local_get_const_i32:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::i32, conbine_pending.off1);
                emit_const_i32_to(bytecode, conbine_pending.imm_i32);
                break;
            }
            case conbine_pending_kind::local_get_const_i32_add:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::i32, conbine_pending.off1);
                emit_const_i32_to(bytecode, conbine_pending.imm_i32);
                emit_opfunc_to(bytecode, translate::get_uwvmint_i32_add_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                if constexpr(stacktop_enabled) { stacktop_after_pop_n_if_reachable(bytecode, 1uz); }
                break;
            }
            case conbine_pending_kind::local_get_const_i32_add_localget:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::i32, conbine_pending.off1);
                emit_const_i32_to(bytecode, conbine_pending.imm_i32);
                emit_opfunc_to(bytecode, translate::get_uwvmint_i32_add_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                if constexpr(stacktop_enabled) { stacktop_after_pop_n_if_reachable(bytecode, 1uz); }
                emit_local_get_typed_to(bytecode, conbine_pending.vt, conbine_pending.off2);
                break;
            }
            case conbine_pending_kind::local_get_const_i64:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::i64, conbine_pending.off1);
                emit_const_i64_to(bytecode, conbine_pending.imm_i64);
                break;
            }
            case conbine_pending_kind::local_get_const_i32_cmp_brif:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32); }
                switch(conbine_pending.brif_cmp)
                {
                    case conbine_brif_cmp_kind::i32_eq:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_eq_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case conbine_brif_cmp_kind::i32_ne:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_ne_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case conbine_brif_cmp_kind::i32_lt_s:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_lt_s_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case conbine_brif_cmp_kind::i32_lt_u:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_lt_u_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case conbine_brif_cmp_kind::i32_gt_s:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_gt_s_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case conbine_brif_cmp_kind::i32_gt_u:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_gt_u_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case conbine_brif_cmp_kind::i32_le_s:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_le_s_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case conbine_brif_cmp_kind::i32_le_u:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_le_u_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case conbine_brif_cmp_kind::i32_ge_s:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_ge_s_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case conbine_brif_cmp_kind::i32_ge_u:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_ge_u_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case conbine_brif_cmp_kind::none:
                    {
                        [[fallthrough]];
                    }
                    [[unlikely]] default:
                    {
#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#  endif
                        ::fast_io::fast_terminate();
                    }
                }

                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.imm_i32);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }

                break;
            }
            case conbine_pending_kind::local_get_const_i64_cmp_brif:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32); }
                switch(conbine_pending.brif_cmp)
                {
                    case conbine_brif_cmp_kind::i64_eq:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_eq_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case conbine_brif_cmp_kind::i64_ne:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_ne_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case conbine_brif_cmp_kind::i64_lt_s:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_lt_s_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case conbine_brif_cmp_kind::i64_lt_u:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_lt_u_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case conbine_brif_cmp_kind::i64_gt_s:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_gt_s_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case conbine_brif_cmp_kind::i64_gt_u:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_gt_u_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case conbine_brif_cmp_kind::i64_le_s:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_le_s_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case conbine_brif_cmp_kind::i64_le_u:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_le_u_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case conbine_brif_cmp_kind::i64_ge_s:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_ge_s_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case conbine_brif_cmp_kind::i64_ge_u:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_ge_u_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case conbine_brif_cmp_kind::none:
                    {
                        [[fallthrough]];
                    }
                    [[unlikely]] default:
                    {
#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#  endif
                        ::fast_io::fast_terminate();
                    }
                }

                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.imm_i64);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }

                break;
            }
#  ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
            case conbine_pending_kind::local_get_const_f32:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f32, conbine_pending.off1);
                emit_const_f32_to(bytecode, conbine_pending.imm_f32);
                break;
            }
            case conbine_pending_kind::local_get_const_f64:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f64, conbine_pending.off1);
                emit_const_f64_to(bytecode, conbine_pending.imm_f64);
                break;
            }
            case conbine_pending_kind::f32_add_imm_local_settee_same:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_f32_add_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.imm_f32);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f32); }
                break;
            }
            case conbine_pending_kind::f32_mul_imm_local_settee_same:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_f32_mul_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.imm_f32);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f32); }
                break;
            }
            case conbine_pending_kind::f32_sub_imm_local_settee_same:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f32, conbine_pending.off1);
                emit_const_f32_to(bytecode, conbine_pending.imm_f32);
                emit_opfunc_to(bytecode, translate::get_uwvmint_f32_sub_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                if constexpr(stacktop_enabled) { stacktop_after_pop_n_if_reachable(bytecode, 1uz); }
                break;
            }
            case conbine_pending_kind::f64_add_imm_local_settee_same:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_f64_add_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.imm_f64);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64); }
                break;
            }
            case conbine_pending_kind::f64_mul_imm_local_settee_same:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_f64_mul_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.imm_f64);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64); }
                break;
            }
            case conbine_pending_kind::f64_sub_imm_local_settee_same:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f64, conbine_pending.off1);
                emit_const_f64_to(bytecode, conbine_pending.imm_f64);
                emit_opfunc_to(bytecode, translate::get_uwvmint_f64_sub_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                if constexpr(stacktop_enabled) { stacktop_after_pop_n_if_reachable(bytecode, 1uz); }
                break;
            }
            case conbine_pending_kind::f32_acc_add_floor_localget_wait_add:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f32, conbine_pending.off1);
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f32, conbine_pending.off2);
                emit_opfunc_to(bytecode, translate::get_uwvmint_f32_floor_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::f32_acc_add_ceil_localget_wait_add:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f32, conbine_pending.off1);
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f32, conbine_pending.off2);
                emit_opfunc_to(bytecode, translate::get_uwvmint_f32_ceil_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::f32_acc_add_trunc_localget_wait_add:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f32, conbine_pending.off1);
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f32, conbine_pending.off2);
                emit_opfunc_to(bytecode, translate::get_uwvmint_f32_trunc_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::f32_acc_add_nearest_localget_wait_add:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f32, conbine_pending.off1);
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f32, conbine_pending.off2);
                emit_opfunc_to(bytecode, translate::get_uwvmint_f32_nearest_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::f32_acc_add_abs_localget_wait_add:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f32, conbine_pending.off1);
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_f32_abs_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off2);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f32); }
                break;
            }
            case conbine_pending_kind::f32_acc_add_negabs_localget_wait_const:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f32, conbine_pending.off1);
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f32, conbine_pending.off2);
                break;
            }
            case conbine_pending_kind::f32_acc_add_negabs_localget_wait_copysign:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f32, conbine_pending.off1);
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f32, conbine_pending.off2);
                emit_const_f32_to(bytecode, static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32>(-1.0f));
                break;
            }
            case conbine_pending_kind::f32_acc_add_negabs_localget_wait_add:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f32, conbine_pending.off1);
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f32, conbine_pending.off2);
                emit_const_f32_to(bytecode, static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32>(-1.0f));
                emit_opfunc_to(bytecode, translate::get_uwvmint_f32_copysign_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                if constexpr(stacktop_enabled) { stacktop_after_pop_n_if_reachable(bytecode, 1uz); }
                break;
            }
            case conbine_pending_kind::f64_acc_add_floor_localget_wait_add:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f64, conbine_pending.off1);
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_f64_floor_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off2);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64); }
                break;
            }
            case conbine_pending_kind::f64_acc_add_ceil_localget_wait_add:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f64, conbine_pending.off1);
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_f64_ceil_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off2);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64); }
                break;
            }
            case conbine_pending_kind::f64_acc_add_trunc_localget_wait_add:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f64, conbine_pending.off1);
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_f64_trunc_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off2);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64); }
                break;
            }
            case conbine_pending_kind::f64_acc_add_nearest_localget_wait_add:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f64, conbine_pending.off1);
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_f64_nearest_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off2);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64); }
                break;
            }
            case conbine_pending_kind::f64_acc_add_abs_localget_wait_add:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f64, conbine_pending.off1);
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_f64_abs_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off2);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64); }
                break;
            }
            case conbine_pending_kind::f64_acc_add_negabs_localget_wait_const:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f64, conbine_pending.off1);
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f64, conbine_pending.off2);
                break;
            }
            case conbine_pending_kind::f64_acc_add_negabs_localget_wait_copysign:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f64, conbine_pending.off1);
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f64, conbine_pending.off2);
                emit_const_f64_to(bytecode, static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>(-1.0));
                break;
            }
            case conbine_pending_kind::f64_acc_add_negabs_localget_wait_add:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f64, conbine_pending.off1);
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f64, conbine_pending.off2);
                emit_const_f64_to(bytecode, static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>(-1.0));
                emit_opfunc_to(bytecode, translate::get_uwvmint_f64_copysign_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                if constexpr(stacktop_enabled) { stacktop_after_pop_n_if_reachable(bytecode, 1uz); }
                break;
            }
            case conbine_pending_kind::f32_acc_add_floor_localget_set_acc:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f32, conbine_pending.off1);
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f32, conbine_pending.off2);
                emit_opfunc_to(bytecode, translate::get_uwvmint_f32_floor_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_opfunc_to(bytecode, translate::get_uwvmint_f32_add_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                if constexpr(stacktop_enabled) { stacktop_after_pop_n_if_reachable(bytecode, 1uz); }
                break;
            }
            case conbine_pending_kind::f32_acc_add_ceil_localget_set_acc:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f32, conbine_pending.off1);
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f32, conbine_pending.off2);
                emit_opfunc_to(bytecode, translate::get_uwvmint_f32_ceil_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_opfunc_to(bytecode, translate::get_uwvmint_f32_add_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                if constexpr(stacktop_enabled) { stacktop_after_pop_n_if_reachable(bytecode, 1uz); }
                break;
            }
            case conbine_pending_kind::f32_acc_add_trunc_localget_set_acc:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f32, conbine_pending.off1);
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f32, conbine_pending.off2);
                emit_opfunc_to(bytecode, translate::get_uwvmint_f32_trunc_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_opfunc_to(bytecode, translate::get_uwvmint_f32_add_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                if constexpr(stacktop_enabled) { stacktop_after_pop_n_if_reachable(bytecode, 1uz); }
                break;
            }
            case conbine_pending_kind::f32_acc_add_nearest_localget_set_acc:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f32, conbine_pending.off1);
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f32, conbine_pending.off2);
                emit_opfunc_to(bytecode, translate::get_uwvmint_f32_nearest_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_opfunc_to(bytecode, translate::get_uwvmint_f32_add_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                if constexpr(stacktop_enabled) { stacktop_after_pop_n_if_reachable(bytecode, 1uz); }
                break;
            }
            case conbine_pending_kind::f32_acc_add_abs_localget_set_acc:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f32, conbine_pending.off1);
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_f32_abs_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off2);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f32); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_f32_add_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                if constexpr(stacktop_enabled) { stacktop_after_pop_n_if_reachable(bytecode, 1uz); }
                break;
            }
            case conbine_pending_kind::f32_acc_add_negabs_localget_set_acc:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f32, conbine_pending.off1);
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f32, conbine_pending.off2);
                emit_const_f32_to(bytecode, static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32>(-1.0f));
                emit_opfunc_to(bytecode, translate::get_uwvmint_f32_copysign_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                if constexpr(stacktop_enabled) { stacktop_after_pop_n_if_reachable(bytecode, 1uz); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_f32_add_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                if constexpr(stacktop_enabled) { stacktop_after_pop_n_if_reachable(bytecode, 1uz); }
                break;
            }
            case conbine_pending_kind::f64_acc_add_floor_localget_set_acc:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f64, conbine_pending.off1);
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_f64_floor_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off2);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_f64_add_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                if constexpr(stacktop_enabled) { stacktop_after_pop_n_if_reachable(bytecode, 1uz); }
                break;
            }
            case conbine_pending_kind::f64_acc_add_ceil_localget_set_acc:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f64, conbine_pending.off1);
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_f64_ceil_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off2);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_f64_add_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                if constexpr(stacktop_enabled) { stacktop_after_pop_n_if_reachable(bytecode, 1uz); }
                break;
            }
            case conbine_pending_kind::f64_acc_add_trunc_localget_set_acc:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f64, conbine_pending.off1);
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_f64_trunc_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off2);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_f64_add_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                if constexpr(stacktop_enabled) { stacktop_after_pop_n_if_reachable(bytecode, 1uz); }
                break;
            }
            case conbine_pending_kind::f64_acc_add_nearest_localget_set_acc:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f64, conbine_pending.off1);
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_f64_nearest_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off2);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_f64_add_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                if constexpr(stacktop_enabled) { stacktop_after_pop_n_if_reachable(bytecode, 1uz); }
                break;
            }
            case conbine_pending_kind::f64_acc_add_abs_localget_set_acc:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f64, conbine_pending.off1);
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_f64_abs_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off2);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_f64_add_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                if constexpr(stacktop_enabled) { stacktop_after_pop_n_if_reachable(bytecode, 1uz); }
                break;
            }
            case conbine_pending_kind::f64_acc_add_negabs_localget_set_acc:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f64, conbine_pending.off1);
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f64, conbine_pending.off2);
                emit_const_f64_to(bytecode, static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>(-1.0));
                emit_opfunc_to(bytecode, translate::get_uwvmint_f64_copysign_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                if constexpr(stacktop_enabled) { stacktop_after_pop_n_if_reachable(bytecode, 1uz); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_f64_add_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                if constexpr(stacktop_enabled) { stacktop_after_pop_n_if_reachable(bytecode, 1uz); }
                break;
            }
            case conbine_pending_kind::const_f32:
            {
                emit_const_f32_to(bytecode, conbine_pending.imm_f32);
                break;
            }
            case conbine_pending_kind::const_f64:
            {
                emit_const_f64_to(bytecode, conbine_pending.imm_f64);
                break;
            }
            case conbine_pending_kind::const_f32_localget:
            {
                emit_const_f32_to(bytecode, conbine_pending.imm_f32);
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f32, conbine_pending.off1);
                break;
            }
            case conbine_pending_kind::const_f64_localget:
            {
                emit_const_f64_to(bytecode, conbine_pending.imm_f64);
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f64, conbine_pending.off1);
                break;
            }
            case conbine_pending_kind::f32_div_from_imm_localtee_wait:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_f32_div_from_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.imm_f32);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f32); }
                break;
            }
            case conbine_pending_kind::f64_div_from_imm_localtee_wait:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_f64_div_from_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.imm_f64);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64); }
                break;
            }
#   ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
            case conbine_pending_kind::float_mul_2localget:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, conbine_pending.vt); }
                if(conbine_pending.vt == curr_operand_stack_value_type::f32)
                {
                    emit_opfunc_to(bytecode, translate::get_uwvmint_f32_mul_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                }
                else
                {
                    emit_opfunc_to(bytecode, translate::get_uwvmint_f64_mul_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                }
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.off2);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(conbine_pending.vt); }

                break;
            }
            case conbine_pending_kind::float_mul_2localget_local3:
            {
                if(conbine_pending.vt == curr_operand_stack_value_type::f32)
                {
                    if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32); }
                    emit_opfunc_to(bytecode, translate::get_uwvmint_f32_mul_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                    emit_imm_to(bytecode, conbine_pending.off1);
                    emit_imm_to(bytecode, conbine_pending.off2);
                    if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f32); }
                    emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f32, conbine_pending.off3);
                }
                else
                {
                    if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64); }
                    emit_opfunc_to(bytecode, translate::get_uwvmint_f64_mul_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                    emit_imm_to(bytecode, conbine_pending.off1);
                    emit_imm_to(bytecode, conbine_pending.off2);
                    if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64); }
                    emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f64, conbine_pending.off3);
                }
                break;
            }
            case conbine_pending_kind::float_2mul_wait_second_mul:
            {
                if(conbine_pending.vt == curr_operand_stack_value_type::f32)
                {
                    if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32); }
                    emit_opfunc_to(bytecode, translate::get_uwvmint_f32_mul_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                    emit_imm_to(bytecode, conbine_pending.off1);
                    emit_imm_to(bytecode, conbine_pending.off2);
                    if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f32); }
                    emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f32, conbine_pending.off3);
                    emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f32, conbine_pending.off4);
                }
                else
                {
                    if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64); }
                    emit_opfunc_to(bytecode, translate::get_uwvmint_f64_mul_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                    emit_imm_to(bytecode, conbine_pending.off1);
                    emit_imm_to(bytecode, conbine_pending.off2);
                    if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64); }
                    emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f64, conbine_pending.off3);
                    emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f64, conbine_pending.off4);
                }
                break;
            }
            case conbine_pending_kind::float_2mul_after_second_mul:
            {
                if(conbine_pending.vt == curr_operand_stack_value_type::f32)
                {
                    if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32); }
                    emit_opfunc_to(bytecode, translate::get_uwvmint_f32_mul_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                    emit_imm_to(bytecode, conbine_pending.off1);
                    emit_imm_to(bytecode, conbine_pending.off2);
                    if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f32); }
                    if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32); }
                    emit_opfunc_to(bytecode, translate::get_uwvmint_f32_mul_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                    emit_imm_to(bytecode, conbine_pending.off3);
                    emit_imm_to(bytecode, conbine_pending.off4);
                    if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f32); }
                }
                else
                {
                    if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64); }
                    emit_opfunc_to(bytecode, translate::get_uwvmint_f64_mul_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                    emit_imm_to(bytecode, conbine_pending.off1);
                    emit_imm_to(bytecode, conbine_pending.off2);
                    if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64); }
                    if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64); }
                    emit_opfunc_to(bytecode, translate::get_uwvmint_f64_mul_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                    emit_imm_to(bytecode, conbine_pending.off3);
                    emit_imm_to(bytecode, conbine_pending.off4);
                    if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64); }
                }
                break;
            }
#   endif
            case conbine_pending_kind::select_localget3:
            {
                emit_local_get_typed_to(bytecode, conbine_pending.vt, conbine_pending.off1);
                emit_local_get_typed_to(bytecode, conbine_pending.vt, conbine_pending.off2);
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::i32, conbine_pending.off3);
                break;
            }
            case conbine_pending_kind::select_after_select:
            {
                emit_local_get_typed_to(bytecode, conbine_pending.vt, conbine_pending.off1);
                emit_local_get_typed_to(bytecode, conbine_pending.vt, conbine_pending.off2);
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::i32, conbine_pending.off3);
                switch(conbine_pending.vt)
                {
                    case curr_operand_stack_value_type::i32:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_select_i32_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case curr_operand_stack_value_type::i64:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_select_i64_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case curr_operand_stack_value_type::f32:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_select_f32_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case curr_operand_stack_value_type::f64:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_select_f64_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    [[unlikely]] default:
                    {
#   if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#   endif
                        break;
                    }
                }
                stacktop_after_pop_n_if_reachable(bytecode, 2uz);
                break;
            }
            case conbine_pending_kind::mac_localget3:
            {
                emit_local_get_typed_to(bytecode, conbine_pending.vt, conbine_pending.off1);
                emit_local_get_typed_to(bytecode, conbine_pending.vt, conbine_pending.off2);
                emit_local_get_typed_to(bytecode, conbine_pending.vt, conbine_pending.off3);
                break;
            }
            case conbine_pending_kind::mac_after_mul:
            {
                emit_local_get_typed_to(bytecode, conbine_pending.vt, conbine_pending.off1);
                emit_local_get_typed_to(bytecode, conbine_pending.vt, conbine_pending.off2);
                emit_local_get_typed_to(bytecode, conbine_pending.vt, conbine_pending.off3);
                if(conbine_pending.vt == curr_operand_stack_value_type::i32)
                {
                    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_mul_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                }
                else if(conbine_pending.vt == curr_operand_stack_value_type::i64)
                {
                    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_mul_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                }
                else if(conbine_pending.vt == curr_operand_stack_value_type::f32)
                {
                    emit_opfunc_to(bytecode, translate::get_uwvmint_f32_mul_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                }
                else
                {
                    emit_opfunc_to(bytecode, translate::get_uwvmint_f64_mul_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                }
                stacktop_after_pop_n_if_reachable(bytecode, 1uz);
                break;
            }
            case conbine_pending_kind::mac_after_add:
            {
                emit_local_get_typed_to(bytecode, conbine_pending.vt, conbine_pending.off1);
                emit_local_get_typed_to(bytecode, conbine_pending.vt, conbine_pending.off2);
                emit_local_get_typed_to(bytecode, conbine_pending.vt, conbine_pending.off3);
                if(conbine_pending.vt == curr_operand_stack_value_type::i32)
                {
                    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_mul_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                    stacktop_after_pop_n_if_reachable(bytecode, 1uz);
                    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_add_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                    stacktop_after_pop_n_if_reachable(bytecode, 1uz);
                }
                else if(conbine_pending.vt == curr_operand_stack_value_type::i64)
                {
                    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_mul_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                    stacktop_after_pop_n_if_reachable(bytecode, 1uz);
                    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_add_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                    stacktop_after_pop_n_if_reachable(bytecode, 1uz);
                }
                else if(conbine_pending.vt == curr_operand_stack_value_type::f32)
                {
                    emit_opfunc_to(bytecode, translate::get_uwvmint_f32_mul_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                    stacktop_after_pop_n_if_reachable(bytecode, 1uz);
                    emit_opfunc_to(bytecode, translate::get_uwvmint_f32_add_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                    stacktop_after_pop_n_if_reachable(bytecode, 1uz);
                }
                else
                {
                    emit_opfunc_to(bytecode, translate::get_uwvmint_f64_mul_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                    stacktop_after_pop_n_if_reachable(bytecode, 1uz);
                    emit_opfunc_to(bytecode, translate::get_uwvmint_f64_add_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                    stacktop_after_pop_n_if_reachable(bytecode, 1uz);
                }
                break;
            }

#   ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
            case conbine_pending_kind::f32_add_2localget_local_set: [[fallthrough]];
            case conbine_pending_kind::f32_add_2localget_local_tee:
            {
                stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32);
                emit_opfunc_to(bytecode, translate::get_uwvmint_f32_add_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.off2);
                stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f32);
                break;
            }
            case conbine_pending_kind::f64_add_2localget_local_set: [[fallthrough]];
            case conbine_pending_kind::f64_add_2localget_local_tee:
            {
                stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64);
                emit_opfunc_to(bytecode, translate::get_uwvmint_f64_add_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.off2);
                stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64);
                break;
            }

            case conbine_pending_kind::i32_rem_u_2localget_wait_eqz:
            {
                stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
                emit_opfunc_to(bytecode, translate::get_uwvmint_i32_rem_u_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.off2);
                stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32);
                break;
            }
            case conbine_pending_kind::i32_rem_u_eqz_2localget_wait_brif:
            {
                stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
                emit_opfunc_to(bytecode, translate::get_uwvmint_i32_rem_u_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.off2);
                stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32);
                emit_opfunc_to(bytecode, translate::get_uwvmint_i32_eqz_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                break;
            }
#   endif

#   ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
            case conbine_pending_kind::for_i32_inc_after_tee:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_i32_add_imm_local_tee_same_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.imm_i32);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
                break;
            }
            case conbine_pending_kind::for_i32_inc_after_end_const:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_i32_add_imm_local_tee_same_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.imm_i32);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
                stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
                emit_opfunc_to(bytecode, translate::get_uwvmint_i32_const_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.imm_i32_2);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
                break;
            }
            case conbine_pending_kind::for_i32_inc_after_cmp:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_i32_add_imm_local_tee_same_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.imm_i32);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
                stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
                emit_opfunc_to(bytecode, translate::get_uwvmint_i32_const_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.imm_i32_2);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_i32_lt_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                stacktop_after_pop_n_if_reachable(bytecode, 1uz);
                break;
            }
            case conbine_pending_kind::for_ptr_inc_after_tee:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_i32_add_imm_local_tee_same_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.imm_i32);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
                break;
            }
            case conbine_pending_kind::for_ptr_inc_after_pend_get:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_i32_add_imm_local_tee_same_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.imm_i32);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::i32, conbine_pending.off2);
                break;
            }
            case conbine_pending_kind::for_ptr_inc_after_cmp:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_i32_add_imm_local_tee_same_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.imm_i32);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::i32, conbine_pending.off2);
                emit_opfunc_to(bytecode, translate::get_uwvmint_i32_ne_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                stacktop_after_pop_n_if_reachable(bytecode, 1uz);
                break;
            }
#   endif
            case conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_gets: [[fallthrough]];
            case conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_step_const: [[fallthrough]];
            case conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_add: [[fallthrough]];
            case conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_tee: [[fallthrough]];
            case conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_convert: [[fallthrough]];
            case conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_cmp: [[fallthrough]];
            case conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_eqz:
            {
                // Sequence:
                //   local.get(sqrt:f64); local.get(i:i32); i32.const step; i32.add; local.tee i;
                //   f64.convert_i32_u; f64.lt; i32.eqz
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::f64, conbine_pending.off1);
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::i32, conbine_pending.off2);
                if(conbine_pending.kind == conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_gets) { break; }

                stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
                emit_opfunc_to(bytecode, translate::get_uwvmint_i32_const_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.imm_i32);
                stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32);
                if(conbine_pending.kind == conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_step_const) { break; }

                emit_opfunc_to(bytecode, translate::get_uwvmint_i32_add_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                stacktop_after_pop_n_if_reachable(bytecode, 1uz);
                if(conbine_pending.kind == conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_add) { break; }

                (void)emit_local_tee_typed_to(bytecode, curr_operand_stack_value_type::i32, conbine_pending.off2);
                if(conbine_pending.kind == conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_tee) { break; }

                if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::f64) &&
                             !stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f64))
                {
                    stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64);
                }
                emit_opfunc_to(bytecode, translate::get_uwvmint_f64_convert_i32_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f64))
                {
                    if constexpr(stacktop_enabled)
                    {
                        if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::f64); }
                    }
                }
                else
                {
                    stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::f64);
                }
                if(conbine_pending.kind == conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_convert) { break; }

                if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i32) &&
                             !stacktop_ranges_merged_for(curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32))
                {
                    stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
                }
                emit_opfunc_to(bytecode, translate::get_uwvmint_f64_lt_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                wasm1_code next_opbase{};  // init
                if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
                if(next_opbase == wasm1_code::br_if)
                {
                    if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32))
                    {
                        stacktop_after_pop_n_no_fill_if_reachable(1uz);
                        if constexpr(stacktop_enabled)
                        {
                            if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::i32); }
                        }
                    }
                    else
                    {
                        stacktop_after_pop_n_push1_typed_no_fill_if_reachable(2uz, curr_operand_stack_value_type::i32);
                    }
                }
                else
                {
                    if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32))
                    {
                        stacktop_after_pop_n_retype_top_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::i32);
                    }
                    else
                    {
                        stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 2uz, curr_operand_stack_value_type::i32);
                    }
                }

                if(conbine_pending.kind == conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_cmp) { break; }

                emit_opfunc_to(bytecode, translate::get_uwvmint_i32_eqz_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::xorshift_pre_shr: [[fallthrough]];
            case conbine_pending_kind::xorshift_after_shr: [[fallthrough]];
            case conbine_pending_kind::xorshift_after_xor1: [[fallthrough]];
            case conbine_pending_kind::xorshift_after_xor1_getx: [[fallthrough]];
            case conbine_pending_kind::xorshift_after_xor1_getx_constb: [[fallthrough]];
            case conbine_pending_kind::xorshift_after_shl:
            {
                // Sequence:
                //   local.get x; local.get x; i32.const a; i32.shr_u; i32.xor;
                //   local.get x; i32.const b; i32.shl; i32.xor
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::i32, conbine_pending.off1);
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::i32, conbine_pending.off1);
                stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
                emit_opfunc_to(bytecode, translate::get_uwvmint_i32_const_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.imm_i32);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
                if(conbine_pending.kind == conbine_pending_kind::xorshift_pre_shr) { break; }
                emit_opfunc_to(bytecode, translate::get_uwvmint_i32_shr_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                stacktop_after_pop_n_if_reachable(bytecode, 1uz);
                if(conbine_pending.kind == conbine_pending_kind::xorshift_after_shr) { break; }
                emit_opfunc_to(bytecode, translate::get_uwvmint_i32_xor_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                stacktop_after_pop_n_if_reachable(bytecode, 1uz);
                if(conbine_pending.kind == conbine_pending_kind::xorshift_after_xor1) { break; }
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::i32, conbine_pending.off1);
                if(conbine_pending.kind == conbine_pending_kind::xorshift_after_xor1_getx) { break; }
                stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
                emit_opfunc_to(bytecode, translate::get_uwvmint_i32_const_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.imm_i32_2);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
                if(conbine_pending.kind == conbine_pending_kind::xorshift_after_xor1_getx_constb) { break; }
                emit_opfunc_to(bytecode, translate::get_uwvmint_i32_shl_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                stacktop_after_pop_n_if_reachable(bytecode, 1uz);
                break;
            }
            case conbine_pending_kind::rot_xor_add_after_rotl: [[fallthrough]];
            case conbine_pending_kind::rot_xor_add_after_gety: [[fallthrough]];
            case conbine_pending_kind::rot_xor_add_after_xor: [[fallthrough]];
            case conbine_pending_kind::rot_xor_add_after_xor_constc:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_i32_rotl_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.imm_i32);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
                if(conbine_pending.kind == conbine_pending_kind::rot_xor_add_after_rotl) { break; }
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::i32, conbine_pending.off2);
                if(conbine_pending.kind == conbine_pending_kind::rot_xor_add_after_gety) { break; }
                emit_opfunc_to(bytecode, translate::get_uwvmint_i32_xor_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                stacktop_after_pop_n_if_reachable(bytecode, 1uz);
                if(conbine_pending.kind == conbine_pending_kind::rot_xor_add_after_xor) { break; }
                stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
                emit_opfunc_to(bytecode, translate::get_uwvmint_i32_const_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.imm_i32_2);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
                break;
            }
            case conbine_pending_kind::rot_xor_add_i64_after_rotl: [[fallthrough]];
            case conbine_pending_kind::rot_xor_add_i64_after_gety: [[fallthrough]];
            case conbine_pending_kind::rot_xor_add_i64_after_xor: [[fallthrough]];
            case conbine_pending_kind::rot_xor_add_i64_after_xor_constc:
            {
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64); }
                emit_opfunc_to(bytecode,
                               translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<
                                   CompileOption,
                                   ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::rotl>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                emit_imm_to(bytecode, conbine_pending.imm_i64);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64); }
                if(conbine_pending.kind == conbine_pending_kind::rot_xor_add_i64_after_rotl) { break; }
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::i64, conbine_pending.off2);
                if(conbine_pending.kind == conbine_pending_kind::rot_xor_add_i64_after_gety) { break; }
                emit_opfunc_to(bytecode, translate::get_uwvmint_i64_xor_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                stacktop_after_pop_n_if_reachable(bytecode, 1uz);
                if(conbine_pending.kind == conbine_pending_kind::rot_xor_add_i64_after_xor) { break; }
                stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
                emit_opfunc_to(bytecode, translate::get_uwvmint_i64_const_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.imm_i64_2);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64); }
                break;
            }
            case conbine_pending_kind::rotl_xor_local_set_after_rotl: [[fallthrough]];
            case conbine_pending_kind::rotl_xor_local_set_after_xor:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::i32, conbine_pending.off1);
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_i32_rotl_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off2);
                emit_imm_to(bytecode, conbine_pending.imm_i32);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
                if(conbine_pending.kind == conbine_pending_kind::rotl_xor_local_set_after_rotl) { break; }
                emit_opfunc_to(bytecode, translate::get_uwvmint_i32_xor_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                stacktop_after_pop_n_if_reachable(bytecode, 1uz);
                break;
            }
            case conbine_pending_kind::rotl_xor_local_set_i64_after_rotl: [[fallthrough]];
            case conbine_pending_kind::rotl_xor_local_set_i64_after_xor:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::i64, conbine_pending.off1);
                if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64); }
                emit_opfunc_to(bytecode,
                               translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<
                                   CompileOption,
                                   ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::rotl>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off2);
                emit_imm_to(bytecode, conbine_pending.imm_i64);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64); }
                if(conbine_pending.kind == conbine_pending_kind::rotl_xor_local_set_i64_after_rotl) { break; }
                emit_opfunc_to(bytecode, translate::get_uwvmint_i64_xor_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                stacktop_after_pop_n_if_reachable(bytecode, 1uz);
                break;
            }
            case conbine_pending_kind::u16_copy_scaled_index_after_shl: [[fallthrough]];
            case conbine_pending_kind::u16_copy_scaled_index_after_load:
            {
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::i32, conbine_pending.off1);
                emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::i32, conbine_pending.off2);
                stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
                emit_opfunc_to(bytecode, translate::get_uwvmint_i32_const_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.imm_i32);
                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
                emit_opfunc_to(bytecode, translate::get_uwvmint_i32_shl_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                stacktop_after_pop_n_if_reachable(bytecode, 1uz);
                if(conbine_pending.kind == conbine_pending_kind::u16_copy_scaled_index_after_shl) { break; }

                ensure_memory0_resolved();
                if constexpr(CompileOption.is_tail_call)
                {
                    emit_opfunc_to(
                        bytecode,
                        translate::get_uwvmint_i32_load16_u_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
                }
                else
                {
                    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_load16_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                }
                emit_imm_to(bytecode, resolved_memory0.memory_p);
                emit_imm_to(bytecode, conbine_pending.imm_u32);
                break;
            }
#  endif
# endif  // UWVM_ENABLE_UWVM_INT_COMBINE_OPS
            [[unlikely]] default:
            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                break;
            }
        }

        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;

        if(runtime_log_on && runtime_log_emit_conbine && kind_before != conbine_pending_kind::none) [[unlikely]]
        {
            wasm1_code next_op{};
            ::std::uint_least8_t next_op_u8{};
            bool const has_next_op{code_curr != code_end};
            if(has_next_op)
            {
                ::std::memcpy(::std::addressof(next_op), code_curr, sizeof(wasm1_code));
                next_op_u8 = static_cast<::std::uint_least8_t>(next_op);
            }

            ::fast_io::io::print(::uwvm2::uwvm::io::u8runtime_log_output,
                                 u8"[uwvm-int-translator] fn=",
                                 function_index,
                                 u8" ip=",
                                 static_cast<::std::size_t>(code_curr - code_begin),
                                 u8" event=conbine.flush.after | kind=",
                                 runtime_log_conbine_kind_name(kind_before),
                                 u8" next_op=",
                                 (has_next_op ? runtime_log_op_name(next_op) : u8"<eof>"),
                                 u8"(",
                                 static_cast<::std::size_t>(next_op_u8),
                                 u8")",
                                 u8" bytecode{main=",
                                 bytecode.size(),
                                 u8"(+",
                                 bytecode.size() - bc_before,
                                 u8"),thunk=",
                                 thunks.size(),
                                 u8"(+",
                                 thunks.size() - thunks_before,
                                 u8")}\n");
        }
    }};

[[maybe_unused]] auto const runtime_uwvm_int_conbine_pending_level{
    [](conbine_pending_kind kind) constexpr noexcept -> runtime_uwvm_int_opcode_conbination_level_t
    {
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
#  ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
        switch(kind)
        {
            case conbine_pending_kind::for_i32_inc_after_tee: [[fallthrough]];
            case conbine_pending_kind::for_i32_inc_after_end_const: [[fallthrough]];
            case conbine_pending_kind::for_i32_inc_after_cmp: [[fallthrough]];
            case conbine_pending_kind::for_ptr_inc_after_tee: [[fallthrough]];
            case conbine_pending_kind::for_ptr_inc_after_pend_get: [[fallthrough]];
            case conbine_pending_kind::for_ptr_inc_after_cmp:
            {
                return runtime_uwvm_int_opcode_conbination_level_t::extra;
            }
            default: break;
        }
#  endif
        auto const kind_u{static_cast<unsigned>(kind)};
        if(kind_u >= static_cast<unsigned>(conbine_pending_kind::local_get_const_f32) &&
           kind_u <= static_cast<unsigned>(conbine_pending_kind::u16_copy_scaled_index_after_load))
        {
            return runtime_uwvm_int_opcode_conbination_level_t::heavy;
        }
# endif
        return kind == conbine_pending_kind::none ? runtime_uwvm_int_opcode_conbination_level_t::disable : runtime_uwvm_int_opcode_conbination_level_t::soft;
    }};

[[maybe_unused]] auto const runtime_uwvm_int_conbine_pending_allowed{
    [&](conbine_pending_kind kind) constexpr noexcept -> bool
    { return runtime_uwvm_int_opcode_conbination_level_at_least(runtime_uwvm_int_opcode_conbination_level, runtime_uwvm_int_conbine_pending_level(kind)); }};

[[maybe_unused]] auto const runtime_uwvm_int_br_if_fuse_allowed{[&](br_if_fuse_kind kind) constexpr noexcept -> bool
                                                                {
                                                                    if(kind == br_if_fuse_kind::none) { return true; }
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
                                                                    auto const kind_u{static_cast<unsigned>(kind)};
                                                                    if(kind_u >= static_cast<unsigned>(br_if_fuse_kind::f32_eq) &&
                                                                       kind_u <= static_cast<unsigned>(br_if_fuse_kind::f64_lt_eqz))
                                                                    {
                                                                        return runtime_uwvm_int_opcode_conbination_heavy_enabled;
                                                                    }
# endif
                                                                    return runtime_uwvm_int_opcode_conbination_enabled;
                                                                }};

/*
 * Pending-fusion continuation policy.
 *
 * `conbine_pending` is the translator's single short-range provider-delay lane. It is used both by
 * ordinary adjacent-op conbine patterns and by delay-local. This predicate is deliberately a safety
 * gate, not the final pattern selector:
 *
 * - Returning true means the current opcode is still inside a known, bytecode-contiguous window that
 *   some later case may consume, specialize, or intentionally flush.
 * - Returning false forces `flush_conbine_pending()` before the opcode is translated, materializing the
 *   delayed provider sequence in its original bytecode order.
 *
 * Delay-local policy:
 *
 * - A pending `local_get` may be continued into selected arithmetic, conversion, memory, or branch
 *   consumers. Those consumers can then emit `*_localget_rhs`/local-slot variants instead of emitting a
 *   standalone `local.get` opfunc.
 * - The validation stack has already seen the normal Wasm push/pop effects. The rearrangement is only in
 *   the emitted runtime bytecode: the local-backed RHS is read at the consumer and the transient RHS stack
 *   slot is never materialized in the operand-stack cache/ring.
 *
 * Conflict handling with conbine:
 *
 * - Larger or more constrained conbine windows are allowed to take priority when they produce a better
 *   semantic unit, for example local+const address forms, update-local skeletons, memory-side fusions, or
 *   branch fusions.
 * - If a delay-local consumer would hide an opcode that another fusion must see, the consumer case can
 *   explicitly flush or decline its variant. Examples include preserving `local.tee + br_if` and
 *   `i32.and + br_if` branch fusions.
 * - Byref/loop mode keeps a narrower continuation set because only a subset of consumers has explicit
 *   byref implementations; unsupported windows are flushed instead of being kept speculatively.
 */
[[maybe_unused]] auto const conbine_can_continue{
    [&](wasm1_code op) constexpr noexcept -> bool
    {
        if(!runtime_uwvm_int_opcode_conbination_enabled) [[unlikely]] { return conbine_pending.kind == conbine_pending_kind::none; }
        if(!runtime_uwvm_int_conbine_pending_allowed(conbine_pending.kind)) [[unlikely]] { return false; }

        if constexpr(!CompileOption.is_tail_call)
        {
            // Byref/loop interpreter mode: keep the non-tailcall combine subset narrow, but still allow
            // the memory-side chains that have explicit byref consumers in `memory_cases.h`:
            // - `local.get addr ; <load>`
            // - `local.get addr ; i32.const imm ; i32.add ; <load>`
            // - `local.get addr ; i32.const imm ; i32.add ; local.get v ; <store>`
            // This preserves bytecode-order safety while making the existing byref local-plus-imm emitters reachable.
            switch(conbine_pending.kind)
            {
                case conbine_pending_kind::none: return true;
                case conbine_pending_kind::local_get:
                    if(conbine_pending.vt == curr_operand_stack_value_type::i32 &&
                       (op == wasm1_code::i32_const || op == wasm1_code::i32_load || op == wasm1_code::i64_load ||
                        (runtime_uwvm_int_opcode_conbination_heavy_enabled && (op == wasm1_code::f32_load || op == wasm1_code::f64_load))))
                    {
                        return true;
                    }
                    break;
                case conbine_pending_kind::local_get_const_i32:
                    if(op == wasm1_code::i32_add) { return true; }
                    break;
                case conbine_pending_kind::local_get_const_i32_add:
                    if(op == wasm1_code::local_get || op == wasm1_code::i32_load ||
                       (runtime_uwvm_int_opcode_conbination_heavy_enabled && (op == wasm1_code::f32_load || op == wasm1_code::f64_load)))
                    {
                        return true;
                    }
                    break;
                case conbine_pending_kind::local_get_const_i32_add_localget:
                    if(conbine_pending.vt == curr_operand_stack_value_type::i32 && op == wasm1_code::i32_store) { return true; }
                    if(runtime_uwvm_int_opcode_conbination_heavy_enabled && conbine_pending.vt == curr_operand_stack_value_type::f32 &&
                       op == wasm1_code::f32_store)
                    {
                        return true;
                    }
                    if(runtime_uwvm_int_opcode_conbination_heavy_enabled && conbine_pending.vt == curr_operand_stack_value_type::f64 &&
                       op == wasm1_code::f64_store)
                    {
                        return true;
                    }
                    break;
                default: break;
            }
        }

        switch(conbine_pending.kind)
        {
            case conbine_pending_kind::none:
            {
                return true;
            }
            case conbine_pending_kind::const_i32:
            {
                switch(op)
                {
                    case wasm1_code::local_set: [[fallthrough]];
                    case wasm1_code::i32_add: [[fallthrough]];
                    case wasm1_code::i32_sub: [[fallthrough]];
                    case wasm1_code::i32_mul: [[fallthrough]];
                    case wasm1_code::i32_and: [[fallthrough]];
                    case wasm1_code::i32_or: [[fallthrough]];
                    case wasm1_code::i32_xor: [[fallthrough]];
                    case wasm1_code::i32_shl: [[fallthrough]];
                    case wasm1_code::i32_shr_s: [[fallthrough]];
                    case wasm1_code::i32_shr_u: [[fallthrough]];
                    case wasm1_code::i32_rotl: [[fallthrough]];
                    case wasm1_code::i32_rotr:
                    {
                        return true;
                    }
                    [[unlikely]] default:
                    {
                        return false;
                    }
                }
            }
            case conbine_pending_kind::const_i64:
            {
                switch(op)
                {
                    case wasm1_code::local_set: [[fallthrough]];
                    case wasm1_code::i64_add: [[fallthrough]];
                    case wasm1_code::i64_sub: [[fallthrough]];
                    case wasm1_code::i64_mul: [[fallthrough]];
                    case wasm1_code::i64_and: [[fallthrough]];
                    case wasm1_code::i64_or: [[fallthrough]];
                    case wasm1_code::i64_xor: [[fallthrough]];
                    case wasm1_code::i64_shl: [[fallthrough]];
                    case wasm1_code::i64_shr_s: [[fallthrough]];
                    case wasm1_code::i64_shr_u: [[fallthrough]];
                    case wasm1_code::i64_rotl: [[fallthrough]];
                    case wasm1_code::i64_rotr:
                    {
                        return true;
                    }
                    [[unlikely]] default:
                    {
                        return false;
                    }
                }
            }
            case conbine_pending_kind::local_get:
            {
                if(op == wasm1_code::local_get) { return true; }
                if(conbine_pending.vt == curr_operand_stack_value_type::i32)
                {
                    return op == wasm1_code::i32_const || op == wasm1_code::i32_eqz || op == wasm1_code::i32_load ||
                           (CompileOption.is_tail_call && (op == wasm1_code::i64_load || op == wasm1_code::f32_load || op == wasm1_code::f64_load)) ||
                           (CompileOption.is_tail_call && (op == wasm1_code::i32_load8_s || op == wasm1_code::i32_load8_u || op == wasm1_code::i32_load16_s ||
                                                           op == wasm1_code::i32_load16_u))
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT
                           || (runtime_uwvm_int_delay_local_enabled && (op == wasm1_code::i32_add || op == wasm1_code::i32_xor))
# endif
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY
                           || (runtime_uwvm_int_delay_local_enabled &&
                               (op == wasm1_code::i32_sub || op == wasm1_code::i32_mul || op == wasm1_code::i32_and || op == wasm1_code::i32_or ||
                                op == wasm1_code::i32_shl || op == wasm1_code::i32_shr_s || op == wasm1_code::i32_shr_u || op == wasm1_code::i32_rotl ||
                                op == wasm1_code::i32_rotr || op == wasm1_code::i32_div_s || op == wasm1_code::i32_div_u || op == wasm1_code::i32_rem_s ||
                                op == wasm1_code::i32_rem_u))
# endif
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
                           || (runtime_uwvm_int_opcode_conbination_heavy_enabled &&
                               (op == wasm1_code::i32_clz || op == wasm1_code::i32_ctz || op == wasm1_code::i32_popcnt || op == wasm1_code::f32_convert_i32_s ||
                                op == wasm1_code::f32_convert_i32_u || op == wasm1_code::f64_convert_i32_s || op == wasm1_code::f64_convert_i32_u ||
                                op == wasm1_code::i64_extend_i32_s || op == wasm1_code::i64_extend_i32_u))
# endif
                        ;
                }
                if(conbine_pending.vt == curr_operand_stack_value_type::i64)
                {
                    return op == wasm1_code::i64_const || op == wasm1_code::i64_eqz
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT
                           || (runtime_uwvm_int_delay_local_enabled && (op == wasm1_code::i64_add || op == wasm1_code::i64_xor))
# endif
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY
                           || (runtime_uwvm_int_delay_local_enabled &&
                               (op == wasm1_code::i64_sub || op == wasm1_code::i64_mul || op == wasm1_code::i64_and || op == wasm1_code::i64_or ||
                                op == wasm1_code::i64_shl || op == wasm1_code::i64_shr_s || op == wasm1_code::i64_shr_u || op == wasm1_code::i64_rotl ||
                                op == wasm1_code::i64_rotr || op == wasm1_code::i64_div_s || op == wasm1_code::i64_div_u || op == wasm1_code::i64_rem_s ||
                                op == wasm1_code::i64_rem_u))
# endif
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
                           || (runtime_uwvm_int_opcode_conbination_heavy_enabled &&
                               (op == wasm1_code::i64_clz || op == wasm1_code::i64_ctz || op == wasm1_code::i64_popcnt))
# endif
                        ;
                }
                if(conbine_pending.vt == curr_operand_stack_value_type::f32)
                {
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT
                    if(runtime_uwvm_int_delay_local_enabled && (op == wasm1_code::f32_add || op == wasm1_code::f32_mul)) { return true; }
# endif
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY
                    if(runtime_uwvm_int_delay_local_enabled && (op == wasm1_code::f32_sub || op == wasm1_code::f32_div || op == wasm1_code::f32_min ||
                                                                op == wasm1_code::f32_max || op == wasm1_code::f32_copysign))
                    {
                        return true;
                    }
# endif
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
                    if(!runtime_uwvm_int_opcode_conbination_heavy_enabled) { return false; }
                    if(op == wasm1_code::f32_eq || op == wasm1_code::f32_ne || op == wasm1_code::f32_lt || op == wasm1_code::f32_gt ||
                       op == wasm1_code::f32_le || op == wasm1_code::f32_ge)
                    {
                        return true;
                    }
                    switch(op)
                    {
                        case wasm1_code::f32_const:
                        case wasm1_code::f32_ceil:
                        case wasm1_code::f32_floor:
                        case wasm1_code::f32_trunc:
                        case wasm1_code::f32_nearest:
                        case wasm1_code::f32_abs:
                        case wasm1_code::f32_neg:
                        case wasm1_code::f32_sqrt:
                        case wasm1_code::i32_trunc_f32_s:
                        case wasm1_code::i32_trunc_f32_u:
                        {
                            return true;
                        }
                        [[unlikely]] default:
                        {
                            return false;
                        }
                    }
# else
                    return false;
# endif
                }
                if(conbine_pending.vt == curr_operand_stack_value_type::f64)
                {
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT
                    if(runtime_uwvm_int_delay_local_enabled && (op == wasm1_code::f64_add || op == wasm1_code::f64_mul)) { return true; }
# endif
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY
                    if(runtime_uwvm_int_delay_local_enabled && (op == wasm1_code::f64_sub || op == wasm1_code::f64_div || op == wasm1_code::f64_min ||
                                                                op == wasm1_code::f64_max || op == wasm1_code::f64_copysign))
                    {
                        return true;
                    }
# endif
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
                    if(!runtime_uwvm_int_opcode_conbination_heavy_enabled) { return false; }
                    if(op == wasm1_code::f64_eq || op == wasm1_code::f64_ne || op == wasm1_code::f64_lt || op == wasm1_code::f64_gt ||
                       op == wasm1_code::f64_le || op == wasm1_code::f64_ge)
                    {
                        return true;
                    }
                    switch(op)
                    {
                        case wasm1_code::f64_const:
                        case wasm1_code::f64_ceil:
                        case wasm1_code::f64_floor:
                        case wasm1_code::f64_trunc:
                        case wasm1_code::f64_nearest:
                        case wasm1_code::f64_abs:
                        case wasm1_code::f64_neg:
                        case wasm1_code::f64_sqrt:
                        case wasm1_code::i32_trunc_f64_s:
                        case wasm1_code::i32_trunc_f64_u:
                        {
                            return true;
                        }
                        [[unlikely]] default:
                        {
                            return false;
                        }
                    }
# else
                    return false;
# endif
                }
                return false;
            }
            case conbine_pending_kind::local_get2:
            {
                // Extra-heavy: allow extending the pending local.get window with another local.get
                // so we can form 3-local fusions such as:
                // - `select(local.get a,b,cond)`
                // - `mac(acc + x*y -> acc)`
                if(op == wasm1_code::local_get) { return runtime_uwvm_int_opcode_conbination_heavy_enabled; }

                if(conbine_pending.vt == curr_operand_stack_value_type::i32)
                {
                    switch(op)
                    {
                        case wasm1_code::i32_const: [[fallthrough]];
                        case wasm1_code::i32_add: [[fallthrough]];
                        case wasm1_code::i32_sub: [[fallthrough]];
                        case wasm1_code::i32_mul: [[fallthrough]];
                        case wasm1_code::i32_and: [[fallthrough]];
                        case wasm1_code::i32_or: [[fallthrough]];
                        case wasm1_code::i32_xor: [[fallthrough]];
                        case wasm1_code::i32_rem_s: [[fallthrough]];
                        case wasm1_code::i32_rem_u: [[fallthrough]];
                        case wasm1_code::i32_load: [[fallthrough]];
                        case wasm1_code::f32_load: [[fallthrough]];
                        case wasm1_code::f64_load: [[fallthrough]];
                        case wasm1_code::i32_load8_s: [[fallthrough]];
                        case wasm1_code::i32_load8_u: [[fallthrough]];
                        case wasm1_code::i32_load16_s: [[fallthrough]];
                        case wasm1_code::i32_load16_u: [[fallthrough]];
                        case wasm1_code::i32_store: [[fallthrough]];
                        case wasm1_code::i32_store8: [[fallthrough]];
                        case wasm1_code::i32_store16:
                        {
                            return true;
                        }
                        [[unlikely]] default:
                        {
                            return false;
                        }
                    }
                }
                if(conbine_pending.vt == curr_operand_stack_value_type::i64)
                {
                    switch(op)
                    {
                        case wasm1_code::i64_add: [[fallthrough]];
                        case wasm1_code::i64_sub: [[fallthrough]];
                        case wasm1_code::i64_mul: [[fallthrough]];
                        case wasm1_code::i64_and: [[fallthrough]];
                        case wasm1_code::i64_or: [[fallthrough]];
                        case wasm1_code::i64_xor: [[fallthrough]];
                        case wasm1_code::i64_shl: [[fallthrough]];
                        case wasm1_code::i64_shr_s: [[fallthrough]];
                        case wasm1_code::i64_shr_u: [[fallthrough]];
                        case wasm1_code::i64_rotl: [[fallthrough]];
                        case wasm1_code::i64_rotr:
                        {
                            return true;
                        }
                        [[unlikely]] default:
                        {
                            return false;
                        }
                    }
                }
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
                if(conbine_pending.vt == curr_operand_stack_value_type::f32)
                {
                    if(!runtime_uwvm_int_opcode_conbination_heavy_enabled) { return false; }
                    switch(op)
                    {
                        case wasm1_code::f32_add: [[fallthrough]];
                        case wasm1_code::f32_sub: [[fallthrough]];
                        case wasm1_code::f32_mul: [[fallthrough]];
                        case wasm1_code::f32_div: [[fallthrough]];
                        case wasm1_code::f32_min: [[fallthrough]];
                        case wasm1_code::f32_max: [[fallthrough]];
                        case wasm1_code::f32_lt:
                        {
                            return true;
                        }
                        [[unlikely]] default:
                        {
                            return false;
                        }
                    }
                }
                if(conbine_pending.vt == curr_operand_stack_value_type::f64)
                {
                    if(!runtime_uwvm_int_opcode_conbination_heavy_enabled) { return false; }
                    switch(op)
                    {
                        case wasm1_code::f64_add: [[fallthrough]];
                        case wasm1_code::f64_sub: [[fallthrough]];
                        case wasm1_code::f64_mul: [[fallthrough]];
                        case wasm1_code::f64_div: [[fallthrough]];
                        case wasm1_code::f64_min: [[fallthrough]];
                        case wasm1_code::f64_max:
                        {
                            return true;
                        }
                        [[unlikely]] default:
                        {
                            return false;
                        }
                    }
                }
                if(op == wasm1_code::local_get) { return runtime_uwvm_int_opcode_conbination_heavy_enabled; }
# endif
                return false;
            }
            case conbine_pending_kind::local_get_i32_localget:
            {
                if(conbine_pending.vt == curr_operand_stack_value_type::i32) { return op == wasm1_code::i32_store; }
                if(conbine_pending.vt == curr_operand_stack_value_type::i64) { return op == wasm1_code::i64_store || op == wasm1_code::i64_store32; }
                return false;
            }
            case conbine_pending_kind::local_get2_const_i32:
            {
                return op == wasm1_code::i32_mul || op == wasm1_code::i32_shl
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
                       || (runtime_uwvm_int_opcode_conbination_heavy_enabled && op == wasm1_code::i32_rotl)
# endif
                    ;
            }
            case conbine_pending_kind::local_get2_const_i64:
            {
                return op == wasm1_code::i64_rotl;
            }
            case conbine_pending_kind::local_get2_const_i32_mul: [[fallthrough]];
            case conbine_pending_kind::local_get2_const_i32_shl:
            {
                return op == wasm1_code::i32_add;
            }
            case conbine_pending_kind::i32_add_2localget_local_set:
            {
                return op == wasm1_code::local_set;
            }
            case conbine_pending_kind::i32_add_2localget_local_tee:
            {
                return op == wasm1_code::local_tee;
            }
            case conbine_pending_kind::i32_add_imm_local_settee_same:
            case conbine_pending_kind::i32_sub_imm_local_settee_same:
            case conbine_pending_kind::i32_mul_imm_local_settee_same:
            case conbine_pending_kind::i32_and_imm_local_settee_same:
            case conbine_pending_kind::i32_or_imm_local_settee_same:
            case conbine_pending_kind::i32_xor_imm_local_settee_same:
            case conbine_pending_kind::i32_shl_imm_local_settee_same:
            case conbine_pending_kind::i32_shr_s_imm_local_settee_same:
            case conbine_pending_kind::i32_shr_u_imm_local_settee_same:
            case conbine_pending_kind::i32_rotl_imm_local_settee_same:
            case conbine_pending_kind::i32_rotr_imm_local_settee_same:
            {
                return op == wasm1_code::local_set || op == wasm1_code::local_tee;
            }
            case conbine_pending_kind::i64_add_2localget_local_set:
            {
                return op == wasm1_code::local_set;
            }
            case conbine_pending_kind::i64_add_2localget_local_tee:
            {
                return op == wasm1_code::local_tee;
            }
            case conbine_pending_kind::i64_add_imm_local_settee_same:
            case conbine_pending_kind::i64_sub_imm_local_settee_same:
            case conbine_pending_kind::i64_mul_imm_local_settee_same:
            case conbine_pending_kind::i64_and_imm_local_settee_same:
            case conbine_pending_kind::i64_or_imm_local_settee_same:
            case conbine_pending_kind::i64_xor_imm_local_settee_same:
            case conbine_pending_kind::i64_shl_imm_local_settee_same:
            case conbine_pending_kind::i64_shr_s_imm_local_settee_same:
            case conbine_pending_kind::i64_shr_u_imm_local_settee_same:
            case conbine_pending_kind::i64_rotl_imm_local_settee_same:
            case conbine_pending_kind::i64_rotr_imm_local_settee_same:
            {
                return op == wasm1_code::local_set || op == wasm1_code::local_tee;
            }
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
            case conbine_pending_kind::f32_add_imm_local_settee_same: [[fallthrough]];
            case conbine_pending_kind::f32_mul_imm_local_settee_same: [[fallthrough]];
            case conbine_pending_kind::f32_sub_imm_local_settee_same: [[fallthrough]];
            case conbine_pending_kind::f64_add_imm_local_settee_same: [[fallthrough]];
            case conbine_pending_kind::f64_mul_imm_local_settee_same: [[fallthrough]];
            case conbine_pending_kind::f64_sub_imm_local_settee_same:
            {
                return op == wasm1_code::local_set || op == wasm1_code::local_tee;
            }
            case conbine_pending_kind::f32_acc_add_floor_localget_wait_add:
            {
                return op == wasm1_code::f32_floor || op == wasm1_code::f32_add;
            }
            case conbine_pending_kind::f32_acc_add_ceil_localget_wait_add:
            {
                return op == wasm1_code::f32_ceil || op == wasm1_code::f32_add;
            }
            case conbine_pending_kind::f32_acc_add_trunc_localget_wait_add:
            {
                return op == wasm1_code::f32_trunc || op == wasm1_code::f32_add;
            }
            case conbine_pending_kind::f32_acc_add_nearest_localget_wait_add:
            {
                return op == wasm1_code::f32_nearest || op == wasm1_code::f32_add;
            }
            case conbine_pending_kind::f32_acc_add_abs_localget_wait_add:
            {
                return op == wasm1_code::f32_abs || op == wasm1_code::f32_add;
            }
            case conbine_pending_kind::f32_acc_add_negabs_localget_wait_const:
            {
                return op == wasm1_code::f32_const;
            }
            case conbine_pending_kind::f32_acc_add_negabs_localget_wait_copysign:
            {
                return op == wasm1_code::f32_copysign;
            }
            case conbine_pending_kind::f32_acc_add_negabs_localget_wait_add:
            {
                return op == wasm1_code::f32_add;
            }
            case conbine_pending_kind::f32_acc_add_floor_localget_set_acc: [[fallthrough]];
            case conbine_pending_kind::f32_acc_add_ceil_localget_set_acc: [[fallthrough]];
            case conbine_pending_kind::f32_acc_add_trunc_localget_set_acc: [[fallthrough]];
            case conbine_pending_kind::f32_acc_add_nearest_localget_set_acc: [[fallthrough]];
            case conbine_pending_kind::f32_acc_add_abs_localget_set_acc: [[fallthrough]];
            case conbine_pending_kind::f32_acc_add_negabs_localget_set_acc:
            {
                return op == wasm1_code::local_set;
            }
            case conbine_pending_kind::f64_acc_add_floor_localget_wait_add:
            {
                return op == wasm1_code::f64_floor || op == wasm1_code::f64_add;
            }
            case conbine_pending_kind::f64_acc_add_ceil_localget_wait_add:
            {
                return op == wasm1_code::f64_ceil || op == wasm1_code::f64_add;
            }
            case conbine_pending_kind::f64_acc_add_trunc_localget_wait_add:
            {
                return op == wasm1_code::f64_trunc || op == wasm1_code::f64_add;
            }
            case conbine_pending_kind::f64_acc_add_nearest_localget_wait_add:
            {
                return op == wasm1_code::f64_nearest || op == wasm1_code::f64_add;
            }
            case conbine_pending_kind::f64_acc_add_abs_localget_wait_add:
            {
                return op == wasm1_code::f64_abs || op == wasm1_code::f64_add;
            }
            case conbine_pending_kind::f64_acc_add_negabs_localget_wait_const:
            {
                return op == wasm1_code::f64_const;
            }
            case conbine_pending_kind::f64_acc_add_negabs_localget_wait_copysign:
            {
                return op == wasm1_code::f64_copysign;
            }
            case conbine_pending_kind::f64_acc_add_negabs_localget_wait_add:
            {
                return op == wasm1_code::f64_add;
            }
            case conbine_pending_kind::f64_acc_add_floor_localget_set_acc: [[fallthrough]];
            case conbine_pending_kind::f64_acc_add_ceil_localget_set_acc: [[fallthrough]];
            case conbine_pending_kind::f64_acc_add_trunc_localget_set_acc: [[fallthrough]];
            case conbine_pending_kind::f64_acc_add_nearest_localget_set_acc: [[fallthrough]];
            case conbine_pending_kind::f64_acc_add_abs_localget_set_acc: [[fallthrough]];
            case conbine_pending_kind::f64_acc_add_negabs_localget_set_acc:
            {
                return op == wasm1_code::local_set;
            }
# endif
            case conbine_pending_kind::local_get_eqz_i32: [[fallthrough]];
            case conbine_pending_kind::local_get_eqz_i64: [[fallthrough]];
            case conbine_pending_kind::local_get_const_i32_cmp_brif: [[fallthrough]];
            case conbine_pending_kind::local_get_const_i64_cmp_brif:
            {
                return op == wasm1_code::br_if;
            }
            case conbine_pending_kind::local_get_const_i32:
            {
                switch(op)
                {
                    case wasm1_code::i32_add: [[fallthrough]];
                    case wasm1_code::i32_sub: [[fallthrough]];
                    case wasm1_code::i32_mul: [[fallthrough]];
                    case wasm1_code::i32_and: [[fallthrough]];
                    case wasm1_code::i32_or: [[fallthrough]];
                    case wasm1_code::i32_xor: [[fallthrough]];
                    case wasm1_code::i32_shl: [[fallthrough]];
                    case wasm1_code::i32_shr_s: [[fallthrough]];
                    case wasm1_code::i32_shr_u: [[fallthrough]];
                    case wasm1_code::i32_eq: [[fallthrough]];
                    case wasm1_code::i32_ne: [[fallthrough]];
                    case wasm1_code::i32_lt_s: [[fallthrough]];
                    case wasm1_code::i32_lt_u: [[fallthrough]];
                    case wasm1_code::i32_gt_s: [[fallthrough]];
                    case wasm1_code::i32_gt_u: [[fallthrough]];
                    case wasm1_code::i32_le_s: [[fallthrough]];
                    case wasm1_code::i32_le_u: [[fallthrough]];
                    case wasm1_code::i32_ge_s: [[fallthrough]];
                    case wasm1_code::i32_ge_u: [[fallthrough]];
                    case wasm1_code::i32_store: [[fallthrough]];
                    case wasm1_code::i32_store8: [[fallthrough]];
                    case wasm1_code::i32_store16: return true;
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
                    case wasm1_code::i32_rotl:
                    case wasm1_code::i32_rotr:
                        return runtime_uwvm_int_opcode_conbination_heavy_enabled;
# endif
                    [[unlikely]] default:
                        return false;
                }
            }
            case conbine_pending_kind::local_get_const_i32_add:
            {
                switch(op)
                {
                    case wasm1_code::local_get: [[fallthrough]];
                    case wasm1_code::i32_load:
                    {
                        return true;
                    }
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
                    case wasm1_code::f32_load: [[fallthrough]];
                    case wasm1_code::f64_load:
                    {
                        return runtime_uwvm_int_opcode_conbination_heavy_enabled;
                    }
# endif
                    [[unlikely]] default:
                    {
                        return false;
                    }
                }
            }
            case conbine_pending_kind::local_get_const_i32_add_localget:
            {
                if(conbine_pending.vt == curr_operand_stack_value_type::i32) { return op == wasm1_code::i32_store; }
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
                if(conbine_pending.vt == curr_operand_stack_value_type::f32)
                {
                    return runtime_uwvm_int_opcode_conbination_heavy_enabled && op == wasm1_code::f32_store;
                }
                if(conbine_pending.vt == curr_operand_stack_value_type::f64)
                {
                    return runtime_uwvm_int_opcode_conbination_heavy_enabled && op == wasm1_code::f64_store;
                }
# endif
                return false;
            }
            case conbine_pending_kind::local_get_const_i64:
            {
                switch(op)
                {
                    case wasm1_code::i64_eq: [[fallthrough]];
                    case wasm1_code::i64_ne: [[fallthrough]];
                    case wasm1_code::i64_lt_s: [[fallthrough]];
                    case wasm1_code::i64_lt_u: [[fallthrough]];
                    case wasm1_code::i64_gt_s: [[fallthrough]];
                    case wasm1_code::i64_gt_u: [[fallthrough]];
                    case wasm1_code::i64_le_s: [[fallthrough]];
                    case wasm1_code::i64_le_u: [[fallthrough]];
                    case wasm1_code::i64_ge_s: [[fallthrough]];
                    case wasm1_code::i64_ge_u: [[fallthrough]];
                    case wasm1_code::i64_add: [[fallthrough]];
                    case wasm1_code::i64_sub: [[fallthrough]];
                    case wasm1_code::i64_mul: [[fallthrough]];
                    case wasm1_code::i64_and: [[fallthrough]];
                    case wasm1_code::i64_or: [[fallthrough]];
                    case wasm1_code::i64_xor: [[fallthrough]];
                    case wasm1_code::i64_shl: [[fallthrough]];
                    case wasm1_code::i64_shr_s: [[fallthrough]];
                    case wasm1_code::i64_shr_u: [[fallthrough]];
                    case wasm1_code::i64_rotl: [[fallthrough]];
                    case wasm1_code::i64_rotr:
                    {
                        return true;
                    }
                    [[unlikely]] default:
                    {
                        return false;
                    }
                }
            }
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
            case conbine_pending_kind::local_get_const_f32: [[fallthrough]];
            case conbine_pending_kind::local_get_const_f64:
            {
                if(conbine_pending.kind == conbine_pending_kind::local_get_const_f32)
                {
                    return op == wasm1_code::f32_add || op == wasm1_code::f32_sub || op == wasm1_code::f32_mul || op == wasm1_code::f32_min ||
                           op == wasm1_code::f32_max;
                }
                return op == wasm1_code::f64_add || op == wasm1_code::f64_sub || op == wasm1_code::f64_mul || op == wasm1_code::f64_min ||
                       op == wasm1_code::f64_max;
            }
            case conbine_pending_kind::const_f32:
            {
                switch(op)
                {
                    case wasm1_code::local_get: [[fallthrough]];
                    case wasm1_code::f32_add: [[fallthrough]];
                    case wasm1_code::f32_sub: [[fallthrough]];
                    case wasm1_code::f32_mul: [[fallthrough]];
                    case wasm1_code::f32_div: [[fallthrough]];
                    case wasm1_code::f32_min: [[fallthrough]];
                    case wasm1_code::f32_max: [[fallthrough]];
                    case wasm1_code::f32_copysign:
                    {
                        return true;
                    }
                    [[unlikely]] default:
                    {
                        return false;
                    }
                }
            }
            case conbine_pending_kind::const_f64:
            {
                switch(op)
                {
                    case wasm1_code::local_get: [[fallthrough]];
                    case wasm1_code::f64_add: [[fallthrough]];
                    case wasm1_code::f64_sub: [[fallthrough]];
                    case wasm1_code::f64_mul: [[fallthrough]];
                    case wasm1_code::f64_div: [[fallthrough]];
                    case wasm1_code::f64_min: [[fallthrough]];
                    case wasm1_code::f64_max: [[fallthrough]];
                    case wasm1_code::f64_copysign:
                    {
                        return true;
                    }
                    [[unlikely]] default:
                    {
                        return false;
                    }
                }
            }
            case conbine_pending_kind::const_f32_localget:
            {
                return op == wasm1_code::f32_div || op == wasm1_code::f32_sub;
            }
            case conbine_pending_kind::const_f64_localget:
            {
                return op == wasm1_code::f64_div || op == wasm1_code::f64_sub;
            }
            case conbine_pending_kind::f32_div_from_imm_localtee_wait:
            {
                return op == wasm1_code::local_tee;
            }
            case conbine_pending_kind::f64_div_from_imm_localtee_wait:
            {
                return op == wasm1_code::local_tee;
            }
            case conbine_pending_kind::float_mul_2localget:
            {
                return op == wasm1_code::local_get;
            }
            case conbine_pending_kind::float_mul_2localget_local3:
            {
                if(op == wasm1_code::local_get) { return true; }
                if(conbine_pending.vt == curr_operand_stack_value_type::f32) { return op == wasm1_code::f32_add || op == wasm1_code::f32_sub; }
                return op == wasm1_code::f64_add || op == wasm1_code::f64_sub;
            }
            case conbine_pending_kind::float_2mul_wait_second_mul:
            {
                return conbine_pending.vt == curr_operand_stack_value_type::f32 ? op == wasm1_code::f32_mul : op == wasm1_code::f64_mul;
            }
            case conbine_pending_kind::float_2mul_after_second_mul:
            {
                if(conbine_pending.vt == curr_operand_stack_value_type::f32) { return op == wasm1_code::f32_add || op == wasm1_code::f32_sub; }
                return op == wasm1_code::f64_add || op == wasm1_code::f64_sub;
            }
            case conbine_pending_kind::select_localget3:
            {
                return op == wasm1_code::select;
            }
            case conbine_pending_kind::select_after_select:
            {
                // Heavy select fusions:
                // - `i32` supports local.set only (no local.tee fused op in conbine_heavy).
                // - `f32` supports both local.set and local.tee.
                return op == wasm1_code::local_set || (op == wasm1_code::local_tee && conbine_pending.vt == curr_operand_stack_value_type::f32);
            }
            case conbine_pending_kind::mac_localget3:
            {
                if(conbine_pending.vt == curr_operand_stack_value_type::i32) { return op == wasm1_code::i32_mul; }
                if(conbine_pending.vt == curr_operand_stack_value_type::i64) { return op == wasm1_code::i64_mul; }
                if(conbine_pending.vt == curr_operand_stack_value_type::f32) { return op == wasm1_code::f32_mul; }
                return op == wasm1_code::f64_mul;
            }
            case conbine_pending_kind::mac_after_mul:
            {
                if(conbine_pending.vt == curr_operand_stack_value_type::i32) { return op == wasm1_code::i32_add; }
                if(conbine_pending.vt == curr_operand_stack_value_type::i64) { return op == wasm1_code::i64_add; }
                if(conbine_pending.vt == curr_operand_stack_value_type::f32) { return op == wasm1_code::f32_add; }
                return op == wasm1_code::f64_add;
            }
            case conbine_pending_kind::mac_after_add:
            {
                // Heavy MAC fusions:
                // - `local.set acc` supports {i32,i64,f32,f64}.
                // - `local.tee acc` supports {i32,i64,f32,f64}. (See `case wasm1_code::local_tee`.)
                return op == wasm1_code::local_set || op == wasm1_code::local_tee;
            }
            case conbine_pending_kind::f32_add_2localget_local_set:
            {
                return op == wasm1_code::local_set;
            }
            case conbine_pending_kind::f32_add_2localget_local_tee:
            {
                return op == wasm1_code::local_tee;
            }
            case conbine_pending_kind::f64_add_2localget_local_set:
            {
                return op == wasm1_code::local_set;
            }
            case conbine_pending_kind::f64_add_2localget_local_tee:
            {
                return op == wasm1_code::local_tee;
            }
            case conbine_pending_kind::i32_rem_u_2localget_wait_eqz:
            {
                return op == wasm1_code::i32_eqz;
            }
            case conbine_pending_kind::i32_rem_u_eqz_2localget_wait_brif:
            {
                return op == wasm1_code::br_if;
            }
#  ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
            case conbine_pending_kind::for_i32_inc_after_tee:
            {
                return op == wasm1_code::i32_const;
            }
            case conbine_pending_kind::for_i32_inc_after_end_const:
            {
                return op == wasm1_code::i32_lt_u;
            }
            case conbine_pending_kind::for_i32_inc_after_cmp:
            {
                return op == wasm1_code::br_if;
            }
            case conbine_pending_kind::for_ptr_inc_after_tee:
            {
                return op == wasm1_code::local_get;
            }
            case conbine_pending_kind::for_ptr_inc_after_pend_get:
            {
                return op == wasm1_code::i32_ne;
            }
            case conbine_pending_kind::for_ptr_inc_after_cmp:
            {
                return op == wasm1_code::br_if;
            }
#  endif
            case conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_gets:
            {
                return op == wasm1_code::i32_const;
            }
            case conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_step_const:
            {
                return op == wasm1_code::i32_add;
            }
            case conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_add:
            {
                return op == wasm1_code::local_tee;
            }
            case conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_tee:
            {
                return op == wasm1_code::f64_convert_i32_u;
            }
            case conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_convert:
            {
                return op == wasm1_code::f64_lt;
            }
            case conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_cmp:
            {
                return op == wasm1_code::i32_eqz;
            }
            case conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_eqz:
            {
                return op == wasm1_code::br_if;
            }
            case conbine_pending_kind::xorshift_pre_shr:
            {
                return op == wasm1_code::i32_shr_u;
            }
            case conbine_pending_kind::xorshift_after_shr:
            {
                return op == wasm1_code::i32_xor;
            }
            case conbine_pending_kind::xorshift_after_xor1:
            {
                return op == wasm1_code::local_get;
            }
            case conbine_pending_kind::xorshift_after_xor1_getx:
            {
                return op == wasm1_code::i32_const;
            }
            case conbine_pending_kind::xorshift_after_xor1_getx_constb:
            {
                return op == wasm1_code::i32_shl;
            }
            case conbine_pending_kind::xorshift_after_shl:
            {
                return op == wasm1_code::i32_xor;
            }
            case conbine_pending_kind::rot_xor_add_after_rotl:
            {
                return op == wasm1_code::local_get;
            }
            case conbine_pending_kind::rot_xor_add_after_gety:
            {
                return op == wasm1_code::i32_xor;
            }
            case conbine_pending_kind::rot_xor_add_after_xor:
            {
                return op == wasm1_code::i32_const;
            }
            case conbine_pending_kind::rot_xor_add_after_xor_constc:
            {
                return op == wasm1_code::i32_add;
            }
            case conbine_pending_kind::rot_xor_add_i64_after_rotl:
            {
                return op == wasm1_code::local_get;
            }
            case conbine_pending_kind::rot_xor_add_i64_after_gety:
            {
                return op == wasm1_code::i64_xor;
            }
            case conbine_pending_kind::rot_xor_add_i64_after_xor:
            {
                return op == wasm1_code::i64_const;
            }
            case conbine_pending_kind::rot_xor_add_i64_after_xor_constc:
            {
                return op == wasm1_code::i64_add;
            }
            case conbine_pending_kind::rotl_xor_local_set_after_rotl:
            {
                return op == wasm1_code::i32_xor;
            }
            case conbine_pending_kind::rotl_xor_local_set_after_xor:
            {
                return op == wasm1_code::local_set || op == wasm1_code::local_tee;
            }
            case conbine_pending_kind::rotl_xor_local_set_i64_after_rotl:
            {
                return op == wasm1_code::i64_xor;
            }
            case conbine_pending_kind::rotl_xor_local_set_i64_after_xor:
            {
                return op == wasm1_code::local_set || op == wasm1_code::local_tee;
            }
            case conbine_pending_kind::u16_copy_scaled_index_after_shl:
            {
                return op == wasm1_code::i32_load16_u;
            }
            case conbine_pending_kind::u16_copy_scaled_index_after_load:
            {
                return op == wasm1_code::i32_store16;
            }
# endif
            [[unlikely]] default:
            {
                return false;
            }
        }
    }};
#endif

// Per-op logging snapshots both bytecode deltas and abstract stack/cache state. It is noisy by
// design and therefore gated behind `runtime_log_emit_wasm_ops`.
auto const runtime_log_wasm_op_state{[&]([[maybe_unused]] ::uwvm2::utils::container::u8string_view phase,
                                         [[maybe_unused]] wasm1_code op,
                                         [[maybe_unused]] ::std::byte const* wasm_ip,
                                         [[maybe_unused]] ::std::size_t bytecode_before,
                                         [[maybe_unused]] ::std::size_t thunks_before,
                                         [[maybe_unused]] ::std::uint_least64_t opfunc_main_before,
                                         [[maybe_unused]] ::std::uint_least64_t opfunc_thunk_before) constexpr noexcept
                                     {
                                         if(!runtime_log_on || !runtime_log_emit_wasm_ops) [[likely]] { return; }

                                         auto const bc_delta{bytecode.size() - bytecode_before};
                                         auto const thunk_delta{thunks.size() - thunks_before};
                                         auto const opfunc_main_delta{runtime_log_stats.opfunc_main_count - opfunc_main_before};
                                         auto const opfunc_thunk_delta{runtime_log_stats.opfunc_thunk_count - opfunc_thunk_before};
                                         using op_underlying_t = ::std::underlying_type_t<wasm1_code>;
                                         auto const op_u{static_cast<::std::uint_least32_t>(static_cast<op_underlying_t>(op))};

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
                                         if(conbine_pending.kind == conbine_pending_kind::none)
                                         {
                                             ::fast_io::io::print(::uwvm2::uwvm::io::u8runtime_log_output,
                                                                  u8"[uwvm-int-translator] fn=",
                                                                  function_index,
                                                                  u8" ip=",
                                                                  static_cast<::std::size_t>(wasm_ip - code_begin),
                                                                  u8" event=",
                                                                  phase,
                                                                  u8" | op=",
                                                                  runtime_log_op_name(op),
                                                                  u8" op_u=",
                                                                  ::fast_io::mnp::hex0x(op_u),
                                                                  u8" bytecode{main=",
                                                                  bytecode.size(),
                                                                  u8"(+",
                                                                  bc_delta,
                                                                  u8"),thunk=",
                                                                  thunks.size(),
                                                                  u8"(+",
                                                                  thunk_delta,
                                                                  u8")} opfunc{main=+",
                                                                  opfunc_main_delta,
                                                                  u8",thunk=+",
                                                                  opfunc_thunk_delta,
                                                                  u8"}",
                                                                  u8" operand{sz=",
                                                                  operand_stack.size(),
                                                                  u8",bytes=",
                                                                  operand_stack_bytes,
                                                                  u8"} polymorphic=",
                                                                  is_polymorphic,
                                                                  u8" conbine{none}",
                                                                  u8" stacktop{mem=",
                                                                  stacktop_memory_count,
                                                                  u8",cache=",
                                                                  stacktop_cache_count,
                                                                  u8",i32=",
                                                                  stacktop_cache_i32_count,
                                                                  u8",i64=",
                                                                  stacktop_cache_i64_count,
                                                                  u8",f32=",
                                                                  stacktop_cache_f32_count,
                                                                  u8",f64=",
                                                                  stacktop_cache_f64_count,
                                                                  u8"} currpos{i32=",
                                                                  curr_stacktop.i32_stack_top_curr_pos,
                                                                  u8",i64=",
                                                                  curr_stacktop.i64_stack_top_curr_pos,
                                                                  u8",f32=",
                                                                  curr_stacktop.f32_stack_top_curr_pos,
                                                                  u8",f64=",
                                                                  curr_stacktop.f64_stack_top_curr_pos,
                                                                  u8",v128=",
                                                                  curr_stacktop.v128_stack_top_curr_pos,
                                                                  u8"}\n");
                                         }
                                         else
                                         {
                                             ::fast_io::io::print(::uwvm2::uwvm::io::u8runtime_log_output,
                                                                  u8"[uwvm-int-translator] fn=",
                                                                  function_index,
                                                                  u8" ip=",
                                                                  static_cast<::std::size_t>(wasm_ip - code_begin),
                                                                  u8" event=",
                                                                  phase,
                                                                  u8" | op=",
                                                                  runtime_log_op_name(op),
                                                                  u8" op_u=",
                                                                  ::fast_io::mnp::hex0x(op_u),
                                                                  u8" bytecode{main=",
                                                                  bytecode.size(),
                                                                  u8"(+",
                                                                  bc_delta,
                                                                  u8"),thunk=",
                                                                  thunks.size(),
                                                                  u8"(+",
                                                                  thunk_delta,
                                                                  u8")} opfunc{main=+",
                                                                  opfunc_main_delta,
                                                                  u8",thunk=+",
                                                                  opfunc_thunk_delta,
                                                                  u8"}",
                                                                  u8" operand{sz=",
                                                                  operand_stack.size(),
                                                                  u8",bytes=",
                                                                  operand_stack_bytes,
                                                                  u8"} polymorphic=",
                                                                  is_polymorphic,
                                                                  u8" conbine{kind=",
                                                                  runtime_log_conbine_kind_name(conbine_pending.kind),
                                                                  u8",brif=",
                                                                  runtime_log_conbine_brif_cmp_name(conbine_pending.brif_cmp),
                                                                  u8",vt=",
                                                                  runtime_log_vt_name(conbine_pending.vt),
                                                                  u8",off{",
                                                                  conbine_pending.off1,
                                                                  u8",",
                                                                  conbine_pending.off2,
                                                                  u8",",
                                                                  conbine_pending.off3,
                                                                  u8",",
                                                                  conbine_pending.off4,
                                                                  u8"} imm{i32=",
                                                                  conbine_pending.imm_i32,
                                                                  u8",i32_2=",
                                                                  conbine_pending.imm_i32_2,
                                                                  u8",u32=",
                                                                  conbine_pending.imm_u32,
                                                                  u8",u32_2=",
                                                                  conbine_pending.imm_u32_2,
                                                                  u8",i64=",
                                                                  conbine_pending.imm_i64,
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
                                                                  u8",f32=",
                                                                  conbine_pending.imm_f32,
                                                                  u8",f64=",
                                                                  conbine_pending.imm_f64,
# endif
                                                                  u8"}}",
                                                                  u8" stacktop{mem=",
                                                                  stacktop_memory_count,
                                                                  u8",cache=",
                                                                  stacktop_cache_count,
                                                                  u8",i32=",
                                                                  stacktop_cache_i32_count,
                                                                  u8",i64=",
                                                                  stacktop_cache_i64_count,
                                                                  u8",f32=",
                                                                  stacktop_cache_f32_count,
                                                                  u8",f64=",
                                                                  stacktop_cache_f64_count,
                                                                  u8"} currpos{i32=",
                                                                  curr_stacktop.i32_stack_top_curr_pos,
                                                                  u8",i64=",
                                                                  curr_stacktop.i64_stack_top_curr_pos,
                                                                  u8",f32=",
                                                                  curr_stacktop.f32_stack_top_curr_pos,
                                                                  u8",f64=",
                                                                  curr_stacktop.f64_stack_top_curr_pos,
                                                                  u8",v128=",
                                                                  curr_stacktop.v128_stack_top_curr_pos,
                                                                  u8"}\n");
                                         }

#else
                                         ::fast_io::io::print(::uwvm2::uwvm::io::u8runtime_log_output,
                                                              u8"[uwvm-int-translator] fn=",
                                                              function_index,
                                                              u8" ip=",
                                                              static_cast<::std::size_t>(wasm_ip - code_begin),
                                                              u8" event=",
                                                              phase,
                                                              u8" | op=",
                                                              runtime_log_op_name(op),
                                                              u8" op_u=",
                                                              ::fast_io::mnp::hex0x(op_u),
                                                              u8" bytecode{main=",
                                                              bytecode.size(),
                                                              u8"(+",
                                                              bc_delta,
                                                              u8"),thunk=",
                                                              thunks.size(),
                                                              u8"(+",
                                                              thunk_delta,
                                                              u8")} opfunc{main=+",
                                                              opfunc_main_delta,
                                                              u8",thunk=+",
                                                              opfunc_thunk_delta,
                                                              u8"}",
                                                              u8" operand{sz=",
                                                              operand_stack.size(),
                                                              u8",bytes=",
                                                              operand_stack_bytes,
                                                              u8"} polymorphic=",
                                                              is_polymorphic,
                                                              u8" conbine{none}",
                                                              u8" stacktop{mem=",
                                                              stacktop_memory_count,
                                                              u8",cache=",
                                                              stacktop_cache_count,
                                                              u8",i32=",
                                                              stacktop_cache_i32_count,
                                                              u8",i64=",
                                                              stacktop_cache_i64_count,
                                                              u8",f32=",
                                                              stacktop_cache_f32_count,
                                                              u8",f64=",
                                                              stacktop_cache_f64_count,
                                                              u8"} currpos{i32=",
                                                              curr_stacktop.i32_stack_top_curr_pos,
                                                              u8",i64=",
                                                              curr_stacktop.i64_stack_top_curr_pos,
                                                              u8",f32=",
                                                              curr_stacktop.f32_stack_top_curr_pos,
                                                              u8",f64=",
                                                              curr_stacktop.f64_stack_top_curr_pos,
                                                              u8",v128=",
                                                              curr_stacktop.v128_stack_top_curr_pos,
                                                              u8"}\n");
#endif
                                     }};

bool finished_current_func{};

auto const translate_one_opcode{
    [&](auto const& translate_one_opcode_self) constexpr UWVM_THROWS -> void
    {
        if(code_curr == code_end) [[unlikely]]
        {
            // [... ] | (end)
            // [safe] | unsafe (could be the section_end)
            //          ^^ code_curr

            // Validation completes when the end is reached, so this condition can never be met. If it were met, it would indicate a missing end.

            err.err_curr = code_curr;
            err.err_code = code_validation_error_code::missing_end;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        // opbase ...
        // [safe] unsafe (could be the section_end)
        // ^^ code_curr

        auto const op_begin{code_curr};

        wasm1_code curr_opbase;  // no initialize necessary
        ::std::memcpy(::std::addressof(curr_opbase), code_curr, sizeof(wasm1_code));
        if(runtime_log_on && runtime_log_emit_wasm_ops) [[unlikely]]
        {
            runtime_log_curr_ip = static_cast<::std::size_t>(op_begin - code_begin);
            ++runtime_log_stats.wasm_op_count;
        }

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        stacktop_dbg_last_op = curr_opbase;
        stacktop_dbg_last_ip = static_cast<::std::size_t>(op_begin - code_begin);
#endif

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
        if(!runtime_uwvm_int_opcode_conbination_enabled) [[unlikely]]
        {
            if(br_if_fuse.kind != br_if_fuse_kind::none)
            {
                br_if_fuse.kind = br_if_fuse_kind::none;
                br_if_fuse.site = SIZE_MAX;
                br_if_fuse.end = SIZE_MAX;
                br_if_fuse.stacktop_currpos_at_site = {};
            }
            if(conbine_pending.kind != conbine_pending_kind::none) { flush_conbine_pending(); }
        }
        else
        {
            // Combine state: only fuse if the next opcode is immediately `br_if`.
            if(br_if_fuse.kind != br_if_fuse_kind::none && (!runtime_uwvm_int_br_if_fuse_allowed(br_if_fuse.kind) || curr_opbase != wasm1_code::br_if))
                [[unlikely]]
            {
                br_if_fuse.kind = br_if_fuse_kind::none;
                br_if_fuse.site = SIZE_MAX;
                br_if_fuse.end = SIZE_MAX;
                br_if_fuse.stacktop_currpos_at_site = {};
            }

            // Conbine state machine: flush pending ops unless the current opcode can continue the fusion.
            // Global pending-fusion entry gate. Unknown or unsupported continuations are flushed here before
            // the current opcode is translated, so delay-local and adjacent conbine patterns never cross an
            // opcode boundary that could change observable order or hide a higher-priority consumer.
            if(conbine_pending.kind != conbine_pending_kind::none && !conbine_can_continue(curr_opbase)) [[unlikely]] { flush_conbine_pending(); }
        }
#endif

        ::std::size_t bytecode_before{};
        ::std::size_t thunks_before{};
        ::std::uint_least64_t opfunc_main_before{};
        ::std::uint_least64_t opfunc_thunk_before{};

        if(runtime_log_on && runtime_log_emit_wasm_ops) [[unlikely]]
        {
            bytecode_before = bytecode.size();
            thunks_before = thunks.size();
            opfunc_main_before = runtime_log_stats.opfunc_main_count;
            opfunc_thunk_before = runtime_log_stats.opfunc_thunk_count;
            runtime_log_wasm_op_state(u8"wasm.op.before", curr_opbase, op_begin, bytecode_before, thunks_before, opfunc_main_before, opfunc_thunk_before);
        }

        // Re-emit Wasm instead of copying emitted u2 bytecode: opfunc pointers are
        // specialized by the current register-ring positions. See loop_unwind.md.
        [[maybe_unused]] auto const replay_loop_body{
            [&](::std::byte const* replay_begin, ::std::byte const* replay_end, ::std::size_t replay_count) constexpr UWVM_THROWS -> void
            {
                auto const saved_code_curr{code_curr};
                auto const body_wasm_bytes{static_cast<::std::size_t>(replay_end - replay_begin)};

                for(::std::size_t i{}; i != replay_count; ++i)
                {
                    auto const bytecode_before_replay{bytecode.size()};
                    code_curr = replay_begin;

                    while(code_curr != replay_end)
                    {
                        if(code_curr > replay_end) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }

                        translate_one_opcode_self(translate_one_opcode_self);

                        if(finished_current_func || code_curr > replay_end) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }
                    }

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
                    if(br_if_fuse.kind != br_if_fuse_kind::none) [[unlikely]]
                    {
                        br_if_fuse.kind = br_if_fuse_kind::none;
                        br_if_fuse.site = SIZE_MAX;
                        br_if_fuse.end = SIZE_MAX;
                        br_if_fuse.stacktop_currpos_at_site = {};
                    }
                    if(conbine_pending.kind != conbine_pending_kind::none) [[unlikely]] { flush_conbine_pending(); }
#endif

                    if(runtime_log_on) [[unlikely]]
                    {
                        ++runtime_log_stats.loop_unwind_replayed_body_count;
                        runtime_log_stats.loop_unwind_replayed_wasm_bytes += body_wasm_bytes;
                        runtime_log_stats.loop_unwind_replayed_bytecode_bytes += bytecode.size() - bytecode_before_replay;
                    }
                }

                code_curr = saved_code_curr;
            }};

        // The opcode fragments are included inside the switch so they can share the helper lambdas above
        // while still keeping each opcode family in a separate file.
        /// @warning Extension point: new opcode families must be included here and mirrored in the lazy validator immediate scanner.
        switch(curr_opbase)
        {
#include "opcode/control_flow_cases.h"
#include "opcode/branch_cases.h"
#include "opcode/call_cases.h"
#include "opcode/variable_cases.h"
#include "opcode/memory_cases.h"
#include "opcode/const_compare_cases.h"
#include "opcode/int_numeric_cases.h"
#include "opcode/float_numeric_convert_cases.h"
            [[unlikely]] default:
            {
                switch(static_cast<wasm_byte>(curr_opbase))
                {
#include "opcode/wasm1p1_cases.h"
                    [[unlikely]] default:
                    {
                        err.err_curr = code_curr;
                        err.err_selectable.u8 = static_cast<::std::uint_least8_t>(curr_opbase);
                        err.err_code = code_validation_error_code::illegal_opbase;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }
                }
            }
        }

        if(runtime_log_on && runtime_log_emit_wasm_ops) [[unlikely]]
        {
            runtime_log_wasm_op_state(u8"wasm.op.after", curr_opbase, op_begin, bytecode_before, thunks_before, opfunc_main_before, opfunc_thunk_before);
        }
    }};

// Main validation/translation loop. Each iteration consumes exactly one Wasm opcode plus its
// immediates and delegates semantic handling to the opcode include fragments above.
for(;;)
{
    translate_one_opcode(translate_one_opcode);
    if(finished_current_func) { break; }
}
}
