// Real optimized producer probe: an addressless aggregate with two independently
// live fields may be represented by DW_OP_piece/bit_piece locations. The native
// test must inspect actual DWARF; this source alone does not guarantee emission.
struct ObjectFragments { int left; int right; };
static volatile int source_pieces_observed;
extern "C" __attribute__((noinline, export_name("source_pieces_leaf_cpp")))
int source_pieces_leaf_cpp(int value)
{ source_pieces_observed = value; return value + 1; }
extern "C" __attribute__((noinline, export_name("source_pieces_outer_cpp")))
int source_pieces_outer_cpp(int seed)
{
    ObjectFragments fragments{seed * 3, seed + 7};
    source_pieces_observed = fragments.left; // PIECES_CPP_STOP
    int forwarded = source_pieces_leaf_cpp(fragments.left);
    source_pieces_observed = fragments.right;
    return forwarded ^ fragments.right;
}
extern "C" __attribute__((export_name("_start"))) void _start()
{ if(source_pieces_outer_cpp(5) != 28) { __builtin_trap(); } }
