// Real standard-library references; compile unchanged with -O0 -g.
#include <array>
#include <string>
#include <vector>
struct ReferenceMember { int value; };
volatile int pretty_reference_result;
extern "C" [[gnu::noinline]] int pretty_reference_probe(std::vector<int> const& ref,
    std::vector<int>& mut, std::vector<int>&& rvalue, std::string const& text,
    std::array<int,3> const& fixed, int const& scalar, ReferenceMember const& object)
{
    auto const& alias=ref;
    auto const& scalar_ref=scalar;
    auto const* pointer=&ref;
    unsigned vector_bytes=sizeof(ref),scalar_bytes=sizeof(scalar);
    asm volatile("" : : : "memory");
    pretty_reference_result=ref[1]+mut[0]+rvalue[1]+fixed[1]+scalar_ref+object.value; // PRETTY_REFERENCES_STOP
    return ref[1]!=8 || alias[2]!=9 || (*pointer)[0]!=7 || mut[1]!=11 || rvalue[1]!=14 ||
        text!="reference text" || fixed[1]!=17 || scalar!=19 || object.value!=20 || vector_bytes==0u || scalar_bytes!=sizeof(int);
}
int main()
{
    std::vector<int> values{7,8,9},mutable_values{10,11,12},moved_values{13,14,15};
    std::string title{"reference text"};std::array<int,3> fixed{16,17,18};int scalar{19};ReferenceMember object{20};
    return pretty_reference_probe(values,mutable_values,static_cast<std::vector<int>&&>(moved_values),title,fixed,scalar,object);
}
