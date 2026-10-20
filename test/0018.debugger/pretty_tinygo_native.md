# TinyGo source debugger acceptance

The live producer profile is TinyGo 0.40.1, Go 1.25.14 and LLVM 20.1.1,
targeting WASIp1 with original compiler DWARF. The independent Linux native
oracle uses the same Go fixture source with Go 1.25.14. Native success alone
does not qualify the Wasm debugger: the unchanged Wasm must also reach its
actual source breakpoint, satisfy the variable checks, and exit normally.

Build each fixture under the designated Linux test cgroup:

    tinygo build -target=wasi -opt=0 -scheduler=none -gc=leaking \
      -no-debug=false -p=1 -o fixture.wasm fixture.go

The fixtures are in ../0017.runtime/fixtures:

| Fixture | Actual checks |
| --- | --- |
| debug_source_tinygo_collections.go | maps, named maps, nil and empty maps; buffered, named, closed, nil, send-only and receive-only channels; len/cap and arithmetic |
| debug_source_tinygo_arrays.go | arrays, nested arrays, named arrays, zero-length and zero-sized elements, array pointers, UTF-8 string carriers |
| debug_source_tinygo_builtins.go | string/slice len, slice cap, nil and empty descriptors |
| debug_source_tinygo.go | producer and native oracle only; basic source sessions remain unqualified |

Use run_dap_tinygo_objects.py and run_dap_tinygo_collections.py with the
collections fixture; run_dap_tinygo_array_objects.py with the arrays fixture;
and ../0017.runtime/run_debug_source_tinygo_builtins_cli.py with the builtins
fixture. Each runner requires --uwvm, --build-receipt, --wasm, --source and
--out. The build receipt must describe the exact fresh executable. Run both
--policy instruction and --policy unwind. Add --ros for the ROS executable
or --fixed-llvm-full for the stable executable.

A call-stack policy named instruction is independent of DAP stepping
granularity. These DAP clients select source level and request statement
stepping. Native instruction stepping requires a genuine owned native trap;
a cooperative source stop must not be described as a successful native step.

The DAP checks keep scalar values separate from type spelling, require
read-only opaque references, exercise named/indexed pagination, reject
malformed expressions, and verify that execution retires old frames,
scopes and copied objects. They wait for the original guest and broker to
exit normally. Legacy before-control flags are separate from current
acceptance.

A launch pause can precede every Wasm participant. The DAP thread list then
stays empty and the stopped event has no invented thread ID. After setup or
introspection, these clients request threads to refresh the actual stopped
status, then continue the complete debug domain with threadId: 0. Zero is a
routing value for that continuation, not a participant or frame selector.
Single-thread continuation is unavailable. Refused control arguments retire
cached stopped authority; a fresh authenticated status is required afterward.

The inherited late debug endpoint uses a genuine registered guest worker
and waits for managed native, execution, producer and OS-thread retirement.
WASI proc_exit preserves the original guest code; Linux process status uses
its low eight bits. Closing or quitting the endpoint detaches and continues
the guest. run_late_control_proc_exit.py checks normal return, zero/nonzero/
UINT32_MAX exits, quit and disconnect under both policies. It keeps the
parent endpoint open until original-process retirement except in the explicit
disconnect case, and verifies that retirement with the retained pidfd.

The TinyGo 0.40 map descriptor has eight fields and no flags field. Its
Wasm32 layout is 40 bytes; the no-scheduler channel descriptor is 36 bytes.
The evaluator checks the complete owned DWARF layout, including field
types, offsets and total extent. It copies only the bounded map/channel
header to answer len/cap and never follows bucket, function or queue
pointers. Later flags/slot-width layouts retain their existing checks.
Unknown layouts remain unavailable.

source_tinygo_map_layout.cc tests 32/64-bit descriptor data, legacy/flags/slot
layouts, nil maps, read limits, missing pointer bits, provider refusal,
integer bounds and malformed layouts. It is registered as
--case source_tinygo_map_layout in run_pretty_printer_matrix.py. QEMU
component runs validate these data operations on the requested ABIs; they
do not qualify a foreign full LLVM runtime or source debugger.

Map and channel source types may be erased to runtime pointer DIEs by this
producer. The current acceptance covers their header queries and copied
struct/pointer views, not typed enumeration of map entries. It does not
establish standard Go-Wasm, every TinyGo release/scheduler, or all-language
pretty-printer parity.
