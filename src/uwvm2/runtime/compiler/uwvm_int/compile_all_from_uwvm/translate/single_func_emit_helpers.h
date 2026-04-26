bool const runtime_log_on{uwvm2::uwvm::io::enable_runtime_log};
// Verbose emit logging for offline analysis (enabled only when `-Rclog` is used).
constexpr bool runtime_log_emit_opfuncs{true};
constexpr bool runtime_log_emit_cf{true};

struct runtime_log_stats_t
{
    ::std::uint_least64_t wasm_op_count{};
    ::std::uint_least64_t opfunc_main_count{};
    ::std::uint_least64_t opfunc_thunk_count{};
    ::std::uint_least64_t label_placeholder_main_count{};
    ::std::uint_least64_t label_placeholder_thunk_count{};
    ::std::uint_least64_t cf_br_count{};
    ::std::uint_least64_t cf_br_transform_count{};
    ::std::uint_least64_t cf_br_if_count{};
    ::std::uint_least64_t cf_loop_entry_transform_count{};
    ::std::uint_least64_t cf_loop_entry_canonicalize_to_mem_count{};
    ::std::uint_least64_t stacktop_spill1_count{};
    ::std::uint_least64_t stacktop_spillN_count{};
    ::std::uint_least64_t stacktop_fill1_count{};
    ::std::uint_least64_t stacktop_fillN_count{};
};

runtime_log_stats_t runtime_log_stats{};

// Best-effort: current Wasm IP for emit logs.
::std::size_t runtime_log_curr_ip{};

auto const runtime_log_bc_name{[](bool in_thunk) constexpr noexcept -> ::uwvm2::utils::container::u8string_view
                               {
                                   if(in_thunk) { return ::uwvm2::utils::container::u8string_view{u8"thunk"}; }
                                   return ::uwvm2::utils::container::u8string_view{u8"main"};
                               }};

auto const runtime_log_op_name{[](::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code op) constexpr noexcept -> ::uwvm2::utils::container::u8string_view
                               {
                                   switch(op)
                                   {
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::unreachable: return u8"unreachable";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::nop: return u8"nop";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::block: return u8"block";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::loop: return u8"loop";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::if_: return u8"if";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::else_: return u8"else";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::end: return u8"end";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::br: return u8"br";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::br_if: return u8"br_if";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::br_table: return u8"br_table";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::return_: return u8"return";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::call: return u8"call";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::call_indirect: return u8"call_indirect";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::drop: return u8"drop";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::select: return u8"select";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::local_get: return u8"local_get";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::local_set: return u8"local_set";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::local_tee: return u8"local_tee";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::global_get: return u8"global_get";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::global_set: return u8"global_set";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_load: return u8"i32_load";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_load: return u8"i64_load";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f32_load: return u8"f32_load";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f64_load: return u8"f64_load";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_load8_s: return u8"i32_load8_s";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_load8_u: return u8"i32_load8_u";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_load16_s: return u8"i32_load16_s";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_load16_u: return u8"i32_load16_u";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_load8_s: return u8"i64_load8_s";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_load8_u: return u8"i64_load8_u";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_load16_s: return u8"i64_load16_s";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_load16_u: return u8"i64_load16_u";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_load32_s: return u8"i64_load32_s";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_load32_u: return u8"i64_load32_u";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_store: return u8"i32_store";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_store: return u8"i64_store";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f32_store: return u8"f32_store";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f64_store: return u8"f64_store";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_store8: return u8"i32_store8";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_store16: return u8"i32_store16";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_store8: return u8"i64_store8";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_store16: return u8"i64_store16";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_store32: return u8"i64_store32";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::memory_size: return u8"memory_size";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::memory_grow: return u8"memory_grow";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_const: return u8"i32_const";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_const: return u8"i64_const";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f32_const: return u8"f32_const";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f64_const: return u8"f64_const";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_eqz: return u8"i32_eqz";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_eq: return u8"i32_eq";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_ne: return u8"i32_ne";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_lt_s: return u8"i32_lt_s";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_lt_u: return u8"i32_lt_u";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_gt_s: return u8"i32_gt_s";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_gt_u: return u8"i32_gt_u";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_le_s: return u8"i32_le_s";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_le_u: return u8"i32_le_u";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_ge_s: return u8"i32_ge_s";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_ge_u: return u8"i32_ge_u";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_eqz: return u8"i64_eqz";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_eq: return u8"i64_eq";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_ne: return u8"i64_ne";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_lt_s: return u8"i64_lt_s";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_lt_u: return u8"i64_lt_u";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_gt_s: return u8"i64_gt_s";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_gt_u: return u8"i64_gt_u";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_le_s: return u8"i64_le_s";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_le_u: return u8"i64_le_u";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_ge_s: return u8"i64_ge_s";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_ge_u: return u8"i64_ge_u";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f32_eq: return u8"f32_eq";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f32_ne: return u8"f32_ne";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f32_lt: return u8"f32_lt";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f32_gt: return u8"f32_gt";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f32_le: return u8"f32_le";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f32_ge: return u8"f32_ge";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f64_eq: return u8"f64_eq";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f64_ne: return u8"f64_ne";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f64_lt: return u8"f64_lt";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f64_gt: return u8"f64_gt";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f64_le: return u8"f64_le";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f64_ge: return u8"f64_ge";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_clz: return u8"i32_clz";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_ctz: return u8"i32_ctz";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_popcnt: return u8"i32_popcnt";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_add: return u8"i32_add";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_sub: return u8"i32_sub";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_mul: return u8"i32_mul";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_div_s: return u8"i32_div_s";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_div_u: return u8"i32_div_u";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_rem_s: return u8"i32_rem_s";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_rem_u: return u8"i32_rem_u";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_and: return u8"i32_and";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_or: return u8"i32_or";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_xor: return u8"i32_xor";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_shl: return u8"i32_shl";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_shr_s: return u8"i32_shr_s";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_shr_u: return u8"i32_shr_u";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_rotl: return u8"i32_rotl";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_rotr: return u8"i32_rotr";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_clz: return u8"i64_clz";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_ctz: return u8"i64_ctz";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_popcnt: return u8"i64_popcnt";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_add: return u8"i64_add";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_sub: return u8"i64_sub";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_mul: return u8"i64_mul";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_div_s: return u8"i64_div_s";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_div_u: return u8"i64_div_u";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_rem_s: return u8"i64_rem_s";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_rem_u: return u8"i64_rem_u";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_and: return u8"i64_and";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_or: return u8"i64_or";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_xor: return u8"i64_xor";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_shl: return u8"i64_shl";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_shr_s: return u8"i64_shr_s";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_shr_u: return u8"i64_shr_u";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_rotl: return u8"i64_rotl";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_rotr: return u8"i64_rotr";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f32_abs: return u8"f32_abs";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f32_neg: return u8"f32_neg";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f32_ceil: return u8"f32_ceil";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f32_floor: return u8"f32_floor";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f32_trunc: return u8"f32_trunc";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f32_nearest: return u8"f32_nearest";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f32_sqrt: return u8"f32_sqrt";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f32_add: return u8"f32_add";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f32_sub: return u8"f32_sub";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f32_mul: return u8"f32_mul";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f32_div: return u8"f32_div";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f32_min: return u8"f32_min";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f32_max: return u8"f32_max";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f32_copysign: return u8"f32_copysign";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f64_abs: return u8"f64_abs";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f64_neg: return u8"f64_neg";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f64_ceil: return u8"f64_ceil";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f64_floor: return u8"f64_floor";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f64_trunc: return u8"f64_trunc";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f64_nearest: return u8"f64_nearest";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f64_sqrt: return u8"f64_sqrt";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f64_add: return u8"f64_add";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f64_sub: return u8"f64_sub";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f64_mul: return u8"f64_mul";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f64_div: return u8"f64_div";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f64_min: return u8"f64_min";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f64_max: return u8"f64_max";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f64_copysign: return u8"f64_copysign";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_wrap_i64: return u8"i32_wrap_i64";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_trunc_f32_s: return u8"i32_trunc_f32_s";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_trunc_f32_u: return u8"i32_trunc_f32_u";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_trunc_f64_s: return u8"i32_trunc_f64_s";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_trunc_f64_u: return u8"i32_trunc_f64_u";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_extend_i32_s: return u8"i64_extend_i32_s";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_extend_i32_u: return u8"i64_extend_i32_u";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_trunc_f32_s: return u8"i64_trunc_f32_s";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_trunc_f32_u: return u8"i64_trunc_f32_u";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_trunc_f64_s: return u8"i64_trunc_f64_s";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_trunc_f64_u: return u8"i64_trunc_f64_u";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f32_convert_i32_s: return u8"f32_convert_i32_s";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f32_convert_i32_u: return u8"f32_convert_i32_u";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f32_convert_i64_s: return u8"f32_convert_i64_s";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f32_convert_i64_u: return u8"f32_convert_i64_u";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f32_demote_f64: return u8"f32_demote_f64";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f64_convert_i32_s: return u8"f64_convert_i32_s";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f64_convert_i32_u: return u8"f64_convert_i32_u";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f64_convert_i64_s: return u8"f64_convert_i64_s";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f64_convert_i64_u: return u8"f64_convert_i64_u";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f64_promote_f32: return u8"f64_promote_f32";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i32_reinterpret_f32: return u8"i32_reinterpret_f32";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::i64_reinterpret_f64: return u8"i64_reinterpret_f64";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f32_reinterpret_i32: return u8"f32_reinterpret_i32";
                                       case ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::f64_reinterpret_i64:
                                           return u8"f64_reinterpret_i64";
                                       [[unlikely]] default:
                                           return u8"<unknown>";
                                   }
                               }};

auto const runtime_log_vt_name{[]([[maybe_unused]] curr_operand_stack_value_type vt) constexpr noexcept -> ::uwvm2::utils::container::u8string_view
                               {
                                   switch(vt)
                                   {
                                       case curr_operand_stack_value_type::i32: return u8"i32";
                                       case curr_operand_stack_value_type::i64: return u8"i64";
                                       case curr_operand_stack_value_type::f32: return u8"f32";
                                       case curr_operand_stack_value_type::f64:
                                           return u8"f64";
                                       [[unlikely]] default:
                                           return u8"?";
                                   }
                               }};

auto const bytecode_reserve_suggest{[&]() constexpr noexcept
                                    {
                                        // Heuristic: most ops expand from 1 byte opcode to (ptr + immediates). Use a conservative multiplier.
                                        auto const code_size{static_cast<::std::size_t>(code_end - code_begin)};
                                        // Reduce upfront allocations for modules with many small functions.
                                        // The emitter grows geometrically if this estimate is too small.
                                        constexpr ::std::size_t mul{8uz};
                                        if(code_size > (::std::numeric_limits<::std::size_t>::max() / mul))
                                        {
                                            // Overflow-safe fallback: skip the multiplier rather than attempting an impossible reserve().
                                            return code_size;
                                        }
                                        return code_size * mul;
                                    }()};
bytecode.reserve(bytecode_reserve_suggest);

auto const ensure_vec_capacity{[&](bytecode_vec_t& dst, ::std::size_t add_bytes) constexpr UWVM_THROWS
                               {
                                   auto const curr{dst.size()};
                                   if(add_bytes > (::std::numeric_limits<::std::size_t>::max() - curr)) [[unlikely]]
                                   {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                       ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                                       ::fast_io::fast_terminate();
                                   }
                                   auto const need{curr + add_bytes};
                                   if(need <= dst.capacity()) { return; }

                                   // Grow geometrically to preserve amortized O(1) push_back.
                                   ::std::size_t new_cap{dst.capacity()};
                                   if(new_cap == 0uz) { new_cap = 1uz; }
                                   while(new_cap < need)
                                   {
                                       if(new_cap > (::std::numeric_limits<::std::size_t>::max() / 2uz))
                                       {
                                           new_cap = need;
                                           break;
                                       }
                                       new_cap *= 2uz;
                                   }
                                   dst.reserve(new_cap);
                               }};

auto const emit_bytes_to{[&](bytecode_vec_t& dst, ::std::byte const* src, ::std::size_t n) constexpr UWVM_THROWS
                         {
                             if(n == 0uz) { return; }
                             ensure_vec_capacity(dst, n);
                             auto out{dst.imp.curr_ptr};
                             dst.imp.curr_ptr += n;
                             ::std::memcpy(out, src, n);
                         }};

auto const emit_imm_to{[&]<typename T>(bytecode_vec_t& dst, T const& v) constexpr UWVM_THROWS
                       {
                           static_assert(::std::is_trivially_copyable_v<T>);
                           ensure_vec_capacity(dst, sizeof(T));
                           auto out{dst.imp.curr_ptr};
                           dst.imp.curr_ptr += sizeof(T);
                           ::std::memcpy(out, ::std::addressof(v), sizeof(T));
                       }};

auto const emit_imm{[&]<typename T>(T const& v) constexpr UWVM_THROWS { emit_imm_to(bytecode, v); }};

labels.clear();
ptr_fixups.clear();

auto const new_label{[&](bool in_thunk) constexpr UWVM_THROWS -> ::std::size_t
                     {
                         if(labels.size() == labels.capacity()) { labels.reserve(labels.capacity() ? labels.capacity() * 2uz : 64uz); }
                         // Safe: ensured capacity.
                         labels.push_back_unchecked(label_info_t{.offset = SIZE_MAX, .in_thunk = in_thunk});
                         return labels.size() - 1uz;
                     }};

auto const set_label_offset{[&](::std::size_t label_id, ::std::size_t off) constexpr noexcept { labels.index_unchecked(label_id).offset = off; }};

// Thunk bytecode (appended after main `bytecode` so it never shifts main offsets).
thunks.clear();

auto const emit_ptr_label_placeholder{[&](::std::size_t label_id, bool in_thunk) constexpr UWVM_THROWS
                                      {
                                          rel_offset_t const placeholder{};
                                          ::std::size_t const site{in_thunk ? thunks.size() : bytecode.size()};
                                          if(runtime_log_on) [[unlikely]]
                                          {
                                              if(in_thunk) { ++runtime_log_stats.label_placeholder_thunk_count; }
                                              else
                                              {
                                                  ++runtime_log_stats.label_placeholder_main_count;
                                              }
                                              if(runtime_log_emit_cf)
                                              {
                                                  ::fast_io::io::print(::uwvm2::uwvm::io::u8runtime_log_output,
                                                                       u8"[uwvm-int-translator] fn=",
                                                                       function_index,
                                                                       u8" ip=",
                                                                       runtime_log_curr_ip,
                                                                       u8" event=bytecode.emit.imm | kind=label_placeholder bc=",
                                                                       runtime_log_bc_name(in_thunk),
                                                                       u8" off=",
                                                                       site,
                                                                       u8" label_id=",
                                                                       label_id,
                                                                       u8"\n");
                                              }
                                          }
                                          if(in_thunk) { emit_imm_to(thunks, placeholder); }
                                          else
                                          {
                                              emit_imm(placeholder);
                                          }

                                          if(ptr_fixups.size() == ptr_fixups.capacity())
                                          {
                                              ptr_fixups.reserve(ptr_fixups.capacity() ? ptr_fixups.capacity() * 2uz : 256uz);
                                          }
                                          // Safe: ensured capacity.
                                          ptr_fixups.push_back_unchecked(ptr_fixup_t{.site = site, .label_id = label_id, .in_thunk = in_thunk});
                                      }};

