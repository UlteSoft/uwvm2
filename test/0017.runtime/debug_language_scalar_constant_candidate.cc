// Finite candidate component only. These synthetic FormValues test scalar
// semantics; they NEVER qualify real producer DWARF or a product source value.
#include "candidates/language_scalar_constant.h.proposed"
#include <fast_io.h>
namespace candidate = ::uwvm2::uwvm::debugger::language_constant_candidate;
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
    type.encoding = ::llvm::dwarf::DW_ATE_float; type.byte_count = 4u; type.byte_size = 4u;
    check(candidate::decode(exact, type, 4u, plan) == candidate::error::unavailable, "float encoding cannot accidentally print integer bits");
    type.encoding = ::llvm::dwarf::DW_ATE_signed;
    check(candidate::decode(positive, type, 2u, plan) == candidate::error::unavailable, "non-Wasm address width cannot enter a location plan");
    ::fast_io::io::println("PASS finite scalar-constant candidate component; real producer/product qualification separate");
}
