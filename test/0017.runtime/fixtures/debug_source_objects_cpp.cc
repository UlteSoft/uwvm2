// Real C++ class/base/member/enum/bitfield and multidimensional array DWARF.
// No host callbacks, source-expression execution or external standard library.
enum class ObjectColor : int { negative = -1, warm = 3 };
struct ObjectBase { int base; };
class ObjectPacket : public ObjectBase
{
public:
    int grid[2][3];
    signed int signed_bits : 5;
    unsigned int flag_bits : 7;
    ObjectColor color;
    ObjectPacket* next;
};
static volatile int object_observed;
extern "C" __attribute__((noinline, export_name("source_objects_checkpoint_cpp")))
int source_objects_checkpoint_cpp(ObjectPacket const& object)
{
    int result = object.base + object.grid[1][2] + object.signed_bits + static_cast<int>(object.color);
    object_observed = result;
    return result;
}
extern "C" __attribute__((noinline, export_name("source_objects_outer_cpp")))
int source_objects_outer_cpp(int seed)
{
    ObjectPacket object;
    object.base = seed;
    object.grid[0][0] = 1; object.grid[0][1] = 2; object.grid[0][2] = 3;
    object.grid[1][0] = 4; object.grid[1][1] = 5; object.grid[1][2] = 6;
    object.signed_bits = -2; object.flag_bits = 91; object.color = ObjectColor::warm;
    object.next = &object;
    object_observed = object.base; // OBJECT_CPP_STOP: inspect object before call.
    int result = source_objects_checkpoint_cpp(object);
    return result;
}
extern "C" __attribute__((export_name("_start"))) void _start()
{ if(source_objects_outer_cpp(5) != 12) { __builtin_trap(); } }
