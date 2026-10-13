// Independent native C type/value witnesses; no IO.
#define UWVM_ESCAPE_CASE(expr,value) _Static_assert((expr)==(value),"C escape value"); _Static_assert(sizeof(expr)==sizeof(int),"C escape type");
#include "fixtures/debug_source_character_escape_cases.h"
#undef UWVM_ESCAPE_CASE
int main(void)
{
 unsigned checks=0;
#define UWVM_ESCAPE_CASE(expr,value) if((expr)!=(value) || sizeof(expr)!=sizeof(int)) { return 1; } ++checks;
#include "fixtures/debug_source_character_escape_cases.h"
#undef UWVM_ESCAPE_CASE
 return checks==283u?0:1;
}
