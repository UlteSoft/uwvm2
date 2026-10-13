struct Widget { int value; int method(int x) const & { return value+x; } };
__attribute__((noinline)) int callee(int x) { return x+1; }
__attribute__((noinline)) void type_target()
{
    Widget widget{7};int matrix[2][3]{{1,2,3},{4,5,6}};
    int (*function)(int)=callee;int (*array_pointer)[2][3]=&matrix;
    int Widget::* data_member=&Widget::value;
    int (Widget::* method_member)(int) const & = &Widget::method;
    for(;;) { asm volatile("" : : "r"(&widget),"r"(&matrix),"r"(&function),"r"(&array_pointer),"r"(&data_member),"r"(&method_member) : "memory"); } // TYPES_READY
}
extern "C" void _start() { type_target(); }
