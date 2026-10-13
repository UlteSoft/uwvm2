package main

type Holder struct {
    Text string
    Numbers []int32
}
var Kept = Holder{Text: "uwvm-tinygo", Numbers: []int32{21,22,23}}
var Observed int32

//go:noinline
func inspect(box *Holder) int32 {
    result := int32(len(box.Text)) + box.Numbers[1]
    Observed = result
    return result
}
func main() {
    Observed = inspect(&Kept)
    if Observed != 33 { panic("sequence result") }
}
