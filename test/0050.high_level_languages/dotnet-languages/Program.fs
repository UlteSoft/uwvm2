module HighLevelFSharp

open System
open System.Collections.Generic
open System.Text

[<AbstractClass>]
type Accumulator(initial: int) =
    let mutable value = initial
    member _.Value with get () = value and set next = value <- next
    abstract Add: int -> int

type Counter() =
    inherit Accumulator(40)
    override this.Add(amount) =
        this.Value <- this.Value + amount
        this.Value

type Reading = { Name: string; Value: int }
type Measurement = Valid of Reading | Missing

let require condition message =
    if not condition then invalidOp message

let semantics () =
    let counter = Counter() :> Accumulator
    let classes = counter.Add(2)
    let numbers = [| 1 .. 10 |]
    let arrays = List<int>(numbers) |> Seq.sum
    let readings = [ Valid { Name = "joint"; Value = 1 }; Missing; Valid { Name = "joint"; Value = 5 } ]
    let records = readings |> List.sumBy (function Valid r when r.Name = "joint" -> r.Value | _ -> 0)
    let text = "π你好🙂"
    let mutable exceptions = 0
    try invalidOp "expected"
    with :? InvalidOperationException as error when error.Message = "expected" -> exceptions <- exceptions + 1
    try
        let _ = numbers.[classes]
        ()
    with :? IndexOutOfRangeException -> exceptions <- exceptions + 1
    try
        let _ = Checked.(+) Int32.MaxValue (classes - 41)
        ()
    with :? OverflowException -> exceptions <- exceptions + 1
    require (classes = 42 && arrays = 55 && records = 6 && text.Length = 5 && Encoding.UTF8.GetByteCount(text) = 12 && exceptions = 3) "F# semantics"
    classes, arrays, records, text.Length, Encoding.UTF8.GetByteCount(text), exceptions

let kernel iterations =
    let mutable value = 7u
    for index in 0 .. iterations - 1 do
        value <- (value * 33u + uint32 index) ^^^ 0x55aau
    value

[<EntryPoint>]
let main args =
    let parsed = if args.Length = 0 then true, 1000 else Int32.TryParse(args.[0])
    match parsed with
    | true, iterations when args.Length <= 1 && iterations >= 1 && iterations <= 1000000 ->
        let classes, arrays, records, utf16, utf8, exceptions = semantics ()
        printfn "fsharp classes=%d arrays=%d records=%d utf16=%d utf8=%d exceptions=%d iterations=%d checksum=%u" classes arrays records utf16 utf8 exceptions iterations (kernel iterations)
        0
    | _ ->
        Console.Error.WriteLine("fsharp invalid iterations")
        2
