// Complete the per-local-function call metadata after all local bodies have been translated.
// This include fragment is shared by the eager and lazy compilation paths; the caller has already
// sized `storage.local_defined_call_info` and initialized module/function identity fields.
// The runtime call bridge consumes these records for direct local calls, optional tiered-JIT entry,
// and the interpreter-side trivial-call fast path.
if(local_func_count != 0uz)
{
    // Translate a WebAssembly scalar value type to the byte footprint used by the UWVM stack ABI.
    // Unsupported or unexpected value types deliberately map to zero so the caller can reject the
    // record instead of publishing inconsistent call-frame sizes.
    auto const abi_bytes{[](wasm_value_type t) constexpr noexcept -> ::std::size_t
                         {
                             switch(t)
                             {
                                 // i32/f32 occupy one 32-bit stack slot in the call bridge ABI.
                                 case wasm_value_type::i32:
                                 case wasm_value_type::f32: return 4uz;
                                 // i64/f64 occupy one 64-bit stack slot and must preserve natural width.
                                 case wasm_value_type::i64:
                                 case wasm_value_type::f64:
                                     return 8uz;
                                 // Keep the metadata writer fail-closed when a non-scalar type reaches this path.
                                 [[unlikely]] default:
                                     return 0uz;
                             }
                         }};

    // `i` is the local-defined function index. The corresponding WebAssembly function index
    // was set earlier as `import_func_count + i`.
    for(::std::size_t i{}; i != local_func_count; ++i)
    {
        auto& info{storage.local_defined_call_info.index_unchecked(i)};

        // Preserve both runtime and compiled views of the same local-defined function.
        // The runtime pointer stays opaque to optable code, while `compiled_func` gives the bridge
        // direct access to local/operand-stack frame limits and translated interpreter operands.
        info.runtime_func = ::std::addressof(curr_module.local_defined_function_vec_storage.index_unchecked(i));
        info.compiled_func = ::std::addressof(storage.local_funcs.index_unchecked(i));

        // Function type metadata is required to compute the exact caller-stack byte layout.
        // A missing type means module storage is internally inconsistent after validation.
        auto const ft{curr_module.local_defined_function_vec_storage.index_unchecked(i).function_type_ptr};
        if(ft == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        // Sum the parameter byte footprint in declaration order. The call bridge treats the
        // complete parameter list as a contiguous byte range immediately below the current stack top.
        ::std::size_t para_bytes{};
        for(auto it{ft->parameter.begin}; it != ft->parameter.end; ++it)
        {
            auto const add{abi_bytes(*it)};
            // Reject unsupported parameter types before they can corrupt stack-frame arithmetic.
            if(add == 0uz) [[unlikely]] { ::fast_io::fast_terminate(); }
            // Guard the size accumulation explicitly even though validated WebAssembly signatures
            // should be small; this keeps the metadata table robust against malformed storage.
            if(add > (::std::numeric_limits<::std::size_t>::max() - para_bytes)) [[unlikely]] { ::fast_io::fast_terminate(); }
            para_bytes += add;
        }

        // Sum the result byte footprint separately because the bridge rewrites the caller stack
        // from the argument area to the result area after the callee returns.
        ::std::size_t res_bytes{};
        for(auto it{ft->result.begin}; it != ft->result.end; ++it)
        {
            auto const add{abi_bytes(*it)};
            // A zero-sized ABI mapping here means the result type is not supported by this fast path.
            if(add == 0uz) [[unlikely]] { ::fast_io::fast_terminate(); }
            // Keep result-size arithmetic well-defined before publishing the value to runtime code.
            if(add > (::std::numeric_limits<::std::size_t>::max() - res_bytes)) [[unlikely]] { ::fast_io::fast_terminate(); }
            res_bytes += add;
        }

        // Store the exact byte counts used later by `execute_defined_*` and
        // `try_execute_trivial_defined_call` to move stack-top pointers safely.
        info.param_bytes = para_bytes;
        info.result_bytes = res_bytes;

        // Look for tiny canonical function bodies that can be executed directly by the call bridge
        // without entering the translated interpreter body. The matcher only reports bytecode shape;
        // the signature checks below prove that the matched body is ABI-compatible with the function type.
        auto const m{details::match_trivial_call_inline_body(curr_module.local_defined_function_vec_storage.index_unchecked(i).wasm_code_ptr)};
        using trivial_kind_t = ::uwvm2::runtime::compiler::uwvm_int::optable::trivial_defined_call_kind;
        if(m.kind != trivial_kind_t::none)
        {
            // Cache signature arity and a narrow type predicate to keep each fast-path guard explicit.
            auto const param_n{static_cast<::std::size_t>(ft->parameter.end - ft->parameter.begin)};
            auto const res_n{static_cast<::std::size_t>(ft->result.end - ft->result.begin)};
            auto const is_i32{[](wasm_value_type t) constexpr noexcept { return t == wasm_value_type::i32; }};

            // `ok` means both the bytecode pattern and the declared function type describe the same
            // trivial operation. This prevents, for example, an `i32.const` body from being published
            // as a fast-path for a function whose result is not exactly one i32.
            bool ok{};
            switch(m.kind)
            {
                // Empty bodies, or bodies reduced to `drop; end`, are valid only for void results.
                case trivial_kind_t::nop_void: ok = (res_n == 0uz); break;
                // Constant and param-forwarding forms materialize exactly one i32 result.
                case trivial_kind_t::const_i32: ok = (res_n == 1uz) && is_i32(ft->result.begin[0]); break;
                case trivial_kind_t::param0_i32:
                    ok = (res_n == 1uz) && is_i32(ft->result.begin[0]) && (param_n >= 1uz) && is_i32(ft->parameter.begin[0]);
                    break;
                // Unary i32 arithmetic/hash forms read param0 and write one i32 result.
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
                // Binary i32 forms require two i32 parameters and produce one i32 result.
                case trivial_kind_t::xor_i32:
                    ok = (param_n == 2uz) && (res_n == 1uz) && is_i32(ft->parameter.begin[0]) && is_i32(ft->parameter.begin[1]) && is_i32(ft->result.begin[0]);
                    break;
                case trivial_kind_t::xor_add_const_i32:
                    ok = (param_n == 2uz) && (res_n == 1uz) && is_i32(ft->parameter.begin[0]) && is_i32(ft->parameter.begin[1]) && is_i32(ft->result.begin[0]);
                    break;
                case trivial_kind_t::sub_or_const_i32:
                    ok = (param_n == 2uz) && (res_n == 1uz) && is_i32(ft->parameter.begin[0]) && is_i32(ft->parameter.begin[1]) && is_i32(ft->result.begin[0]);
                    break;
                // The wide reduction pattern is intentionally strict: eight i32 parameters, one i32 result.
                case trivial_kind_t::sum8_xor_const_i32:
                {
                    if(param_n == 8uz && res_n == 1uz && is_i32(ft->result.begin[0]))
                    {
                        ok = true;
                        // Verify every reduction input explicitly so mixed-type signatures cannot reuse
                        // an i32-only stack transform.
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
                // A future enum value must not become callable through the trivial fast path until
                // its signature contract is added here.
                [[unlikely]] default:
                    break;
            }

            if(ok)
            {
                // Publish the verified fast-path descriptor. The immediates are interpreted according
                // to `trivial_kind` by the runtime bridge, so they are copied only after the signature
                // contract has been proven above.
                info.trivial_kind = m.kind;
                info.trivial_imm = m.imm;
                info.trivial_imm2 = m.imm2;
            }
        }
    }
}
