# Source pretty printers

The source debugger formats owned DWARF objects through finite library layout
profiles. Printing never calls the inferior, loads an executable printer from a
guest, or interprets a Wasm function-table index as a memory address. Unknown
layouts retain their original copied object display.

Use `set print pretty on|off` to choose the session default, `show print pretty`
to inspect it, and `print /r EXPR` or `print /p EXPR` to override one query.
`set print elements N` accepts 0 through 4096; zero requests a summary without
container element copies. Independent byte, node, depth and read budgets still
bound larger requests. `show print elements` and `info pretty-printer` work
before a guest starts. Print preferences survive the standalone `run` command.
Automatic displays and TUI inspection use the same preferences.
Authenticated broker sessions retain the same preferences. Changing printer
configuration retires DAP's cached source views without advancing the guest.

Application layouts can be registered without executable scripts:

```
pretty-printer sequence appvec "application::Vector<*>" data count capacity
pretty-printer string apptext "application::Text" data size
pretty-printer pointer owner "application::Owner<*>" pointer
pretty-printer optional maybe "application::Maybe<*>" engaged value
pretty-printer disable appvec
pretty-printer enable appvec
pretty-printer delete appvec
pretty-printer list
```

Names select immutable host profiles; paths select actual DWARF fields. Exact
type patterns take priority over bounded `*`/`?` patterns. A registry holds at
most 64 profiles, and the built-in group can be toggled with the name `builtin`.
Mutation acknowledgements are short; listing preserves complete rows within
its response budget and reports shown/total counts explicitly.

## Profiles and metadata

| Family | Required evidence |
| --- | --- |
| libstdc++ and libc++ vector | Consistent begin/end/capacity pointer types, target widths, offsets and ordered aligned ranges |
| libstdc++ C++11 string and string view | Exact data pointer and size fields; copied character type |
| libc++ string | Explicit `__is_long_`, branch-specific size/data metadata and copied availability; explicit short and long layout profiles |
| C++ span | Dynamic extent fields or an exact static template constant |
| GNU/libc++ deque, forward_list, list, ordered and unordered map/set | Typed nodes, unambiguous storage, verified links and bounded traversal; libc++ deque block size belongs to the exact deque type |
| GNU/libc++ vector of bool and bitset | Typed storage words, exact lengths/bit counts and bounded copied prefixes |
| C++ optional, variant and expected | Copied discriminant selects the exact template payload and preserves its availability; invalid discriminants retain raw fields |
| C++ unique/shared/weak pointer | Typed stored/control pointers, copied ABI ownership counts; weak pointers never read the pointee |
| C++ array, pair and tuple | Already copied fields; a finite scalar preview |
| C++ stack, queue, priority_queue and chrono values | Exact backing-container/representation template relation and copied field |
| Rust Vec and String | Explicit Rust language, matching pointer/length/capacity and raw-buffer profile |
| C++ coroutine handle and compiler frame | Exact single-pointer handle or compiler-specific complete frame schema |

The type index retains type/value template parameters, complete aggregate DIEs
and finite class static constants under the existing metadata budgets. Exact
specification references bind out-of-class constants to their owner. Instance
member constants do not establish a class ABI constant. A type spelling does
not prove a template relationship or authorize a read. Unsupported old string
ABIs, fancy pointers, incomplete types and unrecognized library layouts retain
the raw object. Stateful compressed-pair storage is selected by its template
type and ordinal, rather than an ambiguous repeated `__value_` member name.

Container indexing for matched contiguous vector/string/view/span profiles
uses the same source expression grammar and read transaction as ordinary
source pointers. The whole declared range must fit the guest address width
before selecting one element. A negative index, index outside the length,
unknown pointer carrier or malformed descriptor cannot initiate a copy.
`std::array` indexing selects its proven local storage array and needs no
additional memory read. Summary strings have a 4096-byte ceiling, including
nested printer summaries.

## Copies, limits and protocol

`source_language_expression` invokes the formatter inside the runtime's existing
canonical stopped-memory transaction. That transaction authenticates the stop,
participant, activation, code/source generation and memory ownership. Printer
reads share its remaining read allowance. The pure formatter receives only
owned bytes, availability masks, immutable type records and the synchronous
copy callback; it retains none of these after return.

The default formatter limits are 32 copies, 64 KiB copied payload, 1024 result
nodes, 200 elements and eight recursive printer levels. Member discovery has a
4096-edge work limit per path, a finite depth and explicit ambiguity/cycle
rejection. Nested containers share the same allowance. Linear payload prefixes
are copied in one batch; DWARF validation is performed once before element
builders walk their selected types. Linked structures track visited addresses
and verify complete termination when they claim a complete result.

Each heap element root is marked `copied-child` in the bounded source-object
display. Its byte extent belongs to its own separately copied object; ordinary
descendants remain inside that object. A quoted escaped `pretty` field is
presentation data. The DAP adapter keeps raw values and the copied tree, displays
the summary and allows paging the retained elements. Neither field creates an
`evaluateName`, `memoryReference`, pointer dereference permission or source
selector. Stop/error retirement still clears the original references. Raw
queries preserve the original object display.

## Coroutine interpretation

A non-null erased coroutine handle identifies a guest frame offset, but does
not prove a frame type or promise offset. It is displayed with that limitation.
An explicitly typed Clang or GCC frame can show its copied resume/destroy/index
fields and promise subtree. Null resume with non-null destroy can identify a
done/final-suspended frame; non-null entries identify a resumable frame. These
bytes cannot in general distinguish running from suspended, or destroyed from
uninitialized storage. Such lifecycle states stay unknown. No printer follows
resume/destroy function values as linear-memory pointers.

Reference implementations were studied, with independently implemented layout
checks here:

- [GDB source](https://sourceware.org/git/?p=binutils-gdb.git), commit `8f7fb58377a9a50c668190a4a740a4078355076d`.
- [LLVM libc++/LLDB source](https://github.com/llvm/llvm-project/tree/fe04b9b583adcf40c44c3f525682ec28af4509ae), including C++ container formatters and `Coroutines.cpp`.
- [GCC coroutine lowering](https://github.com/gcc-mirror/gcc/blob/9a00f0797c90485cd70ac86a8751c9565c3979dd/gcc/cp/coroutines.cc).
- [Clang coroutine debugging](https://clang.llvm.org/docs/DebuggingCoroutines.html).

Component DATA tests, compiler-produced live CLI/DAP tests, QEMU component
coverage and native hardware measurements are separate qualifications. A
formatter microbenchmark is not an end-to-end GDB/LLDB latency measurement;
QEMU component success does not qualify every target's complete debugger.
