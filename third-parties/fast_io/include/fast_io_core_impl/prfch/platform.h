#pragma once

namespace fast_io
{

/// @brief Broad instruction-set families relevant to prefetch lowering and strategy selection.
/// @details This is deliberately not an ABI enumeration. Several ABIs may share one instruction set, while a single
///          ISA may cover cores with very different memory systems. Higher-level policies must therefore combine this
///          value with a source-lifetime proof and measured cost policy; ISA membership alone never authorizes a hint.
enum class prfch_isa
{
	unknown,
	x86,
	arm,
	aarch64,
	riscv,
	powerpc,
	mips,
	s390,
	loongarch,
	sparc,
	hexagon,
	wasm,
	avr,
	bpf,
	other
};

// clang-format off
/*
New-architecture data-prefetch qualification protocol
=====================================================

Keep compiler capability, microarchitectural cost, and production profitability as three independent proofs. A new
ISA, vendor family, or tuning family must be evaluated in the following order:

1. Target identity
   - Find a documented, stable preprocessor contract for the ISA and tune family. Inspect the macro set for every
     supported compiler and every relevant `-march`, `-mcpu`, or `-mtune` value.
   - Reject compatibility macros which do not uniquely describe the selected backend tune. If no stable contract
     exists, leave `native_prfch_tune` as `generic`; do not infer a processor from the host at run time in this header.

2. Primitive lowering
   - Compile focused, externally visible, noinline probes for read and write at every supported cache level and
     retention value. Instruction-prefetch probes are separate because their address-form constraints may differ.
   - Inspect final assembly or disassembly, not LLVM IR. Verify the exact opcode, cache/retention encoding, address
     form, and absence of an unintended read-for-write degradation. Repeat at the optimization levels and relocation
     models used by production builds.
   - Add compile-time assertions for `native_prfch_isa`, `native_prfch_tune`, and the advertised capability concepts.
     A code-generation checker should fail if the expected instruction count changes.

3. Static scheduling analysis
   - Feed only the extracted production kernel or focused opcode sequence to `llvm-mca`, using the same target CPU as
     the compiled probe. Record micro-op count, modeled latency, reciprocal throughput, and execution-resource pressure.
   - Treat `llvm-mca` as instruction-cost and contention evidence only. It does not model cache residency, TLB state,
     DRAM latency, memory bandwidth, hardware-prefetch behavior, OS migration, or whether the hint arrives in time.
     Identical scheduling models across product names are not evidence of identical memory systems.

4. Runtime experiment
   - Compare a semantics-identical baseline and candidate. Keep allocation, randomization, validation, checksums, and
     cache scrubbing outside the timed region. Preserve noinline kernel boundaries and compiler memory barriers so the
     real copies remain observable without adding artificial work to one side.
   - Test read and write independently. Cover payloads below, at, and above each proposed threshold; short and long
     chains; contiguous and deliberately discontinuous layouts; hot-resident and cache-pressure states; empty entries;
     and every relevant core class. The hinted address must remain inside a live, ordinary-cacheable object.
   - Make the benchmark's run-time hint threshold equal to the candidate production threshold and record it in every
     result row. Inspect the timed candidate assembly and confirm that every admitted boundary case actually executes
     the intended hint. Timing a compile-time-present but run-time-skipped instruction is control-flow evidence, not
     prefetch evidence.
   - Bind the process to one idle core when the OS supports affinity, leave SMT siblings idle, and record topology,
     compiler version, target flags, macros, and operating-system context. When affinity is unavailable, disclose that
     limitation and require additional independent processes. If the OS exposes the current CPU, sample it immediately
     before and after each timed operation and retain a pair only when both kernels used the same accepted core. Do not
     reject a useful sample merely because untimed cache scrubbing migrated before that observation window.
   - Use alternating paired baseline/candidate samples and at least three independent process-level seeds. Retain the
     paired ratios and their complete ranges, not only independently rounded medians. A cache-pressure improvement cannot
     admit a policy if the matching hot negative control has a repeatable regression.
   - Interpolate between proposed endpoints. If any intermediate cell fails the retained rule, encode only the tested
     discrete values or narrow the domain; never present endpoint measurements as proof of a continuous interval. Test
     mixed payload sizes separately before permitting them through a policy established with uniform-payload fixtures.

5. Promotion boundary
   - Successful lowering admits `data_available`. Stable family identification plus conservative cost evidence may
     admit `conservative_read_prfch_platform` or `conservative_write_prfch_platform` independently.
   - Only repeated measurements of the complete production memory operation may admit a print, concat, scan, or other
     site gate. Before activation, compile proved and unproved inputs through the public entry point and require the
     expected hint only in the proved final assembly; also run the public operation's boundary and output tests. Carry
     the measured minimum work, lookahead distance, cache level, retention, provenance, lifetime, and range proof into
     that site. Evidence from one direction, call site, core class, or product must not widen another by analogy.
   - Preserve the commands, raw-result location, aggregates, negative controls, and rejected regions under
     `benchmark/0022.prfch/`. For a future architecture, complete the applicable evidence stage before changing its
     classifier, capability flag, broad experimental permission, or site-specific allow-list predicate.
*/
// clang-format on

/// @brief Conservative compiler-tuning families used only to choose measured prefetch policies.
/// @details GCC exposes selected x86 `-mtune` choices as `__tune_*` macros, often canonicalizing multiple CPU names to
///          one macro. Clang and MSVC generally do not provide an equivalent preprocessor contract. `generic` therefore
///          means "no usable compile-time tune proof", not "a slow CPU". The categories intentionally remain broad: a
///          header-only library cannot safely choose a microarchitecture-maximal distance for every processor that can
///          execute the same binary.
enum class prfch_tune
{
	generic,
	x86_intel_core,
	x86_intel_atom,
	x86_intel_hybrid,
	x86_amd_zen,
	x86_amd_legacy,
	arm_application,
	arm_server,
	other_known,
	arm_apple
};

namespace details
{

inline constexpr prfch_isa native_prfch_isa =
#if defined(__aarch64__) || defined(__arm64__) || defined(__arm64) || defined(_M_ARM64) || \
	defined(__arm64ec__) || defined(_M_ARM64EC)
	prfch_isa::aarch64;
#elif (defined(__x86_64__) || defined(__i386__) || defined(__x86__) || defined(_M_X64) || \
	   defined(_M_AMD64) || defined(_M_IX86)) &&                                          \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	prfch_isa::x86;
#elif defined(__arm__) || defined(_M_ARM)
	prfch_isa::arm;
#elif defined(__riscv)
	prfch_isa::riscv;
#elif defined(__powerpc__) || defined(__powerpc64__) || defined(__ppc__) || defined(__PPC__) || defined(_M_PPC)
	prfch_isa::powerpc;
#elif defined(__mips__) || defined(__mips)
	prfch_isa::mips;
#elif defined(__s390__) || defined(__s390x__)
	prfch_isa::s390;
#elif defined(__loongarch__)
	prfch_isa::loongarch;
#elif defined(__sparc__) || defined(__sparc)
	prfch_isa::sparc;
#elif defined(__hexagon__)
	prfch_isa::hexagon;
#elif defined(__wasm__)
	prfch_isa::wasm;
#elif defined(__AVR__)
	prfch_isa::avr;
#elif defined(__bpf__)
	prfch_isa::bpf;
#else
	prfch_isa::unknown;
#endif

inline constexpr prfch_tune native_prfch_tune =
// GCC's target backend defines `__tune_*` according to the selected x86 tuning model. Clang intentionally exposes
// compatibility macros such as `__tune_k8__` even when `-mtune` names an unrelated Intel or AMD core; treating those
// macros as evidence misclassifies every Clang x86-64 translation unit. Unknown compiler families therefore remain
// generic unless they acquire a documented, tested contract of their own. Apple AArch64 is a platform-family
// exception rather than a per-core tune, because every supported M-generation target exposes the same stable macro
// pair. Genuine MSVC x86 is another narrow exception: `/favor:ATOM` has the documented `__ATOM__ == 1` preprocessor
// contract. Other `/favor` modes remain unclassified, and ARM64EC is explicitly excluded because x64 compatibility
// macros do not prove that its native ISA is x86.
#if defined(__APPLE__) && \
	(defined(__aarch64__) || defined(__arm64__) || defined(__arm64))
	// Apple Clang exposes no stable per-M-generation preprocessor contract: `-mcpu=apple-m1` through `apple-m5`
	// define the same Apple/AArch64 platform macros. This family therefore records the common Apple-silicon ABI
	// envelope, not a claim that M-series cache sizes, memory bandwidth, or prefetch profitability are identical.
	prfch_tune::arm_apple;
#elif defined(_MSC_VER) && !defined(__clang__) && defined(__ATOM__) && __ATOM__ == 1 && \
	(defined(_M_X64) || defined(_M_AMD64) || defined(_M_IX86)) &&                       \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	prfch_tune::x86_intel_atom;
#elif defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER) && \
	!defined(__INTEL_LLVM_COMPILER)
#if defined(__tune_alderlake__) || defined(__tune_arrowlake__) || defined(__tune_arrowlake_s__) || \
	defined(__tune_pantherlake__) || defined(__tune_novalake__)
	prfch_tune::x86_intel_hybrid;
#elif defined(__tune_bonnell__) || defined(__tune_silvermont__) || defined(__tune_goldmont__) ||      \
	defined(__tune_goldmont_plus__) || defined(__tune_tremont__) || defined(__tune_sierraforest__) || \
	defined(__tune_grandridge__) || defined(__tune_clearwaterforest__)
	prfch_tune::x86_intel_atom;
#elif defined(__tune_core2__) || defined(__tune_nehalem__) || defined(__tune_westmere__) ||                  \
	defined(__tune_sandybridge__) || defined(__tune_ivybridge__) || defined(__tune_haswell__) ||             \
	defined(__tune_broadwell__) || defined(__tune_skylake__) || defined(__tune_cannonlake__) ||              \
	defined(__tune_skylake_avx512__) || defined(__tune_cooperlake__) ||                                      \
	defined(__tune_icelake_client__) || defined(__tune_icelake_server__) || defined(__tune_cascadelake__) || \
	defined(__tune_tigerlake__) || defined(__tune_rocketlake__) || defined(__tune_sapphirerapids__) ||       \
	defined(__tune_emeraldrapids__) || defined(__tune_graniterapids__) ||                                    \
	defined(__tune_graniterapids_d__) || defined(__tune_diamondrapids__)
	prfch_tune::x86_intel_core;
#elif defined(__tune_znver1__) || defined(__tune_znver2__) || defined(__tune_znver3__) || \
	defined(__tune_znver4__) || defined(__tune_znver5__) || defined(__tune_znver6__)
	prfch_tune::x86_amd_zen;
#elif defined(__tune_k8__) || defined(__tune_amdfam10__) || defined(__tune_bdver1__) || \
	defined(__tune_bdver2__) || defined(__tune_bdver3__) || defined(__tune_bdver4__) || \
	defined(__tune_btver1__) || defined(__tune_btver2__)
	// GCC also canonicalizes a few non-AMD CPU names to its K8 scheduling model. This enum records the selected cost
	// family, not a run-time vendor assertion, and the conservative policy keeps the whole class disabled initially.
	prfch_tune::x86_amd_legacy;
#elif defined(__tune_neoverse_n1__) || defined(__tune_neoverse_n2__) || defined(__tune_neoverse_v1__) || \
	defined(__tune_neoverse_v2__)
	prfch_tune::arm_server;
#elif defined(__tune_cortex_a53__) || defined(__tune_cortex_a55__) || defined(__tune_cortex_a57__) || \
	defined(__tune_cortex_a72__) || defined(__tune_cortex_a76__) || defined(__tune_cortex_a77__) ||   \
	defined(__tune_cortex_a78__) || defined(__tune_cortex_a510__) || defined(__tune_cortex_a710__) || \
	defined(__tune_cortex_x1__)
	prfch_tune::arm_application;
#else
	prfch_tune::generic;
#endif
#else
	prfch_tune::generic;
#endif

inline constexpr bool native_data_prfch_available =
#if FAST_IO_HAS_BUILTIN(__builtin_prefetch) || FAST_IO_HAS_BUILTIN(__builtin_arm_prefetch)
	true;
#elif defined(_MSC_VER) && !defined(__clang__) &&                                                  \
	(defined(_M_ARM) || defined(_M_ARM64) || defined(_M_ARM64EC) ||                                \
	 ((defined(_M_X64) || defined(_M_AMD64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC))) || \
	 (defined(_M_IX86_FP) && _M_IX86_FP >= 1))
	true;
#else
	false;
#endif

inline constexpr bool native_instruction_prfch_available =
#if (defined(__aarch64__) || defined(__arm64__) || defined(__arm64) || defined(__arm64ec__) || \
	 defined(_M_ARM64) || defined(_M_ARM64EC)) &&                                              \
	FAST_IO_HAS_BUILTIN(__builtin_arm_prefetch)
	true;
