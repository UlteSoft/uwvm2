// Isolated diagnostic-capture helper regression. The runner extracts the exact
// candidate helpers into the include below. No generated JIT, guest exception,
// section-manager FDE registration, or Win64 qualification is claimed here.
#include <fast_io.h>
#include <uwvm2/runtime/compiler/llvm_jit/native_unwind_platform.h>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <latch>
#include <thread>
#include <vector>
#include <uwvm2/runtime/lib/uwvm_runtime_execution_entry.h>

#if !defined(_WIN32) && (defined(__GNUC__) || defined(__clang__)) && __has_include(<unwind.h>) && \
    UWVM2_RUNTIME_LLVM_JIT_NATIVE_UNWIND_PLATFORM_SUPPORTED && defined(__EXCEPTIONS) && \
    __has_builtin(__builtin_dwarf_cfa)
# define UWVM_TEST_EH_BOUNDARY_AVAILABLE 1
# include "native_eh_candidate_helpers.h"
#else
# define UWVM_TEST_EH_BOUNDARY_AVAILABLE 0
#endif
#pragma pop_macro("UWVM2_RUNTIME_LLVM_JIT_NATIVE_UNWIND_PLATFORM_SUPPORTED")
#pragma pop_macro("UWVM2_RUNTIME_LLVM_JIT_WIN64_SEH_PLATFORM_SUPPORTED")

#if UWVM_TEST_EH_BOUNDARY_AVAILABLE
namespace h = uwvm_eh_candidate_helpers;
namespace detail = uwvm2::runtime::lib::details;
namespace
{
    struct output
    {
        h::call_stack_tls_state tls{};
        h::llvm_jit_unwind_backtrace_storage bounded{}, full{}, legacy{};
        std::array<std::uintptr_t, 4> boundary_cfas{};
        std::array<std::uintptr_t, 3> island_cfas{};
        std::uintptr_t selected{}, legacy_selected{}, wrong_peer_cfa{};
        std::size_t execution_depth{};
        volatile unsigned after_call{};
        unsigned cleanup{}, caught{}, outer{};
        std::vector<std::size_t> merged_bounded{}, merged_full{}, merged_legacy{};
        bool bounded_merge_finished{}, full_merge_finished{}, legacy_merge_finished{};
    };
    unsigned checks{};
# define CHECK(condition) do { ++checks; if(!(condition)) { ::fast_io::io::perrln("FAIL native EH boundary ", __LINE__, ": ", #condition); return 1; } } while(false)

    struct entry_guard
    {
        std::size_t& depth;
        explicit entry_guard(output& out, detail::runtime_execution_entry_reentry policy = detail::runtime_execution_entry_reentry::reject) noexcept
            : depth{out.execution_depth}
        {
            if(!detail::runtime_execution_entry_enter(depth, policy)) { ::fast_io::fast_terminate(); }
        }
        ~entry_guard() noexcept
        {
            if(detail::runtime_execution_entry_leave(depth) == detail::runtime_execution_entry_leave_result::invalid)
            { ::fast_io::fast_terminate(); }
        }
    };
    struct boundary_guard
    {
        h::llvm_jit_native_call_boundary record;
        detail::runtime_atomic_borrowed_record_scope<h::llvm_jit_native_call_boundary> published;
        boundary_guard(output& out, std::uintptr_t actual_cfa, bool root, std::size_t logical_depth) noexcept
            : record{std::atomic_ref<h::llvm_jit_native_call_boundary const*>{out.tls.llvm_jit_native_boundary}.load(std::memory_order_acquire),
                     actual_cfa, logical_depth, root},
              published{out.tls.llvm_jit_native_boundary, record.previous, &record}
        {}
        // Declaration order restores the atomic borrowed head before record dies.
    };
    struct logical_guard
    {
        h::call_stack_tls_state& tls;
        logical_guard(output& out, std::size_t identity) noexcept : tls{out.tls}
        { tls.frames.emplace_back(9uz, identity); }
        ~logical_guard() noexcept { tls.frames.pop_back_unchecked(); }
    };
    void after(output& out) noexcept { out.after_call = out.after_call + 1u; }
    std::vector<std::size_t> merged(output const& out, h::llvm_jit_unwind_backtrace_storage const& trace, bool& finished);

