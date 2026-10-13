// Real -g producer fixture: constant aggregate/array/enum/self-pointer layout.
// Freestanding Wasm32; its guest offset must never be treated as a host pointer.
typedef enum ObjectShade { shade_negative = -1, shade_warm = 3 } ObjectShade;
typedef struct ObjectNode
{
    int value;
    unsigned char lanes[3];
    ObjectShade shade;
    struct ObjectNode* next;
} ObjectNode;
static volatile int object_observed;
__attribute__((noinline, export_name("source_objects_checkpoint_c")))
int source_objects_checkpoint_c(ObjectNode const* node)
{
    int result = node->value + node->lanes[2] + (int)node->shade;
    object_observed = result;
    return result;
}
__attribute__((noinline, export_name("source_objects_outer_c")))
int source_objects_outer_c(int seed)
{
    ObjectNode object;
    object.value = seed; object.lanes[0] = 2; object.lanes[1] = 4; object.lanes[2] = 6;
    object.shade = shade_warm;
    object.next = &object;
    object_observed = object.value; // OBJECT_C_STOP: inspect object before call.
    int result = source_objects_checkpoint_c(&object);
    return result;
}
__attribute__((export_name("_start"))) void _start(void)
{ if(source_objects_outer_c(5) != 14) { __builtin_trap(); } }
