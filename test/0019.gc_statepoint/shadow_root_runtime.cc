// Standalone root-slot/collector experiment, deliberately outside product src.
// The required overlay supplies the exclusive local-only sweep prototype.
#include <fast_io.h>
#include <uwvm2/uwvm/runtime/storage/gc_object.h>
#include <array>
#include <cstdlib>
#include <initializer_list>
#include <memory>

namespace gc = ::uwvm2::uwvm::runtime::storage;
namespace types = ::uwvm2::parser::wasm::standard::wasm3::type;

struct root_frame
{
    root_frame* previous{};
    gc::gc_reference const* slots{};
    ::std::size_t capacity{};
    ::std::size_t live_count{};
};
static_assert(::std::is_standard_layout_v<root_frame>);
static_assert(sizeof(root_frame) == 4uz * sizeof(void*));
static_assert(sizeof(gc::gc_reference) == 2uz * sizeof(void*));

static thread_local root_frame* root_head{};
static gc::gc_object_store* active_store{};
static ::std::size_t collection_count{}, reclaimed_count{}, largest_frame_count{};
struct probe_exception {};

static void require(bool condition) noexcept
{
    if(!condition) { ::fast_io::fast_terminate(); }
}

extern "C" void uwvm_probe_root_enter(root_frame* frame, gc::gc_reference const* slots,
                                      ::std::size_t capacity) noexcept
{
    require(frame != nullptr && slots != nullptr && capacity != 0uz);
    // [native caller-owned frame and slot array] live until leave, including EH cleanup.
    // ^^ frame/slots are compiler-supplied addresses, never guest-supplied tokens.
    frame->previous = root_head;
    frame->slots = slots;
    frame->capacity = capacity;
    frame->live_count = 0uz;
    // [fully initialized frame][old root chain] this native thread alone publishes it.
    // ^^ root_head changes only after every borrowed address/count is initialized.
    root_head = frame;
}

extern "C" void uwvm_probe_root_publish(root_frame* frame, ::std::size_t count) noexcept
{
    require(frame != nullptr && frame == root_head && count <= frame->capacity);
    // All slots in [0,count) have been written by the IR before this call.
    // This single-thread experiment has no concurrent reader or collection handshake.
    frame->live_count = count;
}

extern "C" void uwvm_probe_root_leave(root_frame* frame) noexcept
{
    require(frame != nullptr && frame == root_head);
    // [current frame][older caller chain] both remain live at this cleanup point.
    // ^^ detach before the compiler releases the current native frame.
    root_head = frame->previous;
    frame->previous = nullptr;
    frame->slots = nullptr;
    frame->capacity = frame->live_count = 0uz;
}

extern "C" void uwvm_probe_collect() noexcept
{
    require(active_store != nullptr);
    ::std::array<gc::gc_reference, 64uz> roots{};
    ::std::size_t count{}, frames{};
    for(auto const* current{root_head}; current != nullptr;)
    {
        require(current->slots != nullptr && current->live_count <= current->capacity);
        require(current->live_count <= roots.size() - count);
        for(::std::size_t index{}; index != current->live_count; ++index)
        {
            // [0,live_count) initialized compiler root slots; [count,64) destination room.
            roots[count++] = current->slots[index];
        }
        ++frames;
        // [borrowed caller root chain] the current native call keeps every frame alive.
        // ^^ follow only the link installed by enter, before any frame can leave.
        current = current->previous;
    }
    if(frames > largest_frame_count) { largest_frame_count = frames; }
    ::std::size_t reclaimed{};
    require(active_store->collect_exclusive_local_graph(roots.data(), count, reclaimed) ==
            gc::gc_object_status::ok);
    ++collection_count;
    reclaimed_count += reclaimed;
    for(::std::size_t index{}; index != count; ++index)
    {
        gc::gc_object_value observed{};
        require(active_store->struct_get(roots[index], 0uz, false, observed) == gc::gc_object_status::ok);
    }
}

extern "C" void uwvm_probe_allocate_collect() noexcept
{
    gc::gc_reference garbage{};
    require(active_store->struct_new_default(0u, garbage) == gc::gc_object_status::ok);
    uwvm_probe_collect();
    gc::gc_object_value observed{};
    require(active_store->struct_get(garbage, 0uz, false, observed) == gc::gc_object_status::invalid_reference);
}

extern "C" void uwvm_probe_throw()
{
    uwvm_probe_allocate_collect();
    throw probe_exception{};
}

extern "C" void shadow_many_roots(gc::gc_reference*, gc::gc_reference const*) noexcept;
extern "C" void shadow_branch_root(gc::gc_reference*, gc::gc_reference const*, bool) noexcept;
extern "C" void shadow_loop_root(gc::gc_reference*, gc::gc_reference const*, ::std::size_t) noexcept;
extern "C" void shadow_exception_cleanup(gc::gc_reference const*);