#elif (defined(__aarch64__) || defined(__arm64__) || defined(__arm64)) && \
	FAST_IO_HAS_BUILTIN(__builtin_aarch64_plix)
	true;
#elif defined(__aarch64__) && defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 16
	true;
#elif defined(__arm__) && FAST_IO_HAS_BUILTIN(__builtin_arm_prefetch)
			true;
#elif defined(_MSC_VER) && !defined(__clang__) && (defined(_M_ARM64) || defined(_M_ARM64EC))
			true;
#elif defined(__x86_64__) && defined(__PREFETCHI__) && FAST_IO_HAS_BUILTIN(__builtin_ia32_prefetchi) && \
	defined(__clang__) && !defined(__INTEL_LLVM_COMPILER) &&                                            \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
			// Clang exposes the pointer-shaped intrinsic through llvm.prefetch.  Only a final RIP-relative operand is
			// architecturally effective; a non-RIP-relative form is ignored.  The dynamic API preserves Clang's documented
			// syntax, while `prfch_instruction_local` expresses the stronger caller-owned effectiveness precondition.
	true;
#else
			false;
#endif

/// @brief Whether the direct function-address convenience API has a compiler lowering.
/// @details AArch64 and ARM can use the same general-address PLI operation as the dynamic API. Clang x86 preserves its
///          pointer-shaped PREFETCHI builtin for a statically named target, but only a locally bound/hidden target is
///          guaranteed to become an effective RIP-relative operand. Genuine GCC is deliberately excluded from this
///          convenience: its target builtin diagnoses operands which cannot be materialized RIP-relatively, so GCC's
///          support belongs only to the explicit local-binding API below.
inline constexpr bool native_direct_instruction_prfch_available =
	native_instruction_prfch_available;

