This PRIVATE source proposal supplies only the existing pause domain's cold
external-park accounting transaction. It does not issue a Wasm/native address,
code-owner, call target, breakpoint, execution or restore permission.

`with_externally_parked_participant_for_resume` holds ONE real domain mutex. It
checks the current actual ticket, ALLN, nonempty enrolled roster, no retained
checkpoint transition, and the selected real external slot. Its noncopyable,
nonmovable `external_resume_borrow` lives only inside this synchronous callback.
`commit` removes the actual parked count/flags BEFORE calling the bounded
nonthrowing internal backend wake. False restores the flags/count under the
same mutex, and each borrow admits at most one attempt. The helper returns the
actual successful commit, never a callback-supplied boolean.

The runtime must additionally authenticate its canonical physical trap/cursor,
current true caller+callee endpoints/source/generation/epoch and genuine trap
revision while holding publication inside this same domain transaction. Native
register inspection must release its native transition guard before acquiring
the wake guard. Publication stays held until true event preparation, actual
park accounting and backend wake finish; independent ALLN replacements then
see a real running participant. Controller mutex or immediate query alone
cannot provide this ordering. No borrowed callback, location or resume object
may escape, and no recursive domain call or worker/ACK wait is allowed inside.
Preflight may throw before commit; after successful wake it must be nonthrowing.

`cooperative_external_resume.cc` uses actual domain leases and real participant
polling/external accounting, failed wake rollback, wrong-ticket refusal, a real
competing ALLN mutator and a real selected worker gate. That last gate tests
ONLY the domain seam, not a kernel JIT permission or perf SIGTRAP. Build/run only
by the Linux keeper inside the existing 64GiB cgroup, bounded 30 seconds and
256MiB for the runtime fixture. Existing module partition already exports this
header; fresh header/BMI qualification remains mandatory. No native compilation
or execution was performed while producing this source packet.
