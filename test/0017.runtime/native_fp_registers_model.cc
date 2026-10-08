// Pure owned display/model + libc-typed HOST ABI copy component.
// DATA construction here is NOT a genuine JIT frame/trap or runtime authority.
#include <uwvm2/uwvm/debugger/native_registers.h>
#include <uwvm2/uwvm/debugger/native_step.h>
#include <fast_io.h>
#include <limits>
#include <type_traits>
namespace regs = ::uwvm2::uwvm::debugger::native_registers;
static_assert(regs::fp_index_of(regs::architecture::x86_64, u8"$xmm0") == 0u);
static_assert(regs::fp_index_of(regs::architecture::x86_64, u8"xmm15") == 15u);
static_assert(regs::fp_index_of(regs::architecture::x86_64, u8"st(0)") == 16u);
static_assert(regs::fp_index_of(regs::architecture::x86_64, u8"$st7") == 23u);
static_assert(regs::fp_index_of(regs::architecture::x86_64, u8"mxcsr") == 28u);
static_assert(regs::fp_index_of(regs::architecture::x86_64, u8"ymm0") == regs::max_fp_registers);
static_assert(regs::fp_index_of(regs::architecture::x86_64, u8"fip") == regs::max_fp_registers);
static_assert(regs::fp_index_of(regs::architecture::x86_64, u8"fdp") == regs::max_fp_registers);
static_assert(regs::fp_index_of(regs::architecture::aarch64, u8"xmm0") == regs::max_fp_registers);
static_assert(regs::fp_name(regs::architecture::x86_64, regs::max_fp_registers) == nullptr);
static_assert(regs::fp_width(24u) == 2u && regs::fp_width(25u) == 2u);
static_assert(regs::fp_width(26u) == 1u && regs::fp_width(27u) == 2u);
static_assert(regs::fp_width((::std::numeric_limits<::std::size_t>::max)()) == 0u);
static void check(bool value) noexcept { if(!value) { ::fast_io::fast_terminate(); } }
int main()
{
    regs::snapshot owned{}; owned.machine = regs::architecture::x86_64;
    check(!owned.floating.available);
#if defined(__linux__) && defined(__x86_64__) && __SIZEOF_POINTER__ == 8
    // This host test owns the complete libc SDK object. No address originates
    // from guest data or a wire request. The actual integration test separately
    // demands the real kernel trap + private owner/domain/code publication.
    ::ucontext_t context{};
    using host_fp = ::std::remove_pointer_t<decltype(context.uc_mcontext.fpregs)>;
    if constexpr(sizeof(host_fp) == 512u)
    {
        host_fp source{};
        auto* bytes{reinterpret_cast<unsigned char*>(::std::addressof(source))};
        // [owned SDK host object ... 512 bytes] end
        // [safe                              ] bounded object-representation fill.
        //  ^^ DATA pattern exercises reserved exclusion and no native-memory authority.
        for(::std::size_t i{}; i != 512u; ++i) { bytes[i] = static_cast<unsigned char>(i); }
        // Independent Intel layout oracle: abridged FTW byte4 only,
        // poisoned reserved byte5, FOP byte6/7 with poisoned reserved upper5.
        bytes[4u] = 0xa5u; bytes[5u] = 0xdeu;
        bytes[6u] = 0x67u; bytes[7u] = 0xf9u;
        context.uc_mcontext.fpregs = ::std::addressof(source);
        ::uwvm2::uwvm::debugger::native_step::details::capture_kernel_fp_registers(owned.floating, context);
        check(owned.floating.available);
        for(::std::size_t i{}; i != 16u; ++i)
        {
            auto const& value{owned.floating.values[i]}; check(value.width == 16u);
            for(::std::size_t k{}; k != 16u; ++k)
            { check(value.bytes[k] == static_cast<::std::uint8_t>(160u + i * 16u + k)); }
        }
        for(::std::size_t i{}; i != 8u; ++i)
        {
            auto const& value{owned.floating.values[16u + i]}; check(value.width == 10u);
            for(::std::size_t k{}; k != 10u; ++k)
            { check(value.bytes[k] == static_cast<::std::uint8_t>(32u + i * 16u + k)); }
            for(::std::size_t k{10u}; k != 16u; ++k) { check(value.bytes[k] == 0u); }
        }
        // Expected fixed widths/offsets are independent of the producer's
        // fp_width; the old mirrored oracle incorrectly accepted reserved byte5.
        constexpr ::std::array<::std::size_t, 6u> control_widths{2u, 2u, 1u, 2u, 4u, 4u};
        constexpr ::std::array<::std::size_t, 6u> control_offsets{0u, 2u, 4u, 6u, 24u, 28u};
        for(::std::size_t i{}; i != control_widths.size(); ++i)
        {
            // [fixed six independent control oracles][owned register slots24..29]
            // [safe                                ][safe                       ]
            //  ^^ both extents are fixed before all scalar subscripts.
            auto const width{control_widths[i]}, offset{control_offsets[i]};
            auto const& value{owned.floating.values[24u + i]};
            check(value.width == width && regs::fp_width(24u + i) == width);
            for(::std::size_t k{}; k != width; ++k)
            {
                auto const expected{offset == 4u ? 0xa5u : offset == 6u ? (k == 0u ? 0x67u : 0x01u) : offset + k};
                check(value.bytes[k] == static_cast<::std::uint8_t>(expected));
            }
            for(::std::size_t k{width}; k != 16u; ++k) { check(value.bytes[k] == 0u); }
        }
        check(owned.floating.values[26u].bytes[1u] == 0u);
        check((owned.floating.values[27u].bytes[1u] & 0xf8u) == 0u);
        auto const retained{owned.floating};
        // Changing ONLY actual SDK reserved storage must not change any named
        // output value. This remains owned HOST DATA, never a kernel trap claim.
        bytes[5u] ^= 0xffu; bytes[7u] ^= 0xf8u;
        regs::fp_snapshot changed_reserved{};
        ::uwvm2::uwvm::debugger::native_step::details::capture_kernel_fp_registers(changed_reserved, context);
        check(changed_reserved == retained);
        context.uc_mcontext.fpregs = nullptr;
        ::uwvm2::uwvm::debugger::native_step::details::capture_kernel_fp_registers(owned.floating, context);
        check(!owned.floating.available && retained.available);
        check(retained.values[15u].bytes[15u] == 0x9fu);
    }
#endif
    ::fast_io::io::println("native_fp_registers_model: PASS HOST owned DATA, no native runtime acceptance");
}
