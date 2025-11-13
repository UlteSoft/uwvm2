/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       WebAssembly Release 1.0 (2019-07-20)
 * @details     antecedent dependency: null
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-05-07
 * @copyright   APL-2.0 License
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V /  | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

#pragma once

#ifndef UWVM_MODULE
// std
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <climits>
# include <concepts>
# include <type_traits>
# include <utility>
# include <memory>
# include <bit>
# include <numeric>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// platform
# if defined(_MSC_VER) && !defined(__clang__)
#  if !defined(_KERNEL_MODE) && defined(_M_AMD64)
#   include <emmintrin.h>  // MSVC x86_64-SSE2
#  endif
#  if !defined(_KERNEL_MODE) && defined(_M_ARM64)
#   include <arm_neon.h>  // MSVC aarch64-NEON
#  endif
# endif
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/utils/intrinsics/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/utils/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/section/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/opcode/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include "def.h"
# include "feature_def.h"
# include "parser_limit.h"
# include "types.h"
# include "type_section.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1::features
{
    struct function_section_storage_t UWVM_TRIVIALLY_RELOCATABLE_IF_ELIGIBLE
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view section_name{u8"Function"};
        inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte section_id{
            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(::uwvm2::parser::wasm::standard::wasm1::section::section_id::function_sec)};

        ::uwvm2::parser::wasm::standard::wasm1::section::section_span_view sec_span{};

        ::uwvm2::parser::wasm::standard::wasm1::features::vectypeidx_minimize_storage_t funcs{};
    };

    /// @brief  Supported SIMD instruction sets: (Algorithm by MacroModel)
    ///             x86/x64 architectures:
    ///                 * SSE2: 128-bit vector processing, 16 bytes at a time
    ///                 * SSSE3: Enhanced byte reordering
    ///                 * SSE4: Optimized vector test instructions
    ///                 * AVX: 256-bit vector processing, 32 bytes at a time
    ///                 * AVX2: Enhanced 256-bit integer operations
    ///                 * AVX512BW: 512-bit vector processing, 64 bytes at a time
    ///                 * AVX512VBMI + AVX512VBMI2: Advanced byte reordering and substitution for the most complex variable-length integer parsing
    ///             ARM Architecture:
    ///                 * NEON: 128-bit vector processing
    ///                 * SVE: Variable-length vector expansion, supports dynamic vector lengths
    ///                 * SME SVE Stream Mode: SVE Stream Mode in Matrix Expansion (not recommended for CPUs such as the Apple M4 because of high latency)
    ///             Other architectures:
    ///                 * LoongArch LASX: 256-bit vector processing
    ///                 * LoongArch LSX: 128-bit vector processing
    ///                 * WebAssembly SIMD128: 128-bit WebAssembly SIMD instructions

    /// @see WebAssembly Release 1.0 (2019-07-20) § 5.2.2
    /// Note: The side conditions N > 7 in the productions for non-terminal bytes of the u and s encodings restrict
    /// the encoding's length. However, “trailing zeros” are still allowed within these bounds. For example, 0x03 and
    /// 0x83 0x00 are both well-formed encodings for the value 3 as a u8 . Similarly, either of 0x7e and 0xFE 0x7F and
    /// 0xFE 0xFF 0x7F are well-formed encodings of the value -2 as a s16 .
    ///
    /// The side conditions on the value N of terminal bytes further enforce that any unused bits in these bytes must be 0
    /// for positive values and 1 for negative ones. For example, 0x83 0x10 is malformed as a u8 encoding. Similarly,
    /// both 0x83 0x3E and 0xFF 0x7B are malformed as s8 encodings.

    /// @brief      Convert view to vec
    /// @param      func_counter The correct u8 size has been processed
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* change_u8_1b_view_to_vec(
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<function_section_storage_t> sec_adl,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
        ::std::byte const* const section_leb_begin,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para,
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32& func_counter,
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 const func_count) UWVM_THROWS
    {
        auto const& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<type_section_storage_t<Fs...>>(module_storage.sections)};
        auto const type_section_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(typesec.types.size())};

        UWVM_ASSERT(type_section_count < static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 7u);
        [[assume(type_section_count < static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 7u)]];

        auto& functionsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<function_section_storage_t>(module_storage.sections)};

        // Storing a typeidx takes 1 bytes, change u8view to u8_vector
        functionsec.funcs.change_mode(::uwvm2::parser::wasm::standard::wasm1::features::vectypeidx_minimize_storage_mode::u8_vector);

        // The size comparison between u32 and size_t has already been checked
        functionsec.funcs.storage.typeidx_u8_vector.reserve(static_cast<::std::size_t>(func_count));

        static_assert(sizeof(::std::byte) == sizeof(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8));

        // Copy the situation that has been processed correctly
        // The func_counter is exactly the right size
        // On platforms where CHAR_BIT is greater than 8, the func_counter that has been processed does not contain invalid high bit information at all and is
        // copied directly
        ::std::memcpy(functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr,
                      section_leb_begin,
                      func_counter * sizeof(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8));

        // Addition doesn't overflow here.
        functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr += func_counter;

        // [typeidxbef ...] typeidx1 ... typeidx2 ...
        // [     safe     ] unsafe (could be the section_end)
        //                  ^^ section_curr

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        // Used to check if correct content is recognized as an error.
        bool correct_sequence_right_taken_for_wrong{true};
#endif

        while(section_curr != section_end) [[likely]]
        {
            // Ensuring the existence of valid information

            // [ ... typeidx1] ... typeidx2 ...
            // [     safe    ] unsafe (could be the section_end)
            //       ^^ section_curr

            // check function counter
            // Ensure content is available before counting (section_curr != section_end)
            if(::uwvm2::parser::wasm::utils::counter_selfinc_throw_when_overflow(func_counter, section_curr, err) > func_count) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u32 = func_count;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // [ ... typeidx1] ... typeidx2 ...
            // [     safe    ] unsafe (could be the section_end)
            //       ^^ section_curr

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK) && (CHAR_BIT > 8)
            // Check for cases where the high position is an illegal value in platform which CHAR_BIT > 8
            if(::std::to_integer<::std::uint_least8_t>(*section_curr) & ~static_cast<::std::uint_least8_t>(0xFFu)) [[unlikely]]
            {
                correct_sequence_right_taken_for_wrong = false;
            }
#endif

            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 typeidx;  // No initialization necessary

            using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

            auto const [typeidx_next, typeidx_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                            ::fast_io::mnp::leb128_get(typeidx))};

            if(typeidx_err != ::fast_io::parse_code::ok) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_type_index;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(typeidx_err);
            }

            // [ ... typeidx1 ...] typeidx2 ...
            // [      safe       ] unsafe (could be the section_end)
            //       ^^ section_curr

            // check: type_index should less than type_section_count
            if(typeidx >= type_section_count) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u32 = typeidx;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_type_index;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // write data, func_counter has been checked and is ready to be written.
            functionsec.funcs.storage.typeidx_u8_vector.emplace_back_unchecked(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8>(typeidx));

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            if(static_cast<::std::size_t>(reinterpret_cast<::std::byte const*>(typeidx_next) - section_curr) > 1uz) [[unlikely]]
            {
                correct_sequence_right_taken_for_wrong = false;
            }
#endif

            section_curr = reinterpret_cast<::std::byte const*>(typeidx_next);

            // [ ... typeidx1 ...] typeidx2 ...
            // [      safe       ] unsafe (could be the section_end)
            //                     ^^ section_curr
        }

        // [before_section ... | func_count ... typeidx1 ... ...] (end)
        // [                       safe                         ] unsafe (could be the section_end)
        //                                                        ^^ section_curr

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(correct_sequence_right_taken_for_wrong) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif

        return section_curr;
    }

    /// @brief      Maximum typeidx in [0, 2^7)
    /// @details    Storing a typeidx takes up 1 bytes, typeidx corresponding uleb128 varies from 1 bytes, linear read/write, no simd optimization
    ///             Sequential scanning, correctly handling all cases of uleb128 u32, allowing up to 5 bytes.
    ///
    ///             According to the characteristics of wasm, wasm-ld will try to avoid redundancy 0 allowed by uleb128, so basically all cases are one byte,
    ///             this time we only need to check whether they are all 1 byte, and no more than typeidx, the total length matches, to meet these points can be
    ///             used directly to view to avoid copying, and you can simd parallel computation and writing. In rare cases, such as the user to modify the
    ///             wasm file, or wasm-ld does not use the least byte representation, encounter redundancy 0, uleb128<u32> will appear to be greater than 1 byte
    ///             in length, but meets the standard, this time it will be allocated space and the previous contents of the memcpy to the space, and then
    ///             continue to read to ensure that the subsequent access to ensure that access can be accessed through the index directly to the This will
    ///             increase the scanning time.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* scan_function_section_impl_u8_1b(
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<function_section_storage_t> sec_adl,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para,
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32& func_counter,
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 const func_count) UWVM_THROWS
    {
        auto const& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<type_section_storage_t<Fs...>>(module_storage.sections)};
        auto const type_section_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(typesec.types.size())};

        UWVM_ASSERT(type_section_count < static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 7u);
        [[assume(type_section_count < static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 7u)]];

        auto& functionsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<function_section_storage_t>(module_storage.sections)};

        // used directly to u8 view to avoid copying
        functionsec.funcs.mode = ::uwvm2::parser::wasm::standard::wasm1::features::vectypeidx_minimize_storage_mode::u8_view;

        // storage section_leb_begin
        ::std::byte const* const section_leb_begin{section_curr};

        // set view begin
        using wasm_u8_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8 const*;
        functionsec.funcs.storage.typeidx_u8_view.begin = reinterpret_cast<wasm_u8_const_may_alias_ptr>(section_curr);

#if defined(_MSC_VER) && !defined(__clang__)
        /// MSVC

# if !defined(_KERNEL_MODE) && (defined(_M_AMD64) || defined(_M_ARM64))
        /// (Little Endian), MSVC, ::fast_io::intrinsics::simd_vector
        /// x86_64-sse2, aarch64-neon

        if constexpr(::std::endian::native == ::std::endian::little)
        {
            static_assert(::std::same_as<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte, ::std::uint8_t>);

            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte const simd_vector_check{
                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(type_section_count)};

            // use fast_io simd on msvc
            using u8x16simd = ::fast_io::intrinsics::simd_vector<::std::uint8_t, 16uz>;

            u8x16simd const simd_vector_check_u8x16{
                simd_vector_check,  // 0
                simd_vector_check,  // 1
                simd_vector_check,  // 2
                simd_vector_check,  // 3
                simd_vector_check,  // 4
                simd_vector_check,  // 5
                simd_vector_check,  // 6
                simd_vector_check,  // 7
                simd_vector_check,  // 8
                simd_vector_check,  // 9
                simd_vector_check,  // 10
                simd_vector_check,  // 11
                simd_vector_check,  // 12
                simd_vector_check,  // 13
                simd_vector_check,  // 14
                simd_vector_check   // 15
            };

            while(static_cast<::std::size_t>(section_end - section_curr) >= 16uz) [[likely]]
            {
                // [before_section ... | func_count ... typeidx1 ... (15) ...] ...
                // [                        safe                             ] unsafe (could be the section_end)
                //                                      ^^ section_curr
                //                                      [   simd_vector_str  ]

                u8x16simd simd_vector_str;  // No initialization necessary

                simd_vector_str.load(section_curr);

                // It's already a little-endian.

                auto const check_upper{simd_vector_str >= simd_vector_check_u8x16};

                if(!is_all_zeros(check_upper)) [[unlikely]]
                {
                    // There are special cases greater than typeidx, which may be legal redundancy 0 or illegal data, all converted to vec to determine.
                    return change_u8_1b_view_to_vec(sec_adl,
                                                    module_storage,
                                                    section_leb_begin,
                                                    section_curr,
                                                    section_end,
                                                    err,
                                                    fs_para,
                                                    func_counter,
                                                    func_count);
                }
                else
                {
                    // all are single bytes, so there are 16
                    // There is no need to staging func_counter_this_round_end, as the special case no longer exists.
                    ::uwvm2::parser::wasm::utils::counter_addass_throw_when_overflow(func_counter,
                                                                                     static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(16u),
                                                                                     section_curr,
                                                                                     err);

                    // check counter
                    if(func_counter > func_count) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_selectable.u32 = func_count;
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    section_curr += 16uz;

                    // [before_section ... | func_count ... typeidx1 ... (15) ...] typeidx16
                    // [                        safe                             ] unsafe (could be the section_end)
                    //                                                             ^^ section_curr
                }

                // After testing, 6 are the fastest

                // View mode, have to make reservations
                ::uwvm2::utils::intrinsics::universal::prefetch<::uwvm2::utils::intrinsics::universal::pfc_mode::read,
                                                                ::uwvm2::utils::intrinsics::universal::pfc_level::L2,
                                                                ::uwvm2::utils::intrinsics::universal::ret_policy::keep>(
                    reinterpret_cast<::std::byte const*>(section_curr) + 64u * 6u);
            }
        }
# endif

#elif defined(__LITTLE_ENDIAN__) && (defined(__ARM_FEATURE_SVE) || (defined(UWVM_ENABLE_SME_SVE_STREAM_MODE) && defined(__ARM_FEATURE_SME)))
        /// (Little Endian) Variable Length
        /// mask: aarch64-sve

        // Lambda is an external function call, and error handling needs to be carried out in current function
        bool need_change_u8_1b_view_to_vec{};

        // Support for certain cpu's that don't have SVE but have SME like Apple M4
        //
        // However, for this type of cpu (e.g. Apple M4), the sme module is separated from the cpu,
        // resulting in very high latency when using it (on Apple M4, SME (SVE stream mode) 15x slower than NEON).
        //
        // You can use macro UWVM_ENABLE_SME_SVE_STREAM_MODE to enable SVE stream mode in SME in the above cpu (e.g. Apple M4).

        // SME SVE STREAM MODE: The always_inline attribute cannot be added here because it does not match the function call.
        [&]
# if (defined(UWVM_ENABLE_SME_SVE_STREAM_MODE) && defined(__ARM_FEATURE_SME)) && !defined(__ARM_FEATURE_SVE)
            __arm_locally_streaming
# else
            UWVM_ALWAYS_INLINE
# endif
            () constexpr UWVM_THROWS -> void
        {
            static_assert(::std::same_as<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte, ::std::uint8_t>);

            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte const simd_vector_check{
                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(type_section_count)};

            auto const all_one_predicate_reg{::uwvm2::utils::intrinsics::arm_sve::svptrue_b8()};
            auto const svc_sz{::uwvm2::utils::intrinsics::arm_sve::svcntb()};

# if (defined(__ARM_NEON) && (UWVM_HAS_BUILTIN(__builtin_neon_vmaxvq_u32) || UWVM_HAS_BUILTIN(__builtin_aarch64_reduc_umax_scal_v4si_uu))) &&                  \
     !((defined(UWVM_ENABLE_SME_SVE_STREAM_MODE) && defined(__ARM_FEATURE_SME)) && !defined(__ARM_FEATURE_SVE))
            // When the cpu supports the vector length of sve to be the same as the vector length of neon, since the computations are all on the basic side and
            // read 16 bytes at a time, sve needs to additionally process predicates, resulting in lower throughput, so here it switches back to using neon

            // arm neon does not mix with sme's sve stream mode

            if(svc_sz == 16u)
            {
                using i64x2simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::int64_t;
                using u64x2simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint64_t;
                using u32x4simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint32_t;
                using c8x16simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = char;
                using u8x16simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint8_t;
                using i8x16simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::int8_t;

                while(static_cast<::std::size_t>(section_end - section_curr) >= 16uz) [[likely]]
                {
                    // [before_section ... | func_count ... typeidx1 ... (15) ...] ...
                    // [                        safe                             ] unsafe (could be the section_end)
                    //                                      ^^ section_curr
                    //                                      [   simd_vector_str  ]

                    u8x16simd simd_vector_str;  // No initialization necessary

                    ::std::memcpy(::std::addressof(simd_vector_str), section_curr, sizeof(u8x16simd));

                    // It's already a little-endian.

                    auto const check_upper{simd_vector_str >= simd_vector_check};

                    if(
#  if UWVM_HAS_BUILTIN(__builtin_neon_vmaxvq_u32)                    // Only supported by clang
                        __builtin_neon_vmaxvq_u32(::std::bit_cast<u32x4simd>(check_upper))
#  elif UWVM_HAS_BUILTIN(__builtin_aarch64_reduc_umax_scal_v4si_uu)  // Only supported by GCC
                        __builtin_aarch64_reduc_umax_scal_v4si_uu(::std::bit_cast<u32x4simd>(check_upper))
#  else
#   error "missing instructions"
#  endif
                            ) [[unlikely]]
                    {
                        need_change_u8_1b_view_to_vec = true;
                        return;
                    }
                    else
                    {
                        // all are single bytes, so there are 16
                        ::uwvm2::parser::wasm::utils::counter_addass_throw_when_overflow(
                            func_counter,
                            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(16u),
                            section_curr,
                            err);

                        // check counter
                        if(func_counter > func_count) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_selectable.u32 = func_count;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        section_curr += 16uz;

                        // [before_section ... | func_count ... typeidx1 ... (15) ...] typeidx16
                        // [                        safe                             ] unsafe (could be the section_end)
                        //                                                             ^^ section_curr
                    }

#  if !((defined(UWVM_ENABLE_SME_SVE_STREAM_MODE) && defined(__ARM_FEATURE_SME)) && !defined(__ARM_FEATURE_SVE))
                    // Unable to use prefetch in sve stream mode
                    // After testing, 6 are the fastest

                    // View mode, have to make reservations
                    ::uwvm2::utils::intrinsics::universal::prefetch<::uwvm2::utils::intrinsics::universal::pfc_mode::read,
                                                                    ::uwvm2::utils::intrinsics::universal::pfc_level::L2,
                                                                    ::uwvm2::utils::intrinsics::universal::ret_policy::keep>(
                        reinterpret_cast<::std::byte const*>(section_curr) + 64u * 6u);
#  endif
                }
            }
            else
# endif
            {
                while(static_cast<::std::size_t>(section_end - section_curr) >= svc_sz) [[likely]]
                {
                    // [before_section ... | func_count ... typeidx1 ... (svc_sz - 1) ...] ...
                    // [                        safe                                     ] unsafe (could be the section_end)
                    //                                      ^^ section_curr
                    //                                      [       simd_vector_str      ]

                    using uint8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = ::std::uint8_t const*;

                    // The entire sve can be loaded
                    auto const simd_vector_str{
                        ::uwvm2::utils::intrinsics::arm_sve::svld1_u8(all_one_predicate_reg, reinterpret_cast<uint8_t_const_may_alias_ptr>(section_curr))};

                    // check simd_vector_str >= simd_vector_check
                    auto const check_upper{::uwvm2::utils::intrinsics::arm_sve::svcmpge_n_u8(all_one_predicate_reg, simd_vector_str, simd_vector_check)};

                    if(::uwvm2::utils::intrinsics::arm_sve::svptest_any(all_one_predicate_reg, check_upper)) [[unlikely]]
                    {
                        need_change_u8_1b_view_to_vec = true;
                        return;
                    }
                    else
                    {
                        // all are single bytes, so there are 64
                        // There is no need to staging func_counter_this_round_end, as the special case no longer exists.
                        ::uwvm2::parser::wasm::utils::counter_addass_throw_when_overflow(
                            func_counter,
                            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(svc_sz),
                            section_curr,
                            err);

                        // check counter
                        if(func_counter > func_count) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_selectable.u32 = func_count;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        section_curr += svc_sz;

                        // [before_section ... | func_count ... typeidx1 ... (svc_sz - 1) ...] typeidx64
                        // [                        safe                                     ] unsafe (could be the section_end)
                        //                                                                     ^^ section_curr
                    }

# if !((defined(UWVM_ENABLE_SME_SVE_STREAM_MODE) && defined(__ARM_FEATURE_SME)) && !defined(__ARM_FEATURE_SVE))

                    // After testing, 8 are the fastest

                    // View mode, have to make reservations
                    ::uwvm2::utils::intrinsics::universal::prefetch<::uwvm2::utils::intrinsics::universal::pfc_mode::read,
                                                                    ::uwvm2::utils::intrinsics::universal::pfc_level::L2,
                                                                    ::uwvm2::utils::intrinsics::universal::ret_policy::keep>(
                        reinterpret_cast<::std::byte const*>(section_curr) + 64u * 8u);
# endif
                }
            }

            // [before_section ... | func_count ... typeidx1 ... (svc_sz - 1) ...] typeidx64 ... (?) ... ] (end)
            // [                        safe                                                             ] unsafe (could be the section_end)
            //                                                                     ^^ section_curr
            //
            // Or maybe:
            //
            // [before_section ... | func_count ... typeidx1 ... (svc_sz - 1) ...] (end)
            // [                        safe                                     ] unsafe (could be the section_end)
            //                                                                     ^^ section_curr
        }
        ();

        if(need_change_u8_1b_view_to_vec) [[unlikely]]
        {
            return change_u8_1b_view_to_vec(sec_adl, module_storage, section_leb_begin, section_curr, section_end, err, fs_para, func_counter, func_count);
        }

#elif UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__) &&                                                                        \
    (defined(__AVX512BW__) && (UWVM_HAS_BUILTIN(__builtin_ia32_ucmpb512_mask)))
        /// (Little Endian)
        /// x86_64-avx512bw

        using i64x8simd [[__gnu__::__vector_size__(64)]] [[maybe_unused]] = ::std::int64_t;
        using u64x8simd [[__gnu__::__vector_size__(64)]] [[maybe_unused]] = ::std::uint64_t;
        using u32x16simd [[__gnu__::__vector_size__(64)]] [[maybe_unused]] = ::std::uint32_t;
        using c8x64simd [[__gnu__::__vector_size__(64)]] [[maybe_unused]] = char;
        using u8x64simd [[__gnu__::__vector_size__(64)]] [[maybe_unused]] = ::std::uint8_t;
        using i8x64simd [[__gnu__::__vector_size__(64)]] [[maybe_unused]] = ::std::int8_t;

        static_assert(::std::same_as<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte, ::std::uint8_t>);

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte const simd_vector_check{
            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(type_section_count)};

        u8x64simd const simd_vector_check_u8x64simd{
            simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check,  // 0 - 6
            simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check,  // 7 - 13
            simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check,  // 14 - 20
            simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check,  // 21 - 27
            simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check,  // 28 - 34
            simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check,  // 35 - 41
            simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check,  // 42 - 48
            simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check,  // 49 - 55
            simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check,  // 56 - 62
            simd_vector_check                                                                                                                     // 63
        };

        while(static_cast<::std::size_t>(section_end - section_curr) >= 64uz) [[likely]]
        {
            // [before_section ... | func_count ... typeidx1 ... (63) ...] ...
            // [                        safe                             ] unsafe (could be the section_end)
            //                                      ^^ section_curr
            //                                      [   simd_vector_str  ]

            u8x64simd simd_vector_str;  // No initialization necessary

            ::std::memcpy(::std::addressof(simd_vector_str), section_curr, sizeof(u8x64simd));

            // It's already a little-endian.

            // simd_vector_str >= simd_vector_check_u8x64simd
            ::std::uint64_t const mask{__builtin_ia32_ucmpb512_mask(simd_vector_str, simd_vector_check_u8x64simd, 0x05, UINT64_MAX)};

            if(mask) [[unlikely]]
            {
                // There are special cases greater than typeidx, which may be legal redundancy 0 or illegal data, all converted to vec to determine.
                return change_u8_1b_view_to_vec(sec_adl, module_storage, section_leb_begin, section_curr, section_end, err, fs_para, func_counter, func_count);
            }
            else
            {
                // all are single bytes, so there are 64
                // There is no need to staging func_counter_this_round_end, as the special case no longer exists.
                ::uwvm2::parser::wasm::utils::counter_addass_throw_when_overflow(func_counter,
                                                                                 static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(64u),
                                                                                 section_curr,
                                                                                 err);

                // check counter
                if(func_counter > func_count) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_selectable.u32 = func_count;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                section_curr += 64uz;

                // [before_section ... | func_count ... typeidx1 ... (63) ...] typeidx64
                // [                        safe                             ] unsafe (could be the section_end)
                //                                                             ^^ section_curr
            }

            // After testing, 8 are the fastest

            // View mode, have to make reservations
            ::uwvm2::utils::intrinsics::universal::prefetch<::uwvm2::utils::intrinsics::universal::pfc_mode::read,
                                                            ::uwvm2::utils::intrinsics::universal::pfc_level::L2,
                                                            ::uwvm2::utils::intrinsics::universal::ret_policy::keep>(
                reinterpret_cast<::std::byte const*>(section_curr) + 64u * 8u);
        }

#elif UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__) &&                                                                        \
    ((defined(__AVX__) && UWVM_HAS_BUILTIN(__builtin_ia32_ptestz256)) || (defined(__loongarch_asx) && UWVM_HAS_BUILTIN(__builtin_lasx_xbnz_v)))
        /// (Little Endian), [[gnu::vector_size]]
        /// mask: x86_64-avx, loongarch-ASX

        using i64x4simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = ::std::int64_t;
        using u64x4simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = ::std::uint64_t;
        using u32x8simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = ::std::uint32_t;
        using c8x32simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = char;
        using u8x32simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = ::std::uint8_t;
        using i8x32simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = ::std::int8_t;

        static_assert(::std::same_as<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte, ::std::uint8_t>);

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte const simd_vector_check{
            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(type_section_count)};

        while(static_cast<::std::size_t>(section_end - section_curr) >= 32uz) [[likely]]
        {
            // [before_section ... | func_count ... typeidx1 ... (31) ...] ...
            // [                        safe                             ] unsafe (could be the section_end)
            //                                      ^^ section_curr
            //                                      [   simd_vector_str  ]

            u8x32simd simd_vector_str;  // No initialization necessary

            ::std::memcpy(::std::addressof(simd_vector_str), section_curr, sizeof(u8x32simd));

            /// @note The sse4 version of scan_function_section_impl_uN_Xb is about 6% faster than avx (mundane read+judge+ptest) on Windows because the kernel
            ///       mapping in the Windows kernel's ntoskrnl.exe uses the `movups xmm` instruction for out-of-page interrupts (The reason is that the kernel
            ///       mapping in the Windows kernel ntoskrnl.exe will use the `movups xmm` instruction for out-of-page interrupts (ntoskrnl didn't do the avx
            ///       adaptation, and used the vex version), which leads to the state switching of the ymm mixed with this kind of instruction and wastes dozens
            ///       of cpu instruction cycles for the switching back and forth. linux doesn't have this problem at all, and at the same time, due to the
            ///       better algorithm of the kernel mapping, the parsing efficiency is 4 times higher than that of Windows, and most of the time is wasted in
            ///       the ntoskrnl (This can be tested with vtune). Here still use avx version, if you need sse4 version, please choose sse4 version.
            ///
            ///       (It may be misrepresented, but the fact is that the time consumption of ntoskrnl is tested in vtune to increase a lot,
            ///       while the simd processing part of the time decreases)

            // It's already a little-endian.

            auto const check_upper{simd_vector_str >= simd_vector_check};

            if(
# if defined(__AVX__) && UWVM_HAS_BUILTIN(__builtin_ia32_ptestz256)
                !__builtin_ia32_ptestz256(::std::bit_cast<i64x4simd>(check_upper), ::std::bit_cast<i64x4simd>(check_upper))
# elif defined(__loongarch_asx) && UWVM_HAS_BUILTIN(__builtin_lasx_xbnz_v)
                __builtin_lasx_xbnz_v(::std::bit_cast<u8x32simd>(check_upper))  /// @todo need check
# else
#  error "missing instructions"
# endif
                    ) [[unlikely]]
            {
                // There are special cases greater than typeidx, which may be legal redundancy 0 or illegal data, all converted to vec to determine.
                return change_u8_1b_view_to_vec(sec_adl, module_storage, section_leb_begin, section_curr, section_end, err, fs_para, func_counter, func_count);
            }
            else
            {
                // all are single bytes, so there are 32
                ::uwvm2::parser::wasm::utils::counter_addass_throw_when_overflow(func_counter,
                                                                                 static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(32u),
                                                                                 section_curr,
                                                                                 err);

                // check counter
                if(func_counter > func_count) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_selectable.u32 = func_count;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                section_curr += 32uz;

                // [before_section ... | func_count ... typeidx1 ... (31) ...] typeidx32
                // [                        safe                             ] unsafe (could be the section_end)
                //                                                             ^^ section_curr
            }

            // After testing, 8 are the fastest

            // View mode, have to make reservations
            ::uwvm2::utils::intrinsics::universal::prefetch<::uwvm2::utils::intrinsics::universal::pfc_mode::read,
                                                            ::uwvm2::utils::intrinsics::universal::pfc_level::L2,
                                                            ::uwvm2::utils::intrinsics::universal::ret_policy::keep>(
                reinterpret_cast<::std::byte const*>(section_curr) + 64u * 8u);
        }

#elif UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__) &&                                                                        \
    ((defined(__SSE2__) && UWVM_HAS_BUILTIN(__builtin_ia32_pmovmskb128)) ||                                                                                    \
     (defined(__ARM_NEON) && (UWVM_HAS_BUILTIN(__builtin_neon_vmaxvq_u32) || UWVM_HAS_BUILTIN(__builtin_aarch64_reduc_umax_scal_v4si_uu))) ||                  \
     (defined(__loongarch_sx) && UWVM_HAS_BUILTIN(__builtin_lsx_bnz_v)) ||                                                                                     \
     (defined(__wasm_simd128__) && (UWVM_HAS_BUILTIN(__builtin_wasm_bitmask_i8x16) || UWVM_HAS_BUILTIN(__builtin_wasm_all_true_i8x16))))
        /// (Little Endian), [[gnu::vector_size]]
        /// x86_64-sse2, aarch64-neon, loongarch-SX, wasm-wasmsimd128

        using i64x2simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::int64_t;
        using u64x2simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint64_t;
        using u32x4simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint32_t;
        using c8x16simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = char;
        using u8x16simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint8_t;
        using i8x16simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::int8_t;

        static_assert(::std::same_as<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte, ::std::uint8_t>);

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte const simd_vector_check{
            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(type_section_count)};

        while(static_cast<::std::size_t>(section_end - section_curr) >= 16uz) [[likely]]
        {
            // [before_section ... | func_count ... typeidx1 ... (15) ...] ...
            // [                        safe                             ] unsafe (could be the section_end)
            //                                      ^^ section_curr
            //                                      [   simd_vector_str  ]

            u8x16simd simd_vector_str;  // No initialization necessary

            ::std::memcpy(::std::addressof(simd_vector_str), section_curr, sizeof(u8x16simd));

            // It's already a little-endian.

            auto const check_upper{simd_vector_str >= simd_vector_check};

            if(
# if defined(__SSE4_1__) && UWVM_HAS_BUILTIN(__builtin_ia32_ptestz128)
                !__builtin_ia32_ptestz128(::std::bit_cast<i64x2simd>(check_upper), ::std::bit_cast<i64x2simd>(check_upper))
# elif defined(__SSE2__) && UWVM_HAS_BUILTIN(__builtin_ia32_pmovmskb128)
                __builtin_ia32_pmovmskb128(::std::bit_cast<c8x16simd>(check_upper))
# elif defined(__wasm_simd128__) && UWVM_HAS_BUILTIN(__builtin_wasm_all_true_i8x16)
                !__builtin_wasm_all_true_i8x16(::std::bit_cast<i8x16simd>(~check_upper))
# elif defined(__wasm_simd128__) && UWVM_HAS_BUILTIN(__builtin_wasm_bitmask_i8x16)
                __builtin_wasm_bitmask_i8x16(::std::bit_cast<i8x16simd>(check_upper))
# elif defined(__ARM_NEON) && UWVM_HAS_BUILTIN(__builtin_neon_vmaxvq_u32)                  // Only supported by clang
                __builtin_neon_vmaxvq_u32(::std::bit_cast<u32x4simd>(check_upper))
# elif defined(__ARM_NEON) && UWVM_HAS_BUILTIN(__builtin_aarch64_reduc_umax_scal_v4si_uu)  // Only supported by GCC
                __builtin_aarch64_reduc_umax_scal_v4si_uu(::std::bit_cast<u32x4simd>(check_upper))
# elif defined(__loongarch_sx) && UWVM_HAS_BUILTIN(__builtin_lsx_bnz_v)
                __builtin_lsx_bnz_v(::std::bit_cast<u8x16simd>(check_upper))  /// @todo need check
# else
#  error "missing instructions"
# endif
                    ) [[unlikely]]
            {
                // There are special cases greater than typeidx, which may be legal redundancy 0 or illegal data, all converted to vec to determine.
                return change_u8_1b_view_to_vec(sec_adl, module_storage, section_leb_begin, section_curr, section_end, err, fs_para, func_counter, func_count);
            }
            else
            {
                // all are single bytes, so there are 16
                ::uwvm2::parser::wasm::utils::counter_addass_throw_when_overflow(func_counter,
                                                                                 static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(16u),
                                                                                 section_curr,
                                                                                 err);

                // check counter
                if(func_counter > func_count) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_selectable.u32 = func_count;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                section_curr += 16uz;

                // [before_section ... | func_count ... typeidx1 ... (15) ...] typeidx16
                // [                        safe                             ] unsafe (could be the section_end)
                //                                                             ^^ section_curr
            }

            // After testing, 6 are the fastest

            // View mode, have to make reservations
            ::uwvm2::utils::intrinsics::universal::prefetch<::uwvm2::utils::intrinsics::universal::pfc_mode::read,
                                                            ::uwvm2::utils::intrinsics::universal::pfc_level::L2,
                                                            ::uwvm2::utils::intrinsics::universal::ret_policy::keep>(
                reinterpret_cast<::std::byte const*>(section_curr) + 64u * 6u);
        }
#endif

        // tail handling

#if defined(_MSC_VER) && !defined(__clang__)
        // MSVC, x86_64, i386, aarch64, arm, arm64ec, ...
        // msvc, avoid macro-contamination

        while(section_curr != section_end) [[likely]]
        {
            // Ensuring the existence of valid information

            // [ ... typeidx1] ... typeidx2 ...
            // [     safe    ] unsafe (could be the section_end)
            //       ^^ section_curr

            // This needs to be judged first before setting the value, to ensure that the special case call view_to_vec can get the correct function_counter
            // Need to stash func_counter_this_round_end because special cases are not yet handled
            auto const func_counter_this_round_end{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(func_counter + 1u)};

            // Avoid overflow, Direct treatment here
            if(func_counter_this_round_end < func_counter) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::counter_overflows;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // check function counter
            // Ensure content is available before counting (section_curr != section_end)
            if(func_counter_this_round_end > func_count) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u32 = func_count;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // [ ... typeidx1] ... typeidx2 ...
            // [     safe    ] unsafe (could be the section_end)
            //       ^^ section_curr

            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 typeidx;  // No initialization necessary

            using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

            auto const [typeidx_next, typeidx_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                            ::fast_io::mnp::leb128_get(typeidx))};

            if(typeidx_err != ::fast_io::parse_code::ok) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_type_index;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(typeidx_err);
            }

            // [ ... typeidx1 ...] typeidx2 ...
            // [      safe       ] unsafe (could be the section_end)
            //       ^^ section_curr

            // check: type_index should less than type_section_count
            if(typeidx >= type_section_count) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u32 = typeidx;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_type_index;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // Check for the presence of redundant 0, (check single byte)
            if(static_cast<::std::size_t>(reinterpret_cast<::std::byte const*>(typeidx_next) - section_curr) > 1uz) [[unlikely]]
            {
                return change_u8_1b_view_to_vec(sec_adl, module_storage, section_leb_begin, section_curr, section_end, err, fs_para, func_counter, func_count);
            }

            // The state of func_counter can be changed

            section_curr = reinterpret_cast<::std::byte const*>(typeidx_next);

            // [ ... typeidx1 ...] typeidx2 ...
            // [      safe       ] unsafe (could be the section_end)
            //                     ^^ section_curr

            // Processed and returned to stock. Special cases processed.
            func_counter = func_counter_this_round_end;
        }

#elif defined(__LITTLE_ENDIAN__) && (defined(__ARM_FEATURE_SVE) || (defined(UWVM_ENABLE_SME_SVE_STREAM_MODE) && defined(__ARM_FEATURE_SME)))
        // sve, sme (sve stream mode)

        [&]
# if (defined(UWVM_ENABLE_SME_SVE_STREAM_MODE) && defined(__ARM_FEATURE_SME)) && !defined(__ARM_FEATURE_SVE)
            __arm_locally_streaming
# else
            UWVM_ALWAYS_INLINE
# endif
            () constexpr UWVM_THROWS -> void
        {
            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte const simd_vector_check{
                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(type_section_count)};

            // Generate quantifiers for final tail processing
            ::uwvm2::utils::intrinsics::arm_sve::svbool_t load_predicate;  // No initialization necessary

# if SIZE_MAX > UINT32_MAX
            load_predicate = ::uwvm2::utils::intrinsics::arm_sve::svwhilelt_b8_u64(::std::bit_cast<::std::uint64_t>(section_curr),
                                                                                   ::std::bit_cast<::std::uint64_t>(section_end));
# elif SIZE_MAX > UINT16_MAX
            load_predicate = ::uwvm2::utils::intrinsics::arm_sve::svwhilelt_b8_u32(::std::bit_cast<::std::uint32_t>(section_curr),
                                                                                   ::std::bit_cast<::std::uint32_t>(section_end));
# else
#  error "missing instructions"
# endif

            using uint8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = ::std::uint8_t const*;

            // sve safely loads memory via predicates
            auto const simd_vector_str{
                ::uwvm2::utils::intrinsics::arm_sve::svld1_u8(load_predicate, reinterpret_cast<uint8_t_const_may_alias_ptr>(section_curr))};

            // check: simd_vector_str >= simd_vector_check
            auto const check_upper{::uwvm2::utils::intrinsics::arm_sve::svcmpge_n_u8(load_predicate, simd_vector_str, simd_vector_check)};

            if(::uwvm2::utils::intrinsics::arm_sve::svptest_any(load_predicate, check_upper)) [[unlikely]]
            {
                // Lambda is an external function call, and error handling needs to be carried out in current function
                need_change_u8_1b_view_to_vec = true;
                return;
            }
            else
            {
                using uint8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = ::std::uint8_t const*;

                auto const last_load_predicate_size{static_cast<::std::size_t>(reinterpret_cast<uint8_t_const_may_alias_ptr>(section_end) -
                                                                               reinterpret_cast<uint8_t_const_may_alias_ptr>(section_curr))};

                // all are single bytes, so there are 64
                // There is no need to staging func_counter_this_round_end, as the special case no longer exists.
                ::uwvm2::parser::wasm::utils::counter_addass_throw_when_overflow(
                    func_counter,
                    static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(last_load_predicate_size),
                    section_curr,
                    err);

                // check counter
                if(func_counter > func_count) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_selectable.u32 = func_count;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                section_curr += last_load_predicate_size;

                // [before_section ... | func_count ... typeidx1 ... (last_load_predicate_size - 1) ...] (end)
                // [                        safe                                                       ] unsafe (could be the section_end)
                //                                                                                       ^^ section_curr
            }
        }
        ();

        if(need_change_u8_1b_view_to_vec) [[unlikely]]
        {
            return change_u8_1b_view_to_vec(sec_adl, module_storage, section_leb_begin, section_curr, section_end, err, fs_para, func_counter, func_count);
        }

#elif UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__) &&                                                                        \
    (defined(__AVX512BW__) && (UWVM_HAS_BUILTIN(__builtin_ia32_ucmpb512_mask) && UWVM_HAS_BUILTIN(__builtin_ia32_loaddquqi512_mask)))
        // avx512bw

        {
            ::std::uint64_t load_mask{UINT64_MAX};

            using uint8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = ::std::uint8_t const*;

            auto const last_load_predicate_size{static_cast<::std::size_t>(reinterpret_cast<uint8_t_const_may_alias_ptr>(section_end) -
                                                                           reinterpret_cast<uint8_t_const_may_alias_ptr>(section_curr))};

            if(last_load_predicate_size)
            {
                // avoids ub: "u64max >> 64u"
                // naver cause ub: last_load_predicate_size > 0u && last_load_predicate_size < 64u

                load_mask >>= 64uz - last_load_predicate_size;

                using loaddquqi512_para_const_may_alias_ptr UWVM_GNU_MAY_ALIAS =
# if defined(__clang__)
                    c8x64simd const*
# else
                    char const*
# endif
                    ;

                auto const need_check{::std::bit_cast<u8x64simd>(
                    __builtin_ia32_loaddquqi512_mask(reinterpret_cast<loaddquqi512_para_const_may_alias_ptr>(section_curr), c8x64simd{}, load_mask))};

                // need_check >= simd_vector_check_u8x64simd
                ::std::uint64_t const mask{__builtin_ia32_ucmpb512_mask(need_check, simd_vector_check_u8x64simd, 0x05, load_mask)};

                if(mask) [[unlikely]]
                {
                    // There are special cases greater than typeidx, which may be legal redundancy 0 or illegal data, all converted to vec to determine.
                    return change_u8_1b_view_to_vec(sec_adl,
                                                    module_storage,
                                                    section_leb_begin,
                                                    section_curr,
                                                    section_end,
                                                    err,
                                                    fs_para,
                                                    func_counter,
                                                    func_count);
                }
                else
                {
                    // all are single bytes, so there are 'last_load_predicate_size'
                    // There is no need to staging func_counter_this_round_end, as the special case no longer exists.
                    ::uwvm2::parser::wasm::utils::counter_addass_throw_when_overflow(
                        func_counter,
                        static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(last_load_predicate_size),
                        section_curr,
                        err);

                    // check counter
                    if(func_counter > func_count) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_selectable.u32 = func_count;
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    section_curr += last_load_predicate_size;

                    // [before_section ... | func_count ... typeidx1 ... (last_load_predicate_size - 1) ...] (section_end)
                    // [                        safe                                                       ] unsafe (could be the section_end)
                    //                                                                                       ^^ section_curr
                }
            }
        }

#else
        // non-simd or simd (non-sve) tail-treatment
        // non-sve, default
        // arm-sve no tail treatment required

        while(section_curr != section_end) [[likely]]
        {
            // Ensuring the existence of valid information

            // [ ... typeidx1] ... typeidx2 ...
            // [     safe    ] unsafe (could be the section_end)
            //       ^^ section_curr

            // This needs to be judged first before setting the value, to ensure that the special case call view_to_vec can get the correct function_counter
            // Need to stash func_counter_this_round_end because special cases are not yet handled
            auto const func_counter_this_round_end{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(func_counter + 1u)};

            // Avoid overflow, Direct treatment here
            if(func_counter_this_round_end < func_counter) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::counter_overflows;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // check function counter
            // Ensure content is available before counting (section_curr != section_end)
            if(func_counter_this_round_end > func_count) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u32 = func_count;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // [ ... typeidx1] ... typeidx2 ...
            // [     safe    ] unsafe (could be the section_end)
            //       ^^ section_curr

# if CHAR_BIT > 8
            // On platforms where CHAR_BIT is greater than 8, using view requires that you also check for invalid messages in the high bits, and if so, switch
            // to vec The processing of uleb128 will completely ignore high level invalid messages, so they must be checked manually But this is legal, wasm
            // files on platforms where CHAR_BIT is greater than 8 only the eighth bit is valid information, so no error is reported
            if(::std::to_integer<::std::uint_least8_t>(*section_curr) & ~static_cast<::std::uint_least8_t>(0xFFu)) [[unlikely]]
            {
                return change_u8_1b_view_to_vec(sec_adl, module_storage, section_leb_begin, section_curr, section_end, err, fs_para, func_counter, func_count);
            }
# endif

            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 typeidx;  // No initialization necessary

            using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

            auto const [typeidx_next, typeidx_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                            ::fast_io::mnp::leb128_get(typeidx))};

            if(typeidx_err != ::fast_io::parse_code::ok) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_type_index;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(typeidx_err);
            }

            // [ ... typeidx1 ...] typeidx2 ...
            // [      safe       ] unsafe (could be the section_end)
            //       ^^ section_curr

            // check: type_index should less than type_section_count
            if(typeidx >= type_section_count) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u32 = typeidx;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_type_index;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // Check for the presence of redundant 0, (check single byte)
            if(static_cast<::std::size_t>(reinterpret_cast<::std::byte const*>(typeidx_next) - section_curr) > 1uz) [[unlikely]]
            {
                return change_u8_1b_view_to_vec(sec_adl, module_storage, section_leb_begin, section_curr, section_end, err, fs_para, func_counter, func_count);
            }

            // The state of func_counter can be changed

            section_curr = reinterpret_cast<::std::byte const*>(typeidx_next);

            // [ ... typeidx1 ...] typeidx2 ...
            // [      safe       ] unsafe (could be the section_end)
            //                     ^^ section_curr

            // Processed and returned to stock. Special cases processed.
            func_counter = func_counter_this_round_end;
        }
#endif

        // set view end
        functionsec.funcs.storage.typeidx_u8_view.end = reinterpret_cast<wasm_u8_const_may_alias_ptr>(section_curr);

        return section_curr;
    }

#if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__) && UWVM_HAS_BUILTIN(__builtin_shufflevector) &&                             \
    (((defined(__AVX2__) && UWVM_HAS_BUILTIN(__builtin_ia32_pmovmskb256)) && (defined(__SSSE3__) && UWVM_HAS_BUILTIN(__builtin_ia32_pshufb128))) ||            \
     ((defined(__loongarch_asx) && UWVM_HAS_BUILTIN(__builtin_lasx_xvmskltz_b)) && (defined(__loongarch_sx) && UWVM_HAS_BUILTIN(__builtin_lsx_vshuf_b))) ||    \
     ((defined(__SSSE3__) && UWVM_HAS_BUILTIN(__builtin_ia32_pshufb128) && UWVM_HAS_BUILTIN(__builtin_ia32_palignr128)) &&                                     \
      (defined(__SSE2__) && UWVM_HAS_BUILTIN(__builtin_ia32_pmovmskb128))) ||                                                                                  \
     (defined(__loongarch_sx) && UWVM_HAS_BUILTIN(__builtin_lsx_vshuf_b) && UWVM_HAS_BUILTIN(__builtin_lsx_vsrli_b) &&                                         \
      UWVM_HAS_BUILTIN(__builtin_lsx_vmskltz_b)))

    // shuffle simd128, need mask_u16 and shuffle

    struct simd128_shuffle_table_t
    {
        using u8x16simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint8_t;

        u8x16simd shuffle_mask{};   // shuffle mask u8x32 c-> u32x8
        unsigned processed_simd{};  // How many u32 typeidx can be output 0 == error
        unsigned processed_byte{};  // How many bytes are scanned? 0 == error
    };

    template <::std::size_t table_size, ::std::size_t max_leb_size, ::std::uint8_t mask_zero>
        requires (::std::has_single_bit(table_size) && ::std::has_single_bit(max_leb_size))  // Check to see if it is an nth power of 2
    inline constexpr ::uwvm2::utils::container::array<simd128_shuffle_table_t, table_size> generate_simd128_shuffle_table() noexcept
    {
        // Since all shuffles have a channel width of 128, only the 128 version can be done
        constexpr ::std::size_t vector_size{16uz};

        // How many bytes of source data can be read at a time
        constexpr ::std::size_t max_bytes_can_handle_per_round{vector_size / max_leb_size};

        using u8x16simd [[__gnu__::__vector_size__(16)]] = ::std::uint8_t;

        // This value represents the index that is 0 after mask, and you need to make sure that the value after adding 8 can also be masked to 0
        constexpr u8x16simd can_mask_zero{
            mask_zero,  // 0
            mask_zero,  // 1
            mask_zero,  // 2
            mask_zero,  // 3
            mask_zero,  // 4
            mask_zero,  // 5
            mask_zero,  // 6
            mask_zero,  // 7
            mask_zero,  // 8
            mask_zero,  // 9
            mask_zero,  // 10
            mask_zero,  // 11
            mask_zero,  // 12
            mask_zero,  // 13
            mask_zero,  // 14
            mask_zero   // 15
        };

        ::uwvm2::utils::container::array<simd128_shuffle_table_t, table_size> ret{};

        for(::std::size_t i{}; i != table_size; ++i)
        {
            // First set to all mask_zero for subsequent operations
            ret[i].shuffle_mask = can_mask_zero;

            // Storing part of the data for calculations
            ::std::size_t bytes_remaining{max_bytes_can_handle_per_round};
            ::std::size_t temp_i{i};

            ::std::size_t simd_counter{};

            bool error{};

            ::std::size_t j_counter{};

            while(bytes_remaining)
            {
                auto const ctro{static_cast<unsigned>(::std::countr_one(temp_i))};
                if(ctro > max_leb_size - 1u)
                {
                    // The number of bits in the pop exceeds the set number of bits
                    error = true;
                    break;
                }
                else if(ctro == bytes_remaining)
                {
                    // POP at the end makes it impossible to process
                    break;
                }

                auto const shr{ctro + 1u};

                for(unsigned j{}; j != shr; ++j)
                {
                    ret[i].shuffle_mask[simd_counter * max_leb_size + j] = static_cast<::std::uint8_t>(j_counter);
                    ++j_counter;
                }

                bytes_remaining -= shr;
                temp_i >>= shr;
                ++simd_counter;
            }

            if(error)
            {
                ret[i].shuffle_mask = can_mask_zero;
                ret[i].processed_simd = 0u;
                ret[i].processed_byte = 0u;
            }
            else
            {
                ret[i].processed_simd = simd_counter;
                ret[i].processed_byte = max_bytes_can_handle_per_round - bytes_remaining;
            }
        }

        return ret;
    }

    // Need to provide the ability to calculate after vector add 8
    inline constexpr ::std::uint8_t mask_zero{static_cast<::std::uint8_t>(0x80u)};
    inline constexpr auto simd128_shuffle_table{generate_simd128_shuffle_table<256uz, 2uz, mask_zero>()};

    /*
     *  simd128_shuffle_table: mask_zero == 80
     *
     *  0b00000000 [00 80 01 80 02 80 03 80 04 80 05 80 06 80 07 80] 8 8
     *  0b00000001 [00 01 02 80 03 80 04 80 05 80 06 80 07 80 80 80] 7 8
     *  0b00000010 [00 80 01 02 03 80 04 80 05 80 06 80 07 80 80 80] 7 8
     *  0b00000011 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00000100 [00 80 01 80 02 03 04 80 05 80 06 80 07 80 80 80] 7 8
     *  0b00000101 [00 01 02 03 04 80 05 80 06 80 07 80 80 80 80 80] 6 8
     *  0b00000110 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00000111 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00001000 [00 80 01 80 02 80 03 04 05 80 06 80 07 80 80 80] 7 8
     *  0b00001001 [00 01 02 80 03 04 05 80 06 80 07 80 80 80 80 80] 6 8
     *  0b00001010 [00 80 01 02 03 04 05 80 06 80 07 80 80 80 80 80] 6 8
     *  0b00001011 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00001100 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00001101 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00001110 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00001111 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00010000 [00 80 01 80 02 80 03 80 04 05 06 80 07 80 80 80] 7 8
     *  0b00010001 [00 01 02 80 03 80 04 05 06 80 07 80 80 80 80 80] 6 8
     *  0b00010010 [00 80 01 02 03 80 04 05 06 80 07 80 80 80 80 80] 6 8
     *  0b00010011 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00010100 [00 80 01 80 02 03 04 05 06 80 07 80 80 80 80 80] 6 8
     *  0b00010101 [00 01 02 03 04 05 06 80 07 80 80 80 80 80 80 80] 5 8
     *  0b00010110 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00010111 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00011000 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00011001 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00011010 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00011011 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00011100 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00011101 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00011110 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00011111 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00100000 [00 80 01 80 02 80 03 80 04 80 05 06 07 80 80 80] 7 8
     *  0b00100001 [00 01 02 80 03 80 04 80 05 06 07 80 80 80 80 80] 6 8
     *  0b00100010 [00 80 01 02 03 80 04 80 05 06 07 80 80 80 80 80] 6 8
     *  0b00100011 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00100100 [00 80 01 80 02 03 04 80 05 06 07 80 80 80 80 80] 6 8
     *  0b00100101 [00 01 02 03 04 80 05 06 07 80 80 80 80 80 80 80] 5 8
     *  0b00100110 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00100111 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00101000 [00 80 01 80 02 80 03 04 05 06 07 80 80 80 80 80] 6 8
     *  0b00101001 [00 01 02 80 03 04 05 06 07 80 80 80 80 80 80 80] 5 8
     *  0b00101010 [00 80 01 02 03 04 05 06 07 80 80 80 80 80 80 80] 5 8
     *  0b00101011 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00101100 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00101101 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00101110 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00101111 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00110000 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00110001 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00110010 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00110011 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00110100 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00110101 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00110110 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00110111 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00111000 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00111001 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00111010 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00111011 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00111100 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00111101 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00111110 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b00111111 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01000000 [00 80 01 80 02 80 03 80 04 80 05 80 06 07 80 80] 7 8
     *  0b01000001 [00 01 02 80 03 80 04 80 05 80 06 07 80 80 80 80] 6 8
     *  0b01000010 [00 80 01 02 03 80 04 80 05 80 06 07 80 80 80 80] 6 8
     *  0b01000011 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01000100 [00 80 01 80 02 03 04 80 05 80 06 07 80 80 80 80] 6 8
     *  0b01000101 [00 01 02 03 04 80 05 80 06 07 80 80 80 80 80 80] 5 8
     *  0b01000110 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01000111 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01001000 [00 80 01 80 02 80 03 04 05 80 06 07 80 80 80 80] 6 8
     *  0b01001001 [00 01 02 80 03 04 05 80 06 07 80 80 80 80 80 80] 5 8
     *  0b01001010 [00 80 01 02 03 04 05 80 06 07 80 80 80 80 80 80] 5 8
     *  0b01001011 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01001100 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01001101 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01001110 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01001111 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01010000 [00 80 01 80 02 80 03 80 04 05 06 07 80 80 80 80] 6 8
     *  0b01010001 [00 01 02 80 03 80 04 05 06 07 80 80 80 80 80 80] 5 8
     *  0b01010010 [00 80 01 02 03 80 04 05 06 07 80 80 80 80 80 80] 5 8
     *  0b01010011 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01010100 [00 80 01 80 02 03 04 05 06 07 80 80 80 80 80 80] 5 8
     *  0b01010101 [00 01 02 03 04 05 06 07 80 80 80 80 80 80 80 80] 4 8
     *  0b01010110 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01010111 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01011000 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01011001 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01011010 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01011011 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01011100 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01011101 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01011110 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01011111 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01100000 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01100001 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01100010 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01100011 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01100100 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01100101 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01100110 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01100111 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01101000 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01101001 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01101010 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01101011 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01101100 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01101101 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01101110 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01101111 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01110000 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01110001 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01110010 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01110011 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01110100 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01110101 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01110110 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01110111 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01111000 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01111001 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01111010 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01111011 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01111100 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01111101 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01111110 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b01111111 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10000000 [00 80 01 80 02 80 03 80 04 80 05 80 06 80 80 80] 7 7
     *  0b10000001 [00 01 02 80 03 80 04 80 05 80 06 80 80 80 80 80] 6 7
     *  0b10000010 [00 80 01 02 03 80 04 80 05 80 06 80 80 80 80 80] 6 7
     *  0b10000011 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10000100 [00 80 01 80 02 03 04 80 05 80 06 80 80 80 80 80] 6 7
     *  0b10000101 [00 01 02 03 04 80 05 80 06 80 80 80 80 80 80 80] 5 7
     *  0b10000110 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10000111 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10001000 [00 80 01 80 02 80 03 04 05 80 06 80 80 80 80 80] 6 7
     *  0b10001001 [00 01 02 80 03 04 05 80 06 80 80 80 80 80 80 80] 5 7
     *  0b10001010 [00 80 01 02 03 04 05 80 06 80 80 80 80 80 80 80] 5 7
     *  0b10001011 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10001100 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10001101 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10001110 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10001111 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10010000 [00 80 01 80 02 80 03 80 04 05 06 80 80 80 80 80] 6 7
     *  0b10010001 [00 01 02 80 03 80 04 05 06 80 80 80 80 80 80 80] 5 7
     *  0b10010010 [00 80 01 02 03 80 04 05 06 80 80 80 80 80 80 80] 5 7
     *  0b10010011 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10010100 [00 80 01 80 02 03 04 05 06 80 80 80 80 80 80 80] 5 7
     *  0b10010101 [00 01 02 03 04 05 06 80 80 80 80 80 80 80 80 80] 4 7
     *  0b10010110 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10010111 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10011000 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10011001 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10011010 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10011011 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10011100 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10011101 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10011110 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10011111 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10100000 [00 80 01 80 02 80 03 80 04 80 05 06 80 80 80 80] 6 7
     *  0b10100001 [00 01 02 80 03 80 04 80 05 06 80 80 80 80 80 80] 5 7
     *  0b10100010 [00 80 01 02 03 80 04 80 05 06 80 80 80 80 80 80] 5 7
     *  0b10100011 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10100100 [00 80 01 80 02 03 04 80 05 06 80 80 80 80 80 80] 5 7
     *  0b10100101 [00 01 02 03 04 80 05 06 80 80 80 80 80 80 80 80] 4 7
     *  0b10100110 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10100111 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10101000 [00 80 01 80 02 80 03 04 05 06 80 80 80 80 80 80] 5 7
     *  0b10101001 [00 01 02 80 03 04 05 06 80 80 80 80 80 80 80 80] 4 7
     *  0b10101010 [00 80 01 02 03 04 05 06 80 80 80 80 80 80 80 80] 4 7
     *  0b10101011 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10101100 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10101101 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10101110 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10101111 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10110000 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10110001 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10110010 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10110011 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10110100 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10110101 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10110110 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10110111 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10111000 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10111001 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10111010 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10111011 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10111100 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10111101 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10111110 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b10111111 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11000000 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11000001 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11000010 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11000011 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11000100 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11000101 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11000110 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11000111 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11001000 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11001001 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11001010 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11001011 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11001100 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11001101 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11001110 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11001111 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11010000 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11010001 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11010010 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11010011 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11010100 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11010101 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11010110 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11010111 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11011000 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11011001 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11011010 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11011011 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11011100 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11011101 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11011110 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11011111 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11100000 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11100001 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11100010 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11100011 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11100100 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11100101 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11100110 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11100111 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11101000 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11101001 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11101010 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11101011 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11101100 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11101101 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11101110 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11101111 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11110000 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11110001 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11110010 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11110011 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11110100 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11110101 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11110110 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11110111 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11111000 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11111001 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11111010 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11111011 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11111100 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11111101 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11111110 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     *  0b11111111 [80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80] 0 0
     */

#endif

    /// @brief      Maximum typeidx in [2^7, 2^8)
    /// @details    Storing a typeidx takes up 1 bytes, typeidx corresponding uleb128 varies from 1-2 bytes, linear read/write, no simd optimization
    ///             Sequential scanning, correctly handling all cases of uleb128 u32, allowing up to 5 bytes.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* scan_function_section_impl_u8_2b(
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<function_section_storage_t> sec_adl,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para,
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32& func_counter,
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 const func_count) UWVM_THROWS
    {
        auto const& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<type_section_storage_t<Fs...>>(module_storage.sections)};
        auto const type_section_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(typesec.types.size())};

        UWVM_ASSERT(type_section_count >= static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 7u &&
                    type_section_count < static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 8u);
        [[assume(type_section_count >= static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 7u &&
                 type_section_count < static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 8u)]];

        auto& functionsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<function_section_storage_t>(module_storage.sections)};

        // Storing a typeidx takes up 1 bytes, use u8_vector
        functionsec.funcs.mode = ::uwvm2::parser::wasm::standard::wasm1::features::vectypeidx_minimize_storage_mode::u8_vector;

        // The size comparison between u32 and size_t has already been checked
        functionsec.funcs.storage.typeidx_u8_vector.reserve(static_cast<::std::size_t>(func_count));

        // [before_section ... | func_count ...] typeidx1 ... typeidx2 ...
        // [              safe                 ] unsafe (could be the section_end)
        //                                       ^^ section_curr

        /// @todo support SVE2: svtbl

#if defined(_MSC_VER) && !defined(__clang__)
        /// MSVC

# if !defined(_KERNEL_MODE) && (defined(_M_AMD64) || defined(_M_ARM64))
        /// (Little Endian), MSVC, ::fast_io::intrinsics::simd_vector
        /// x86_64-sse2, aarch64-neon

        if constexpr(::std::endian::native == ::std::endian::little)
        {
            static_assert(::std::same_as<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte, ::std::uint8_t>);

            constexpr auto simd_vector_check{static_cast<::std::uint8_t>(127u)};

            // use fast_io simd on msvc
            using u8x16simd = ::fast_io::intrinsics::simd_vector<::std::uint8_t, 16uz>;

            constexpr u8x16simd simd_vector_check_u8x16{
                simd_vector_check,  // 0
                simd_vector_check,  // 1
                simd_vector_check,  // 2
                simd_vector_check,  // 3
                simd_vector_check,  // 4
                simd_vector_check,  // 5
                simd_vector_check,  // 6
                simd_vector_check,  // 7
                simd_vector_check,  // 8
                simd_vector_check,  // 9
                simd_vector_check,  // 10
                simd_vector_check,  // 11
                simd_vector_check,  // 12
                simd_vector_check,  // 13
                simd_vector_check,  // 14
                simd_vector_check   // 15
            };

            while(static_cast<::std::size_t>(section_end - section_curr) >= 16uz) [[likely]]
            {
                // [before_section ... | func_count ... typeidx1 ... (15) ...] ...
                // [                        safe                             ] unsafe (could be the section_end)
                //                                      ^^ section_curr
                //                                      [   simd_vector_str  ]

                u8x16simd simd_vector_str;  // No initialization necessary

                simd_vector_str.load(section_curr);

                // It's already a little-endian.

                auto const check_highest_bit{simd_vector_str > simd_vector_check_u8x16};

                if(!is_all_zeros(check_highest_bit))
                {
                    auto const section_curr_end{section_curr + 16u};

                    // Since parse_by_scan uses section_end, it is possible that section_curr will be greater than section_curr_end, use '<' instead of '!='

                    while(section_curr < section_curr_end)
                    {
                        // The outer boundary is unknown and needs to be rechecked
                        // [ ... typeidx1] ... (outer) ] typeidx2 ...
                        // [     safe    ] ... (outer) ] unsafe (could be the section_end)
                        //       ^^ section_curr

                        if(::uwvm2::parser::wasm::utils::counter_selfinc_throw_when_overflow(func_counter, section_curr, err) > func_count) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_selectable.u32 = func_count;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 typeidx;  // No initialization necessary

                        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                        auto const [typeidx_next, typeidx_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                                        reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                                        ::fast_io::mnp::leb128_get(typeidx))};

                        if(typeidx_err != ::fast_io::parse_code::ok) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_type_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(typeidx_err);
                        }

                        // The outer boundary is unknown and needs to be rechecked
                        // [ ... typeidx1 ...] ... (outer) ] typeidx2 ...
                        // [      safe       ] ... (outer) ] unsafe (could be the section_end)
                        //       ^^ section_curr

                        // There's a good chance there's an error here.
                        // Or there is a leb redundancy 0
                        if(typeidx >= type_section_count) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_selectable.u32 = typeidx;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_type_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        // write data, func_counter has been checked and is ready to be written.
                        functionsec.funcs.storage.typeidx_u8_vector.emplace_back_unchecked(
                            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8>(typeidx));

                        section_curr = reinterpret_cast<::std::byte const*>(typeidx_next);

                        // The outer boundary is unknown and needs to be rechecked
                        // [ ... typeidx1 ...] typeidx2 ...] ...
                        // [      safe       ] ... (outer) ] unsafe (could be the section_end)
                        //                     ^^ section_curr
                    }

                    // [before_section ... | func_count ... typeidx1 ... (15) ... ...  ] typeidxN
                    // [                        safe                                   ] unsafe (could be the section_end)
                    //                                                                   ^^ section_curr
                    //                                      [   simd_vector_str  ] ... ] (Depends on the size of section_curr in relation to section_curr_end)
                }
                else
                {
                    // all are single bytes, so there are 16
                    ::uwvm2::parser::wasm::utils::counter_addass_throw_when_overflow(func_counter,
                                                                                     static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(16u),
                                                                                     section_curr,
                                                                                     err);

                    // check counter
                    if(func_counter > func_count) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_selectable.u32 = func_count;
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // Since everything is less than 128, there is no need to check the typeidx.

                    // write data
                    simd_vector_str.store(functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr);

                    functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr += 16uz;

                    section_curr += 16uz;

                    // [before_section ... | func_count ... typeidx1 ... (15) ...] typeidx16
                    // [                        safe                             ] unsafe (could be the section_end)
                    //                                                             ^^ section_curr
                }

                // After testing, 4 and 3 are the fastest

                ::uwvm2::utils::intrinsics::universal::prefetch<::uwvm2::utils::intrinsics::universal::pfc_mode::read,
                                                                ::uwvm2::utils::intrinsics::universal::pfc_level::nta,
                                                                ::uwvm2::utils::intrinsics::universal::ret_policy::strm>(
                    reinterpret_cast<::std::byte const*>(section_curr) + 64u * 4u);
                ::uwvm2::utils::intrinsics::universal::prefetch<::uwvm2::utils::intrinsics::universal::pfc_mode::write,
                                                                ::uwvm2::utils::intrinsics::universal::pfc_level::L2,
                                                                ::uwvm2::utils::intrinsics::universal::ret_policy::keep>(
                    reinterpret_cast<::std::byte const*>(functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr) + 64u * 3u);
            }
        }
# endif

#elif UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__) && UWVM_HAS_BUILTIN(__builtin_shufflevector) &&                           \
    (defined(__AVX512VBMI__) && defined(__AVX512VBMI2__) && defined(__AVX512BW__) && defined(__AVX512F__))
        /// (Little Endian), [[gnu::vector_size]], has mask-u16, can shuffle, simd512
        /// x86_64-avx512vbmi2

        auto error_handler{[&](::std::size_t n) constexpr UWVM_THROWS -> void
                           {
                               // Need to ensure that section_curr to section_curr + n is memory safe
                               // Processing uses section_curr_end to guarantee entry into the loop for inspection.
                               // Also use section_end to ensure that the scanning of the last leb128 is handled correctly, both memory safe.

                               auto const section_curr_end{section_curr + n};

            // [ ... typeidx1 ... ] (section_curr_end) ... (outer) ]
            // [     safe     ... ]                    ... (outer) ] unsafe (could be the section_end)
            //       ^^ section_curr

            // Since parse_by_scan uses section_end, it is possible that section_curr will be greater than section_curr_end, use '<' instead
            // of '!='

# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                               // Used to check if correct content is recognized as an error.
                               bool correct_sequence_right_taken_for_wrong{true};
# endif

                               while(section_curr < section_curr_end)
                               {
                                   // The outer boundary is unknown and needs to be rechecked
                                   // [ ... typeidx1] ... (outer) ] typeidx2 ...
                                   // [     safe    ] ... (outer) ] unsafe (could be the section_end)
                                   //       ^^ section_curr

                                   if(::uwvm2::parser::wasm::utils::counter_selfinc_throw_when_overflow(func_counter, section_curr, err) > func_count)
                                       [[unlikely]]
                                   {
                                       err.err_curr = section_curr;
                                       err.err_selectable.u32 = func_count;
                                       err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                                       ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                   }

                                   ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 typeidx;  // No initialization necessary

                                   using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                                   auto const [typeidx_next, typeidx_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                                                   reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                                                   ::fast_io::mnp::leb128_get(typeidx))};

                                   if(typeidx_err != ::fast_io::parse_code::ok) [[unlikely]]
                                   {
                                       err.err_curr = section_curr;
                                       err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_type_index;
                                       ::uwvm2::parser::wasm::base::throw_wasm_parse_code(typeidx_err);
                                   }

                                   // The outer boundary is unknown and needs to be rechecked
                                   // [ ... typeidx1 ...] ... (outer) ] typeidx2 ...
                                   // [      safe       ] ... (outer) ] unsafe (could be the section_end)
                                   //       ^^ section_curr

                                   // There's a good chance there's an error here.
                                   // Or there is a leb redundancy 0
                                   if(typeidx >= type_section_count) [[unlikely]]
                                   {
                                       err.err_curr = section_curr;
                                       err.err_selectable.u32 = typeidx;
                                       err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_type_index;
                                       ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                   }

                                   // write data, func_counter has been checked and is ready to be written.
                                   functionsec.funcs.storage.typeidx_u8_vector.emplace_back_unchecked(
                                       static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8>(typeidx));

# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                   if(static_cast<::std::size_t>(reinterpret_cast<::std::byte const*>(typeidx_next) - section_curr) > 2uz) [[unlikely]]
                                   {
                                       correct_sequence_right_taken_for_wrong = false;
                                   }
# endif

                                   section_curr = reinterpret_cast<::std::byte const*>(typeidx_next);

                                   // The outer boundary is unknown and needs to be rechecked
                                   // [ ... typeidx1 ...] typeidx2 ...] ...
                                   // [      safe       ] ... (outer) ] unsafe (could be the section_end)
                                   //                     ^^ section_curr
                               }

            // [before_section ... | func_count ... typeidx1 ... (n - 1) ... ...  ] typeidxN
            // [                        safe                                      ] unsafe (could be the section_end)
            //                                                                      ^^ section_curr
            //                                      [   simd_vector_str  ]  ...   ] (Depends on the size of section_curr in relation to
            //                                      section_curr_end)

# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                               if(correct_sequence_right_taken_for_wrong) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif
                           }};

        using i64x8simd [[__gnu__::__vector_size__(64)]] [[maybe_unused]] = ::std::int64_t;
        using u64x8simd [[__gnu__::__vector_size__(64)]] [[maybe_unused]] = ::std::uint64_t;
        using u32x15simd [[__gnu__::__vector_size__(64)]] [[maybe_unused]] = ::std::uint32_t;
        using u16x32simd [[__gnu__::__vector_size__(64)]] [[maybe_unused]] = ::std::uint16_t;
        using c8x64simd [[__gnu__::__vector_size__(64)]] [[maybe_unused]] = char;
        using u8x64simd [[__gnu__::__vector_size__(64)]] [[maybe_unused]] = ::std::uint8_t;
        using i8x64simd [[__gnu__::__vector_size__(64)]] [[maybe_unused]] = ::std::int8_t;

        using i64x4simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = ::std::int64_t;
        using u64x4simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = ::std::uint64_t;
        using u32x8simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = ::std::uint32_t;
        using u16x16simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = ::std::uint16_t;
        using c8x32simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = char;
        using u8x32simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = ::std::uint8_t;
        using i8x32simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = ::std::int8_t;

        using i64x2simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::int64_t;
        using u64x2simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint64_t;
        using u32x4simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint32_t;
        using u16x8simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint16_t;
        using c8x16simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = char;
        using u8x16simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint8_t;
        using i8x16simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::int8_t;

        static_assert(::std::same_as<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte, ::std::uint8_t>);

        while(static_cast<::std::size_t>(section_end - section_curr) >= 64uz) [[likely]]
        {

            // [before_section ... | func_count ... typeidx1 ... (63) ...] ...
            // [                        safe                             ] unsafe (could be the section_end)
            //                                      ^^ section_curr
            //                                      [   simd_vector_str  ]

            u8x64simd simd_vector_str;  // No initialization necessary

            ::std::memcpy(::std::addressof(simd_vector_str), section_curr, sizeof(u8x64simd));

            // It's already a little-endian.

            // When the highest bit of each byte is pop, then mask out 1

            ::std::uint64_t const check_mask{
# if defined(__AVX512BW__) && UWVM_HAS_BUILTIN(__builtin_ia32_cvtb2mask512)
                static_cast<::std::uint64_t>(__builtin_ia32_cvtb2mask512(::std::bit_cast<c8x64simd>(simd_vector_str)))
# else
#  error "missing instructions"
# endif
            };

            if(
# if UWVM_HAS_BUILTIN(__builtin_expect_with_probability)
                __builtin_expect_with_probability(static_cast<bool>(check_mask), true, 1.0 / 4.0)
# else
                check_mask
# endif
            )
            {
                auto const func_counter_end{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(func_counter + 64u)};

                if(func_counter_end < func_counter) [[unlikely]]
                {
                    // It's overflowing. It's jumping right out.
                    break;
                }

                if(func_counter_end > func_count) [[unlikely]]
                {
                    // Near the end, jump directly out of the simd section and hand it over to the tail.
                    break;
                }

                // Check for leb over 2 bytes

                // check_mask is a mask that checks the highest bit (eighth bit) of each byte. If there is a 2-byte uleb there will be a 1 and a 0. When there
                // are two consecutive 1's it means that it must represent unprocessable by the u16. Here a single 1 in the highest bit of the check_mask cannot
                // tell if there are more one's to follow, and is handed over to the next round of processing by the subsequent first_round_processes_31bit.

                if(check_mask & (check_mask << 1u)) [[unlikely]]
                {
                    error_handler(64u);
                    continue;
                }

                ::std::uint16_t const simd_vector_check{static_cast<::std::uint16_t>(type_section_count)};

                ::std::uint64_t check_mask_curr{check_mask};

                ::std::uint32_t const check_mask_curr_1st{static_cast<::std::uint32_t>(check_mask_curr)};

                // Write 32 directly when the current 32 are all zeros
                if(!check_mask_curr_1st)
                {
                    check_mask_curr >>= 32u;

                    auto const need_write_u8x32x2{::std::bit_cast<::uwvm2::utils::container::array<u8x32simd, 2uz>>(simd_vector_str)};

                    auto const need_write_u8x32x2v0{need_write_u8x32x2.front_unchecked()};

                    // Checked. No overflow.
                    func_counter += 32u;

                    ::std::memcpy(functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr, ::std::addressof(need_write_u8x32x2v0), sizeof(u8x32simd));

                    functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr += 32u;

                    section_curr += 32u;

                    // Convert the acquired 32 bits for next use.

                    simd_vector_str = __builtin_shufflevector(simd_vector_str,
                                                              simd_vector_str,
                                                              32,  // 0
                                                              33,  // 1
                                                              34,  // 2
                                                              35,  // 3
                                                              36,  // 4
                                                              37,  // 5
                                                              38,  // 6
                                                              39,  // 7
                                                              40,  // 8
                                                              41,  // 9
                                                              42,  // 10
                                                              43,  // 11
                                                              44,  // 12
                                                              45,  // 13
                                                              46,  // 14
                                                              47,  // 15
                                                              48,  // 16
                                                              49,  // 17
                                                              50,  // 18
                                                              51,  // 19
                                                              52,  // 20
                                                              53,  // 21
                                                              54,  // 22
                                                              55,  // 23
                                                              56,  // 24
                                                              57,  // 25
                                                              58,  // 26
                                                              59,  // 27
                                                              60,  // 28
                                                              61,  // 29
                                                              62,  // 30
                                                              63,  // 31
                                                              -1,  // 32
                                                              -1,  // 33
                                                              -1,  // 34
                                                              -1,  // 35
                                                              -1,  // 36
                                                              -1,  // 37
                                                              -1,  // 38
                                                              -1,  // 39
                                                              -1,  // 40
                                                              -1,  // 41
                                                              -1,  // 42
                                                              -1,  // 43
                                                              -1,  // 44
                                                              -1,  // 45
                                                              -1,  // 46
                                                              -1,  // 47
                                                              -1,  // 48
                                                              -1,  // 49
                                                              -1,  // 50
                                                              -1,  // 51
                                                              -1,  // 52
                                                              -1,  // 53
                                                              -1,  // 54
                                                              -1,  // 55
                                                              -1,  // 56
                                                              -1,  // 57
                                                              -1,  // 58
                                                              -1,  // 59
                                                              -1,  // 60
                                                              -1,  // 61
                                                              -1,  // 62
                                                              -1   // 63
                    );
                }

                // 2nd

                {
                    ::std::uint32_t const check_mask_curr_2nd{static_cast<::std::uint32_t>(check_mask_curr)};

                    bool const first_round_processes_31bit{static_cast<bool>(check_mask_curr_2nd & static_cast<::std::uint32_t>(0x8000'0000u))};
                    unsigned const first_round{32u - static_cast<unsigned>(first_round_processes_31bit)};
                    unsigned const handled_simd{32u - static_cast<unsigned>(::std::popcount(check_mask_curr_2nd))};

                    constexpr auto mask{static_cast<::std::uint64_t>(0x5555'5555'5555'5555u)};

                    constexpr u16x32simd conversion_table_1{
                        0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u,  // 0 - 10
                        0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u,  // 11 - 21
                        0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u            // 22 - 31
                    };

                    ::std::uint32_t check_mask_curr_2nd_curtailment{};

                    // Avoiding the effects of the highest invalid bit

                    for(::std::uint32_t check_mask_curr_2nd_tmp{check_mask_curr_2nd & static_cast<::std::uint32_t>(0x7FFF'FFFFu)}; check_mask_curr_2nd_tmp;)
                    {
                        auto const crtz{static_cast<unsigned>(::std::countr_zero(check_mask_curr_2nd_tmp))};

                        // check_mask_curr_2nd_tmp != 0, check_mask_curr_2nd_tmp & 0x8000'0000 == 0
                        // check_mask_curr_2nd_tmp max == 0b01..., crtz max == 30u
                        // crtz < 31u, crtz + 1u < 32u, never cause ub

                        auto const sizeFF{crtz + 1u};

                        UWVM_ASSERT(sizeFF >= 1u && sizeFF < 32u);
                        [[assume(sizeFF >= 1u && sizeFF < 32u)]];

                        auto const FF{(static_cast<::std::uint32_t>(1u) << sizeFF) - 1u};
                        check_mask_curr_2nd_curtailment |= check_mask_curr_2nd_tmp & FF;

                        check_mask_curr_2nd_tmp &= ~FF;
                        check_mask_curr_2nd_tmp >>= 1u;
                    }

                    // check_mask_curr_2nd_curtailment : ..., 0b,     1b,     0b,     0b,     0b,     1b,     0b
                    // convert to:
                    // conversion_res_2:                 ..., 0b, 0b, 1b, 0b, 0b, 0b, 0b, 0b, 0b, 0b, 1b, 0b, 0b, 0b

                    u16x32simd conversion_res_1;

# if defined(__AVX512BW__) && UWVM_HAS_BUILTIN(__builtin_ia32_selectw_512)         // Clang
                    conversion_res_1 = __builtin_ia32_selectw_512(check_mask_curr_2nd_curtailment, conversion_table_1, u16x32simd{});
# elif defined(__AVX512BW__) && UWVM_HAS_BUILTIN(__builtin_ia32_movdquhi512_mask)  // GCC
                    conversion_res_1 = __builtin_ia32_movdquhi512_mask(conversion_table_1, u16x32simd{}, check_mask_curr_2nd_curtailment);
# else
#  error "missing instructions"
# endif

                    ::std::uint64_t conversion_res_2;
# if defined(__AVX512BW__) && UWVM_HAS_BUILTIN(__builtin_ia32_cvtb2mask512)
                    conversion_res_2 = static_cast<::std::uint64_t>(__builtin_ia32_cvtb2mask512(::std::bit_cast<c8x64simd>(conversion_res_1)));
# else
#  error "missing instructions"
# endif

                    conversion_res_2 |= mask;

                    // check_mask_curr_2nd_curtailment : ..., 0b,     1b,     0b,     0b,     0b,     1b,     0b
                    // convert to:
                    // conversion_res_2:                 ..., 0b, 0b, 1b, 0b, 0b, 0b, 0b, 0b, 0b, 0b, 1b, 0b, 0b, 0b

                    constexpr u8x64simd conversion_table_2{
                        0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,  // 0 - 21
                        22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43,  // 22 - 43
                        44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63           // 44 - 63
                    };

                    u8x64simd mask_table;

# if defined(__AVX512VBMI2__) && UWVM_HAS_BUILTIN(__builtin_ia32_expandqi512_mask)
                    mask_table = __builtin_ia32_expandqi512_mask(conversion_table_2, u8x64simd{}, conversion_res_2);
# else
#  error "missing instructions"
# endif

                    u16x32simd mask_res;

# if defined(__AVX512VBMI__) && UWVM_HAS_BUILTIN(__builtin_ia32_permvarqi512_mask)                                               // GCC
                    mask_res = ::std::bit_cast<u16x32simd>(__builtin_ia32_permvarqi512_mask(::std::bit_cast<c8x64simd>(simd_vector_str),
                                                                                            ::std::bit_cast<c8x64simd>(mask_table),
                                                                                            c8x64simd{},
                                                                                            conversion_res_2));
# elif defined(__AVX512VBMI__) && UWVM_HAS_BUILTIN(__builtin_ia32_permvarqi512) && UWVM_HAS_BUILTIN(__builtin_ia32_selectb_512)  // clang
                    mask_res = ::std::bit_cast<u16x32simd>(__builtin_ia32_selectb_512(
                        conversion_res_2,
                        __builtin_ia32_permvarqi512(::std::bit_cast<c8x64simd>(simd_vector_str), ::std::bit_cast<c8x64simd>(mask_table)),
                        c8x64simd{}));
# else
#  error "missing instructions"
# endif
                    auto const res{(mask_res & static_cast<::std::uint16_t>(0x7Fu)) | ((mask_res & static_cast<::std::uint16_t>(0x7F00u)) >> 1u)};

                    // The data out of shuffle is 16-bit [0, 2^14) and may be greater than or equal to typeidx, which needs to be checked.

                    ::std::uint32_t const gen_mask{(static_cast<::std::uint32_t>(1u) << handled_simd) - 1u};

                    u16x32simd const simd_vector_check_u16x32{
                        simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check,
                        simd_vector_check, simd_vector_check, simd_vector_check,  // 0 - 6
                        simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check,
                        simd_vector_check, simd_vector_check, simd_vector_check,  // 7 - 13
                        simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check,
                        simd_vector_check, simd_vector_check, simd_vector_check,  // 14 - 20
                        simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check,
                        simd_vector_check, simd_vector_check, simd_vector_check,                    // 21 - 27
                        simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check  // 28 - 31
                    };

                    if(
# if defined(__AVX512BW__) && UWVM_HAS_BUILTIN(__builtin_ia32_cmpw512_mask)
                        __builtin_ia32_cmpw512_mask(res, simd_vector_check_u16x32, 0x05, gen_mask)
# else
#  error "missing instructions"
# endif
                            ) [[unlikely]]
                    {
                        // [before_section ... | func_count ... (32, 0) ... typeidx1 ... (63 - (32, 0)) ...] ...
                        // [                        safe                                                   ] unsafe (could be the section_end)
                        //                                                  ^^ section_curr
                        //                                                                              ^^^^^^^ section_curr + 32uz

                        // it can be processed up to 32
                        error_handler(32uz);

                        // Start the next round straight away
                        continue;
                    }

                    auto const need_write{__builtin_shufflevector(::std::bit_cast<u8x64simd>(res),
                                                                  ::std::bit_cast<u8x64simd>(res),
                                                                  0,   // 0
                                                                  2,   // 1
                                                                  4,   // 2
                                                                  6,   // 3
                                                                  8,   // 4
                                                                  10,  // 5
                                                                  12,  // 6
                                                                  14,  // 7
                                                                  16,  // 8
                                                                  18,  // 9
                                                                  20,  // 10
                                                                  22,  // 11
                                                                  24,  // 12
                                                                  26,  // 13
                                                                  28,  // 14
                                                                  30,  // 15
                                                                  32,  // 16
                                                                  34,  // 17
                                                                  36,  // 18
                                                                  38,  // 19
                                                                  40,  // 20
                                                                  42,  // 21
                                                                  44,  // 22
                                                                  46,  // 23
                                                                  48,  // 24
                                                                  50,  // 25
                                                                  52,  // 26
                                                                  54,  // 27
                                                                  56,  // 28
                                                                  58,  // 29
                                                                  60,  // 30
                                                                  62,  // 31
                                                                  -1,  // 32
                                                                  -1,  // 33
                                                                  -1,  // 34
                                                                  -1,  // 35
                                                                  -1,  // 36
                                                                  -1,  // 37
                                                                  -1,  // 38
                                                                  -1,  // 39
                                                                  -1,  // 40
                                                                  -1,  // 41
                                                                  -1,  // 42
                                                                  -1,  // 43
                                                                  -1,  // 44
                                                                  -1,  // 45
                                                                  -1,  // 46
                                                                  -1,  // 47
                                                                  -1,  // 48
                                                                  -1,  // 49
                                                                  -1,  // 50
                                                                  -1,  // 51
                                                                  -1,  // 52
                                                                  -1,  // 53
                                                                  -1,  // 54
                                                                  -1,  // 55
                                                                  -1,  // 56
                                                                  -1,  // 57
                                                                  -1,  // 58
                                                                  -1,  // 59
                                                                  -1,  // 60
                                                                  -1,  // 61
                                                                  -1,  // 62
                                                                  -1   // 63
                                                                  )};

                    auto const need_write_u8x32x2{::std::bit_cast<::uwvm2::utils::container::array<u8x32simd, 2uz>>(need_write)};

                    auto const need_write_u8x32x2v0{need_write_u8x32x2.front_unchecked()};

                    // Checked. No overflow.
                    func_counter += handled_simd;

                    //  [... curr ... (63) ...]
                    //  [      safe           ] unsafe (could be the section_end)
                    //       ^^ functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr
                    //      [    needwrite   ]

                    // The above func_counter check checks for a maximum of 64 data to be processed at a time,
                    // and it's perfectly safe to do so here without any additional checks

                    ::std::memcpy(functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr, ::std::addressof(need_write_u8x32x2v0), sizeof(u8x32simd));

                    functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr += handled_simd;

                    //  [... curr ... (63) ...]
                    //  [          safe       ] unsafe (could be the section_end)
                    //                    ^^ functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr
                    // Or: (Security boundaries for writes are checked)
                    //                        ^^ functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr
                    //      [    needwrite   ]

                    section_curr += first_round;

                    // [before_section ... | func_count ... typeidx1 ... (63) ...] ...
                    // [                        safe                             ] unsafe (could be the section_end)
                    //                                                   ^^^^^^^ section_curr (processed_byte_counter_sum <= 64)
                    //                                      [   simd_vector_str  ]
                }
            }
            else
            {
                // All correct, can change state: func_counter, section_curr

                // all are single bytes, so there are 64
                ::uwvm2::parser::wasm::utils::counter_addass_throw_when_overflow(func_counter,
                                                                                 static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(64u),
                                                                                 section_curr,
                                                                                 err);

                // check counter
                if(func_counter > func_count) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_selectable.u32 = func_count;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                // Since everything is less than 128, there is no need to check the typeidx.

                // write data, func_counter has been checked and is ready to be written.
                ::std::memcpy(functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr, ::std::addressof(simd_vector_str), sizeof(u8x64simd));

                functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr += 64uz;

                section_curr += 64uz;

                // [before_section ... | func_count ... typeidx1 ... (63) ...] typeidx64
                // [                        safe                             ] unsafe (could be the section_end)
                //                                                             ^^ section_curr
            }

            // After testing, 12 and 8 are the fastest

            ::uwvm2::utils::intrinsics::universal::prefetch<::uwvm2::utils::intrinsics::universal::pfc_mode::read,
                                                            ::uwvm2::utils::intrinsics::universal::pfc_level::nta,
                                                            ::uwvm2::utils::intrinsics::universal::ret_policy::strm>(
                reinterpret_cast<::std::byte const*>(section_curr) + 64u * 12u);
            ::uwvm2::utils::intrinsics::universal::prefetch<::uwvm2::utils::intrinsics::universal::pfc_mode::write,
                                                            ::uwvm2::utils::intrinsics::universal::pfc_level::L2,
                                                            ::uwvm2::utils::intrinsics::universal::ret_policy::keep>(
                reinterpret_cast<::std::byte const*>(functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr) + 64u * 8u);
        }

#elif UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__) && UWVM_HAS_BUILTIN(__builtin_shufflevector) &&                           \
        ((defined(__AVX2__) && UWVM_HAS_BUILTIN(__builtin_ia32_pmovmskb256) && UWVM_HAS_BUILTIN(__builtin_ia32_pshufb256)) &&                                  \
         (defined(__AVX__) && UWVM_HAS_BUILTIN(__builtin_ia32_ptestz256))) ||                                                                                  \
    (defined(__loongarch_asx) && UWVM_HAS_BUILTIN(__builtin_lasx_xvmskltz_b) && UWVM_HAS_BUILTIN(__builtin_lasx_xvshuf_b) &&                                   \
     UWVM_HAS_BUILTIN(__builtin_lasx_xbnz_v))
        /// (Little Endian), [[gnu::vector_size]], has mask-u16, can shuffle, simd256
        /// x86_64-avx2, loongarch-LSX
        // This error handle is provided when simd is unable to handle the error, including when it encounters a memory boundary, or when there is an error that
        // needs to be localized.

        /// @note The sse4 version of scan_function_section_impl_uN_Xb is about 6% faster than avx (mundane read+judge+ptest) on Windows because the kernel
        ///       mapping in the Windows kernel's ntoskrnl.exe uses the `movups xmm` instruction for out-of-page interrupts (The reason is that the kernel
        ///       mapping in the Windows kernel ntoskrnl.exe will use the `movups xmm` instruction for out-of-page interrupts (ntoskrnl didn't do the avx
        ///       adaptation, and used the vex version), which leads to the state switching of the ymm mixed with this kind of instruction and wastes dozens
        ///       of cpu instruction cycles for the switching back and forth. linux doesn't have this problem at all, and at the same time, due to the
        ///       better algorithm of the kernel mapping, the parsing efficiency is 4 times higher than that of Windows, and most of the time is wasted in
        ///       the ntoskrnl (This can be tested with vtune). Here still use avx version, if you need sse4 version, please choose sse4 version.
        ///
        ///       (It may be misrepresented, but the fact is that the time consumption of ntoskrnl is tested in vtune to increase a lot,
        ///       while the simd processing part of the time decreases)

        auto error_handler{[&](::std::size_t n) constexpr UWVM_THROWS -> void
                           {
                               // Need to ensure that section_curr to section_curr + n is memory safe
                               // Processing uses section_curr_end to guarantee entry into the loop for inspection.
                               // Also use section_end to ensure that the scanning of the last leb128 is handled correctly, both memory safe.

                               auto const section_curr_end{section_curr + n};

            // [ ... typeidx1 ... ] (section_curr_end) ... (outer) ]
            // [     safe     ... ]                    ... (outer) ] unsafe (could be the section_end)
            //       ^^ section_curr

            // Since parse_by_scan uses section_end, it is possible that section_curr will be greater than section_curr_end, use '<' instead
            // of '!='

# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                               // Used to check if correct content is recognized as an error.
                               bool correct_sequence_right_taken_for_wrong{true};
# endif

                               while(section_curr < section_curr_end)
                               {
                                   // The outer boundary is unknown and needs to be rechecked
                                   // [ ... typeidx1] ... (outer) ] typeidx2 ...
                                   // [     safe    ] ... (outer) ] unsafe (could be the section_end)
                                   //       ^^ section_curr

                                   if(::uwvm2::parser::wasm::utils::counter_selfinc_throw_when_overflow(func_counter, section_curr, err) > func_count)
                                       [[unlikely]]
                                   {
                                       err.err_curr = section_curr;
                                       err.err_selectable.u32 = func_count;
                                       err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                                       ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                   }

                                   ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 typeidx;  // No initialization necessary

                                   using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                                   auto const [typeidx_next, typeidx_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                                                   reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                                                   ::fast_io::mnp::leb128_get(typeidx))};

                                   if(typeidx_err != ::fast_io::parse_code::ok) [[unlikely]]
                                   {
                                       err.err_curr = section_curr;
                                       err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_type_index;
                                       ::uwvm2::parser::wasm::base::throw_wasm_parse_code(typeidx_err);
                                   }

                                   // The outer boundary is unknown and needs to be rechecked
                                   // [ ... typeidx1 ...] ... (outer) ] typeidx2 ...
                                   // [      safe       ] ... (outer) ] unsafe (could be the section_end)
                                   //       ^^ section_curr

                                   // There's a good chance there's an error here.
                                   // Or there is a leb redundancy 0
                                   if(typeidx >= type_section_count) [[unlikely]]
                                   {
                                       err.err_curr = section_curr;
                                       err.err_selectable.u32 = typeidx;
                                       err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_type_index;
                                       ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                   }

                                   // write data, func_counter has been checked and is ready to be written.
                                   functionsec.funcs.storage.typeidx_u8_vector.emplace_back_unchecked(
                                       static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8>(typeidx));

# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                   if(static_cast<::std::size_t>(reinterpret_cast<::std::byte const*>(typeidx_next) - section_curr) > 2uz) [[unlikely]]
                                   {
                                       correct_sequence_right_taken_for_wrong = false;
                                   }
# endif

                                   section_curr = reinterpret_cast<::std::byte const*>(typeidx_next);

                                   // The outer boundary is unknown and needs to be rechecked
                                   // [ ... typeidx1 ...] typeidx2 ...] ...
                                   // [      safe       ] ... (outer) ] unsafe (could be the section_end)
                                   //                     ^^ section_curr
                               }

            // [before_section ... | func_count ... typeidx1 ... (n - 1) ... ...  ] typeidxN
            // [                        safe                                      ] unsafe (could be the section_end)
            //                                                                      ^^ section_curr
            //                                      [   simd_vector_str  ]  ...   ] (Depends on the size of section_curr in relation to
            //                                      section_curr_end)

# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                               if(correct_sequence_right_taken_for_wrong) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif
                           }};

        using i64x4simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = ::std::int64_t;
        using u64x4simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = ::std::uint64_t;
        using u32x8simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = ::std::uint32_t;
        using u16x16simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = ::std::uint16_t;
        using c8x32simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = char;
        using u8x32simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = ::std::uint8_t;
        using i8x32simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = ::std::int8_t;

        using i64x2simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::int64_t;
        using u64x2simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint64_t;
        using u32x4simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint32_t;
        using u16x8simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint16_t;
        using c8x16simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = char;
        using u8x16simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint8_t;
        using i8x16simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::int8_t;

        static_assert(::std::same_as<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte, ::std::uint8_t>);

        while(static_cast<::std::size_t>(section_end - section_curr) >= 32uz) [[likely]]
        {
            // [before_section ... | func_count ... typeidx1 ... (31) ...] ...
            // [                        safe                             ] unsafe (could be the section_end)
            //                                      ^^ section_curr
            //                                      [   simd_vector_str  ]

            u8x32simd simd_vector_str;  // No initialization necessary

            ::std::memcpy(::std::addressof(simd_vector_str), section_curr, sizeof(u8x32simd));

            // It's already a little-endian.

            // When the highest bit of each byte is pop, then mask out 1

            auto const check_mask{
# if defined(__AVX2__) && UWVM_HAS_BUILTIN(__builtin_ia32_pmovmskb256)
                static_cast<unsigned>(__builtin_ia32_pmovmskb256(::std::bit_cast<c8x32simd>(simd_vector_str)))
# elif defined(__loongarch_asx) && UWVM_HAS_BUILTIN(__builtin_lasx_xvmskltz_b)
                ::std::bit_cast<::uwvm2::utils::container::array<unsigned, 8uz>>(__builtin_lasx_xvmskltz_b(::std::bit_cast<i8x32simd>(simd_vector_str)))
                    .front_unchecked()
# else
#  error "missing instructions"
# endif
            };

            if(
# if UWVM_HAS_BUILTIN(__builtin_expect_with_probability)
                __builtin_expect_with_probability(static_cast<bool>(check_mask), true, 1.0 / 4.0)
# else
                check_mask
# endif
            )
            {
                ::std::uint16_t const simd_vector_check{static_cast<::std::uint16_t>(type_section_count)};

                auto check_mask_curr{check_mask};  // uleb128 mask

                // Record the number of bytes processed in the first round
                unsigned first_and_second_round_handle_bytes{};

                // 1st and 2nd round
                {
                    unsigned const check_table_index_1st{check_mask_curr & 0xFFu};

                    // according to the information in the table, when the eighth bit is 1, then the processing length is 7
                    bool const first_round_is_seven{static_cast<bool>(check_table_index_1st & 0x80u)};

                    check_mask_curr >>= 8u - static_cast<unsigned>(first_round_is_seven);

                    first_and_second_round_handle_bytes += static_cast<unsigned>(first_round_is_seven);

                    unsigned const check_table_index_2nd{check_mask_curr & 0xFFu};

                    // according to the information in the table, when the eighth bit is 1, then the processing length is 7
                    bool const second_round_is_seven{static_cast<bool>(check_table_index_2nd & 0x80u)};

                    check_mask_curr >>= 8u - static_cast<unsigned>(second_round_is_seven);

                    first_and_second_round_handle_bytes += static_cast<unsigned>(second_round_is_seven);

                    // When there are few operations, 2-branch prediction may seem inferior to one operation + one branch prediction
                    // For parsing random data that may be encountered, it is more efficient to use bitwise operations than to add branches using logical
                    // operators.

                    if(check_table_index_1st | check_table_index_2nd)
                    {
                        // There are non-zero terms in 1st and 2nd, so it's straightforward to construct a 256 (128 * 2) shuffle.

                        //  [...] curr ...
                        //  [sf ] unsafe (could be the section_end)
                        //        ^^ functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr

                        // The write size is 16 bits, but the valid data may be less than 16 bits, directly check the maximum value, more than that, then enter
                        // the tail processing Cannot change state until error checking: func_counter, section_curr

                        auto const func_counter_end{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(func_counter + 16u)};

                        if(func_counter_end < func_counter) [[unlikely]]
                        {
                            // It's overflowing. It's jumping right out.
                            break;
                        }

                        if(func_counter_end > func_count) [[unlikely]]
                        {
                            // Near the end, jump directly out of the simd section and hand it over to the tail.
                            break;
                        }

                        //  [... curr ... (15) ...]
                        //  [        safe         ] unsafe (could be the section_end)
                        //       ^^ functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr
                        //      [ needwrite ] ... ] (Always writes 16 bits, but valid data may be less than 16 bits)

                        auto const& curr_table_1st{simd128_shuffle_table.index_unchecked(static_cast<::std::size_t>(check_table_index_1st))};
                        auto const curr_table_shuffle_mask_1st{curr_table_1st.shuffle_mask};
                        auto const curr_table_processed_simd_1st{curr_table_1st.processed_simd};  // size of handled u32
                        auto const curr_table_processed_byte_1st{curr_table_1st.processed_byte};  // size of handled uleb128

                        auto const& curr_table_2nd{simd128_shuffle_table.index_unchecked(static_cast<::std::size_t>(check_table_index_2nd))};
                        auto const curr_table_shuffle_mask_2nd{curr_table_2nd.shuffle_mask};
                        auto const curr_table_processed_simd_2nd{curr_table_2nd.processed_simd};  // size of handled u32
                        auto const curr_table_processed_byte_2nd{curr_table_2nd.processed_byte};  // size of handled uleb128

                        // When the number of consecutive bits is greater than 2, switch back to the normal processing method
                        // Error handling is almost never encountered, using logical operators to increase short-circuit evaluation, with branch prediction
                        // always faster than bitwise operations

                        if(!curr_table_processed_simd_1st || !curr_table_processed_simd_2nd) [[unlikely]]
                        {
                            // [before_section ... | func_count ... typeidx1 ... (31) ...] ...
                            // [                        safe                             ] unsafe (could be the section_end)
                            //                                      ^^ section_curr
                            //                                                             ^^ section_curr + 32uz

                            // it can be processed up to 32
                            error_handler(32uz);

                            // Start the next round straight away
                            continue;
                        }

                        //  [... curr ... (15) ...]
                        //  [        safe         ] unsafe (could be the section_end)
                        //       ^^ functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr
                        //      [ needwrite ] ... ] (Always writes 16 bits, but valid data may be less than 16 bits)

                        // shuffle and write

                        // avx2 since shuffle is in channels of 128, just splice directly
                        ::uwvm2::utils::container::array<u8x16simd, 2uz> const mask_table_u8x16x2{curr_table_shuffle_mask_1st, curr_table_shuffle_mask_2nd};
                        u8x32simd const mask_tableu8x32{::std::bit_cast<u8x32simd>(mask_table_u8x16x2)};

                        u8x32simd simd_vector_str_shuffle;

                        if(curr_table_processed_byte_1st == 7u)
                        {
                            simd_vector_str_shuffle = __builtin_shufflevector(simd_vector_str,
                                                                              simd_vector_str,
                                                                              0,   // 0
                                                                              1,   // 1
                                                                              2,   // 2
                                                                              3,   // 3
                                                                              4,   // 4
                                                                              5,   // 5
                                                                              6,   // 6
                                                                              -1,  // 7
                                                                              -1,  // 8
                                                                              -1,  // 9
                                                                              -1,  // 10
                                                                              -1,  // 11
                                                                              -1,  // 12
                                                                              -1,  // 13
                                                                              -1,  // 14
                                                                              -1,  // 15
                                                                              7,   // 16
                                                                              8,   // 17
                                                                              9,   // 18
                                                                              10,  // 19
                                                                              11,  // 20
                                                                              12,  // 21
                                                                              13,  // 22
                                                                              14,  // 23
                                                                              -1,  // 24
                                                                              -1,  // 25
                                                                              -1,  // 26
                                                                              -1,  // 27
                                                                              -1,  // 28
                                                                              -1,  // 29
                                                                              -1,  // 30
                                                                              -1   // 31
                            );
                        }
                        else
                        {
                            simd_vector_str_shuffle = __builtin_shufflevector(simd_vector_str,
                                                                              simd_vector_str,
                                                                              0,   // 0
                                                                              1,   // 1
                                                                              2,   // 2
                                                                              3,   // 3
                                                                              4,   // 4
                                                                              5,   // 5
                                                                              6,   // 6
                                                                              7,   // 7
                                                                              -1,  // 8
                                                                              -1,  // 9
                                                                              -1,  // 10
                                                                              -1,  // 11
                                                                              -1,  // 12
                                                                              -1,  // 13
                                                                              -1,  // 14
                                                                              -1,  // 15
                                                                              8,   // 16
                                                                              9,   // 17
                                                                              10,  // 18
                                                                              11,  // 19
                                                                              12,  // 20
                                                                              13,  // 21
                                                                              14,  // 22
                                                                              15,  // 23
                                                                              -1,  // 24
                                                                              -1,  // 25
                                                                              -1,  // 26
                                                                              -1,  // 27
                                                                              -1,  // 28
                                                                              -1,  // 29
                                                                              -1,  // 30
                                                                              -1   // 31
                            );
                        }

                        u16x16simd mask_res;

# if defined(__AVX2__) && UWVM_HAS_BUILTIN(__builtin_ia32_pshufb256)
                        mask_res = ::std::bit_cast<u16x16simd>(__builtin_ia32_pshufb256(simd_vector_str_shuffle, mask_tableu8x32));
# elif defined(__loongarch_asx) && UWVM_HAS_BUILTIN(__builtin_lasx_xvshuf_b)
                        mask_res = ::std::bit_cast<u16x16simd>(
                            __builtin_lasx_xvshuf_b(simd_vector_str_shuffle, simd_vector_str_shuffle, mask_tableu8x32));  /// @todo need check
# else
#  error "missing instructions"
# endif

                        // The leb128 that has been merged into u16x8 is finalized to get the value corresponding to u16

                        auto const res{(mask_res & static_cast<::std::uint16_t>(0x7Fu)) | ((mask_res & static_cast<::std::uint16_t>(0x7F00u)) >> 1u)};

                        // The data out of shuffle is 16-bit [0, 2^14) and may be greater than or equal to typeidx, which needs to be checked.

                        auto const check_upper{res >= simd_vector_check};

                        if(
# if defined(__AVX__) && UWVM_HAS_BUILTIN(__builtin_ia32_ptestz256)
                            !__builtin_ia32_ptestz256(::std::bit_cast<i64x4simd>(check_upper), ::std::bit_cast<i64x4simd>(check_upper))
# elif defined(__loongarch_asx) && UWVM_HAS_BUILTIN(__builtin_lasx_xbnz_v)
                            __builtin_lasx_xbnz_v(::std::bit_cast<u8x32simd>(check_upper))  /// @todo need check
# else
#  error "missing instructions"
# endif
                                ) [[unlikely]]
                        {
                            // [before_section ... | func_count ... typeidx1 ... (31) ...] ...
                            // [                        safe                             ] unsafe (could be the section_end)
                            //                                      ^^ section_curr
                            //                                                             ^^ section_curr + 32uz

                            // it can be processed up to 32
                            error_handler(32uz);

                            // Start the next round straight away
                            continue;
                        }

                        // All correct, can change state: func_counter, section_curr

                        auto const curr_table_processed_simd_sum{curr_table_processed_simd_1st + curr_table_processed_simd_2nd};
                        auto const curr_table_processed_byte_sum{curr_table_processed_byte_1st + curr_table_processed_byte_2nd};

                        // Checked. No overflow.
                        func_counter += curr_table_processed_simd_sum;

                        // There is no need to check function_counter, because curr_table_processed_simd_sum is always less than or equal to 16u.

                        // Since typeidx can write to a u8, reduce u16x16 to u8x16

                        u8x32simd needwrite_u8x32;

                        switch(curr_table_processed_simd_1st)
                        {
                            case 4u:
                            {
                                needwrite_u8x32 = __builtin_shufflevector(::std::bit_cast<u8x32simd>(res),
                                                                          ::std::bit_cast<u8x32simd>(res),
                                                                          0,   // 0
                                                                          2,   // 1
                                                                          4,   // 2
                                                                          6,   // 3
                                                                          16,  // 4
                                                                          18,  // 5
                                                                          20,  // 6
                                                                          22,  // 7
                                                                          24,  // 8
                                                                          26,  // 9
                                                                          28,  // 10
                                                                          30,  // 11
                                                                          -1,  // 12
                                                                          -1,  // 13
                                                                          -1,  // 14
                                                                          -1,  // 15
                                                                          -1,  // 16
                                                                          -1,  // 17
                                                                          -1,  // 18
                                                                          -1,  // 19
                                                                          -1,  // 20
                                                                          -1,  // 21
                                                                          -1,  // 22
                                                                          -1,  // 23
                                                                          -1,  // 24
                                                                          -1,  // 25
                                                                          -1,  // 26
                                                                          -1,  // 27
                                                                          -1,  // 28
                                                                          -1,  // 29
                                                                          -1,  // 30
                                                                          -1   // 31
                                );
                                break;
                            }
                            case 5u:
                            {
                                needwrite_u8x32 = __builtin_shufflevector(::std::bit_cast<u8x32simd>(res),
                                                                          ::std::bit_cast<u8x32simd>(res),
                                                                          0,   // 0
                                                                          2,   // 1
                                                                          4,   // 2
                                                                          6,   // 3
                                                                          8,   // 4
                                                                          16,  // 5
                                                                          18,  // 6
                                                                          20,  // 7
                                                                          22,  // 8
                                                                          24,  // 9
                                                                          26,  // 10
                                                                          28,  // 11
                                                                          30,  // 12
                                                                          -1,  // 13
                                                                          -1,  // 14
                                                                          -1,  // 15
                                                                          -1,  // 16
                                                                          -1,  // 17
                                                                          -1,  // 18
                                                                          -1,  // 19
                                                                          -1,  // 20
                                                                          -1,  // 21
                                                                          -1,  // 22
                                                                          -1,  // 23
                                                                          -1,  // 24
                                                                          -1,  // 25
                                                                          -1,  // 26
                                                                          -1,  // 27
                                                                          -1,  // 28
                                                                          -1,  // 29
                                                                          -1,  // 30
                                                                          -1   // 31
                                );
                                break;
                            }
                            case 6u:
                            {
                                needwrite_u8x32 = __builtin_shufflevector(::std::bit_cast<u8x32simd>(res),
                                                                          ::std::bit_cast<u8x32simd>(res),
                                                                          0,   // 0
                                                                          2,   // 1
                                                                          4,   // 2
                                                                          6,   // 3
                                                                          8,   // 4
                                                                          10,  // 5
                                                                          16,  // 6
                                                                          18,  // 7
                                                                          20,  // 8
                                                                          22,  // 9
                                                                          24,  // 10
                                                                          26,  // 11
                                                                          28,  // 12
                                                                          30,  // 13
                                                                          -1,  // 14
                                                                          -1,  // 15
                                                                          -1,  // 16
                                                                          -1,  // 17
                                                                          -1,  // 18
                                                                          -1,  // 19
                                                                          -1,  // 20
                                                                          -1,  // 21
                                                                          -1,  // 22
                                                                          -1,  // 23
                                                                          -1,  // 24
                                                                          -1,  // 25
                                                                          -1,  // 26
                                                                          -1,  // 27
                                                                          -1,  // 28
                                                                          -1,  // 29
                                                                          -1,  // 30
                                                                          -1   // 31
                                );
                                break;
                            }
                            case 7u:
                            {
                                needwrite_u8x32 = __builtin_shufflevector(::std::bit_cast<u8x32simd>(res),
                                                                          ::std::bit_cast<u8x32simd>(res),
                                                                          0,   // 0
                                                                          2,   // 1
                                                                          4,   // 2
                                                                          6,   // 3
                                                                          8,   // 4
                                                                          10,  // 5
                                                                          12,  // 6
                                                                          16,  // 7
                                                                          18,  // 8
                                                                          20,  // 9
                                                                          22,  // 10
                                                                          24,  // 11
                                                                          26,  // 12
                                                                          28,  // 13
                                                                          30,  // 14
                                                                          -1,  // 15
                                                                          -1,  // 16
                                                                          -1,  // 17
                                                                          -1,  // 18
                                                                          -1,  // 19
                                                                          -1,  // 20
                                                                          -1,  // 21
                                                                          -1,  // 22
                                                                          -1,  // 23
                                                                          -1,  // 24
                                                                          -1,  // 25
                                                                          -1,  // 26
                                                                          -1,  // 27
                                                                          -1,  // 28
                                                                          -1,  // 29
                                                                          -1,  // 30
                                                                          -1   // 31
                                );
                                break;
                            }
                            case 8u:
                            {
                                needwrite_u8x32 = __builtin_shufflevector(::std::bit_cast<u8x32simd>(res),
                                                                          ::std::bit_cast<u8x32simd>(res),
                                                                          0,   // 0
                                                                          2,   // 1
                                                                          4,   // 2
                                                                          6,   // 3
                                                                          8,   // 4
                                                                          10,  // 5
                                                                          12,  // 6
                                                                          14,  // 7
                                                                          16,  // 8
                                                                          18,  // 9
                                                                          20,  // 10
                                                                          22,  // 11
                                                                          24,  // 12
                                                                          26,  // 13
                                                                          28,  // 14
                                                                          30,  // 15
                                                                          -1,  // 16
                                                                          -1,  // 17
                                                                          -1,  // 18
                                                                          -1,  // 19
                                                                          -1,  // 20
                                                                          -1,  // 21
                                                                          -1,  // 22
                                                                          -1,  // 23
                                                                          -1,  // 24
                                                                          -1,  // 25
                                                                          -1,  // 26
                                                                          -1,  // 27
                                                                          -1,  // 28
                                                                          -1,  // 29
                                                                          -1,  // 30
                                                                          -1   // 31
                                );
                                break;
                            }
                            [[unlikely]] default:
                            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                                ::std::unreachable();
                            }
                        }

                        auto const needwrite_u8x16x2{::std::bit_cast<::uwvm2::utils::container::array<u8x16simd, 2uz>>(needwrite_u8x32)};

                        auto const needwrite_u8x16x2v0{needwrite_u8x16x2.front_unchecked()};

                        //  [... curr ... (15) ... curr_next ... (15) ...]
                        //  [                 safe                       ] unsafe (could be the section_end)
                        //       ^^ functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr
                        //      [    needwrite   ]

                        ::std::memcpy(functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr, ::std::addressof(needwrite_u8x16x2v0), sizeof(u8x16simd));

                        functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr += curr_table_processed_simd_sum;

                        //  [... curr ... (15) ... curr_next ... (15) ...]
                        //  [                 safe                       ] unsafe (could be the section_end)
                        //                    ^^ functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr
                        // Or: (Security boundaries for writes are checked)
                        //                        ^^ functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr
                        //      [    needwrite   ]

                        section_curr += curr_table_processed_byte_sum;

                        // [before_section ... | func_count ... typeidx1 ... (15) ... ... (15) ...] ...
                        // [                        safe                                          ] unsafe (could be the section_end)
                        //                                                   ^^^^^^^ section_curr (curr_table_processed_byte <= 8)
                        //                                      [   simd_vector_str               ]
                    }
                    else
                    {
                        // If check_table_index_1st or check_table_index_2nd is 0, write directly

                        // check func_counter before write
                        // The check is all done and the status can be changed, if it is less than the value written, then something is wrong.

                        ::uwvm2::parser::wasm::utils::counter_addass_throw_when_overflow(
                            func_counter,
                            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(16u),
                            section_curr,
                            err);

                        // check counter
                        if(func_counter > func_count) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_selectable.u32 = func_count;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        // Since everything is less than 128, there is no need to check the typeidx.

                        // write 16 byte
                        using u8x16simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint8_t;

                        auto const needwrite_u8x16x2{::std::bit_cast<::uwvm2::utils::container::array<u8x16simd, 2uz>>(simd_vector_str)};

                        //  XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX
                        // [                 needwrite_u16x8x2v0           ]

                        auto const needwrite_u8x16x2v0{needwrite_u8x16x2.front_unchecked()};

                        //  [... curr ... (15) ... curr_next ... (15) ...]
                        //  [                 safe                       ] unsafe (could be the section_end)
                        //       ^^ functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr
                        //      [    needwrite    ]

                        ::std::memcpy(functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr, ::std::addressof(needwrite_u8x16x2v0), sizeof(u8x16simd));

                        functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr += 16u;

                        //  [... curr_last ... (15) ... curr ... (15) ...]
                        //  [                  safe                      ] unsafe (could be the section_end)
                        //                              ^^ functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr

                        section_curr += 16u;

                        // [before_section ... | func_count ... typeidx1 ... (15) ... typeidxc ... (15) ...] ...
                        // [                                safe                                           ] unsafe (could be the section_end)
                        //                                                            ^^ section_curr
                        //                                      [                simd_vector_str           ]
                    }
                }

                u8x32simd third_fourth_round_simd_u8x32;

                switch(first_and_second_round_handle_bytes)
                {
                    case 0u:  // handle 16u
                    {
                        auto const shuffle_u8x32{__builtin_shufflevector(simd_vector_str,
                                                                         simd_vector_str,
                                                                         16,  // 0
                                                                         17,  // 1
                                                                         18,  // 2
                                                                         19,  // 3
                                                                         20,  // 4
                                                                         21,  // 5
                                                                         22,  // 6
                                                                         23,  // 7
                                                                         24,  // 8
                                                                         25,  // 9
                                                                         26,  // 10
                                                                         27,  // 11
                                                                         28,  // 12
                                                                         29,  // 13
                                                                         30,  // 14
                                                                         31,  // 15
                                                                         -1,  // 16
                                                                         -1,  // 17
                                                                         -1,  // 18
                                                                         -1,  // 19
                                                                         -1,  // 20
                                                                         -1,  // 21
                                                                         -1,  // 22
                                                                         -1,  // 23
                                                                         -1,  // 24
                                                                         -1,  // 25
                                                                         -1,  // 26
                                                                         -1,  // 27
                                                                         -1,  // 28
                                                                         -1,  // 29
                                                                         -1,  // 30
                                                                         -1   // 31
                                                                         )};
                        third_fourth_round_simd_u8x32 = ::std::bit_cast<u8x32simd>(shuffle_u8x32);
                        break;
                    }
                    case 1u:  // handle 15u
                    {
                        auto const shuffle_u8x32{__builtin_shufflevector(simd_vector_str,
                                                                         simd_vector_str,
                                                                         15,  // 0
                                                                         16,  // 1
                                                                         17,  // 2
                                                                         18,  // 3
                                                                         19,  // 4
                                                                         20,  // 5
                                                                         21,  // 6
                                                                         22,  // 7
                                                                         23,  // 8
                                                                         24,  // 9
                                                                         25,  // 10
                                                                         26,  // 11
                                                                         27,  // 12
                                                                         28,  // 13
                                                                         29,  // 14
                                                                         30,  // 15
                                                                         -1,  // 16
                                                                         -1,  // 17
                                                                         -1,  // 18
                                                                         -1,  // 19
                                                                         -1,  // 20
                                                                         -1,  // 21
                                                                         -1,  // 22
                                                                         -1,  // 23
                                                                         -1,  // 24
                                                                         -1,  // 25
                                                                         -1,  // 26
                                                                         -1,  // 27
                                                                         -1,  // 28
                                                                         -1,  // 29
                                                                         -1,  // 30
                                                                         -1   // 31
                                                                         )};
                        third_fourth_round_simd_u8x32 = ::std::bit_cast<u8x32simd>(shuffle_u8x32);
                        break;
                    }
                    case 2u:  // handle 14u
                    {
                        auto const shuffle_u8x32{__builtin_shufflevector(simd_vector_str,
                                                                         simd_vector_str,
                                                                         14,  // 0
                                                                         15,  // 1
                                                                         16,  // 2
                                                                         17,  // 3
                                                                         18,  // 4
                                                                         19,  // 5
                                                                         20,  // 6
                                                                         21,  // 7
                                                                         22,  // 8
                                                                         23,  // 9
                                                                         24,  // 10
                                                                         25,  // 11
                                                                         26,  // 12
                                                                         27,  // 13
                                                                         28,  // 14
                                                                         29,  // 15
                                                                         -1,  // 16
                                                                         -1,  // 17
                                                                         -1,  // 18
                                                                         -1,  // 19
                                                                         -1,  // 20
                                                                         -1,  // 21
                                                                         -1,  // 22
                                                                         -1,  // 23
                                                                         -1,  // 24
                                                                         -1,  // 25
                                                                         -1,  // 26
                                                                         -1,  // 27
                                                                         -1,  // 28
                                                                         -1,  // 29
                                                                         -1,  // 30
                                                                         -1   // 31
                                                                         )};
                        third_fourth_round_simd_u8x32 = ::std::bit_cast<u8x32simd>(shuffle_u8x32);
                        break;
                    }
                    [[unlikely]] default:
                    {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                        ::std::unreachable();
                    }
                }

                // 3rd and 4th round
                {
                    unsigned const check_table_index_1st{check_mask_curr & 0xFFu};

                    // according to the information in the table, when the eighth bit is 1, then the processing length is 7
                    bool const first_round_is_seven{static_cast<bool>(check_table_index_1st & 0x80u)};

                    check_mask_curr >>= 8u - static_cast<unsigned>(first_round_is_seven);

                    unsigned const check_table_index_2nd{check_mask_curr & 0xFFu};

                    // according to the information in the table, when the eighth bit is 1, then the processing length is 7

                    // check_mask_curr to be useless and no further calculations are needed.

                    // When there are few operations, 2-branch prediction may seem inferior to one operation + one branch prediction
                    // For parsing random data that may be encountered, it is more efficient to use bitwise operations than to add branches using logical
                    // operators.

                    if(check_table_index_1st | check_table_index_2nd)
                    {
                        // There are non-zero terms in 1st and 2nd, so it's straightforward to construct a 256 (128 * 2) shuffle.

                        //  [...] curr ...
                        //  [sf ] unsafe (could be the section_end)
                        //        ^^ functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr

                        // The write size is 16 bits, but the valid data may be less than 16 bits, directly check the maximum value, more than that, then enter
                        // the tail processing Cannot change state until error checking: func_counter, section_curr

                        auto const func_counter_end{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(func_counter + 16u)};

                        if(func_counter_end < func_counter) [[unlikely]]
                        {
                            // It's overflowing. It's jumping right out.
                            break;
                        }

                        if(func_counter_end > func_count) [[unlikely]]
                        {
                            // Near the end, jump directly out of the simd section and hand it over to the tail.
                            break;
                        }

                        //  [... curr ... (15) ...]
                        //  [        safe         ] unsafe (could be the section_end)
                        //       ^^ functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr
                        //      [ needwrite ] ... ] (Always writes 16 bits, but valid data may be less than 16 bits)

                        auto const& curr_table_1st{simd128_shuffle_table.index_unchecked(static_cast<::std::size_t>(check_table_index_1st))};
                        auto const curr_table_shuffle_mask_1st{curr_table_1st.shuffle_mask};
                        auto const curr_table_processed_simd_1st{curr_table_1st.processed_simd};  // size of handled u32
                        auto const curr_table_processed_byte_1st{curr_table_1st.processed_byte};  // size of handled uleb128

                        auto const& curr_table_2nd{simd128_shuffle_table.index_unchecked(static_cast<::std::size_t>(check_table_index_2nd))};
                        auto const curr_table_shuffle_mask_2nd{curr_table_2nd.shuffle_mask};
                        auto const curr_table_processed_simd_2nd{curr_table_2nd.processed_simd};  // size of handled u32
                        auto const curr_table_processed_byte_2nd{curr_table_2nd.processed_byte};  // size of handled uleb128

                        // When the number of consecutive bits is greater than 2, switch back to the normal processing method
                        // Error handling is almost never encountered, using logical operators to increase short-circuit evaluation, with branch prediction
                        // always faster than bitwise operations

                        if(!curr_table_processed_simd_1st || !curr_table_processed_simd_2nd) [[unlikely]]
                        {
                            // [before_section ... | func_count ... typeidx1 ... (31) ...] ...
                            // [                        safe                             ] unsafe (could be the section_end)
                            //                                      ^^ section_curr
                            //                                                             ^^ section_curr + 32uz

                            // it can be processed up to 32 - 16 == 16
                            error_handler(16uz);

                            // Start the next round straight away
                            continue;
                        }

                        //  [... curr ... (15) ...]
                        //  [        safe         ] unsafe (could be the section_end)
                        //       ^^ functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr
                        //      [ needwrite ] ... ] (Always writes 16 bits, but valid data may be less than 16 bits)

                        // shuffle and write

                        ::uwvm2::utils::container::array<u8x16simd, 2uz> const mask_table_u8x16x2{curr_table_shuffle_mask_1st, curr_table_shuffle_mask_2nd};
                        u8x32simd const mask_tableu8x32{::std::bit_cast<u8x32simd>(mask_table_u8x16x2)};

                        u8x32simd simd_vector_str_shuffle;

                        if(curr_table_processed_byte_1st == 7u)
                        {
                            simd_vector_str_shuffle = __builtin_shufflevector(third_fourth_round_simd_u8x32,
                                                                              third_fourth_round_simd_u8x32,
                                                                              0,   // 0
                                                                              1,   // 1
                                                                              2,   // 2
                                                                              3,   // 3
                                                                              4,   // 4
                                                                              5,   // 5
                                                                              6,   // 6
                                                                              -1,  // 7
                                                                              -1,  // 8
                                                                              -1,  // 9
                                                                              -1,  // 10
                                                                              -1,  // 11
                                                                              -1,  // 12
                                                                              -1,  // 13
                                                                              -1,  // 14
                                                                              -1,  // 15
                                                                              7,   // 16
                                                                              8,   // 17
                                                                              9,   // 18
                                                                              10,  // 19
                                                                              11,  // 20
                                                                              12,  // 21
                                                                              13,  // 22
                                                                              14,  // 23
                                                                              -1,  // 24
                                                                              -1,  // 25
                                                                              -1,  // 26
                                                                              -1,  // 27
                                                                              -1,  // 28
                                                                              -1,  // 29
                                                                              -1,  // 30
                                                                              -1   // 31
                            );
                        }
                        else
                        {
                            simd_vector_str_shuffle = __builtin_shufflevector(third_fourth_round_simd_u8x32,
                                                                              third_fourth_round_simd_u8x32,
                                                                              0,   // 0
                                                                              1,   // 1
                                                                              2,   // 2
                                                                              3,   // 3
                                                                              4,   // 4
                                                                              5,   // 5
                                                                              6,   // 6
                                                                              7,   // 7
                                                                              -1,  // 8
                                                                              -1,  // 9
                                                                              -1,  // 10
                                                                              -1,  // 11
                                                                              -1,  // 12
                                                                              -1,  // 13
                                                                              -1,  // 14
                                                                              -1,  // 15
                                                                              8,   // 16
                                                                              9,   // 17
                                                                              10,  // 18
                                                                              11,  // 19
                                                                              12,  // 20
                                                                              13,  // 21
                                                                              14,  // 22
                                                                              15,  // 23
                                                                              -1,  // 24
                                                                              -1,  // 25
                                                                              -1,  // 26
                                                                              -1,  // 27
                                                                              -1,  // 28
                                                                              -1,  // 29
                                                                              -1,  // 30
                                                                              -1   // 31
                            );
                        }

                        u16x16simd mask_res;

# if defined(__AVX2__) && UWVM_HAS_BUILTIN(__builtin_ia32_pshufb256)
                        mask_res = ::std::bit_cast<u16x16simd>(__builtin_ia32_pshufb256(simd_vector_str_shuffle, mask_tableu8x32));
# elif defined(__loongarch_asx) && UWVM_HAS_BUILTIN(__builtin_lasx_xvshuf_b)
                        mask_res = ::std::bit_cast<u16x16simd>(
                            __builtin_lasx_xvshuf_b(simd_vector_str_shuffle, simd_vector_str_shuffle, mask_tableu8x32));  /// @todo need check
# else
#  error "missing instructions"
# endif

                        // The leb128 that has been merged into u16x8 is finalized to get the value corresponding to u16

                        auto const res{(mask_res & static_cast<::std::uint16_t>(0x7Fu)) | ((mask_res & static_cast<::std::uint16_t>(0x7F00u)) >> 1u)};

                        // The data out of shuffle is 16-bit [0, 2^14) and may be greater than or equal to typeidx, which needs to be checked.

                        auto const check_upper{res >= simd_vector_check};

                        if(
# if defined(__AVX__) && UWVM_HAS_BUILTIN(__builtin_ia32_ptestz256)
                            !__builtin_ia32_ptestz256(::std::bit_cast<i64x4simd>(check_upper), ::std::bit_cast<i64x4simd>(check_upper))
# elif defined(__loongarch_asx) && UWVM_HAS_BUILTIN(__builtin_lasx_xbnz_v)
                            __builtin_lasx_xbnz_v(::std::bit_cast<u8x32simd>(check_upper))  /// @todo need check
# else
#  error "missing instructions"
# endif
                                ) [[unlikely]]
                        {
                            // [before_section ... | func_count ... typeidx1 ... (31) ...] ...
                            // [                        safe                             ] unsafe (could be the section_end)
                            //                                      ^^ section_curr
                            //                                                             ^^ section_curr + 32uz

                            // it can be processed up to 32 - 16 == 16
                            error_handler(16uz);

                            // Start the next round straight away
                            continue;
                        }

                        // All correct, can change state: func_counter, section_curr

                        auto const curr_table_processed_simd_sum{curr_table_processed_simd_1st + curr_table_processed_simd_2nd};
                        auto const curr_table_processed_byte_sum{curr_table_processed_byte_1st + curr_table_processed_byte_2nd};

                        // Checked. No overflow.
                        func_counter += curr_table_processed_simd_sum;

                        // There is no need to check function_counter, because curr_table_processed_simd_sum is always less than or equal to 16u.

                        // Since typeidx can write to a u8, reduce u16x16 to u8x16

                        u8x32simd needwrite_u8x32;

                        switch(curr_table_processed_simd_1st)
                        {
                            case 4u:
                            {
                                needwrite_u8x32 = __builtin_shufflevector(::std::bit_cast<u8x32simd>(res),
                                                                          ::std::bit_cast<u8x32simd>(res),
                                                                          0,   // 0
                                                                          2,   // 1
                                                                          4,   // 2
                                                                          6,   // 3
                                                                          16,  // 4
                                                                          18,  // 5
                                                                          20,  // 6
                                                                          22,  // 7
                                                                          24,  // 8
                                                                          26,  // 9
                                                                          28,  // 10
                                                                          30,  // 11
                                                                          -1,  // 12
                                                                          -1,  // 13
                                                                          -1,  // 14
                                                                          -1,  // 15
                                                                          -1,  // 16
                                                                          -1,  // 17
                                                                          -1,  // 18
                                                                          -1,  // 19
                                                                          -1,  // 20
                                                                          -1,  // 21
                                                                          -1,  // 22
                                                                          -1,  // 23
                                                                          -1,  // 24
                                                                          -1,  // 25
                                                                          -1,  // 26
                                                                          -1,  // 27
                                                                          -1,  // 28
                                                                          -1,  // 29
                                                                          -1,  // 30
                                                                          -1   // 31
                                );
                                break;
                            }
                            case 5u:
                            {
                                needwrite_u8x32 = __builtin_shufflevector(::std::bit_cast<u8x32simd>(res),
                                                                          ::std::bit_cast<u8x32simd>(res),
                                                                          0,   // 0
                                                                          2,   // 1
                                                                          4,   // 2
                                                                          6,   // 3
                                                                          8,   // 4
                                                                          16,  // 5
                                                                          18,  // 6
                                                                          20,  // 7
                                                                          22,  // 8
                                                                          24,  // 9
                                                                          26,  // 10
                                                                          28,  // 11
                                                                          30,  // 12
                                                                          -1,  // 13
                                                                          -1,  // 14
                                                                          -1,  // 15
                                                                          -1,  // 16
                                                                          -1,  // 17
                                                                          -1,  // 18
                                                                          -1,  // 19
                                                                          -1,  // 20
                                                                          -1,  // 21
                                                                          -1,  // 22
                                                                          -1,  // 23
                                                                          -1,  // 24
                                                                          -1,  // 25
                                                                          -1,  // 26
                                                                          -1,  // 27
                                                                          -1,  // 28
                                                                          -1,  // 29
                                                                          -1,  // 30
                                                                          -1   // 31
                                );
                                break;
                            }
                            case 6u:
                            {
                                needwrite_u8x32 = __builtin_shufflevector(::std::bit_cast<u8x32simd>(res),
                                                                          ::std::bit_cast<u8x32simd>(res),
                                                                          0,   // 0
                                                                          2,   // 1
                                                                          4,   // 2
                                                                          6,   // 3
                                                                          8,   // 4
                                                                          10,  // 5
                                                                          16,  // 6
                                                                          18,  // 7
                                                                          20,  // 8
                                                                          22,  // 9
                                                                          24,  // 10
                                                                          26,  // 11
                                                                          28,  // 12
                                                                          30,  // 13
                                                                          -1,  // 14
                                                                          -1,  // 15
                                                                          -1,  // 16
                                                                          -1,  // 17
                                                                          -1,  // 18
                                                                          -1,  // 19
                                                                          -1,  // 20
                                                                          -1,  // 21
                                                                          -1,  // 22
                                                                          -1,  // 23
                                                                          -1,  // 24
                                                                          -1,  // 25
                                                                          -1,  // 26
                                                                          -1,  // 27
                                                                          -1,  // 28
                                                                          -1,  // 29
                                                                          -1,  // 30
                                                                          -1   // 31
                                );
                                break;
                            }
                            case 7u:
                            {
                                needwrite_u8x32 = __builtin_shufflevector(::std::bit_cast<u8x32simd>(res),
                                                                          ::std::bit_cast<u8x32simd>(res),
                                                                          0,   // 0
                                                                          2,   // 1
                                                                          4,   // 2
                                                                          6,   // 3
                                                                          8,   // 4
                                                                          10,  // 5
                                                                          12,  // 6
                                                                          16,  // 7
                                                                          18,  // 8
                                                                          20,  // 9
                                                                          22,  // 10
                                                                          24,  // 11
                                                                          26,  // 12
                                                                          28,  // 13
                                                                          30,  // 14
                                                                          -1,  // 15
                                                                          -1,  // 16
                                                                          -1,  // 17
                                                                          -1,  // 18
                                                                          -1,  // 19
                                                                          -1,  // 20
                                                                          -1,  // 21
                                                                          -1,  // 22
                                                                          -1,  // 23
                                                                          -1,  // 24
                                                                          -1,  // 25
                                                                          -1,  // 26
                                                                          -1,  // 27
                                                                          -1,  // 28
                                                                          -1,  // 29
                                                                          -1,  // 30
                                                                          -1   // 31
                                );
                                break;
                            }
                            case 8u:
                            {
                                needwrite_u8x32 = __builtin_shufflevector(::std::bit_cast<u8x32simd>(res),
                                                                          ::std::bit_cast<u8x32simd>(res),
                                                                          0,   // 0
                                                                          2,   // 1
                                                                          4,   // 2
                                                                          6,   // 3
                                                                          8,   // 4
                                                                          10,  // 5
                                                                          12,  // 6
                                                                          14,  // 7
                                                                          16,  // 8
                                                                          18,  // 9
                                                                          20,  // 10
                                                                          22,  // 11
                                                                          24,  // 12
                                                                          26,  // 13
                                                                          28,  // 14
                                                                          30,  // 15
                                                                          -1,  // 16
                                                                          -1,  // 17
                                                                          -1,  // 18
                                                                          -1,  // 19
                                                                          -1,  // 20
                                                                          -1,  // 21
                                                                          -1,  // 22
                                                                          -1,  // 23
                                                                          -1,  // 24
                                                                          -1,  // 25
                                                                          -1,  // 26
                                                                          -1,  // 27
                                                                          -1,  // 28
                                                                          -1,  // 29
                                                                          -1,  // 30
                                                                          -1   // 31
                                );
                                break;
                            }
                            [[unlikely]] default:
                            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                                ::std::unreachable();
                            }
                        }

                        auto const needwrite_u8x16x2{::std::bit_cast<::uwvm2::utils::container::array<u8x16simd, 2uz>>(needwrite_u8x32)};

                        auto const needwrite_u8x16x2v0{needwrite_u8x16x2.front_unchecked()};

                        //  [... curr ... (15) ... curr_next ... (15) ...]
                        //  [                 safe                       ] unsafe (could be the section_end)
                        //       ^^ functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr
                        //      [    needwrite   ]

                        ::std::memcpy(functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr, ::std::addressof(needwrite_u8x16x2v0), sizeof(u8x16simd));

                        functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr += curr_table_processed_simd_sum;

                        //  [... curr ... (15) ... curr_next ... (15) ...]
                        //  [                 safe                       ] unsafe (could be the section_end)
                        //                    ^^ functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr
                        // Or: (Security boundaries for writes are checked)
                        //                        ^^ functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr
                        //      [    needwrite   ]

                        section_curr += curr_table_processed_byte_sum;

                        // [before_section ... | func_count ... typeidx1 ... (15) ... ... (15) ...] ...
                        // [                        safe                                          ] unsafe (could be the section_end)
                        //                                                   ^^^^^^^ section_curr (curr_table_processed_byte <= 8)
                        //                                      [   simd_vector_str               ]
                    }
                    else
                    {
                        // If check_table_index_1st or check_table_index_2nd is 0, write directly

                        // check func_counter before write
                        // The check is all done and the status can be changed, if it is less than the value written, then something is wrong.

                        ::uwvm2::parser::wasm::utils::counter_addass_throw_when_overflow(
                            func_counter,
                            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(16u),
                            section_curr,
                            err);

                        // check counter
                        if(func_counter > func_count) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_selectable.u32 = func_count;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        // Since everything is less than 128, there is no need to check the typeidx.

                        // write 16 byte
                        using u8x16simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint8_t;

                        auto const needwrite_u8x16x2{::std::bit_cast<::uwvm2::utils::container::array<u8x16simd, 2uz>>(third_fourth_round_simd_u8x32)};

                        //  XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX
                        // [                 needwrite_u16x8x2v0           ]

                        auto const needwrite_u8x16x2v0{needwrite_u8x16x2.front_unchecked()};

                        //  [... curr ... (15) ... curr_next ... (15) ...]
                        //  [                 safe                       ] unsafe (could be the section_end)
                        //       ^^ functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr
                        //      [    needwrite    ]

                        ::std::memcpy(functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr, ::std::addressof(needwrite_u8x16x2v0), sizeof(u8x16simd));

                        functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr += 16u;

                        //  [... curr_last ... (15) ... curr ... (15) ...]
                        //  [                  safe                      ] unsafe (could be the section_end)
                        //                              ^^ functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr

                        section_curr += 16u;

                        // [before_section ... | func_count ... typeidx1 ... (15) ... typeidxc ... (15) ...] ...
                        // [                                safe                                           ] unsafe (could be the section_end)
                        //                                                            ^^ section_curr
                        //                                      [                simd_vector_str           ]
                    }
                }
            }
            else
            {
                // All correct, can change state: func_counter, section_curr

                // all are single bytes, so there are 32
                ::uwvm2::parser::wasm::utils::counter_addass_throw_when_overflow(func_counter,
                                                                                 static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(32u),
                                                                                 section_curr,
                                                                                 err);

                // check counter
                if(func_counter > func_count) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_selectable.u32 = func_count;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                // Since everything is less than 128, there is no need to check the typeidx.

                // write data
                ::std::memcpy(functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr, ::std::addressof(simd_vector_str), sizeof(u8x32simd));

                functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr += 32uz;

                section_curr += 32uz;

                // [before_section ... | func_count ... typeidx1 ... (31) ...] typeidx32
                // [                        safe                             ] unsafe (could be the section_end)
                //                                                             ^^ section_curr
            }

            // After testing, 6 and 4 are the fastest

            ::uwvm2::utils::intrinsics::universal::prefetch<::uwvm2::utils::intrinsics::universal::pfc_mode::read,
                                                            ::uwvm2::utils::intrinsics::universal::pfc_level::nta,
                                                            ::uwvm2::utils::intrinsics::universal::ret_policy::strm>(
                reinterpret_cast<::std::byte const*>(section_curr) + 64u * 6u);
            ::uwvm2::utils::intrinsics::universal::prefetch<::uwvm2::utils::intrinsics::universal::pfc_mode::write,
                                                            ::uwvm2::utils::intrinsics::universal::pfc_level::L2,
                                                            ::uwvm2::utils::intrinsics::universal::ret_policy::keep>(
                reinterpret_cast<::std::byte const*>(functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr) + 64u * 4u);
        }

#elif UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__) && UWVM_HAS_BUILTIN(__builtin_shufflevector) &&                           \
    (((defined(__SSSE3__) && UWVM_HAS_BUILTIN(__builtin_ia32_pshufb128) && UWVM_HAS_BUILTIN(__builtin_ia32_palignr128)) &&                                     \
      (defined(__SSE2__) && UWVM_HAS_BUILTIN(__builtin_ia32_pmovmskb128))) ||                                                                                  \
     (defined(__loongarch_sx) && UWVM_HAS_BUILTIN(__builtin_lsx_vshuf_b) && UWVM_HAS_BUILTIN(__builtin_lsx_vsrli_b) &&                                         \
      UWVM_HAS_BUILTIN(__builtin_lsx_vmskltz_b)))

        /// (Little Endian), [[gnu::vector_size]], has mask-u16, can shuffle, simd128
        /// x86_64-ssse3, loongarch-SX

        // This error handle is provided when simd is unable to handle the error, including when it encounters a memory boundary, or when there is an error that
        // needs to be localized.

        auto error_handler{[&](::std::size_t n) constexpr UWVM_THROWS -> void
                           {
                               // Need to ensure that section_curr to section_curr + n is memory safe
                               // Processing uses section_curr_end to guarantee entry into the loop for inspection.
                               // Also use section_end to ensure that the scanning of the last leb128 is handled correctly, both memory safe.

                               auto const section_curr_end{section_curr + n};

            // [ ... typeidx1 ... ] (section_curr_end) ... (outer) ]
            // [     safe     ... ]                    ... (outer) ] unsafe (could be the section_end)
            //       ^^ section_curr

            // Since parse_by_scan uses section_end, it is possible that section_curr will be greater than section_curr_end, use '<' instead
            // of '!='

# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                               // Used to check if correct content is recognized as an error.
                               bool correct_sequence_right_taken_for_wrong{true};
# endif

                               while(section_curr < section_curr_end)
                               {
                                   // The outer boundary is unknown and needs to be rechecked
                                   // [ ... typeidx1] ... (outer) ] typeidx2 ...
                                   // [     safe    ] ... (outer) ] unsafe (could be the section_end)
                                   //       ^^ section_curr

                                   if(::uwvm2::parser::wasm::utils::counter_selfinc_throw_when_overflow(func_counter, section_curr, err) > func_count)
                                       [[unlikely]]
                                   {
                                       err.err_curr = section_curr;
                                       err.err_selectable.u32 = func_count;
                                       err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                                       ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                   }

                                   ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 typeidx;  // No initialization necessary

                                   using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                                   auto const [typeidx_next, typeidx_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                                                   reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                                                   ::fast_io::mnp::leb128_get(typeidx))};

                                   if(typeidx_err != ::fast_io::parse_code::ok) [[unlikely]]
                                   {
                                       err.err_curr = section_curr;
                                       err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_type_index;
                                       ::uwvm2::parser::wasm::base::throw_wasm_parse_code(typeidx_err);
                                   }

                                   // The outer boundary is unknown and needs to be rechecked
                                   // [ ... typeidx1 ...] ... (outer) ] typeidx2 ...
                                   // [      safe       ] ... (outer) ] unsafe (could be the section_end)
                                   //       ^^ section_curr

                                   // There's a good chance there's an error here.
                                   // Or there is a leb redundancy 0
                                   if(typeidx >= type_section_count) [[unlikely]]
                                   {
                                       err.err_curr = section_curr;
                                       err.err_selectable.u32 = typeidx;
                                       err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_type_index;
                                       ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                   }

                                   // write data, func_counter has been checked and is ready to be written.
                                   functionsec.funcs.storage.typeidx_u8_vector.emplace_back_unchecked(
                                       static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8>(typeidx));

# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                   if(static_cast<::std::size_t>(reinterpret_cast<::std::byte const*>(typeidx_next) - section_curr) > 2uz) [[unlikely]]
                                   {
                                       correct_sequence_right_taken_for_wrong = false;
                                   }
# endif

                                   section_curr = reinterpret_cast<::std::byte const*>(typeidx_next);

                                   // The outer boundary is unknown and needs to be rechecked
                                   // [ ... typeidx1 ...] typeidx2 ...] ...
                                   // [      safe       ] ... (outer) ] unsafe (could be the section_end)
                                   //                     ^^ section_curr
                               }

            // [before_section ... | func_count ... typeidx1 ... (n - 1) ... ...  ] typeidxN
            // [                        safe                                      ] unsafe (could be the section_end)
            //                                                                      ^^ section_curr
            //                                      [   simd_vector_str  ]   ...  ] (Depends on the size of section_curr in relation to
            //                                      section_curr_end)

# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                               if(correct_sequence_right_taken_for_wrong) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif
                           }};

        using i64x2simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::int64_t;
        using u64x2simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint64_t;
        using u32x4simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint32_t;
        using u16x8simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint16_t;
        using c8x16simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = char;
        using u8x16simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint8_t;
        using i8x16simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::int8_t;

        static_assert(::std::same_as<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte, ::std::uint8_t>);

        // No prefetching of table entries is required, ensuring no contamination of the cache

        while(static_cast<::std::size_t>(section_end - section_curr) >= 16uz) [[likely]]
        {
            // [before_section ... | func_count ... typeidx1 ... (15) ...] ...
            // [                        safe                             ] unsafe (could be the section_end)
            //                                      ^^ section_curr
            //                                      [   simd_vector_str  ]

            u8x16simd simd_vector_str;  // No initialization necessary

            ::std::memcpy(::std::addressof(simd_vector_str), section_curr, sizeof(u8x16simd));

            // It's already a little-endian.

            // When the highest bit of each byte is pop, then mask out 1
            auto const check_mask{
# if defined(__SSE2__) && UWVM_HAS_BUILTIN(__builtin_ia32_pmovmskb128)
                static_cast<unsigned>(__builtin_ia32_pmovmskb128(::std::bit_cast<c8x16simd>(simd_vector_str)))
# elif defined(__loongarch_sx) && UWVM_HAS_BUILTIN(__builtin_lsx_vmskltz_b)
                ::std::bit_cast<::uwvm2::utils::container::array<unsigned, 4uz>>(__builtin_lsx_vmskltz_b(::std::bit_cast<i8x16simd>(simd_vector_str)))
                    .front_unchecked()
# else
#  error "missing instructions"
# endif
            };

            // Since mask 16-bit stuff, it can be assumed that mask is less than 2^16
            UWVM_ASSERT(check_mask < 1u << 16u);
            [[assume(check_mask < 1u << 16u)]];

            if(
# if UWVM_HAS_BUILTIN(__builtin_expect_with_probability)
                __builtin_expect_with_probability(static_cast<bool>(check_mask), true, 1.0 / 4.0)
# else
                check_mask
# endif
            )
            {
                ::std::uint16_t const simd_vector_check{static_cast<::std::uint16_t>(type_section_count)};

                auto check_mask_curr{check_mask};  // uleb128 mask

                // Record the number of bytes processed in the first round
                ::std::uint8_t first_round_handle_bytes{static_cast<::std::uint8_t>(8u)};

                // 1st round
                {
                    unsigned const check_table_index{check_mask_curr & 0xFFu};

                    // If check_mask_curr is 0, write directly

                    if(!check_table_index)
                    {
                        // All correct, can change state: func_counter, section_curr

                        // The first 8 bits are single byte typeidx is writable at once.

                        check_mask_curr >>= 8u;

                        // check func_counter before write
                        ::uwvm2::parser::wasm::utils::counter_addass_throw_when_overflow(
                            func_counter,
                            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(8u),
                            section_curr,
                            err);

                        // check counter
                        if(func_counter > func_count) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_selectable.u32 = func_count;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        // Since everything is less than 128, there is no need to check the typeidx.

                        // write 8 byte
                        using u8x8simd [[__gnu__::__vector_size__(8)]] [[maybe_unused]] = ::std::uint8_t;

                        auto const needwrite_u8x8x2{::std::bit_cast<::uwvm2::utils::container::array<u8x8simd, 2uz>>(simd_vector_str)};

                        //  XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX
                        // [   needwrite_u8x8x2v0  ]

                        auto const needwrite_u8x8x2v0{needwrite_u8x8x2.front_unchecked()};

                        //  [... curr ... (7) ... curr_next ... (7) ...]
                        //  [                 safe                     ] unsafe (could be the section_end)
                        //       ^^ functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr
                        //      [    needwrite   ]

                        ::std::memcpy(functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr, ::std::addressof(needwrite_u8x8x2v0), sizeof(u8x8simd));

                        functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr += 8u;

                        //  [... curr_last ... (7) ... curr ... (7) ...]
                        //  [                  safe                    ] unsafe (could be the section_end)
                        //                             ^^ functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr

                        section_curr += 8u;

                        // [before_section ... | func_count ... typeidx1 ... (7) ... typeidxc ... (7) ...] ...
                        // [                                safe                                         ] unsafe (could be the section_end)
                        //                                                           ^^ section_curr
                        //                                      [                simd_vector_str         ]
                    }
                    else
                    {
                        //  [...] curr ...
                        //  [sf ] unsafe (could be the section_end)
                        //        ^^ functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr

                        // The write size is 8 bits, but the valid data may be less than 8 bits, directly check the maximum value, more than that, then enter
                        // the tail processing Cannot change state until error checking: func_counter, section_curr

                        auto const func_counter_end{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(func_counter + 8u)};

                        if(func_counter_end < func_counter) [[unlikely]]
                        {
                            // It's overflowing. It's jumping right out.
                            break;
                        }

                        if(func_counter_end > func_count) [[unlikely]]
                        {
                            // Near the end, jump directly out of the simd section and hand it over to the tail.
                            break;
                        }

                        //  [... curr ... (7) ...]
                        //  [        safe        ] unsafe (could be the section_end)
                        //       ^^ functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr
                        //      [ needwrite ] ...] (Always writes 8 bits, but valid data may be less than 8 bits)

                        auto const& curr_table{simd128_shuffle_table.index_unchecked(static_cast<::std::size_t>(check_table_index))};
                        auto const curr_table_shuffle_mask{curr_table.shuffle_mask};
                        auto const curr_table_processed_simd{curr_table.processed_simd};  // size of handled u32
                        auto const curr_table_processed_byte{curr_table.processed_byte};  // size of handled uleb128

                        first_round_handle_bytes = static_cast<::std::uint8_t>(curr_table_processed_byte);

                        // When the number of consecutive bits is greater than 2, switch back to the normal processing method
                        if(!curr_table_processed_simd) [[unlikely]]
                        {
                            // [before_section ... | func_count ... typeidx1 ... (15) ...] ...
                            // [                        safe                             ] unsafe (could be the section_end)
                            //                                      ^^ section_curr
                            //                                                             ^^ section_curr + 16uz

                            // If not yet processed in the first round, it can be processed up to 16
                            error_handler(16uz);
                            // Start the next round straight away
                            continue;
                        }

                        // Remove the second round of processing for the next round of calculations
                        check_mask_curr >>= curr_table_processed_byte;

                        //  [... curr ... (7) ... curr_next ... (7) ...]
                        //  [                 safe                     ] unsafe (could be the section_end)
                        //       ^^ functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr
                        //      [    needwrite   ]
                        //      [    ctps  ]  ...]

                        // shuffle and write

                        u16x8simd mask_res;

# if defined(__SSSE3__) && UWVM_HAS_BUILTIN(__builtin_ia32_pshufb128)
                        mask_res = ::std::bit_cast<u16x8simd>(__builtin_ia32_pshufb128(simd_vector_str, curr_table_shuffle_mask));
# elif defined(__loongarch_sx) && UWVM_HAS_BUILTIN(__builtin_lsx_vshuf_b)
                        mask_res = ::std::bit_cast<u16x8simd>(__builtin_lsx_vshuf_b(simd_vector_str, simd_vector_str, curr_table_shuffle_mask));
# else
#  error "missing instructions"
# endif

                        // The leb128 that has been merged into u16x8 is finalized to get the value corresponding to u16

                        auto const res{(mask_res & static_cast<::std::uint16_t>(0x7Fu)) | ((mask_res & static_cast<::std::uint16_t>(0x7F00u)) >> 1u)};

                        // The data out of shuffle is 16-bit [0, 2^14) and may be greater than or equal to typeidx, which needs to be checked.

                        auto const check_upper{res >= simd_vector_check};

                        if(
# if defined(__SSE4_1__) && UWVM_HAS_BUILTIN(__builtin_ia32_ptestz128)
                            !__builtin_ia32_ptestz128(::std::bit_cast<i64x2simd>(check_upper), ::std::bit_cast<i64x2simd>(check_upper))
# elif defined(__SSE2__) && UWVM_HAS_BUILTIN(__builtin_ia32_pmovmskb128)
                            __builtin_ia32_pmovmskb128(::std::bit_cast<c8x16simd>(check_upper))
# elif defined(__loongarch_sx) && UWVM_HAS_BUILTIN(__builtin_lsx_bnz_v)
                            __builtin_lsx_bnz_v(::std::bit_cast<u8x16simd>(check_upper))  /// @todo need check
# else
#  error "missing instructions"
# endif
                                ) [[unlikely]]
                        {
                            // [before_section ... | func_count ... typeidx1 ... (15) ...] ...
                            // [                        safe                             ] unsafe (could be the section_end)
                            //                                      ^^ section_curr
                            //                                                             ^^ section_curr + 16uz

                            // If not yet processed in the first round, it can be processed up to 16
                            // Ensure that there are no modifications to the state: func_counter, section_curr
                            error_handler(16uz);
                            // Start the next round straight away
                            continue;
                        }

                        // All correct, can change state: func_counter, section_curr

                        // check func_counter before write
                        // Checked. No overflow.
                        func_counter += curr_table_processed_simd;

                        // There is no need to check function_counter, because curr_table_processed_simd is always less than or equal to 8u.

                        // Since typeidx can write to a u8, reduce u16x8 to u8x8

                        // Because the value will be overwritten, use -1 to indicate that any value can be written by the fastest means possible.
                        auto const needwrite_u8x16{::std::bit_cast<u8x16simd>(__builtin_shufflevector(::std::bit_cast<u8x16simd>(res),
                                                                                                      ::std::bit_cast<u8x16simd>(res),
                                                                                                      0,   // 0
                                                                                                      2,   // 1
                                                                                                      4,   // 2
                                                                                                      6,   // 3
                                                                                                      8,   // 4
                                                                                                      10,  // 5
                                                                                                      12,  // 6
                                                                                                      14,  // 7
                                                                                                      -1,  // 8
                                                                                                      -1,  // 9
                                                                                                      -1,  // 10
                                                                                                      -1,  // 11
                                                                                                      -1,  // 12
                                                                                                      -1,  // 13
                                                                                                      -1,  // 14
                                                                                                      -1   // 15
                                                                                                      ))};

                        using u8x8simd [[__gnu__::__vector_size__(8)]] [[maybe_unused]] = ::std::uint8_t;
                        auto const needwrite_u8x8x2{::std::bit_cast<::uwvm2::utils::container::array<u8x8simd, 2uz>>(needwrite_u8x16)};

                        auto const needwrite_u8x8x2v0{needwrite_u8x8x2.front_unchecked()};

                        //  [... curr ... (7) ... curr_next ... (7) ...]
                        //  [                 safe                     ] unsafe (could be the section_end)
                        //       ^^ functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr
                        //      [    needwrite   ]

                        ::std::memcpy(functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr, ::std::addressof(needwrite_u8x8x2v0), sizeof(u8x8simd));

                        functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr += curr_table_processed_simd;

                        //  [... curr ... (7) ... curr_next ... (7) ...]
                        //  [                 safe                     ] unsafe (could be the section_end)
                        //                    ^^ functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr
                        // Or: (Security boundaries for writes are checked)
                        //                        ^^ functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr
                        //      [    needwrite   ]

                        section_curr += curr_table_processed_byte;

                        // [before_section ... | func_count ... typeidx1 ... (7) ... ... (7) ...] ...
                        // [                        safe                                        ] unsafe (could be the section_end)
                        //                                                   ^^^^^^^ section_curr (curr_table_processed_byte <= 8)
                        //                                      [   simd_vector_str             ]
                    }
                }

                // 2nd round
                {
                    unsigned const check_table_index{check_mask_curr & 0xFFu};

                    // There are only 7 and 8 and 0 in the table, where the 0 have been handled out of the error,
                    // and if a non-7 or non-8 is encountered, there is a problem with the surface

                    // When first_round_handle_bytes is 7, check_table_index is always greater than 0，
                    // Because the highest bit of the first 8 bits is pop, 0bxxxx'xxxx'1xxx'xxxxu >> 7u == 0bxxxx'xxx1
                    UWVM_ASSERT((first_round_handle_bytes == static_cast<::std::uint8_t>(7u) && check_table_index != 0u) ||
                                first_round_handle_bytes == static_cast<::std::uint8_t>(8u));
                    [[assume((first_round_handle_bytes == static_cast<::std::uint8_t>(7u) && check_table_index != 0u) ||
                             first_round_handle_bytes == static_cast<::std::uint8_t>(8u))]];

                    if(!check_table_index)
                    {
                        // It's the last round, but since it's already established, it's better to calculate it directly.

                        // All correct, can change state: func_counter, section_curr

                        // The first 8 bits are single byte typeidx is writable at once.

                        // last round no necessary to check_mask_curr >>= 8u;

                        // check func_counter before write
                        ::uwvm2::parser::wasm::utils::counter_addass_throw_when_overflow(
                            func_counter,
                            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(8u),
                            section_curr,
                            err);

                        // check counter
                        if(func_counter > func_count) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_selectable.u32 = func_count;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        // Since everything is less than 128, there is no need to check the typeidx.

                        // Requires a vector right shift to implement

                        //  XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX
                        // [       last write      ]
                        //                      ^^^^^^ curr (begin + first_round_handle_bytes)
                        //                      [      need write       ]..]

                        // When first_round_handle_bytes is 7, check_table_index is always greater than 0，
                        // Because the highest bit of the first 8 bits is pop, 0bxxxx'xxxx'1xxx'xxxxu >> 7u == 0bxxxx'xxx1

                        UWVM_ASSERT(first_round_handle_bytes == static_cast<::std::uint8_t>(8u));

                        // Here the shuffle simulation element is kept shifted right by 8 bits to facilitate subsequent expansion,
                        // while the subsequent taking of the first address has been shown to be optimized by the llvm into a single instruction

                        u8x16simd const simd_vector_str_need_shf{
                            __builtin_shufflevector(simd_vector_str, simd_vector_str, 8, 9, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1)};

                        // write 8 byte
                        using u8x8simd [[__gnu__::__vector_size__(8)]] [[maybe_unused]] = ::std::uint8_t;

                        auto const needwrite_u8x8x2{::std::bit_cast<::uwvm2::utils::container::array<u8x8simd, 2uz>>(simd_vector_str_need_shf)};

                        //  XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX
                        // [       last write      ]
                        //                      ^^^^^^ curr (begin + first_round_handle_bytes)
                        //                      [      need write       ]..]

                        auto const needwrite_u8x8x2v0{needwrite_u8x8x2.front_unchecked()};

                        //  [... curr_old ... (7) ... curr ... (7) ...] ...
                        //  [                 safe                    ] unsafe (could be the section_end)
                        //                        ^^^^^^ functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr
                        //                       [    needwrite   ]...]

                        ::std::memcpy(functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr, ::std::addressof(needwrite_u8x8x2v0), sizeof(u8x8simd));

                        functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr += 8u;

                        //  [... curr_last ... (7) ... curr ... (7) ...] ...
                        //  [                  safe                    ] unsafe (could be the section_end)
                        //                                           ^^^^^^ functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr

                        section_curr += 8u;

                        // [before_section ... | func_count ... typeidx1 ... (7) ... typeidxc ... (7) ...] ...
                        // [                                safe                                         ] unsafe (could be the section_end)
                        //                                                                             ^^^^^^ section_curr
                        //                                      [                simd_vector_str         ]
                    }
                    else
                    {
                        // The write size is 8 bits, but the valid data may be less than 8 bits, directly check the maximum value, more than that, then enter
                        // the tail processing

                        //  [...] curr ... (curr + 8u)
                        //  [sf ] unsafe (could be the section_end)
                        //  [ safe  ... ] unsafe (Left over from above, length unknown, can't be used)
                        //        ^^ functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr

                        // Cannot change state until error checking: func_counter, section_curr

                        auto const func_counter_end{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(func_counter + 8u)};

                        if(func_counter_end < func_counter) [[unlikely]]
                        {
                            // It's overflowing. It's jumping right out.
                            break;
                        }

                        if(func_counter_end > func_count) [[unlikely]]
                        {
                            // Near the end, jump directly out of the simd section and hand it over to the tail.
                            break;
                        }

                        //  [... curr ... (7) ...]
                        //  [        safe        ] unsafe (could be the section_end)
                        //       ^^ functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr
                        //      [ needwrite ] ...] (Always writes 8 bits, but valid data may be less than 8 bits)

                        auto const& curr_table{simd128_shuffle_table.index_unchecked(static_cast<::std::size_t>(check_table_index))};
                        // Since it's the second round, the data has to be moved back  the number of bytes processed in the first round
                        auto const curr_table_shuffle_mask{curr_table.shuffle_mask + first_round_handle_bytes};
                        auto const curr_table_processed_simd{curr_table.processed_simd};  // size of handled u32
                        auto const curr_table_processed_byte{curr_table.processed_byte};  // size of handled uleb128

                        // When the number of consecutive bits is greater than 2, switch back to the normal processing method
                        if(!curr_table_processed_simd) [[unlikely]]
                        {
                            // [before_section ... | func_count ... typeidx1 ... (7) ... ... (7) ...] ...
                            // [                        safe                                        ] unsafe (could be the section_end)
                            //                                                   ^^^ ^^^ section_curr (indeterminate location)
                            //                                                                   ^^^ ^^^ section_curr + 8uz

                            // Second round can only handle 16 - first round max 8 = 8
                            // Ensure that there are no modifications to the state: func_counter, section_curr
                            error_handler(8uz);
                            // Start the next round straight away
                            continue;
                        }

                        // last round no necessary to check_mask_curr >>= curr_table_processed_byte;

                        // shuffle and write

                        u16x8simd mask_res;

# if defined(__SSSE3__) && UWVM_HAS_BUILTIN(__builtin_ia32_pshufb128)
                        mask_res = ::std::bit_cast<u16x8simd>(__builtin_ia32_pshufb128(simd_vector_str, curr_table_shuffle_mask));
# elif defined(__loongarch_sx) && UWVM_HAS_BUILTIN(__builtin_lsx_vshuf_b)
                        mask_res = ::std::bit_cast<u16x8simd>(__builtin_lsx_vshuf_b(simd_vector_str, simd_vector_str, curr_table_shuffle_mask));
# else
#  error "missing instructions"
# endif

                        // The leb128 that has been merged into u16x8 is finalized to get the value corresponding to u16

                        auto const res{(mask_res & static_cast<::std::uint16_t>(0x7Fu)) | ((mask_res & static_cast<::std::uint16_t>(0x7F00u)) >> 1u)};

                        // The data out of shuffle is 16-bit [0, 2^14) and may be greater than or equal to typeidx, which needs to be checked.

                        auto const check_upper{res >= simd_vector_check};

                        if(
# if defined(__SSE4_1__) && UWVM_HAS_BUILTIN(__builtin_ia32_ptestz128)
                            !__builtin_ia32_ptestz128(::std::bit_cast<i64x2simd>(check_upper), ::std::bit_cast<i64x2simd>(check_upper))
# elif defined(__SSE2__) && UWVM_HAS_BUILTIN(__builtin_ia32_pmovmskb128)
                            __builtin_ia32_pmovmskb128(::std::bit_cast<c8x16simd>(check_upper))
# elif defined(__loongarch_sx) && UWVM_HAS_BUILTIN(__builtin_lsx_bnz_v)
                            __builtin_lsx_bnz_v(::std::bit_cast<u8x16simd>(check_upper))  /// @todo need check
# else
#  error "missing instructions"
# endif
                                ) [[unlikely]]
                        {
                            // [before_section ... | func_count ... typeidx1 ... (7) ... ... (7) ...] ...
                            // [                        safe                                        ] unsafe (could be the section_end)
                            //                                                   ^^^^^^^ section_curr (indeterminate location)
                            //                                                                   ^^^^^^^ section_curr + 8uz

                            // Second round can only handle 16 - first round max 8 = 8
                            error_handler(8uz);
                            // Start the next round straight away
                            continue;
                        }

                        // All correct, can change state: func_counter, section_curr

                        // check func_counter before write
                        // Checked. No overflow.
                        func_counter += curr_table_processed_simd;

                        // There is no need to check function_counter, because curr_table_processed_simd is always less than or equal to 8u.

                        // Since typeidx can write to a u8, reduce u16x8 to u8x8

                        // Because the value will be overwritten, use -1 to indicate that any value can be written by the fastest means possible.
                        auto const needwrite_u8x16{::std::bit_cast<u8x16simd>(__builtin_shufflevector(::std::bit_cast<u8x16simd>(res),
                                                                                                      ::std::bit_cast<u8x16simd>(res),
                                                                                                      0,   // 0
                                                                                                      2,   // 1
                                                                                                      4,   // 2
                                                                                                      6,   // 3
                                                                                                      8,   // 4
                                                                                                      10,  // 5
                                                                                                      12,  // 6
                                                                                                      14,  // 7
                                                                                                      -1,  // 8
                                                                                                      -1,  // 9
                                                                                                      -1,  // 10
                                                                                                      -1,  // 11
                                                                                                      -1,  // 12
                                                                                                      -1,  // 13
                                                                                                      -1,  // 14
                                                                                                      -1   // 15
                                                                                                      ))};

                        using u8x8simd [[__gnu__::__vector_size__(8)]] [[maybe_unused]] = ::std::uint8_t;
                        auto const needwrite_u8x8x2{::std::bit_cast<::uwvm2::utils::container::array<u8x8simd, 2uz>>(needwrite_u8x16)};

                        auto const needwrite_u8x8x2v0{needwrite_u8x8x2.front_unchecked()};

                        //  [... curr_old ... (7) ... curr ... (7) ... ]
                        //  [                 safe                     ] unsafe (could be the section_end)
                        //                            ^^ functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr
                        //                            [    needwrite   ]

                        ::std::memcpy(functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr, ::std::addressof(needwrite_u8x8x2v0), sizeof(u8x8simd));

                        functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr += curr_table_processed_simd;

                        //  [... curr_old ... (7) ... curr ... (7) ... ]
                        //  [                 safe                     ] unsafe (could be the section_end)
                        //                                               ^^ functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr
                        //                            [    needwrite   ]

                        section_curr += curr_table_processed_byte;

                        // [before_section ... | func_count ... typeidx1 ... (7) ... ... (7) ...] ...
                        // [                        safe                                        ] unsafe (could be the section_end)
                        //                                                                   ^^^^^^^ section_curr (curr_table_processed_byte <= 8)
                        //                                      [   simd_vector_str             ]
                    }
                }
            }
            else
            {
                // All correct, can change state: func_counter, section_curr

                // all are single bytes, so there are 16
                ::uwvm2::parser::wasm::utils::counter_addass_throw_when_overflow(func_counter,
                                                                                 static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(16u),
                                                                                 section_curr,
                                                                                 err);

                // check counter
                if(func_counter > func_count) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_selectable.u32 = func_count;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                // Since everything is less than 128, there is no need to check the typeidx.

                // write data
                ::std::memcpy(functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr, ::std::addressof(simd_vector_str), sizeof(u8x16simd));

                functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr += 16uz;

                section_curr += 16uz;

                // [before_section ... | func_count ... typeidx1 ... (15) ...] typeidx16
                // [                        safe                             ] unsafe (could be the section_end)
                //                                                             ^^ section_curr
            }

            // After testing, 4 and 3 are the fastest

            ::uwvm2::utils::intrinsics::universal::prefetch<::uwvm2::utils::intrinsics::universal::pfc_mode::read,
                                                            ::uwvm2::utils::intrinsics::universal::pfc_level::nta,
                                                            ::uwvm2::utils::intrinsics::universal::ret_policy::strm>(
                reinterpret_cast<::std::byte const*>(section_curr) + 64u * 4u);
            ::uwvm2::utils::intrinsics::universal::prefetch<::uwvm2::utils::intrinsics::universal::pfc_mode::write,
                                                            ::uwvm2::utils::intrinsics::universal::pfc_level::L2,
                                                            ::uwvm2::utils::intrinsics::universal::ret_policy::keep>(
                reinterpret_cast<::std::byte const*>(functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr) + 64u * 3u);
        }

#elif UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__) &&                                                                        \
    ((defined(__SSE2__) && UWVM_HAS_BUILTIN(__builtin_ia32_pmovmskb128)) ||                                                                                    \
     (defined(__ARM_NEON) && (UWVM_HAS_BUILTIN(__builtin_neon_vmaxvq_u32) || UWVM_HAS_BUILTIN(__builtin_aarch64_reduc_umax_scal_v4si_uu))) ||                  \
     (defined(__wasm_simd128__) && (UWVM_HAS_BUILTIN(__builtin_wasm_bitmask_i8x16) || UWVM_HAS_BUILTIN(__builtin_wasm_all_true_i8x16))))
        /// (Little Endian), [[gnu::vector_size]], no mask-u16, simd128
        ///  x86_64-sse2, aarch64-neon, wasm-simd128 (Since the wasm simd128 shuffle requirement is immediate, it cannot be used.)
        ///  Since the arm neon does not have a move mask, the shuffle algorithm cannot be used.

        using i64x2simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::int64_t;
        using u64x2simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint64_t;
        using u32x4simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint32_t;
        using c8x16simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = char;
        using u8x16simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint8_t;
        using i8x16simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::int8_t;

        static_assert(::std::same_as<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte, ::std::uint8_t>);

        while(static_cast<::std::size_t>(section_end - section_curr) >= 16uz) [[likely]]
        {
            // [before_section ... | func_count ... typeidx1 ... (15) ...] ...
            // [                        safe                             ] unsafe (could be the section_end)
            //                                      ^^ section_curr
            //                                      [   simd_vector_str  ]

            u8x16simd simd_vector_str;  // No initialization necessary

            ::std::memcpy(::std::addressof(simd_vector_str), section_curr, sizeof(u8x16simd));

            // It's already a little-endian.

            // Using "x > 127u" instead of "x & 0x80u" guarantees that the result is fully bit-set. The rest is left to the compiler to optimize
            auto const check_highest_bit{simd_vector_str > static_cast<::std::uint8_t>(127u)};

            auto const has_pop_highest_bit{
# if defined(__SSE2__) && UWVM_HAS_BUILTIN(__builtin_ia32_pmovmskb128)
                __builtin_ia32_pmovmskb128(::std::bit_cast<c8x16simd>(check_highest_bit))
# elif defined(__ARM_NEON) && UWVM_HAS_BUILTIN(__builtin_neon_vmaxvq_u32)                  // Only supported by clang
                __builtin_neon_vmaxvq_u32(::std::bit_cast<u32x4simd>(check_highest_bit))
# elif defined(__ARM_NEON) && UWVM_HAS_BUILTIN(__builtin_aarch64_reduc_umax_scal_v4si_uu)  // Only supported by GCC
                __builtin_aarch64_reduc_umax_scal_v4si_uu(::std::bit_cast<u32x4simd>(check_highest_bit))
# elif defined(__wasm_simd128__) && UWVM_HAS_BUILTIN(__builtin_wasm_all_true_i8x16)
                !__builtin_wasm_all_true_i8x16(::std::bit_cast<i8x16simd>(~check_highest_bit))
# elif defined(__wasm_simd128__) && UWVM_HAS_BUILTIN(__builtin_wasm_bitmask_i8x16)
                __builtin_wasm_bitmask_i8x16(::std::bit_cast<i8x16simd>(check_highest_bit))
# else
#  error "missing instructions"
# endif
            };

            if(
# if UWVM_HAS_BUILTIN(__builtin_expect_with_probability)
                __builtin_expect_with_probability(static_cast<bool>(has_pop_highest_bit), true, 1.0 / 4.0)
# else
                has_pop_highest_bit
# endif
            )
            {
                auto const section_curr_end{section_curr + 16u};

                // Since parse_by_scan uses section_end, it is possible that section_curr will be greater than section_curr_end, use '<' instead of '!='

                while(section_curr < section_curr_end)
                {
                    // The outer boundary is unknown and needs to be rechecked
                    // [ ... typeidx1] ... (outer) ] typeidx2 ...
                    // [     safe    ] ... (outer) ] unsafe (could be the section_end)
                    //       ^^ section_curr

                    if(::uwvm2::parser::wasm::utils::counter_selfinc_throw_when_overflow(func_counter, section_curr, err) > func_count) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_selectable.u32 = func_count;
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 typeidx;  // No initialization necessary

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    auto const [typeidx_next, typeidx_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                                    reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                                    ::fast_io::mnp::leb128_get(typeidx))};

                    if(typeidx_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_type_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(typeidx_err);
                    }

                    // The outer boundary is unknown and needs to be rechecked
                    // [ ... typeidx1 ...] ... (outer) ] typeidx2 ...
                    // [      safe       ] ... (outer) ] unsafe (could be the section_end)
                    //       ^^ section_curr

                    // There's a good chance there's an error here.
                    // Or there is a leb redundancy 0
                    if(typeidx >= type_section_count) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_selectable.u32 = typeidx;
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_type_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // write data, func_counter has been checked and is ready to be written.
                    functionsec.funcs.storage.typeidx_u8_vector.emplace_back_unchecked(
                        static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8>(typeidx));

                    section_curr = reinterpret_cast<::std::byte const*>(typeidx_next);

                    // The outer boundary is unknown and needs to be rechecked
                    // [ ... typeidx1 ...] typeidx2 ...] ...
                    // [      safe       ] ... (outer) ] unsafe (could be the section_end)
                    //                     ^^ section_curr
                }

                // [before_section ... | func_count ... typeidx1 ... (15) ... ...  ] typeidxN
                // [                        safe                                   ] unsafe (could be the section_end)
                //                                                                   ^^ section_curr
                //                                      [   simd_vector_str  ] ... ] (Depends on the size of section_curr in relation to section_curr_end)
            }
            else
            {
                // all are single bytes, so there are 16
                ::uwvm2::parser::wasm::utils::counter_addass_throw_when_overflow(func_counter,
                                                                                 static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(16u),
                                                                                 section_curr,
                                                                                 err);

                // check counter
                if(func_counter > func_count) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_selectable.u32 = func_count;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                // Since everything is less than 128, there is no need to check the typeidx.

                // write data
                ::std::memcpy(functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr, ::std::addressof(simd_vector_str), sizeof(u8x16simd));

                functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr += 16uz;

                section_curr += 16uz;

                // [before_section ... | func_count ... typeidx1 ... (15) ...] typeidx16
                // [                        safe                             ] unsafe (could be the section_end)
                //                                                             ^^ section_curr
            }

            // After testing, 4 and 3 are the fastest

            ::uwvm2::utils::intrinsics::universal::prefetch<::uwvm2::utils::intrinsics::universal::pfc_mode::read,
                                                            ::uwvm2::utils::intrinsics::universal::pfc_level::nta,
                                                            ::uwvm2::utils::intrinsics::universal::ret_policy::strm>(
                reinterpret_cast<::std::byte const*>(section_curr) + 64u * 4u);
            ::uwvm2::utils::intrinsics::universal::prefetch<::uwvm2::utils::intrinsics::universal::pfc_mode::write,
                                                            ::uwvm2::utils::intrinsics::universal::pfc_level::L2,
                                                            ::uwvm2::utils::intrinsics::universal::ret_policy::keep>(
                reinterpret_cast<::std::byte const*>(functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr) + 64u * 3u);
        }
#endif

        // tail handling
        /// @todo support AVX512vbmi2
        /// @todo support sve2

#if defined(_MSC_VER) && !defined(__clang__)
        // MSVC, x86_64, i386, aarch64, arm, arm64ec, ...
        // msvc, avoid macro-contamination

        while(section_curr != section_end) [[likely]]
        {
            // Ensuring the existence of valid information

            // [ ... typeidx1] ... typeidx2 ...
            // [     safe    ] unsafe (could be the section_end)
            //       ^^ section_curr

            // check function counter
            // Ensure content is available before counting (section_curr != section_end)
            if(::uwvm2::parser::wasm::utils::counter_selfinc_throw_when_overflow(func_counter, section_curr, err) > func_count) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u32 = func_count;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // [ ... typeidx1] ... typeidx2 ...
            // [     safe    ] unsafe (could be the section_end)
            //       ^^ section_curr

            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 typeidx;  // No initialization necessary

            using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

            auto const [typeidx_next, typeidx_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                            ::fast_io::mnp::leb128_get(typeidx))};

            if(typeidx_err != ::fast_io::parse_code::ok) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_type_index;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(typeidx_err);
            }

            // [ ... typeidx1 ...] typeidx2 ...
            // [      safe       ] unsafe (could be the section_end)
            //       ^^ section_curr

            // check: type_index should less than type_section_count
            if(typeidx >= type_section_count) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u32 = typeidx;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_type_index;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // write data, func_counter has been checked and is ready to be written.
            functionsec.funcs.storage.typeidx_u8_vector.emplace_back_unchecked(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8>(typeidx));

            section_curr = reinterpret_cast<::std::byte const*>(typeidx_next);

            // [ ... typeidx1 ...] typeidx2 ...
            // [      safe       ] unsafe (could be the section_end)
            //                     ^^ section_curr
        }

        // [before_section ... | func_count ... typeidx1 ... ...] (end)
        // [                       safe                         ] unsafe (could be the section_end)
        //                                                        ^^ section_curr

#else
        // default

        while(section_curr != section_end) [[likely]]
        {
            // Ensuring the existence of valid information

            // [ ... typeidx1] ... typeidx2 ...
            // [     safe    ] unsafe (could be the section_end)
            //       ^^ section_curr

            // check function counter
            // Ensure content is available before counting (section_curr != section_end)
            if(::uwvm2::parser::wasm::utils::counter_selfinc_throw_when_overflow(func_counter, section_curr, err) > func_count) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u32 = func_count;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // [ ... typeidx1] ... typeidx2 ...
            // [     safe    ] unsafe (could be the section_end)
            //       ^^ section_curr

            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 typeidx;  // No initialization necessary

            using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

            auto const [typeidx_next, typeidx_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                            ::fast_io::mnp::leb128_get(typeidx))};

            if(typeidx_err != ::fast_io::parse_code::ok) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_type_index;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(typeidx_err);
            }

            // [ ... typeidx1 ...] typeidx2 ...
            // [      safe       ] unsafe (could be the section_end)
            //       ^^ section_curr

            // check: type_index should less than type_section_count
            if(typeidx >= type_section_count) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u32 = typeidx;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_type_index;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // write data, func_counter has been checked and is ready to be written.
            functionsec.funcs.storage.typeidx_u8_vector.emplace_back_unchecked(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8>(typeidx));

            section_curr = reinterpret_cast<::std::byte const*>(typeidx_next);

            // [ ... typeidx1 ...] typeidx2 ...
            // [      safe       ] unsafe (could be the section_end)
            //                     ^^ section_curr
        }

        // [before_section ... | func_count ... typeidx1 ... ...] (end)
        // [                       safe                         ] unsafe (could be the section_end)
        //                                                        ^^ section_curr

#endif

        return section_curr;
    }

    /// @brief      Maximum typeidx in [2^8, 2^14)
    /// @details    Storing a typeidx takes up 2 bytes, typeidx corresponding uleb128 varies from 1-2 bytes, linear read/write, no simd optimization
    ///             Sequential scanning, correctly handling all cases of uleb128 u32, allowing up to 5 bytes.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* scan_function_section_impl_u16_2b(
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<function_section_storage_t> sec_adl,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para,
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32& func_counter,
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 const func_count) UWVM_THROWS
    {
        auto const& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<type_section_storage_t<Fs...>>(module_storage.sections)};
        auto const type_section_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(typesec.types.size())};

        [[assume(type_section_count >= static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 8u &&
                 type_section_count < static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 14u)]];

        auto& functionsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<function_section_storage_t>(module_storage.sections)};

        // Storing a typeidx takes up 2 bytes, use u16_vector
        functionsec.funcs.mode = ::uwvm2::parser::wasm::standard::wasm1::features::vectypeidx_minimize_storage_mode::u16_vector;

        // The size comparison between u32 and size_t has already been checked
        functionsec.funcs.storage.typeidx_u16_vector.reserve(static_cast<::std::size_t>(func_count));

        // [before_section ... | func_count ...] typeidx1 ... typeidx2 ...
        // [              safe                 ] unsafe (could be the section_end)
        //                                       ^^ section_curr

        // This algorithm is the same as the one for scan_function_section_impl_u8_2b, just make sure to scale u8 to u16 on writes.

        /// @todo support AVX512vbmi2
        /// @todo support SVE2: svtbl

#if defined(_MSC_VER) && !defined(__clang__)
        /// MSVC

# if !defined(_KERNEL_MODE) && (defined(_M_AMD64) || defined(_M_ARM64))
        /// (Little Endian), MSVC, ::fast_io::intrinsics::simd_vector
        /// x86_64-sse2, aarch64-neon
        if constexpr(::std::endian::native == ::std::endian::little)
        {
            static_assert(::std::same_as<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte, ::std::uint8_t>);

            constexpr auto simd_vector_check{static_cast<::std::uint8_t>(127u)};

            // use fast_io simd on msvc
            using u8x16simd = ::fast_io::intrinsics::simd_vector<::std::uint8_t, 16uz>;
            using u16x8simd = ::fast_io::intrinsics::simd_vector<::std::uint16_t, 8uz>;

            constexpr u8x16simd simd_vector_check_u8x16{
                simd_vector_check,  // 0
                simd_vector_check,  // 1
                simd_vector_check,  // 2
                simd_vector_check,  // 3
                simd_vector_check,  // 4
                simd_vector_check,  // 5
                simd_vector_check,  // 6
                simd_vector_check,  // 7
                simd_vector_check,  // 8
                simd_vector_check,  // 9
                simd_vector_check,  // 10
                simd_vector_check,  // 11
                simd_vector_check,  // 12
                simd_vector_check,  // 13
                simd_vector_check,  // 14
                simd_vector_check   // 15
            };

            while(static_cast<::std::size_t>(section_end - section_curr) >= 16uz) [[likely]]
            {
                // [before_section ... | func_count ... typeidx1 ... (15) ...] ...
                // [                        safe                             ] unsafe (could be the section_end)
                //                                      ^^ section_curr
                //                                      [   simd_vector_str  ]

                u8x16simd simd_vector_str;  // No initialization necessary

                simd_vector_str.load(section_curr);

                // It's already a little-endian.

                auto const check_highest_bit{simd_vector_str > simd_vector_check_u8x16};

                if(!is_all_zeros(check_highest_bit))
                {
                    auto const section_curr_end{section_curr + 16u};

                    // Since parse_by_scan uses section_end, it is possible that section_curr will be greater than section_curr_end, use '<' instead of '!='

                    while(section_curr < section_curr_end)
                    {
                        // The outer boundary is unknown and needs to be rechecked
                        // [ ... typeidx1] ... (outer) ] typeidx2 ...
                        // [     safe    ] ... (outer) ] unsafe (could be the section_end)
                        //       ^^ section_curr

                        if(::uwvm2::parser::wasm::utils::counter_selfinc_throw_when_overflow(func_counter, section_curr, err) > func_count) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_selectable.u32 = func_count;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 typeidx;  // No initialization necessary

                        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                        auto const [typeidx_next, typeidx_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                                        reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                                        ::fast_io::mnp::leb128_get(typeidx))};

                        if(typeidx_err != ::fast_io::parse_code::ok) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_type_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(typeidx_err);
                        }

                        // The outer boundary is unknown and needs to be rechecked
                        // [ ... typeidx1 ...] ... (outer) ] typeidx2 ...
                        // [      safe       ] ... (outer) ] unsafe (could be the section_end)
                        //       ^^ section_curr

                        // There's a good chance there's an error here.
                        // Or there is a leb redundancy 0
                        if(typeidx >= type_section_count) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_selectable.u32 = typeidx;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_type_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        // write data, func_counter has been checked and is ready to be written.
                        functionsec.funcs.storage.typeidx_u16_vector.emplace_back_unchecked(
                            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u16>(typeidx));

                        section_curr = reinterpret_cast<::std::byte const*>(typeidx_next);

                        // The outer boundary is unknown and needs to be rechecked
                        // [ ... typeidx1 ...] typeidx2 ...] ...
                        // [      safe       ] ... (outer) ] unsafe (could be the section_end)
                        //                     ^^ section_curr
                    }

                    // [before_section ... | func_count ... typeidx1 ... (15) ... ...  ] typeidxN
                    // [                        safe                                   ] unsafe (could be the section_end)
                    //                                                                   ^^ section_curr
                    //                                      [   simd_vector_str  ] ... ] (Depends on the size of section_curr in relation to section_curr_end)
                }
                else
                {
                    // All correct, can change state: func_counter, section_curr

                    // all are single bytes, so there are 16
                    ::uwvm2::parser::wasm::utils::counter_addass_throw_when_overflow(func_counter,
                                                                                     static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(16u),
                                                                                     section_curr,
                                                                                     err);

                    // check counter
                    if(func_counter > func_count) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_selectable.u32 = func_count;
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // Since everything is less than 128, there is no need to check the typeidx.

                    // write data
                    auto functionsec_funcs_storage_typeidx_u16_vector_imp_curr_ptr_tmp{functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr};

                    u8x16simd const u8x16v0{
                        simd_vector_str[0],               // 0
                        static_cast<::std::uint8_t>(0u),  // 1
                        simd_vector_str[1],               // 2
                        static_cast<::std::uint8_t>(0u),  // 3
                        simd_vector_str[2],               // 4
                        static_cast<::std::uint8_t>(0u),  // 5
                        simd_vector_str[3],               // 6
                        static_cast<::std::uint8_t>(0u),  // 7
                        simd_vector_str[4],               // 8
                        static_cast<::std::uint8_t>(0u),  // 9
                        simd_vector_str[5],               // 10
                        static_cast<::std::uint8_t>(0u),  // 11
                        simd_vector_str[6],               // 12
                        static_cast<::std::uint8_t>(0u),  // 13
                        simd_vector_str[7],               // 14
                        static_cast<::std::uint8_t>(0u)   // 15
                    };

                    auto const u16x8v0{::std::bit_cast<u16x8simd>(u8x16v0)};

                    u16x8v0.store(functionsec_funcs_storage_typeidx_u16_vector_imp_curr_ptr_tmp);

                    functionsec_funcs_storage_typeidx_u16_vector_imp_curr_ptr_tmp += 8uz;

                    u8x16simd const u8x16v1{
                        simd_vector_str[8],               // 0
                        static_cast<::std::uint8_t>(0u),  // 1
                        simd_vector_str[9],               // 2
                        static_cast<::std::uint8_t>(0u),  // 3
                        simd_vector_str[10],              // 4
                        static_cast<::std::uint8_t>(0u),  // 5
                        simd_vector_str[11],              // 6
                        static_cast<::std::uint8_t>(0u),  // 7
                        simd_vector_str[12],              // 8
                        static_cast<::std::uint8_t>(0u),  // 9
                        simd_vector_str[13],              // 10
                        static_cast<::std::uint8_t>(0u),  // 11
                        simd_vector_str[14],              // 12
                        static_cast<::std::uint8_t>(0u),  // 13
                        simd_vector_str[15],              // 14
                        static_cast<::std::uint8_t>(0u)   // 15
                    };

                    auto const u16x8v1{::std::bit_cast<u16x8simd>(u8x16v1)};

                    u16x8v1.store(functionsec_funcs_storage_typeidx_u16_vector_imp_curr_ptr_tmp);

                    functionsec_funcs_storage_typeidx_u16_vector_imp_curr_ptr_tmp += 8uz;

                    functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr = functionsec_funcs_storage_typeidx_u16_vector_imp_curr_ptr_tmp;

                    section_curr += 16uz;

                    // [before_section ... | func_count ... typeidx1 ... (15) ...] typeidx16
                    // [                        safe                             ] unsafe (could be the section_end)
                    //                                                             ^^ section_curr
                }

                // After testing, 2 and 3 are the fastest

                ::uwvm2::utils::intrinsics::universal::prefetch<::uwvm2::utils::intrinsics::universal::pfc_mode::read,
                                                                ::uwvm2::utils::intrinsics::universal::pfc_level::nta,
                                                                ::uwvm2::utils::intrinsics::universal::ret_policy::strm>(
                    reinterpret_cast<::std::byte const*>(section_curr) + 64u * 2u);
                ::uwvm2::utils::intrinsics::universal::prefetch<::uwvm2::utils::intrinsics::universal::pfc_mode::write,
                                                                ::uwvm2::utils::intrinsics::universal::pfc_level::L2,
                                                                ::uwvm2::utils::intrinsics::universal::ret_policy::keep>(
                    reinterpret_cast<::std::byte const*>(functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr) + 64u * 3u);
            }
        }
# endif

#elif UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__) && UWVM_HAS_BUILTIN(__builtin_shufflevector) &&                           \
    (defined(__AVX512VBMI__) && defined(__AVX512VBMI2__) && defined(__AVX512BW__) && defined(__AVX512F__))
        /// (Little Endian), [[gnu::vector_size]], has mask-u16, can shuffle, simd512
        /// x86_64-avx512vbmi2

        auto error_handler{[&](::std::size_t n) constexpr UWVM_THROWS -> void
                           {
                               // Need to ensure that section_curr to section_curr + n is memory safe
                               // Processing uses section_curr_end to guarantee entry into the loop for inspection.
                               // Also use section_end to ensure that the scanning of the last leb128 is handled correctly, both memory safe.

                               auto const section_curr_end{section_curr + n};

            // [ ... typeidx1 ... ] (section_curr_end) ... (outer) ]
            // [     safe     ... ]                    ... (outer) ] unsafe (could be the section_end)
            //       ^^ section_curr

            // Since parse_by_scan uses section_end, it is possible that section_curr will be greater than section_curr_end, use '<' instead
            // of '!='

# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                               // Used to check if correct content is recognized as an error.
                               bool correct_sequence_right_taken_for_wrong{true};
# endif

                               while(section_curr < section_curr_end)
                               {
                                   // The outer boundary is unknown and needs to be rechecked
                                   // [ ... typeidx1] ... (outer) ] typeidx2 ...
                                   // [     safe    ] ... (outer) ] unsafe (could be the section_end)
                                   //       ^^ section_curr

                                   if(::uwvm2::parser::wasm::utils::counter_selfinc_throw_when_overflow(func_counter, section_curr, err) > func_count)
                                       [[unlikely]]
                                   {
                                       err.err_curr = section_curr;
                                       err.err_selectable.u32 = func_count;
                                       err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                                       ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                   }

                                   ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 typeidx;  // No initialization necessary

                                   using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                                   auto const [typeidx_next, typeidx_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                                                   reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                                                   ::fast_io::mnp::leb128_get(typeidx))};

                                   if(typeidx_err != ::fast_io::parse_code::ok) [[unlikely]]
                                   {
                                       err.err_curr = section_curr;
                                       err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_type_index;
                                       ::uwvm2::parser::wasm::base::throw_wasm_parse_code(typeidx_err);
                                   }

                                   // The outer boundary is unknown and needs to be rechecked
                                   // [ ... typeidx1 ...] ... (outer) ] typeidx2 ...
                                   // [      safe       ] ... (outer) ] unsafe (could be the section_end)
                                   //       ^^ section_curr

                                   // There's a good chance there's an error here.
                                   // Or there is a leb redundancy 0
                                   if(typeidx >= type_section_count) [[unlikely]]
                                   {
                                       err.err_curr = section_curr;
                                       err.err_selectable.u32 = typeidx;
                                       err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_type_index;
                                       ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                   }

                                   // write data, func_counter has been checked and is ready to be written.
                                   functionsec.funcs.storage.typeidx_u16_vector.emplace_back_unchecked(
                                       static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u16>(typeidx));

# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                   if(static_cast<::std::size_t>(reinterpret_cast<::std::byte const*>(typeidx_next) - section_curr) > 2uz) [[unlikely]]
                                   {
                                       correct_sequence_right_taken_for_wrong = false;
                                   }
# endif

                                   section_curr = reinterpret_cast<::std::byte const*>(typeidx_next);

                                   // The outer boundary is unknown and needs to be rechecked
                                   // [ ... typeidx1 ...] typeidx2 ...] ...
                                   // [      safe       ] ... (outer) ] unsafe (could be the section_end)
                                   //                     ^^ section_curr
                               }

            // [before_section ... | func_count ... typeidx1 ... (n - 1) ... ...  ] typeidxN
            // [                        safe                                      ] unsafe (could be the section_end)
            //                                                                      ^^ section_curr
            //                                      [   simd_vector_str  ]  ...   ] (Depends on the size of section_curr in relation to
            //                                      section_curr_end)

# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                               if(correct_sequence_right_taken_for_wrong) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif
                           }};

        using i64x8simd [[__gnu__::__vector_size__(64)]] [[maybe_unused]] = ::std::int64_t;
        using u64x8simd [[__gnu__::__vector_size__(64)]] [[maybe_unused]] = ::std::uint64_t;
        using u32x15simd [[__gnu__::__vector_size__(64)]] [[maybe_unused]] = ::std::uint32_t;
        using u16x32simd [[__gnu__::__vector_size__(64)]] [[maybe_unused]] = ::std::uint16_t;
        using c8x64simd [[__gnu__::__vector_size__(64)]] [[maybe_unused]] = char;
        using u8x64simd [[__gnu__::__vector_size__(64)]] [[maybe_unused]] = ::std::uint8_t;
        using i8x64simd [[__gnu__::__vector_size__(64)]] [[maybe_unused]] = ::std::int8_t;

        using i64x4simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = ::std::int64_t;
        using u64x4simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = ::std::uint64_t;
        using u32x8simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = ::std::uint32_t;
        using u16x16simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = ::std::uint16_t;
        using c8x32simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = char;
        using u8x32simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = ::std::uint8_t;
        using i8x32simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = ::std::int8_t;

        using i64x2simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::int64_t;
        using u64x2simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint64_t;
        using u32x4simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint32_t;
        using u16x8simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint16_t;
        using c8x16simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = char;
        using u8x16simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint8_t;
        using i8x16simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::int8_t;

        static_assert(::std::same_as<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte, ::std::uint8_t>);

        while(static_cast<::std::size_t>(section_end - section_curr) >= 64uz) [[likely]]
        {

            // [before_section ... | func_count ... typeidx1 ... (63) ...] ...
            // [                        safe                             ] unsafe (could be the section_end)
            //                                      ^^ section_curr
            //                                      [   simd_vector_str  ]

            u8x64simd simd_vector_str;  // No initialization necessary

            ::std::memcpy(::std::addressof(simd_vector_str), section_curr, sizeof(u8x64simd));

            // It's already a little-endian.

            // When the highest bit of each byte is pop, then mask out 1

            ::std::uint64_t const check_mask{
# if defined(__AVX512BW__) && UWVM_HAS_BUILTIN(__builtin_ia32_cvtb2mask512)
                static_cast<::std::uint64_t>(__builtin_ia32_cvtb2mask512(::std::bit_cast<c8x64simd>(simd_vector_str)))
# else
#  error "missing instructions"
# endif
            };

            if(
# if UWVM_HAS_BUILTIN(__builtin_expect_with_probability)
                __builtin_expect_with_probability(static_cast<bool>(check_mask), true, 1.0 / 4.0)
# else
                check_mask
# endif
            )
            {
                auto const func_counter_end{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(func_counter + 64u)};

                if(func_counter_end < func_counter) [[unlikely]]
                {
                    // It's overflowing. It's jumping right out.
                    break;
                }

                if(func_counter_end > func_count) [[unlikely]]
                {
                    // Near the end, jump directly out of the simd section and hand it over to the tail.
                    break;
                }

                // Check for leb over 2 bytes

                // check_mask is a mask that checks the highest bit (eighth bit) of each byte. If there is a 2-byte uleb there will be a 1 and a 0. When there
                // are two consecutive 1's it means that it must represent unprocessable by the u16. Here a single 1 in the highest bit of the check_mask cannot
                // tell if there are more one's to follow, and is handed over to the next round of processing by the subsequent first_round_processes_31bit.

                if(check_mask & (check_mask << 1u)) [[unlikely]]
                {
                    error_handler(64u);
                    continue;
                }

                ::std::uint16_t const simd_vector_check{static_cast<::std::uint16_t>(type_section_count)};

                ::std::uint64_t check_mask_curr{check_mask};

                ::std::uint32_t const check_mask_curr_1st{static_cast<::std::uint32_t>(check_mask_curr)};

                // Write 32 directly when the current 32 are all zeros
                if(!check_mask_curr_1st)
                {
                    check_mask_curr >>= 32u;

                    auto const need_write_u16x32{::std::bit_cast<u16x32simd>(__builtin_shufflevector(simd_vector_str,
                                                                                                     u8x64simd{},
                                                                                                     0,   // 0
                                                                                                     64,  // 1
                                                                                                     1,   // 2
                                                                                                     64,  // 3
                                                                                                     2,   // 4
                                                                                                     64,  // 5
                                                                                                     3,   // 6
                                                                                                     64,  // 7
                                                                                                     4,   // 8
                                                                                                     64,  // 9
                                                                                                     5,   // 10
                                                                                                     64,  // 11
                                                                                                     6,   // 12
                                                                                                     64,  // 13
                                                                                                     7,   // 14
                                                                                                     64,  // 15
                                                                                                     8,   // 16
                                                                                                     64,  // 17
                                                                                                     9,   // 18
                                                                                                     64,  // 19
                                                                                                     10,  // 20
                                                                                                     64,  // 21
                                                                                                     11,  // 22
                                                                                                     64,  // 23
                                                                                                     12,  // 24
                                                                                                     64,  // 25
                                                                                                     13,  // 26
                                                                                                     64,  // 27
                                                                                                     14,  // 28
                                                                                                     64,  // 29
                                                                                                     15,  // 30
                                                                                                     64,  // 31
                                                                                                     16,  // 32
                                                                                                     64,  // 33
                                                                                                     17,  // 34
                                                                                                     64,  // 35
                                                                                                     18,  // 36
                                                                                                     64,  // 37
                                                                                                     19,  // 38
                                                                                                     64,  // 39
                                                                                                     20,  // 40
                                                                                                     64,  // 41
                                                                                                     21,  // 42
                                                                                                     64,  // 43
                                                                                                     22,  // 44
                                                                                                     64,  // 45
                                                                                                     23,  // 46
                                                                                                     64,  // 47
                                                                                                     24,  // 48
                                                                                                     64,  // 49
                                                                                                     25,  // 50
                                                                                                     64,  // 51
                                                                                                     26,  // 52
                                                                                                     64,  // 53
                                                                                                     27,  // 54
                                                                                                     64,  // 55
                                                                                                     28,  // 56
                                                                                                     64,  // 57
                                                                                                     29,  // 58
                                                                                                     64,  // 59
                                                                                                     30,  // 60
                                                                                                     64,  // 61
                                                                                                     31,  // 62
                                                                                                     64   // 63
                                                                                                     ))};

                    // Checked. No overflow.
                    func_counter += 32u;

                    ::std::memcpy(functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr, ::std::addressof(need_write_u16x32), sizeof(u16x32simd));

                    functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr += 32u;

                    section_curr += 32u;

                    // Convert the acquired 32 bits for next use.

                    simd_vector_str = __builtin_shufflevector(simd_vector_str,
                                                              simd_vector_str,
                                                              32,  // 0
                                                              33,  // 1
                                                              34,  // 2
                                                              35,  // 3
                                                              36,  // 4
                                                              37,  // 5
                                                              38,  // 6
                                                              39,  // 7
                                                              40,  // 8
                                                              41,  // 9
                                                              42,  // 10
                                                              43,  // 11
                                                              44,  // 12
                                                              45,  // 13
                                                              46,  // 14
                                                              47,  // 15
                                                              48,  // 16
                                                              49,  // 17
                                                              50,  // 18
                                                              51,  // 19
                                                              52,  // 20
                                                              53,  // 21
                                                              54,  // 22
                                                              55,  // 23
                                                              56,  // 24
                                                              57,  // 25
                                                              58,  // 26
                                                              59,  // 27
                                                              60,  // 28
                                                              61,  // 29
                                                              62,  // 30
                                                              63,  // 31
                                                              -1,  // 32
                                                              -1,  // 33
                                                              -1,  // 34
                                                              -1,  // 35
                                                              -1,  // 36
                                                              -1,  // 37
                                                              -1,  // 38
                                                              -1,  // 39
                                                              -1,  // 40
                                                              -1,  // 41
                                                              -1,  // 42
                                                              -1,  // 43
                                                              -1,  // 44
                                                              -1,  // 45
                                                              -1,  // 46
                                                              -1,  // 47
                                                              -1,  // 48
                                                              -1,  // 49
                                                              -1,  // 50
                                                              -1,  // 51
                                                              -1,  // 52
                                                              -1,  // 53
                                                              -1,  // 54
                                                              -1,  // 55
                                                              -1,  // 56
                                                              -1,  // 57
                                                              -1,  // 58
                                                              -1,  // 59
                                                              -1,  // 60
                                                              -1,  // 61
                                                              -1,  // 62
                                                              -1   // 63
                    );
                }

                // 2nd

                {
                    ::std::uint32_t const check_mask_curr_2nd{static_cast<::std::uint32_t>(check_mask_curr)};

                    bool const first_round_processes_31bit{static_cast<bool>(check_mask_curr_2nd & static_cast<::std::uint32_t>(0x8000'0000u))};
                    unsigned const first_round{32u - static_cast<unsigned>(first_round_processes_31bit)};
                    unsigned const handled_simd{32u - static_cast<unsigned>(::std::popcount(check_mask_curr_2nd))};

                    constexpr auto mask{static_cast<::std::uint64_t>(0x5555'5555'5555'5555u)};

                    constexpr u16x32simd conversion_table_1{
                        0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u,  // 0 - 10
                        0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u,  // 11 - 21
                        0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u, 0xFF00u            // 22 - 31
                    };

                    ::std::uint32_t check_mask_curr_2nd_curtailment{};

                    // Avoiding the effects of the highest invalid bit

                    for(::std::uint32_t check_mask_curr_2nd_tmp{check_mask_curr_2nd & static_cast<::std::uint32_t>(0x7FFF'FFFFu)}; check_mask_curr_2nd_tmp;)
                    {
                        auto const crtz{static_cast<unsigned>(::std::countr_zero(check_mask_curr_2nd_tmp))};

                        // check_mask_curr_2nd_tmp != 0, check_mask_curr_2nd_tmp & 0x8000'0000 == 0
                        // check_mask_curr_2nd_tmp max == 0b01..., crtz max == 30u
                        // crtz < 31u, crtz + 1u < 32u, never cause ub

                        auto const sizeFF{crtz + 1u};

                        [[assume(sizeFF >= 1u && sizeFF < 32u)]];

                        auto const FF{(static_cast<::std::uint32_t>(1u) << sizeFF) - 1u};
                        check_mask_curr_2nd_curtailment |= check_mask_curr_2nd_tmp & FF;

                        check_mask_curr_2nd_tmp &= ~FF;
                        check_mask_curr_2nd_tmp >>= 1u;
                    }

                    // check_mask_curr_2nd_curtailment : ..., 0b,     1b,     0b,     0b,     0b,     1b,     0b
                    // convert to:
                    // conversion_res_2:                 ..., 0b, 0b, 1b, 0b, 0b, 0b, 0b, 0b, 0b, 0b, 1b, 0b, 0b, 0b

                    u16x32simd conversion_res_1;

# if defined(__AVX512BW__) && UWVM_HAS_BUILTIN(__builtin_ia32_selectw_512)         // Clang
                    conversion_res_1 = __builtin_ia32_selectw_512(check_mask_curr_2nd_curtailment, conversion_table_1, u16x32simd{});
# elif defined(__AVX512BW__) && UWVM_HAS_BUILTIN(__builtin_ia32_movdquhi512_mask)  // GCC
                    conversion_res_1 = __builtin_ia32_movdquhi512_mask(conversion_table_1, u16x32simd{}, check_mask_curr_2nd_curtailment);
# else
#  error "missing instructions"
# endif

                    ::std::uint64_t conversion_res_2;
# if defined(__AVX512BW__) && UWVM_HAS_BUILTIN(__builtin_ia32_cvtb2mask512)
                    conversion_res_2 = static_cast<::std::uint64_t>(__builtin_ia32_cvtb2mask512(::std::bit_cast<c8x64simd>(conversion_res_1)));
# else
#  error "missing instructions"
# endif

                    conversion_res_2 |= mask;

                    // check_mask_curr_2nd_curtailment : ..., 0b,     1b,     0b,     0b,     0b,     1b,     0b
                    // convert to:
                    // conversion_res_2:                 ..., 0b, 0b, 1b, 0b, 0b, 0b, 0b, 0b, 0b, 0b, 1b, 0b, 0b, 0b

                    constexpr u8x64simd conversion_table_2{
                        0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,  // 0 -21
                        22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43,  // 22 - 43
                        44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63           // 44 - 63
                    };

                    u8x64simd mask_table;

# if defined(__AVX512VBMI2__) && UWVM_HAS_BUILTIN(__builtin_ia32_expandqi512_mask)
                    mask_table = __builtin_ia32_expandqi512_mask(conversion_table_2, u8x64simd{}, conversion_res_2);
# else
#  error "missing instructions"
# endif

                    u16x32simd mask_res;

# if defined(__AVX512VBMI__) && UWVM_HAS_BUILTIN(__builtin_ia32_permvarqi512_mask)                                               // GCC
                    mask_res = ::std::bit_cast<u16x32simd>(__builtin_ia32_permvarqi512_mask(::std::bit_cast<c8x64simd>(simd_vector_str),
                                                                                            ::std::bit_cast<c8x64simd>(mask_table),
                                                                                            c8x64simd{},
                                                                                            conversion_res_2));
# elif defined(__AVX512VBMI__) && UWVM_HAS_BUILTIN(__builtin_ia32_permvarqi512) && UWVM_HAS_BUILTIN(__builtin_ia32_selectb_512)  // clang
                    mask_res = ::std::bit_cast<u16x32simd>(__builtin_ia32_selectb_512(
                        conversion_res_2,
                        __builtin_ia32_permvarqi512(::std::bit_cast<c8x64simd>(simd_vector_str), ::std::bit_cast<c8x64simd>(mask_table)),
                        c8x64simd{}));
# else
#  error "missing instructions"
# endif
                    auto const res{(mask_res & static_cast<::std::uint16_t>(0x7Fu)) | ((mask_res & static_cast<::std::uint16_t>(0x7F00u)) >> 1u)};

                    // The data out of shuffle is 16-bit [0, 2^14) and may be greater than or equal to typeidx, which needs to be checked.

                    ::std::uint32_t const gen_mask{(static_cast<::std::uint32_t>(1u) << handled_simd) - 1u};

                    u16x32simd const simd_vector_check_u16x32{
                        simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check,
                        simd_vector_check, simd_vector_check, simd_vector_check,  // 0 - 6
                        simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check,
                        simd_vector_check, simd_vector_check, simd_vector_check,  // 7 - 13
                        simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check,
                        simd_vector_check, simd_vector_check, simd_vector_check,  // 14 - 20
                        simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check,
                        simd_vector_check, simd_vector_check, simd_vector_check,                    // 21 - 27
                        simd_vector_check, simd_vector_check, simd_vector_check, simd_vector_check  // 28 - 31
                    };

                    if(
# if defined(__AVX512BW__) && UWVM_HAS_BUILTIN(__builtin_ia32_cmpw512_mask)
                        __builtin_ia32_cmpw512_mask(res, simd_vector_check_u16x32, 0x05, gen_mask)
# else
#  error "missing instructions"
# endif
                            ) [[unlikely]]
                    {
                        // [before_section ... | func_count ... (32, 0) ... typeidx1 ... (63 - (32, 0)) ...] ...
                        // [                        safe                                                   ] unsafe (could be the section_end)
                        //                                                  ^^ section_curr
                        //                                                                              ^^^^^^^ section_curr + 32uz

                        // it can be processed up to 32
                        error_handler(32uz);

                        // Start the next round straight away
                        continue;
                    }

                    auto const need_write_u16x32{::std::bit_cast<u16x32simd>(res)};

                    // Checked. No overflow.
                    func_counter += handled_simd;

                    //  [... curr ... (63) ...]
                    //  [      safe           ] unsafe (could be the section_end)
                    //       ^^ functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr
                    //      [    needwrite   ]

                    // The above func_counter check checks for a maximum of 64 data to be processed at a time,
                    // and it's perfectly safe to do so here without any additional checks

                    ::std::memcpy(functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr, ::std::addressof(need_write_u16x32), sizeof(u16x32simd));

                    functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr += handled_simd;

                    //  [... curr ... (63) ...]
                    //  [          safe       ] unsafe (could be the section_end)
                    //                    ^^ functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr
                    // Or: (Security boundaries for writes are checked)
                    //                        ^^ functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr
                    //      [    needwrite   ]

                    section_curr += first_round;

                    // [before_section ... | func_count ... typeidx1 ... (63) ...] ...
                    // [                        safe                             ] unsafe (could be the section_end)
                    //                                                   ^^^^^^^ section_curr (processed_byte_counter_sum <= 64)
                    //                                      [   simd_vector_str  ]
                }
            }
            else
            {
                // All correct, can change state: func_counter, section_curr

                // all are single bytes, so there are 64
                ::uwvm2::parser::wasm::utils::counter_addass_throw_when_overflow(func_counter,
                                                                                 static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(64u),
                                                                                 section_curr,
                                                                                 err);

                // check counter
                if(func_counter > func_count) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_selectable.u32 = func_count;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                // Since everything is less than 128, there is no need to check the typeidx.

                using u16x64multisimd2x512 [[__gnu__::__vector_size__(128)]] [[maybe_unused]] = ::std::uint16_t;
                using u8x128multisimd2x512 [[__gnu__::__vector_size__(128)]] [[maybe_unused]] = ::std::uint8_t;

                ::uwvm2::utils::container::array<u8x64simd, 2uz> const need_write_u8x64x2{simd_vector_str};

                auto const need_write_u8x128{::std::bit_cast<u8x128multisimd2x512>(need_write_u8x64x2)};

                auto const need_write_u16x64{::std::bit_cast<u16x64multisimd2x512>(__builtin_shufflevector(need_write_u8x128,
                                                                                                           u8x128multisimd2x512{},
                                                                                                           0,    // 0
                                                                                                           128,  // 1
                                                                                                           1,    // 2
                                                                                                           128,  // 3
                                                                                                           2,    // 4
                                                                                                           128,  // 5
                                                                                                           3,    // 6
                                                                                                           128,  // 7
                                                                                                           4,    // 8
                                                                                                           128,  // 9
                                                                                                           5,    // 10
                                                                                                           128,  // 11
                                                                                                           6,    // 12
                                                                                                           128,  // 13
                                                                                                           7,    // 14
                                                                                                           128,  // 15
                                                                                                           8,    // 16
                                                                                                           128,  // 17
                                                                                                           9,    // 18
                                                                                                           128,  // 19
                                                                                                           10,   // 20
                                                                                                           128,  // 21
                                                                                                           11,   // 22
                                                                                                           128,  // 23
                                                                                                           12,   // 24
                                                                                                           128,  // 25
                                                                                                           13,   // 26
                                                                                                           128,  // 27
                                                                                                           14,   // 28
                                                                                                           128,  // 29
                                                                                                           15,   // 30
                                                                                                           128,  // 31
                                                                                                           16,   // 32
                                                                                                           128,  // 33
                                                                                                           17,   // 34
                                                                                                           128,  // 35
                                                                                                           18,   // 36
                                                                                                           128,  // 37
                                                                                                           19,   // 38
                                                                                                           128,  // 39
                                                                                                           20,   // 40
                                                                                                           128,  // 41
                                                                                                           21,   // 42
                                                                                                           128,  // 43
                                                                                                           22,   // 44
                                                                                                           128,  // 45
                                                                                                           23,   // 46
                                                                                                           128,  // 47
                                                                                                           24,   // 48
                                                                                                           128,  // 49
                                                                                                           25,   // 50
                                                                                                           128,  // 51
                                                                                                           26,   // 52
                                                                                                           128,  // 53
                                                                                                           27,   // 54
                                                                                                           128,  // 55
                                                                                                           28,   // 56
                                                                                                           128,  // 57
                                                                                                           29,   // 58
                                                                                                           128,  // 59
                                                                                                           30,   // 60
                                                                                                           128,  // 61
                                                                                                           31,   // 62
                                                                                                           128,  // 63
                                                                                                           32,   // 64
                                                                                                           128,  // 65
                                                                                                           33,   // 66
                                                                                                           128,  // 67
                                                                                                           34,   // 68
                                                                                                           128,  // 69
                                                                                                           35,   // 70
                                                                                                           128,  // 71
                                                                                                           36,   // 72
                                                                                                           128,  // 73
                                                                                                           37,   // 74
                                                                                                           128,  // 75
                                                                                                           38,   // 76
                                                                                                           128,  // 77
                                                                                                           39,   // 78
                                                                                                           128,  // 79
                                                                                                           40,   // 80
                                                                                                           128,  // 81
                                                                                                           41,   // 82
                                                                                                           128,  // 83
                                                                                                           42,   // 84
                                                                                                           128,  // 85
                                                                                                           43,   // 86
                                                                                                           128,  // 87
                                                                                                           44,   // 88
                                                                                                           128,  // 89
                                                                                                           45,   // 90
                                                                                                           128,  // 91
                                                                                                           46,   // 92
                                                                                                           128,  // 93
                                                                                                           47,   // 94
                                                                                                           128,  // 95
                                                                                                           48,   // 96
                                                                                                           128,  // 97
                                                                                                           49,   // 98
                                                                                                           128,  // 99
                                                                                                           50,   // 100
                                                                                                           128,  // 101
                                                                                                           51,   // 102
                                                                                                           128,  // 103
                                                                                                           52,   // 104
                                                                                                           128,  // 105
                                                                                                           53,   // 106
                                                                                                           128,  // 107
                                                                                                           54,   // 108
                                                                                                           128,  // 109
                                                                                                           55,   // 110
                                                                                                           128,  // 111
                                                                                                           56,   // 112
                                                                                                           128,  // 113
                                                                                                           57,   // 114
                                                                                                           128,  // 115
                                                                                                           58,   // 116
                                                                                                           128,  // 117
                                                                                                           59,   // 118
                                                                                                           128,  // 119
                                                                                                           60,   // 120
                                                                                                           128,  // 121
                                                                                                           61,   // 122
                                                                                                           128,  // 123
                                                                                                           62,   // 124
                                                                                                           128,  // 125
                                                                                                           63,   // 126
                                                                                                           128   // 127
                                                                                                           ))};

                // write data
                ::std::memcpy(functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr, ::std::addressof(need_write_u16x64), sizeof(u16x64multisimd2x512));

                functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr += 64uz;

                section_curr += 64uz;

                // [before_section ... | func_count ... typeidx1 ... (63) ...] typeidx64
                // [                        safe                             ] unsafe (could be the section_end)
                //                                                             ^^ section_curr
            }

            ::uwvm2::utils::intrinsics::universal::prefetch<::uwvm2::utils::intrinsics::universal::pfc_mode::read,
                                                            ::uwvm2::utils::intrinsics::universal::pfc_level::nta,
                                                            ::uwvm2::utils::intrinsics::universal::ret_policy::strm>(
                reinterpret_cast<::std::byte const*>(section_curr) + 64u * 8u);

            // Write up to 64 * 2, so prefetch 64 * 12 and 64 * 13.
            // Tested to be 3% faster than 64 * 8 and 64 * 9.

            ::uwvm2::utils::intrinsics::universal::prefetch<::uwvm2::utils::intrinsics::universal::pfc_mode::write,
                                                            ::uwvm2::utils::intrinsics::universal::pfc_level::L2,
                                                            ::uwvm2::utils::intrinsics::universal::ret_policy::keep>(
                reinterpret_cast<::std::byte const*>(functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr) + 64u * 12u);
            ::uwvm2::utils::intrinsics::universal::prefetch<::uwvm2::utils::intrinsics::universal::pfc_mode::write,
                                                            ::uwvm2::utils::intrinsics::universal::pfc_level::L2,
                                                            ::uwvm2::utils::intrinsics::universal::ret_policy::keep>(
                reinterpret_cast<::std::byte const*>(functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr) + 64u * (12u + 1u));
        }

#elif UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__) && UWVM_HAS_BUILTIN(__builtin_shufflevector) &&                           \
        ((defined(__AVX2__) && UWVM_HAS_BUILTIN(__builtin_ia32_pmovmskb256) && UWVM_HAS_BUILTIN(__builtin_ia32_pshufb256)) &&                                  \
         (defined(__AVX__) && UWVM_HAS_BUILTIN(__builtin_ia32_ptestz256))) ||                                                                                  \
    (defined(__loongarch_asx) && UWVM_HAS_BUILTIN(__builtin_lasx_xvmskltz_b) && UWVM_HAS_BUILTIN(__builtin_lasx_xvshuf_b) &&                                   \
     UWVM_HAS_BUILTIN(__builtin_lasx_xbnz_v))
        /// (Little Endian), [[gnu::vector_size]], has mask-u16, can shuffle, simd256
        /// x86_64-avx2, loongarch-LSX
        // This error handle is provided when simd is unable to handle the error, including when it encounters a memory boundary, or when there is an error that
        // needs to be localized.

        /// @note The sse4 version of scan_function_section_impl_uN_Xb is about 6% faster than avx (mundane read+judge+ptest) on Windows because the kernel
        ///       mapping in the Windows kernel's ntoskrnl.exe uses the `movups xmm` instruction for out-of-page interrupts (The reason is that the kernel
        ///       mapping in the Windows kernel ntoskrnl.exe will use the `movups xmm` instruction for out-of-page interrupts (ntoskrnl didn't do the avx
        ///       adaptation, and used the vex version), which leads to the state switching of the ymm mixed with this kind of instruction and wastes dozens
        ///       of cpu instruction cycles for the switching back and forth. linux doesn't have this problem at all, and at the same time, due to the
        ///       better algorithm of the kernel mapping, the parsing efficiency is 4 times higher than that of Windows, and most of the time is wasted in
        ///       the ntoskrnl (This can be tested with vtune). Here still use avx version, if you need sse4 version, please choose sse4 version.
        ///
        ///       (It may be misrepresented, but the fact is that the time consumption of ntoskrnl is tested in vtune to increase a lot,
        ///       while the simd processing part of the time decreases)

        auto error_handler{[&](::std::size_t n) constexpr UWVM_THROWS -> void
                           {
                               // Need to ensure that section_curr to section_curr + n is memory safe
                               // Processing uses section_curr_end to guarantee entry into the loop for inspection.
                               // Also use section_end to ensure that the scanning of the last leb128 is handled correctly, both memory safe.

                               auto const section_curr_end{section_curr + n};

            // [ ... typeidx1 ... ] (section_curr_end) ... (outer) ]
            // [     safe     ... ]                    ... (outer) ] unsafe (could be the section_end)
            //       ^^ section_curr

            // Since parse_by_scan uses section_end, it is possible that section_curr will be greater than section_curr_end, use '<' instead
            // of '!='

# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                               // Used to check if correct content is recognized as an error.
                               bool correct_sequence_right_taken_for_wrong{true};
# endif

                               while(section_curr < section_curr_end)
                               {
                                   // The outer boundary is unknown and needs to be rechecked
                                   // [ ... typeidx1] ... (outer) ] typeidx2 ...
                                   // [     safe    ] ... (outer) ] unsafe (could be the section_end)
                                   //       ^^ section_curr

                                   if(::uwvm2::parser::wasm::utils::counter_selfinc_throw_when_overflow(func_counter, section_curr, err) > func_count)
                                       [[unlikely]]
                                   {
                                       err.err_curr = section_curr;
                                       err.err_selectable.u32 = func_count;
                                       err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                                       ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                   }

                                   ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 typeidx;  // No initialization necessary

                                   using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                                   auto const [typeidx_next, typeidx_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                                                   reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                                                   ::fast_io::mnp::leb128_get(typeidx))};

                                   if(typeidx_err != ::fast_io::parse_code::ok) [[unlikely]]
                                   {
                                       err.err_curr = section_curr;
                                       err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_type_index;
                                       ::uwvm2::parser::wasm::base::throw_wasm_parse_code(typeidx_err);
                                   }

                                   // The outer boundary is unknown and needs to be rechecked
                                   // [ ... typeidx1 ...] ... (outer) ] typeidx2 ...
                                   // [      safe       ] ... (outer) ] unsafe (could be the section_end)
                                   //       ^^ section_curr

                                   // There's a good chance there's an error here.
                                   // Or there is a leb redundancy 0
                                   if(typeidx >= type_section_count) [[unlikely]]
                                   {
                                       err.err_curr = section_curr;
                                       err.err_selectable.u32 = typeidx;
                                       err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_type_index;
                                       ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                   }

                                   // write data, func_counter has been checked and is ready to be written.
                                   functionsec.funcs.storage.typeidx_u16_vector.emplace_back_unchecked(
                                       static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u16>(typeidx));

# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                   if(static_cast<::std::size_t>(reinterpret_cast<::std::byte const*>(typeidx_next) - section_curr) > 2uz) [[unlikely]]
                                   {
                                       correct_sequence_right_taken_for_wrong = false;
                                   }
# endif

                                   section_curr = reinterpret_cast<::std::byte const*>(typeidx_next);

                                   // The outer boundary is unknown and needs to be rechecked
                                   // [ ... typeidx1 ...] typeidx2 ...] ...
                                   // [      safe       ] ... (outer) ] unsafe (could be the section_end)
                                   //                     ^^ section_curr
                               }

            // [before_section ... | func_count ... typeidx1 ... (n - 1) ... ...  ] typeidxN
            // [                        safe                                      ] unsafe (could be the section_end)
            //                                                                      ^^ section_curr
            //                                      [   simd_vector_str  ]  ...   ] (Depends on the size of section_curr in relation to
            //                                      section_curr_end)

# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                               if(correct_sequence_right_taken_for_wrong) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif
                           }};

        using i64x4simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = ::std::int64_t;
        using u64x4simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = ::std::uint64_t;
        using u32x8simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = ::std::uint32_t;
        using u16x16simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = ::std::uint16_t;
        using c8x32simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = char;
        using u8x32simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = ::std::uint8_t;
        using i8x32simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = ::std::int8_t;

        using i64x2simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::int64_t;
        using u64x2simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint64_t;
        using u32x4simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint32_t;
        using u16x8simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint16_t;
        using c8x16simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = char;
        using u8x16simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint8_t;
        using i8x16simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::int8_t;

        static_assert(::std::same_as<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte, ::std::uint8_t>);

        while(static_cast<::std::size_t>(section_end - section_curr) >= 32uz) [[likely]]
        {
            // [before_section ... | func_count ... typeidx1 ... (31) ...] ...
            // [                        safe                             ] unsafe (could be the section_end)
            //                                      ^^ section_curr
            //                                      [   simd_vector_str  ]

            u8x32simd simd_vector_str;  // No initialization necessary

            ::std::memcpy(::std::addressof(simd_vector_str), section_curr, sizeof(u8x32simd));

            // It's already a little-endian.

            // When the highest bit of each byte is pop, then mask out 1

            auto const check_mask{
# if defined(__AVX2__) && UWVM_HAS_BUILTIN(__builtin_ia32_pmovmskb256)
                static_cast<unsigned>(__builtin_ia32_pmovmskb256(::std::bit_cast<c8x32simd>(simd_vector_str)))
# elif defined(__loongarch_asx) && UWVM_HAS_BUILTIN(__builtin_lasx_xvmskltz_b)
                ::std::bit_cast<::uwvm2::utils::container::array<unsigned, 8uz>>(__builtin_lasx_xvmskltz_b(::std::bit_cast<i8x32simd>(simd_vector_str)))
                    .front_unchecked()
# else
#  error "missing instructions"
# endif
            };

            if(
# if UWVM_HAS_BUILTIN(__builtin_expect_with_probability)
                __builtin_expect_with_probability(static_cast<bool>(check_mask), true, 1.0 / 4.0)
# else
                check_mask
# endif
            )
            {
                ::std::uint16_t const simd_vector_check{static_cast<::std::uint16_t>(type_section_count)};

                auto check_mask_curr{check_mask};  // uleb128 mask

                // Record the number of bytes processed in the first round
                unsigned first_and_second_round_handle_bytes{};

                // 1st and 2nd round
                {
                    unsigned const check_table_index_1st{check_mask_curr & 0xFFu};

                    // according to the information in the table, when the eighth bit is 1, then the processing length is 7
                    bool const first_round_is_seven{static_cast<bool>(check_table_index_1st & 0x80u)};

                    check_mask_curr >>= 8u - static_cast<unsigned>(first_round_is_seven);

                    first_and_second_round_handle_bytes += static_cast<unsigned>(first_round_is_seven);

                    unsigned const check_table_index_2nd{check_mask_curr & 0xFFu};

                    // according to the information in the table, when the eighth bit is 1, then the processing length is 7
                    bool const second_round_is_seven{static_cast<bool>(check_table_index_2nd & 0x80u)};

                    check_mask_curr >>= 8u - static_cast<unsigned>(second_round_is_seven);

                    first_and_second_round_handle_bytes += static_cast<unsigned>(second_round_is_seven);

                    // When there are few operations, 2-branch prediction may seem inferior to one operation + one branch prediction
                    // For parsing random data that may be encountered, it is more efficient to use bitwise operations than to add branches using logical
                    // operators.

                    if(check_table_index_1st | check_table_index_2nd)
                    {
                        // There are non-zero terms in 1st and 2nd, so it's straightforward to construct a 256 (128 * 2) shuffle.

                        //  [...] curr ...
                        //  [sf ] unsafe (could be the section_end)
                        //        ^^ functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr

                        // The write size is 16 bits, but the valid data may be less than 16 bits, directly check the maximum value, more than that, then enter
                        // the tail processing Cannot change state until error checking: func_counter, section_curr

                        auto const func_counter_end{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(func_counter + 16u)};

                        if(func_counter_end < func_counter) [[unlikely]]
                        {
                            // It's overflowing. It's jumping right out.
                            break;
                        }

                        if(func_counter_end > func_count) [[unlikely]]
                        {
                            // Near the end, jump directly out of the simd section and hand it over to the tail.
                            break;
                        }

                        //  [... curr ... (15) ...]
                        //  [        safe         ] unsafe (could be the section_end)
                        //       ^^ functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr
                        //      [ needwrite ] ... ] (Always writes 16 bits, but valid data may be less than 16 bits)

                        auto const& curr_table_1st{simd128_shuffle_table.index_unchecked(static_cast<::std::size_t>(check_table_index_1st))};
                        auto const curr_table_shuffle_mask_1st{curr_table_1st.shuffle_mask};
                        auto const curr_table_processed_simd_1st{curr_table_1st.processed_simd};  // size of handled u32
                        auto const curr_table_processed_byte_1st{curr_table_1st.processed_byte};  // size of handled uleb128

                        auto const& curr_table_2nd{simd128_shuffle_table.index_unchecked(static_cast<::std::size_t>(check_table_index_2nd))};
                        auto const curr_table_shuffle_mask_2nd{curr_table_2nd.shuffle_mask};
                        auto const curr_table_processed_simd_2nd{curr_table_2nd.processed_simd};  // size of handled u32
                        auto const curr_table_processed_byte_2nd{curr_table_2nd.processed_byte};  // size of handled uleb128

                        // When the number of consecutive bits is greater than 2, switch back to the normal processing method
                        // Error handling is almost never encountered, using logical operators to increase short-circuit evaluation, with branch prediction
                        // always faster than bitwise operations

                        if(!curr_table_processed_simd_1st || !curr_table_processed_simd_2nd) [[unlikely]]
                        {
                            // [before_section ... | func_count ... typeidx1 ... (31) ...] ...
                            // [                        safe                             ] unsafe (could be the section_end)
                            //                                      ^^ section_curr
                            //                                                             ^^ section_curr + 32uz

                            // it can be processed up to 32
                            error_handler(32uz);

                            // Start the next round straight away
                            continue;
                        }

                        //  [... curr ... (15) ...]
                        //  [        safe         ] unsafe (could be the section_end)
                        //       ^^ functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr
                        //      [ needwrite ] ... ] (Always writes 16 bits, but valid data may be less than 16 bits)

                        // shuffle and write

                        ::uwvm2::utils::container::array<u8x16simd, 2uz> const mask_table_u8x16x2{curr_table_shuffle_mask_1st, curr_table_shuffle_mask_2nd};
                        u8x32simd const mask_tableu8x32{::std::bit_cast<u8x32simd>(mask_table_u8x16x2)};

                        u8x32simd simd_vector_str_shuffle;

                        if(curr_table_processed_byte_1st == 7u)
                        {
                            simd_vector_str_shuffle = __builtin_shufflevector(simd_vector_str,
                                                                              simd_vector_str,
                                                                              0,   // 0
                                                                              1,   // 1
                                                                              2,   // 2
                                                                              3,   // 3
                                                                              4,   // 4
                                                                              5,   // 5
                                                                              6,   // 6
                                                                              -1,  // 7
                                                                              -1,  // 8
                                                                              -1,  // 9
                                                                              -1,  // 10
                                                                              -1,  // 11
                                                                              -1,  // 12
                                                                              -1,  // 13
                                                                              -1,  // 14
                                                                              -1,  // 15
                                                                              7,   // 16
                                                                              8,   // 17
                                                                              9,   // 18
                                                                              10,  // 19
                                                                              11,  // 20
                                                                              12,  // 21
                                                                              13,  // 22
                                                                              14,  // 23
                                                                              -1,  // 24
                                                                              -1,  // 25
                                                                              -1,  // 26
                                                                              -1,  // 27
                                                                              -1,  // 28
                                                                              -1,  // 29
                                                                              -1,  // 30
                                                                              -1   // 31
                            );
                        }
                        else
                        {
                            simd_vector_str_shuffle = __builtin_shufflevector(simd_vector_str,
                                                                              simd_vector_str,
                                                                              0,   // 0
                                                                              1,   // 1
                                                                              2,   // 2
                                                                              3,   // 3
                                                                              4,   // 4
                                                                              5,   // 5
                                                                              6,   // 6
                                                                              7,   // 7
                                                                              -1,  // 8
                                                                              -1,  // 9
                                                                              -1,  // 10
                                                                              -1,  // 11
                                                                              -1,  // 12
                                                                              -1,  // 13
                                                                              -1,  // 14
                                                                              -1,  // 15
                                                                              8,   // 16
                                                                              9,   // 17
                                                                              10,  // 18
                                                                              11,  // 19
                                                                              12,  // 20
                                                                              13,  // 21
                                                                              14,  // 22
                                                                              15,  // 23
                                                                              -1,  // 24
                                                                              -1,  // 25
                                                                              -1,  // 26
                                                                              -1,  // 27
                                                                              -1,  // 28
                                                                              -1,  // 29
                                                                              -1,  // 30
                                                                              -1   // 31
                            );
                        }

                        u16x16simd mask_res;

# if defined(__AVX2__) && UWVM_HAS_BUILTIN(__builtin_ia32_pshufb256)
                        mask_res = ::std::bit_cast<u16x16simd>(__builtin_ia32_pshufb256(simd_vector_str_shuffle, mask_tableu8x32));
# elif defined(__loongarch_asx) && UWVM_HAS_BUILTIN(__builtin_lasx_xvshuf_b)
                        mask_res = ::std::bit_cast<u16x16simd>(
                            __builtin_lasx_xvshuf_b(simd_vector_str_shuffle, simd_vector_str_shuffle, mask_tableu8x32));  /// @todo need check
# else
#  error "missing instructions"
# endif

                        // The leb128 that has been merged into u16x8 is finalized to get the value corresponding to u16

                        auto const res{(mask_res & static_cast<::std::uint16_t>(0x7Fu)) | ((mask_res & static_cast<::std::uint16_t>(0x7F00u)) >> 1u)};

                        // The data out of shuffle is 16-bit [0, 2^14) and may be greater than or equal to typeidx, which needs to be checked.

                        auto const check_upper{res >= simd_vector_check};

                        if(
# if defined(__AVX__) && UWVM_HAS_BUILTIN(__builtin_ia32_ptestz256)
                            !__builtin_ia32_ptestz256(::std::bit_cast<i64x4simd>(check_upper), ::std::bit_cast<i64x4simd>(check_upper))
# elif defined(__loongarch_asx) && UWVM_HAS_BUILTIN(__builtin_lasx_xbnz_v)
                            __builtin_lasx_xbnz_v(::std::bit_cast<u8x32simd>(check_upper))  /// @todo need check
# else
#  error "missing instructions"
# endif
                                ) [[unlikely]]
                        {
                            // [before_section ... | func_count ... typeidx1 ... (31) ...] ...
                            // [                        safe                             ] unsafe (could be the section_end)
                            //                                      ^^ section_curr
                            //                                                             ^^ section_curr + 32uz

                            // it can be processed up to 32
                            error_handler(32uz);

                            // Start the next round straight away
                            continue;
                        }

                        // All correct, can change state: func_counter, section_curr

                        auto const curr_table_processed_simd_sum{curr_table_processed_simd_1st + curr_table_processed_simd_2nd};
                        auto const curr_table_processed_byte_sum{curr_table_processed_byte_1st + curr_table_processed_byte_2nd};

                        // Checked. No overflow.
                        func_counter += curr_table_processed_simd_sum;

                        // There is no need to check function_counter, because curr_table_processed_simd_sum is always less than or equal to 16u.

                        // Since typeidx can write to a u8, reduce u16x16 to u8x16

                        u16x16simd needwrite_u16x16;

                        switch(curr_table_processed_simd_1st)
                        {
                            case 4u:
                            {
                                needwrite_u16x16 = __builtin_shufflevector(::std::bit_cast<u16x16simd>(res),
                                                                           ::std::bit_cast<u16x16simd>(res),
                                                                           0,   // 0
                                                                           1,   // 1
                                                                           2,   // 2
                                                                           3,   // 3
                                                                           8,   // 4
                                                                           9,   // 5
                                                                           10,  // 6
                                                                           11,  // 7
                                                                           12,  // 8
                                                                           13,  // 9
                                                                           14,  // 10
                                                                           15,  // 11
                                                                           -1,  // 12
                                                                           -1,  // 13
                                                                           -1,  // 14
                                                                           -1   // 15
                                );
                                break;
                            }
                            case 5u:
                            {
                                needwrite_u16x16 = __builtin_shufflevector(::std::bit_cast<u16x16simd>(res),
                                                                           ::std::bit_cast<u16x16simd>(res),
                                                                           0,   // 0
                                                                           1,   // 1
                                                                           2,   // 2
                                                                           3,   // 3
                                                                           4,   // 4
                                                                           8,   // 5
                                                                           9,   // 6
                                                                           10,  // 7
                                                                           11,  // 8
                                                                           12,  // 9
                                                                           13,  // 10
                                                                           14,  // 11
                                                                           15,  // 12
                                                                           -1,  // 13
                                                                           -1,  // 14
                                                                           -1   // 15
                                );
                                break;
                            }
                            case 6u:
                            {
                                needwrite_u16x16 = __builtin_shufflevector(::std::bit_cast<u16x16simd>(res),
                                                                           ::std::bit_cast<u16x16simd>(res),
                                                                           0,   // 0
                                                                           1,   // 1
                                                                           2,   // 2
                                                                           3,   // 3
                                                                           4,   // 4
                                                                           5,   // 5
                                                                           8,   // 6
                                                                           9,   // 7
                                                                           10,  // 8
                                                                           11,  // 9
                                                                           12,  // 10
                                                                           13,  // 11
                                                                           14,  // 12
                                                                           15,  // 13
                                                                           -1,  // 14
                                                                           -1   // 15
                                );
                                break;
                            }
                            case 7u:
                            {
                                needwrite_u16x16 = __builtin_shufflevector(::std::bit_cast<u16x16simd>(res),
                                                                           ::std::bit_cast<u16x16simd>(res),
                                                                           0,   // 0
                                                                           1,   // 1
                                                                           2,   // 2
                                                                           3,   // 3
                                                                           4,   // 4
                                                                           5,   // 5
                                                                           6,   // 6
                                                                           8,   // 7
                                                                           9,   // 8
                                                                           10,  // 9
                                                                           11,  // 10
                                                                           12,  // 11
                                                                           13,  // 12
                                                                           14,  // 13
                                                                           15,  // 14
                                                                           -1   // 15
                                );
                                break;
                            }
                            case 8u:
                            {
                                needwrite_u16x16 = ::std::bit_cast<u16x16simd>(res);
                                break;
                            }
                            [[unlikely]] default:
                            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                                ::std::unreachable();
                            }
                        }

                        //  [... curr ... (15) ... curr_next ... (15) ...]
                        //  [                 safe                       ] unsafe (could be the section_end)
                        //       ^^ functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr
                        //      [    needwrite   ]

                        ::std::memcpy(functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr, ::std::addressof(needwrite_u16x16), sizeof(u16x16simd));

                        functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr += curr_table_processed_simd_sum;

                        //  [... curr ... (15) ... curr_next ... (15) ...]
                        //  [                 safe                       ] unsafe (could be the section_end)
                        //                    ^^ functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr
                        // Or: (Security boundaries for writes are checked)
                        //                        ^^ functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr
                        //      [    needwrite   ]

                        section_curr += curr_table_processed_byte_sum;

                        // [before_section ... | func_count ... typeidx1 ... (15) ... ... (15) ...] ...
                        // [                        safe                                          ] unsafe (could be the section_end)
                        //                                                   ^^^^^^^ section_curr (curr_table_processed_byte <= 8)
                        //                                      [   simd_vector_str               ]
                    }
                    else
                    {
                        // If check_table_index_1st or check_table_index_2nd is 0, write directly

                        // check func_counter before write
                        // The check is all done and the status can be changed, if it is less than the value written, then something is wrong.

                        ::uwvm2::parser::wasm::utils::counter_addass_throw_when_overflow(
                            func_counter,
                            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(16u),
                            section_curr,
                            err);

                        // check counter
                        if(func_counter > func_count) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_selectable.u32 = func_count;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        // Since everything is less than 128, there is no need to check the typeidx.

                        // write 16 * 2 byte

                        auto const needwrite_u16x16{::std::bit_cast<u16x16simd>(__builtin_shufflevector(simd_vector_str,
                                                                                                        u8x32simd{},
                                                                                                        0,   // 0
                                                                                                        32,  // 1
                                                                                                        1,   // 2
                                                                                                        32,  // 3
                                                                                                        2,   // 4
                                                                                                        32,  // 5
                                                                                                        3,   // 6
                                                                                                        32,  // 7
                                                                                                        4,   // 8
                                                                                                        32,  // 9
                                                                                                        5,   // 10
                                                                                                        32,  // 11
                                                                                                        6,   // 12
                                                                                                        32,  // 13
                                                                                                        7,   // 14
                                                                                                        32,  // 15
                                                                                                        8,   // 16
                                                                                                        32,  // 17
                                                                                                        9,   // 18
                                                                                                        32,  // 19
                                                                                                        10,  // 20
                                                                                                        32,  // 21
                                                                                                        11,  // 22
                                                                                                        32,  // 23
                                                                                                        12,  // 24
                                                                                                        32,  // 25
                                                                                                        13,  // 26
                                                                                                        32,  // 27
                                                                                                        14,  // 28
                                                                                                        32,  // 29
                                                                                                        15,  // 30
                                                                                                        32   // 31
                                                                                                        ))};

                        //  [... curr ... (15) ... curr_next ... (15) ...]
                        //  [                 safe                       ] unsafe (could be the section_end)
                        //       ^^ functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr
                        //      [    needwrite    ]

                        ::std::memcpy(functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr, ::std::addressof(needwrite_u16x16), sizeof(u16x16simd));

                        functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr += 16u;

                        //  [... curr_last ... (15) ... curr ... (15) ...]
                        //  [                  safe                      ] unsafe (could be the section_end)
                        //                              ^^ functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr

                        section_curr += 16u;

                        // [before_section ... | func_count ... typeidx1 ... (15) ... typeidxc ... (15) ...] ...
                        // [                                safe                                           ] unsafe (could be the section_end)
                        //                                                            ^^ section_curr
                        //                                      [                simd_vector_str           ]
                    }
                }

                u8x32simd third_fourth_round_simd_u8x32;

                switch(first_and_second_round_handle_bytes)
                {
                    case 0u:  // handle 16u
                    {
                        auto const shuffle_u8x32{__builtin_shufflevector(simd_vector_str,
                                                                         simd_vector_str,
                                                                         16,  // 0
                                                                         17,  // 1
                                                                         18,  // 2
                                                                         19,  // 3
                                                                         20,  // 4
                                                                         21,  // 5
                                                                         22,  // 6
                                                                         23,  // 7
                                                                         24,  // 8
                                                                         25,  // 9
                                                                         26,  // 10
                                                                         27,  // 11
                                                                         28,  // 12
                                                                         29,  // 13
                                                                         30,  // 14
                                                                         31,  // 15
                                                                         -1,  // 16
                                                                         -1,  // 17
                                                                         -1,  // 18
                                                                         -1,  // 19
                                                                         -1,  // 20
                                                                         -1,  // 21
                                                                         -1,  // 22
                                                                         -1,  // 23
                                                                         -1,  // 24
                                                                         -1,  // 25
                                                                         -1,  // 26
                                                                         -1,  // 27
                                                                         -1,  // 28
                                                                         -1,  // 29
                                                                         -1,  // 30
                                                                         -1   // 31
                                                                         )};
                        third_fourth_round_simd_u8x32 = ::std::bit_cast<u8x32simd>(shuffle_u8x32);
                        break;
                    }
                    case 1u:  // handle 15u
                    {
                        auto const shuffle_u8x32{__builtin_shufflevector(simd_vector_str,
                                                                         simd_vector_str,
                                                                         15,  // 0
                                                                         16,  // 1
                                                                         17,  // 2
                                                                         18,  // 3
                                                                         19,  // 4
                                                                         20,  // 5
                                                                         21,  // 6
                                                                         22,  // 7
                                                                         23,  // 8
                                                                         24,  // 9
                                                                         25,  // 10
                                                                         26,  // 11
                                                                         27,  // 12
                                                                         28,  // 13
                                                                         29,  // 14
                                                                         30,  // 15
                                                                         -1,  // 16
                                                                         -1,  // 17
                                                                         -1,  // 18
                                                                         -1,  // 19
                                                                         -1,  // 20
                                                                         -1,  // 21
                                                                         -1,  // 22
                                                                         -1,  // 23
                                                                         -1,  // 24
                                                                         -1,  // 25
                                                                         -1,  // 26
                                                                         -1,  // 27
                                                                         -1,  // 28
                                                                         -1,  // 29
                                                                         -1,  // 30
                                                                         -1   // 31
                                                                         )};
                        third_fourth_round_simd_u8x32 = ::std::bit_cast<u8x32simd>(shuffle_u8x32);
                        break;
                    }
                    case 2u:  // handle 14u
                    {
                        auto const shuffle_u8x32{__builtin_shufflevector(simd_vector_str,
                                                                         simd_vector_str,
                                                                         14,  // 0
                                                                         15,  // 1
                                                                         16,  // 2
                                                                         17,  // 3
                                                                         18,  // 4
                                                                         19,  // 5
                                                                         20,  // 6
                                                                         21,  // 7
                                                                         22,  // 8
                                                                         23,  // 9
                                                                         24,  // 10
                                                                         25,  // 11
                                                                         26,  // 12
                                                                         27,  // 13
                                                                         28,  // 14
                                                                         29,  // 15
                                                                         -1,  // 16
                                                                         -1,  // 17
                                                                         -1,  // 18
                                                                         -1,  // 19
                                                                         -1,  // 20
                                                                         -1,  // 21
                                                                         -1,  // 22
                                                                         -1,  // 23
                                                                         -1,  // 24
                                                                         -1,  // 25
                                                                         -1,  // 26
                                                                         -1,  // 27
                                                                         -1,  // 28
                                                                         -1,  // 29
                                                                         -1,  // 30
                                                                         -1   // 31
                                                                         )};
                        third_fourth_round_simd_u8x32 = ::std::bit_cast<u8x32simd>(shuffle_u8x32);
                        break;
                    }
                    [[unlikely]] default:
                    {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                        ::std::unreachable();
                    }
                }

                // 3rd and 4th round
                {
                    unsigned const check_table_index_1st{check_mask_curr & 0xFFu};

                    // according to the information in the table, when the eighth bit is 1, then the processing length is 7
                    bool const first_round_is_seven{static_cast<bool>(check_table_index_1st & 0x80u)};

                    check_mask_curr >>= 8u - static_cast<unsigned>(first_round_is_seven);

                    unsigned const check_table_index_2nd{check_mask_curr & 0xFFu};

                    // according to the information in the table, when the eighth bit is 1, then the processing length is 7

                    // check_mask_curr to be useless and no further calculations are needed.

                    // When there are few operations, 2-branch prediction may seem inferior to one operation + one branch prediction
                    // For parsing random data that may be encountered, it is more efficient to use bitwise operations than to add branches using logical
                    // operators.

                    if(check_table_index_1st | check_table_index_2nd)
                    {
                        // There are non-zero terms in 1st and 2nd, so it's straightforward to construct a 256 (128 * 2) shuffle.

                        //  [...] curr ...
                        //  [sf ] unsafe (could be the section_end)
                        //        ^^ functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr

                        // The write size is 16 bits, but the valid data may be less than 16 bits, directly check the maximum value, more than that, then enter
                        // the tail processing Cannot change state until error checking: func_counter, section_curr

                        auto const func_counter_end{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(func_counter + 16u)};

                        if(func_counter_end < func_counter) [[unlikely]]
                        {
                            // It's overflowing. It's jumping right out.
                            break;
                        }

                        if(func_counter_end > func_count) [[unlikely]]
                        {
                            // Near the end, jump directly out of the simd section and hand it over to the tail.
                            break;
                        }

                        //  [... curr ... (15) ...]
                        //  [        safe         ] unsafe (could be the section_end)
                        //       ^^ functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr
                        //      [ needwrite ] ... ] (Always writes 16 bits, but valid data may be less than 16 bits)

                        auto const& curr_table_1st{simd128_shuffle_table.index_unchecked(static_cast<::std::size_t>(check_table_index_1st))};
                        auto const curr_table_shuffle_mask_1st{curr_table_1st.shuffle_mask};
                        auto const curr_table_processed_simd_1st{curr_table_1st.processed_simd};  // size of handled u32
                        auto const curr_table_processed_byte_1st{curr_table_1st.processed_byte};  // size of handled uleb128

                        auto const& curr_table_2nd{simd128_shuffle_table.index_unchecked(static_cast<::std::size_t>(check_table_index_2nd))};
                        auto const curr_table_shuffle_mask_2nd{curr_table_2nd.shuffle_mask};
                        auto const curr_table_processed_simd_2nd{curr_table_2nd.processed_simd};  // size of handled u32
                        auto const curr_table_processed_byte_2nd{curr_table_2nd.processed_byte};  // size of handled uleb128

                        // When the number of consecutive bits is greater than 2, switch back to the normal processing method
                        // Error handling is almost never encountered, using logical operators to increase short-circuit evaluation, with branch prediction
                        // always faster than bitwise operations

                        if(!curr_table_processed_simd_1st || !curr_table_processed_simd_2nd) [[unlikely]]
                        {
                            // [before_section ... | func_count ... typeidx1 ... (31) ...] ...
                            // [                        safe                             ] unsafe (could be the section_end)
                            //                                      ^^ section_curr
                            //                                                             ^^ section_curr + 32uz

                            // it can be processed up to 32 - 16 == 16
                            error_handler(16uz);

                            // Start the next round straight away
                            continue;
                        }

                        //  [... curr ... (15) ...]
                        //  [        safe         ] unsafe (could be the section_end)
                        //       ^^ functionsec.funcs.storage.typeidx_u8_vector.imp.curr_ptr
                        //      [ needwrite ] ... ] (Always writes 16 bits, but valid data may be less than 16 bits)

                        // shuffle and write

                        ::uwvm2::utils::container::array<u8x16simd, 2uz> const mask_table_u8x16x2{curr_table_shuffle_mask_1st, curr_table_shuffle_mask_2nd};
                        u8x32simd const mask_tableu8x32{::std::bit_cast<u8x32simd>(mask_table_u8x16x2)};

                        u8x32simd simd_vector_str_shuffle;

                        if(curr_table_processed_byte_1st == 7u)
                        {
                            simd_vector_str_shuffle = __builtin_shufflevector(third_fourth_round_simd_u8x32,
                                                                              third_fourth_round_simd_u8x32,
                                                                              0,   // 0
                                                                              1,   // 1
                                                                              2,   // 2
                                                                              3,   // 3
                                                                              4,   // 4
                                                                              5,   // 5
                                                                              6,   // 6
                                                                              -1,  // 7
                                                                              -1,  // 8
                                                                              -1,  // 9
                                                                              -1,  // 10
                                                                              -1,  // 11
                                                                              -1,  // 12
                                                                              -1,  // 13
                                                                              -1,  // 14
                                                                              -1,  // 15
                                                                              7,   // 16
                                                                              8,   // 17
                                                                              9,   // 18
                                                                              10,  // 19
                                                                              11,  // 20
                                                                              12,  // 21
                                                                              13,  // 22
                                                                              14,  // 23
                                                                              -1,  // 24
                                                                              -1,  // 25
                                                                              -1,  // 26
                                                                              -1,  // 27
                                                                              -1,  // 28
                                                                              -1,  // 29
                                                                              -1,  // 30
                                                                              -1   // 31
                            );
                        }
                        else
                        {
                            simd_vector_str_shuffle = __builtin_shufflevector(third_fourth_round_simd_u8x32,
                                                                              third_fourth_round_simd_u8x32,
                                                                              0,   // 0
                                                                              1,   // 1
                                                                              2,   // 2
                                                                              3,   // 3
                                                                              4,   // 4
                                                                              5,   // 5
                                                                              6,   // 6
                                                                              7,   // 7
                                                                              -1,  // 8
                                                                              -1,  // 9
                                                                              -1,  // 10
                                                                              -1,  // 11
                                                                              -1,  // 12
                                                                              -1,  // 13
                                                                              -1,  // 14
                                                                              -1,  // 15
                                                                              8,   // 16
                                                                              9,   // 17
                                                                              10,  // 18
                                                                              11,  // 19
                                                                              12,  // 20
                                                                              13,  // 21
                                                                              14,  // 22
                                                                              15,  // 23
                                                                              -1,  // 24
                                                                              -1,  // 25
                                                                              -1,  // 26
                                                                              -1,  // 27
                                                                              -1,  // 28
                                                                              -1,  // 29
                                                                              -1,  // 30
                                                                              -1   // 31
                            );
                        }

                        u16x16simd mask_res;

# if defined(__AVX2__) && UWVM_HAS_BUILTIN(__builtin_ia32_pshufb256)
                        mask_res = ::std::bit_cast<u16x16simd>(__builtin_ia32_pshufb256(simd_vector_str_shuffle, mask_tableu8x32));
# elif defined(__loongarch_asx) && UWVM_HAS_BUILTIN(__builtin_lasx_xvshuf_b)
                        mask_res = ::std::bit_cast<u16x16simd>(
                            __builtin_lasx_xvshuf_b(simd_vector_str_shuffle, simd_vector_str_shuffle, mask_tableu8x32));  /// @todo need check
# else
#  error "missing instructions"
# endif

                        // The leb128 that has been merged into u16x8 is finalized to get the value corresponding to u16

                        auto const res{(mask_res & static_cast<::std::uint16_t>(0x7Fu)) | ((mask_res & static_cast<::std::uint16_t>(0x7F00u)) >> 1u)};

                        // The data out of shuffle is 16-bit [0, 2^14) and may be greater than or equal to typeidx, which needs to be checked.

                        auto const check_upper{res >= simd_vector_check};

                        if(
# if defined(__AVX__) && UWVM_HAS_BUILTIN(__builtin_ia32_ptestz256)
                            !__builtin_ia32_ptestz256(::std::bit_cast<i64x4simd>(check_upper), ::std::bit_cast<i64x4simd>(check_upper))
# elif defined(__loongarch_asx) && UWVM_HAS_BUILTIN(__builtin_lasx_xbnz_v)
                            __builtin_lasx_xbnz_v(::std::bit_cast<u8x32simd>(check_upper))  /// @todo need check
# else
#  error "missing instructions"
# endif
                                ) [[unlikely]]
                        {
                            // [before_section ... | func_count ... typeidx1 ... (31) ...] ...
                            // [                        safe                             ] unsafe (could be the section_end)
                            //                                      ^^ section_curr
                            //                                                             ^^ section_curr + 32uz

                            // it can be processed up to 32 - 16 == 16
                            error_handler(16uz);

                            // Start the next round straight away
                            continue;
                        }

                        // All correct, can change state: func_counter, section_curr

                        auto const curr_table_processed_simd_sum{curr_table_processed_simd_1st + curr_table_processed_simd_2nd};
                        auto const curr_table_processed_byte_sum{curr_table_processed_byte_1st + curr_table_processed_byte_2nd};

                        // Checked. No overflow.
                        func_counter += curr_table_processed_simd_sum;

                        // There is no need to check function_counter, because curr_table_processed_simd_sum is always less than or equal to 16u.

                        // Since typeidx can write to a u8, reduce u16x16 to u8x16

                        u16x16simd needwrite_u16x16;

                        switch(curr_table_processed_simd_1st)
                        {
                            case 4u:
                            {
                                needwrite_u16x16 = __builtin_shufflevector(::std::bit_cast<u16x16simd>(res),
                                                                           ::std::bit_cast<u16x16simd>(res),
                                                                           0,   // 0
                                                                           1,   // 1
                                                                           2,   // 2
                                                                           3,   // 3
                                                                           8,   // 4
                                                                           9,   // 5
                                                                           10,  // 6
                                                                           11,  // 7
                                                                           12,  // 8
                                                                           13,  // 9
                                                                           14,  // 10
                                                                           15,  // 11
                                                                           -1,  // 12
                                                                           -1,  // 13
                                                                           -1,  // 14
                                                                           -1   // 15
                                );
                                break;
                            }
                            case 5u:
                            {
                                needwrite_u16x16 = __builtin_shufflevector(::std::bit_cast<u16x16simd>(res),
                                                                           ::std::bit_cast<u16x16simd>(res),
                                                                           0,   // 0
                                                                           1,   // 1
                                                                           2,   // 2
                                                                           3,   // 3
                                                                           4,   // 4
                                                                           8,   // 5
                                                                           9,   // 6
                                                                           10,  // 7
                                                                           11,  // 8
                                                                           12,  // 9
                                                                           13,  // 10
                                                                           14,  // 11
                                                                           15,  // 12
                                                                           -1,  // 13
                                                                           -1,  // 14
                                                                           -1   // 15
                                );
                                break;
                            }
                            case 6u:
                            {
                                needwrite_u16x16 = __builtin_shufflevector(::std::bit_cast<u16x16simd>(res),
                                                                           ::std::bit_cast<u16x16simd>(res),
                                                                           0,   // 0
                                                                           1,   // 1
                                                                           2,   // 2
                                                                           3,   // 3
                                                                           4,   // 4
                                                                           5,   // 5
                                                                           8,   // 6
                                                                           9,   // 7
                                                                           10,  // 8
                                                                           11,  // 9
                                                                           12,  // 10
                                                                           13,  // 11
                                                                           14,  // 12
                                                                           15,  // 13
                                                                           -1,  // 14
                                                                           -1   // 15
                                );
                                break;
                            }
                            case 7u:
                            {
                                needwrite_u16x16 = __builtin_shufflevector(::std::bit_cast<u16x16simd>(res),
                                                                           ::std::bit_cast<u16x16simd>(res),
                                                                           0,   // 0
                                                                           1,   // 1
                                                                           2,   // 2
                                                                           3,   // 3
                                                                           4,   // 4
                                                                           5,   // 5
                                                                           6,   // 6
                                                                           8,   // 7
                                                                           9,   // 8
                                                                           10,  // 9
                                                                           11,  // 10
                                                                           12,  // 11
                                                                           13,  // 12
                                                                           14,  // 13
                                                                           15,  // 14
                                                                           -1   // 15
                                );
                                break;
                            }
                            case 8u:
                            {
                                needwrite_u16x16 = ::std::bit_cast<u16x16simd>(res);
                                break;
                            }
                            [[unlikely]] default:
                            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                                ::std::unreachable();
                            }
                        }

                        //  [... curr ... (15) ... curr_next ... (15) ...]
                        //  [                 safe                       ] unsafe (could be the section_end)
                        //       ^^ functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr
                        //      [    needwrite   ]

                        ::std::memcpy(functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr, ::std::addressof(needwrite_u16x16), sizeof(u16x16simd));

                        functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr += curr_table_processed_simd_sum;

                        //  [... curr ... (15) ... curr_next ... (15) ...]
                        //  [                 safe                       ] unsafe (could be the section_end)
                        //                    ^^ functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr
                        // Or: (Security boundaries for writes are checked)
                        //                        ^^ functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr
                        //      [    needwrite   ]

                        section_curr += curr_table_processed_byte_sum;

                        // [before_section ... | func_count ... typeidx1 ... (15) ... ... (15) ...] ...
                        // [                        safe                                          ] unsafe (could be the section_end)
                        //                                                   ^^^^^^^ section_curr (curr_table_processed_byte <= 8)
                        //                                      [   simd_vector_str               ]
                    }
                    else
                    {
                        // If check_table_index_1st or check_table_index_2nd is 0, write directly

                        // check func_counter before write
                        // The check is all done and the status can be changed, if it is less than the value written, then something is wrong.

                        ::uwvm2::parser::wasm::utils::counter_addass_throw_when_overflow(
                            func_counter,
                            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(16u),
                            section_curr,
                            err);

                        // check counter
                        if(func_counter > func_count) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_selectable.u32 = func_count;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        // Since everything is less than 128, there is no need to check the typeidx.

                        // write 16 * 2 byte

                        auto const needwrite_u16x16{::std::bit_cast<u16x16simd>(__builtin_shufflevector(third_fourth_round_simd_u8x32,
                                                                                                        u8x32simd{},
                                                                                                        0,   // 0
                                                                                                        32,  // 1
                                                                                                        1,   // 2
                                                                                                        32,  // 3
                                                                                                        2,   // 4
                                                                                                        32,  // 5
                                                                                                        3,   // 6
                                                                                                        32,  // 7
                                                                                                        4,   // 8
                                                                                                        32,  // 9
                                                                                                        5,   // 10
                                                                                                        32,  // 11
                                                                                                        6,   // 12
                                                                                                        32,  // 13
                                                                                                        7,   // 14
                                                                                                        32,  // 15
                                                                                                        8,   // 16
                                                                                                        32,  // 17
                                                                                                        9,   // 18
                                                                                                        32,  // 19
                                                                                                        10,  // 20
                                                                                                        32,  // 21
                                                                                                        11,  // 22
                                                                                                        32,  // 23
                                                                                                        12,  // 24
                                                                                                        32,  // 25
                                                                                                        13,  // 26
                                                                                                        32,  // 27
                                                                                                        14,  // 28
                                                                                                        32,  // 29
                                                                                                        15,  // 30
                                                                                                        32   // 31
                                                                                                        ))};

                        //  [... curr ... (15) ... curr_next ... (15) ...]
                        //  [                 safe                       ] unsafe (could be the section_end)
                        //       ^^ functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr
                        //      [    needwrite    ]

                        ::std::memcpy(functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr, ::std::addressof(needwrite_u16x16), sizeof(u16x16simd));

                        functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr += 16u;

                        //  [... curr_last ... (15) ... curr ... (15) ...]
                        //  [                  safe                      ] unsafe (could be the section_end)
                        //                              ^^ functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr

                        section_curr += 16u;

                        // [before_section ... | func_count ... typeidx1 ... (15) ... typeidxc ... (15) ...] ...
                        // [                                safe                                           ] unsafe (could be the section_end)
                        //                                                            ^^ section_curr
                        //                                      [                simd_vector_str           ]
                    }
                }
            }
            else
            {
                // All correct, can change state: func_counter, section_curr

                // all are single bytes, so there are 32
                ::uwvm2::parser::wasm::utils::counter_addass_throw_when_overflow(func_counter,
                                                                                 static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(32u),
                                                                                 section_curr,
                                                                                 err);

                // check counter
                if(func_counter > func_count) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_selectable.u32 = func_count;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                // Since everything is less than 128, there is no need to check the typeidx.

                // write data
                auto functionsec_funcs_storage_typeidx_u16_vector_imp_curr_ptr_tmp{functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr};

                u16x16simd const u16x16v0{::std::bit_cast<u16x16simd>(__builtin_shufflevector(simd_vector_str,
                                                                                              u8x32simd{},
                                                                                              0,   // 0
                                                                                              32,  // 1
                                                                                              1,   // 2
                                                                                              32,  // 3
                                                                                              2,   // 4
                                                                                              32,  // 5
                                                                                              3,   // 6
                                                                                              32,  // 7
                                                                                              4,   // 8
                                                                                              32,  // 9
                                                                                              5,   // 10
                                                                                              32,  // 11
                                                                                              6,   // 12
                                                                                              32,  // 13
                                                                                              7,   // 14
                                                                                              32,  // 15
                                                                                              8,   // 16
                                                                                              32,  // 17
                                                                                              9,   // 18
                                                                                              32,  // 19
                                                                                              10,  // 20
                                                                                              32,  // 21
                                                                                              11,  // 22
                                                                                              32,  // 23
                                                                                              12,  // 24
                                                                                              32,  // 25
                                                                                              13,  // 26
                                                                                              32,  // 27
                                                                                              14,  // 28
                                                                                              32,  // 29
                                                                                              15,  // 30
                                                                                              32   // 31
                                                                                              ))};

                ::std::memcpy(functionsec_funcs_storage_typeidx_u16_vector_imp_curr_ptr_tmp, ::std::addressof(u16x16v0), sizeof(u16x16simd));

                functionsec_funcs_storage_typeidx_u16_vector_imp_curr_ptr_tmp += 16uz;

                u16x16simd const u16x16v1{::std::bit_cast<u16x16simd>(__builtin_shufflevector(simd_vector_str,
                                                                                              u8x32simd{},
                                                                                              16,  // 0
                                                                                              32,  // 1
                                                                                              17,  // 2
                                                                                              32,  // 3
                                                                                              18,  // 4
                                                                                              32,  // 5
                                                                                              19,  // 6
                                                                                              32,  // 7
                                                                                              20,  // 8
                                                                                              32,  // 9
                                                                                              21,  // 10
                                                                                              32,  // 11
                                                                                              22,  // 12
                                                                                              32,  // 13
                                                                                              23,  // 14
                                                                                              32,  // 15
                                                                                              24,  // 16
                                                                                              32,  // 17
                                                                                              25,  // 18
                                                                                              32,  // 19
                                                                                              26,  // 20
                                                                                              32,  // 21
                                                                                              27,  // 22
                                                                                              32,  // 23
                                                                                              28,  // 24
                                                                                              32,  // 25
                                                                                              29,  // 26
                                                                                              32,  // 27
                                                                                              30,  // 28
                                                                                              32,  // 29
                                                                                              31,  // 30
                                                                                              32   // 31
                                                                                              ))};

                ::std::memcpy(functionsec_funcs_storage_typeidx_u16_vector_imp_curr_ptr_tmp, ::std::addressof(u16x16v1), sizeof(u16x16simd));

                functionsec_funcs_storage_typeidx_u16_vector_imp_curr_ptr_tmp += 16uz;

                functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr = functionsec_funcs_storage_typeidx_u16_vector_imp_curr_ptr_tmp;

                section_curr += 32uz;

                // [before_section ... | func_count ... typeidx1 ... (31) ...] typeidx32
                // [                        safe                             ] unsafe (could be the section_end)
                //                                                             ^^ section_curr
            }

            // Each write is prefetched one write at a time within a full cache line.

            // After testing, 4 and 6 are the fastest

            ::uwvm2::utils::intrinsics::universal::prefetch<::uwvm2::utils::intrinsics::universal::pfc_mode::read,
                                                            ::uwvm2::utils::intrinsics::universal::pfc_level::nta,
                                                            ::uwvm2::utils::intrinsics::universal::ret_policy::strm>(
                reinterpret_cast<::std::byte const*>(section_curr) + 64u * 4u);
            // Write up to 32 * 2, so prefetch 64 * 6.
            ::uwvm2::utils::intrinsics::universal::prefetch<::uwvm2::utils::intrinsics::universal::pfc_mode::write,
                                                            ::uwvm2::utils::intrinsics::universal::pfc_level::L2,
                                                            ::uwvm2::utils::intrinsics::universal::ret_policy::keep>(
                reinterpret_cast<::std::byte const*>(functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr) + 64u * 6u);
        }

#elif UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__) && UWVM_HAS_BUILTIN(__builtin_shufflevector) &&                           \
    (((defined(__SSSE3__) && UWVM_HAS_BUILTIN(__builtin_ia32_pshufb128) && UWVM_HAS_BUILTIN(__builtin_ia32_palignr128)) &&                                     \
      (defined(__SSE2__) && UWVM_HAS_BUILTIN(__builtin_ia32_pmovmskb128))) ||                                                                                  \
     (defined(__loongarch_sx) && UWVM_HAS_BUILTIN(__builtin_lsx_vshuf_b) && UWVM_HAS_BUILTIN(__builtin_lsx_vsrli_b) &&                                         \
      UWVM_HAS_BUILTIN(__builtin_lsx_vmskltz_b)))

        /// (Little Endian), [[gnu::vector_size]], has mask-u16, can shuffle, simd128
        /// x86_64-ssse3, loongarch-SX

        // This error handle is provided when simd is unable to handle the error, including when it encounters a memory boundary, or when there is an error that
        // needs to be localized.

        auto error_handler{[&](::std::size_t n) constexpr UWVM_THROWS -> void
                           {
                               // Need to ensure that section_curr to section_curr + n is memory safe
                               // Processing uses section_curr_end to guarantee entry into the loop for inspection.
                               // Also use section_end to ensure that the scanning of the last leb128 is handled correctly, both memory safe.

                               auto const section_curr_end{section_curr + n};

            // [ ... typeidx1 ... ] (section_curr_end) ... (outer) ]
            // [     safe     ... ]                    ... (outer) ] unsafe (could be the section_end)
            //       ^^ section_curr

            // Since parse_by_scan uses section_end, it is possible that section_curr will be greater than section_curr_end, use '<' instead
            // of '!='

# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                               // Used to check if correct content is recognized as an error.
                               bool correct_sequence_right_taken_for_wrong{true};
# endif

                               while(section_curr < section_curr_end)
                               {
                                   // The outer boundary is unknown and needs to be rechecked
                                   // [ ... typeidx1] ... (outer) ] typeidx2 ...
                                   // [     safe    ] ... (outer) ] unsafe (could be the section_end)
                                   //       ^^ section_curr

                                   if(::uwvm2::parser::wasm::utils::counter_selfinc_throw_when_overflow(func_counter, section_curr, err) > func_count)
                                       [[unlikely]]
                                   {
                                       err.err_curr = section_curr;
                                       err.err_selectable.u32 = func_count;
                                       err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                                       ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                   }

                                   ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 typeidx;  // No initialization necessary

                                   using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                                   auto const [typeidx_next, typeidx_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                                                   reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                                                   ::fast_io::mnp::leb128_get(typeidx))};

                                   if(typeidx_err != ::fast_io::parse_code::ok) [[unlikely]]
                                   {
                                       err.err_curr = section_curr;
                                       err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_type_index;
                                       ::uwvm2::parser::wasm::base::throw_wasm_parse_code(typeidx_err);
                                   }

                                   // The outer boundary is unknown and needs to be rechecked
                                   // [ ... typeidx1 ...] ... (outer) ] typeidx2 ...
                                   // [      safe       ] ... (outer) ] unsafe (could be the section_end)
                                   //       ^^ section_curr

                                   // There's a good chance there's an error here.
                                   // Or there is a leb redundancy 0
                                   if(typeidx >= type_section_count) [[unlikely]]
                                   {
                                       err.err_curr = section_curr;
                                       err.err_selectable.u32 = typeidx;
                                       err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_type_index;
                                       ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                   }

                                   // write data, func_counter has been checked and is ready to be written.
                                   functionsec.funcs.storage.typeidx_u16_vector.emplace_back_unchecked(
                                       static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u16>(typeidx));

# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                   if(static_cast<::std::size_t>(reinterpret_cast<::std::byte const*>(typeidx_next) - section_curr) > 2uz) [[unlikely]]
                                   {
                                       correct_sequence_right_taken_for_wrong = false;
                                   }
# endif

                                   section_curr = reinterpret_cast<::std::byte const*>(typeidx_next);

                                   // The outer boundary is unknown and needs to be rechecked
                                   // [ ... typeidx1 ...] typeidx2 ...] ...
                                   // [      safe       ] ... (outer) ] unsafe (could be the section_end)
                                   //                     ^^ section_curr
                               }

            // [before_section ... | func_count ... typeidx1 ... (n - 1) ... ...  ] typeidxN
            // [                        safe                                      ] unsafe (could be the section_end)
            //                                                                      ^^ section_curr
            //                                      [   simd_vector_str  ]   ...  ] (Depends on the size of section_curr in relation to
            //                                      section_curr_end)

# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                               if(correct_sequence_right_taken_for_wrong) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif
                           }};

        using i64x2simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::int64_t;
        using u64x2simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint64_t;
        using u32x4simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint32_t;
        using u16x8simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint16_t;
        using c8x16simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = char;
        using u8x16simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint8_t;
        using i8x16simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::int8_t;

        static_assert(::std::same_as<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte, ::std::uint8_t>);

        // No prefetching of table entries is required, ensuring no contamination of the cache

        while(static_cast<::std::size_t>(section_end - section_curr) >= 16uz) [[likely]]
        {
            // [before_section ... | func_count ... typeidx1 ... (15) ...] ...
            // [                        safe                             ] unsafe (could be the section_end)
            //                                      ^^ section_curr
            //                                      [   simd_vector_str  ]

            u8x16simd simd_vector_str;  // No initialization necessary

            ::std::memcpy(::std::addressof(simd_vector_str), section_curr, sizeof(u8x16simd));

            // It's already a little-endian.

            // When the highest bit of each byte is pop, then mask out 1
            auto const check_mask{
# if defined(__SSE2__) && UWVM_HAS_BUILTIN(__builtin_ia32_pmovmskb128)
                static_cast<unsigned>(__builtin_ia32_pmovmskb128(::std::bit_cast<c8x16simd>(simd_vector_str)))
# elif defined(__loongarch_sx) && UWVM_HAS_BUILTIN(__builtin_lsx_vmskltz_b)
                ::std::bit_cast<::uwvm2::utils::container::array<unsigned, 4uz>>(__builtin_lsx_vmskltz_b(::std::bit_cast<i8x16simd>(simd_vector_str)))
                    .front_unchecked()
# else
#  error "missing instructions"
# endif
            };

            // Since mask 16-bit stuff, it can be assumed that mask is less than 2^16
            UWVM_ASSERT(check_mask < 1u << 16u);
            [[assume(check_mask < 1u << 16u)]];

            if(
# if UWVM_HAS_BUILTIN(__builtin_expect_with_probability)
                __builtin_expect_with_probability(static_cast<bool>(check_mask), true, 1.0 / 4.0)
# else
                check_mask
# endif
            )
            {
                ::std::uint16_t const simd_vector_check{static_cast<::std::uint16_t>(type_section_count)};

                auto check_mask_curr{check_mask};  // uleb128 mask

                // Record the number of bytes processed in the first round
                ::std::uint8_t first_round_handle_bytes{static_cast<::std::uint8_t>(8u)};

                // 1st round
                {
                    unsigned const check_table_index{check_mask_curr & 0xFFu};

                    // If check_mask_curr is 0, write directly

                    if(!check_table_index)
                    {
                        // All correct, can change state: func_counter, section_curr

                        // The first 8 bits are single byte typeidx is writable at once.

                        check_mask_curr >>= 8u;

                        // check func_counter before write
                        ::uwvm2::parser::wasm::utils::counter_addass_throw_when_overflow(
                            func_counter,
                            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(8u),
                            section_curr,
                            err);

                        // check counter
                        if(func_counter > func_count) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_selectable.u32 = func_count;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        // Since everything is less than 128, there is no need to check the typeidx.

                        // write 8 x 2 byte
                        //  XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX
                        // [   needwrite_u8x8x2v0  ]
                        //            \|/ x2
                        // [             needwrite_u16x8x2v0               ]

                        auto const needwrite_u16x8{::std::bit_cast<u16x8simd>(
                            __builtin_shufflevector(simd_vector_str, u8x16simd{}, 0, 16, 1, 16, 2, 16, 3, 16, 4, 16, 5, 16, 6, 16, 7, 16))};

                        //  [... curr ... (7) ... curr_next ... (7) ...]
                        //  [                 safe                     ] unsafe (could be the section_end)
                        //       ^^ functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr
                        //      [    needwrite   ]

                        ::std::memcpy(functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr, ::std::addressof(needwrite_u16x8), sizeof(u16x8simd));

                        functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr += 8u;

                        //  [... curr_last ... (7) ... curr ... (7) ...]
                        //  [                  safe                    ] unsafe (could be the section_end)
                        //                             ^^ functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr

                        section_curr += 8u;

                        // [before_section ... | func_count ... typeidx1 ... (7) ... typeidxc ... (7) ...] ...
                        // [                                safe                                         ] unsafe (could be the section_end)
                        //                                                           ^^ section_curr
                        //                                      [                simd_vector_str         ]
                    }
                    else
                    {
                        //  [...] curr ...
                        //  [sf ] unsafe (could be the section_end)
                        //        ^^ functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr

                        // The write size is 8 bits, but the valid data may be less than 8 bits, directly check the maximum value, more than that, then enter
                        // the tail processing Cannot change state until error checking: func_counter, section_curr

                        auto const func_counter_end{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(func_counter + 8u)};

                        if(func_counter_end < func_counter) [[unlikely]]
                        {
                            // It's overflowing. It's jumping right out.
                            break;
                        }

                        if(func_counter_end > func_count) [[unlikely]]
                        {
                            // Near the end, jump directly out of the simd section and hand it over to the tail.
                            break;
                        }

                        //  [... curr ... (7) ...]
                        //  [        safe        ] unsafe (could be the section_end)
                        //       ^^ functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr
                        //      [ needwrite ] ...] (Always writes 8 bits, but valid data may be less than 8 bits)

                        auto const& curr_table{simd128_shuffle_table.index_unchecked(static_cast<::std::size_t>(check_table_index))};
                        auto const curr_table_shuffle_mask{curr_table.shuffle_mask};
                        auto const curr_table_processed_simd{curr_table.processed_simd};  // size of handled u32
                        auto const curr_table_processed_byte{curr_table.processed_byte};  // size of handled uleb128

                        first_round_handle_bytes = static_cast<::std::uint8_t>(curr_table_processed_byte);

                        // When the number of consecutive bits is greater than 2, switch back to the normal processing method
                        if(!curr_table_processed_simd) [[unlikely]]
                        {
                            // [before_section ... | func_count ... typeidx1 ... (15) ...] ...
                            // [                        safe                             ] unsafe (could be the section_end)
                            //                                      ^^ section_curr
                            //                                                             ^^ section_curr + 16uz

                            // If not yet processed in the first round, it can be processed up to 16
                            error_handler(16uz);
                            // Start the next round straight away
                            continue;
                        }

                        // Remove the second round of processing for the next round of calculations
                        check_mask_curr >>= curr_table_processed_byte;

                        //  [... curr ... (7) ... curr_next ... (7) ...]
                        //  [                 safe                     ] unsafe (could be the section_end)
                        //       ^^ functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr
                        //      [    needwrite   ]
                        //      [    ctps  ]  ...]

                        // shuffle and write

                        u16x8simd mask_res;

# if defined(__SSSE3__) && UWVM_HAS_BUILTIN(__builtin_ia32_pshufb128)
                        mask_res = ::std::bit_cast<u16x8simd>(__builtin_ia32_pshufb128(simd_vector_str, curr_table_shuffle_mask));
# elif defined(__loongarch_sx) && UWVM_HAS_BUILTIN(__builtin_lsx_vshuf_b)
                        mask_res = ::std::bit_cast<u16x8simd>(__builtin_lsx_vshuf_b(simd_vector_str, simd_vector_str, curr_table_shuffle_mask));
# else
#  error "missing instructions"
# endif

                        // The leb128 that has been merged into u16x8 is finalized to get the value corresponding to u16

                        auto const res{(mask_res & static_cast<::std::uint16_t>(0x7Fu)) | ((mask_res & static_cast<::std::uint16_t>(0x7F00u)) >> 1u)};

                        // The data out of shuffle is 16-bit [0, 2^14) and may be greater than or equal to typeidx, which needs to be checked.

                        auto const check_upper{res >= simd_vector_check};

                        if(
# if defined(__SSE4_1__) && UWVM_HAS_BUILTIN(__builtin_ia32_ptestz128)
                            !__builtin_ia32_ptestz128(::std::bit_cast<i64x2simd>(check_upper), ::std::bit_cast<i64x2simd>(check_upper))
# elif defined(__SSE2__) && UWVM_HAS_BUILTIN(__builtin_ia32_pmovmskb128)
                            __builtin_ia32_pmovmskb128(::std::bit_cast<c8x16simd>(check_upper))
# elif defined(__loongarch_sx) && UWVM_HAS_BUILTIN(__builtin_lsx_bnz_v)
                            __builtin_lsx_bnz_v(::std::bit_cast<u8x16simd>(check_upper))  /// @todo need check
# else
#  error "missing instructions"
# endif
                                ) [[unlikely]]
                        {
                            // [before_section ... | func_count ... typeidx1 ... (15) ...] ...
                            // [                        safe                             ] unsafe (could be the section_end)
                            //                                      ^^ section_curr
                            //                                                             ^^ section_curr + 16uz

                            // If not yet processed in the first round, it can be processed up to 16
                            // Ensure that there are no modifications to the state: func_counter, section_curr
                            error_handler(16uz);
                            // Start the next round straight away
                            continue;
                        }

                        // All correct, can change state: func_counter, section_curr

                        // check func_counter before write
                        // Checked. No overflow.
                        func_counter += curr_table_processed_simd;

                        // There is no need to check function_counter, because curr_table_processed_simd is always less than or equal to 8u.

                        auto const needwrite_u16x8{::std::bit_cast<u16x8simd>(res)};

                        //  [... curr ... (7) ... curr_next ... (7) ...]
                        //  [                 safe                     ] unsafe (could be the section_end)
                        //       ^^ functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr
                        //      [    needwrite   ]

                        ::std::memcpy(functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr, ::std::addressof(needwrite_u16x8), sizeof(u16x8simd));

                        functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr += curr_table_processed_simd;

                        //  [... curr ... (7) ... curr_next ... (7) ...]
                        //  [                 safe                     ] unsafe (could be the section_end)
                        //                    ^^ functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr
                        // Or: (Security boundaries for writes are checked)
                        //                        ^^ functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr
                        //      [    needwrite   ]

                        section_curr += curr_table_processed_byte;

                        // [before_section ... | func_count ... typeidx1 ... (7) ... ... (7) ...] ...
                        // [                        safe                                        ] unsafe (could be the section_end)
                        //                                                   ^^^^^^^ section_curr (curr_table_processed_byte <= 8)
                        //                                      [   simd_vector_str             ]
                    }
                }

                // 2nd round
                {
                    unsigned const check_table_index{check_mask_curr & 0xFFu};

                    // There are only 7 and 8 and 0 in the table, where the 0 have been handled out of the error,
                    // and if a non-7 or non-8 is encountered, there is a problem with the surface

                    // When first_round_handle_bytes is 7, check_table_index is always greater than 0，
                    // Because the highest bit of the first 8 bits is pop, 0bxxxx'xxxx'1xxx'xxxxu >> 7u == 0bxxxx'xxx1
                    UWVM_ASSERT((first_round_handle_bytes == static_cast<::std::uint8_t>(7u) && check_table_index != 0u) ||
                                first_round_handle_bytes == static_cast<::std::uint8_t>(8u));
                    [[assume((first_round_handle_bytes == static_cast<::std::uint8_t>(7u) && check_table_index != 0u) ||
                             first_round_handle_bytes == static_cast<::std::uint8_t>(8u))]];

                    if(!check_table_index)
                    {
                        // It's the last round, but since it's already established, it's better to calculate it directly.

                        // All correct, can change state: func_counter, section_curr

                        // The first 8 bits are single byte typeidx is writable at once.

                        // last round no necessary to check_mask_curr >>= 8u;

                        // check func_counter before write
                        ::uwvm2::parser::wasm::utils::counter_addass_throw_when_overflow(
                            func_counter,
                            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(8u),
                            section_curr,
                            err);

                        // check counter
                        if(func_counter > func_count) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_selectable.u32 = func_count;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        // Since everything is less than 128, there is no need to check the typeidx.

                        // Requires a vector right shift to implement

                        //  XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX
                        // [       last write      ]
                        //                      ^^^^^^ curr (begin + first_round_handle_bytes)
                        //                      [      need write       ]..]

                        // When first_round_handle_bytes is 7, check_table_index is always greater than 0，
                        // Because the highest bit of the first 8 bits is pop, 0bxxxx'xxxx'1xxx'xxxxu >> 7u == 0bxxxx'xxx1

                        UWVM_ASSERT(first_round_handle_bytes == static_cast<::std::uint8_t>(8u));
                        [[assume(first_round_handle_bytes == static_cast<::std::uint8_t>(8u))]];

                        // Here the shuffle simulation element is kept shifted right by 8 bits to facilitate subsequent expansion,
                        // while the subsequent taking of the first address has been shown to be optimized by the llvm into a single instruction

                        u16x8simd const simd_vector_str_need_write{::std::bit_cast<u16x8simd>(
                            __builtin_shufflevector(simd_vector_str, u8x16simd{}, 8, 16, 9, 16, 10, 16, 11, 16, 12, 16, 13, 16, 14, 16, 15, 16))};

                        //  [... curr_old ... (7) ... curr ... (7) ...] ...
                        //  [                 safe                    ] unsafe (could be the section_end)
                        //                        ^^^^^^ functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr
                        //                       [    needwrite   ]...]

                        ::std::memcpy(functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr,
                                      ::std::addressof(simd_vector_str_need_write),
                                      sizeof(u16x8simd));

                        functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr += 8u;

                        //  [... curr_last ... (7) ... curr ... (7) ...] ...
                        //  [                  safe                    ] unsafe (could be the section_end)
                        //                                           ^^^^^^ functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr

                        section_curr += 8u;

                        // [before_section ... | func_count ... typeidx1 ... (7) ... typeidxc ... (7) ...] ...
                        // [                                safe                                         ] unsafe (could be the section_end)
                        //                                                                             ^^^^^^ section_curr
                        //                                      [                simd_vector_str         ]
                    }
                    else
                    {
                        // The write size is 8 bits, but the valid data may be less than 8 bits, directly check the maximum value, more than that, then enter
                        // the tail processing

                        //  [...] curr ... (curr + 8u)
                        //  [sf ] unsafe (could be the section_end)
                        //  [ safe  ... ] unsafe (Left over from above, length unknown, can't be used)
                        //        ^^ functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr

                        // Cannot change state until error checking: func_counter, section_curr

                        auto const func_counter_end{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(func_counter + 8u)};

                        if(func_counter_end < func_counter) [[unlikely]]
                        {
                            // It's overflowing. It's jumping right out.
                            break;
                        }

                        if(func_counter_end > func_count) [[unlikely]]
                        {
                            // Near the end, jump directly out of the simd section and hand it over to the tail.
                            break;
                        }

                        //  [... curr ... (7) ...]
                        //  [        safe        ] unsafe (could be the section_end)
                        //       ^^ functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr
                        //      [ needwrite ] ...] (Always writes 8 bits, but valid data may be less than 8 bits)

                        auto const& curr_table{simd128_shuffle_table.index_unchecked(static_cast<::std::size_t>(check_table_index))};
                        // Since it's the second round, the data has to be moved back  the number of bytes processed in the first round
                        auto const curr_table_shuffle_mask{curr_table.shuffle_mask + first_round_handle_bytes};
                        auto const curr_table_processed_simd{curr_table.processed_simd};  // size of handled u32
                        auto const curr_table_processed_byte{curr_table.processed_byte};  // size of handled uleb128

                        // When the number of consecutive bits is greater than 2, switch back to the normal processing method
                        if(!curr_table_processed_simd) [[unlikely]]
                        {
                            // [before_section ... | func_count ... typeidx1 ... (7) ... ... (7) ...] ...
                            // [                        safe                                        ] unsafe (could be the section_end)
                            //                                                   ^^^ ^^^ section_curr (indeterminate location)
                            //                                                                   ^^^ ^^^ section_curr + 8uz

                            // Second round can only handle 16 - first round max 8 = 8
                            // Ensure that there are no modifications to the state: func_counter, section_curr
                            error_handler(8uz);
                            // Start the next round straight away
                            continue;
                        }

                        // last round no necessary to check_mask_curr >>= curr_table_processed_byte;

                        // shuffle and write

                        u16x8simd mask_res;

# if defined(__SSSE3__) && UWVM_HAS_BUILTIN(__builtin_ia32_pshufb128)
                        mask_res = ::std::bit_cast<u16x8simd>(__builtin_ia32_pshufb128(simd_vector_str, curr_table_shuffle_mask));
# elif defined(__loongarch_sx) && UWVM_HAS_BUILTIN(__builtin_lsx_vshuf_b)
                        mask_res = ::std::bit_cast<u16x8simd>(__builtin_lsx_vshuf_b(simd_vector_str, simd_vector_str, curr_table_shuffle_mask));
# else
#  error "missing instructions"
# endif

                        // The leb128 that has been merged into u16x8 is finalized to get the value corresponding to u16

                        auto const res{(mask_res & static_cast<::std::uint16_t>(0x7Fu)) | ((mask_res & static_cast<::std::uint16_t>(0x7F00u)) >> 1u)};

                        // The data out of shuffle is 16-bit [0, 2^14) and may be greater than or equal to typeidx, which needs to be checked.

                        auto const check_upper{res >= simd_vector_check};

                        if(
# if defined(__SSE4_1__) && UWVM_HAS_BUILTIN(__builtin_ia32_ptestz128)
                            !__builtin_ia32_ptestz128(::std::bit_cast<i64x2simd>(check_upper), ::std::bit_cast<i64x2simd>(check_upper))
# elif defined(__SSE2__) && UWVM_HAS_BUILTIN(__builtin_ia32_pmovmskb128)
                            __builtin_ia32_pmovmskb128(::std::bit_cast<c8x16simd>(check_upper))
# elif defined(__loongarch_sx) && UWVM_HAS_BUILTIN(__builtin_lsx_bnz_v)
                            __builtin_lsx_bnz_v(::std::bit_cast<u8x16simd>(check_upper))  /// @todo need check
# else
#  error "missing instructions"
# endif
                                ) [[unlikely]]
                        {
                            // [before_section ... | func_count ... typeidx1 ... (7) ... ... (7) ...] ...
                            // [                        safe                                        ] unsafe (could be the section_end)
                            //                                                   ^^^^^^^ section_curr (indeterminate location)
                            //                                                                   ^^^^^^^ section_curr + 8uz

                            // Second round can only handle 16 - first round max 8 = 8
                            error_handler(8uz);
                            // Start the next round straight away
                            continue;
                        }

                        // All correct, can change state: func_counter, section_curr

                        // check func_counter before write
                        // Checked. No overflow.
                        func_counter += curr_table_processed_simd;

                        // There is no need to check function_counter, because curr_table_processed_simd is always less than or equal to 8u.

                        auto const needwrite_u16x8{::std::bit_cast<u16x8simd>(res)};

                        //  [... curr_old ... (7) ... curr ... (7) ... ]
                        //  [                 safe                     ] unsafe (could be the section_end)
                        //                            ^^ functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr
                        //                            [    needwrite   ]

                        ::std::memcpy(functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr, ::std::addressof(needwrite_u16x8), sizeof(u16x8simd));

                        functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr += curr_table_processed_simd;

                        //  [... curr_old ... (7) ... curr ... (7) ... ]
                        //  [                 safe                     ] unsafe (could be the section_end)
                        //                                               ^^ functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr
                        //                            [    needwrite   ]

                        section_curr += curr_table_processed_byte;

                        // [before_section ... | func_count ... typeidx1 ... (7) ... ... (7) ...] ...
                        // [                        safe                                        ] unsafe (could be the section_end)
                        //                                                                   ^^^^^^^ section_curr (curr_table_processed_byte <= 8)
                        //                                      [   simd_vector_str             ]
                    }
                }
            }
            else
            {
                // All correct, can change state: func_counter, section_curr

                // all are single bytes, so there are 16
                ::uwvm2::parser::wasm::utils::counter_addass_throw_when_overflow(func_counter,
                                                                                 static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(16u),
                                                                                 section_curr,
                                                                                 err);

                // check counter
                if(func_counter > func_count) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_selectable.u32 = func_count;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                // Since everything is less than 128, there is no need to check the typeidx.

                // write data
                auto functionsec_funcs_storage_typeidx_u16_vector_imp_curr_ptr_tmp{functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr};

                u16x8simd const u16x8v0{
                    ::std::bit_cast<u16x8simd>(__builtin_shufflevector(simd_vector_str, u8x16simd{}, 0, 16, 1, 16, 2, 16, 3, 16, 4, 16, 5, 16, 6, 16, 7, 16))};

                ::std::memcpy(functionsec_funcs_storage_typeidx_u16_vector_imp_curr_ptr_tmp, ::std::addressof(u16x8v0), sizeof(u16x8simd));

                functionsec_funcs_storage_typeidx_u16_vector_imp_curr_ptr_tmp += 8uz;

                u16x8simd const u16x8v1{::std::bit_cast<u16x8simd>(
                    __builtin_shufflevector(simd_vector_str, u8x16simd{}, 8, 16, 9, 16, 10, 16, 11, 16, 12, 16, 13, 16, 14, 16, 15, 16))};

                ::std::memcpy(functionsec_funcs_storage_typeidx_u16_vector_imp_curr_ptr_tmp, ::std::addressof(u16x8v1), sizeof(u16x8simd));

                functionsec_funcs_storage_typeidx_u16_vector_imp_curr_ptr_tmp += 8uz;

                functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr = functionsec_funcs_storage_typeidx_u16_vector_imp_curr_ptr_tmp;

                section_curr += 16uz;

                // [before_section ... | func_count ... typeidx1 ... (15) ...] typeidx16
                // [                        safe                             ] unsafe (could be the section_end)
                //                                                             ^^ section_curr
            }

            // After testing, 2 and 3 are the fastest

            // Each write is prefetched one write at a time within a full cache line.
            ::uwvm2::utils::intrinsics::universal::prefetch<::uwvm2::utils::intrinsics::universal::pfc_mode::read,
                                                            ::uwvm2::utils::intrinsics::universal::pfc_level::nta,
                                                            ::uwvm2::utils::intrinsics::universal::ret_policy::strm>(
                reinterpret_cast<::std::byte const*>(section_curr) + 64u * 2u);
            // Write up to 16 * 2, so prefetch 64 * 3.
            ::uwvm2::utils::intrinsics::universal::prefetch<::uwvm2::utils::intrinsics::universal::pfc_mode::write,
                                                            ::uwvm2::utils::intrinsics::universal::pfc_level::L2,
                                                            ::uwvm2::utils::intrinsics::universal::ret_policy::keep>(
                reinterpret_cast<::std::byte const*>(functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr) + 64u * 3u);
        }
#elif UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__) &&                                                                        \
    ((defined(__SSE2__) && UWVM_HAS_BUILTIN(__builtin_ia32_pmovmskb128)) ||                                                                                    \
     (defined(__ARM_NEON) && (UWVM_HAS_BUILTIN(__builtin_neon_vmaxvq_u32) || UWVM_HAS_BUILTIN(__builtin_aarch64_reduc_umax_scal_v4si_uu))) ||                  \
     (defined(__wasm_simd128__) && (UWVM_HAS_BUILTIN(__builtin_wasm_bitmask_i8x16) || UWVM_HAS_BUILTIN(__builtin_wasm_all_true_i8x16))))
        /// (Little Endian), [[gnu::vector_size]], no mask-u16, simd128
        ///  x86_64-sse2, aarch64-neon, wasm-simd128 (Since the wasm simd128 shuffle requirement is immediate, it cannot be used.)
        ///  Since the arm neon does not have a move mask, the shuffle algorithm cannot be used.

        using i64x2simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::int64_t;
        using u64x2simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint64_t;
        using u32x4simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint32_t;
        using u16x8simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint16_t;
        using c8x16simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = char;
        using u8x16simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint8_t;
        using i8x16simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::int8_t;

        static_assert(::std::same_as<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte, ::std::uint8_t>);

        while(static_cast<::std::size_t>(section_end - section_curr) >= 16uz) [[likely]]
        {
            // [before_section ... | func_count ... typeidx1 ... (15) ...] ...
            // [                        safe                             ] unsafe (could be the section_end)
            //                                      ^^ section_curr
            //                                      [   simd_vector_str  ]

            u8x16simd simd_vector_str;  // No initialization necessary

            ::std::memcpy(::std::addressof(simd_vector_str), section_curr, sizeof(u8x16simd));

            // It's already a little-endian.

            // Using "x > 127u" instead of "x & 0x80u" guarantees that the result is fully bit-set. The rest is left to the compiler to optimize
            auto const check_highest_bit{simd_vector_str > static_cast<::std::uint8_t>(127u)};

            auto const has_pop_highest_bit{
# if defined(__SSE2__) && UWVM_HAS_BUILTIN(__builtin_ia32_pmovmskb128)
                __builtin_ia32_pmovmskb128(::std::bit_cast<c8x16simd>(check_highest_bit))
# elif defined(__ARM_NEON) && UWVM_HAS_BUILTIN(__builtin_neon_vmaxvq_u32)                  // Only supported by clang
                __builtin_neon_vmaxvq_u32(::std::bit_cast<u32x4simd>(check_highest_bit))
# elif defined(__ARM_NEON) && UWVM_HAS_BUILTIN(__builtin_aarch64_reduc_umax_scal_v4si_uu)  // Only supported by GCC
                __builtin_aarch64_reduc_umax_scal_v4si_uu(::std::bit_cast<u32x4simd>(check_highest_bit))
# elif defined(__wasm_simd128__) && UWVM_HAS_BUILTIN(__builtin_wasm_all_true_i8x16)
                !__builtin_wasm_all_true_i8x16(::std::bit_cast<i8x16simd>(~check_highest_bit))
# elif defined(__wasm_simd128__) && UWVM_HAS_BUILTIN(__builtin_wasm_bitmask_i8x16)
                __builtin_wasm_bitmask_i8x16(::std::bit_cast<i8x16simd>(check_highest_bit))
# else
#  error "missing instructions"
# endif
            };

            if(
# if UWVM_HAS_BUILTIN(__builtin_expect_with_probability)
                __builtin_expect_with_probability(static_cast<bool>(has_pop_highest_bit), true, 1.0 / 4.0)
# else
                has_pop_highest_bit
# endif
            )
            {
                auto const section_curr_end{section_curr + 16u};

                // Since parse_by_scan uses section_end, it is possible that section_curr will be greater than section_curr_end, use '<' instead of '!='

                while(section_curr < section_curr_end)
                {
                    // The outer boundary is unknown and needs to be rechecked
                    // [ ... typeidx1] ... (outer) ] typeidx2 ...
                    // [     safe    ] ... (outer) ] unsafe (could be the section_end)
                    //       ^^ section_curr

                    if(::uwvm2::parser::wasm::utils::counter_selfinc_throw_when_overflow(func_counter, section_curr, err) > func_count) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_selectable.u32 = func_count;
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 typeidx;  // No initialization necessary

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    auto const [typeidx_next, typeidx_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                                    reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                                    ::fast_io::mnp::leb128_get(typeidx))};

                    if(typeidx_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_type_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(typeidx_err);
                    }

                    // The outer boundary is unknown and needs to be rechecked
                    // [ ... typeidx1 ...] ... (outer) ] typeidx2 ...
                    // [      safe       ] ... (outer) ] unsafe (could be the section_end)
                    //       ^^ section_curr

                    // There's a good chance there's an error here.
                    // Or there is a leb redundancy 0
                    if(typeidx >= type_section_count) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_selectable.u32 = typeidx;
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_type_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // write data, func_counter has been checked and is ready to be written.
                    functionsec.funcs.storage.typeidx_u16_vector.emplace_back_unchecked(
                        static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u16>(typeidx));

                    section_curr = reinterpret_cast<::std::byte const*>(typeidx_next);

                    // The outer boundary is unknown and needs to be rechecked
                    // [ ... typeidx1 ...] typeidx2 ...] ...
                    // [      safe       ] ... (outer) ] unsafe (could be the section_end)
                    //                     ^^ section_curr
                }

                // [before_section ... | func_count ... typeidx1 ... (15) ... ...  ] typeidxN
                // [                        safe                                   ] unsafe (could be the section_end)
                //                                                                   ^^ section_curr
                //                                      [   simd_vector_str  ] ... ] (Depends on the size of section_curr in relation to section_curr_end)
            }
            else
            {
                // All correct, can change state: func_counter, section_curr

                // all are single bytes, so there are 16
                ::uwvm2::parser::wasm::utils::counter_addass_throw_when_overflow(func_counter,
                                                                                 static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(16u),
                                                                                 section_curr,
                                                                                 err);

                // check counter
                if(func_counter > func_count) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_selectable.u32 = func_count;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                // Since everything is less than 128, there is no need to check the typeidx.

                // write data
                auto functionsec_funcs_storage_typeidx_u16_vector_imp_curr_ptr_tmp{functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr};

                u16x8simd const u16x8v0{
                    ::std::bit_cast<u16x8simd>(__builtin_shufflevector(simd_vector_str, u8x16simd{}, 0, 16, 1, 16, 2, 16, 3, 16, 4, 16, 5, 16, 6, 16, 7, 16))};

                ::std::memcpy(functionsec_funcs_storage_typeidx_u16_vector_imp_curr_ptr_tmp, ::std::addressof(u16x8v0), sizeof(u16x8simd));

                functionsec_funcs_storage_typeidx_u16_vector_imp_curr_ptr_tmp += 8uz;

                u16x8simd const u16x8v1{::std::bit_cast<u16x8simd>(
                    __builtin_shufflevector(simd_vector_str, u8x16simd{}, 8, 16, 9, 16, 10, 16, 11, 16, 12, 16, 13, 16, 14, 16, 15, 16))};

                ::std::memcpy(functionsec_funcs_storage_typeidx_u16_vector_imp_curr_ptr_tmp, ::std::addressof(u16x8v1), sizeof(u16x8simd));

                functionsec_funcs_storage_typeidx_u16_vector_imp_curr_ptr_tmp += 8uz;

                functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr = functionsec_funcs_storage_typeidx_u16_vector_imp_curr_ptr_tmp;

                section_curr += 16uz;

                // [before_section ... | func_count ... typeidx1 ... (15) ...] typeidx16
                // [                        safe                             ] unsafe (could be the section_end)
                //                                                             ^^ section_curr
            }

            // Each write is prefetched one write at a time within a full cache line.

            // After testing, 2 and 3 are the fastest

            ::uwvm2::utils::intrinsics::universal::prefetch<::uwvm2::utils::intrinsics::universal::pfc_mode::read,
                                                            ::uwvm2::utils::intrinsics::universal::pfc_level::nta,
                                                            ::uwvm2::utils::intrinsics::universal::ret_policy::strm>(
                reinterpret_cast<::std::byte const*>(section_curr) + 64u * 2u);
            ::uwvm2::utils::intrinsics::universal::prefetch<::uwvm2::utils::intrinsics::universal::pfc_mode::write,
                                                            ::uwvm2::utils::intrinsics::universal::pfc_level::L2,
                                                            ::uwvm2::utils::intrinsics::universal::ret_policy::keep>(
                reinterpret_cast<::std::byte const*>(functionsec.funcs.storage.typeidx_u16_vector.imp.curr_ptr) + 64u * 3u);
        }
#endif

        // tail handling
        /// @todo support AVX512vbmi2
        /// @todo support sve2

#if defined(_MSC_VER) && !defined(__clang__)
        // MSVC, x86_64, i386, aarch64, arm, arm64ec, ...
        // msvc, avoid macro-contamination

        while(section_curr != section_end) [[likely]]
        {
            // Ensuring the existence of valid information

            // [ ... typeidx1] ... typeidx2 ...
            // [     safe    ] unsafe (could be the section_end)
            //       ^^ section_curr

            // check function counter
            // Ensure content is available before counting (section_curr != section_end)
            if(::uwvm2::parser::wasm::utils::counter_selfinc_throw_when_overflow(func_counter, section_curr, err) > func_count) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u32 = func_count;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // [ ... typeidx1] ... typeidx2 ...
            // [     safe    ] unsafe (could be the section_end)
            //       ^^ section_curr

            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 typeidx;  // No initialization necessary

            using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

            auto const [typeidx_next, typeidx_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                            ::fast_io::mnp::leb128_get(typeidx))};

            if(typeidx_err != ::fast_io::parse_code::ok) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_type_index;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(typeidx_err);
            }

            // [ ... typeidx1 ...] typeidx2 ...
            // [      safe       ] unsafe (could be the section_end)
            //       ^^ section_curr

            // check: type_index should less than type_section_count
            if(typeidx >= type_section_count) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u32 = typeidx;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_type_index;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // write data, func_counter has been checked and is ready to be written.
            functionsec.funcs.storage.typeidx_u16_vector.emplace_back_unchecked(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u16>(typeidx));

            section_curr = reinterpret_cast<::std::byte const*>(typeidx_next);

            // [ ... typeidx1 ...] typeidx2 ...
            // [      safe       ] unsafe (could be the section_end)
            //                     ^^ section_curr
        }

        // [before_section ... | func_count ... typeidx1 ... ...] (end)
        // [                       safe                         ] unsafe (could be the section_end)
        //                                                        ^^ section_curr

#else
        // default

        while(section_curr != section_end) [[likely]]
        {
            // Ensuring the existence of valid information

            // [ ... typeidx1] ... typeidx2 ...
            // [     safe    ] unsafe (could be the section_end)
            //       ^^ section_curr

            // check function counter
            // Ensure content is available before counting (section_curr != section_end)
            if(::uwvm2::parser::wasm::utils::counter_selfinc_throw_when_overflow(func_counter, section_curr, err) > func_count) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u32 = func_count;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // [ ... typeidx1] ... typeidx2 ...
            // [     safe    ] unsafe (could be the section_end)
            //       ^^ section_curr

            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 typeidx;  // No initialization necessary

            using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

            auto const [typeidx_next, typeidx_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                            ::fast_io::mnp::leb128_get(typeidx))};

            if(typeidx_err != ::fast_io::parse_code::ok) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_type_index;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(typeidx_err);
            }

            // [ ... typeidx1 ...] typeidx2 ...
            // [      safe       ] unsafe (could be the section_end)
            //       ^^ section_curr

            // check: type_index should less than type_section_count
            if(typeidx >= type_section_count) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u32 = typeidx;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_type_index;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // write data, func_counter has been checked and is ready to be written.
            functionsec.funcs.storage.typeidx_u16_vector.emplace_back_unchecked(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u16>(typeidx));

            section_curr = reinterpret_cast<::std::byte const*>(typeidx_next);

            // [ ... typeidx1 ...] typeidx2 ...
            // [      safe       ] unsafe (could be the section_end)
            //                     ^^ section_curr
        }

        // [before_section ... | func_count ... typeidx1 ... ...] (end)
        // [                       safe                         ] unsafe (could be the section_end)
        //                                                        ^^ section_curr

#endif

        return section_curr;
    }

    /// @brief      Maximum typeidx in [2^14, 2^16)
    /// @details    Storing a typeidx takes up 2 bytes, typeidx corresponding uleb128 varies from 1-3 bytes, linear read/write, no simd optimization
    ///             Sequential scanning, correctly handling all cases of uleb128 u32, allowing up to 5 bytes.
    ///             simd has basically no advantage in this area of processing
    ///             If your wasm file has this many typeidx, please post an issue requesting the adaptation of simd!
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* scan_function_section_impl_u16_3b(
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<function_section_storage_t> sec_adl,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para,
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32& func_counter,
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 const func_count) UWVM_THROWS
    {
        auto const& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<type_section_storage_t<Fs...>>(module_storage.sections)};
        auto const type_section_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(typesec.types.size())};

        UWVM_ASSERT(type_section_count >= static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 14u &&
                    type_section_count < static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 16u);
        [[assume(type_section_count >= static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 14u &&
                 type_section_count < static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 16u)]];

        auto& functionsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<function_section_storage_t>(module_storage.sections)};

        // Storing a typeidx takes up 2 bytes, use u16_vector
        functionsec.funcs.mode = ::uwvm2::parser::wasm::standard::wasm1::features::vectypeidx_minimize_storage_mode::u16_vector;

        // The size comparison between u32 and size_t has already been checked
        functionsec.funcs.storage.typeidx_u16_vector.reserve(static_cast<::std::size_t>(func_count));

        // [before_section ... | func_count ...] typeidx1 ... typeidx2 ...
        // [              safe                 ] unsafe (could be the section_end)
        //                                       ^^ section_curr

        while(section_curr != section_end) [[likely]]
        {
            // Ensuring the existence of valid information

            // [ ... typeidx1] ... typeidx2 ...
            // [     safe    ] unsafe (could be the section_end)
            //       ^^ section_curr

            // check function counter
            // Ensure content is available before counting (section_curr != section_end)
            if(::uwvm2::parser::wasm::utils::counter_selfinc_throw_when_overflow(func_counter, section_curr, err) > func_count) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u32 = func_count;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // [ ... typeidx1] ... typeidx2 ...
            // [     safe    ] unsafe (could be the section_end)
            //       ^^ section_curr

            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 typeidx;  // No initialization necessary

            using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

            auto const [typeidx_next, typeidx_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                            ::fast_io::mnp::leb128_get(typeidx))};

            if(typeidx_err != ::fast_io::parse_code::ok) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_type_index;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(typeidx_err);
            }

            // [ ... typeidx1 ...] typeidx2 ...
            // [      safe       ] unsafe (could be the section_end)
            //       ^^ section_curr

            // check: type_index should less than type_section_count
            if(typeidx >= type_section_count) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u32 = typeidx;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_type_index;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // write data, func_counter has been checked and is ready to be written.
            functionsec.funcs.storage.typeidx_u16_vector.emplace_back_unchecked(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u16>(typeidx));

            section_curr = reinterpret_cast<::std::byte const*>(typeidx_next);

            // [ ... typeidx1 ...] typeidx2 ...
            // [      safe       ] unsafe (could be the section_end)
            //                     ^^ section_curr
        }

        // [before_section ... | func_count ... typeidx1 ... ...] (end)
        // [                       safe                         ] unsafe (could be the section_end)
        //                                                        ^^ section_curr

        return section_curr;
    }

    /// @brief      Maximum typeidx in [2^16, 2^32)
    /// @details    Storing a typeidx takes up 4 bytes, typeidx corresponding uleb128 varies from 1-5 bytes, linear read/write, no simd optimization
    ///             Sequential scanning, correctly handling all cases of uleb128 u32, allowing up to 5 bytes.
    ///             simd has basically no advantage in this area of processing
    ///             If your wasm file has this many typeidx, please post an issue requesting the adaptation of simd!
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* scan_function_section_impl_u32_5b(
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<function_section_storage_t> sec_adl,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para,
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32& func_counter,
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 const func_count) UWVM_THROWS
    {
        auto const& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<type_section_storage_t<Fs...>>(module_storage.sections)};
        auto const type_section_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(typesec.types.size())};

        UWVM_ASSERT(type_section_count >= static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 16u);
        [[assume(type_section_count >= static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 16u)]];

        auto& functionsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<function_section_storage_t>(module_storage.sections)};

        // Storing a typeidx takes up 4 bytes, use u32_vector
        functionsec.funcs.mode = ::uwvm2::parser::wasm::standard::wasm1::features::vectypeidx_minimize_storage_mode::u32_vector;

        // The size comparison between u32 and size_t has already been checked
        functionsec.funcs.storage.typeidx_u32_vector.reserve(static_cast<::std::size_t>(func_count));

        // [before_section ... | func_count ...] typeidx1 ... typeidx2 ...
        // [              safe                 ] unsafe (could be the section_end)
        //                                       ^^ section_curr

        while(section_curr != section_end) [[likely]]
        {
            // Ensuring the existence of valid information

            // [ ... typeidx1] ... typeidx2 ...
            // [     safe    ] unsafe (could be the section_end)
            //       ^^ section_curr

            // check function counter
            // Ensure content is available before counting (section_curr != section_end)
            if(::uwvm2::parser::wasm::utils::counter_selfinc_throw_when_overflow(func_counter, section_curr, err) > func_count) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u32 = func_count;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // [ ... typeidx1] ... typeidx2 ...
            // [     safe    ] unsafe (could be the section_end)
            //       ^^ section_curr

            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 typeidx;  // No initialization necessary

            using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

            auto const [typeidx_next, typeidx_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                            ::fast_io::mnp::leb128_get(typeidx))};

            if(typeidx_err != ::fast_io::parse_code::ok) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_type_index;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(typeidx_err);
            }

            // [ ... typeidx1 ...] typeidx2 ...
            // [      safe       ] unsafe (could be the section_end)
            //       ^^ section_curr

            // check: type_index should less than type_section_count
            if(typeidx >= type_section_count) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u32 = typeidx;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_type_index;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // write data, func_counter has been checked and is ready to be written.
            functionsec.funcs.storage.typeidx_u32_vector.emplace_back_unchecked(typeidx);

            section_curr = reinterpret_cast<::std::byte const*>(typeidx_next);

            // [ ... typeidx1 ...] typeidx2 ...
            // [      safe       ] unsafe (could be the section_end)
            //                     ^^ section_curr
        }

        // [before_section ... | func_count ... typeidx1 ... ...] (end)
        // [                       safe                         ] unsafe (could be the section_end)
        //                                                        ^^ section_curr

        return section_curr;
    }

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
    /// @brief      check_function_section
    /// @details    Since simd handles so much, you have to do a content check again in the usual way in debug mode
    ///             Crashes straight away when something goes wrong
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void check_function_section([[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<function_section_storage_t> sec_adl,
                                                 ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
                                                 ::std::byte const* const section_begin,
                                                 ::std::byte const* const section_end) noexcept
    {
        // Note that section_begin may be equal to section_end
        // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)

        // get function_section_storage_t from storages
        auto const& functionsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<function_section_storage_t>(module_storage.sections)};

        // check has typesec
        auto const& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<type_section_storage_t<Fs...>>(module_storage.sections)};

        auto section_curr{section_begin};

        // [before_section ... ] | func_count typeidx1 ...
        // [        safe       ] | unsafe (could be the section_end)
        //                         ^^ section_curr

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 func_count;  // No initialization necessary

        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

        // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)
        auto const [func_count_next, func_count_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                              ::fast_io::mnp::leb128_get(func_count))};

        if(func_count_err != ::fast_io::parse_code::ok) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }

        // [before_section ... | func_count ...] typeidx1 ...
        // [             safe                  ] unsafe (could be the section_end)
        //                       ^^ section_curr

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 func_counter{};  // use for check

        section_curr = reinterpret_cast<::std::byte const*>(func_count_next);  // never out of bounds

        // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)

        // [before_section ... | func_count ...] typeidx1 ...
        // [              safe                 ] unsafe (could be the section_end)
        //                                       ^^ section_curr

        // handle it
        auto const type_section_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(typesec.types.size())};

        ::std::size_t index{};

        while(section_curr != section_end) [[likely]]
        {
            // Ensuring the existence of valid information

            // [ ... typeidx1] ... typeidx2 ...
            // [     safe    ] unsafe (could be the section_end)
            //       ^^ section_curr

            // check function counter
            // Ensure content is available before counting (section_curr != section_end)

            auto const func_counter_end{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(func_counter + 1u)};

            // check overflow
            if(func_counter_end < func_counter) [[unlikely]]
            {
                // overflow
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
            }

            func_counter = func_counter_end;

            if(func_counter > func_count) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }

            // [ ... typeidx1] ... typeidx2 ...
            // [     safe    ] unsafe (could be the section_end)
            //       ^^ section_curr

            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 typeidx;  // No initialization necessary

            using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

            auto const [typeidx_next, typeidx_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                            ::fast_io::mnp::leb128_get(typeidx))};

            if(typeidx_err != ::fast_io::parse_code::ok) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }

            // [ ... typeidx1 ...] typeidx2 ...
            // [      safe       ] unsafe (could be the section_end)
            //       ^^ section_curr

            // check: type_index should less than type_section_count
            if(typeidx >= type_section_count) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }

            if(typeidx != functionsec.funcs.index_unchecked(index)) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }

            section_curr = reinterpret_cast<::std::byte const*>(typeidx_next);

            // [ ... typeidx1 ...] typeidx2 ...
            // [      safe       ] unsafe (could be the section_end)
            //                     ^^ section_curr

            ++index;
        }

        // [before_section ... | func_count ... typeidx1 ... ...] (end)
        // [                       safe                         ] unsafe (could be the section_end)
        //                                                        ^^ section_curr
    }
#endif

    /// @brief Define the handler function for type_section
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void handle_binfmt_ver1_extensible_section_define(
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<function_section_storage_t> sec_adl,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
        ::std::byte const* const section_begin,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para,
        [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::max_section_id_map_sec_id_t& wasm_order,
        ::std::byte const* const sec_id_module_ptr) UWVM_THROWS
    {
#ifdef UWVM_TIMER
        ::uwvm2::utils::debug::timer parsing_timer{u8"parse function section (id: 3)"};
#endif
        // Note that section_begin may be equal to section_end
        // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)

        // get function_section_storage_t from storages
        auto& functionsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<function_section_storage_t>(module_storage.sections)};

        // import section
        auto const& importsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<import_section_storage_t<Fs...>>(module_storage.sections)};
        // importdesc[0]: func
        constexpr ::std::size_t importdesc_count{importsec.importdesc_count};
        static_assert(importdesc_count > 0uz);  // Ensure that subsequent index visits do not cross boundaries
        auto const imported_func_size{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(importsec.importdesc.index_unchecked(0uz).size())};

        // check duplicate
        if(functionsec.sec_span.sec_begin) [[unlikely]]
        {
            err.err_curr = sec_id_module_ptr;
            err.err_selectable.u8 = functionsec.section_id;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::duplicate_section;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        // check has typesec
        auto const& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<type_section_storage_t<Fs...>>(module_storage.sections)};

        // The WASM standard does not require that the type segment must appear before the function segment is parsed. Assuming that there is no type segment,
        // if the function segment contains a non-zero number of func, an illegal type error will definitely be reported.
#if 0
        if(!typesec.sec_span.sec_begin) [[unlikely]]
        {
            err.err_curr = sec_id_module_ptr;
            err.err_selectable.u8arr[0] = typesec.section_id;
            err.err_selectable.u8arr[1] = functionsec.section_id;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::forward_dependency_missing;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
#endif

        using wasm_byte_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte const*;

        functionsec.sec_span.sec_begin = reinterpret_cast<wasm_byte_const_may_alias_ptr>(section_begin);
        functionsec.sec_span.sec_end = reinterpret_cast<wasm_byte_const_may_alias_ptr>(section_end);

        auto section_curr{section_begin};

        // [before_section ... ] | func_count typeidx1 ...
        // [        safe       ] | unsafe (could be the section_end)
        //                         ^^ section_curr

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 func_count;  // No initialization necessary

        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

        // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)
        auto const [func_count_next, func_count_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                              ::fast_io::mnp::leb128_get(func_count))};

        if(func_count_err != ::fast_io::parse_code::ok) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_func_count;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(func_count_err);
        }

        // [before_section ... | func_count ...] typeidx1 ...
        // [             safe                  ] unsafe (could be the section_end)
        //                       ^^ section_curr

        // The size_t of some platforms is smaller than u32, in these platforms you need to do a size check before conversion
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasm_u32_max{::std::numeric_limits<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>::max()};
        if constexpr(size_t_max < wasm_u32_max)
        {
            // The size_t of current platforms is smaller than u32, in these platforms you need to do a size check before conversion
            if(func_count > size_t_max) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u64 = static_cast<::std::uint_least64_t>(func_count);
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::size_exceeds_the_maximum_value_of_size_t;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }

        if constexpr((::std::same_as<wasm1, Fs> || ...))
        {
            // The code and function share the same limit.

            auto const& wasm1_feapara_r{::uwvm2::parser::wasm::concepts::get_curr_feature_parameter<wasm1>(fs_para)};
            auto const& parser_limit{wasm1_feapara_r.parser_limit};
            if(static_cast<::std::size_t>(func_count) > parser_limit.max_code_sec_codes) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.exceed_the_max_parser_limit.name = u8"codesec_codes";
                err.err_selectable.exceed_the_max_parser_limit.value = static_cast<::std::size_t>(func_count);
                err.err_selectable.exceed_the_max_parser_limit.maxval = parser_limit.max_code_sec_codes;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::exceed_the_max_parser_limit;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }

        if(func_count > wasm_u32_max - imported_func_size) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_selectable.imp_def_num_exceed_u32max.type = 0x00;  // func
            err.err_selectable.imp_def_num_exceed_u32max.defined = func_count;
            err.err_selectable.imp_def_num_exceed_u32max.imported = imported_func_size;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::imp_def_num_exceed_u32max;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 func_counter{};  // use for check

        section_curr = reinterpret_cast<::std::byte const*>(func_count_next);  // never out of bounds

        // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)

        // [before_section ... | func_count ...] typeidx1 ...
        // [              safe                 ] unsafe (could be the section_end)
        //                                       ^^ section_curr

        // handle it
        auto const type_section_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(typesec.types.size())};

        if(type_section_count < static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 7u)
        {
            // storage: 1 byte
            // vectypeidx: almost always 1 byte, may be redundant zeros (up to 5 byte)
            // simd, zero-copy optimization supported!
            section_curr = scan_function_section_impl_u8_1b(sec_adl, module_storage, section_curr, section_end, err, fs_para, func_counter, func_count);
        }
        else if(type_section_count < static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 8u)
        {
            // storage: 1 byte
            // vectypeidx: most 1 byte - 2 byte, may be redundant zeros (up to 5 byte)
            // simd supported!
            section_curr = scan_function_section_impl_u8_2b(sec_adl, module_storage, section_curr, section_end, err, fs_para, func_counter, func_count);
        }
        else if(type_section_count < static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 14u)
        {
            // storage: 2 byte
            // vectypeidx: most 1 byte - 2 byte, may be redundant zeros (up to 5 byte)
            // simd supported!
            section_curr = scan_function_section_impl_u16_2b(sec_adl, module_storage, section_curr, section_end, err, fs_para, func_counter, func_count);
        }
        else if(type_section_count < static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 16u)
        {
            // storage: 2 byte
            // vectypeidx uleb128: most: 1 byte - 3 byte, may be redundant zeros (up to 5 byte)
            // If your wasm file has this many typeidx, please post an issue requesting the adaptation of simd!
            section_curr = scan_function_section_impl_u16_3b(sec_adl, module_storage, section_curr, section_end, err, fs_para, func_counter, func_count);
        }
        else
        {
            // storage: 4 byte
            // vectypeidx uleb128: most: 1 byte - 5 byte
            // If your wasm file has this many typeidx, please post an issue requesting the adaptation of simd!
            section_curr = scan_function_section_impl_u32_5b(sec_adl, module_storage, section_curr, section_end, err, fs_para, func_counter, func_count);
        }

        // [before_section ... | func_count ... typeidx1 ... ...] (end)
        // [                       safe                         ] unsafe (could be the section_end)
        //                                                        ^^ section_curr

        // check function counter match
        if(func_counter != func_count) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_selectable.u32arr[0] = func_counter;
            err.err_selectable.u32arr[1] = func_count;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_not_match_the_actual_number;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        // Since simd handles so much, you have to do a content check again in the usual way in debug mode
        check_function_section(sec_adl, module_storage, section_begin, section_end);
#endif
    }

    /// @brief Wrapper for the section storage structure
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct function_section_storage_section_details_wrapper_t
    {
        function_section_storage_t const* function_section_storage_ptr{};
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const* all_sections_ptr{};
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr function_section_storage_section_details_wrapper_t<Fs...> section_details(
        function_section_storage_t const& function_section_storage,
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const& all_sections) noexcept
    {
        return {::std::addressof(function_section_storage), ::std::addressof(all_sections)};
    }

    /// @brief Print the function section details
    /// @throws maybe throw fast_io::error, see the implementation of the stream
    template <::std::integral char_type, typename Stm, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<char_type, function_section_storage_section_details_wrapper_t<Fs...>>,
                                       Stm && stream,
                                       function_section_storage_section_details_wrapper_t<Fs...> const function_section_details_wrapper)
    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(function_section_details_wrapper.function_section_storage_ptr == nullptr || function_section_details_wrapper.all_sections_ptr == nullptr)
            [[unlikely]]
        {
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
#endif

        auto const function_section_span{function_section_details_wrapper.function_section_storage_ptr->sec_span};
        auto const function_section_size{static_cast<::std::size_t>(function_section_span.sec_end - function_section_span.sec_begin)};

        if(function_section_size)
        {
            auto const function_size{function_section_details_wrapper.function_section_storage_ptr->funcs.size()};

            if constexpr(::std::same_as<char_type, char>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), "\nFunction[", function_size, "]:\n");
            }
            else if constexpr(::std::same_as<char_type, wchar_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L"\nFunction[", function_size, L"]:\n");
            }
            else if constexpr(::std::same_as<char_type, char8_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8"\nFunction[", function_size, u8"]:\n");
            }
            else if constexpr(::std::same_as<char_type, char16_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u"\nFunction[", function_size, u"]:\n");
            }
            else if constexpr(::std::same_as<char_type, char32_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U"\nFunction[", function_size, U"]:\n");
            }

            auto const& importsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<import_section_storage_t<Fs...>>(
                *function_section_details_wrapper.all_sections_ptr)};
            static_assert(importsec.importdesc_count > 0uz);
            auto func_counter{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(importsec.importdesc.index_unchecked(0uz).size())};

            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 localdef_counter{};

            auto const& function_section{*function_section_details_wrapper.function_section_storage_ptr};

            switch(function_section.funcs.mode)
            {
                case ::uwvm2::parser::wasm::standard::wasm1::features::vectypeidx_minimize_storage_mode::null:
                {
                    // do nothing, no element
                    break;
                }
                case ::uwvm2::parser::wasm::standard::wasm1::features::vectypeidx_minimize_storage_mode::u8_view:
                {
                    auto const u8_view_begin{function_section.funcs.storage.typeidx_u8_view.begin};
                    auto const u8_view_end{function_section.funcs.storage.typeidx_u8_view.end};

                    for(auto u8_view_curr{u8_view_begin}; u8_view_curr != u8_view_end; ++u8_view_curr)
                    {
                        auto const type_index{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(*u8_view_curr)};

                        if constexpr(::std::same_as<char_type, char>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                             " - localfunc[",
                                                                             localdef_counter,
                                                                             "] -> func[",
                                                                             func_counter,
                                                                             "]: {sig: type[",
                                                                             type_index,
                                                                             "]}\n");
                        }
                        else if constexpr(::std::same_as<char_type, wchar_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                             L" - localfunc[",
                                                                             localdef_counter,
                                                                             L"] -> func[",
                                                                             func_counter,
                                                                             L"]: {sig: type[",
                                                                             type_index,
                                                                             L"]}\n");
                        }
                        else if constexpr(::std::same_as<char_type, char8_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                             u8" - localfunc[",
                                                                             localdef_counter,
                                                                             u8"] -> func[",
                                                                             func_counter,
                                                                             u8"]: {sig: type[",
                                                                             type_index,
                                                                             u8"]}\n");
                        }
                        else if constexpr(::std::same_as<char_type, char16_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                             u" - localfunc[",
                                                                             localdef_counter,
                                                                             u"] -> func[",
                                                                             func_counter,
                                                                             u"]: {sig: type[",
                                                                             type_index,
                                                                             u"]}\n");
                        }
                        else if constexpr(::std::same_as<char_type, char32_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                             U" - localfunc[",
                                                                             localdef_counter,
                                                                             U"] -> func[",
                                                                             func_counter,
                                                                             U"]: {sig: type[",
                                                                             type_index,
                                                                             U"]}\n");
                        }

                        ++func_counter;
                        ++localdef_counter;
                    }

                    break;
                }
                case ::uwvm2::parser::wasm::standard::wasm1::features::vectypeidx_minimize_storage_mode::u8_vector:
                {
                    for(auto const u8_vec_curr: function_section.funcs.storage.typeidx_u8_vector)
                    {
                        auto const type_index{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(u8_vec_curr)};

                        if constexpr(::std::same_as<char_type, char>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                             " - localfunc[",
                                                                             localdef_counter,
                                                                             "] -> func[",
                                                                             func_counter,
                                                                             "]: {sig: type[",
                                                                             type_index,
                                                                             "]}\n");
                        }
                        else if constexpr(::std::same_as<char_type, wchar_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                             L" - localfunc[",
                                                                             localdef_counter,
                                                                             L"] -> func[",
                                                                             func_counter,
                                                                             L"]: {sig: type[",
                                                                             type_index,
                                                                             L"]}\n");
                        }
                        else if constexpr(::std::same_as<char_type, char8_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                             u8" - localfunc[",
                                                                             localdef_counter,
                                                                             u8"] -> func[",
                                                                             func_counter,
                                                                             u8"]: {sig: type[",
                                                                             type_index,
                                                                             u8"]}\n");
                        }
                        else if constexpr(::std::same_as<char_type, char16_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                             u" - localfunc[",
                                                                             localdef_counter,
                                                                             u"] -> func[",
                                                                             func_counter,
                                                                             u"]: {sig: type[",
                                                                             type_index,
                                                                             u"]}\n");
                        }
                        else if constexpr(::std::same_as<char_type, char32_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                             U" - localfunc[",
                                                                             localdef_counter,
                                                                             U"] -> func[",
                                                                             func_counter,
                                                                             U"]: {sig: type[",
                                                                             type_index,
                                                                             U"]}\n");
                        }

                        ++func_counter;
                        ++localdef_counter;
                    }

                    break;
                }
                case ::uwvm2::parser::wasm::standard::wasm1::features::vectypeidx_minimize_storage_mode::u16_vector:
                {
                    for(auto const u16_vec_curr: function_section.funcs.storage.typeidx_u16_vector)
                    {
                        auto const type_index{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(u16_vec_curr)};

                        if constexpr(::std::same_as<char_type, char>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                             " - localfunc[",
                                                                             localdef_counter,
                                                                             "] -> func[",
                                                                             func_counter,
                                                                             "]: {sig: type[",
                                                                             type_index,
                                                                             "]}\n");
                        }
                        else if constexpr(::std::same_as<char_type, wchar_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                             L" - localfunc[",
                                                                             localdef_counter,
                                                                             L"] -> func[",
                                                                             func_counter,
                                                                             L"]: {sig: type[",
                                                                             type_index,
                                                                             L"]}\n");
                        }
                        else if constexpr(::std::same_as<char_type, char8_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                             u8" - localfunc[",
                                                                             localdef_counter,
                                                                             u8"] -> func[",
                                                                             func_counter,
                                                                             u8"]: {sig: type[",
                                                                             type_index,
                                                                             u8"]}\n");
                        }
                        else if constexpr(::std::same_as<char_type, char16_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                             u" - localfunc[",
                                                                             localdef_counter,
                                                                             u"] -> func[",
                                                                             func_counter,
                                                                             u"]: {sig: type[",
                                                                             type_index,
                                                                             u"]}\n");
                        }
                        else if constexpr(::std::same_as<char_type, char32_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                             U" - localfunc[",
                                                                             localdef_counter,
                                                                             U"] -> func[",
                                                                             func_counter,
                                                                             U"]: {sig: type[",
                                                                             type_index,
                                                                             U"]}\n");
                        }

                        ++func_counter;
                        ++localdef_counter;
                    }

                    break;
                }
                case ::uwvm2::parser::wasm::standard::wasm1::features::vectypeidx_minimize_storage_mode::u32_vector:
                {
                    for(auto const u32_vec_curr: function_section.funcs.storage.typeidx_u32_vector)
                    {
                        auto const type_index{u32_vec_curr};

                        if constexpr(::std::same_as<char_type, char>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                             " - localfunc[",
                                                                             localdef_counter,
                                                                             "] -> func[",
                                                                             func_counter,
                                                                             "]: {sig: type[",
                                                                             type_index,
                                                                             "]}\n");
                        }
                        else if constexpr(::std::same_as<char_type, wchar_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                             L" - localfunc[",
                                                                             localdef_counter,
                                                                             L"] -> func[",
                                                                             func_counter,
                                                                             L"]: {sig: type[",
                                                                             type_index,
                                                                             L"]}\n");
                        }
                        else if constexpr(::std::same_as<char_type, char8_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                             u8" - localfunc[",
                                                                             localdef_counter,
                                                                             u8"] -> func[",
                                                                             func_counter,
                                                                             u8"]: {sig: type[",
                                                                             type_index,
                                                                             u8"]}\n");
                        }
                        else if constexpr(::std::same_as<char_type, char16_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                             u" - localfunc[",
                                                                             localdef_counter,
                                                                             u"] -> func[",
                                                                             func_counter,
                                                                             u"]: {sig: type[",
                                                                             type_index,
                                                                             u"]}\n");
                        }
                        else if constexpr(::std::same_as<char_type, char32_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                             U" - localfunc[",
                                                                             localdef_counter,
                                                                             U"] -> func[",
                                                                             func_counter,
                                                                             U"]: {sig: type[",
                                                                             type_index,
                                                                             U"]}\n");
                        }

                        ++func_counter;
                        ++localdef_counter;
                    }

                    break;
                }
                [[unlikely]] default:
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::std::unreachable();
                }
            }
        }
    }
}

/// @brief Define container optimization operations for use with fast_io
UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <>
    struct is_trivially_copyable_or_relocatable<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>
    {
        inline static constexpr bool value = true;
    };

    template <>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>
    {
        inline static constexpr bool value = true;
    };
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
