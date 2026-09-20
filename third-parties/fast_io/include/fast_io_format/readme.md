# Why `fast_io_new` Still Provides `format` — and Why Format Strings Stop at Output

`fast_io_new` is fundamentally built around typed I/O operations and static manipulators, not around format strings. The project README explicitly describes static I/O manipulators as a core design choice.

So why does `fast_io_new` still provide a complete formatting frontend?

Because **format strings are useful as a notation for output layout**.

They are not a good general abstraction for I/O.

That distinction is important.

## `format` is a frontend, not an I/O engine

The `fast_io_new` format implementation is intentionally separated from the actual I/O layer.

A format literal is compiled into structural format IR, validated at compile time, lowered into ordinary typed I/O arguments, and then passed to the existing `fast_io::io` machinery. The implementation explicitly states that successful formatted output ultimately becomes `fast_io::io::print`, while formatted string materialization goes through the normal concat engine.

The grammar compiler itself is `consteval`. The source even makes the design requirement explicit: format parsing must not silently move to runtime.

The public interface follows the same rule:

```cpp
fast_io::fmt::print<"value = {}">(value);
```

The format is a template argument, not an arbitrary runtime string.

Runtime array forms are deliberately deleted instead of falling back to a runtime parser.

So the model is:

```text
compile-time format literal
        ↓
compile grammar
        ↓
structural format IR
        ↓
lower fields into typed manipulators / typed I/O arguments
        ↓
fast_io::io::print
```

There is no reason for formatted output to require a second runtime formatting engine.

The format syntax is simply a convenient compile-time frontend for constructing the same operation graph that `fast_io::io` already understands.

This is also why formatted materialization can use the same design:

```cpp
auto str = fast_io::fmt::concat_std<"x = {}, y = {}">(x, y);
```

The format layer still lowers into the ordinary concat machinery; allocation, sizing, and leaf formatting remain responsibilities of the I/O layer.

In other words:

> **`format` is syntax sugar for output composition. It is not the foundation of the I/O model.**

That allows us to provide familiar `{fmt}`-style notation without forcing the runtime architecture to become `{fmt}`-style architecture.

It also explains why good performance is still possible: after compile-time parsing and lowering, the runtime operation is ordinary `fast_io::io`. There is no need to rediscover the format grammar while performing I/O.

Performance should of course be measured on a particular compiler, platform, workload, and library revision rather than treated as a universal constant. The architectural point is simpler: **the format frontend does not require a separate runtime format interpreter**.

---

## The four fundamental I/O relationships

A useful way to understand the design is to stop thinking in terms of individual function names and instead look at the two kinds of things involved:

* **values**, which can be printed or scanned;
* **I/O objects**, which can produce or consume data.

From those two categories we obtain four natural operations:

| Operation          | From  | To     | Meaning                       |
| ------------------ | ----- | ------ | ----------------------------- |
| `print` / `concat` | value | output | produce a representation      |
| `scan`             | input | value  | interpret input as values     |
| `to`               | value | value  | convert through I/O protocols |
| `transmit`         | input | output | move data between I/O objects |

This is the actual I/O model.

### `print` and `concat`: value → output

`print` combines things being output with an output object:

```cpp
fast_io::io::print(out, "value = ", value, "\n");
```

Conceptually:

```text
printable values
      ↓
 output object
```

`concat` is the materializing version of the same idea:

```cpp
auto str = fast_io::concat_std(
    "value = ",
    value,
    "\n"
);
```

Instead of writing into an external output object, the output representation is materialized as a string-like object. The repository contains this exact style of `concat_std` usage.

These operations are fundamentally about **producing output**.

That is precisely the domain where a format string makes sense.

---

## Why format strings work for output

Consider:

```cpp
fast_io::fmt::print<"x = {}, y = {}">(x, y);
```

The meaning of the literals is completely straightforward:

```text
"x = "    → output these characters
{}        → output x
", y = "  → output these characters
{}        → output y
```

The format describes a construction.

There is no ambiguity about what ordinary characters mean.

There is also no need for the formatter to inspect some unknown future input before deciding what operation should happen next.

Formatting is normally a forward process:

```text
known format
+
known arguments
+
output object
        ↓
produce characters
```

The caller controls the output.

That makes a format string a compact and useful notation for a linear output layout.

And because the format literal is known at compile time, `fast_io_new` can compile that notation into the ordinary typed I/O operations it would have executed anyway.

This is the narrow reason that `format` is worth keeping.

---

# `scan`: input → value

Scanning is fundamentally different:

```cpp
int value;
fast_io::io::scan(input, value);
```

Conceptually:

```text
input object
    ↓
scannable value
```

The important word here is **input object**.

