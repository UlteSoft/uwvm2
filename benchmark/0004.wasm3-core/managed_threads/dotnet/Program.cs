using System.Diagnostics;
using System.Numerics;
using System.Threading;

// Cross-language analogue of wasm_thread_performance.cc, not the same Wasm code.
static class ManagedThreads
{
    const int Words = 1024;
    const int Passes = 32;
    static readonly uint[] Memory = new uint[4 * Words];

    static uint Kernel(int worker)
    {
        Thread.MemoryBarrier();
        int baseIndex = worker * Words;
        uint seed = 0x12345u + (uint)worker;
        for (int i = 0; i < Words; ++i) Memory[baseIndex + i] = (uint)i + seed;
        for (uint pass = 0; pass < Passes; ++pass)
            for (int i = 0; i < Words; ++i)
            {
                int address = baseIndex + i;
                Memory[address] = unchecked((BitOperations.RotateLeft(Memory[address], 7)
                                            ^ (seed + pass)) + 0x9e3779b9u);
            }
        uint checksum = 0;
        for (int i = 0; i < Words; ++i)
            checksum = unchecked(BitOperations.RotateLeft(checksum, 5) + Memory[baseIndex + i]);
        return checksum;
    }

    static ulong Measure(bool threaded, int workers, int rounds, int sample, uint[] expected)
    {
        long wallTicks = 0;
        ulong checksum = 0;
        for (int round = 0; round < rounds; ++round)
        {
            uint[] results = new uint[workers];
            Thread?[] threads = new Thread?[workers];
            long start = Stopwatch.GetTimestamp();
            for (int worker = 0; worker < workers; ++worker)
            {
                int index = worker;
                if (threaded)
                {
                    threads[worker] = new Thread(() => results[index] = Kernel(index));
                    threads[worker]!.Start();
                }
                else results[worker] = Kernel(worker);
            }
            if (threaded)
                foreach (Thread? thread in threads) thread!.Join();
            wallTicks += Stopwatch.GetTimestamp() - start;
            for (int worker = 0; worker < workers; ++worker)
            {
                if (results[worker] != expected[worker])
                    throw new Exception($"kernel checksum mismatch for worker {worker}");
                checksum += results[worker];
            }
        }
        if (sample >= 0)
        {
            double wallNs = (double)wallTicks * 1_000_000_000d / Stopwatch.Frequency / rounds;
            Console.WriteLine($"{{\"runtime\":\"dotnet-platform-thread\",\"workers\":{workers},"
                            + $"\"threaded\":{threaded.ToString().ToLowerInvariant()},\"sample\":{sample},"
                            + $"\"rounds\":{rounds},\"wall_ns\":{wallNs:F0},\"checksum\":{checksum},"
                            + $"\"requested_threads\":{(threaded ? workers * rounds : 0)}}}");
        }
        return checksum;
    }

    static void Main(string[] args)
    {
        if (args.Length != 3 || !int.TryParse(args[0], out int samples)
            || !int.TryParse(args[1], out int rounds)
            || !int.TryParse(args[2], out int offset)
            || samples < 1 || samples > 99 || rounds < 1 || rounds > 1024
            || offset < 0 || offset > 1)
            throw new ArgumentException("samples rounds order-offset required within bounds");
        uint[] expected = new uint[4];
        for (int worker = 0; worker < 4; ++worker) expected[worker] = Kernel(worker);
        for (int sample = -2; sample < samples; ++sample)
            foreach (int workers in new[] { 1, 4 })
            {
                bool firstThreaded = ((sample + offset) & 1) != 0;
                Measure(firstThreaded, workers, rounds, sample, expected);
                Measure(!firstThreaded, workers, rounds, sample, expected);
            }
    }
}
