PRIVATE constructor-failure successor of the actual-target VIEW plus exact
CALL64r GPR DATA accessor. BEFORE production is exact operand-after-VIEW R4 AFTER;
apply only as a narrow delta to that closed dependency, never overlay LIVE.

Both public MC constructors remain noexcept. All possibly allocating Triple,
string, table/context/disassembler initialization runs inside private helpers
invoked inside a real C++-exception catch in supported builds. Allocation failure
leaves the partially constructed tables under unique_ptr RAII and resets the
provider, so callers receive unavailable DATA. In a no-exception LLVM/allocator
build the allocator's existing OOM behavior remains; this source does not claim
to catch LLVM fatal-abort contracts or signals. No target/owner/step permission
or ordinary Wasm instruction is changed.

The own-main `native_owned_decoder_allocation_failure.cc` component injects one
real C++ allocation failure matching MCContext allocation size through global
operator new, once in each constructor. Storage ownership uses FastIO's existing
native allocator. The test must observe two actual failures, both unavailable
providers, and later constructor recovery; allocation-size collision inside an
SDK's fatal allocator is an actual fixture failure, not a PASS/SKIP. Its target
record is explicit structural owned DATA produced from real registered MC tables,
not a published engine or code capability. It never executes emitted machine
code or dereferences a PC. Keeper-only compile/run in original Linux 64GiB cgroup,
20-second/512MiB runtime bound, fresh actual header/SDK/BMI closure. No native or
C++ compilation was performed while preparing this packet.