    [[gnu::noinline]] void capture(output& out) noexcept
    {
        out.selected = h::llvm_jit_exception_outer_native_boundary_cfa(out.tls);
        out.legacy_selected = h::legacy_llvm_jit_exception_outer_native_boundary_cfa(out.tls);
        out.bounded = h::capture_llvm_jit_exception_unwind_backtrace(out.tls);
        out.full = h::capture_llvm_jit_unwind_backtrace(0uz);
        out.legacy = out.legacy_selected == 0u ? h::capture_llvm_jit_unwind_backtrace(0uz) :
                     h::capture_llvm_jit_unwind_backtrace_impl<true>(0uz, out.legacy_selected);
        // All native records and logical spans are still live. Keep only
        // copied identities/flags after this call; no borrowed record escapes.
        out.merged_bounded = merged(out, out.bounded, out.bounded_merge_finished);
        out.merged_full = merged(out, out.full, out.full_merge_finished);
        out.merged_legacy = merged(out, out.legacy, out.legacy_merge_finished);
        after(out);
    }
    [[gnu::noinline]] void boundary_three(output& out) noexcept
    {
        entry_guard entry{out, detail::runtime_execution_entry_reentry::allow_public_llvm_raw};
        out.boundary_cfas[3] = reinterpret_cast<std::uintptr_t>(__builtin_dwarf_cfa());
        boundary_guard boundary{out, out.boundary_cfas[3], out.execution_depth == 1uz, out.tls.frames.size()};
        logical_guard frame{out, 103uz};
        capture(out);
        after(out);
    }
    [[gnu::noinline]] void island_three(output& out) noexcept
    {
        out.island_cfas[2] = reinterpret_cast<std::uintptr_t>(__builtin_dwarf_cfa());
        logical_guard frame{out, 102uz};
        boundary_three(out);
        after(out);
    }
    [[gnu::noinline]] void boundary_two(output& out) noexcept
    {
        entry_guard entry{out, detail::runtime_execution_entry_reentry::allow_public_llvm_raw};
        out.boundary_cfas[2] = reinterpret_cast<std::uintptr_t>(__builtin_dwarf_cfa());
        boundary_guard boundary{out, out.boundary_cfas[2], out.execution_depth == 1uz, out.tls.frames.size()};
        island_three(out);
        after(out);
    }
    [[gnu::noinline]] void island_two(output& out) noexcept
    {
        out.island_cfas[1] = reinterpret_cast<std::uintptr_t>(__builtin_dwarf_cfa());
        logical_guard frame{out, 101uz};
        boundary_two(out);
        after(out);
    }
    [[gnu::noinline]] void boundary_one(output& out) noexcept
    {
        entry_guard entry{out, detail::runtime_execution_entry_reentry::allow_public_llvm_raw};
        out.boundary_cfas[1] = reinterpret_cast<std::uintptr_t>(__builtin_dwarf_cfa());
        boundary_guard boundary{out, out.boundary_cfas[1], out.execution_depth == 1uz, out.tls.frames.size()};
        island_two(out);
        after(out);
    }
    [[gnu::noinline]] void island_one(output& out) noexcept
    {
        out.island_cfas[0] = reinterpret_cast<std::uintptr_t>(__builtin_dwarf_cfa());
        logical_guard frame{out, 100uz};
        boundary_one(out);
        after(out);
    }
    [[gnu::noinline]] void root_entry(output& out) noexcept
    {
        entry_guard entry{out};
        out.boundary_cfas[0] = reinterpret_cast<std::uintptr_t>(__builtin_dwarf_cfa());
        boundary_guard root{out, out.boundary_cfas[0], true, out.tls.frames.size()};
        island_one(out);
        after(out);
    }
    [[gnu::noinline]] void unanchored_outer_island(output& out) noexcept
    {
        // The old v1 defect: execution began without a published outer anchor,
        // yet the first nested public raw reentry was treated as the root.
        entry_guard entry{out};
        island_one(out);
        after(out);
    }
    [[gnu::noinline]] void missing_entry(output& out) noexcept
    { entry_guard entry{out}; capture(out); after(out); }

