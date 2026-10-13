// Allocation policy on original WASI FD-table count fixtures. These tests
// grant no capture authority; the runtime test separately uses a genuine stop.
#include <uwvm2/imported/wasi/wasip1/fd_manager/impl.h>
#include <fast_io.h>
namespace fm=::uwvm2::imported::wasi::wasip1::fd_manager;
static unsigned checks{},cases{};
static void require(bool good,char const* why)
{ ++checks;if(!good) { ::fast_io::io::perrln("wasip1_fd_slot_reuse: ",::fast_io::mnp::os_c_str(why));::fast_io::fast_terminate(); } }
int main(int argc,char** argv)
{
    if(argc!=2) { return 64; }
    for(unsigned dense{};dense!=17u;++dense)
    {
        fm::wasm_fd_storage_t table{};
        for(unsigned n{};n!=dense;++n) { table.opens.emplace_back(fm::wasi_no_construct); }
        for(unsigned sparse{};sparse!=17u;++sparse)
        {
            if(sparse) { table.renumber_map.emplace(INT32_MAX-static_cast<int>(sparse),fm::wasi_fd_unique_ptr_t{fm::wasi_no_construct}); }
            for(unsigned closed{};closed!=dense+2u;++closed)
            {
                table.closes.clear();for(unsigned n{};n!=closed;++n) { table.closes.push_back(n); }
                ++cases;
                // Reference counts include reserved dense cells and one scan
                // cell for each sparse FD, irrespective of its numeric value.
                auto total=dense+sparse;
                auto after=total+(closed==0u ? 1u : 0u);
                for(unsigned limit{};limit!=total+3u;++limit)
                {
                    require(table.fits_allocation_scan_limit(limit)==(closed<=dense && after<=limit),
                        "allocation must reuse a closed cell or charge one additional scan cell");
                    auto occupied=closed<=dense ? dense-closed+sparse : 0u;
                    require(table.fits_occupied_slot_limit(limit)==(closed<=dense && occupied<=limit),
                        "occupied FD policy counts reserved slots separately from reusable closed cells");
                }
                require(table.fits_allocation_scan_limit(SIZE_MAX)==(closed<=dense),
                    "maximum host size budget cannot overflow the subtraction bound");
            }
        }
    }
    fm::wasm_fd_storage_t boundary{};
    boundary.opens.reserve(65535u);
    for(unsigned n{};n!=65535u;++n) { boundary.opens.emplace_back(fm::wasi_no_construct); }
    boundary.renumber_map.emplace(INT32_MAX,fm::wasi_fd_unique_ptr_t{fm::wasi_no_construct});
    ++cases;
    require(!boundary.fits_allocation_scan_limit(65536u),"full scan table refuses an additional FD");
    require(boundary.fits_allocation_scan_limit(65537u),"one scan cell of actual growth is admissible");
    boundary.closes.push_back(65534u);
    require(boundary.fits_allocation_scan_limit(65536u),"exact 65536-cell table can reuse one dense hole");
    require(!boundary.fits_allocation_scan_limit(65535u),"a reusable hole cannot excuse an already oversized scan table");
    require(boundary.fits_occupied_slot_limit(65535u),"closed hole reduces occupancy without changing scan size");
    boundary.renumber_map.emplace(INT32_MAX-1,fm::wasi_fd_unique_ptr_t{fm::wasi_no_construct});
    require(!boundary.fits_allocation_scan_limit(65536u),"sparse entries also enforce the scan bound during reuse");
    ::fast_io::dir_file directory{::fast_io::mnp::os_c_str(argv[1])};
    ::fast_io::native_file evidence{::fast_io::at(directory),u8"slot-policy.txt",
        ::fast_io::open_mode::out|::fast_io::open_mode::creat|::fast_io::open_mode::excl};
    ::fast_io::io::println(evidence,"actual native policy run checks=",checks," cases=",cases);
    ::fast_io::io::println("wasip1_fd_slot_reuse ",checks," checks passed cases=",cases," unsupported=0");
}
