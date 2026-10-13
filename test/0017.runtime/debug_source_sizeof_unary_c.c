/* Independent C17/C23 unary sizeof witnesses; no IO or operand execution. */
volatile int guarded_value;
short guarded_array[5];
int* guarded_pointer;
#define U_ASSERT(expr,value) _Static_assert((expr)==(value),"native unary sizeof witness")
U_ASSERT(sizeof guarded_value,sizeof(int));
U_ASSERT(sizeof +guarded_value,sizeof(int));
U_ASSERT(sizeof -guarded_value,sizeof(int));
U_ASSERT(sizeof ~guarded_value,sizeof(int));
U_ASSERT(sizeof !guarded_value,sizeof(int));
U_ASSERT(sizeof sizeof guarded_value,sizeof(__SIZE_TYPE__));
U_ASSERT(sizeof -1 + 2,sizeof(int)+2);
U_ASSERT(sizeof -1 * 2,sizeof(int)*2);
U_ASSERT(!sizeof guarded_value,0);
U_ASSERT(sizeof !!guarded_value,sizeof(int));
U_ASSERT(sizeof '(' ,sizeof(int));
U_ASSERT(sizeof 1.0,sizeof(double));
U_ASSERT(sizeof -1.0f,sizeof(float));
U_ASSERT(sizeof + (short)guarded_value,sizeof(int));
U_ASSERT(sizeof - (unsigned char)guarded_value,sizeof(int));
U_ASSERT(sizeof *guarded_pointer,sizeof(int));
U_ASSERT(sizeof guarded_array,5*sizeof(short));
U_ASSERT(sizeof guarded_array[1],sizeof(short));
U_ASSERT(sizeof guarded_pointer,sizeof(void*));
U_ASSERT(sizeof 1 ? 2 : 3,2);
int main(void)
{ return sizeof guarded_value!=sizeof(int) || sizeof *guarded_pointer!=sizeof(int) || sizeof guarded_array!=5*sizeof(short) || sizeof -1+2!=sizeof(int)+2 || sizeof sizeof guarded_value!=sizeof(__SIZE_TYPE__); }
