#pragma once
namespace uwvm2::runtime::compiler::uwvm_int::threaded
{
    namespace float_details
    {
        namespace strict=::uwvm2::runtime::compiler::shared::strict_float;
        template<unsigned OutBits,bool Signed,typename UInt>
        [[nodiscard]] inline status truncate(UInt raw,bool saturating,::std::uint64_t& result) noexcept
        {
            constexpr unsigned fraction{sizeof(UInt)==4?23u:52u},bias{sizeof(UInt)==4?127u:1023u};
            constexpr unsigned maximum_exponent{sizeof(UInt)==4?255u:2047u};
            constexpr UInt sign{UInt{1}<<(sizeof(UInt)*8u-1u)};
            auto const encoded{static_cast<unsigned>((raw&~sign)>>fraction)};
            bool const negative{(raw&sign)!=0};
            auto const payload{static_cast<::std::uint64_t>(raw&((UInt{1}<<fraction)-1u))};
            constexpr ::std::uint64_t unsigned_max{OutBits==64u?UINT64_MAX:UINT32_MAX};
            constexpr ::std::uint64_t signed_limit{::std::uint64_t{1}<<(OutBits-1u)};
            auto overflow{[&]() noexcept
            {
                if(!saturating) { return status::trap_integer_overflow; }
                if constexpr(Signed) { result=negative?signed_limit:signed_limit-1u; }
                else { result=negative?0u:unsigned_max; }
                return status::ok;
            }};
            if(encoded==maximum_exponent)
            {
                if(payload) { result=0;return saturating?status::ok:status::trap_invalid_conversion; }
                return overflow();
            }
            result=0;if(encoded<bias) { return status::ok; }
            auto const exponent{encoded-bias};if(exponent>=OutBits) { return overflow(); }
            auto const significand{(::std::uint64_t{1}<<fraction)|payload};
            auto const magnitude{exponent>=fraction?significand<<(exponent-fraction):significand>>(fraction-exponent)};
            if constexpr(Signed)
            { if(magnitude>(negative?signed_limit:signed_limit-1u)) { return overflow(); } }
            else { if(negative && magnitude) { return overflow(); } }
            result=negative ? ::std::uint64_t{}-magnitude : magnitude;
            if constexpr(OutBits==32u) { result=static_cast<::std::uint32_t>(result); }
            return status::ok;
        }
    }