    [[gnu::noinline]] void peer_anchor(std::uintptr_t& cfa, std::latch& ready, std::latch& release) noexcept
    {
        // Keep a REAL native peer CFA live. The owning test only copies its
        // integer value; it never borrows a peer record or scans another stack.
        cfa = reinterpret_cast<std::uintptr_t>(__builtin_dwarf_cfa());
        ready.count_down();
        release.wait();
        std::atomic_signal_fence(std::memory_order_seq_cst);
    }
    [[gnu::noinline]] void wrong_anchor_entry(output& out) noexcept
    {
        entry_guard entry{out};
        std::latch ready{1}, release{1};
        std::thread peer{peer_anchor, std::ref(out.wrong_peer_cfa), std::ref(ready), std::ref(release)};
        ready.wait();
        {
            boundary_guard wrong{out, out.wrong_peer_cfa, true, 0uz};
            capture(out);
        }
        release.count_down();
        peer.join();
        after(out);
    }
    [[gnu::noinline]] void invalid_depth_entry(output& out) noexcept
    {
        entry_guard entry{out};
        boundary_guard invalid{out, reinterpret_cast<std::uintptr_t>(__builtin_dwarf_cfa()), true, 1uz};
        capture(out);
        after(out);
    }
    [[gnu::noinline]] void nonmonotonic_depth_inner(output& out) noexcept
    {
        boundary_guard invalid{out, reinterpret_cast<std::uintptr_t>(__builtin_dwarf_cfa()), false, 0uz};
        capture(out);
        after(out);
    }
    [[gnu::noinline]] void nonmonotonic_depth_entry(output& out) noexcept
    {
        entry_guard entry{out};
        logical_guard frame{out, 51uz};
        boundary_guard root{out, reinterpret_cast<std::uintptr_t>(__builtin_dwarf_cfa()), true, 1uz};
        nonmonotonic_depth_inner(out);
        after(out);
    }
    [[gnu::noinline]] void overflowing_boundaries(output& out, unsigned remaining) noexcept
    {
        boundary_guard boundary{out, reinterpret_cast<std::uintptr_t>(__builtin_dwarf_cfa()), false, 0uz};
        if(remaining == 0u) { capture(out); }
        else { overflowing_boundaries(out, remaining - 1u); }
        after(out);
    }
    [[gnu::noinline]] void overflow_entry(output& out) noexcept
    {
        entry_guard entry{out};
        boundary_guard root{out, reinterpret_cast<std::uintptr_t>(__builtin_dwarf_cfa()), true, 0uz};
        overflowing_boundaries(out, 64u);  // 65 transition records plus a root.
        after(out);
    }
    [[gnu::noinline]] void recursive_boundaries(output& out, unsigned index) noexcept
    {
        logical_guard frame{out, 42uz};
        out.boundary_cfas[index] = reinterpret_cast<std::uintptr_t>(__builtin_dwarf_cfa());
        boundary_guard boundary{out, out.boundary_cfas[index], false, out.tls.frames.size()};
        if(index == 3u) { capture(out); }
        else { recursive_boundaries(out, index + 1u); }
        after(out);
    }
    [[gnu::noinline]] void recursion_entry(output& out) noexcept
    {
        entry_guard entry{out};
        out.boundary_cfas[0] = reinterpret_cast<std::uintptr_t>(__builtin_dwarf_cfa());
        boundary_guard root{out, out.boundary_cfas[0], true, 0uz};
        recursive_boundaries(out, 1u);
        after(out);
    }
    struct payload_error { output* identity; std::uint64_t bits; };
    struct cleanup_guard { output& out; ~cleanup_guard() noexcept { ++out.cleanup; } };
    [[gnu::noinline]] void throw_leaf(output& out)
    { capture(out); throw payload_error{&out, 0xfedcba9876543210ULL}; }
    [[gnu::noinline]] void rethrow_boundary(output& out)
    {
        boundary_guard boundary{out, reinterpret_cast<std::uintptr_t>(__builtin_dwarf_cfa()), false, 0uz};
        try { cleanup_guard cleanup{out}; throw_leaf(out); }
        catch(payload_error const& error)
        {
            if(error.identity == &out && error.bits == 0xfedcba9876543210ULL && out.cleanup == 1u) { ++out.caught; }
            throw;  // Genuine C++ propagation; never a noexcept ABI alias.
        }
    }
    [[gnu::noinline]] void throwing_root(output& out)
    {
        entry_guard entry{out};
        out.boundary_cfas[0] = reinterpret_cast<std::uintptr_t>(__builtin_dwarf_cfa());
        boundary_guard root{out, out.boundary_cfas[0], true, 0uz};
        cleanup_guard cleanup{out};
        rethrow_boundary(out);
        after(out);
    }
    using scenario = void (*)(output&);
    [[gnu::noinline]] void outer_host_shell(output& out, unsigned remaining, scenario body)
    {
        if(remaining == 0u) { body(out); }
        else { outer_host_shell(out, remaining - 1u, body); }
        // At least 81 ordinary native host activations remain live beneath the
        // execution root even with -O3; no sibling-call elimination is possible.
        after(out);
    }
    std::size_t position(h::llvm_jit_unwind_backtrace_storage const& trace, std::uintptr_t cfa) noexcept
    {
        for(std::size_t index{}; index != trace.size; ++index)
        { if(trace.cfas[index] == cfa) { return index; } }
        return trace.size;
    }
    std::vector<std::size_t> merged(output const& out, h::llvm_jit_unwind_backtrace_storage const& trace, bool& finished)
    {
        // Called only by capture, while the actual borrowed records are LIVE.
        std::vector<std::size_t> identities;
        auto emit = [&](h::call_stack_frame frame) noexcept { identities.push_back(frame.function_index); };
        auto state = h::begin_llvm_jit_native_frame_merge(out.tls, emit);
        for(std::size_t index{}; index != trace.size; ++index)
        {
            h::consume_llvm_jit_native_unwind_boundary(state, trace.cfas[index], emit);
            for(std::size_t island{}; island != out.island_cfas.size(); ++island)
            { if(out.island_cfas[island] != 0u && trace.cfas[index] == out.island_cfas[island]) { identities.push_back((island + 1uz) * 1000uz); } }
        }
        finished = h::llvm_jit_native_frame_merge_finished(state);
        return identities;
    }
}

