#!/usr/bin/env python3
"""Pure Python graph/oracle checks only; does not compile or execute any managed/VM code."""
import hashlib
import json
from pathlib import Path
from generate_general_gc import FAMILIES, PHASES, ROOTS, WIDTH, MASK, SEED, SALT, oracle

class Node:
    def __init__(self, value, next_node=None):
        self.value = value
        self.next = next_node

def graph_model(family, phase, count, break_restore=False):
    roots = [None] * ROOTS
    node_roots = [None] * ROOTS
    state = SEED
    total = 0
    allocated = 0
    def advance(value):
        return (value * 1664525 + 1013904223) & MASK
    def create(slot, value):
        nonlocal allocated
        if family == "mutable-struct":
            roots[slot] = Node(value); allocated += 1
        elif family == "numeric-array":
            roots[slot] = [value] * WIDTH; allocated += 1
        else:
            a = Node(value)
            b = Node((value * 3 + 17) & MASK, a)
            a.next = b
            allocated += 2
            if family == "reference-array":
                roots[slot] = [a] * WIDTH
                node_roots[slot] = a
                allocated += 1
            else:
                roots[slot] = a
    def load(slot):
        if family == "reference-array":
            cells = roots[slot]; a = node_roots[slot]; b = a.next
            return a, b, cells
        if family == "reference-cycle":
            a = roots[slot]; return a, a.next, None
        return roots[slot], None, roots[slot] if family == "numeric-array" else None
    def cycle(a, b):
        if b is None or b.next is not a:
            raise AssertionError("broken B.next=A")
    def retained(slot):
        a, b, cells = load(slot)
        if family == "mutable-struct": value = a.value
        elif family == "reference-cycle": value = a.value + b.value; cycle(a,b)
        elif family == "numeric-array":
            assert len(cells) == WIDTH; value = sum(cells)
        else:
            assert len(cells) == WIDTH
            value = sum(cell.value for cell in cells); cycle(a,b)
        return value & MASK
    if phase == "mutate":
        for slot in range(ROOTS):
            state = advance(state); create(slot, state)
    for i in range(count):
        state = advance(state); slot = i & (ROOTS-1)
        if phase == "allocate":
            if i >= ROOTS: total = (total + retained(slot)) & MASK
            create(slot, state)
        a,b,cells = load(slot)
        value = (state+i) & MASK
        if family == "mutable-struct":
            a.value = value; observed = a.value
        elif family == "reference-cycle":
            a.value = value; a.next = a
            observed = a.value + a.next.value
            if not break_restore: a.next = b
            observed += a.next.value
            cycle(a,b)
        elif family == "numeric-array":
            selected = i & (WIDTH-1); following=(selected+1)&(WIDTH-1)
            cells[selected]=state ^ SALT; cells[following]=value
            observed=cells[selected]+cells[following]+len(cells)
        else:
            a.value=value
            selected=i&(WIDTH-1); following=(selected+1)&(WIDTH-1)
            cells[selected]=b; cells[following]=a
            observed=cells[selected].value+cells[following].value+len(cells)
            cycle(a,b)
        total=(total+observed)&MASK
    rootsum=sum(retained(slot) for slot in range(ROOTS)) & MASK
    return {"step_checksum_u32":total, "root_checksum_u32":rootsum,
            "return_checksum_u32":total ^ rootsum, "last_lcg_u32":state,
            "guest_planned_allocations":allocated}

def main():
    checks = 0
    for n in (1024, 1025, 4096, 65536):
        for family in FAMILIES:
            for phase in PHASES:
                expected=oracle(family,phase,n); actual=graph_model(family,phase,n)
                for key,value in actual.items():
                    assert value == expected[key], (family,phase,n,key,value,expected[key])
                    checks += 1
    # The broken restore eventually corrupts the actual root graph; the
    # primitive oracle and restored source loop must not accept its checksum.
    failures=0
    for phase in PHASES:
        rejected=False
        try:
            bad=graph_model("reference-cycle",phase,1025,True)
        except AssertionError:
            rejected=True
        if not rejected:
            expected=oracle("reference-cycle",phase,1025)
            assert any(value != expected[key] for key,value in bad.items()), "broken restore accepted"
        failures += 1
    source=Path(__file__).parent
    paths=["managed_general/java/GeneralGc.java","managed_general/dotnet/Program.cs",
           "managed_general/dotnet/GeneralGc.csproj","generate_general_gc.py"]
    pins={p:{"bytes":(source/p).stat().st_size,
             "sha256":hashlib.sha256((source/p).read_bytes()).hexdigest()} for p in paths}
    print(json.dumps({"scope":"pure graph/scalar math; not Java/CSharp syntax, native, VM or performance PASS",
                      "positive_scalar_checks":checks, "broken_restore_controls":failures,
                      "source_pins":pins}))
if __name__=="__main__": main()
