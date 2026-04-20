# A Mathematical and Microarchitectural Comparison of the u2 and wasm3 Interpreters

## Abstract

This note presents a paper-style comparison between **u2** (the UWVM2 interpreter) and **wasm3** (M3), focusing on the exact question raised in practice: **why can u2 approach ~2× speedup over wasm3 in flat, branch-poor, local-light scientific kernels, yet fall back to only ~10% advantage in more realistic kernels with more control flow, more locals, and larger instruction working sets?**

The central claim is not that one interpreter is universally superior. In fact, **u2 explicitly states that it does not claim to be “fastest” or “optimal” across all workloads, and it also explicitly trades code size for hot-path shape**. Its design goal is to accelerate stack-heavy WebAssembly by using a **direct-threaded pointer stream**, a **register-ring stack-top cache**, and **translation-time fusion**, whereas wasm3 uses a **tightly chained meta-machine**, a **small fixed virtual register file**, and **operation-level variantization/fusion**. :contentReference[oaicite:0]{index=0}

The mathematical result of this comparison is straightforward:

- u2 wins when it preserves a **high stack-top cache hit rate**, **high fusion density**, and a **small hot instruction working set**.
- u2 loses much of its edge when **control-flow boundaries**, **local-heavy operand sourcing**, and **large code footprints** reduce those advantages.
- Consequently, the user’s practical observation is structurally consistent with the public architecture documents: **u2’s peak advantage is a conditional high-water mark, not an unconditional average-case law**. :contentReference[oaicite:1]{index=1}

---

## 1. Architectural Summary

### 1.1 u2

u2 is described by its own repository as a **non-JIT, non-self-modifying, direct-threaded interpreter**. The translator emits a code page consisting of **opfunc pointers plus immediates**, so runtime execution does not re-decode Wasm bytecode. The design goal is to accelerate **stack-heavy Wasm** by turning hot scalar operations into mostly **register-resident ALU operations**. The distinguishing feature is a **register-ring stack-top cache**, together with adjacent fusion (`conbine*`) and `delay_local`-style provider/consumer fusion. u2 also states clearly that it **trades code size for predictable hot-path shape** and is **not positioned as a minimal/lightweight interpreter** in the wasm3 class. :contentReference[oaicite:2]{index=2}

### 1.2 wasm3

wasm3 translates Wasm opcodes into more efficient **operations**, which are **C functions with a single fixed signature** such as `Operation_Whatever(pc, sp, mem, r0, fp0)`. These operations are **tightly chained**: each operation drives execution forward by tail-calling or jumping to the next one, with no outer switch loop. wasm3 also uses **operation variants** and **fused operations**, and explicitly describes its design as translating the Wasm stack machine into a more direct **register-file approach** with a tiny virtual machine register set. wasm3 also publicizes a very small footprint target: roughly **~64 KB code and ~10 KB RAM** as minimum useful system requirements. :contentReference[oaicite:3]{index=3}

---

## 2. The Core Comparative Thesis

The user’s empirical claim can be formalized as follows:

> **u2 reaches its peak only in nearly flat, scientific, branch-poor, local-light kernels.**
> In those cases it may approach **~2× wasm3**.
> In kernels with many branches, blocks, loops, local accesses, and larger instruction working sets, the advantage may shrink to **~10%**.

This statement is highly plausible under the public architectural descriptions of both engines. u2 itself reports an AArch64/Apple Silicon workflow in which the wall-time geomean ratio was `uwvm/wasm3 = 0.5957`, while simultaneously warning that these are **not universal claims**. u2 also notes that when average fusion density falls to around **~1.5 Wasm ops per dispatch**, threaded interpreters converge toward the same dispatch ceiling and performance differences narrow; it further states that its broader generality increases dependence-chain length and register pressure in **memory-heavy or branch-heavy kernels**. :contentReference[oaicite:4]{index=4}

---

## 3. A Unified Performance Model

Let:

