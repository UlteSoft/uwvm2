// This checked-error fixture is separate from the full Go panic/recover fixture.
package main

import (
    "errors"
    "fmt"
    "os"
    "runtime"
    "strconv"
    "unicode/utf8"
)

type accumulator interface { add(int) int }
type counter struct { value int }
func (c *counter) add(amount int) int { c.value += amount; return c.value }

func sum[T ~int](values []T) T {
    var total T
    for _, value := range values { total += value }
    return total
}

func checkedAt(values []int, index int) (int, error) {
    if index < 0 || index >= len(values) { return 0, errors.New("index out of range") }
    return values[index], nil
}

//go:noinline
func debugValue() int {
    value := 40
    value += 2 // DEBUG_BEFORE
    return value // DEBUG_AFTER
}

func kernel(iterations int) uint32 {
    value := uint32(7)
    for index := 0; index < iterations; index++ { value = (value*33 + uint32(index)) ^ 0x55aa }
    return value
}

func main() {
    iterations := 1000
    valid := len(os.Args) <= 2
    if len(os.Args) == 2 {
        var err error
        iterations, err = strconv.Atoi(os.Args[1])
        valid = err == nil
    }
    if !valid || iterations < 1 || iterations > 1000000 {
        fmt.Fprintln(os.Stderr, "tinygo invalid iterations")
        os.Exit(2)
    }
    var object accumulator = &counter{40}
    classes := object.add(2)
    numbers := []int{1, 2, 3, 4, 5, 6, 7, 8, 9, 10}
    arrays := sum(numbers)
    records := map[string]int{"left": 1, "right": 5}
    recordSum := records["left"] + records["right"]
    text := "π你好🙂"
    checks := 0
    if _, err := strconv.Atoi("bad"); err != nil { checks++ }
    if _, err := checkedAt(numbers, classes); err != nil { checks++ }
    channel := make(chan int)
    go func() { channel <- classes }()
    scheduled := <-channel
    blocks := make([][]byte, 16)
    for index := range blocks { blocks[index] = make([]byte, 1024); blocks[index][0] = byte(index) }
    runtime.GC()
    memory := 0
    for _, block := range blocks { memory += int(block[0]) }
    value := debugValue()
    if classes != 42 || arrays != 55 || recordSum != 6 || utf8.RuneCountInString(text) != 4 || len(text) != 12 || checks != 2 || scheduled != 42 || memory != 120 || value != 42 {
        panic("TinyGo semantics")
    }
    fmt.Printf("tinygo classes=%d arrays=%d records=%d unicode=4 utf8=12 checks=%d scheduled=%d memory=%d value=%d iterations=%d checksum=%d\n", classes, arrays, recordSum, checks, scheduled, memory, value, iterations, kernel(iterations))
}