    inline bool engine::floating(unsigned op) noexcept
    {
        namespace strict=float_details::strict;
        ::std::uint64_t a{},b{},out{};if(!pop(a)) { return false; }
        if((op>=0x5bu&&op<=0x66u)||(op>=0x8bu&&op<=0xa6u))
        {
            bool const wide{(op>=0x61u&&op<=0x66u) || op>=0x99u};
            bool const comparison{op<=0x66u};
            unsigned const index{op-(comparison?(wide?0x61u:0x5bu):(wide?0x99u:0x8bu))};
            if(comparison || index>=7u)
            { b=a;if(!pop(a)) { return false; } }
            auto evaluate{[&]<typename UInt,typename Float>(UInt x,UInt y) noexcept -> ::std::uint64_t
            {
                constexpr UInt sign{UInt{1}<<(sizeof(UInt)*8u-1u)};
                if(comparison)
                {
                    using c=strict::comparison;
                    switch(index)
                    { case 0:return strict::compare_ieee_bits<c::eq>(x,y);case 1:return strict::compare_ieee_bits<c::ne>(x,y);
                      case 2:return strict::compare_ieee_bits<c::lt>(x,y);case 3:return strict::compare_ieee_bits<c::gt>(x,y);
                      case 4:return strict::compare_ieee_bits<c::le>(x,y);default:return strict::compare_ieee_bits<c::ge>(x,y); }
                }
                using r=strict::integral_rounding;using n=strict::operation;
                switch(index)
                {
                    case 0:return x&~sign;case 1:return x^sign;
                    case 2:return strict::round_integral_bits<r::ceil>(x);case 3:return strict::round_integral_bits<r::floor>(x);
                    case 4:return strict::round_integral_bits<r::trunc>(x);case 5:return strict::round_integral_bits<r::nearest>(x);
                    case 6:return ::std::bit_cast<UInt>(strict::square_root(::std::bit_cast<Float>(x)));
                    case 7:return ::std::bit_cast<UInt>(strict::binary<n::add>(::std::bit_cast<Float>(x),::std::bit_cast<Float>(y)));
                    case 8:return ::std::bit_cast<UInt>(strict::binary<n::sub>(::std::bit_cast<Float>(x),::std::bit_cast<Float>(y)));
                    case 9:return ::std::bit_cast<UInt>(strict::binary<n::mul>(::std::bit_cast<Float>(x),::std::bit_cast<Float>(y)));
                    case 10:return ::std::bit_cast<UInt>(strict::binary<n::div>(::std::bit_cast<Float>(x),::std::bit_cast<Float>(y)));
                    case 11:return strict::minmax_bits<true>(x,y);case 12:return strict::minmax_bits<false>(x,y);
                    default:return (x&~sign)|(y&sign);
                }
            }};
            return push(wide?evaluate.template operator()<::std::uint64_t,double>(a,b):
                evaluate.template operator()<::std::uint32_t,float>(static_cast<::std::uint32_t>(a),static_cast<::std::uint32_t>(b)));
        }
        if((op>=0xa8u&&op<=0xabu)||(op>=0xaeu&&op<=0xb1u)||(op>=0x200u&&op<=0x207u))
        {
            bool const saturating{op>=0x200u};
            auto const index{saturating?op-0x200u:op<=0xabu?op-0xa8u:op-0xaeu+4u};
            bool const out64{index>=4u},from64{(index&2u)!=0},signed_result{(index&1u)==0};
            auto convert{[&]<typename U>() noexcept
            {
                auto const raw{static_cast<U>(a)};
                if(out64) { return signed_result?float_details::truncate<64u,true>(raw,saturating,out):float_details::truncate<64u,false>(raw,saturating,out); }
                return signed_result?float_details::truncate<32u,true>(raw,saturating,out):float_details::truncate<32u,false>(raw,saturating,out);
            }};
            auto const code{from64?convert.template operator()<::std::uint64_t>():convert.template operator()<::std::uint32_t>()};
            return code==status::ok?push(out):fail(code);
        }
        switch(op)
        {
            case 0xb2:return push(strict::integer_to_float_bits<float>(::std::bit_cast<::std::int32_t>(static_cast<::std::uint32_t>(a))));
            case 0xb3:return push(strict::integer_to_float_bits<float>(static_cast<::std::uint32_t>(a)));
            case 0xb4:return push(strict::integer_to_float_bits<float>(::std::bit_cast<::std::int64_t>(a)));
            case 0xb5:return push(strict::integer_to_float_bits<float>(a));
            case 0xb6:return push(::std::bit_cast<::std::uint32_t>(strict::convert<float>(::std::bit_cast<double>(a))));
            case 0xb7:return push(strict::integer_to_float_bits<double>(::std::bit_cast<::std::int32_t>(static_cast<::std::uint32_t>(a))));
            case 0xb8:return push(strict::integer_to_float_bits<double>(static_cast<::std::uint32_t>(a)));
            case 0xb9:return push(strict::integer_to_float_bits<double>(::std::bit_cast<::std::int64_t>(a)));
            case 0xba:return push(strict::integer_to_float_bits<double>(a));
            case 0xbb:return push(::std::bit_cast<::std::uint64_t>(strict::convert<double>(::std::bit_cast<float>(static_cast<::std::uint32_t>(a)))));
            // Reinterpret is raw bit transport; it MUST NOT quiet a signaling NaN.
            case 0xbc:case 0xbe:return push(static_cast<::std::uint32_t>(a));
            case 0xbd:case 0xbf:return push(a);
            default:return fail(status::invariant_failure);
        }
    }
}