- \(N\): dynamic Wasm operation count
- \(\phi\): average **fusion density** = Wasm operations executed per dispatch
- \(D\): cost of one threaded dispatch
- \(h\): u2 stack-top cache hit rate
- \(R\): u2 operand cost on cache hit
- \(S\): u2 amortized operand cost on cache miss / spill / refill / canonicalization
- \(M\): wasm3 operand cost under its meta-machine model
- \(C_I\): front-end penalty from instruction footprint and I-cache / BTB pressure
- \(C_B\): control-flow boundary penalty
- \(C_L\): local-heavy operand-provider penalty
- \(C_X\): other engine-specific overheads (traps, runtime services, memory path complexity)

We model runtime as:

\[
T_{m3}
=
\frac{N}{\phi_{m3}}D
+
N M
+
C_{B}^{m3}
+
C_{I}^{m3}
+
C_{X}^{m3}
\]

\[
T_{u2}
=
\frac{N}{\phi_{u2}}D
+
N \bigl(hR + (1-h)S\bigr)
+
C_{B}^{u2}
+
C_{I}^{u2}
+
C_{L}^{u2}
+
C_{X}^{u2}
\]

This decomposition is useful because it isolates the exact channels through which u2’s advantage can disappear.

---

## 4. Dispatch-Level Equivalence: Why This Is *Not* a “switch vs non-switch” Story

Both interpreters are already beyond the classic `switch(opcode)` interpreter model:

- u2 executes a **direct-threaded pointer stream** emitted by the translator. :contentReference[oaicite:5]{index=5}
- wasm3 executes **tightly chained operations**, again without an outer dispatch loop. :contentReference[oaicite:6]{index=6}

Therefore, at the first order, both systems share the same class of dispatch skeleton:

\[
\text{dispatch skeleton} \approx \text{load next target} + \text{indirect branch / tail-call}
\]

This means the dominant difference is usually **not** the presence or absence of threading. Instead, the decisive terms are:

1. **operand traffic**
2. **fusion density**
3. **front-end pressure**
4. **control-flow canonicalization**
5. **operand-provider structure (e.g., locals)**

This directly matches u2’s own conclusion that once fusion density gets low, different threaded interpreters tend to converge toward the same dispatch ceiling. :contentReference[oaicite:7]{index=7}

---

## 5. Operand-Traffic Advantage: Why u2 Peaks in Flat Scientific Kernels

### 5.1 wasm3’s fixed-register meta-machine

wasm3 uses a fixed meta-machine signature:

\[
(pc, sp, mem, r0, fp0)
\]

and explicitly shows an example `op_u64_Or_sr` in which a stack operand is loaded through an offset, then combined with `r0`, then dispatched onward. Its own documentation warns that adding more CPU registers increases operation-space complexity sharply: with one register, up to 3 operations per opcode; with one more register, this grows to 10. :contentReference[oaicite:8]{index=8}

### 5.2 u2’s register-ring stack-top cache

u2 instead caches multiple stack-top values in a **register-ring**, using ABI-scalable argument-passed slots. Its documentation says it benefits especially on ABIs such as **x86_64 SysV** and **AArch64 AAPCS64**, where more stack-top values can remain in registers/locals and common stack operations become mostly **register-register ALU** operations. u2’s `register_ring.h` explicitly contrasts this with an M3-style path that often requires an extra dependent memory load for the stack operand. :contentReference[oaicite:9]{index=9}

### 5.3 Threshold theorem

u2 is faster than wasm3 when:

\[
\frac{N}{\phi_{u2}}D + N(hR + (1-h)S) + \Delta_u
<
\frac{N}{\phi_{m3}}D + NM + \Delta_m
\]

where \(\Delta_u = C_{B}^{u2}+C_{I}^{u2}+C_{L}^{u2}+C_{X}^{u2}\) and \(\Delta_m = C_{B}^{m3}+C_{I}^{m3}+C_{X}^{m3}\).

Rearranging:

\[
D\left(\frac{1}{\phi_{u2}} - \frac{1}{\phi_{m3}}\right)
+
hR + (1-h)S - M
+
\frac{\Delta_u - \Delta_m}{N}
< 0
\]

This is the central inequality of the comparison.

In flat scientific kernels:

- \(\phi_{u2} > \phi_{m3}\)
- \(h \to 1\)
- \(\Delta_u - \Delta_m \to 0\) or remains small

Hence the inequality strongly favors u2.