/// @brief Whether an explicitly asserted locally-bound function can use x86 PREFETCHI.
/// @details Clang and genuine GCC intentionally share only this explicit proof surface, not their compiler behavior.
///          GCC diagnoses a non-RIP-relative operand; Clang accepts a register-indirect spelling which the architecture
///          ignores. Standard C++ cannot inspect ELF/COFF symbol binding, so the token remains a caller assertion in
///          both branches. This flag reports compiler/ISA support, not that a particular function satisfies it.
inline constexpr bool native_local_instruction_prfch_available =
#if (defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64)) && defined(__PREFETCHI__) && \
	FAST_IO_HAS_BUILTIN(__builtin_ia32_prefetchi) &&                                           \
	((defined(__clang__) && !defined(__INTEL_LLVM_COMPILER)) ||                                \
	 (defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER) &&                \
	  !defined(__INTEL_LLVM_COMPILER))) &&                                                     \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	true;
#else
	false;
#endif

struct native_prfch_platform
{
	inline static constexpr prfch_isa isa{native_prfch_isa};
	inline static constexpr prfch_tune tune{native_prfch_tune};
	inline static constexpr bool data_available{native_data_prfch_available};
	inline static constexpr bool instruction_available{native_instruction_prfch_available};
	inline static constexpr bool direct_instruction_available{native_direct_instruction_prfch_available};
	inline static constexpr bool local_instruction_available{native_local_instruction_prfch_available};
};

} // namespace details

