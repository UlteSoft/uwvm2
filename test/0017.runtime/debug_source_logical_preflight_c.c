// Actual C17/C23 operator result types and sequencing, independent of the parser.
#define CHECK(expr) _Static_assert(__builtin_types_compatible_p(__typeof__(expr),int),"C logical result is int")
CHECK((char)1 && (short)2);
CHECK((unsigned int)1 || (long)2);
CHECK((long long)1 && (unsigned long long)2);
CHECK((float)1 || (double)2);
CHECK((_Bool)1 && (_Bool)0);
CHECK(0 && (1/0));
CHECK(1 || (1/0));
static int reads;
static int value(void) { ++reads;return 2; }
int main(void)
{
 for(int i=0;i!=10000;++i)
 {
  if((0 && value())!=0 || (1 || value())!=1 || reads!=i*2)return 1;
  if((1 && value())!=1 || (0 || value())!=1 || reads!=(i+1)*2)return 2;
  if((0 && (1/0))!=0 || (1 || (1/0))!=1)return 3;
 }
 return reads==20000 ? 0 : 4;
}