[[nodiscard]] static types::recursive_type_section declarations()
{
    types::recursive_type_section section{};
    section.type_count = 1u;
    types::recursive_group group{};
    types::sub_type structure{};
    structure.kind = types::composite_kind::struct_;
    types::field_type field{};
    field.storage.value.kind = types::value_kind::i32;
    field.mutable_ = true;
    structure.fields.push_back(field);
    group.types.push_back(::std::move(structure));
    section.groups.push_back(::std::move(group));
    return section;
}

[[nodiscard]] static gc::gc_reference make(::std::uint32_t key)
{
    gc::gc_reference result{};
    auto const value{gc::gc_object_value::i32(key)};
    require(active_store->struct_new(0u, ::std::addressof(value), 1uz, result) == gc::gc_object_status::ok);
    return result;
}

static void verify(gc::gc_reference reference, ::std::uint32_t key)
{
    gc::gc_object_value observed{};
    if(active_store->struct_get(reference, 0uz, false, observed) != gc::gc_object_status::ok)
    {
        ::fast_io::io::print(::fast_io::err(), "shadow_root_missing: key=", ::fast_io::mnp::dec(key), "\n");
        ::fast_io::fast_terminate();
    }
    require(observed.as<::std::uint32_t>() == key);
}

int main()
{
    auto section{declarations()};
    auto leases{::std::make_shared<gc::gc_lease_owner>()};
    auto store{::std::make_shared<gc::gc_object_store>(section, leases)};
    require(store->valid());
    // [shared store: main scope] active_store is a test-owned borrowed pointer.
    active_store = store.get();
    ::std::array<gc::gc_reference, 12uz> inputs{}, outputs{};
    for(::std::size_t index{}; index != inputs.size(); ++index)
    { inputs[index] = make(static_cast<::std::uint32_t>(index + 11u)); }
    auto const dead{make(999u)};
    shadow_many_roots(outputs.data(), inputs.data());
    require(collection_count == 1uz && largest_frame_count == 2uz);
    require(root_head == nullptr);
    for(::std::size_t index{}; index != outputs.size(); ++index)
    {
        require(outputs[index].storage.ptr == inputs[index].storage.ptr);
        verify(outputs[index], static_cast<::std::uint32_t>(index + 11u));
    }
    require(reclaimed_count == 1uz);
    gc::gc_object_value observed{};
    require(store->struct_get(dead, 0uz, false, observed) == gc::gc_object_status::invalid_reference);

    // None of main's C++ arrays are implicit roots. After an IR frame leaves,
    // explicitly collecting with an empty chain reclaims all twelve objects.
    uwvm_probe_collect();
    require(reclaimed_count == 13uz);
    for(bool choose_first : {false, true})
    {
        ::std::array<gc::gc_reference, 2uz> branches{make(71u), make(72u)};
        gc::gc_reference output{};
        shadow_branch_root(::std::addressof(output), branches.data(), choose_first);
        auto const selected{choose_first ? 0uz : 1uz};
        verify(output, selected == 0uz ? 71u : 72u);
        require(store->struct_get(branches[1uz - selected], 0uz, false, observed) ==
                gc::gc_object_status::invalid_reference);
        require(root_head == nullptr);
        uwvm_probe_collect();
    }

    auto loop_input{make(123u)};
    gc::gc_reference loop_output{};
    auto const before_loop{collection_count};
    shadow_loop_root(::std::addressof(loop_output), ::std::addressof(loop_input), 7uz);
    require(collection_count - before_loop == 7uz && root_head == nullptr);
    verify(loop_output, 123u);
    uwvm_probe_collect();

    auto exception_input{make(456u)};
    auto const before_throw{collection_count};
    bool caught{};
    try { shadow_exception_cleanup(::std::addressof(exception_input)); }
    catch(probe_exception const&) { caught = true; }
    require(caught && root_head == nullptr && collection_count == before_throw + 1uz);
    verify(exception_input, 456u);
    uwvm_probe_collect();
    require(store->struct_get(exception_input, 0uz, false, observed) == gc::gc_object_status::invalid_reference);
    // Detach the borrowed test-global before the owning store leaves main's scope.
    active_store = nullptr;
    ::fast_io::io::println("shadow_root_frames PASS; experimental collections=", ::fast_io::mnp::dec(collection_count),
                          "; reclaimed=", ::fast_io::mnp::dec(reclaimed_count),
                          "; max_nested_frames=", ::fast_io::mnp::dec(largest_frame_count));
}