/// @brief Structural contract for a compile-time prefetch platform description.
template <typename platform_type>
concept prfch_platform = requires {
	{ platform_type::isa } -> ::std::convertible_to<prfch_isa>;
	{ platform_type::tune } -> ::std::convertible_to<prfch_tune>;
	{ platform_type::data_available } -> ::std::convertible_to<bool>;
	{ platform_type::instruction_available } -> ::std::convertible_to<bool>;
	typename ::std::integral_constant<prfch_isa, static_cast<prfch_isa>(platform_type::isa)>;
	typename ::std::integral_constant<prfch_tune, static_cast<prfch_tune>(platform_type::tune)>;
	typename ::std::bool_constant<static_cast<bool>(platform_type::data_available)>;
	typename ::std::bool_constant<static_cast<bool>(platform_type::instruction_available)>;
};

template <typename platform_type>
concept data_prfch_platform = prfch_platform<platform_type> && platform_type::data_available;

/// @brief Identifies a platform where the generic instruction-prefetch syntax has a compiler lowering.
/// @details This is not an address-binding proof. In particular, Clang x86 accepts a pointer operand although only a
///          final RIP-relative instruction is architecturally effective. Site code which requires that stronger fact
///          must use `local_instruction_prfch_platform` together with `prfch_local_function_t`.
template <typename platform_type>
concept instruction_prfch_platform = prfch_platform<platform_type> && platform_type::instruction_available;

