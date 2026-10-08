# Actual runtime native-role vocabulary

The cold runtime/full/hot/staged producer marks actual optimized defined functions with `uwvm.native.code-owner.role` values `"0"`, `"1"`, `"2"`, `"3"`. The LLVM23 actual AsmPrinter must recognize these same numeric strings: helper/unknown0, typed1, resume2, raw3. Wire numbers and revision2 remain unchanged. Labels `typed/resume/raw` are not an alternate accepted vocabulary and cannot grant a role.

The genuine object and alias LLVM IR probes now use precisely the runtime values, including explicit helper0. Rebuild the exact fresh SDK after this source-only fix, then emit actual ELF/COFF/Mach-O objects and exercise the existing complete all-symbol/true-begin/end object graph tests. A component built using the old text labels cannot qualify runtime publication. Producer and cache identity must be bound to this actual source inventory; no previous SDK/native qualification is claimed. The macro remains default OFF.
