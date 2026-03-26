// Fill local-defined call-info records (used by the runtime call bridge fast-path).
if(local_func_count != 0uz)
{
    auto const abi_bytes{[](wasm_value_type t) constexpr noexcept -> ::std::size_t
                         {
                             switch(t)
                             {
                                 case wasm_value_type::i32:
                                 case wasm_value_type::f32: return 4uz;
                                 case wasm_value_type::i64:
                                 case wasm_value_type::f64:
                                     return 8uz;
                                 [[unlikely]] default:
                                     return 0uz;
                             }
                         }};

    for(::std::size_t i{}; i != local_func_count; ++i)
    {
        auto& info{storage.local_defined_call_info.index_unchecked(i)};
        info.runtime_func = ::std::addressof(curr_module.local_defined_function_vec_storage.index_unchecked(i));
        info.compiled_func = ::std::addressof(storage.local_funcs.index_unchecked(i));

        auto const ft{curr_module.local_defined_function_vec_storage.index_unchecked(i).function_type_ptr};
        if(ft == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        ::std::size_t para_bytes{};
        for(auto it{ft->parameter.begin}; it != ft->parameter.end; ++it)
        {
            auto const add{abi_bytes(*it)};
            if(add == 0uz) [[unlikely]] { ::fast_io::fast_terminate(); }
            if(add > (::std::numeric_limits<::std::size_t>::max() - para_bytes)) [[unlikely]] { ::fast_io::fast_terminate(); }
            para_bytes += add;
        }

        ::std::size_t res_bytes{};
        for(auto it{ft->result.begin}; it != ft->result.end; ++it)
        {
            auto const add{abi_bytes(*it)};
            if(add == 0uz) [[unlikely]] { ::fast_io::fast_terminate(); }
            if(add > (::std::numeric_limits<::std::size_t>::max() - res_bytes)) [[unlikely]] { ::fast_io::fast_terminate(); }
            res_bytes += add;
        }

        info.param_bytes = para_bytes;
        info.result_bytes = res_bytes;

        auto const m{details::match_trivial_call_inline_body(curr_module.local_defined_function_vec_storage.index_unchecked(i).wasm_code_ptr)};
        using trivial_kind_t = ::uwvm2::runtime::compiler::uwvm_int::optable::trivial_defined_call_kind;
        if(m.kind != trivial_kind_t::none)
        {
            auto const param_n{static_cast<::std::size_t>(ft->parameter.end - ft->parameter.begin)};
            auto const res_n{static_cast<::std::size_t>(ft->result.end - ft->result.begin)};
            auto const is_i32{[](wasm_value_type t) constexpr noexcept { return t == wasm_value_type::i32; }};

            bool ok{};
            switch(m.kind)
            {
                case trivial_kind_t::nop_void: ok = (res_n == 0uz); break;
                case trivial_kind_t::const_i32: ok = (res_n == 1uz) && is_i32(ft->result.begin[0]); break;
                case trivial_kind_t::param0_i32:
                    ok = (res_n == 1uz) && is_i32(ft->result.begin[0]) && (param_n >= 1uz) && is_i32(ft->parameter.begin[0]);
                    break;
                case trivial_kind_t::add_const_i32:
                    ok = (param_n == 1uz) && (res_n == 1uz) && is_i32(ft->parameter.begin[0]) && is_i32(ft->result.begin[0]);
                    break;
                case trivial_kind_t::xor_const_i32:
                    ok = (param_n == 1uz) && (res_n == 1uz) && is_i32(ft->parameter.begin[0]) && is_i32(ft->result.begin[0]);
                    break;
                case trivial_kind_t::mul_const_i32:
                    ok = (param_n == 1uz) && (res_n == 1uz) && is_i32(ft->parameter.begin[0]) && is_i32(ft->result.begin[0]);
                    break;
                case trivial_kind_t::rotr_add_const_i32:
                    ok = (param_n == 1uz) && (res_n == 1uz) && is_i32(ft->parameter.begin[0]) && is_i32(ft->result.begin[0]);
                    break;
                case trivial_kind_t::xorshift32_i32:
                    ok = (param_n == 1uz) && (res_n == 1uz) && is_i32(ft->parameter.begin[0]) && is_i32(ft->result.begin[0]);
                    break;
                case trivial_kind_t::mul_add_const_i32:
                    ok = (param_n == 1uz) && (res_n == 1uz) && is_i32(ft->parameter.begin[0]) && is_i32(ft->result.begin[0]);
                    break;
                case trivial_kind_t::xor_i32:
                    ok = (param_n == 2uz) && (res_n == 1uz) && is_i32(ft->parameter.begin[0]) && is_i32(ft->parameter.begin[1]) && is_i32(ft->result.begin[0]);
                    break;
                case trivial_kind_t::xor_add_const_i32:
                    ok = (param_n == 2uz) && (res_n == 1uz) && is_i32(ft->parameter.begin[0]) && is_i32(ft->parameter.begin[1]) && is_i32(ft->result.begin[0]);
                    break;
                case trivial_kind_t::sub_or_const_i32:
                    ok = (param_n == 2uz) && (res_n == 1uz) && is_i32(ft->parameter.begin[0]) && is_i32(ft->parameter.begin[1]) && is_i32(ft->result.begin[0]);
                    break;
                case trivial_kind_t::sum8_xor_const_i32:
                {
                    if(param_n == 8uz && res_n == 1uz && is_i32(ft->result.begin[0]))
                    {
                        ok = true;
                        for(::std::size_t j{}; j != 8uz; ++j)
                        {
                            if(!is_i32(ft->parameter.begin[j]))
                            {
                                ok = false;
                                break;
                            }
                        }
                    }
                    break;
                }
                [[unlikely]] default:
                    break;
            }

            if(ok)
            {
                info.trivial_kind = m.kind;
                info.trivial_imm = m.imm;
                info.trivial_imm2 = m.imm2;
            }
        }
    }
}

return storage;
