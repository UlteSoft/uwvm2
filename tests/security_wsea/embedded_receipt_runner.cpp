// Exercises the production per-load receipt, including in-place metadata changes.
#include <uwvm2/utils/security/wsea.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

int main(int argc,char** argv)
{
    if(argc!=3) { return 64; }
    namespace wsea=uwvm2::utils::security::wsea;
    auto selected{wsea::select_module(argv[2],argv[1])};
    if(!selected) { std::cout<<"DENY "<<selected.code<<'\n'; return 2; }
    std::ifstream file(argv[2],std::ios::binary);
    std::vector<char> image{std::istreambuf_iterator<char>{file},{}};
    auto bytes{reinterpret_cast<std::byte const*>(image.data())};
    auto verified{wsea::verify_module(argv[2],bytes,image.size())};
    if(!verified || !verified.authorization) { std::cout<<"DENY "<<(verified.code?verified.code:"VM_003")<<'\n'; return 2; }
    auto matches=[&] { return verified.authorization->matches(bytes,image.size()); };
    if(!matches()) { return 3; }
    image.back()^=1; bool metadata_rejected{!matches()}; image.back()^=1;
    image[7]^=1; bool content_rejected{!matches()}; image[7]^=1;
    auto copy{image}; bool pointer_rejected{!verified.authorization->matches(reinterpret_cast<std::byte const*>(copy.data()),copy.size())};
    if(!metadata_rejected || !content_rejected || !pointer_rejected || !matches()) { return 4; }
    std::cout<<"READY receipt-integrity-pass"<<std::endl;
    for(std::string command; std::getline(std::cin,command);)
    {
        if(command=="match") { std::cout<<(matches()?"MATCH":"DENY")<<std::endl; }
        else if(command=="quit") { return 0; }
        else { return 64; }
    }
    return 0;
}
