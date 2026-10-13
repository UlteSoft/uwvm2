using System.Diagnostics;
using System.Runtime;

static class ManagedRing
{
    private const uint Seed = 123456789u;
    private const uint A = 1664525u;
    private const uint C = 1013904223u;
    private static readonly object[] Roots = new object[1024];

    private sealed class Box(uint value)
    {
        internal readonly uint Value = value;
    }

    private static uint Run(int count)
    {
        uint state = Seed;
        for (int remaining = count; remaining != 0; --remaining)
        {
            state = unchecked(state * A + C);
            int index = remaining & 1023;
            Roots[index] = new Box(state);
            state = ((Box)Roots[index]).Value;
        }
        return state;
    }

    private static uint Expected(uint count)
    {
        uint multiplier = 1, increment = 0, powerMultiplier = A, powerIncrement = C;
        while (count != 0)
        {
            if ((count & 1) != 0)
            {
                multiplier = unchecked(multiplier * powerMultiplier);
                increment = unchecked(increment * powerMultiplier + powerIncrement);
            }
            powerIncrement = unchecked((powerMultiplier + 1) * powerIncrement);
            powerMultiplier = unchecked(powerMultiplier * powerMultiplier);
            count >>= 1;
        }
        return unchecked(multiplier * Seed + increment);
    }

    private static uint VerifyRoots(int count)
    {
        if (count < Roots.Length) throw new ArgumentException("need at least 1024 measured steps");
        uint state = Expected((uint)(count - Roots.Length));
        uint expectedXor = 0;
        for (int i = 0; i < Roots.Length; ++i)
        {
            state = unchecked(state * A + C);
            int index = (Roots.Length - i) & (Roots.Length - 1);
            if (Roots[index] is not Box box || box.Value != state)
                throw new Exception($"root ring slot mismatch at {index}");
            expectedXor ^= state;
        }
        uint observedXor = 0;
        foreach (object root in Roots)
        {
            if (root is not Box box) throw new Exception("missing live root");
            observedXor ^= box.Value;
        }
        if (observedXor != expectedXor) throw new Exception("root ring mismatch");
        return observedXor;
    }

    public static void Main(string[] args)
    {
        bool gcTelemetry = args.Length == 2 && args[1] == "--gc-telemetry";
        if ((args.Length != 1 && !gcTelemetry) ||
            !int.TryParse(args[0], out int count) || count <= 0)
            throw new ArgumentException("expected one positive iteration count and optional --gc-telemetry");
        for (int i = 0; i < 8; ++i)
            if (Run(250000) != Expected(250000)) throw new Exception("warmup mismatch");
        int[] collectionsBefore = gcTelemetry ? [GC.CollectionCount(0), GC.CollectionCount(1), GC.CollectionCount(2)] : [];
        long pauseBeforeTicks = gcTelemetry ? GC.GetTotalPauseDuration().Ticks : 0;
        long allocatedBefore = gcTelemetry ? GC.GetTotalAllocatedBytes(false) : 0;
        long started = Stopwatch.GetTimestamp();
        uint actual = Run(count);
        double elapsed = Stopwatch.GetElapsedTime(started).TotalNanoseconds;
        int[] collectionsAfter = gcTelemetry ? [GC.CollectionCount(0), GC.CollectionCount(1), GC.CollectionCount(2)] : [];
        long pauseAfterTicks = gcTelemetry ? GC.GetTotalPauseDuration().Ticks : 0;
        long allocatedAfter = gcTelemetry ? GC.GetTotalAllocatedBytes(false) : 0;
        if (actual != Expected((uint)count)) throw new Exception("result mismatch");
        uint rootChecksum = VerifyRoots(count);
        bool serverGc = GCSettings.IsServerGC;
        long gcHeapBudgetBytes = GC.GetGCMemoryInfo().TotalAvailableMemoryBytes;
        long configuredHeapLimitBytes = -1;
        foreach (var setting in GC.GetConfigurationVariables())
            if (setting.Key.Equals("GCHeapHardLimit", StringComparison.OrdinalIgnoreCase))
                configuredHeapLimitBytes = Convert.ToInt64(setting.Value);
        string gcFields = gcTelemetry ?
            $",\"gc_gen0_count\":{collectionsAfter[0] - collectionsBefore[0]}" +
            $",\"gc_gen1_count\":{collectionsAfter[1] - collectionsBefore[1]}" +
            $",\"gc_gen2_count\":{collectionsAfter[2] - collectionsBefore[2]}" +
            $",\"gc_total_pause_ns\":{(pauseAfterTicks - pauseBeforeTicks) * 100L}" +
            $",\"gc_allocated_bytes\":{allocatedAfter - allocatedBefore}" : "";
        Console.WriteLine($"{{\"runtime\":\"dotnet\",\"iterations\":{count},\"guest_ns\":{elapsed:F0},\"checksum\":{actual},\"root_checksum\":{rootChecksum},\"server_gc\":{serverGc.ToString().ToLowerInvariant()},\"gc_heap_limit_config_bytes\":{configuredHeapLimitBytes},\"gc_heap_budget_bytes\":{gcHeapBudgetBytes}{gcFields}}}");
    }
}