---

## 6. Why u2’s Specialization Does *Not* Blow Up Quadratically as Fast as Naive Multi-Register Designs

A major technical virtue of u2 is that it avoids the full \(O(N^2)\) explosion expected from naive two-operand specialization. Its own `register_ring.h` states that for 2-operand ops, specialization growth remains approximately **\(O(N)\)** rather than **\(O(N^2)\)**, because stack-machine two-operand patterns naturally consume adjacent positions such as TOS/NOS, and the ring structure exploits that adjacency. :contentReference[oaicite:10]{index=10}

### Proposition 1

Let the stack-top cache contain \(N\) logical slots.

- A naive two-operand register-specialized interpreter needs to consider ordered pairs \((i,j)\), giving:

\[
N(N-1)=O(N^2)
\]

- A ring-constrained adjacent-consumer model requires only the start position \(i\), with the second operand fixed by adjacency:

\[
N = O(N)
\]

Thus u2’s ring design is mathematically significant: it increases usable register-resident state while controlling specialization complexity more effectively than a naive expansion of wasm3’s fixed-register design.

This is an actual architectural insight, not merely a coding trick. It explains why u2 can push farther into registerized stack execution than wasm3 without immediately exploding into an intractable operation space. :contentReference[oaicite:11]{index=11}

---

## 7. Why the Peak Collapses in Branch-Heavy, Multi-Block, Multi-Loop Workloads

This is the most important part of the user’s practical observation.

### 7.1 u2’s control-flow cost

u2 documents that **control-flow joins** such as loops and `if/else` merges may require a **canonical cache layout**, and that a control-flow boundary may require a **consistent memory stack state**. Its `register_ring.h` lists spilling the cached stack-top segment back to memory when:

- generic memory operations require exposure of the operand stack
- the cached segment is about to be overwritten
- **a control-flow boundary requires a consistent memory stack state** :contentReference[oaicite:12]{index=12}

Hence we can write:

\[
C_{B}^{u2}
\approx
K_{\text{join}} \cdot c_{\text{canon}}
+
K_{\text{spill}} \cdot c_{\text{spill/fill}}
\]

where:

- \(K_{\text{join}}\) = number of hot control-flow joins
- \(K_{\text{spill}}\) = number of hot cache materialization events

The more branches, blocks, and loops a kernel has, the more often u2 must **materialize, rotate, or canonicalize** ring state. That directly erodes the benefit of keeping stack-top values out of memory.

### 7.2 wasm3’s branch/loop cost

wasm3 is not free here either. Its interpreter document states that roughly **90% of opcodes** do not require stack variables, but **branching and call operations do**, and that **loops unwind the stack**: a Continue operation returns, unwinding to the loop operation, which then resumes or propagates further. :contentReference[oaicite:13]{index=13}

Thus:

\[
C_{B}^{m3}
\neq 0
\]

However, the qualitative difference is that branch-heavy code destroys **u2’s strongest differentiator**, namely the ability to keep a large hot stack segment resident in the ring across long arithmetic streaks. In wasm3, the fixed-register-file approach has less headroom, but also less state to preserve across such joins.

### Corollary

If branch frequency rises, then:

\[
\frac{\partial T_{u2}}{\partial K_{\text{join}}}
>
\frac{\partial T_{m3}}{\partial K_{\text{join}}}
\]

in the regime where u2’s performance depends heavily on long uninterrupted ring-resident arithmetic streaks.

This is precisely why branch-rich kernels can compress u2’s advantage from something near 2× to something far smaller.

---

## 8. Why Local-Heavy Code Compresses the Gap

The user’s observation that **local-heavy kernels do not preserve u2’s peak advantage** is also structurally sound.

u2’s document explains that a large fraction of real workloads are built from short provider/consumer patterns such as `local.get` or constants feeding an arithmetic consumer. Its `delay_local` strategy therefore treats `local.get` as a **virtual operand source** and fuses it into the consumer rather than executing it as an independent stack-producing step. :contentReference[oaicite:14]{index=14}

