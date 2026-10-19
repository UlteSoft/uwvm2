// Real ROS ABI import producer v1. Compile unchanged with -O0 -g and exactly
// one of -DPRETTY_ROS1 / -DPRETTY_ROS2. No native ABI stub is supported.
#if !defined(__wasm__)
# error "The ROS import qualification fixture requires actual WebAssembly imports"
#endif
#if defined(PRETTY_ROS1) && !defined(PRETTY_ROS2)
# include "../../../sdk/ros1/abi.h"
# define PRETTY_ROS_ABI_VERSION uwvm_ros1_abi_version
# define PRETTY_ROS_IMPORT_MODULE "uwvm_ros1_v1"
#elif defined(PRETTY_ROS2) && !defined(PRETTY_ROS1)
# include "../../../sdk/ros2/abi.h"
# define PRETTY_ROS_ABI_VERSION uwvm_ros2_abi_version
# define PRETTY_ROS_IMPORT_MODULE "uwvm_ros2_v1"
#else
# error "Select exactly one real ROS ABI using PRETTY_ROS1 or PRETTY_ROS2"
#endif
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
extern "C" __attribute__((used)) char const pretty_ros_fixture_identity[]=
    "uwvm-dbg-pretty-ros-imports-v1;" PRETTY_ROS_IMPORT_MODULE ";abi_version";
volatile unsigned pretty_ros_result;
extern "C" __attribute__((noinline)) int pretty_ros_import_probe(::std::uint32_t version)
{
    ::std::vector<int> values{7,8,9};
    ::std::string short_text{"small"};
    ::std::string long_text{"this is a string longer than the short string buffer"};
    ::std::optional<int> present{7};
    asm volatile("" : : : "memory");
    pretty_ros_result=version+values[1]+short_text.size()+long_text.size()+*present; // PRETTY_ROS_IMPORTS_STOP
    return version==1u && values.size()==3u && values[0]==7 && values[1]==8 && values[2]==9 &&
        short_text=="small" && long_text=="this is a string longer than the short string buffer" &&
        present==7 && pretty_ros_result==73u ? 0 : 1;
}
int main()
{
    auto const version{PRETTY_ROS_ABI_VERSION()};
    if(version!=1u) { return 2; }
    return pretty_ros_import_probe(version);
}
