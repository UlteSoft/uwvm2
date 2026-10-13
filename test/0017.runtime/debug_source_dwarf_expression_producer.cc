// Actual C++ producer SOURCE fixture. Pending keeper compilation with -g/-O0
// for wasm32 AND wasm64 and real DWARF/index/product observation; never claim
// the synthetic metadata tests above qualify this compiler or source stopping.
// Freestanding, no file/native pointer access and no import dependencies.
static_assert(sizeof(unsigned) == 4u);
namespace fixture
{
    struct node { unsigned value; node* next; };
    struct holder { node* ptr; node* array[2]; };
    volatile unsigned stopped{};
}
extern "C" __attribute__((noinline)) unsigned source_expression_fixture()
{
    fixture::node nodes[3]{{11u, nullptr}, {22u, nullptr}, {33u, nullptr}};
    // [owned three nodes ... 0 ... 1 ... 2] end
    // [safe                              ] unsafe (one-past)
    //  ^^ all initial pointer derivations target checked constant in-array indices.
    fixture::node* p[2]{&nodes[0], &nodes[2]};
    fixture::holder root{&nodes[1], {&nodes[0], &nodes[2]}};
    // The different actual targets make *p[1] (33) vs (*p)[1] (22) observable.
    // Break at this real line before loads, using verified emitted debug rows.
    fixture::stopped = 1u;
    // [safe] p contains two proven in-array pointers; p[0] has at least two
    // nodes remaining before +1 implicit indexing. Every load stays in nodes.
    return (*p[1]).value + ((*p)[1]).value + root.ptr->value + root.array[1]->value;
}
