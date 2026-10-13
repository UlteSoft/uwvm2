let observed:i32=0;
class Node { constructor(public value:i32,public next:Node|null) {} }
class Packet { tag:i32=3; grid:StaticArray<i32>=[10,11,12,13,14,15]; }
export function probe_leaf(value:i32):i32 {
 const leaf_cookie=value+11;
 observed=leaf_cookie; // PROBE_LEAF_VALUES
 return leaf_cookie;
}
export function probe_outer(value:i32):i32 {
 const packet=new Packet(); const tail=new Node(9,null); const node=new Node(6,tail);
 const p=node; const shadow:i32=7; const text="uwvm"; const slice=[4,5,6];
 observed=packet.grid[5]+p.next!.value+slice[1]; // PROBE_VALUES
 const child=probe_leaf(5); // PROBE_CALL
 observed=child; // PROBE_AFTER
 observed=shadow; // PROBE_OUTER
 return packet.grid[5]+shadow+20;
}
export function _start():void { if(probe_outer(3)!=42) unreachable(); }

_start(); // Execute the real workload from the compiler-generated module start.