auto const get_branch_target_label_id{[&](block_t const& frame) constexpr noexcept -> ::std::size_t
                                      {
                                          // For Wasm structured control:
                                          // - block/if/else/function: label target is the end.
                                          // - loop: label target is the start.
                                          return frame.type == block_type::loop ? frame.start_label_id : frame.end_label_id;
                                      }};

auto const emit_opfunc_to{[&](bytecode_vec_t& dst, auto fptr) constexpr UWVM_THROWS
                          {
                              static_assert(::std::same_as<decltype(fptr), details::interpreter_expected_opfunc_ptr_t<CompileOption>>,
                                            "emitting opfunc pointer type does not match interpreter mode");
                              if(runtime_log_on) [[unlikely]]
                              {
                                  bool const dst_is_thunk{::std::addressof(dst) == ::std::addressof(thunks)};
                                  ::std::size_t const off{dst.size()};
                                  if(dst_is_thunk) { ++runtime_log_stats.opfunc_thunk_count; }
                                  else
                                  {
                                      ++runtime_log_stats.opfunc_main_count;
                                  }
                                  if(runtime_log_emit_opfuncs)
                                  {
                                      // Print the raw opfunc pointer bits stored into the bytecode stream.
                                      ::std::uintptr_t bits{};
                                      constexpr ::std::size_t copy_n{sizeof(bits) < sizeof(fptr) ? sizeof(bits) : sizeof(fptr)};
                                      ::std::memcpy(::std::addressof(bits), ::std::addressof(fptr), copy_n);
                                      ::fast_io::io::print(::uwvm2::uwvm::io::u8runtime_log_output,
                                                           u8"[uwvm-int-translator] fn=",
                                                           function_index,
                                                           u8" ip=",
                                                           runtime_log_curr_ip,
                                                           u8" event=bytecode.emit.opfunc | bc=",
                                                           runtime_log_bc_name(dst_is_thunk),
                                                           u8" off=",
                                                           off,
                                                           u8" sz=",
                                                           sizeof(fptr),
                                                           u8" fptr_bits=",
                                                           ::fast_io::mnp::hex0x(bits),
                                                           u8"\n");
                                  }
                              }

                              // Best-effort: prefetch the opfunc's instruction stream into cache.
                              // This attempts to reduce cold I$ misses when the threaded interpreter dispatches to the opfunc.
                              if UWVM_IF_NOT_CONSTEVAL
                              {
                                  using fptr_t = decltype(fptr);
                                  if constexpr(::std::is_pointer_v<fptr_t>)
                                  {
                                      auto const addr{reinterpret_cast<void const*>(fptr)};
                                      ::uwvm2::utils::intrinsics::universal::prefetch<::uwvm2::utils::intrinsics::universal::pfc_mode::instruction,
                                                                                      ::uwvm2::utils::intrinsics::universal::pfc_level::L2>(addr);
                                  }
                              }

                              // Note: We intentionally store the raw function pointer bytes into the bytecode stream.
                              emit_imm_to(dst, fptr);
                          }};

// ============================
// Stack-top cache manipulation
// ============================

[[maybe_unused]] auto const codegen_stack_push{[&](curr_operand_stack_value_type vt) constexpr UWVM_THROWS
                                               {
                                                   if(codegen_operand_stack.size() == codegen_operand_stack.capacity())
                                                   {
                                                       codegen_operand_stack.reserve(codegen_operand_stack.capacity() ? codegen_operand_stack.capacity() * 2uz
                                                                                                                      : 64uz);
                                                   }
                                                   // Safe: ensured capacity.
                                                   codegen_operand_stack.push_back_unchecked({.type = vt});
                                               }};

[[maybe_unused]] auto const codegen_stack_pop_n{[&](::std::size_t n) constexpr noexcept
                                                {
                                                    while(n-- != 0uz)
                                                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                                        if(codegen_operand_stack.empty()) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif
                                                        if(codegen_operand_stack.empty()) { return; }
                                                        codegen_operand_stack.pop_back_unchecked();
                                                    }
                                                }};

[[maybe_unused]] auto const codegen_stack_set_top{
    [&](curr_operand_stack_value_type vt) constexpr noexcept
    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(codegen_operand_stack.empty()) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif
        if(codegen_operand_stack.empty()) { return; }
        if constexpr(stacktop_enabled)
        {
            if(!is_polymorphic && stacktop_cache_count != 0uz)
            {
                auto const old_vt{codegen_operand_stack.back_unchecked().type};
                if(old_vt != vt)
                {
                    auto const cache_count_ref_for_vt{[&]([[maybe_unused]] curr_operand_stack_value_type ty) constexpr noexcept -> ::std::size_t&
                                                      {
                                                          switch(ty)
                                                          {
                                                              case curr_operand_stack_value_type::i32:
                                                              {
                                                                  return stacktop_cache_i32_count;
                                                              }
                                                              case curr_operand_stack_value_type::i64:
                                                              {
                                                                  return stacktop_cache_i64_count;
                                                              }
                                                              case curr_operand_stack_value_type::f32:
                                                              {
                                                                  return stacktop_cache_f32_count;
                                                              }
                                                              case curr_operand_stack_value_type::f64:
                                                              {
                                                                  return stacktop_cache_f64_count;
                                                              }
                                                              [[unlikely]] default:
                                                              {
                                                                  return stacktop_cache_i32_count;
                                                              }
                                                          }
                                                      }};

                    // The top value is always inside the cached segment when `stacktop_cache_count != 0`.
                    --cache_count_ref_for_vt(old_vt);
                    ++cache_count_ref_for_vt(vt);
                }
            }
        }
        codegen_operand_stack.back_unchecked().type = vt;
    }};

constexpr auto stacktop_range_begin_pos{[](curr_operand_stack_value_type vt) constexpr noexcept -> ::std::size_t
                                        {
                                            switch(vt)
                                            {
                                                case curr_operand_stack_value_type::i32:
                                                {
                                                    return CompileOption.i32_stack_top_begin_pos;
                                                }
                                                case curr_operand_stack_value_type::i64:
                                                {
                                                    return CompileOption.i64_stack_top_begin_pos;
                                                }
                                                case curr_operand_stack_value_type::f32:
                                                {
                                                    return CompileOption.f32_stack_top_begin_pos;
                                                }
                                                case curr_operand_stack_value_type::f64:
                                                {
                                                    return CompileOption.f64_stack_top_begin_pos;
                                                }
                                                [[unlikely]] default:
                                                {
                                                    return SIZE_MAX;
                                                }
                                            }
                                        }};

constexpr auto stacktop_range_end_pos{[](curr_operand_stack_value_type vt) constexpr noexcept -> ::std::size_t
                                      {
                                          switch(vt)
                                          {
                                              case curr_operand_stack_value_type::i32:
                                              {
                                                  return CompileOption.i32_stack_top_end_pos;
                                              }
                                              case curr_operand_stack_value_type::i64:
                                              {
                                                  return CompileOption.i64_stack_top_end_pos;
                                              }
                                              case curr_operand_stack_value_type::f32:
                                              {
                                                  return CompileOption.f32_stack_top_end_pos;
                                              }
                                              case curr_operand_stack_value_type::f64:
                                              {
                                                  return CompileOption.f64_stack_top_end_pos;
                                              }
                                              [[unlikely]] default:
                                              {
                                                  return SIZE_MAX;
                                              }
                                          }
                                      }};

[[maybe_unused]] constexpr auto stacktop_enabled_for_vt{[](curr_operand_stack_value_type vt) constexpr noexcept -> bool
                                                        {
                                                            switch(vt)
                                                            {
                                                                case curr_operand_stack_value_type::i32:
                                                                {
                                                                    return CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos;
                                                                }
                                                                case curr_operand_stack_value_type::i64:
                                                                {
                                                                    return CompileOption.i64_stack_top_begin_pos != CompileOption.i64_stack_top_end_pos;
                                                                }
                                                                case curr_operand_stack_value_type::f32:
                                                                {
                                                                    return CompileOption.f32_stack_top_begin_pos != CompileOption.f32_stack_top_end_pos;
                                                                }
                                                                case curr_operand_stack_value_type::f64:
                                                                {
                                                                    return CompileOption.f64_stack_top_begin_pos != CompileOption.f64_stack_top_end_pos;
                                                                }
                                                                [[unlikely]] default:
                                                                {
                                                                    return false;
                                                                }
                                                            }
                                                        }};

[[maybe_unused]] constexpr auto stacktop_ranges_merged_for{[](curr_operand_stack_value_type l, curr_operand_stack_value_type r) constexpr noexcept -> bool
                                                           {
                                                               auto const begin_pos{[](curr_operand_stack_value_type vt) constexpr noexcept -> ::std::size_t
                                                                                    {
                                                                                        switch(vt)
                                                                                        {
                                                                                            case curr_operand_stack_value_type::i32:
                                                                                                return CompileOption.i32_stack_top_begin_pos;
                                                                                            case curr_operand_stack_value_type::i64:
                                                                                                return CompileOption.i64_stack_top_begin_pos;
                                                                                            case curr_operand_stack_value_type::f32:
                                                                                                return CompileOption.f32_stack_top_begin_pos;
                                                                                            case curr_operand_stack_value_type::f64:
                                                                                                return CompileOption.f64_stack_top_begin_pos;
                                                                                            [[unlikely]] default:
                                                                                                return SIZE_MAX;
                                                                                        }
                                                                                    }};

                                                               auto const end_pos{[](curr_operand_stack_value_type vt) constexpr noexcept -> ::std::size_t
                                                                                  {
                                                                                      switch(vt)
                                                                                      {
                                                                                          case curr_operand_stack_value_type::i32:
                                                                                              return CompileOption.i32_stack_top_end_pos;
                                                                                          case curr_operand_stack_value_type::i64:
                                                                                              return CompileOption.i64_stack_top_end_pos;
                                                                                          case curr_operand_stack_value_type::f32:
                                                                                              return CompileOption.f32_stack_top_end_pos;
                                                                                          case curr_operand_stack_value_type::f64:
                                                                                              return CompileOption.f64_stack_top_end_pos;
                                                                                          [[unlikely]] default:
                                                                                              return SIZE_MAX;
                                                                                      }
                                                                                  }};

                                                               return begin_pos(l) == begin_pos(r) && end_pos(l) == end_pos(r);
                                                           }};

auto const stacktop_cache_count_ref_for_vt{[&](curr_operand_stack_value_type vt) constexpr noexcept -> ::std::size_t&
                                           {
                                               switch(vt)
                                               {
                                                   case curr_operand_stack_value_type::i32:
                                                   {
                                                       return stacktop_cache_i32_count;
                                                   }
                                                   case curr_operand_stack_value_type::i64:
                                                   {
                                                       return stacktop_cache_i64_count;
                                                   }
                                                   case curr_operand_stack_value_type::f32:
                                                   {
                                                       return stacktop_cache_f32_count;
                                                   }
                                                   case curr_operand_stack_value_type::f64:
                                                   {
                                                       return stacktop_cache_f64_count;
                                                   }
                                                   [[unlikely]] default:
                                                   {
                                                       return stacktop_cache_i32_count;
                                                   }
                                               }
                                           }};

auto const stacktop_cache_count_for_range{
    [&](::std::size_t begin_pos, ::std::size_t end_pos) constexpr noexcept -> ::std::size_t
    {
        ::std::size_t sum{};
        if(stacktop_i32_enabled && begin_pos == CompileOption.i32_stack_top_begin_pos && end_pos == CompileOption.i32_stack_top_end_pos)
        {
            sum += stacktop_cache_i32_count;
        }
        if(stacktop_i64_enabled && begin_pos == CompileOption.i64_stack_top_begin_pos && end_pos == CompileOption.i64_stack_top_end_pos)
        {
            sum += stacktop_cache_i64_count;
        }
        if(stacktop_f32_enabled && begin_pos == CompileOption.f32_stack_top_begin_pos && end_pos == CompileOption.f32_stack_top_end_pos)
        {
            sum += stacktop_cache_f32_count;
        }
        if(stacktop_f64_enabled && begin_pos == CompileOption.f64_stack_top_begin_pos && end_pos == CompileOption.f64_stack_top_end_pos)
        {
            sum += stacktop_cache_f64_count;
        }
        // Note: wasm1 codegen never produces v128 values, so no v128 cache-count here.
        return sum;
    }};

// Reconcile per-type cache counters from the codegen type stack.
// This protects against missing/incorrect per-type updates on ops that only retype the top value
// (e.g., reinterpret/extend) while keeping stack depth unchanged.
[[maybe_unused]] auto const stacktop_rebuild_cache_type_counts_from_codegen{[&]() constexpr noexcept
                                                                            {
                                                                                if constexpr(!stacktop_enabled) { return; }
                                                                                if(is_polymorphic) { return; }

                                                                                stacktop_cache_i32_count = 0uz;
                                                                                stacktop_cache_i64_count = 0uz;
                                                                                stacktop_cache_f32_count = 0uz;
                                                                                stacktop_cache_f64_count = 0uz;

                                                                                auto const total{codegen_operand_stack.size()};
                                                                                auto cache_n{stacktop_cache_count};
                                                                                if(cache_n > total) { cache_n = total; }
                                                                                auto const start{total - cache_n};
                                                                                for(::std::size_t i{}; i != cache_n; ++i)
                                                                                {
                                                                                    auto const vt{codegen_operand_stack.index_unchecked(start + i).type};
                                                                                    ++stacktop_cache_count_ref_for_vt(vt);
                                                                                }
                                                                            }};

auto const stacktop_currpos_for_range{
    [&](::std::size_t begin_pos, ::std::size_t end_pos) constexpr noexcept -> ::std::size_t
    {
        if(stacktop_i32_enabled && begin_pos == CompileOption.i32_stack_top_begin_pos && end_pos == CompileOption.i32_stack_top_end_pos)
        {
            return curr_stacktop.i32_stack_top_curr_pos;
        }
        if(stacktop_i64_enabled && begin_pos == CompileOption.i64_stack_top_begin_pos && end_pos == CompileOption.i64_stack_top_end_pos)
        {
            return curr_stacktop.i64_stack_top_curr_pos;
        }
        if(stacktop_f32_enabled && begin_pos == CompileOption.f32_stack_top_begin_pos && end_pos == CompileOption.f32_stack_top_end_pos)
        {
            return curr_stacktop.f32_stack_top_curr_pos;
        }
        if(stacktop_f64_enabled && begin_pos == CompileOption.f64_stack_top_begin_pos && end_pos == CompileOption.f64_stack_top_end_pos)
        {
            return curr_stacktop.f64_stack_top_curr_pos;
        }
        if(stacktop_v128_enabled && begin_pos == CompileOption.v128_stack_top_begin_pos && end_pos == CompileOption.v128_stack_top_end_pos)
        {
            return curr_stacktop.v128_stack_top_curr_pos;
        }
        return SIZE_MAX;
    }};

auto const stacktop_set_currpos_for_range{
    [&](::std::size_t begin_pos, ::std::size_t end_pos, ::std::size_t pos) constexpr noexcept
    {
        if(stacktop_i32_enabled && begin_pos == CompileOption.i32_stack_top_begin_pos && end_pos == CompileOption.i32_stack_top_end_pos)
        {
            curr_stacktop.i32_stack_top_curr_pos = pos;
        }
        if(stacktop_i64_enabled && begin_pos == CompileOption.i64_stack_top_begin_pos && end_pos == CompileOption.i64_stack_top_end_pos)
        {
            curr_stacktop.i64_stack_top_curr_pos = pos;
        }
        if(stacktop_f32_enabled && begin_pos == CompileOption.f32_stack_top_begin_pos && end_pos == CompileOption.f32_stack_top_end_pos)
        {
            curr_stacktop.f32_stack_top_curr_pos = pos;
        }
        if(stacktop_f64_enabled && begin_pos == CompileOption.f64_stack_top_begin_pos && end_pos == CompileOption.f64_stack_top_end_pos)
        {
            curr_stacktop.f64_stack_top_curr_pos = pos;
        }
        if(stacktop_v128_enabled && begin_pos == CompileOption.v128_stack_top_begin_pos && end_pos == CompileOption.v128_stack_top_end_pos)
        {
            curr_stacktop.v128_stack_top_curr_pos = pos;
        }
    }};

