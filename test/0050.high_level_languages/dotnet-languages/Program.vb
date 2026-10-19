Option Strict On
Option Explicit On

Imports System
Imports System.Collections.Generic
Imports System.Text

Public MustInherit Class Accumulator
    Protected Value As Integer = 40
    Public MustOverride Function Add(amount As Integer) As Integer
End Class

Public NotInheritable Class Counter
    Inherits Accumulator
    Public Overrides Function Add(amount As Integer) As Integer
        Value += amount
        Return Value
    End Function
End Class

Public Module HighLevelVisualBasic
    Private Sub Require(condition As Boolean, message As String)
        If Not condition Then Throw New InvalidOperationException(message)
    End Sub

    Private Function Semantics() As Integer()
        Dim counter As Accumulator = New Counter()
        Dim classes As Integer = counter.Add(2)
        Dim numbers As Integer() = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}
        Dim arrays As Integer = 0
        For Each number As Integer In New List(Of Integer)(numbers)
            arrays += number
        Next
        Dim records As New Dictionary(Of String, Integer) From {{"left", 1}, {"right", 5}}
        Dim recordSum As Integer = records("left") + records("right")
        Dim text As String = "π你好🙂"
        Dim exceptions As Integer = 0
        Try
            Throw New InvalidOperationException("expected")
        Catch errorValue As InvalidOperationException When errorValue.Message = "expected"
            exceptions += 1
        End Try
        Try
            Dim ignored As Integer = numbers(classes)
        Catch errorValue As IndexOutOfRangeException
            exceptions += 1
        End Try
        Try
            Dim maximum As Integer = Integer.MaxValue
            maximum += classes - 41
        Catch errorValue As OverflowException
            exceptions += 1
        End Try
        Require(classes = 42 AndAlso arrays = 55 AndAlso recordSum = 6 AndAlso text.Length = 5 AndAlso Encoding.UTF8.GetByteCount(text) = 12 AndAlso exceptions = 3, "VB semantics")
        Return {classes, arrays, recordSum, text.Length, Encoding.UTF8.GetByteCount(text), exceptions}
    End Function

    Private Function Kernel(iterations As Integer) As UInteger
        Dim value As ULong = 7UL
        For index As Integer = 0 To iterations - 1
            value = ((value * 33UL + CULng(index)) Xor &H55AAUL) And &HFFFFFFFFUL
        Next
        Return CUInt(value)
    End Function

    Public Function Main(args As String()) As Integer
        Dim iterations As Integer = 1000
        If args.Length > 1 OrElse (args.Length = 1 AndAlso Not Integer.TryParse(args(0), iterations)) OrElse iterations < 1 OrElse iterations > 1000000 Then
            Console.Error.WriteLine("vb invalid iterations")
            Return 2
        End If
        Dim values As Integer() = Semantics()
        Console.WriteLine("vb classes=" & values(0) & " arrays=" & values(1) & " records=" & values(2) & " utf16=" & values(3) & " utf8=" & values(4) & " exceptions=" & values(5) & " iterations=" & iterations & " checksum=" & Kernel(iterations))
        Return 0
    End Function
End Module