wasm3 does something analogous at a more compact level: it maps opcodes to source-operand variants and fused operations rather than insisting on a naive standalone provider step every time. Its documentation explicitly says opcodes map to up to 3 operations depending on source operands and commutativity, and that common sequences can be fused. :contentReference[oaicite:15]{index=15}

### The key consequence

When a workload is dominated by **locals as operand providers**, both interpreters move toward the same abstract optimization regime:

- do **not** execute providers as standalone stack traffic if avoidable
- move the provider into the consumer form

As a result, u2’s unique edge from a large register-resident stack-top cache is partially replaced by a more generic provider/consumer optimization that both designs can exploit.

Formally, define \(q\) as the fraction of dynamic operand supply events coming from locals rather than ring-resident continuation.

Then the “u2-exclusive” operand advantage shrinks as \(q\) rises:

\[
\text{u2-exclusive gain} \sim (1-q)\cdot G_{\text{ring}}
\]

So in local-heavy workloads, even if both engines are good, **they become good for increasingly similar reasons**, and the relative gap narrows.

---

## 9. Code Size, I-Cache, and Why Front-End Pressure Can Erase a Large Share of u2’s Theoretical Gain

u2 explicitly acknowledges that it **trades code size** for hot-path specialization and does **not** attempt to minimize code size. It also publishes an example in which enabling `delay_local` increases Mach-O `__TEXT` from roughly **1,409,024** to **1,441,792** bytes, with extra-heavy options increasing it further. wasm3, by contrast, publicly emphasizes an approximately **64 KB code** minimum-useful footprint. :contentReference[oaicite:16]{index=16}

The user also reports a practical stripped pure-interpreter comparison on the order of **u2 ≈ 800 KB vs wasm3 ≈ 64 KB**. Even if these are not identical measurement conventions, they point in the same qualitative direction: **u2’s interpreter text is much larger**.

### 9.1 A simple front-end model

Let \(W\) be the hot opfunc working-set size and \(I\) the effective front-end capacity (L1I + BTB locality envelope, loosely speaking). Then:

- if \(W \ll I\), front-end penalties are minor
- if \(W \gtrsim I\), I-cache misses and branch-target disruption become significant

We can write, qualitatively:

\[
C_I \approx f\!\left(\frac{W}{I}\right), \quad f' > 0
\]

u2’s specialization helps only when the additional hot variants remain in a small working set. In flat arithmetic kernels this is often true. In heterogeneous kernels with many locals, branches, memory accesses, traps, and control variants, the hot target set grows. Then u2 pays:

\[
C_{I}^{u2} \gg C_{I}^{m3}
\]

even while its back-end arithmetic path remains individually stronger.

This is exactly why a design can be **more optimal locally** and yet **less dominant globally**.

---

## 10. Why the Gap Can Realistically Shrink from ~2× to ~10%

Suppose in an ideal flat arithmetic kernel:

\[
T_{m3} = 1.00
\]

\[
T_{u2}^{\text{ideal}} = 0.50
\]

which corresponds to about **2×** speedup.

Now include degradations:

\[
T_{u2}^{\text{real}}
=
0.50
+
\Delta_{\phi}
+
\Delta_{B}
+
\Delta_{L}
+
\Delta_{I}
\]

where:

- \(\Delta_{\phi}\): fusion-density loss
- \(\Delta_{B}\): control-flow canonicalization and spill/fill loss
- \(\Delta_{L}\): local-heavy convergence loss
- \(\Delta_{I}\): instruction-footprint/front-end loss

If:

\[
\Delta_{\phi}
+
\Delta_{B}
+
\Delta_{L}
+
\Delta_{I}
\approx 0.40
\]

then:

\[
T_{u2}^{\text{real}} \approx 0.90
\]

which corresponds to only about **11%** advantage over wasm3.

This is not a speculative story. It is the natural quantitative outcome of the public architecture:

- u2 itself says low fusion density narrows differences among threaded interpreters. :contentReference[oaicite:17]{index=17}
- u2 itself says branch-heavy and memory-heavy kernels increase dependence chains and register pressure. :contentReference[oaicite:18]{index=18}
- u2 itself documents control-flow canonicalization and spill/materialization at boundaries. :contentReference[oaicite:19]{index=19}
- both systems support source-operand fusion, reducing naive standalone-provider overhead. :contentReference[oaicite:20]{index=20}
- u2 explicitly accepts larger code size as part of the design trade. :contentReference[oaicite:21]{index=21}

