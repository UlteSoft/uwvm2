// Freestanding real producer fixture; compile only in keeper's original cgroup.
// Stable globals exercise pointer/member/array chains. A live integer loop
// keeps a real Wasm stop available; no host/native pointer or import is used.
namespace pointerlab
{
    struct Node { int value; Node* next; int cells[2][2]; };
    Node nodes[2] = {{17, &nodes[1], {{1,2},{3,18}}}, {41, nullptr, {{5,6},{7,42}}}};
    Node* current = &nodes[0];
    volatile unsigned observed{};
}
extern "C" __attribute__((visibility("default"))) void source_pointer_loop()
{
    auto* live = pointerlab::current;
    for(;;)
    {
        pointerlab::observed = pointerlab::observed + 1u;
        asm volatile("" : : "r"(live) : "memory");
    }
}
