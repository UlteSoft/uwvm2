// Finite production metadata component only. These synthetic FormValues test scalar
// semantics; they NEVER qualify real producer DWARF or a product source value.
#include <uwvm2/uwvm/debugger/source_dwarf_constants.h>
#include <fast_io.h>
namespace candidate = ::uwvm2::uwvm::debugger::source_dwarf_constants;
namespace dwarf = ::uwvm2::uwvm::debugger::source_dwarf;
static void check(bool value, ::std::string_view message)
{ if(!value) { ::fast_io::io::perrln("language_scalar_constant_candidate: ", message); ::fast_io::fast_terminate(); } }
int main()
{
    dwarf::type_record type{};
    type.kind = dwarf::type_kind::scalar; type.size_known = true;
    type.byte_count = 4u; type.byte_size = 4u; type.encoding = ::llvm::dwarf::DW_ATE_signed;
    dwarf::location_plan plan{};
    auto const signed_value{::llvm::DWARFFormValue::createFromSValue(::llvm::dwarf::DW_FORM_sdata, -3)};
    check(candidate::decode(signed_value, type, 4u, plan) == candidate::error::none &&
          plan.kind == dwarf::plan_kind::constant_value && ::std::bit_cast<::std::int64_t>(plan.constant_bits) == -3,
          "signed LEB constant is an immutable value, never an address");
    auto const exact{::llvm::DWARFFormValue::createFromUValue(::llvm::dwarf::DW_FORM_data4, 0xfffffffdu)};
    check(candidate::decode(exact, type, 4u, plan) == candidate::error::none &&
          ::std::bit_cast<::std::int64_t>(plan.constant_bits) == -3, "exact type-width signed target bits");
    auto const ambiguous{::llvm::DWARFFormValue::createFromUValue(::llvm::dwarf::DW_FORM_data1, 0xffu)};
    check(candidate::decode(ambiguous, type, 4u, plan) == candidate::error::ambiguous &&
          plan.kind == dwarf::plan_kind::unavailable, "short fixed signed representation must not guess extension");
    auto const positive{::llvm::DWARFFormValue::createFromUValue(::llvm::dwarf::DW_FORM_udata, 23u)};
    check(candidate::decode(positive, type, 4u, plan) == candidate::error::none && plan.constant_bits == 23u,
          "actual producer scalar constant form can preserve a lexical shadow value");
    auto const too_large{::llvm::DWARFFormValue::createFromUValue(::llvm::dwarf::DW_FORM_udata, 0x80000000u)};
    check(candidate::decode(too_large, type, 4u, plan) == candidate::error::value_too_large &&
          plan.kind == dwarf::plan_kind::unavailable, "positive unsigned LEB cannot wrap signed destination");
    type.encoding = ::llvm::dwarf::DW_ATE_unsigned;
    check(candidate::decode(signed_value, type, 4u, plan) == candidate::error::value_too_large, "negative signed value cannot wrap unsigned destination");
    type.encoding = ::llvm::dwarf::DW_ATE_boolean; type.byte_count = 1u; type.byte_size = 1u;
    check(candidate::decode(positive, type, 4u, plan) == candidate::error::value_too_large, "boolean constant requires canonical zero/one");
    type.encoding = ::llvm::dwarf::DW_ATE_UTF;
    for(auto width : {1u, 2u, 4u})
    {
        type.byte_count = static_cast<::std::uint8_t>(width); type.byte_size = width;
        auto const bits{width == 1u ? 0x80ULL : width == 2u ? 0x3bbULL : 0x1f642ULL};
        auto const utf{::llvm::DWARFFormValue::createFromUValue(::llvm::dwarf::DW_FORM_udata, bits)};
        check(candidate::decode(utf, type, 4u, plan) == candidate::error::none &&
              plan.constant_bits == bits && !plan.signed_constant && !plan.implicit_constant &&
              plan.direct_constant_attribute && plan.byte_count == width,
              "UTF direct constant preserves bounded unsigned target code unit");
        auto const extended{::llvm::DWARFFormValue::createFromUValue(::llvm::dwarf::DW_FORM_udata,
            width == 1u ? 0xffffffffffffff80ULL : width == 2u ? 0xffffffffffffd83dULL : 0xffffffff80000042ULL)};
        check(candidate::decode(extended, type, 4u, plan) == candidate::error::none &&
              plan.constant_bits == (width == 1u ? 0x80ULL : width == 2u ? 0xd83dULL : 0x80000042ULL),
              "Clang UTF unsigned LEB preserves only the exact declared-width sign extension");
        auto const wrong_extension{::llvm::DWARFFormValue::createFromUValue(::llvm::dwarf::DW_FORM_udata,
            width == 1u ? 0xffffffffffffff00ULL : width == 2u ? 0xfffffffffffe8000ULL : 0xfffffffe80000042ULL)};
        check(candidate::decode(wrong_extension, type, 4u, plan) == candidate::error::value_too_large &&
              plan.kind == dwarf::plan_kind::unavailable, "UTF extension must exactly match its source high bit");
        check(candidate::decode(signed_value, type, 4u, plan) == candidate::error::value_too_large &&
              plan.kind == dwarf::plan_kind::unavailable, "negative UTF number must not wrap");
        auto const overflow_utf{::llvm::DWARFFormValue::createFromUValue(::llvm::dwarf::DW_FORM_udata, 1ULL << (width * 8u))};
        check(candidate::decode(overflow_utf, type, 8u, plan) == candidate::error::value_too_large &&
              plan.kind == dwarf::plan_kind::unavailable, "UTF integer must fit actual target width");
    }
    type.byte_count = 2u; type.byte_size = 2u;
    auto const surrogate{::llvm::DWARFFormValue::createFromUValue(::llvm::dwarf::DW_FORM_data2, 0xd83du)};
    check(candidate::decode(surrogate, type, 8u, plan) == candidate::error::none && plan.constant_bits == 0xd83du,
          "single UTF16 surrogate is a code unit, not a guessed character");
    type.byte_count = 8u; type.byte_size = 8u;
    check(candidate::decode(positive, type, 4u, plan) == candidate::error::unavailable &&
          plan.kind == dwarf::plan_kind::unavailable, "unsupported eight-byte UTF scalar remains unavailable");
    type.byte_count = 4u; type.byte_size = 4u; type.kind = dwarf::type_kind::pointer;
    check(candidate::decode(positive, type, 4u, plan) == candidate::error::unavailable &&
          plan.kind == dwarf::plan_kind::unavailable, "constant bytes grant no pointer authority");
    type.kind = dwarf::type_kind::scalar;
    type.encoding = ::llvm::dwarf::DW_ATE_float; type.byte_count = 4u; type.byte_size = 4u;
    check(candidate::decode(exact, type, 4u, plan) == candidate::error::none && plan.implicit_constant &&
          plan.direct_constant_attribute && plan.implicit_bytes[0] == ::std::byte{0xfdu} &&
          plan.implicit_bytes[3] == ::std::byte{0xffu}, "exact float form retains target bits, not numeric conversion");
    auto floating{[&](::llvm::dwarf::Form form, ::std::span<::std::uint8_t const> bytes)
    { return candidate::decode(::llvm::DWARFFormValue::createFromBlockValue(form, {bytes.data(), bytes.size()}), type, 4u, plan); }};
    ::std::array<::std::uint8_t, 8u> payload{0x34u, 0x12u, 0xc0u, 0x7fu, 0u, 0u, 0u, 0u};
    for(auto form : {::llvm::dwarf::DW_FORM_block, ::llvm::dwarf::DW_FORM_block1,
                     ::llvm::dwarf::DW_FORM_block2, ::llvm::dwarf::DW_FORM_block4})
    {
        check(floating(form, ::std::span{payload}.first(4u)) == candidate::error::none &&
              plan.implicit_bytes[0] == ::std::byte{0x34u} && plan.implicit_bytes[3] == ::std::byte{0x7fu},
              "bounded binary32 NaN block retains exact payload on any host");
    }
    check(floating(::llvm::dwarf::DW_FORM_exprloc, ::std::span{payload}.first(4u)) == candidate::error::unavailable &&
          plan.kind == dwarf::plan_kind::unavailable, "expression bytes cannot substitute a float constant");
    for(::std::size_t n : {0u, 3u, 5u, 8u})
    { check(floating(::llvm::dwarf::DW_FORM_block1, ::std::span{payload}.first(n)) == candidate::error::unavailable,
            "float block width must match the actual type"); }
    check(candidate::decode(positive, type, 4u, plan) == candidate::error::none &&
          plan.implicit_bytes[0] == ::std::byte{23u} && plan.implicit_bytes[3] == ::std::byte{0u},
          "LLVM unsigned LEB represents floating bits, no numeric conversion");
    check(candidate::decode(signed_value, type, 4u, plan) == candidate::error::unavailable &&
          plan.kind == dwarf::plan_kind::unavailable, "signed LEB floating representation remains unsupported");
    auto const float_overflow{::llvm::DWARFFormValue::createFromUValue(::llvm::dwarf::DW_FORM_udata, 1ULL << 32u)};
    check(candidate::decode(float_overflow, type, 4u, plan) == candidate::error::value_too_large &&
          plan.kind == dwarf::plan_kind::unavailable, "float unsigned LEB must fit actual binary32 width");
    auto const overflow{::llvm::DWARFFormValue::createFromUValue(::llvm::dwarf::DW_FORM_data4, 1ULL << 32u)};
    check(candidate::decode(overflow, type, 4u, plan) == candidate::error::value_too_large,
          "malformed synthetic fixed bits cannot exceed their form");
    type.byte_count = 8u; type.byte_size = 8u;
    payload = {0x78u, 0x56u, 0x34u, 0x12u, 0u, 0u, 0xf8u, 0x7fu};
    check(floating(::llvm::dwarf::DW_FORM_block1, payload) == candidate::error::none &&
          plan.implicit_bytes[0] == ::std::byte{0x78u} && plan.implicit_bytes[7] == ::std::byte{0x7fu},
          "binary64 NaN payload remains byte-exact");
    auto const negative_zero{::llvm::DWARFFormValue::createFromUValue(::llvm::dwarf::DW_FORM_data8, 0x8000000000000000ULL)};
    check(candidate::decode(negative_zero, type, 8u, plan) == candidate::error::none &&
          plan.implicit_bytes[7] == ::std::byte{0x80u} && plan.implicit_bytes[0] == ::std::byte{0u},
          "fixed binary64 preserves negative zero at either Wasm address width");
    check(candidate::decode(exact, type, 4u, plan) == candidate::error::unavailable,
          "short fixed float form must not pad a wider type");
    type.byte_count = 2u; type.byte_size = 2u;
    check(floating(::llvm::dwarf::DW_FORM_block1, ::std::span{payload}.first(2u)) == candidate::error::unavailable,
          "unsupported float formats remain unavailable");
    type.byte_count = 4u; type.byte_size = 4u;
    type.encoding = ::llvm::dwarf::DW_ATE_signed;
    check(candidate::decode(positive, type, 2u, plan) == candidate::error::unavailable, "non-Wasm address width cannot enter a location plan");
    ::fast_io::io::println("PASS finite scalar-constant production metadata component; real producer/product qualification separate");
}