/// @brief Identifies a platform where the direct function-address convenience has a compiler lowering.
/// @details Custom descriptions opt in with a constant `direct_instruction_available` member. Keeping this optional
///          field outside `prfch_platform` preserves source compatibility for existing four-field platform models.
///          x86's stricter RIP-relative local-binding contract belongs to `local_instruction_prfch_platform` instead.
template <typename platform_type>
concept direct_instruction_prfch_platform = instruction_prfch_platform<platform_type> && requires {
	{ platform_type::direct_instruction_available } -> ::std::convertible_to<bool>;
	typename ::std::bool_constant<static_cast<bool>(platform_type::direct_instruction_available)>;
} && platform_type::direct_instruction_available;

/// @brief Identifies a platform which can lower an explicit caller assertion of local function binding.
/// @details The optional member is kept outside `prfch_platform`, as with the direct capability, so existing custom
///          four-field descriptions remain valid. A true value does not prove the binding of a concrete symbol; the
///          `prfch_local_function_t` token carries that separate caller-owned precondition.
template <typename platform_type>
concept local_instruction_prfch_platform = prfch_platform<platform_type> && requires {
	{ platform_type::local_instruction_available } -> ::std::convertible_to<bool>;
	typename ::std::bool_constant<static_cast<bool>(platform_type::local_instruction_available)>;
} && platform_type::local_instruction_available;

template <typename platform_type>
concept x86_prfch_platform = prfch_platform<platform_type> && platform_type::isa == prfch_isa::x86;

template <typename platform_type>
concept arm_prfch_platform =
	prfch_platform<platform_type> &&
	(platform_type::isa == prfch_isa::arm || platform_type::isa == prfch_isa::aarch64);

template <typename platform_type>
concept tuned_prfch_platform = prfch_platform<platform_type> && platform_type::tune != prfch_tune::generic;

} // namespace fast_io