An input object is not necessarily a complete string that already exists in memory.

It may be:

```text
a string or contiguous range
a memory-mapped region
a FILE*
a buffered file
stdin
a pipe
a socket
a terminal
another incremental source
```

For a whole-memory range, all input may happen to be visible.

For a stream, it may not be.

A `FILE*`, for example, normally exposes an implementation-managed buffer rather than making the entire underlying file globally available at once.

So a scanner fundamentally operates more like:

```text
scanner state
+
currently available input
        ↓
consume some input
        ↓
new scanner state
+
success / failure / need more input
```

That is a state machine.

Value extraction is only one possible action of that state machine.

---

# Why a scan format string is the wrong abstraction

Suppose somebody tries to mirror formatting:

```cpp
format("{}:{}")
scan("{}:{}")
```

It looks beautifully symmetric.

Semantically it is not symmetric at all.

For formatting:

```text
':' means:
output ':'
```

For scanning:

```text
':' means:
inspect unknown input,
require that the next character is ':',
consume it,
otherwise fail
```

The exact same syntax has changed semantic category.

In formatting, a literal is **data being produced**.

In scanning, a literal is **a grammar production controlling a parser state transition**.

That is already a warning sign.

Then the problem gets worse.

A realistic scanner needs operations such as:

```text
consume input
skip input
inspect without consuming
remember state
resume after partial input
match a delimiter
scan until a delimiter
branch depending on input
repeat a production
discard a recognized value
perform custom parsing
```

A replacement field such as:

```text
{}
```

normally means something like:

```text
recognize
+
consume
+
convert
+
produce a C++ value
```

But that is only one scanner operation.

Sometimes we need:

```text
recognize + consume
```

without producing a value.

Sometimes:

```text
inspect
```

without consuming.

Sometimes:

```text
consume until condition
```

Sometimes:

```text
branch
```

A scanner is therefore not fundamentally a sequence of value extractions.

It is a state machine in which **some transitions happen to produce values**.

---

## IPv4 hides this problem

IPv4 is unusually friendly to a format-string scanner:

```text
uint8 '.' uint8 '.' uint8 '.' uint8
```

It is almost perfectly linear:

```text
extract
match '.'
extract
match '.'
extract
match '.'
extract
```

So a hypothetical API such as:

```cpp
scan<"{}.{}.{}.{}">(a, b, c, d);
```

looks convincing.

But IPv4 is the easy case.

It happens to have a nearly fixed grammar.

---

## IPv6 exposes the real scanner

IPv6 immediately requires more:

```text
2001:db8:0:0:0:0:0:1
2001:db8::1
::1
::
::ffff:192.0.2.1
```

Now the scanner may have to:

```text
scan a hexadecimal group

inspect the next input

if there is one ':':
    continue to another group

if there is '::':
    remember the zero-compression position
    change state
    continue with a variable number of remaining groups

possibly recognize an IPv4 tail

determine how many groups were omitted

construct the final address
```

The interesting operations are no longer just the integer extractions.

The interesting operations are the transitions **between** those extractions.

A format string powerful enough to express all of this would eventually need concepts equivalent to:

```text
match
skip
peek
until
repeat
choice
if
state
backtracking or controlled alternatives
custom productions
```

At that point we have not created a better scan API.

We have simply implemented another parser language inside a string.

That is exactly what static manipulators and typed scanner objects avoid.

---

# Why `format` should therefore stop at the print side

A format string is good at answering:

> **What output layout do I want to produce?**

That maps naturally onto `print` and `concat`.

A format string is not naturally good at answering:

> **How should a parser react to unknown input while maintaining incremental state?**

That belongs to `scan` manipulators and scanner state objects.

This is why providing:

```cpp
fast_io::fmt::print<"...">
```

does **not** imply that we should also provide:

```cpp
fast_io::fmt::scan<"...">
```

The operations are not mathematical inverses just because their surface syntax can be made to look symmetric.

Output formatting describes layout.

Input scanning describes behavior.

---

# `to`: value → value

`to` occupies another quadrant.

Conceptually:

```text
printable value(s)
        ↓
scannable value
```

For example, the repository demonstrates `fast_io::to<T>` in a `constexpr` context.

This is not external input or output.

It is a conversion between the two typed protocol worlds.

One side knows how to produce a representation.

The other side knows how to consume one.

That gives us:

```text
value → value
```

There is no reason to introduce a format string as the central abstraction here either.

If the producing side needs special representation rules, those should be expressed by output manipulators.

If the consuming side requires special parsing rules, those belong to the scanner.

A single format string attempting to govern both sides would once again blur two independent protocols.

---

# `transmit`: input → output

Finally:

