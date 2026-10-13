package main

type NamedArray [4]int32
type ArrayHolder struct {
    Array [4]int32
    Named NamedArray
    Matrix [2][3]int32
    Zero [0]int32
    ZeroSized [6]struct{}
    Pointer *[4]int32
    NilPointer *[4]int32
    ZeroPointer *[0]int32
    Message string
}
var Arrays = ArrayHolder{Array: [4]int32{1, 2, 3, 4}, Named: NamedArray{5, 6, 7, 8}, Matrix: [2][3]int32{{1, 2, 3}, {4, 5, 6}}, Message: "aλ🙂"}
var ArrayObserved int32

//go:noinline
func arrayProbe(box *ArrayHolder, nilBox *ArrayHolder, nilArray *[4]int32, arrayLen int, outerLen int, innerLen int, zeroLen int, zeroSizedLen int) int32 {
    ArrayObserved = int32(arrayLen) // GO_ARRAY_READY
    if box.Array[0] != 1 || nilBox != nil || nilArray != nil {
        panic("array fixture mismatch")
    }
    result := int32(arrayLen*10000 + outerLen*1000 + innerLen*100 + zeroLen*10 + zeroSizedLen)
    ArrayObserved = result
    return result
}
func main() {
    Arrays.Pointer = &Arrays.Array
    ArrayObserved = arrayProbe(&Arrays, nil, nil, len(Arrays.Array), cap(Arrays.Matrix), len(Arrays.Matrix[0]), len(Arrays.Zero), cap(Arrays.ZeroSized))
    if ArrayObserved != 42306 { panic("array compiler oracle mismatch") }
}
