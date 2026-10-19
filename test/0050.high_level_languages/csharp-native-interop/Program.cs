using System;
using System.Globalization;
using System.Runtime.InteropServices;
using System.Text;

internal static class Program
{
    private const ulong Token = 0xFEDCBA9876543210UL;
    private const byte Canary = 0xA5;
    private const int BufferSize = 64;
    private const int Guard = 8;

    [DllImport("hll_native_bridge", EntryPoint = "hll_bridge_copy",
        ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    private static extern ulong Copy(int memoryIndex, ulong inputOffset, ulong inputLength,
        ulong outputOffset, ulong capacity);

    [DllImport("hll_native_bridge", EntryPoint = "hll_roundtrip_u64",
        ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    private static extern ulong Roundtrip(ulong value);

    private static void Require(bool value, string message)
    {
        if (!value) throw new InvalidOperationException(message);
    }

    // wasm32 IntPtr.ToInt32 may be signed: explicitly zero-extend all 32 bits.
    private static ulong Address(IntPtr pointer) => IntPtr.Size == 4
        ? unchecked((ulong)(uint)pointer.ToInt32()) : unchecked((ulong)pointer.ToInt64());

    private static byte[] Read(IntPtr pointer)
    {
        byte[] bytes = new byte[BufferSize];
        Marshal.Copy(pointer, bytes, 0, bytes.Length);
        return bytes;
    }

    private static void Reset(IntPtr pointer)
    {
        byte[] bytes = new byte[BufferSize];
        Array.Fill(bytes, Canary);
        Marshal.Copy(bytes, 0, pointer, bytes.Length);
    }

    private static void Reject(IntPtr output, Func<ulong> action, string label)
    {
        Reset(output);
        Require(action() == 0, label + " returned a token");
        foreach (byte value in Read(output)) Require(value == Canary, label + " changed output");
    }

    private static int Main()
    {
        IntPtr input = IntPtr.Zero, output = IntPtr.Zero;
        try
        {
            byte[] payload = Encoding.UTF8.GetBytes("hll\0π中🙂");
            byte[] literal = { 0x68, 0x6C, 0x6C, 0x00, 0xCF, 0x80, 0xE4, 0xB8, 0xAD, 0xF0, 0x9F, 0x99, 0x82 };
            Require(payload.Length == literal.Length, "UTF8 length");
            for (int i = 0; i < literal.Length; ++i) Require(payload[i] == literal[i], "UTF8 literal");
            input = Marshal.AllocHGlobal(BufferSize);
            output = Marshal.AllocHGlobal(BufferSize);
            Reset(input); Reset(output);
            Marshal.Copy(payload, 0, IntPtr.Add(input, Guard), payload.Length);
            ulong inputOffset = Address(IntPtr.Add(input, Guard));
            ulong outputOffset = Address(IntPtr.Add(output, Guard));
            ulong length = (ulong)payload.Length, capacity = 32;
            ulong token = Copy(0, inputOffset, length, outputOffset, capacity);
            Require(token == Token, "high-bit token mismatch");
            byte[] copied = Read(output);
            for (int i = 0; i < copied.Length; ++i)
                Require(copied[i] == (i >= Guard && i < Guard + payload.Length ? payload[i - Guard] : Canary),
                    "binary output/guard mismatch");
            ulong[] values = { 0, 1, Token, 0x8000000000000000UL, ulong.MaxValue };
            foreach (ulong value in values) Require(Roundtrip(value) == value, "independent ulong roundtrip");
            Reject(output, () => Copy(0, inputOffset, length, outputOffset, length - 1), "cap-short");
            Reject(output, () => Copy(1, inputOffset, length, outputOffset, capacity), "memory-one");
            Reject(output, () => Copy(0, ulong.MaxValue - 4, length, outputOffset, capacity), "input-overflow");
            Reject(output, () => Copy(0, inputOffset, length, ulong.MaxValue - 4, capacity), "output-overflow");
            Reject(output, () => Copy(0, inputOffset, (1UL << 32) | length, outputOffset, capacity), "high-length");
            if (IntPtr.Size == 4)
            {
                // Conversion-only sentinel: never dereference this pointer.
                Require(Address(new IntPtr(unchecked((int)0x80000010U))) == 0x80000010UL,
                    "signed wasm32 pointer was not zero-extended");
                const ulong high = 1UL << 32;
                Reject(output, () => Copy(0, high | inputOffset, length, outputOffset, capacity), "high-input-offset");
                Reject(output, () => Copy(0, inputOffset, length, high | outputOffset, capacity), "high-output-offset");
                Reject(output, () => Copy(0, inputOffset, length, outputOffset, high | capacity), "high-capacity");
            }
            byte[] unchangedInput = Read(input);
            for (int i = 0; i < unchangedInput.Length; ++i)
                Require(unchangedInput[i] == (i >= Guard && i < Guard + payload.Length ? payload[i - Guard] : Canary),
                    "input changed");
            Console.WriteLine("interop generic-c-abi memory=0 bytes=13");
            Console.WriteLine("interop token=0x" + token.ToString("X16", CultureInfo.InvariantCulture));
            Console.WriteLine("interop roundtrip cases=5 high=0xFEDCBA9876543210");
            Console.WriteLine("interop copy=utf8+nul exact=true input-unchanged=true");
            Console.WriteLine("interop rejected=cap-short,memory-one,input-overflow,output-overflow,high-length canary=true");
            Console.WriteLine("interop wasm32-high-arguments=" + (IntPtr.Size == 4 ? "checked" : "not-applicable"));
            return 0;
        }
        catch (Exception error)
        {
            Console.Error.WriteLine("interop failure: " + error);
            return 1;
        }
        finally
        {
            if (input != IntPtr.Zero) Marshal.FreeHGlobal(input);
            if (output != IntPtr.Zero) Marshal.FreeHGlobal(output);
        }
    }
}
