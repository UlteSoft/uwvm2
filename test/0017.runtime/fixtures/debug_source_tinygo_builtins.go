package main

type BuiltinHolder struct {
    Text string
    Numbers []int32
    Nil []int32
    Empty string
}

var Backing = [7]int32{10, 20, 30, 40, 50, 60, 70}
var Builtins = BuiltinHolder{Text: "aλ🙂", Numbers: Backing[1:4:6]}
var Observed int32

//go:noinline
func builtinProbe(box *BuiltinHolder, length int, capacity int, textBytes int, nilLength int, nilCapacity int) int32 {
    Observed = int32(length) // GO_BUILTIN_READY
    if box.Numbers[0] != 20 || len(box.Text) != textBytes {
        panic("Go builtin descriptor mismatch")
    }
    result := int32(length*10000 + capacity*100 + textBytes + nilLength + nilCapacity)
    Observed = result
    return result
}

func main() {
    Observed = builtinProbe(&Builtins, len(Builtins.Numbers), cap(Builtins.Numbers), len(Builtins.Text), len(Builtins.Nil), cap(Builtins.Nil))
    if Observed != 30507 {
        panic("Go builtin oracle mismatch")
    }
}
