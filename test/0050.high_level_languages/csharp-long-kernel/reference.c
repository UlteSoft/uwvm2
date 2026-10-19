#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
int main(void) {
 uint64_t value=UINT64_C(0x12345678);
 for(uint64_t i=0;i<UINT64_C(1000000000);++i) {
  value=(value^(value<<13))&UINT64_C(0xffffffff);
  value=(value^(value>>17))&UINT64_C(0xffffffff);
  value=(value^(value<<5))&UINT64_C(0xffffffff);
  value=(value+i)&UINT64_C(0xffffffff);
 }
 printf("%" PRIu64 "\n",value);
 return 0;
}
