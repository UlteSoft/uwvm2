package main

import (
    "fmt"
    "io"
    "os"
    "strconv"
    "time"
)

// The same source is offered independently to Go and TinyGo.
//go:noinline
func kernel(iterations uint32) uint32 {
    value := uint32(7)
    for index := uint32(0); index < iterations; index++ {
        value = (value*33 + index) ^ 0x55aa
    }
    return value
}

func fail(message string, status int) {
    fmt.Fprintln(os.Stderr, "lcg32 "+message)
    os.Exit(status)
}

func parseIterations() uint32 {
    if len(os.Args) == 1 { return 2000000000 }
    if len(os.Args) != 2 || len(os.Args[1]) == 0 || len(os.Args[1]) > 10 {
        fail("invalid iterations", 2)
    }
    for _, digit := range []byte(os.Args[1]) {
        if digit < '0' || digit > '9' { fail("invalid iterations", 2) }
    }
    value, err := strconv.ParseUint(os.Args[1], 10, 32)
    if err != nil || value == 0 || value > 2000000000 { fail("invalid iterations", 2) }
    return uint32(value)
}

func marker(text string) {
    if _, err := io.WriteString(os.Stderr, text); err != nil { fail("marker output failed", 4) }
}

func gate(wanted byte) {
    var value [1]byte
    if _, err := io.ReadFull(os.Stdin, value[:]); err != nil || value[0] != wanted {
        fail("invalid gate", 3)
    }
}

func main() {
    iterations := parseIterations()
    marker("UWVM_PROFILE_READY\n")
    gate('G')
    start := time.Now()
    checksum := kernel(iterations)
    marker("UWVM_PROFILE_DONE\n")
    elapsed := time.Since(start).Nanoseconds()
    gate('D')
    fmt.Printf("lcg32 iterations=%d checksum=%d\n", iterations, checksum)
    fmt.Printf("lcg32 kernel ns=%d\n", elapsed)
}
