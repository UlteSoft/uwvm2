package main
var Observed int32
//go:noinline
func worker(id int32, done chan int32) {
    value := id + 10
    Observed = value // WORKER_STOP
    done <- value
}
func main() {
    done := make(chan int32,2)
    go worker(1,done)
    go worker(2,done)
    a := <-done
    b := <-done
    if a+b != 23 { panic("bad workers") }
}
