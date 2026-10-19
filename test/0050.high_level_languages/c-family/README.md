# C and C++ producer/debug fixtures

These standalone examples need no ROS SDK or ROS authority. The C fixture uses
a struct/function-pointer interface, arrays, UTF-8 bytes and defined uint32
arithmetic. C++ additionally uses virtual dispatch, std::array, std::vector and
std::string. Both include an un-inlined debug_value function whose scalar changes
from40 to42 across a source step. C++ uses C linkage for that function so its
exact name is comparable with all/none host selections; this does not test C++
name demangling.

Using an installed WASI SDK, build and run the basic command:

    "$WASI_SDK/bin/clang" --target=wasm32-wasip1 -O2 -g c_language.c -o c_language.wasm
    "$WASI_SDK/bin/clang++" --target=wasm32-wasip1 -std=c++20 -O2 -g -fno-exceptions -fno-rtti cpp_language.cpp -o cpp_language.wasm
    uwvm --run c_language.wasm
    uwvm --run cpp_language.wasm

Use the runtime/backend options appropriate to the selected product. Its
supported Wasm features and admitted WASI resources still apply. These examples
use stdout and require no directories, clocks, random source or ROS imports.
For source-level inspection use -O0 -g -gdwarf-4 and preserve the source files at
their recorded paths. Optimized examples retain producer names/debug data but
do not promise every local remains observable.

The explicit no-exceptions/no-RTTI C++ profile is a bounded foundation test.
It does not certify C++ exception unwinding, dynamic_cast, threads or every
standard-library facility. Existing exception/threaded suites retain their own
qualification. Native reference builds and Wasmtime/three-UWVM execution
checks are separate recorded steps; no timings from this fixture are a native
performance claim.

All fifteen host language-debug adapters default on. See
[language switches](../../../docs/debugger/language-options.md). Trimming an
adapter does not reject legal Wasm from that language; keeping full debug
support is recommended. Source-level values and generic Wasm control are
qualified separately in the actual Linux matrix.

[Actual Debug/Release, native reference and source/variable results](../results/native-budget/language-c-family-20261019/README.md)
record the frozen compiler profiles and all68 closed actor commands, including
four additional serial-profile regressions. Their untimed durations are not
native performance results.