---

## 11. Comparative Table

| Dimension | u2 | wasm3 | Mathematical consequence |
|---|---|---|---|
| Dispatch | Direct-threaded pointer stream | Tightly chained operation stream | Same dispatch class; differences move elsewhere |
| Register model | ABI-scalable register-ring stack-top cache | Small fixed register file (`pc, sp, mem, r0, fp0`) | u2 has higher upside on long arithmetic streaks |
| Operand path | Often register-register on cache hit | Often register + stack operand load | u2 reduces dependent memory loads when hit rate is high |
| Fusion | Adjacent fusion + heavy/extra-heavy + `delay_local` | Operation variants + fused operations | u2 can raise \(\phi\) further, but at larger cost |
| Control-flow behavior | Canonicalization / spill / fill / transform at joins | Branch/call require stack variables; loops unwind native stack | branch-rich kernels narrow the gap |
| Code size | Intentionally large; specialization-heavy | Compact footprint target | larger \(C_I\) for u2 on broad working sets |
| Best case | Flat scientific kernels, low branch rate, low local disturbance | Stable compact baseline | u2 can approach peak advantage |
| Worst case | many branches, many blocks/loops, large working set, local-heavy or memory-heavy | lower upside but more stable | u2’s advantage may collapse to ~10% |

All architectural entries in this table are grounded in the public documentation of both engines. :contentReference[oaicite:22]{index=22}

---

## 12. Formal Conclusion

### Theorem-like summary

Let

\[
\Delta
=
T_{m3}-T_{u2}
\]

Then:

\[
\Delta
=
D N\left(\frac{1}{\phi_{m3}}-\frac{1}{\phi_{u2}}\right)
+
N\left(M-hR-(1-h)S\right)
+
(\Delta_m-\Delta_u)
\]

where \(\Delta_m-\Delta_u\) bundles the net effect of front-end pressure, control flow, locals, and engine-specific runtime paths.

Then:

1. **If**
   - \(\phi_{u2} \gg \phi_{m3}\),
   - \(h \approx 1\),
   - branch/join frequency is low,
   - local-provider frequency is low,
   - and the hot instruction working set fits comfortably in the front-end,
   
   **then \( \Delta > 0 \) by a large margin**, and u2 may approach a near-2× regime.

2. **If**
   - \(\phi_{u2} \to \phi_{m3}\),
   - \(h\) falls because ring continuity is repeatedly broken,
   - control-flow joins frequently force canonicalization,
   - local-provider dominance makes both engines converge toward similar provider/consumer optimization,
   - and u2’s larger text footprint increases I-cache/BTB pressure,
   
   **then \( \Delta > 0 \) may remain only slightly positive**, or may even vanish on some kernels.

### Final interpretation

The user’s observation is therefore mathematically and architecturally coherent:

- **u2 has a higher peak than wasm3**
- but **that peak is more conditional**
- whereas **wasm3 has a lower ceiling but a more stable efficiency profile**

This is the most precise way to state the comparison without overstating what the public evidence can prove. u2 itself does not claim universality, and the architecture documents of both projects strongly support the conditional-performance interpretation developed above. :contentReference[oaicite:23]{index=23}

---

## 13. Practical Research Hypothesis

If one wanted to turn this note into an actual empirical paper, the correct measurable predictors would be:

\[
(\phi,\; h,\; K_{\text{join}},\; q,\; W/I)
\]

where:

- \(\phi\): fusion density
- \(h\): ring cache hit rate
- \(K_{\text{join}}\): control-flow join frequency
- \(q\): local-provider fraction
- \(W/I\): hot opfunc working set normalized by front-end capacity

The central hypothesis would then be:

\[
\text{Speedup}(u2/m3)
=
F(\phi,\; h,\; K_{\text{join}},\; q,\; W/I)
\]

with positive partial derivatives in \(\phi\) and \(h\), and negative partial derivatives in \(K_{\text{join}}, q,\) and \(W/I\).

That is the cleanest paper-level mathematical framing of the architecture gap.