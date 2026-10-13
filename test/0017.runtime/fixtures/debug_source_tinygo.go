package main

type Node struct { Value int32; Next *Node }
type Packet struct { Tag int32; Grid [2][3]int32 }
type Number int32
var Observed int32
var GlobalPacket = Packet{77, [2][3]int32{{1,2,3},{4,5,6}}}
var GlobalText = "uwvm-tinygo"
var GlobalSlice = []int32{21,22,23}

//go:noinline
func consume(p *Packet, n *Node, s []int32, t string) int32 {
    result := p.Grid[1][2]+n.Next.Value+s[1]+int32(len(t))
    Observed = result // LEAF_VALUES
    return result
}

//go:noinline
func probeOuter(value int32) int32 {
    packet := Packet{value,[2][3]int32{{10,11,12},{13,14,15}}}
    tail := Node{9,nil}
    node := Node{6,&tail}
    p := &node
    text := "tinygo"
    slice := []int32{4,5,6}
    shadow := int32(7)
    negative := int64(-1234567890123)
    flag := value == 3
    decimal := float64(1.25)
    alias := Number(29)
    Observed = shadow // VALUES
    child := consume(&packet,p,slice,text) // CALL_VALUES
    Observed = child // AFTER_VALUES
    { shadow := int32(23); Observed = shadow } // INNER_VALUES
    Observed = shadow // OUTER_VALUES
    return child + int32(negative%7) + int32(decimal) + int32(alias) + value + int32(len(text)) + slice[0] + packet.Tag + p.Value + boolToInt(flag)
}

//go:noinline
func boolToInt(value bool) int32 { if value { return 1 }; return 0 }
func main() { Observed = probeOuter(3) }