auto const stacktop_ring_prev{[&](::std::size_t pos, ::std::size_t begin_pos, ::std::size_t end_pos) constexpr noexcept -> ::std::size_t
                              { return pos == begin_pos ? (end_pos - 1uz) : (pos - 1uz); }};

auto const stacktop_ring_next{[&](::std::size_t pos, ::std::size_t begin_pos, ::std::size_t end_pos) constexpr noexcept -> ::std::size_t
                              { return (pos + 1uz == end_pos) ? begin_pos : (pos + 1uz); }};

auto const stacktop_ring_advance_next{
    [&](::std::size_t pos, ::std::size_t n, ::std::size_t begin_pos, ::std::size_t end_pos) constexpr noexcept -> ::std::size_t
    {
        for(::std::size_t i{}; i != n; ++i) { pos = stacktop_ring_next(pos, begin_pos, end_pos); }
        return pos;
    }};

#if 0
            [[maybe_unused]] auto const stacktop_ring_advance_prev{
                [&](::std::size_t pos, ::std::size_t n, ::std::size_t begin_pos, ::std::size_t end_pos) constexpr noexcept -> ::std::size_t
                {
                    for(::std::size_t i{}; i != n; ++i) { pos = stacktop_ring_prev(pos, begin_pos, end_pos); }
                    return pos;
                }};
#endif

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
auto const stacktop_runtime_depth{[&]() constexpr noexcept -> ::std::size_t { return stacktop_memory_count + stacktop_cache_count; }};
#endif

auto const stacktop_assert_invariants{
    [&]() constexpr noexcept
    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if constexpr(stacktop_enabled)
        {
            // Stack-top cache model is tied to the emitted bytecode stream; validate against the
            // codegen type stack, not the validator operand stack (which can be ahead during conbine).
            if(stacktop_runtime_depth() != codegen_operand_stack.size()) [[unlikely]]
            {
                using op_underlying_t = ::std::underlying_type_t<::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code>;
                auto const op_u{static_cast<::std::uint_least32_t>(static_cast<op_underlying_t>(stacktop_dbg_last_op))};

                ::fast_io::io::perr(::fast_io::u8err(),
                                    u8"[uwvm-int-translator] stacktop invariant failure: fn=",
                                    function_index,
                                    u8" ip=",
                                    stacktop_dbg_last_ip,
                                    u8" op=",
                                    runtime_log_op_name(stacktop_dbg_last_op),
                                    u8" op_u=",
                                    ::fast_io::mnp::hex0x(op_u),
                                    u8" stacktop{mem=",
                                    stacktop_memory_count,
                                    u8",cache=",
                                    stacktop_cache_count,
                                    u8"} codegen_sz=",
                                    codegen_operand_stack.size(),
                                    u8" operand_sz=",
                                    operand_stack.size(),
                                    u8" polymorphic=",
                                    is_polymorphic,
                                    u8"\n");

                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
            }

            if(stacktop_cache_i32_count + stacktop_cache_i64_count + stacktop_cache_f32_count + stacktop_cache_f64_count != stacktop_cache_count)
            {
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
            }

            auto const check_currpos_in_range{
                [&]([[maybe_unused]] ::std::size_t pos, [[maybe_unused]] ::std::size_t begin_pos, [[maybe_unused]] ::std::size_t end_pos) constexpr noexcept
                {
                    if(pos < begin_pos || pos >= end_pos) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
                }};

            if(stacktop_i32_enabled)
            {
                check_currpos_in_range(curr_stacktop.i32_stack_top_curr_pos, CompileOption.i32_stack_top_begin_pos, CompileOption.i32_stack_top_end_pos);
            }
            if(stacktop_i64_enabled)
            {
                check_currpos_in_range(curr_stacktop.i64_stack_top_curr_pos, CompileOption.i64_stack_top_begin_pos, CompileOption.i64_stack_top_end_pos);
            }
            if(stacktop_f32_enabled)
            {
                check_currpos_in_range(curr_stacktop.f32_stack_top_curr_pos, CompileOption.f32_stack_top_begin_pos, CompileOption.f32_stack_top_end_pos);
            }
            if(stacktop_f64_enabled)
            {
                check_currpos_in_range(curr_stacktop.f64_stack_top_curr_pos, CompileOption.f64_stack_top_begin_pos, CompileOption.f64_stack_top_end_pos);
            }
            if(stacktop_v128_enabled)
            {
                check_currpos_in_range(curr_stacktop.v128_stack_top_curr_pos, CompileOption.v128_stack_top_begin_pos, CompileOption.v128_stack_top_end_pos);
            }

            // Capacity check per range (use direct sites so the reported line identifies the failing range).
            if(stacktop_i32_enabled)
            {
                auto const begin_pos{CompileOption.i32_stack_top_begin_pos};
                auto const end_pos{CompileOption.i32_stack_top_end_pos};
                auto const cnt{stacktop_cache_count_for_range(begin_pos, end_pos)};
                auto const cap{end_pos - begin_pos};
                if(cnt > cap) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
            }
            if(stacktop_i64_enabled)
            {
                auto const begin_pos{CompileOption.i64_stack_top_begin_pos};
                auto const end_pos{CompileOption.i64_stack_top_end_pos};
                auto const cnt{stacktop_cache_count_for_range(begin_pos, end_pos)};
                auto const cap{end_pos - begin_pos};
                if(cnt > cap) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
            }
            if(stacktop_f32_enabled)
            {
                auto const begin_pos{CompileOption.f32_stack_top_begin_pos};
                auto const end_pos{CompileOption.f32_stack_top_end_pos};
                auto const cnt{stacktop_cache_count_for_range(begin_pos, end_pos)};
                auto const cap{end_pos - begin_pos};
                if(cnt > cap) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
            }
            if(stacktop_f64_enabled)
            {
                auto const begin_pos{CompileOption.f64_stack_top_begin_pos};
                auto const end_pos{CompileOption.f64_stack_top_end_pos};
                auto const cnt{stacktop_cache_count_for_range(begin_pos, end_pos)};
                auto const cap{end_pos - begin_pos};
                if(cnt > cap) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
            }
            if(stacktop_v128_enabled)
            {
                auto const begin_pos{CompileOption.v128_stack_top_begin_pos};
                auto const end_pos{CompileOption.v128_stack_top_end_pos};
                auto const cnt{stacktop_cache_count_for_range(begin_pos, end_pos)};
                auto const cap{end_pos - begin_pos};
                if(cnt > cap) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
            }
        }
#endif
    }};

auto const emit_stacktop_spill1_typed_to{
    [&]([[maybe_unused]] bytecode_vec_t& dst, [[maybe_unused]] ::std::size_t slot, [[maybe_unused]] curr_operand_stack_value_type vt) constexpr UWVM_THROWS
    {
        if constexpr(stacktop_enabled)
        {
            if(runtime_log_on) [[unlikely]]
            {
                ++runtime_log_stats.stacktop_spill1_count;
                ::fast_io::io::print(::uwvm2::uwvm::io::u8runtime_log_output,
                                     u8"[uwvm-int-translator] fn=",
                                     function_index,
                                     u8" event=stacktop.spill1 | vt=",
                                     runtime_log_vt_name(vt),
                                     u8" slot=",
                                     slot,
                                     u8" currpos{i32=",
                                     curr_stacktop.i32_stack_top_curr_pos,
                                     u8",i64=",
                                     curr_stacktop.i64_stack_top_curr_pos,
                                     u8",f32=",
                                     curr_stacktop.f32_stack_top_curr_pos,
                                     u8",f64=",
                                     curr_stacktop.f64_stack_top_curr_pos,
                                     u8",v128=",
                                     curr_stacktop.v128_stack_top_curr_pos,
                                     u8"} stacktop{mem=",
                                     stacktop_memory_count,
                                     u8",cache=",
                                     stacktop_cache_count,
                                     u8"}\n");
            }

            namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
            switch(vt)
            {
                case curr_operand_stack_value_type::i32:
                {
                    emit_opfunc_to(
                        dst,
                        translate::get_uwvmint_stacktop_to_operand_stack_typed_single_fptr_from_tuple<CompileOption,
                                                                                                      ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32>(
                            slot,
                            interpreter_tuple));
                    break;
                }
                case curr_operand_stack_value_type::i64:
                {
                    emit_opfunc_to(
                        dst,
                        translate::get_uwvmint_stacktop_to_operand_stack_typed_single_fptr_from_tuple<CompileOption,
                                                                                                      ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64>(
                            slot,
                            interpreter_tuple));
                    break;
                }
                case curr_operand_stack_value_type::f32:
                {
                    emit_opfunc_to(
                        dst,
                        translate::get_uwvmint_stacktop_to_operand_stack_typed_single_fptr_from_tuple<CompileOption,
                                                                                                      ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32>(
                            slot,
                            interpreter_tuple));
                    break;
                }
                case curr_operand_stack_value_type::f64:
                {
                    emit_opfunc_to(
                        dst,
                        translate::get_uwvmint_stacktop_to_operand_stack_typed_single_fptr_from_tuple<CompileOption,
                                                                                                      ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>(
                            slot,
                            interpreter_tuple));
                    break;
                }
                [[unlikely]] default:
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    break;
                }
            }
        }
    }};

// Compile-time: whether each value-type range is disjoint (not merged) with other enabled ranges.
// If a range is merged, the untyped multi-count spill/fill opfuncs are not usable (they require StartPos to hit exactly one range),
// so we must fall back to single-value typed spill/fill.
[[maybe_unused]] constexpr auto const stacktop_same_range{
    [](::std::size_t a_begin, ::std::size_t a_end, ::std::size_t b_begin, ::std::size_t b_end) constexpr noexcept -> bool
    { return a_begin == b_begin && a_end == b_end; }};

[[maybe_unused]] constexpr bool const i32_range_unique{stacktop_i32_enabled &&
                                                       ((1uz) +
                                                        (stacktop_i64_enabled && stacktop_same_range(CompileOption.i32_stack_top_begin_pos,
                                                                                                     CompileOption.i32_stack_top_end_pos,
                                                                                                     CompileOption.i64_stack_top_begin_pos,
                                                                                                     CompileOption.i64_stack_top_end_pos)
                                                             ? 1uz
                                                             : 0uz) +
                                                        (stacktop_f32_enabled && stacktop_same_range(CompileOption.i32_stack_top_begin_pos,
                                                                                                     CompileOption.i32_stack_top_end_pos,
                                                                                                     CompileOption.f32_stack_top_begin_pos,
                                                                                                     CompileOption.f32_stack_top_end_pos)
                                                             ? 1uz
                                                             : 0uz) +
                                                        (stacktop_f64_enabled && stacktop_same_range(CompileOption.i32_stack_top_begin_pos,
                                                                                                     CompileOption.i32_stack_top_end_pos,
                                                                                                     CompileOption.f64_stack_top_begin_pos,
                                                                                                     CompileOption.f64_stack_top_end_pos)
                                                             ? 1uz
                                                             : 0uz) +
                                                        (stacktop_v128_enabled && stacktop_same_range(CompileOption.i32_stack_top_begin_pos,
                                                                                                      CompileOption.i32_stack_top_end_pos,
                                                                                                      CompileOption.v128_stack_top_begin_pos,
                                                                                                      CompileOption.v128_stack_top_end_pos)
                                                             ? 1uz
                                                             : 0uz)) == 1uz};

[[maybe_unused]] constexpr bool const i64_range_unique{stacktop_i64_enabled &&
                                                       ((1uz) +
                                                        (stacktop_i32_enabled && stacktop_same_range(CompileOption.i64_stack_top_begin_pos,
                                                                                                     CompileOption.i64_stack_top_end_pos,
                                                                                                     CompileOption.i32_stack_top_begin_pos,
                                                                                                     CompileOption.i32_stack_top_end_pos)
                                                             ? 1uz
                                                             : 0uz) +
                                                        (stacktop_f32_enabled && stacktop_same_range(CompileOption.i64_stack_top_begin_pos,
                                                                                                     CompileOption.i64_stack_top_end_pos,
                                                                                                     CompileOption.f32_stack_top_begin_pos,
                                                                                                     CompileOption.f32_stack_top_end_pos)
                                                             ? 1uz
                                                             : 0uz) +
                                                        (stacktop_f64_enabled && stacktop_same_range(CompileOption.i64_stack_top_begin_pos,
                                                                                                     CompileOption.i64_stack_top_end_pos,
                                                                                                     CompileOption.f64_stack_top_begin_pos,
                                                                                                     CompileOption.f64_stack_top_end_pos)
                                                             ? 1uz
                                                             : 0uz) +
                                                        (stacktop_v128_enabled && stacktop_same_range(CompileOption.i64_stack_top_begin_pos,
                                                                                                      CompileOption.i64_stack_top_end_pos,
                                                                                                      CompileOption.v128_stack_top_begin_pos,
                                                                                                      CompileOption.v128_stack_top_end_pos)
                                                             ? 1uz
                                                             : 0uz)) == 1uz};

[[maybe_unused]] constexpr bool const f32_range_unique{stacktop_f32_enabled &&
                                                       ((1uz) +
                                                        (stacktop_i32_enabled && stacktop_same_range(CompileOption.f32_stack_top_begin_pos,
                                                                                                     CompileOption.f32_stack_top_end_pos,
                                                                                                     CompileOption.i32_stack_top_begin_pos,
                                                                                                     CompileOption.i32_stack_top_end_pos)
                                                             ? 1uz
                                                             : 0uz) +
                                                        (stacktop_i64_enabled && stacktop_same_range(CompileOption.f32_stack_top_begin_pos,
                                                                                                     CompileOption.f32_stack_top_end_pos,
                                                                                                     CompileOption.i64_stack_top_begin_pos,
                                                                                                     CompileOption.i64_stack_top_end_pos)
                                                             ? 1uz
                                                             : 0uz) +
                                                        (stacktop_f64_enabled && stacktop_same_range(CompileOption.f32_stack_top_begin_pos,
                                                                                                     CompileOption.f32_stack_top_end_pos,
                                                                                                     CompileOption.f64_stack_top_begin_pos,
                                                                                                     CompileOption.f64_stack_top_end_pos)
                                                             ? 1uz
                                                             : 0uz) +
                                                        (stacktop_v128_enabled && stacktop_same_range(CompileOption.f32_stack_top_begin_pos,
                                                                                                      CompileOption.f32_stack_top_end_pos,
                                                                                                      CompileOption.v128_stack_top_begin_pos,
                                                                                                      CompileOption.v128_stack_top_end_pos)
                                                             ? 1uz
                                                             : 0uz)) == 1uz};

