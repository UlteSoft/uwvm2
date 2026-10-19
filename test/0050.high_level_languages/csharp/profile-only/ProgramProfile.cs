using System;
using System.Diagnostics;
using System.Collections.Generic;
using System.Runtime.CompilerServices;

internal abstract class Accumulator
{
    protected int value;
    protected Accumulator(int initial) { value = initial; }
    public abstract int Add(int amount);
}

internal sealed class CheckedAccumulator : Accumulator
{
    public CheckedAccumulator(int initial) : base(initial) { }
    public override int Add(int amount) { return value = checked(value + amount); }
}

internal static class Program
{
    private static void Require(bool condition, string message)
    {
        if (!condition) throw new InvalidOperationException(message);
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static int CheckClasses()
    {
        Accumulator counter = new CheckedAccumulator(40);
        int result = counter.Add(2);
        Require(result == 42, "virtual class dispatch");
        return result;
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static int CheckArrays()
    {
        int[] numbers = new int[10];
        for (int i = 0; i < numbers.Length; ++i) numbers[i] = i + 1;
        var copy = new List<int>(numbers);
        int result = 0;
        foreach (int number in copy) result += number;
        Require(result == 55 && copy.Count == 10, "arrays and generic collection");
        return result;
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static int CheckStrings()
    {
        string text = "uwvm-世界";
        string rebuilt = string.Concat(text.Substring(0, 5), "世界");
        Require(text.Length == 7 && rebuilt == text && text[5] == '世', "UTF-16 strings");
        return text.Length;
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static int CheckExceptions()
    {
        int caught = 0;
        try { throw new InvalidOperationException("expected"); }
        catch (InvalidOperationException exception) { if (exception.Message == "expected") ++caught; }
        try { int[] values = new int[1]; values[CheckClasses()] = 1; }
        catch (IndexOutOfRangeException) { ++caught; }
        try { checked { int value = int.MaxValue; value += CheckStrings(); } }
        catch (OverflowException) { ++caught; }
        Require(caught == 3, "managed exception matching");
        return caught;
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static uint Kernel(int iterations)
    {
        uint value = 0x12345678;
        unchecked
        {
            for (int i = 0; i < iterations; ++i)
            {
                value ^= value << 13;
                value ^= value >> 17;
                value ^= value << 5;
                value += (uint)i;
            }
        }
        return value;
    }

    public static int Main(string[] args)
    {
        int iterations;
        try { iterations = args.Length == 0 ? 20000 : int.Parse(args[0]); }
        catch (FormatException) { Console.Error.WriteLine("csharp invalid iterations"); return 2; }
        catch (OverflowException) { Console.Error.WriteLine("csharp invalid iterations"); return 2; }
        if (iterations < 1 || iterations > 100000000)
        {
            Console.Error.WriteLine("csharp invalid iterations");
            return 2;
        }
        int classes = CheckClasses();
        int arrays = CheckArrays();
        int strings = CheckStrings();
        int exceptions = CheckExceptions();
        // Resolve the method and clock once before measuring the selected kernel.
        // Kernel always starts from its fixed seed, so this does not affect it.
        _ = Kernel(1);
        _ = Stopwatch.GetTimestamp();
        long kernelFrequency = Stopwatch.Frequency;
        using var control = Console.OpenStandardInput();
        Console.WriteLine("UWVM_PROFILE_READY");
        Console.Out.Flush();
        if (control.ReadByte() != (byte)'G')
        {
            Console.Error.WriteLine("csharp profile expected G");
            return 2;
        }
        long kernelStart = Stopwatch.GetTimestamp();
        uint checksum = Kernel(iterations);
        long kernelTicks = Stopwatch.GetTimestamp() - kernelStart;
        Console.WriteLine("UWVM_PROFILE_DONE");
        Console.Out.Flush();
        if (control.ReadByte() != (byte)'D')
        {
            Console.Error.WriteLine("csharp profile expected D");
            return 2;
        }
        Console.WriteLine("csharp classes=" + classes + " arrays=" + arrays + " strings=" + strings +
                          " exceptions=" + exceptions + " iterations=" + iterations + " checksum=" + checksum);
        Console.WriteLine("csharp kernel iterations=" + iterations + " ticks=" + kernelTicks + " frequency=" + kernelFrequency);
        return 0;
    }
}
