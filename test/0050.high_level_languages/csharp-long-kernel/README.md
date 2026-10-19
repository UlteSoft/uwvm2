# Longer C# hardware-profile fixture

This is a prepared follow-up to the frozen 100-million-iteration profile.
VTune explicitly warns that its roughly 0.2-second native CoreCLR kernel is
too short for reliable metrics. Passing the basic target PID/TID event-sample
gate does not establish reliable multiplexed microarchitecture percentages.
The original reports, warning and their pass/fail fields remain unchanged.

The same source is compiled for native CoreCLR and Mono/WASI. Its permitted
maximum/default becomes one billion iterations and it rejects extra arguments.
The seed, xorshift/add loop, semantic checks and READY/G/DONE/D protocol stay
the same. The kernel timer stops before DONE marker output; host gate wall time
is a separate measurement. The wider-integer C reference provides an independent
long checksum. A new source hash requires actual new builds and references.

Build Debug and Release, native and wasi, with the pinned SDK/package cache:

~~~sh
bash build.sh Debug native /absolute/output/debug-native
bash build.sh Debug wasi /absolute/output/debug-wasi
bash build.sh Release native /absolute/output/release-native
bash build.sh Release wasi /absolute/output/release-wasi
~~~

Set DOTNET_ROOT to .NET SDK 8.0.408, WASI_SDK_PATH to WASI SDK 20 and
NUGET_PACKAGES to the already populated offline cache. NuGet.Config clears
network feeds; a missing prerequisite fails rather than downloading implicitly.
Keep the portable PDB and retained Wasm DWARF. The Wasm project deliberately
records its Mono interpreter strategy; native CoreCLR is a different strategy.

Qualification is queued on SSH Linux in the original 64 GiB/swap-zero cgroup,
after the earlier source/debug/performance cohorts. It must compare the same
one-billion count in every engine, keep failed/warning rows, and inspect the
actual collector logs before interpreting percentages. The existing conservative
10 ms sampling policy remains in force. A longer kernel is not an automatic
guarantee of reliable metrics.

Intel explains the precision tradeoff in
[Allow Multiple Runs or Multiplex Events](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2025-1/allow-multiple-runs-or-multiplex-events.html).
An attached target cannot use that tool's automatic multiple-launch mode.
No new timing ratio, microarchitecture conclusion, managed debugger or ROS SDK
parity is established by these prepared sources. All language debugger options
remain default-on; trimming specialization does not prohibit legal Wasm.
