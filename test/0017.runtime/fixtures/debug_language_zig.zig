const Packet=extern struct{tag:i32,grid:[2][3]i32};
const Node=extern struct{value:i32,next:?*const Node};
var observed:i32=0;
export fn probe_leaf(value:i32)i32 {
    const leaf_cookie=value+11;
    const sink:*volatile i32=&observed;
    sink.*=leaf_cookie; // PROBE_LEAF_VALUES
    return leaf_cookie;
}
export fn probe_outer(value:i32)i32 {
    const packet=Packet{.tag=value,.grid=.{.{10,11,12},.{13,14,15}}};
    const tail=Node{.value=9,.next=null};
    const node=Node{.value=6,.next=&tail}; const p=&node;
    const shadow:i32=7; const text="uwvm";
    const sink:*volatile i32=&observed;
    sink.*=packet.grid[1][2]+p.next.?.value+text[0]; // PROBE_VALUES
    {
        const inner_shadow:i32=23;
        sink.*=inner_shadow; // PROBE_SHADOW
        const child=probe_leaf(5); // PROBE_CALL
        sink.*=child; // PROBE_AFTER
    }
    sink.*=shadow; // PROBE_OUTER
    return packet.grid[1][2]+shadow+20;
}
pub export fn _start() callconv(.c) void { if(probe_outer(3)!=42) @trap(); }
pub fn main()void { if(probe_outer(3)!=42) @trap(); }
