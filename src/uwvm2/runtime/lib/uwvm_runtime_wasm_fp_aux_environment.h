#pragma once

#if (defined(__GNUC__) || defined(__clang__)) && defined(__linux__) && \
    (defined(__powerpc__) || defined(__powerpc64__) || defined(__ppc__) || defined(__ppc64__)) && !defined(__ALTIVEC__)
# include <sys/auxv.h>
#endif

// MIPS/m68k and explicitly enabled AltiVec paths have no libc dependency; generic Linux PPC additionally uses HWCAP.
namespace uwvm2::runtime::lib::details
{
#if (defined(__GNUC__) || defined(__clang__)) && defined(__linux__) && \
    (defined(__powerpc__) || defined(__powerpc64__) || defined(__ppc__) || defined(__ppc64__)) && !defined(__ALTIVEC__)
    // A generic executable can still host AltiVec code emitted by the native JIT. Do not confuse compiler flags
    // with the execution CPU. Keep optional instructions in isolated helpers and gate them on OS HWCAP.
    struct alignas(16) wasm_fp_vscr_storage { unsigned lanes[4]; };
    [[nodiscard]] inline bool wasm_fp_host_has_altivec() noexcept
    {
        static bool const available{(::getauxval(AT_HWCAP) & 0x10000000ul) != 0ul};
        return available;
    }
    inline void wasm_fp_restore_vscr(wasm_fp_vscr_storage const& saved) noexcept
    {
        wasm_fp_vscr_storage scratch;
        // Restore v0 within the asm: GCC's generic PPC32 ABI cannot use a target("altivec") function.
        // No vector value crosses the C++ ABI, and the compiler need not allocate optional vector registers.
        __asm__ volatile(".machine push\n\t.machine altivec\n\tstvx 0,0,%0\n\tlvx 0,0,%1\n\tmtvscr 0\n\tlvx 0,0,%0\n\t.machine pop"
                         : : "r"(&scratch), "r"(&saved) : "memory");
    }
    inline void wasm_fp_save_vscr(wasm_fp_vscr_storage& saved, bool install_default) noexcept
    {
        wasm_fp_vscr_storage scratch;
        __asm__ volatile(".machine push\n\t.machine altivec\n\tstvx 0,0,%0\n\tmfvscr 0\n\tstvx 0,0,%1\n\tlvx 0,0,%0\n\t.machine pop"
                         : : "r"(&scratch), "r"(&saved) : "memory");
        if(install_default)
        {
            auto ieee{saved};
            for(auto& lane: ieee.lanes) { lane &= ~0x10000u; }
            wasm_fp_restore_vscr(ieee);
        }
    }
#endif
    // Do not replace the scalar libc guard with just these auxiliary registers:
    // notably Linux PPC also coordinates FPSCR exception state with the OS.
    // Conversely FE_DFL_ENV alone does not prove VSCR/MSACSR are safe. Generic
    // PPC uses runtime HWCAP for optional AltiVec; a MIPS build without an MSA
    // guard must not enable uncovered MSA arithmetic in the generated target.
    // ISO fenv does not cover every SIMD control register. VSCR.NJ (PowerPC) and MSACSR.FS/RM/Enables (MIPS MSA)
    // can change vector arithmetic independently of the scalar environment. Preserve them at the same boundaries.
    class scoped_wasm_auxiliary_fp_environment
    {
#if (defined(__GNUC__) || defined(__clang__)) && defined(__ALTIVEC__)
        using vector_type = unsigned int __attribute__((vector_size(16)));
        vector_type saved_vscr;
        bool enabled;
#elif (defined(__GNUC__) || defined(__clang__)) && defined(__linux__) && \
    (defined(__powerpc__) || defined(__powerpc64__) || defined(__ppc__) || defined(__ppc64__))
        wasm_fp_vscr_storage saved_vscr;
        bool enabled;
#elif (defined(__GNUC__) || defined(__clang__)) && defined(__mips_msa)
        unsigned saved_msacsr;
        bool enabled;
#elif (defined(__GNUC__) || defined(__clang__)) && defined(__m68k__) && defined(__HAVE_68881__)
        unsigned saved_fpcr;
        bool enabled;
#endif
    public:
        explicit scoped_wasm_auxiliary_fp_environment(bool active, bool install_default = false) noexcept
#if (defined(__GNUC__) || defined(__clang__)) && defined(__linux__) && !defined(__ALTIVEC__) && \
    (defined(__powerpc__) || defined(__powerpc64__) || defined(__ppc__) || defined(__ppc64__))
            : enabled{active && wasm_fp_host_has_altivec()}
#elif (defined(__GNUC__) || defined(__clang__)) && (defined(__ALTIVEC__) || defined(__mips_msa) || (defined(__m68k__) && defined(__HAVE_68881__)))
            : enabled{active}
#endif
        {
#if (defined(__GNUC__) || defined(__clang__)) && defined(__ALTIVEC__)
            if(!enabled) { return; }
            __asm__ volatile("mfvscr %0" : "=v"(saved_vscr) : : "memory");
            if(install_default)
            {
                // Mask every lane so both endian ABIs work without assuming which C++ element carries VSCR.
                vector_type const ieee{saved_vscr & vector_type{~0x10000u, ~0x10000u, ~0x10000u, ~0x10000u}};
                __asm__ volatile("mtvscr %0" : : "v"(ieee) : "memory");
            }
#elif (defined(__GNUC__) || defined(__clang__)) && defined(__linux__) && \
    (defined(__powerpc__) || defined(__powerpc64__) || defined(__ppc__) || defined(__ppc64__))
            if(enabled) { wasm_fp_save_vscr(saved_vscr, install_default); }
#elif (defined(__GNUC__) || defined(__clang__)) && defined(__mips_msa)
            if(!enabled) { return; }
            __asm__ volatile("cfcmsa %0, $1" : "=r"(saved_msacsr) : : "memory");
            if(install_default)
            {
                // MSA reset/IEEE state: RN ties-to-even, gradual underflow, exception enables and causes clear.
                __asm__ volatile("ctcmsa $1, %0" : : "r"(0u) : "memory");
            }
#elif (defined(__GNUC__) || defined(__clang__)) && defined(__m68k__) && defined(__HAVE_68881__)
            if(!enabled) { return; }
            // libc fenv may retain precision controls. Scalar transfers also avoid depending on an emulator's
            // multiple-control-register transfer implementation when restoring rounding and exception enables.
            __asm__ volatile("fmove.l %%fpcr,%0" : "=dm"(saved_fpcr) : : "memory");
            if(install_default) { __asm__ volatile("fmove.l %0,%%fpcr" : : "dm"(0u) : "memory"); }
#else
            static_cast<void>(active);
            static_cast<void>(install_default);
#endif
        }
        scoped_wasm_auxiliary_fp_environment(scoped_wasm_auxiliary_fp_environment const&) = delete;
        scoped_wasm_auxiliary_fp_environment& operator=(scoped_wasm_auxiliary_fp_environment const&) = delete;
        ~scoped_wasm_auxiliary_fp_environment() noexcept
        {
#if (defined(__GNUC__) || defined(__clang__)) && defined(__ALTIVEC__)
            if(enabled) { __asm__ volatile("mtvscr %0" : : "v"(saved_vscr) : "memory"); }
#elif (defined(__GNUC__) || defined(__clang__)) && defined(__linux__) && \
    (defined(__powerpc__) || defined(__powerpc64__) || defined(__ppc__) || defined(__ppc64__))
            if(enabled) { wasm_fp_restore_vscr(saved_vscr); }
#elif (defined(__GNUC__) || defined(__clang__)) && defined(__mips_msa)
            if(enabled) { __asm__ volatile("ctcmsa $1, %0" : : "r"(saved_msacsr) : "memory"); }
#elif (defined(__GNUC__) || defined(__clang__)) && defined(__m68k__) && defined(__HAVE_68881__)
            if(enabled) { __asm__ volatile("fmove.l %0,%%fpcr" : : "dm"(saved_fpcr) : "memory"); }
#endif
        }
    };
}