[[maybe_unused]] constexpr bool const f64_range_unique{stacktop_f64_enabled &&
                                                       ((1uz) +
                                                        (stacktop_i32_enabled && stacktop_same_range(CompileOption.f64_stack_top_begin_pos,
                                                                                                     CompileOption.f64_stack_top_end_pos,
                                                                                                     CompileOption.i32_stack_top_begin_pos,
                                                                                                     CompileOption.i32_stack_top_end_pos)
                                                             ? 1uz
                                                             : 0uz) +
                                                        (stacktop_i64_enabled && stacktop_same_range(CompileOption.f64_stack_top_begin_pos,
                                                                                                     CompileOption.f64_stack_top_end_pos,
                                                                                                     CompileOption.i64_stack_top_begin_pos,
                                                                                                     CompileOption.i64_stack_top_end_pos)
                                                             ? 1uz
                                                             : 0uz) +
                                                        (stacktop_f32_enabled && stacktop_same_range(CompileOption.f64_stack_top_begin_pos,
                                                                                                     CompileOption.f64_stack_top_end_pos,
                                                                                                     CompileOption.f32_stack_top_begin_pos,
                                                                                                     CompileOption.f32_stack_top_end_pos)
                                                             ? 1uz
                                                             : 0uz) +
                                                        (stacktop_v128_enabled && stacktop_same_range(CompileOption.f64_stack_top_begin_pos,
                                                                                                      CompileOption.f64_stack_top_end_pos,
                                                                                                      CompileOption.v128_stack_top_begin_pos,
                                                                                                      CompileOption.v128_stack_top_end_pos)
                                                             ? 1uz
                                                             : 0uz)) == 1uz};

// Emit a multi-value spill (cache -> memory) for a **single scalar type**.
// This reduces bytecode size and dispatch overhead by combining consecutive spills into one threaded-interpreter opfunc.
[[maybe_unused]] auto const emit_stacktop_spilln_same_vt_to{
    [&]([[maybe_unused]] bytecode_vec_t& dst,
        [[maybe_unused]] ::std::size_t start_pos,
        [[maybe_unused]] ::std::size_t count,
        [[maybe_unused]] curr_operand_stack_value_type vt) constexpr UWVM_THROWS
    {
        if constexpr(!stacktop_enabled) { return; }
        if(count == 0uz) { return; }
        if(runtime_log_on) [[unlikely]] { ++runtime_log_stats.stacktop_spillN_count; }

        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
        using remain_t = ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_remain_size_t;
        auto tmp_currpos{curr_stacktop};
        remain_t remain{};

        switch(vt)
        {
            case curr_operand_stack_value_type::i32:
            {
                if constexpr(stacktop_i32_enabled)
                {
                    tmp_currpos.i32_stack_top_curr_pos = start_pos;
                    remain.i32_stack_top_remain_size = count;
                    if(runtime_log_on) [[unlikely]]
                    {
                        ::fast_io::io::print(::uwvm2::uwvm::io::u8runtime_log_output,
                                             u8"[uwvm-int-translator] fn=",
                                             function_index,
                                             u8" event=stacktop.spillN | vt=i32 start=",
                                             start_pos,
                                             u8" remain(i32)=",
                                             count,
                                             u8" currpos(i32)=",
                                             curr_stacktop.i32_stack_top_curr_pos,
                                             u8"\n");
                    }
                    emit_opfunc_to(
                        dst,
                        translate::get_uwvmint_stacktop_to_operand_stack_fptr_from_tuple<CompileOption, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32>(
                            tmp_currpos,
                            remain,
                            interpreter_tuple));
                    return;
                }
                break;
            }
            case curr_operand_stack_value_type::i64:
            {
                if constexpr(stacktop_i64_enabled)
                {
                    tmp_currpos.i64_stack_top_curr_pos = start_pos;
                    remain.i64_stack_top_remain_size = count;
                    if(runtime_log_on) [[unlikely]]
                    {
                        ::fast_io::io::print(::uwvm2::uwvm::io::u8runtime_log_output,
                                             u8"[uwvm-int-translator] fn=",
                                             function_index,
                                             u8" event=stacktop.spillN | vt=i64 start=",
                                             start_pos,
                                             u8" remain(i64)=",
                                             count,
                                             u8" currpos(i64)=",
                                             curr_stacktop.i64_stack_top_curr_pos,
                                             u8"\n");
                    }
                    emit_opfunc_to(
                        dst,
                        translate::get_uwvmint_stacktop_to_operand_stack_fptr_from_tuple<CompileOption, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64>(
                            tmp_currpos,
                            remain,
                            interpreter_tuple));
                    return;
                }
                break;
            }
            case curr_operand_stack_value_type::f32:
            {
                if constexpr(stacktop_f32_enabled)
                {
                    tmp_currpos.f32_stack_top_curr_pos = start_pos;
                    remain.f32_stack_top_remain_size = count;
                    if(runtime_log_on) [[unlikely]]
                    {
                        ::fast_io::io::print(::uwvm2::uwvm::io::u8runtime_log_output,
                                             u8"[uwvm-int-translator] fn=",
                                             function_index,
                                             u8" event=stacktop.spillN | vt=f32 start=",
                                             start_pos,
                                             u8" remain(f32)=",
                                             count,
                                             u8" currpos(f32)=",
                                             curr_stacktop.f32_stack_top_curr_pos,
                                             u8"\n");
                    }
                    emit_opfunc_to(
                        dst,
                        translate::get_uwvmint_stacktop_to_operand_stack_fptr_from_tuple<CompileOption, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32>(
                            tmp_currpos,
                            remain,
                            interpreter_tuple));
                    return;
                }
                break;
            }
            case curr_operand_stack_value_type::f64:
            {
                if constexpr(stacktop_f64_enabled)
                {
                    tmp_currpos.f64_stack_top_curr_pos = start_pos;
                    remain.f64_stack_top_remain_size = count;
                    if(runtime_log_on) [[unlikely]]
                    {
                        ::fast_io::io::print(::uwvm2::uwvm::io::u8runtime_log_output,
                                             u8"[uwvm-int-translator] fn=",
                                             function_index,
                                             u8" event=stacktop.spillN | vt=f64 start=",
                                             start_pos,
                                             u8" remain(f64)=",
                                             count,
                                             u8" currpos(f64)=",
                                             curr_stacktop.f64_stack_top_curr_pos,
                                             u8"\n");
                    }
                    emit_opfunc_to(
                        dst,
                        translate::get_uwvmint_stacktop_to_operand_stack_fptr_from_tuple<CompileOption, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>(
                            tmp_currpos,
                            remain,
                            interpreter_tuple));
                    return;
                }
                break;
            }
            [[unlikely]] default:
            {
                break;
            }
        }

        // Fallback: emit single-value spills.
        {
            ::std::size_t const begin_pos{stacktop_range_begin_pos(vt)};
            ::std::size_t const end_pos{stacktop_range_end_pos(vt)};

            // Spill in deepest->top order so operand-stack memory preserves deep->top layout.
            ::std::size_t pos{start_pos};
            pos = stacktop_ring_advance_next(pos, count - 1uz, begin_pos, end_pos);  // deepest within the segment
            for(::std::size_t i{}; i != count; ++i)
            {
                emit_stacktop_spill1_typed_to(dst, pos, vt);
                pos = stacktop_ring_prev(pos, begin_pos, end_pos);
            }
        }
    }};

auto const emit_stacktop_fill1_typed_to{
    [&]([[maybe_unused]] bytecode_vec_t& dst, [[maybe_unused]] ::std::size_t slot, [[maybe_unused]] curr_operand_stack_value_type vt) constexpr UWVM_THROWS
    {
        if constexpr(stacktop_enabled)
        {
            if(runtime_log_on) [[unlikely]] { ++runtime_log_stats.stacktop_fill1_count; }
            namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
            switch(vt)
            {
                case curr_operand_stack_value_type::i32:
                {
                    emit_opfunc_to(
                        dst,
                        translate::get_uwvmint_operand_stack_to_stacktop_typed_single_fptr_from_tuple<CompileOption,
                                                                                                      ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32>(
                            slot,
                            interpreter_tuple));
                    break;
                }
                case curr_operand_stack_value_type::i64:
                {
                    emit_opfunc_to(
                        dst,
                        translate::get_uwvmint_operand_stack_to_stacktop_typed_single_fptr_from_tuple<CompileOption,
                                                                                                      ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64>(
                            slot,
                            interpreter_tuple));
                    break;
                }
                case curr_operand_stack_value_type::f32:
                {
                    emit_opfunc_to(
                        dst,
                        translate::get_uwvmint_operand_stack_to_stacktop_typed_single_fptr_from_tuple<CompileOption,
                                                                                                      ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32>(
                            slot,
                            interpreter_tuple));
                    break;
                }
                case curr_operand_stack_value_type::f64:
                {
                    emit_opfunc_to(
                        dst,
                        translate::get_uwvmint_operand_stack_to_stacktop_typed_single_fptr_from_tuple<CompileOption,
                                                                                                      ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>(
                            slot,
                            interpreter_tuple));
                    break;
                }
                [[unlikely]] default:
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    break;
                }
            }
        }
    }};

// Emit a multi-value fill (memory -> cache) for a **single scalar type**.
[[maybe_unused]] auto const emit_stacktop_filln_same_vt_to{
    [&]([[maybe_unused]] bytecode_vec_t& dst,
        [[maybe_unused]] ::std::size_t start_pos,
        [[maybe_unused]] ::std::size_t count,
        [[maybe_unused]] curr_operand_stack_value_type vt) constexpr UWVM_THROWS
    {
        if constexpr(!stacktop_enabled) { return; }
        if(count == 0uz) { return; }
        if(runtime_log_on) [[unlikely]] { ++runtime_log_stats.stacktop_fillN_count; }

        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
        using remain_t = ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_remain_size_t;
        auto tmp_currpos{curr_stacktop};
        remain_t remain{};

        switch(vt)
        {
            case curr_operand_stack_value_type::i32:
            {
                if constexpr(stacktop_i32_enabled)
                {
                    tmp_currpos.i32_stack_top_curr_pos = start_pos;
                    remain.i32_stack_top_remain_size = count;
                    if(runtime_log_on) [[unlikely]]
                    {
                        ::fast_io::io::print(::uwvm2::uwvm::io::u8runtime_log_output,
                                             u8"[uwvm-int-translator] fn=",
                                             function_index,
                                             u8" event=stacktop.fillN | vt=i32 start=",
                                             start_pos,
                                             u8" remain(i32)=",
                                             count,
                                             u8" currpos(i32)=",
                                             curr_stacktop.i32_stack_top_curr_pos,
                                             u8"\n");
                    }
                    emit_opfunc_to(
                        dst,
                        translate::get_uwvmint_operand_stack_to_stacktop_fptr_from_tuple<CompileOption, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32>(
                            tmp_currpos,
                            remain,
                            interpreter_tuple));
                    return;
                }
                break;
            }
            case curr_operand_stack_value_type::i64:
            {
                if constexpr(stacktop_i64_enabled)
                {
                    tmp_currpos.i64_stack_top_curr_pos = start_pos;
                    remain.i64_stack_top_remain_size = count;
                    if(runtime_log_on) [[unlikely]]
                    {
                        ::fast_io::io::print(::uwvm2::uwvm::io::u8runtime_log_output,
                                             u8"[uwvm-int-translator] fn=",
                                             function_index,
                                             u8" event=stacktop.fillN | vt=i64 start=",
                                             start_pos,
                                             u8" remain(i64)=",
                                             count,
                                             u8" currpos(i64)=",
                                             curr_stacktop.i64_stack_top_curr_pos,
                                             u8"\n");
                    }
                    emit_opfunc_to(
                        dst,
                        translate::get_uwvmint_operand_stack_to_stacktop_fptr_from_tuple<CompileOption, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64>(
                            tmp_currpos,
                            remain,
                            interpreter_tuple));
                    return;
                }
                break;
            }
            case curr_operand_stack_value_type::f32:
            {
                if constexpr(stacktop_f32_enabled)
                {
                    tmp_currpos.f32_stack_top_curr_pos = start_pos;
                    remain.f32_stack_top_remain_size = count;
                    if(runtime_log_on) [[unlikely]]
                    {
                        ::fast_io::io::print(::uwvm2::uwvm::io::u8runtime_log_output,
                                             u8"[uwvm-int-translator] fn=",
                                             function_index,
                                             u8" event=stacktop.fillN | vt=f32 start=",
                                             start_pos,
                                             u8" remain(f32)=",
                                             count,
                                             u8" currpos(f32)=",
                                             curr_stacktop.f32_stack_top_curr_pos,
                                             u8"\n");
                    }
                    emit_opfunc_to(
                        dst,
                        translate::get_uwvmint_operand_stack_to_stacktop_fptr_from_tuple<CompileOption, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32>(
                            tmp_currpos,
                            remain,
                            interpreter_tuple));
                    return;
                }
                break;
            }
            case curr_operand_stack_value_type::f64:
            {
                if constexpr(stacktop_f64_enabled)
                {
                    tmp_currpos.f64_stack_top_curr_pos = start_pos;
                    remain.f64_stack_top_remain_size = count;
                    if(runtime_log_on) [[unlikely]]
                    {
                        ::fast_io::io::print(::uwvm2::uwvm::io::u8runtime_log_output,
                                             u8"[uwvm-int-translator] fn=",
                                             function_index,
                                             u8" event=stacktop.fillN | vt=f64 start=",
                                             start_pos,
                                             u8" remain(f64)=",
                                             count,
                                             u8" currpos(f64)=",
                                             curr_stacktop.f64_stack_top_curr_pos,
                                             u8"\n");
                    }
                    emit_opfunc_to(
                        dst,
                        translate::get_uwvmint_operand_stack_to_stacktop_fptr_from_tuple<CompileOption, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>(
                            tmp_currpos,
                            remain,
                            interpreter_tuple));
                    return;
                }
                break;
            }
            [[unlikely]] default:
            {
                break;
            }
        }

        // Fallback: emit single-value fills.
        {
            ::std::size_t const begin_pos{stacktop_range_begin_pos(vt)};
            ::std::size_t const end_pos{stacktop_range_end_pos(vt)};

            // Fill in top->deep order: each fill consumes the current operand-stack top and writes the next ring slot.
            ::std::size_t pos{start_pos};
            for(::std::size_t i{}; i != count; ++i)
            {
                emit_stacktop_fill1_typed_to(dst, pos, vt);
                pos = stacktop_ring_next(pos, begin_pos, end_pos);
            }
        }
    }};

