package main

type NamedMap map[string]int32
type NamedChannel chan int32
type CollectionHolder struct {
    Numbers map[int32]int32
    Named NamedMap
    Empty map[int32]int32
    NilMap map[int32]int32
    Buffered chan int32
    NamedChan NamedChannel
    Closed chan int32
    EmptyChan chan int32
    NilChan chan int32
    Receive <-chan int32
    Send chan<- int32
}

var CollectionObserved int32

//go:noinline
func collectionProbe(box *CollectionHolder, mapLen int, namedLen int, bufferedLen int, bufferedCap int, closedLen int, closedCap int) int32 {
    CollectionObserved = int32(mapLen) // GO_COLLECTION_READY
    if box.Numbers[1] != 11 || box.Named["a"] != 31 || box.Empty == nil || box.NilMap != nil || box.NilChan != nil {
        panic("collection fixture invalid")
    }
    if len(box.Numbers) != mapLen || len(box.Named) != namedLen || len(box.Buffered) != bufferedLen || cap(box.Buffered) != bufferedCap || len(box.Closed) != closedLen || cap(box.Closed) != closedCap {
        panic("collection compiler oracle mismatch")
    }
    return int32(mapLen*100000 + namedLen*10000 + bufferedLen*1000 + bufferedCap*100 + closedLen*10 + closedCap)
}

func main() {
    buffered := make(chan int32, 4)
    buffered <- 17
    buffered <- 19
    closed := make(chan int32, 3)
    closed <- 23
    close(closed)
    named := make(NamedChannel, 5)
    named <- 29
    box := &CollectionHolder{
        Numbers: map[int32]int32{1:11, 2:13, 3:17},
        Named: NamedMap{"a":31, "b":37},
        Empty: make(map[int32]int32),
        Buffered: buffered,
        NamedChan: named,
        Closed: closed,
        EmptyChan: make(chan int32),
        Receive: buffered,
        Send: buffered,
    }
    result := collectionProbe(box, len(box.Numbers), len(box.Named), len(buffered), cap(buffered), len(closed), cap(closed))
    if result != 322413 || <-buffered != 17 || <-closed != 23 || <-closed != 0 {
        panic("collection fixture result mismatch")
    }
    CollectionObserved = result
}