```cpp
transmit(output, input);
```

is:

```text
input object
     ↓
output object
```

The repository demonstrates exactly this relationship with:

```cpp
transmit(fast_io::c_stdout(), fast_io::c_stdin());
```

No value formatting is inherently required.

No value scanning is inherently required.

It is transport.

On platforms where more efficient mechanisms exist, this abstraction may even map to operations fundamentally different from repeated high-level formatting and parsing.

Trying to put a format string here would be conceptually backwards.

If transformation is needed during transmission, it should be represented explicitly as an adapter, codec, filter, manipulator, or transformation stage—not by pretending that raw transport is formatting.

---

# The resulting model

The complete picture is therefore:

```text
                         DESTINATION

                 value                 output
            ┌──────────────────┬──────────────────┐
            │                  │                  │
   value    │       to         │  print / concat  │
            │                  │                  │
SOURCE      ├──────────────────┼──────────────────┤
            │                  │                  │
   input    │      scan        │     transmit     │
            │                  │                  │
            └──────────────────┴──────────────────┘
```

This gives every operation a clear meaning:

```text
print / concat:
    things being output → output representation

scan:
    input source → things being input

to:
    things being output → things being input

transmit:
    input source → output destination
```

Only one quadrant fundamentally describes **output layout**:

```text
print / concat
```

That is why only that quadrant naturally benefits from format-string syntax.

---

# Format strings are notation, not architecture

The important design rule is therefore not:

> fast_io must never support format strings.

That would be unnecessarily restrictive.

The better rule is:

> **A format string may be provided when it is a useful compile-time notation for an operation that is already naturally expressible by the typed I/O model.**

For output, this works extremely well.

The format compiler can turn:

```cpp
fast_io::fmt::print<"{} has {} messages\n">(name, count);
```

into the conceptual equivalent of:

```cpp
fast_io::io::print(
    name,
    " has ",
    count,
    " messages\n"
);
```

with the appropriate output manipulators inserted for field specifications.

The string syntax disappears into typed I/O before runtime I/O begins.

That is a useful frontend.

It does not replace the underlying abstraction.

---

# Why runtime format strings are different

This distinction is also why compile-time formatting matters.

A runtime format string means the program must treat formatting syntax as runtime data:

```text
runtime string
    ↓
runtime parse
    ↓
runtime semantic validation
    ↓
runtime dispatch
    ↓
I/O
```

A fully compile-time format instead allows:

```text
format literal
    ↓
consteval parser
    ↓
compile-time validation
    ↓
compile-time structural program
    ↓
typed fast_io::io operations
    ↓
runtime I/O
```

The current format implementation explicitly uses `consteval` compilation and rejects runtime-array spellings instead of silently introducing a runtime parser.

That is the important difference.

We provide a **compile-time format language**.

We do not make format strings the runtime architecture of the library.

---

# Why there is no corresponding scan-format language

A compile-time scan string would eliminate runtime parsing of the *scan description*, but it would not eliminate the fundamental runtime parser.

The actual input is still unknown.

The scanner must still maintain state.

It may still have only part of the input available.

It may still need to suspend and resume.

It may still branch.

It may still recognize variable structures.

So compiling:

```cpp
scan<"{}:{}">
```

at compile time only compiles a description of the parser.

It does not remove the parser.

And if the description language is weak, it handles only trivial fixed layouts.

If it becomes powerful enough for realistic protocols, we have created another regex/parser/state-machine DSL inside a string.

Neither result is attractive.

The typed scanner protocol already is the state machine.

There is no need to serialize that state machine into a format string and then compile it back into C++ operations.

---

# Design principle

The distinction can be summarized very simply:

> **Formatting describes what we produce. Scanning describes what we do in response to input.**

Producing output is naturally declarative and usually linear.

Parsing input is inherently stateful.

Therefore:

```text
format + print      ✓
format + concat     ✓

format + scan       ✗
format + to         ✗
format + transmit   ✗
```

The first two use a format string as a compact output-layout notation.

The others would use a format string to model abstractions that are not fundamentally formatting problems.

That is where format strings become historical baggage rather than useful syntax.

---

# Final rule

`fast_io_new` should support format syntax without becoming a format-string-based I/O library.

The architecture remains:

```text
typed I/O objects
+
typed values
+
typed manipulators
+
typed scan state
```

The format frontend exists only because output formatting has one special property:

> **A compile-time output layout can be lowered completely into ordinary typed output operations before I/O begins.**

That is why `format` belongs on top of `print` and `concat`.

It is also why we should resist the temptation to mechanically mirror it into `scan`, `to`, or `transmit`.

Surface symmetry is cheap.

Correct abstractions matter more.