auto const stacktop_spill_one_deepest_to{[&](bytecode_vec_t& dst, ::std::size_t max_spill_n = SIZE_MAX) constexpr UWVM_THROWS
                                         {
                                             if constexpr(!stacktop_enabled) { return; }
                                             if(stacktop_cache_count == 0uz) { return; }

                                             stacktop_assert_invariants();

                                             // Deepest cached value is at index `stacktop_memory_count`.
                                             // If several *consecutive* deepest cached values share the same type, batch them into one spill opfunc.
                                             auto const vt{codegen_operand_stack.index_unchecked(stacktop_memory_count).type};
                                             ::std::size_t const begin_pos{stacktop_range_begin_pos(vt)};
                                             ::std::size_t const end_pos{stacktop_range_end_pos(vt)};
                                             ::std::size_t const group_cnt{stacktop_cache_count_for_range(begin_pos, end_pos)};
                                             ::std::size_t const currpos{stacktop_currpos_for_range(begin_pos, end_pos)};
                                             ::std::size_t spill_n{1uz};
                                             {
                                                 auto const cache_end{stacktop_memory_count + stacktop_cache_count};
                                                 while(spill_n < group_cnt && (stacktop_memory_count + spill_n) < cache_end &&
                                                       codegen_operand_stack.index_unchecked(stacktop_memory_count + spill_n).type == vt)
                                                 {
                                                     ++spill_n;
                                                 }
                                             }
                                             if(max_spill_n == 0uz) { return; }
                                             if(spill_n > max_spill_n) { spill_n = max_spill_n; }

                                             // Batch only for disjoint ranges; for merged ranges, fall back to single spills.
                                             // (Typed spill opfunc enforces Count==1.)
                                             if(spill_n > 1uz)
                                             {
                                                 // segment top (least deep among the deepest `spill_n` values)
                                                 ::std::size_t const seg_top_slot{stacktop_ring_advance_next(currpos, group_cnt - spill_n, begin_pos, end_pos)};
                                                 if(runtime_log_on) [[unlikely]]
                                                 {
                                                     ::fast_io::io::print(::uwvm2::uwvm::io::u8runtime_log_output,
                                                                          u8"[uwvm-int-translator] fn=",
                                                                          function_index,
                                                                          u8" event=stacktop.spill_one_deepest(batch) | vt=",
                                                                          runtime_log_vt_name(vt),
                                                                          u8" spill_n=",
                                                                          spill_n,
                                                                          u8" seg_top_slot=",
                                                                          seg_top_slot,
                                                                          u8" group_cnt=",
                                                                          group_cnt,
                                                                          u8" currpos=",
                                                                          currpos,
                                                                          u8" begin=",
                                                                          begin_pos,
                                                                          u8" end=",
                                                                          end_pos,
                                                                          u8"\n");
                                                 }
                                                 emit_stacktop_spilln_same_vt_to(dst, seg_top_slot, spill_n, vt);
                                             }
                                             else
                                             {
                                                 ::std::size_t const deepest_slot{stacktop_ring_advance_next(currpos, group_cnt - 1uz, begin_pos, end_pos)};
                                                 if(runtime_log_on) [[unlikely]]
                                                 {
                                                     ::fast_io::io::print(::uwvm2::uwvm::io::u8runtime_log_output,
                                                                          u8"[uwvm-int-translator] fn=",
                                                                          function_index,
                                                                          u8" event=stacktop.spill_one_deepest(single) | vt=",
                                                                          runtime_log_vt_name(vt),
                                                                          u8" slot=",
                                                                          deepest_slot,
                                                                          u8" group_cnt=",
                                                                          group_cnt,
                                                                          u8" currpos=",
                                                                          currpos,
                                                                          u8" begin=",
                                                                          begin_pos,
                                                                          u8" end=",
                                                                          end_pos,
                                                                          u8"\n");
                                                 }
                                                 emit_stacktop_spill1_typed_to(dst, deepest_slot, vt);
                                             }

                                             // Model effects: one value moved cache -> memory.
                                             stacktop_memory_count += spill_n;
                                             stacktop_cache_count -= spill_n;
                                             stacktop_cache_count_ref_for_vt(vt) -= spill_n;

                                             stacktop_assert_invariants();
                                         }};

auto const stacktop_fill_one_from_memory_to{[&](bytecode_vec_t& dst) constexpr UWVM_THROWS
                                            {
                                                if constexpr(!stacktop_enabled) { return; }
                                                if(stacktop_memory_count == 0uz) { return; }

                                                stacktop_assert_invariants();

                                                // Fill consecutive same-typed values from memory into cache in one opfunc when possible.
                                                auto const vt{codegen_operand_stack.index_unchecked(stacktop_memory_count - 1uz).type};
                                                ::std::size_t const begin_pos{stacktop_range_begin_pos(vt)};
                                                ::std::size_t const end_pos{stacktop_range_end_pos(vt)};
                                                ::std::size_t const group_cnt{stacktop_cache_count_for_range(begin_pos, end_pos)};
                                                ::std::size_t const currpos{stacktop_currpos_for_range(begin_pos, end_pos)};
                                                ::std::size_t const ring_size{end_pos - begin_pos};
                                                ::std::size_t const free_slots{ring_size - group_cnt};
                                                ::std::size_t fill_n{1uz};
                                                if(free_slots > 1uz)
                                                {
                                                    while(fill_n < free_slots && fill_n < stacktop_memory_count &&
                                                          codegen_operand_stack.index_unchecked((stacktop_memory_count - 1uz) - fill_n).type == vt)
                                                    {
                                                        ++fill_n;
                                                    }
                                                }

                                                ::std::size_t const start_slot{stacktop_ring_advance_next(currpos, group_cnt, begin_pos, end_pos)};
                                                if(fill_n > 1uz) { emit_stacktop_filln_same_vt_to(dst, start_slot, fill_n, vt); }
                                                else
                                                {
                                                    emit_stacktop_fill1_typed_to(dst, start_slot, vt);
                                                }

                                                stacktop_memory_count -= fill_n;
                                                stacktop_cache_count += fill_n;
                                                stacktop_cache_count_ref_for_vt(vt) += fill_n;

                                                stacktop_assert_invariants();
                                            }};

auto const stacktop_prepare_push1_typed{[&](bytecode_vec_t& dst, curr_operand_stack_value_type vt) constexpr UWVM_THROWS
                                        {
                                            if constexpr(!stacktop_enabled) { return; }

                                            ::std::size_t const begin_pos{stacktop_range_begin_pos(vt)};
                                            ::std::size_t const end_pos{stacktop_range_end_pos(vt)};
                                            ::std::size_t const ring_size{end_pos - begin_pos};

                                            // Critical correctness: push opfuncs write into `ring_prev(currpos)`. If the ring is full, that slot
                                            // is occupied by the deepest cached value of that range; spill from the deepest cached overall until
                                            // the target range has a free slot.
                                            if(runtime_log_on) [[unlikely]]
                                            {
                                                ::fast_io::io::print(::uwvm2::uwvm::io::u8runtime_log_output,
                                                                     u8"[uwvm-int-translator] fn=",
                                                                     function_index,
                                                                     u8" event=stacktop.prepare_push1(begin) | vt=",
                                                                     runtime_log_vt_name(vt),
                                                                     u8" begin=",
                                                                     begin_pos,
                                                                     u8" end=",
                                                                     end_pos,
                                                                     u8" ring=",
                                                                     ring_size,
                                                                     u8" range_cache=",
                                                                     stacktop_cache_count_for_range(begin_pos, end_pos),
                                                                     u8" stacktop{mem=",
                                                                     stacktop_memory_count,
                                                                     u8",cache=",
                                                                     stacktop_cache_count,
                                                                     u8"}\n");
                                            }
                                            ::std::size_t spill_cnt{};
                                            while(stacktop_cache_count_for_range(begin_pos, end_pos) >= ring_size)
                                            {
                                                // Only spill what is needed to free one slot for this push.
                                                // Over-spilling can strand subsequent opcodes (e.g. `select`) with operands in memory while
                                                // the corresponding opfunc expects them to still reside in the stack-top cache.
                                                stacktop_spill_one_deepest_to(dst, 1uz);
                                                ++spill_cnt;
                                            }

                                            stacktop_assert_invariants();
                                            if(runtime_log_on) [[unlikely]]
                                            {
                                                ::fast_io::io::print(::uwvm2::uwvm::io::u8runtime_log_output,
                                                                     u8"[uwvm-int-translator] fn=",
                                                                     function_index,
                                                                     u8" event=stacktop.prepare_push1(end) | vt=",
                                                                     runtime_log_vt_name(vt),
                                                                     u8" spills=",
                                                                     spill_cnt,
                                                                     u8" range_cache=",
                                                                     stacktop_cache_count_for_range(begin_pos, end_pos),
                                                                     u8" stacktop{mem=",
                                                                     stacktop_memory_count,
                                                                     u8",cache=",
                                                                     stacktop_cache_count,
                                                                     u8"} currpos{i32=",
                                                                     curr_stacktop.i32_stack_top_curr_pos,
                                                                     u8",i64=",
                                                                     curr_stacktop.i64_stack_top_curr_pos,
                                                                     u8",f32=",
                                                                     curr_stacktop.f32_stack_top_curr_pos,
                                                                     u8",f64=",
                                                                     curr_stacktop.f64_stack_top_curr_pos,
                                                                     u8"}\n");
                                            }
                                        }};

auto const stacktop_commit_push1_typed{[&](curr_operand_stack_value_type vt) constexpr noexcept
                                       {
                                           if constexpr(!stacktop_enabled) { return; }

                                           ::std::size_t const begin_pos{stacktop_range_begin_pos(vt)};
                                           ::std::size_t const end_pos{stacktop_range_end_pos(vt)};

                                           ::std::size_t const currpos{stacktop_currpos_for_range(begin_pos, end_pos)};
                                           ::std::size_t const new_pos{stacktop_ring_prev(currpos, begin_pos, end_pos)};
                                           stacktop_set_currpos_for_range(begin_pos, end_pos, new_pos);

                                           ++stacktop_cache_count;
                                           ++stacktop_cache_count_ref_for_vt(vt);
                                           if(runtime_log_on) [[unlikely]]
                                           {
                                               ::fast_io::io::print(::uwvm2::uwvm::io::u8runtime_log_output,
                                                                    u8"[uwvm-int-translator] fn=",
                                                                    function_index,
                                                                    u8" event=stacktop.commit_push1 | vt=",
                                                                    runtime_log_vt_name(vt),
                                                                    u8" currpos=",
                                                                    currpos,
                                                                    u8"->",
                                                                    new_pos,
                                                                    u8" stacktop{mem=",
                                                                    stacktop_memory_count,
                                                                    u8",cache=",
                                                                    stacktop_cache_count,
                                                                    u8"}\n");
                                           }
                                       }};

auto const stacktop_commit_pop_n{[&](::std::size_t n) constexpr noexcept
                                 {
                                     if constexpr(!stacktop_enabled) { return; }
                                     else
                                     {
                                         if(n == 0uz) { return; }

                                         auto const before_curr_stacktop{curr_stacktop};
                                         auto const before_stacktop_memory_count{stacktop_memory_count};
                                         auto const before_stacktop_cache_count{stacktop_cache_count};
                                         auto const before_stacktop_cache_i32_count{stacktop_cache_i32_count};
                                         auto const before_stacktop_cache_i64_count{stacktop_cache_i64_count};
                                         auto const before_stacktop_cache_f32_count{stacktop_cache_f32_count};
                                         auto const before_stacktop_cache_f64_count{stacktop_cache_f64_count};

                                         // Pop from the top; if cache becomes empty, remaining pops consume the memory-only stack.
                                         for(::std::size_t i{}; i != n; ++i)
                                         {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                             if(codegen_operand_stack.empty()) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif
                                             if(codegen_operand_stack.empty()) { return; }

                                             // Pop i-th value from top (type stack is updated by the caller after this commit).
                                             auto const vt{codegen_operand_stack.index_unchecked((codegen_operand_stack.size() - 1uz) - i).type};

                                             ::std::size_t const begin_pos{stacktop_range_begin_pos(vt)};
                                             ::std::size_t const end_pos{stacktop_range_end_pos(vt)};

                                             ::std::size_t const currpos{stacktop_currpos_for_range(begin_pos, end_pos)};
                                             ::std::size_t const new_pos{stacktop_ring_next(currpos, begin_pos, end_pos)};
                                             stacktop_set_currpos_for_range(begin_pos, end_pos, new_pos);

                                             if(stacktop_cache_count != 0uz)
                                             {
                                                 --stacktop_cache_count;
                                                 --stacktop_cache_count_ref_for_vt(vt);
                                             }
                                             else
                                             {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                                 if(stacktop_memory_count == 0uz) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif
                                                 --stacktop_memory_count;
                                             }
                                         }

                                         if(runtime_log_on) [[unlikely]]
                                         {
                                             ::fast_io::io::print(::uwvm2::uwvm::io::u8runtime_log_output,
                                                                  u8"[uwvm-int-translator] fn=",
                                                                  function_index,
                                                                  u8" event=stacktop.commit_pop_n | n=",
                                                                  n,
                                                                  u8" stacktop{mem=",
                                                                  before_stacktop_memory_count,
                                                                  u8",cache=",
                                                                  before_stacktop_cache_count,
                                                                  u8"}->",
                                                                  u8"{mem=",
                                                                  stacktop_memory_count,
                                                                  u8",cache=",
                                                                  stacktop_cache_count,
                                                                  u8"} cache_type{i32=",
                                                                  before_stacktop_cache_i32_count,
                                                                  u8",i64=",
                                                                  before_stacktop_cache_i64_count,
                                                                  u8",f32=",
                                                                  before_stacktop_cache_f32_count,
                                                                  u8",f64=",
                                                                  before_stacktop_cache_f64_count,
                                                                  u8"}->",
                                                                  u8"{i32=",
                                                                  stacktop_cache_i32_count,
                                                                  u8",i64=",
                                                                  stacktop_cache_i64_count,
                                                                  u8",f32=",
                                                                  stacktop_cache_f32_count,
                                                                  u8",f64=",
                                                                  stacktop_cache_f64_count,
                                                                  u8"} currpos{i32=",
                                                                  before_curr_stacktop.i32_stack_top_curr_pos,
                                                                  u8"->",
                                                                  curr_stacktop.i32_stack_top_curr_pos,
                                                                  u8",i64=",
                                                                  before_curr_stacktop.i64_stack_top_curr_pos,
                                                                  u8"->",
                                                                  curr_stacktop.i64_stack_top_curr_pos,
                                                                  u8",f32=",
                                                                  before_curr_stacktop.f32_stack_top_curr_pos,
                                                                  u8"->",
                                                                  curr_stacktop.f32_stack_top_curr_pos,
                                                                  u8",f64=",
                                                                  before_curr_stacktop.f64_stack_top_curr_pos,
                                                                  u8"->",
                                                                  curr_stacktop.f64_stack_top_curr_pos,
                                                                  u8"}\n");
                                         }
                                     }
                                 }};

auto const stacktop_fill_to_canonical{[&](bytecode_vec_t& dst) constexpr UWVM_THROWS
                                      {
                                          if constexpr(!stacktop_enabled) { return; }
                                          else
                                          {

                                              stacktop_assert_invariants();

                                              // Fill from memory into cache as deep as possible while respecting per-range ring capacities.
                                              while(stacktop_memory_count != 0uz)
                                              {
                                                  auto const vt{codegen_operand_stack.index_unchecked(stacktop_memory_count - 1uz).type};
                                                  ::std::size_t const begin_pos{stacktop_range_begin_pos(vt)};
                                                  ::std::size_t const end_pos{stacktop_range_end_pos(vt)};
                                                  ::std::size_t const ring_size{end_pos - begin_pos};
                                                  if(stacktop_cache_count_for_range(begin_pos, end_pos) == ring_size) { break; }
                                                  stacktop_fill_one_from_memory_to(dst);
                                              }

                                              stacktop_assert_invariants();
                                          }
                                      }};

auto const stacktop_flush_all_to_operand_stack{[&](bytecode_vec_t& dst) constexpr UWVM_THROWS
                                               {
                                                   if constexpr(!stacktop_enabled) { return; }
                                                   else
                                                   {
                                                       stacktop_assert_invariants();
                                                       // Spill from deepest to top so operand stack memory ends up in correct deep->top order.
                                                       while(stacktop_cache_count != 0uz) { stacktop_spill_one_deepest_to(dst); }

                                                       stacktop_assert_invariants();
                                                   }
                                               }};

