// Actual production cleanup scopes, exercised with throwing callbacks outside the runtime's still-noexcept entry ABI.
#include <uwvm2/runtime/lib/uwvm_runtime_activation_cleanup.h>

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <type_traits>
#include <initializer_list>

namespace cleanup = uwvm2::runtime::lib::details;
namespace
{
    unsigned checks{};
#define CHECK(condition) do { ++checks; if(!(condition)) { std::fprintf(stderr, "FAIL %u: %s\n", __LINE__, #condition); std::abort(); } } while(false)

    struct scratch_arena
    {
        struct mark_t { std::size_t used{}; };
        std::size_t used{};
        unsigned marks{};
        unsigned releases{};

        mark_t mark() noexcept { ++marks; return {used}; }
        void release(mark_t saved) noexcept
        {
            CHECK(saved.used <= used);
            used = saved.used;
            ++releases;
        }
    };

    struct boundary
    {
        boundary const* previous{};
        unsigned depth{};
    };

    using scratch_scope = cleanup::runtime_scratch_mark_scope<scratch_arena>;
    using boundary_scope = cleanup::runtime_atomic_borrowed_record_scope<boundary>;
    static_assert(!std::is_copy_constructible_v<scratch_scope> && !std::is_move_constructible_v<scratch_scope>);
    static_assert(!std::is_copy_constructible_v<boundary_scope> && !std::is_move_constructible_v<boundary_scope>);
    static_assert(std::is_nothrow_destructible_v<scratch_scope> && std::is_nothrow_destructible_v<boundary_scope>);

    struct observed_boundary
    {
        boundary record;
        boundary const*& head;
        unsigned& retired;
        ~observed_boundary() noexcept
        {
            // This runs after the scope's destructor: no pointer to this dying stack record may remain published.
            CHECK(std::atomic_ref<boundary const*>{head}.load(std::memory_order_acquire) == record.previous);
            ++retired;
        }
    };

    struct injected_exception {};

    int nested_activation(scratch_arena& arena, boundary const*& head, unsigned depth, bool use_scratch,
                          unsigned exit_kind, unsigned& retired)
    {
        scratch_scope scratch{use_scratch ? &arena : nullptr};
        if(use_scratch) { arena.used += 17; }
        auto previous = std::atomic_ref<boundary const*>{head}.load(std::memory_order_acquire);
        observed_boundary current{{previous, depth}, head, retired};
        boundary_scope published{head, previous, &current.record};
        CHECK(std::atomic_ref<boundary const*>{head}.load(std::memory_order_acquire) == &current.record);
        if(previous != nullptr) { CHECK(previous->depth == depth + 1); }
        if(depth != 0) { return nested_activation(arena, head, depth - 1, use_scratch, exit_kind, retired); }
        if(exit_kind == 1) { return 7; }
        if(exit_kind == 2) { throw injected_exception{}; }
        return 42;
    }

    void exit_paths()
    {
        for(bool use_scratch : {false, true})
        {
            for(unsigned depth : {0u, 1u, 63u})
            {
                for(unsigned exit_kind : {0u, 1u, 2u})
                {
                    scratch_arena arena{29};
                    boundary root{nullptr, depth + 1};
                    alignas(std::atomic_ref<boundary const*>::required_alignment) boundary const* head{&root};
                    unsigned retired{};
                    bool caught{};
                    try
                    {
                        int result = nested_activation(arena, head, depth, use_scratch, exit_kind, retired);
                        CHECK(result == (exit_kind == 1 ? 7 : 42));
                    }
                    catch(injected_exception const&) { caught = true; }
                    CHECK(caught == (exit_kind == 2));
                    CHECK(arena.used == 29);
                    CHECK(arena.marks == (use_scratch ? depth + 1 : 0));
                    CHECK(arena.releases == arena.marks);
                    CHECK(retired == depth + 1);
                    CHECK(std::atomic_ref<boundary const*>{head}.load(std::memory_order_acquire) == &root);
                }
            }
        }
    }

    void catch_then_continue()
    {
        scratch_arena arena{5};
        alignas(std::atomic_ref<boundary const*>::required_alignment) boundary const* head{};
        unsigned retired{};
        {
            scratch_scope outer_scratch{&arena};
            arena.used += 10;
            observed_boundary outer{{nullptr, 4}, head, retired};
            boundary_scope outer_publication{head, nullptr, &outer.record};
            for(unsigned repetition{}; repetition != 64; ++repetition)
            {
                try
                {
                    nested_activation(arena, head, 3, true, 2, retired);
                    CHECK(false);
                }
                catch(injected_exception const&) {}
                CHECK(arena.used == 15);
                CHECK(arena.marks == arena.releases + 1);
                CHECK(std::atomic_ref<boundary const*>{head}.load(std::memory_order_acquire) == &outer.record);
            }
        }
        CHECK(arena.used == 5);
        CHECK(arena.marks == 257);
        CHECK(arena.releases == arena.marks);
        CHECK(retired == 257);
        CHECK(std::atomic_ref<boundary const*>{head}.load(std::memory_order_acquire) == nullptr);
    }
}

// Compile this source with -O3 -S to compare successful-path manual cleanup against the production scope. The callback
// is noexcept so these probes isolate normal-path instructions; the tests above separately verify exception unwinding.
struct cleanup_codegen_arena
{
    using mark_t = std::size_t;
    std::size_t cursor;
    mark_t mark() const noexcept { return cursor; }
    void release(mark_t saved) noexcept { cursor = saved; }
};
using cleanup_codegen_callback = void (*)(cleanup_codegen_arena*) noexcept;
extern "C" void cleanup_scratch_manual(cleanup_codegen_arena* arena, cleanup_codegen_callback callback) noexcept
{
    auto saved = arena == nullptr ? 0 : arena->mark();
    callback(arena);
    if(arena != nullptr) { arena->release(saved); }
}
extern "C" void cleanup_scratch_scoped(cleanup_codegen_arena* arena, cleanup_codegen_callback callback) noexcept
{
    cleanup::runtime_scratch_mark_scope<cleanup_codegen_arena> scope{arena};
    callback(arena);
}
using cleanup_codegen_record = unsigned;
using cleanup_codegen_record_callback = void (*)(cleanup_codegen_record const*&) noexcept;
extern "C" void cleanup_boundary_manual(cleanup_codegen_record const*& slot, cleanup_codegen_record const* published,
                                          cleanup_codegen_record_callback callback) noexcept
{
    auto previous = std::atomic_ref<cleanup_codegen_record const*>{slot}.load(std::memory_order_acquire);
    std::atomic_ref<cleanup_codegen_record const*>{slot}.store(published, std::memory_order_release);
    callback(slot);
    std::atomic_ref<cleanup_codegen_record const*>{slot}.store(previous, std::memory_order_release);
}
extern "C" void cleanup_boundary_scoped(cleanup_codegen_record const*& slot, cleanup_codegen_record const* published,
                                          cleanup_codegen_record_callback callback) noexcept
{
    auto previous = std::atomic_ref<cleanup_codegen_record const*>{slot}.load(std::memory_order_acquire);
    cleanup::runtime_atomic_borrowed_record_scope<cleanup_codegen_record> scope{slot, previous, published};
    callback(slot);
}

int main()
{
    exit_paths();
    catch_then_continue();
    std::printf("PASS %u activation cleanup checks: normal/early/throw/nested/repeated-catch, scratch and borrowed atomic boundaries\n", checks);
}