int main()
{
    // Native tests evaluate the actual storage/CFA chain. Product source
    // fingerprint and exact extracted candidate blocks are checked by runner.
    output anchored;
    outer_host_shell(anchored, 80u, root_entry);
    CHECK(anchored.selected == anchored.boundary_cfas[0]);
    CHECK(anchored.bounded.stopped_at_boundary && anchored.bounded.size < 64uz);
    CHECK(anchored.full.size == 64uz && !anchored.full.stopped_at_boundary);
    CHECK(position(anchored.bounded, anchored.boundary_cfas[0]) + 1uz == anchored.bounded.size);
    for(std::size_t index{1uz}; index != 4uz; ++index)
    { CHECK(position(anchored.bounded, anchored.boundary_cfas[index]) < position(anchored.bounded, anchored.boundary_cfas[index - 1uz])); }
    for(auto cfa: anchored.island_cfas) { CHECK(position(anchored.bounded, cfa) < anchored.bounded.size); }
    CHECK((anchored.merged_bounded == std::vector<std::size_t>{103uz, 102uz, 3000uz, 101uz, 2000uz, 100uz, 1000uz}));
    CHECK(anchored.merged_bounded == anchored.merged_full && anchored.bounded_merge_finished && anchored.full_merge_finished);
    CHECK(anchored.tls.llvm_jit_native_boundary == nullptr && anchored.tls.frames.empty() && anchored.execution_depth == 0uz);

    output no_root;
    outer_host_shell(no_root, 80u, unanchored_outer_island);
    CHECK(no_root.selected == 0u && no_root.bounded.size == 64uz && !no_root.bounded.stopped_at_boundary);
    CHECK(no_root.legacy_selected == no_root.boundary_cfas[1] && no_root.legacy.stopped_at_boundary);
    CHECK(position(no_root.bounded, no_root.island_cfas[0]) < no_root.bounded.size);
    CHECK(position(no_root.legacy, no_root.island_cfas[0]) == no_root.legacy.size);
    CHECK(no_root.merged_bounded == anchored.merged_bounded && no_root.bounded_merge_finished);
    CHECK(no_root.merged_legacy.size() + 1uz == no_root.merged_bounded.size() && no_root.legacy_merge_finished);

    for(auto body: std::array<scenario, 4>{missing_entry, invalid_depth_entry, nonmonotonic_depth_entry, overflow_entry})
    {
        output rejected;
        outer_host_shell(rejected, 80u, body);
        CHECK(rejected.selected == 0u && rejected.bounded.size == 64uz && !rejected.bounded.stopped_at_boundary);
        CHECK(rejected.full.size == 64uz && rejected.tls.llvm_jit_native_boundary == nullptr);
    }
    output wrong;
    outer_host_shell(wrong, 80u, wrong_anchor_entry);
    CHECK(wrong.wrong_peer_cfa != 0u && wrong.selected == wrong.wrong_peer_cfa);
    CHECK(wrong.bounded.size == 64uz && !wrong.bounded.stopped_at_boundary);
    CHECK(position(wrong.bounded, wrong.wrong_peer_cfa) == wrong.bounded.size);
    CHECK(!wrong.bounded_merge_finished);

    output recursion;
    outer_host_shell(recursion, 80u, recursion_entry);
    CHECK(recursion.bounded.stopped_at_boundary && recursion.selected == recursion.boundary_cfas[0]);
    CHECK((recursion.merged_bounded == std::vector<std::size_t>{42uz, 42uz, 42uz}));
    CHECK(recursion.bounded_merge_finished);
    for(std::size_t index{1uz}; index != 4uz; ++index)
    {
        auto current = position(recursion.bounded, recursion.boundary_cfas[index]);
        CHECK(current < recursion.bounded.size && recursion.bounded.regions[current] != 0u);
        if(index > 1uz)
        {
            auto previous = position(recursion.bounded, recursion.boundary_cfas[index - 1uz]);
            CHECK(current < previous && recursion.boundary_cfas[index] != recursion.boundary_cfas[index - 1uz]);
        }
    }
    // An entry's builtin DWARF CFA identifies its return boundary. Backtrace
    // associates that CFA with the caller context: boundary 1 is therefore in
    // recursion_entry, whereas boundaries 2 and 3 are in recursive_boundaries.
    // Prove all three physical recursive activations independently of function
    // pointer spelling (which can be a descriptor or signed pointer).
    auto recursive_outer = position(recursion.bounded, recursion.boundary_cfas[2]);
    auto recursive_inner = position(recursion.bounded, recursion.boundary_cfas[3]);
    auto recursive_region = recursion.bounded.regions[recursive_outer];
    CHECK(recursive_region != 0u && recursion.bounded.regions[recursive_inner] == recursive_region);
    std::size_t recursive_physical_frames{};
    for(std::size_t index{}; index != recursion.bounded.size; ++index)
    {
        if(recursion.bounded.regions[index] != recursive_region) { continue; }
        CHECK(recursion.bounded.cfas[index] != 0u);
        for(std::size_t previous{}; previous != index; ++previous)
        {
            if(recursion.bounded.regions[previous] == recursive_region)
            { CHECK(recursion.bounded.cfas[previous] != recursion.bounded.cfas[index]); }
        }
        ++recursive_physical_frames;
    }
    CHECK(recursive_physical_frames >= 3uz);
    output throwing;
    try { outer_host_shell(throwing, 80u, throwing_root); }
    catch(payload_error const& error)
    { CHECK(error.identity == &throwing && error.bits == 0xfedcba9876543210ULL); ++throwing.outer; }
    catch(...) { CHECK(false); }
    CHECK(throwing.cleanup == 2u && throwing.caught == 1u && throwing.outer == 1u);
    CHECK(throwing.bounded.stopped_at_boundary && throwing.tls.llvm_jit_native_boundary == nullptr && throwing.execution_depth == 0uz);
    ::fast_io::io::println("PASS native EH boundary: ", checks,
        " checks; real 81 host frames, nested reentry, preserved v1 omission, unreachable peer CFA, invalid chain, recursion and C++ rethrow; helper only, no JIT qualification");
}
#else
int main()
{
    ::fast_io::io::println("SKIP native EH boundary: eligible GNU/Clang DWARF CFA ABI required; no JIT qualification");
    return 77;
}
#endif
#undef CHECK
#undef UWVM_TEST_EH_BOUNDARY_AVAILABLE