[[maybe_unused]] auto const stacktop_reset_currpos_to_begin{
    [&]() constexpr noexcept
    {
        if constexpr(!stacktop_enabled) { return; }
        curr_stacktop.i32_stack_top_curr_pos = stacktop_i32_enabled ? CompileOption.i32_stack_top_begin_pos : SIZE_MAX;
        curr_stacktop.i64_stack_top_curr_pos = stacktop_i64_enabled ? CompileOption.i64_stack_top_begin_pos : SIZE_MAX;
        curr_stacktop.f32_stack_top_curr_pos = stacktop_f32_enabled ? CompileOption.f32_stack_top_begin_pos : SIZE_MAX;
        curr_stacktop.f64_stack_top_curr_pos = stacktop_f64_enabled ? CompileOption.f64_stack_top_begin_pos : SIZE_MAX;
        curr_stacktop.v128_stack_top_curr_pos = stacktop_v128_enabled ? CompileOption.v128_stack_top_begin_pos : SIZE_MAX;
    }};

[[maybe_unused]] auto const stacktop_transform_currpos_to_begin{
    [&](bytecode_vec_t& dst) constexpr UWVM_THROWS
    {
        if constexpr(stacktop_enabled && CompileOption.is_tail_call && stacktop_regtransform_cf_entry && stacktop_regtransform_supported)
        {
            if(is_polymorphic)
            {
                // Unreachable region: keep compiler-side state deterministic without emitting runtime code.
                stacktop_reset_currpos_to_begin();
                return;
            }

            // If cache is empty there is no live register state to preserve; reset cursors without emitting a transform opfunc.
            if(stacktop_cache_count == 0uz)
            {
                stacktop_reset_currpos_to_begin();
                return;
            }

            namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
            emit_opfunc_to(dst, translate::get_uwvmint_stacktop_transform_to_begin_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            stacktop_reset_currpos_to_begin();
        }
    }};

[[maybe_unused]] auto const stacktop_canonicalize_edge_to_memory{[&](bytecode_vec_t& dst) constexpr UWVM_THROWS
                                                                 {
                                                                     if constexpr(!stacktop_enabled) { return; }
                                                                     // Move all cached values to operand-stack memory (call-like barrier).
                                                                     stacktop_flush_all_to_operand_stack(dst);
                                                                     // Make the empty-cache state deterministic for subsequent codegen.
                                                                     stacktop_reset_currpos_to_begin();
                                                                     stacktop_cache_count = 0uz;
                                                                     stacktop_cache_i32_count = 0uz;
                                                                     stacktop_cache_i64_count = 0uz;
                                                                     stacktop_cache_f32_count = 0uz;
                                                                     stacktop_cache_f64_count = 0uz;
                                                                     stacktop_memory_count = codegen_operand_stack.size();
                                                                 }};

auto const stacktop_prepare_push1_if_reachable{[&](bytecode_vec_t& dst, curr_operand_stack_value_type vt) constexpr UWVM_THROWS
                                               {
                                                   if constexpr(!stacktop_enabled) { return; }
                                                   else
                                                   {
                                                       if(is_polymorphic) { return; }
                                                       stacktop_prepare_push1_typed(dst, vt);
                                                   }
                                               }};

[[maybe_unused]] auto const stacktop_commit_push1_if_reachable{[&](curr_operand_stack_value_type vt) constexpr noexcept
                                                               {
                                                                   if constexpr(!stacktop_enabled) { return; }
                                                                   else
                                                                   {
                                                                       if(is_polymorphic) { return; }
                                                                       stacktop_commit_push1_typed(vt);
                                                                   }
                                                               }};

[[maybe_unused]] auto const stacktop_commit_push1_typed_if_reachable{[&](curr_operand_stack_value_type vt) constexpr UWVM_THROWS
                                                                     {
                                                                         if constexpr(!stacktop_enabled) { return; }
                                                                         else
                                                                         {
                                                                             if(is_polymorphic) { return; }
                                                                             stacktop_commit_push1_typed(vt);
                                                                             codegen_stack_push(vt);
                                                                         }
                                                                     }};

auto const stacktop_after_pop_n_if_reachable{[&](bytecode_vec_t& dst, ::std::size_t n) constexpr UWVM_THROWS
                                             {
                                                 if constexpr(!stacktop_enabled) { return; }
                                                 else
                                                 {
                                                     if(is_polymorphic) { return; }
                                                     stacktop_commit_pop_n(n);
                                                     codegen_stack_pop_n(n);
                                                     stacktop_fill_to_canonical(dst);
                                                 }
                                             }};

// Pop modeling + retype the new top before canonical fill (used for ops like i64.cmp -> i32).
[[maybe_unused]] auto const stacktop_after_pop_n_retype_top_if_reachable{
    [&](bytecode_vec_t& dst, ::std::size_t n, curr_operand_stack_value_type new_top_type) constexpr UWVM_THROWS
    {
        if constexpr(!stacktop_enabled) { return; }
        else
        {
            if(is_polymorphic) { return; }
            stacktop_commit_pop_n(n);
            codegen_stack_pop_n(n);
            codegen_stack_set_top(new_top_type);
            stacktop_fill_to_canonical(dst);
        }
    }};

// Pop+push modeling (used for cross-range ops like f32.cmp -> i32 when i32 range is disjoint from f32 range).
[[maybe_unused]] auto const stacktop_after_pop_n_push1_typed_if_reachable{
    [&](bytecode_vec_t& dst, ::std::size_t pop_n, curr_operand_stack_value_type push_type) constexpr UWVM_THROWS
    {
        if constexpr(!stacktop_enabled) { return; }
        else
        {
            if(is_polymorphic) { return; }
            stacktop_commit_pop_n(pop_n);
            codegen_stack_pop_n(pop_n);
            // Ensure the target stack-top ring has a free slot *after* the pop step.
            // This is required for cross-range ops that keep depth unchanged (pop+push),
            // e.g. `f32.load` (i32 addr -> f32 result) in split GPR/FP ring layouts.
            stacktop_prepare_push1_typed(dst, push_type);
            stacktop_commit_push1_typed(push_type);
            codegen_stack_push(push_type);
            stacktop_fill_to_canonical(dst);
        }
    }};

// Pop+push modeling without emitting any fill ops (used to keep `br_if` fusion candidates contiguous).
[[maybe_unused]] auto const stacktop_after_pop_n_push1_typed_no_fill_if_reachable{
    [&](::std::size_t pop_n, curr_operand_stack_value_type push_type) constexpr noexcept
    {
        if constexpr(!stacktop_enabled) { return; }
        else
        {
            if(is_polymorphic) { return; }
            stacktop_commit_pop_n(pop_n);
            codegen_stack_pop_n(pop_n);
            stacktop_commit_push1_typed(push_type);
            codegen_stack_push(push_type);
        }
    }};

// Pop modeling without emitting any fill ops (used to keep `br_if` fusion candidates contiguous).
[[maybe_unused]] auto const stacktop_after_pop_n_no_fill_if_reachable{[&](::std::size_t n) constexpr noexcept
                                                                      {
                                                                          if constexpr(!stacktop_enabled) { return; }
                                                                          else
                                                                          {
                                                                              if(is_polymorphic) { return; }
                                                                              stacktop_commit_pop_n(n);
                                                                              codegen_stack_pop_n(n);
                                                                          }
                                                                      }};

auto const emit_drop_typed_to{
    [&](bytecode_vec_t& dst, curr_operand_stack_value_type vt) constexpr UWVM_THROWS
    {
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

        auto const emit_real_drop{
            [&]() constexpr UWVM_THROWS
            {
                switch(vt)
                {
                    case curr_operand_stack_value_type::i32:
                    {
                        emit_opfunc_to(dst, translate::get_uwvmint_drop_i32_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        return;
                    }
                    case curr_operand_stack_value_type::i64:
                    {
                        emit_opfunc_to(dst, translate::get_uwvmint_drop_i64_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        return;
                    }
                    case curr_operand_stack_value_type::f32:
                    {
                        emit_opfunc_to(dst, translate::get_uwvmint_drop_f32_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        return;
                    }
                    case curr_operand_stack_value_type::f64:
                    {
                        emit_opfunc_to(dst, translate::get_uwvmint_drop_f64_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        return;
                    }
                    [[unlikely]] default:
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        return;
                    }
                }
            }};

        if constexpr(!stacktop_enabled)
        {
            emit_real_drop();
            return;
        }

        if(is_polymorphic)
        {
            // Unreachable region: emit a real `drop` without mutating compiler-side stack-top model.
            emit_real_drop();
            return;
        }

        // Cache-hit: fold `drop` by updating the compiler-side stack-top cursor (no runtime op).
        // Cache-miss: emit real `drop` to adjust the memory stack pointer, then update the model.
        if(stacktop_cache_count != 0uz)
        {
            stacktop_commit_pop_n(1uz);
            codegen_stack_pop_n(1uz);
            stacktop_fill_to_canonical(dst);
        }
        else
        {
            emit_real_drop();
            stacktop_commit_pop_n(1uz);
            codegen_stack_pop_n(1uz);
            stacktop_fill_to_canonical(dst);
        }
    }};

// Drop modeling without emitting any canonical fills.
// Used for bulk stack-shape repair sequences (br/br_if/br_table/return) where intermediate fills would
// often reload values that are immediately dropped again.
[[maybe_unused]] auto const emit_drop_typed_to_no_fill{
    [&](bytecode_vec_t& dst, curr_operand_stack_value_type vt) constexpr UWVM_THROWS
    {
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

        auto const emit_real_drop{
            [&]() constexpr UWVM_THROWS
            {
                switch(vt)
                {
                    case curr_operand_stack_value_type::i32:
                    {
                        emit_opfunc_to(dst, translate::get_uwvmint_drop_i32_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        return;
                    }
                    case curr_operand_stack_value_type::i64:
                    {
                        emit_opfunc_to(dst, translate::get_uwvmint_drop_i64_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        return;
                    }
                    case curr_operand_stack_value_type::f32:
                    {
                        emit_opfunc_to(dst, translate::get_uwvmint_drop_f32_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        return;
                    }
                    case curr_operand_stack_value_type::f64:
                    {
                        emit_opfunc_to(dst, translate::get_uwvmint_drop_f64_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        return;
                    }
                    [[unlikely]] default:
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        return;
                    }
                }
            }};

        if constexpr(!stacktop_enabled)
        {
            emit_real_drop();
            return;
        }

        if(is_polymorphic)
        {
            // Unreachable region: emit a real `drop` without mutating compiler-side stack-top model.
            emit_real_drop();
            return;
        }

        if(stacktop_cache_count != 0uz)
        {
            stacktop_commit_pop_n(1uz);
            codegen_stack_pop_n(1uz);
        }
        else
        {
            emit_real_drop();
            stacktop_commit_pop_n(1uz);
            codegen_stack_pop_n(1uz);
        }
    }};

// Bulk drop modeling without emitting any canonical fills.
// Used to coalesce long stack-shape repair sequences (br/br_if/br_table/return) into one `drop_bytes` opfunc.
[[maybe_unused]] auto const emit_drop_to_stack_size_no_fill{
    [&](bytecode_vec_t& dst, ::std::size_t target_size) constexpr UWVM_THROWS
    {
        if constexpr(!stacktop_enabled)
        {
            // Fallback: preserve existing behavior (emit per-value drops).
            auto const curr_size{operand_stack.size()};
            if(curr_size <= target_size) { return; }
            for(::std::size_t i{curr_size}; i > target_size; --i) { emit_drop_typed_to_no_fill(dst, operand_stack.index_unchecked(i - 1uz).type); }
            return;
        }

        if(is_polymorphic)
        {
            // Unreachable region: do not mutate compiler-side stack-top model; emit real drops.
            auto const curr_size{operand_stack.size()};
            if(curr_size <= target_size) { return; }
            for(::std::size_t i{curr_size}; i > target_size; --i) { emit_drop_typed_to_no_fill(dst, operand_stack.index_unchecked(i - 1uz).type); }
            return;
        }

        auto const curr_size{codegen_operand_stack.size()};
        if(curr_size <= target_size) { return; }

        ::std::size_t const drop_n{curr_size - target_size};

        // Top `stacktop_cache_count` values are resident in registers (cache); remaining are in operand-stack memory.
        ::std::size_t const cache_pops{stacktop_cache_count < drop_n ? stacktop_cache_count : drop_n};
        ::std::size_t const mem_pops{drop_n - cache_pops};

        if(mem_pops != 0uz)
        {
            ::std::size_t bytes_total{};
            ::std::size_t const mem_end{curr_size - cache_pops};
            for(::std::size_t i{target_size}; i < mem_end; ++i) { bytes_total += operand_stack_valtype_size(codegen_operand_stack.index_unchecked(i).type); }

            if(bytes_total != 0uz)
            {
                using drop_bytes_imm_t = ::std::uint_least32_t;

                namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

                ::std::size_t remaining{bytes_total};
                while(remaining != 0uz)
                {
                    auto const chunk_sz{remaining > ::std::numeric_limits<drop_bytes_imm_t>::max()
                                            ? static_cast<::std::size_t>(::std::numeric_limits<drop_bytes_imm_t>::max())
                                            : remaining};
                    drop_bytes_imm_t const chunk{static_cast<drop_bytes_imm_t>(chunk_sz)};

                    emit_opfunc_to(dst, translate::get_uwvmint_drop_bytes_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                    emit_imm_to(dst, chunk);

                    remaining -= chunk_sz;
                }
            }
        }

        stacktop_commit_pop_n(drop_n);
        codegen_stack_pop_n(drop_n);
    }};

auto const emit_local_get_typed_to{
    [&](bytecode_vec_t& dst, curr_operand_stack_value_type vt, local_offset_t off) constexpr UWVM_THROWS
    {
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

        bool fused_spill_and_local_get{};
        [[maybe_unused]] ::std::size_t fuse_site{};

        if constexpr(stacktop_enabled)
        {
            // local.get pushes 1 value to stack-top cache; spill if ring is full.
            [[maybe_unused]] ::std::size_t const bc_before{dst.size()};
            stacktop_prepare_push1_if_reachable(dst, vt);

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
            // If `stacktop_prepare_push1_*` emitted a spill opfunc immediately before this `local.get`, rewrite that spill opfunc
            // into a fused "spill1 + local.get" opfunc, and reuse the would-be `local.get` opfunc slot for the immediate.
            // This avoids an extra dispatch *and* avoids the runtime "skip next opfunc pointer" pattern.
            if(dst.size() != bc_before)
            {
                constexpr bool i32_i64_merge{CompileOption.i32_stack_top_begin_pos == CompileOption.i64_stack_top_begin_pos &&
                                             CompileOption.i32_stack_top_end_pos == CompileOption.i64_stack_top_end_pos};
                constexpr bool i32_f32_merge{CompileOption.i32_stack_top_begin_pos == CompileOption.f32_stack_top_begin_pos &&
                                             CompileOption.i32_stack_top_end_pos == CompileOption.f32_stack_top_end_pos};
                constexpr bool i32_f64_merge{CompileOption.i32_stack_top_begin_pos == CompileOption.f64_stack_top_begin_pos &&
                                             CompileOption.i32_stack_top_end_pos == CompileOption.f64_stack_top_end_pos};
                constexpr bool i64_f32_merge{CompileOption.i64_stack_top_begin_pos == CompileOption.f32_stack_top_begin_pos &&
                                             CompileOption.i64_stack_top_end_pos == CompileOption.f32_stack_top_end_pos};
                constexpr bool i64_f64_merge{CompileOption.i64_stack_top_begin_pos == CompileOption.f64_stack_top_begin_pos &&
                                             CompileOption.i64_stack_top_end_pos == CompileOption.f64_stack_top_end_pos};
                constexpr bool f32_f64_merge{CompileOption.f32_stack_top_begin_pos == CompileOption.f64_stack_top_begin_pos &&
                                             CompileOption.f32_stack_top_end_pos == CompileOption.f64_stack_top_end_pos};

                using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
                using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
                using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
                using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;

                auto const spilled_vt{codegen_operand_stack.index_unchecked(stacktop_memory_count - 1uz).type};

                // Patch the *last* emitted spill opfunc (spillN is not emitted here; prepare_push1 emits spill1 only).
                using opfunc_ptr_t = decltype(translate::get_uwvmint_local_get_i32_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                fuse_site = dst.size() - sizeof(opfunc_ptr_t);

                auto patch_with{[&](auto fused_fptr) constexpr UWVM_THROWS
                                {
                                    ::std::byte tmp[sizeof(fused_fptr)];
                                    ::std::memcpy(tmp, ::std::addressof(fused_fptr), sizeof(fused_fptr));
                                    ::std::memcpy(dst.data() + fuse_site, tmp, sizeof(fused_fptr));
                                    fused_spill_and_local_get = true;
                                }};

                // Decide spill-value type (SpilledT) from the translator model.
                switch(vt)
                {
                    case curr_operand_stack_value_type::i32:
                    {
                        if(spilled_vt == curr_operand_stack_value_type::i32)
                        {
                            patch_with(translate::get_uwvmint_stacktop_spill1_then_local_get_typed_fptr_from_tuple<CompileOption, wasm_i32, wasm_i32>(
                                curr_stacktop,
                                interpreter_tuple));
                        }
                        else if(spilled_vt == curr_operand_stack_value_type::i64)
                        {
                            if constexpr(i32_i64_merge)
                            {
                                patch_with(translate::get_uwvmint_stacktop_spill1_then_local_get_typed_fptr_from_tuple<CompileOption, wasm_i64, wasm_i32>(
                                    curr_stacktop,
                                    interpreter_tuple));
                            }
                        }
                        else if(spilled_vt == curr_operand_stack_value_type::f32)
                        {
                            if constexpr(i32_f32_merge)
                            {
                                patch_with(translate::get_uwvmint_stacktop_spill1_then_local_get_typed_fptr_from_tuple<CompileOption, wasm_f32, wasm_i32>(
                                    curr_stacktop,
                                    interpreter_tuple));
                            }
                        }
                        else if(spilled_vt == curr_operand_stack_value_type::f64)
                        {
                            if constexpr(i32_f64_merge)
                            {
                                patch_with(translate::get_uwvmint_stacktop_spill1_then_local_get_typed_fptr_from_tuple<CompileOption, wasm_f64, wasm_i32>(
                                    curr_stacktop,
                                    interpreter_tuple));
                            }
                        }
                        break;
                    }
                    case curr_operand_stack_value_type::i64:
                    {
                        if(spilled_vt == curr_operand_stack_value_type::i64)
                        {
                            patch_with(translate::get_uwvmint_stacktop_spill1_then_local_get_typed_fptr_from_tuple<CompileOption, wasm_i64, wasm_i64>(
                                curr_stacktop,
                                interpreter_tuple));
                        }
                        else if(spilled_vt == curr_operand_stack_value_type::i32)
                        {
                            if constexpr(i32_i64_merge)
                            {
                                patch_with(translate::get_uwvmint_stacktop_spill1_then_local_get_typed_fptr_from_tuple<CompileOption, wasm_i32, wasm_i64>(
                                    curr_stacktop,
                                    interpreter_tuple));
                            }
                        }
                        else if(spilled_vt == curr_operand_stack_value_type::f32)
                        {
                            if constexpr(i64_f32_merge)
                            {
                                patch_with(translate::get_uwvmint_stacktop_spill1_then_local_get_typed_fptr_from_tuple<CompileOption, wasm_f32, wasm_i64>(
                                    curr_stacktop,
                                    interpreter_tuple));
                            }
                        }
                        else if(spilled_vt == curr_operand_stack_value_type::f64)
                        {
                            if constexpr(i64_f64_merge)
                            {
                                patch_with(translate::get_uwvmint_stacktop_spill1_then_local_get_typed_fptr_from_tuple<CompileOption, wasm_f64, wasm_i64>(
                                    curr_stacktop,
                                    interpreter_tuple));
                            }
                        }
                        break;
                    }
                    case curr_operand_stack_value_type::f32:
                    {
                        if(spilled_vt == curr_operand_stack_value_type::f32)
                        {
                            patch_with(translate::get_uwvmint_stacktop_spill1_then_local_get_typed_fptr_from_tuple<CompileOption, wasm_f32, wasm_f32>(
                                curr_stacktop,
                                interpreter_tuple));
                        }
                        else if(spilled_vt == curr_operand_stack_value_type::f64)
                        {
                            if constexpr(f32_f64_merge)
                            {
                                patch_with(translate::get_uwvmint_stacktop_spill1_then_local_get_typed_fptr_from_tuple<CompileOption, wasm_f64, wasm_f32>(
                                    curr_stacktop,
                                    interpreter_tuple));
                            }
                        }
                        else if(spilled_vt == curr_operand_stack_value_type::i32)
                        {
                            if constexpr(i32_f32_merge)
                            {
                                patch_with(translate::get_uwvmint_stacktop_spill1_then_local_get_typed_fptr_from_tuple<CompileOption, wasm_i32, wasm_f32>(
                                    curr_stacktop,
                                    interpreter_tuple));
                            }
                        }
                        else if(spilled_vt == curr_operand_stack_value_type::i64)
                        {
                            if constexpr(i64_f32_merge)
                            {
                                patch_with(translate::get_uwvmint_stacktop_spill1_then_local_get_typed_fptr_from_tuple<CompileOption, wasm_i64, wasm_f32>(
                                    curr_stacktop,
                                    interpreter_tuple));
                            }
                        }
                        break;
                    }
                    case curr_operand_stack_value_type::f64:
                    {
                        if(spilled_vt == curr_operand_stack_value_type::f64)
                        {
                            patch_with(translate::get_uwvmint_stacktop_spill1_then_local_get_typed_fptr_from_tuple<CompileOption, wasm_f64, wasm_f64>(
                                curr_stacktop,
                                interpreter_tuple));
                        }
                        else if(spilled_vt == curr_operand_stack_value_type::f32)
                        {
                            if constexpr(f32_f64_merge)
                            {
                                patch_with(translate::get_uwvmint_stacktop_spill1_then_local_get_typed_fptr_from_tuple<CompileOption, wasm_f32, wasm_f64>(
                                    curr_stacktop,
                                    interpreter_tuple));
                            }
                        }
                        else if(spilled_vt == curr_operand_stack_value_type::i32)
                        {
                            if constexpr(i32_f64_merge)
                            {
                                patch_with(translate::get_uwvmint_stacktop_spill1_then_local_get_typed_fptr_from_tuple<CompileOption, wasm_i32, wasm_f64>(
                                    curr_stacktop,
                                    interpreter_tuple));
                            }
                        }
                        else if(spilled_vt == curr_operand_stack_value_type::i64)
                        {
                            if constexpr(i64_f64_merge)
                            {
                                patch_with(translate::get_uwvmint_stacktop_spill1_then_local_get_typed_fptr_from_tuple<CompileOption, wasm_i64, wasm_f64>(
                                    curr_stacktop,
                                    interpreter_tuple));
                            }
                        }
                        break;
                    }
                    [[unlikely]] default:
                    {
                        break;
                    }
                }

                if(fused_spill_and_local_get)
                {
                    // Emit only the immediate; opfunc slot was patched above.
                    emit_imm_to(dst, off);
                }
            }
#endif
        }

        if(!fused_spill_and_local_get) switch(vt)
            {
                case curr_operand_stack_value_type::i32:
                {
                    emit_opfunc_to(dst, translate::get_uwvmint_local_get_i32_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                    emit_imm_to(dst, off);
                    break;
                }
                case curr_operand_stack_value_type::i64:
                {
                    emit_opfunc_to(dst, translate::get_uwvmint_local_get_i64_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                    emit_imm_to(dst, off);
                    break;
                }
                case curr_operand_stack_value_type::f32:
                {
                    emit_opfunc_to(dst, translate::get_uwvmint_local_get_f32_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                    emit_imm_to(dst, off);
                    break;
                }
                case curr_operand_stack_value_type::f64:
                {
                    emit_opfunc_to(dst, translate::get_uwvmint_local_get_f64_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                    emit_imm_to(dst, off);
                    break;
                }
                [[unlikely]] default:
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    break;
                }
            }

        if constexpr(stacktop_enabled)
        {
            // Model effects: 1 value pushed into stack-top cache.
            stacktop_commit_push1_typed_if_reachable(vt);
        }
    }};

[[maybe_unused]] auto const emit_const_i32_to{
    [&](bytecode_vec_t& dst, wasm_i32 imm) constexpr UWVM_THROWS
    {
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

        bool fused_spill_and_const{};

        if constexpr(stacktop_enabled)
        {
            [[maybe_unused]] ::std::size_t const bc_before{dst.size()};
            stacktop_prepare_push1_if_reachable(dst, curr_operand_stack_value_type::i32);

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
            if(dst.size() != bc_before)
            {
                constexpr bool i32_i64_merge{CompileOption.i32_stack_top_begin_pos == CompileOption.i64_stack_top_begin_pos &&
                                             CompileOption.i32_stack_top_end_pos == CompileOption.i64_stack_top_end_pos};
                constexpr bool i32_f32_merge{CompileOption.i32_stack_top_begin_pos == CompileOption.f32_stack_top_begin_pos &&
                                             CompileOption.i32_stack_top_end_pos == CompileOption.f32_stack_top_end_pos};
                constexpr bool i32_f64_merge{CompileOption.i32_stack_top_begin_pos == CompileOption.f64_stack_top_begin_pos &&
                                             CompileOption.i32_stack_top_end_pos == CompileOption.f64_stack_top_end_pos};

                using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
                using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;

                auto const spilled_vt{codegen_operand_stack.index_unchecked(stacktop_memory_count - 1uz).type};

                using opfunc_ptr_t = decltype(translate::get_uwvmint_i32_const_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                ::std::size_t const fuse_site{dst.size() - sizeof(opfunc_ptr_t)};

                auto patch_with{[&](auto fused_fptr) constexpr UWVM_THROWS
                                {
                                    ::std::byte tmp[sizeof(fused_fptr)];
                                    ::std::memcpy(tmp, ::std::addressof(fused_fptr), sizeof(fused_fptr));
                                    ::std::memcpy(dst.data() + fuse_site, tmp, sizeof(fused_fptr));
                                    fused_spill_and_const = true;
                                }};

                if(spilled_vt == curr_operand_stack_value_type::i32)
                {
                    patch_with(translate::get_uwvmint_stacktop_spill1_then_const_typed_fptr_from_tuple<CompileOption, wasm_i32, wasm_i32>(curr_stacktop,
                                                                                                                                          interpreter_tuple));
                }
                else if(spilled_vt == curr_operand_stack_value_type::i64)
                {
                    if constexpr(i32_i64_merge)
                    {
                        patch_with(
                            translate::get_uwvmint_stacktop_spill1_then_const_typed_fptr_from_tuple<CompileOption, wasm_i64, wasm_i32>(curr_stacktop,
                                                                                                                                       interpreter_tuple));
                    }
                }
                else if(spilled_vt == curr_operand_stack_value_type::f32)
                {
                    if constexpr(i32_f32_merge)
                    {
                        patch_with(
                            translate::get_uwvmint_stacktop_spill1_then_const_typed_fptr_from_tuple<CompileOption, wasm_f32, wasm_i32>(curr_stacktop,
                                                                                                                                       interpreter_tuple));
                    }
                }
                else if(spilled_vt == curr_operand_stack_value_type::f64)
                {
                    if constexpr(i32_f64_merge)
                    {
                        patch_with(
                            translate::get_uwvmint_stacktop_spill1_then_const_typed_fptr_from_tuple<CompileOption, wasm_f64, wasm_i32>(curr_stacktop,
                                                                                                                                       interpreter_tuple));
                    }
                }

                if(fused_spill_and_const) { emit_imm_to(dst, imm); }
            }
#endif
        }

        if(!fused_spill_and_const)
        {
            emit_opfunc_to(dst, translate::get_uwvmint_i32_const_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            emit_imm_to(dst, imm);
        }

        if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
    }};

[[maybe_unused]] auto const emit_const_i64_to{
    [&](bytecode_vec_t& dst, wasm_i64 imm) constexpr UWVM_THROWS
    {
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

        bool fused_spill_and_const{};

        if constexpr(stacktop_enabled)
        {
            [[maybe_unused]] ::std::size_t const bc_before{dst.size()};
            stacktop_prepare_push1_if_reachable(dst, curr_operand_stack_value_type::i64);

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
            if(dst.size() != bc_before)
            {
                constexpr bool i32_i64_merge{CompileOption.i32_stack_top_begin_pos == CompileOption.i64_stack_top_begin_pos &&
                                             CompileOption.i32_stack_top_end_pos == CompileOption.i64_stack_top_end_pos};
                constexpr bool i64_f32_merge{CompileOption.i64_stack_top_begin_pos == CompileOption.f32_stack_top_begin_pos &&
                                             CompileOption.i64_stack_top_end_pos == CompileOption.f32_stack_top_end_pos};
                constexpr bool i64_f64_merge{CompileOption.i64_stack_top_begin_pos == CompileOption.f64_stack_top_begin_pos &&
                                             CompileOption.i64_stack_top_end_pos == CompileOption.f64_stack_top_end_pos};

                using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
                using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;

                auto const spilled_vt{codegen_operand_stack.index_unchecked(stacktop_memory_count - 1uz).type};

                using opfunc_ptr_t = decltype(translate::get_uwvmint_i64_const_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                ::std::size_t const fuse_site{dst.size() - sizeof(opfunc_ptr_t)};

                auto patch_with{[&](auto fused_fptr) constexpr UWVM_THROWS
                                {
                                    ::std::byte tmp[sizeof(fused_fptr)];
                                    ::std::memcpy(tmp, ::std::addressof(fused_fptr), sizeof(fused_fptr));
                                    ::std::memcpy(dst.data() + fuse_site, tmp, sizeof(fused_fptr));
                                    fused_spill_and_const = true;
                                }};

                if(spilled_vt == curr_operand_stack_value_type::i64)
                {
                    patch_with(translate::get_uwvmint_stacktop_spill1_then_const_typed_fptr_from_tuple<CompileOption, wasm_i64, wasm_i64>(curr_stacktop,
                                                                                                                                          interpreter_tuple));
                }
                else if(spilled_vt == curr_operand_stack_value_type::i32)
                {
                    if constexpr(i32_i64_merge)
                    {
                        patch_with(
                            translate::get_uwvmint_stacktop_spill1_then_const_typed_fptr_from_tuple<CompileOption, wasm_i32, wasm_i64>(curr_stacktop,
                                                                                                                                       interpreter_tuple));
                    }
                }
                else if(spilled_vt == curr_operand_stack_value_type::f32)
                {
                    if constexpr(i64_f32_merge)
                    {
                        patch_with(
                            translate::get_uwvmint_stacktop_spill1_then_const_typed_fptr_from_tuple<CompileOption, wasm_f32, wasm_i64>(curr_stacktop,
                                                                                                                                       interpreter_tuple));
                    }
                }
                else if(spilled_vt == curr_operand_stack_value_type::f64)
                {
                    if constexpr(i64_f64_merge)
                    {
                        patch_with(
                            translate::get_uwvmint_stacktop_spill1_then_const_typed_fptr_from_tuple<CompileOption, wasm_f64, wasm_i64>(curr_stacktop,
                                                                                                                                       interpreter_tuple));
                    }
                }

                if(fused_spill_and_const) { emit_imm_to(dst, imm); }
            }
#endif
        }

        if(!fused_spill_and_const)
        {
            emit_opfunc_to(dst, translate::get_uwvmint_i64_const_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            emit_imm_to(dst, imm);
        }

        if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64); }
    }};

[[maybe_unused]] auto const emit_const_f32_to{
    [&](bytecode_vec_t& dst, wasm_f32 imm) constexpr UWVM_THROWS
    {
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

        bool fused_spill_and_const{};

        if constexpr(stacktop_enabled)
        {
            [[maybe_unused]] ::std::size_t const bc_before{dst.size()};
            stacktop_prepare_push1_if_reachable(dst, curr_operand_stack_value_type::f32);

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
            if(dst.size() != bc_before)
            {
                constexpr bool i32_f32_merge{CompileOption.i32_stack_top_begin_pos == CompileOption.f32_stack_top_begin_pos &&
                                             CompileOption.i32_stack_top_end_pos == CompileOption.f32_stack_top_end_pos};
                constexpr bool i64_f32_merge{CompileOption.i64_stack_top_begin_pos == CompileOption.f32_stack_top_begin_pos &&
                                             CompileOption.i64_stack_top_end_pos == CompileOption.f32_stack_top_end_pos};
                constexpr bool f32_f64_merge{CompileOption.f32_stack_top_begin_pos == CompileOption.f64_stack_top_begin_pos &&
                                             CompileOption.f32_stack_top_end_pos == CompileOption.f64_stack_top_end_pos};

                using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;

                auto const spilled_vt{codegen_operand_stack.index_unchecked(stacktop_memory_count - 1uz).type};

                using opfunc_ptr_t = decltype(translate::get_uwvmint_f32_const_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                ::std::size_t const fuse_site{dst.size() - sizeof(opfunc_ptr_t)};

                auto patch_with{[&](auto fused_fptr) constexpr UWVM_THROWS
                                {
                                    ::std::byte tmp[sizeof(fused_fptr)];
                                    ::std::memcpy(tmp, ::std::addressof(fused_fptr), sizeof(fused_fptr));
                                    ::std::memcpy(dst.data() + fuse_site, tmp, sizeof(fused_fptr));
                                    fused_spill_and_const = true;
                                }};

                if(spilled_vt == curr_operand_stack_value_type::f32)
                {
                    patch_with(translate::get_uwvmint_stacktop_spill1_then_const_typed_fptr_from_tuple<CompileOption, wasm_f32, wasm_f32>(curr_stacktop,
                                                                                                                                          interpreter_tuple));
                }
                else if(spilled_vt == curr_operand_stack_value_type::f64)
                {
                    if constexpr(f32_f64_merge)
                    {
                        patch_with(
                            translate::get_uwvmint_stacktop_spill1_then_const_typed_fptr_from_tuple<CompileOption, wasm_f64, wasm_f32>(curr_stacktop,
                                                                                                                                       interpreter_tuple));
                    }
                }
                else if(spilled_vt == curr_operand_stack_value_type::i32)
                {
                    if constexpr(i32_f32_merge)
                    {
                        patch_with(
                            translate::get_uwvmint_stacktop_spill1_then_const_typed_fptr_from_tuple<CompileOption, wasm_i32, wasm_f32>(curr_stacktop,
                                                                                                                                       interpreter_tuple));
                    }
                }
                else if(spilled_vt == curr_operand_stack_value_type::i64)
                {
                    if constexpr(i64_f32_merge)
                    {
                        patch_with(
                            translate::get_uwvmint_stacktop_spill1_then_const_typed_fptr_from_tuple<CompileOption, wasm_i64, wasm_f32>(curr_stacktop,
                                                                                                                                       interpreter_tuple));
                    }
                }

                if(fused_spill_and_const) { emit_imm_to(dst, imm); }
            }
#endif
        }

        if(!fused_spill_and_const)
        {
            emit_opfunc_to(dst, translate::get_uwvmint_f32_const_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            emit_imm_to(dst, imm);
        }

        if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f32); }
    }};

[[maybe_unused]] auto const emit_const_f64_to{
    [&](bytecode_vec_t& dst, wasm_f64 imm) constexpr UWVM_THROWS
    {
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

        bool fused_spill_and_const{};

        if constexpr(stacktop_enabled)
        {
            [[maybe_unused]] ::std::size_t const bc_before{dst.size()};
            stacktop_prepare_push1_if_reachable(dst, curr_operand_stack_value_type::f64);

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
            if(dst.size() != bc_before)
            {
                constexpr bool i32_f64_merge{CompileOption.i32_stack_top_begin_pos == CompileOption.f64_stack_top_begin_pos &&
                                             CompileOption.i32_stack_top_end_pos == CompileOption.f64_stack_top_end_pos};
                constexpr bool i64_f64_merge{CompileOption.i64_stack_top_begin_pos == CompileOption.f64_stack_top_begin_pos &&
                                             CompileOption.i64_stack_top_end_pos == CompileOption.f64_stack_top_end_pos};
                constexpr bool f32_f64_merge{CompileOption.f32_stack_top_begin_pos == CompileOption.f64_stack_top_begin_pos &&
                                             CompileOption.f32_stack_top_end_pos == CompileOption.f64_stack_top_end_pos};

                using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;

                auto const spilled_vt{codegen_operand_stack.index_unchecked(stacktop_memory_count - 1uz).type};

                using opfunc_ptr_t = decltype(translate::get_uwvmint_f64_const_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                ::std::size_t const fuse_site{dst.size() - sizeof(opfunc_ptr_t)};

                auto patch_with{[&](auto fused_fptr) constexpr UWVM_THROWS
                                {
                                    ::std::byte tmp[sizeof(fused_fptr)];
                                    ::std::memcpy(tmp, ::std::addressof(fused_fptr), sizeof(fused_fptr));
                                    ::std::memcpy(dst.data() + fuse_site, tmp, sizeof(fused_fptr));
                                    fused_spill_and_const = true;
                                }};

                if(spilled_vt == curr_operand_stack_value_type::f64)
                {
                    patch_with(translate::get_uwvmint_stacktop_spill1_then_const_typed_fptr_from_tuple<CompileOption, wasm_f64, wasm_f64>(curr_stacktop,
                                                                                                                                          interpreter_tuple));
                }
                else if(spilled_vt == curr_operand_stack_value_type::f32)
                {
                    if constexpr(f32_f64_merge)
                    {
                        patch_with(
                            translate::get_uwvmint_stacktop_spill1_then_const_typed_fptr_from_tuple<CompileOption, wasm_f32, wasm_f64>(curr_stacktop,
                                                                                                                                       interpreter_tuple));
                    }
                }
                else if(spilled_vt == curr_operand_stack_value_type::i32)
                {
                    if constexpr(i32_f64_merge)
                    {
                        patch_with(
                            translate::get_uwvmint_stacktop_spill1_then_const_typed_fptr_from_tuple<CompileOption, wasm_i32, wasm_f64>(curr_stacktop,
                                                                                                                                       interpreter_tuple));
                    }
                }
                else if(spilled_vt == curr_operand_stack_value_type::i64)
                {
                    if constexpr(i64_f64_merge)
                    {
                        patch_with(
                            translate::get_uwvmint_stacktop_spill1_then_const_typed_fptr_from_tuple<CompileOption, wasm_i64, wasm_f64>(curr_stacktop,
                                                                                                                                       interpreter_tuple));
                    }
                }

                if(fused_spill_and_const) { emit_imm_to(dst, imm); }
            }
#endif
        }

        if(!fused_spill_and_const)
        {
            emit_opfunc_to(dst, translate::get_uwvmint_f64_const_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            emit_imm_to(dst, imm);
        }

        if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64); }
    }};

auto const emit_local_set_typed_to{
    [&](bytecode_vec_t& dst, curr_operand_stack_value_type vt, local_offset_t off) constexpr UWVM_THROWS
    {
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

        if constexpr(stacktop_enabled)
        {
            // local.set reads its operand from stack-top cache; ensure cache is populated.
            if(!is_polymorphic && stacktop_cache_count == 0uz) { stacktop_fill_to_canonical(dst); }
        }

        switch(vt)
        {
            case curr_operand_stack_value_type::i32:
            {
                emit_opfunc_to(dst, translate::get_uwvmint_local_set_i32_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(dst, off);
                break;
            }
            case curr_operand_stack_value_type::i64:
            {
                emit_opfunc_to(dst, translate::get_uwvmint_local_set_i64_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(dst, off);
                break;
            }
            case curr_operand_stack_value_type::f32:
            {
                emit_opfunc_to(dst, translate::get_uwvmint_local_set_f32_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(dst, off);
                break;
            }
            case curr_operand_stack_value_type::f64:
            {
                emit_opfunc_to(dst, translate::get_uwvmint_local_set_f64_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(dst, off);
                break;
            }
            [[unlikely]] default:
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                break;
            }
        }

        if constexpr(stacktop_enabled)
        {
            // Model effects: 1 value popped (compiler-managed stack-top cursor).
            if(!is_polymorphic)
            {
                stacktop_commit_pop_n(1uz);
                codegen_stack_pop_n(1uz);
                stacktop_fill_to_canonical(dst);
            }
        }
    }};

// Local.set without emitting any canonical fills (see `emit_drop_typed_to_no_fill` rationale).
[[maybe_unused]] auto const emit_local_set_typed_to_no_fill{
    [&](bytecode_vec_t& dst, curr_operand_stack_value_type vt, local_offset_t off) constexpr UWVM_THROWS
    {
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

        if constexpr(stacktop_enabled)
        {
            // local.set reads its operand from stack-top cache; ensure the top value is resident in cache.
            // Avoid a full canonical refill here; bulk repair will canonicalize once at the end when needed.
            if(!is_polymorphic && stacktop_cache_count == 0uz) { stacktop_fill_one_from_memory_to(dst); }
        }

        switch(vt)
        {
            case curr_operand_stack_value_type::i32:
            {
                emit_opfunc_to(dst, translate::get_uwvmint_local_set_i32_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(dst, off);
                break;
            }
            case curr_operand_stack_value_type::i64:
            {
                emit_opfunc_to(dst, translate::get_uwvmint_local_set_i64_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(dst, off);
                break;
            }
            case curr_operand_stack_value_type::f32:
            {
                emit_opfunc_to(dst, translate::get_uwvmint_local_set_f32_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(dst, off);
                break;
            }
            case curr_operand_stack_value_type::f64:
            {
                emit_opfunc_to(dst, translate::get_uwvmint_local_set_f64_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(dst, off);
                break;
            }
            [[unlikely]] default:
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                break;
            }
        }

        if constexpr(stacktop_enabled)
        {
            if(!is_polymorphic)
            {
                stacktop_commit_pop_n(1uz);
                codegen_stack_pop_n(1uz);
            }
        }
    }};

auto const emit_local_tee_typed_to{
    [&](bytecode_vec_t& dst, curr_operand_stack_value_type vt, local_offset_t off) constexpr UWVM_THROWS -> ::std::size_t
    {
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

        if constexpr(stacktop_enabled)
        {
            // local.tee reads the top value from stack-top cache; ensure cache is populated.
            if(!is_polymorphic && stacktop_cache_count == 0uz) { stacktop_fill_to_canonical(dst); }
        }

        ::std::size_t const site{dst.size()};

        switch(vt)
        {
            case curr_operand_stack_value_type::i32:
            {
                emit_opfunc_to(dst, translate::get_uwvmint_local_tee_i32_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(dst, off);
                break;
            }
            case curr_operand_stack_value_type::i64:
            {
                emit_opfunc_to(dst, translate::get_uwvmint_local_tee_i64_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(dst, off);
                break;
            }
            case curr_operand_stack_value_type::f32:
            {
                emit_opfunc_to(dst, translate::get_uwvmint_local_tee_f32_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(dst, off);
                break;
            }
            case curr_operand_stack_value_type::f64:
            {
                emit_opfunc_to(dst, translate::get_uwvmint_local_tee_f64_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(dst, off);
                break;
            }
            [[unlikely]] default:
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                break;
            }
        }

        return site;
    }};

auto const emit_br_to{[&](bytecode_vec_t& dst, ::std::size_t label_id, bool dst_is_thunk) constexpr UWVM_THROWS
                      {
                          namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
                          if(runtime_log_on) [[unlikely]]
                          {
                              ++runtime_log_stats.cf_br_count;
                              if(runtime_log_emit_cf)
                              {
                                  ::fast_io::io::print(::uwvm2::uwvm::io::u8runtime_log_output,
                                                       u8"[uwvm-int-translator] fn=",
                                                       function_index,
                                                       u8" ip=",
                                                       runtime_log_curr_ip,
                                                       u8" event=bytecode.emit.cf | op=br bc=",
                                                       runtime_log_bc_name(dst_is_thunk),
                                                       u8" off=",
                                                       dst.size(),
                                                       u8" label_id=",
                                                       label_id,
                                                       u8"\n");
                              }
                          }
                          emit_opfunc_to(dst, translate::get_uwvmint_br_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                          emit_ptr_label_placeholder(label_id, dst_is_thunk);
                      }};

[[maybe_unused]] auto const emit_br_to_with_stacktop_transform{
    [&](bytecode_vec_t& dst, ::std::size_t label_id, bool dst_is_thunk) constexpr UWVM_THROWS
    {
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

        if constexpr(stacktop_enabled && CompileOption.is_tail_call && stacktop_regtransform_cf_entry && stacktop_regtransform_supported)
        {
            if(!is_polymorphic && stacktop_cache_count != 0uz)
            {
                if(runtime_log_on) [[unlikely]]
                {
                    ++runtime_log_stats.cf_br_transform_count;
                    if(runtime_log_emit_cf)
                    {
                        ::fast_io::io::print(::uwvm2::uwvm::io::u8runtime_log_output,
                                             u8"[uwvm-int-translator] fn=",
                                             function_index,
                                             u8" ip=",
                                             runtime_log_curr_ip,
                                             u8" event=bytecode.emit.cf | op=br_stacktop_transform_to_begin bc=",
                                             runtime_log_bc_name(dst_is_thunk),
                                             u8" off=",
                                             dst.size(),
                                             u8" label_id=",
                                             label_id,
                                             u8" cache=",
                                             stacktop_cache_count,
                                             u8"\n");
                    }
                }
                emit_opfunc_to(dst, translate::get_uwvmint_br_stacktop_transform_to_begin_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_ptr_label_placeholder(label_id, dst_is_thunk);
                return;
            }
        }
#endif

        // Fallback: plain `br`.
        emit_br_to(dst, label_id, dst_is_thunk);
    }};

auto const emit_return_to{[&](bytecode_vec_t& dst) constexpr UWVM_THROWS
                          {
                              namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
                              // Tail-call `return` requires flushing cached stack-top values back to the operand stack memory.
                              // See optable/control.h `uwvmint_return` contract.
                              stacktop_flush_all_to_operand_stack(dst);
                              emit_opfunc_to(dst, translate::get_uwvmint_return_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                          }};
