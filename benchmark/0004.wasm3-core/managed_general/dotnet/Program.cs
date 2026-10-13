using System.Diagnostics;
using System.Globalization;
using System.Runtime;
using System.Text.Json;

// Source-equivalent Core 3 GC workloads; never same-bytecode or same physical layout.
static class GeneralGc
{
    private const int RootCount = 1024;
    private const int Width = 8;
    private const uint Seed = 123456789u;
    private const uint Salt = 0xA5C31F27u;
    // Actual global publication keeps every final group observable after Run returns.
    private static readonly object?[] Roots = new object?[RootCount];
    private static Node?[]? NodeRoots;
    private static uint stepChecksum, rootChecksum, lastState;
    private sealed class Box(uint value) { internal uint Value = value; }
    private sealed class Node(uint value, Node? next)
    {
        internal uint Value = value;
        internal Node? Next = next;
    }
    private static uint Next(uint state) => unchecked(state * 1664525u + 1013904223u);
    private static Node Pair(uint state)
    {
        Node a = new(state, null);
        Node b = new(unchecked(state * 3u + 17u), a);
        a.Next = b;
        return a;
    }
    private static void Cycle(Node a, Node? b)
    {
        if (b is null || !ReferenceEquals(b.Next, a)) throw new Exception("broken B.Next=A");
    }
    private static uint Sum(uint[] cells)
    {
        if (cells.Length != Width) throw new Exception("numeric array length");
        uint total = 0;
        foreach (uint cell in cells) total = unchecked(total + cell);
        return total;
    }
    private static uint Sum(Node[] cells, Node a, Node b)
    {
        if (cells.Length != Width) throw new Exception("reference array length");
        uint total = 0;
        foreach (Node cell in cells)
        {
            if (cell is null) throw new Exception("unexpected null cell");
            total = unchecked(total + cell.Value);
        }
        Cycle(a, b);
        return total;
    }
    private static uint Finish(uint state, uint total, uint roots)
    {
        lastState = state; stepChecksum = total; rootChecksum = roots;
        return total ^ roots;
    }
    private static uint MutableStructAllocate(int count)
    {
        uint state = Seed, total = 0;
        for (int i = 0; i < count; ++i)
        {
            state = Next(state);
            int slot = i & (RootCount - 1);
            if (i >= RootCount)
            {
                Box olda = (Box)Roots[slot]!;
                total = unchecked(total + olda.Value);
            }
            Roots[slot] = new Box(state);
            Box a = (Box)Roots[slot]!;
            a.Value = unchecked(state + (uint)i);
            total = unchecked(total + a.Value);
        }
        uint roots = 0;
        for (int slot = 0; slot < RootCount; ++slot)
        {
            Box a = (Box)Roots[slot]!;
            roots = unchecked(roots + a.Value);
        }
        return Finish(state, total, roots);
    }
    private static uint MutableStructMutate(int count)
    {
        uint state = Seed, total = 0;
        for (int slot = 0; slot < RootCount; ++slot)
        {
            state = Next(state);
            Roots[slot] = new Box(state);
        }
        for (int i = 0; i < count; ++i)
        {
            state = Next(state);
            int slot = i & (RootCount - 1);
            Box a = (Box)Roots[slot]!;
            a.Value = unchecked(state + (uint)i);
            total = unchecked(total + a.Value);
        }
        uint roots = 0;
        for (int slot = 0; slot < RootCount; ++slot)
        {
            Box a = (Box)Roots[slot]!;
            roots = unchecked(roots + a.Value);
        }
        return Finish(state, total, roots);
    }
    private static uint ReferenceCycleAllocate(int count)
    {
        uint state = Seed, total = 0;
        for (int i = 0; i < count; ++i)
        {
            state = Next(state);
            int slot = i & (RootCount - 1);
            if (i >= RootCount)
            {
                Node olda = (Node)Roots[slot]!;
                Node oldb = olda.Next ?? throw new Exception("null A.Next");
                total = unchecked(total + unchecked(olda.Value + oldb.Value));
                Cycle(olda, oldb);
            }
            Roots[slot] = Pair(state);
            Node a = (Node)Roots[slot]!;
            Node b = a.Next ?? throw new Exception("null A.Next");
            a.Value = unchecked(state + (uint)i);
            a.Next = a;
            uint observed = unchecked(a.Value + (a.Next ?? throw new Exception("null self edge")).Value);
            a.Next = b;
            observed = unchecked(observed + (a.Next ?? throw new Exception("null restored edge")).Value);
            Cycle(a, b);
            total = unchecked(total + observed);
        }
        uint roots = 0;
        for (int slot = 0; slot < RootCount; ++slot)
        {
            Node a = (Node)Roots[slot]!;
            Node b = a.Next ?? throw new Exception("null A.Next");
            roots = unchecked(roots + unchecked(a.Value + b.Value));
            Cycle(a, b);
        }
        return Finish(state, total, roots);
    }
    private static uint ReferenceCycleMutate(int count)
    {
        uint state = Seed, total = 0;
        for (int slot = 0; slot < RootCount; ++slot)
        {
            state = Next(state);
            Roots[slot] = Pair(state);
        }
        for (int i = 0; i < count; ++i)
        {
            state = Next(state);
            int slot = i & (RootCount - 1);
            Node a = (Node)Roots[slot]!;
            Node b = a.Next ?? throw new Exception("null A.Next");
            a.Value = unchecked(state + (uint)i);
            a.Next = a;
            uint observed = unchecked(a.Value + (a.Next ?? throw new Exception("null self edge")).Value);
            a.Next = b;
            observed = unchecked(observed + (a.Next ?? throw new Exception("null restored edge")).Value);
            Cycle(a, b);
            total = unchecked(total + observed);
        }
        uint roots = 0;
        for (int slot = 0; slot < RootCount; ++slot)
        {
            Node a = (Node)Roots[slot]!;
            Node b = a.Next ?? throw new Exception("null A.Next");
            roots = unchecked(roots + unchecked(a.Value + b.Value));
            Cycle(a, b);
        }
        return Finish(state, total, roots);
    }
    private static uint NumericArrayAllocate(int count)
    {
        uint state = Seed, total = 0;
        for (int i = 0; i < count; ++i)
        {
            state = Next(state);
            int slot = i & (RootCount - 1);
            if (i >= RootCount)
            {
                uint[] oldcells = (uint[])Roots[slot]!;
                total = unchecked(total + Sum(oldcells));
            }
            uint[] fresh = new uint[Width];
            Array.Fill(fresh, state);
            Roots[slot] = fresh;
            uint[] cells = (uint[])Roots[slot]!;
            int selected = i & (Width - 1);
            int following = (selected + 1) & (Width - 1);
            cells[selected] = state ^ Salt;
            cells[following] = unchecked(state + (uint)i);
            total = unchecked(total + cells[selected] + cells[following] + (uint)cells.Length);
        }
        uint roots = 0;
        for (int slot = 0; slot < RootCount; ++slot)
        {
            uint[] cells = (uint[])Roots[slot]!;
            roots = unchecked(roots + Sum(cells));
        }
        return Finish(state, total, roots);
    }
    private static uint NumericArrayMutate(int count)
    {
        uint state = Seed, total = 0;
        for (int slot = 0; slot < RootCount; ++slot)
        {
            state = Next(state);
            uint[] fresh = new uint[Width];
            Array.Fill(fresh, state);
            Roots[slot] = fresh;
        }
        for (int i = 0; i < count; ++i)
        {
            state = Next(state);
            int slot = i & (RootCount - 1);
            uint[] cells = (uint[])Roots[slot]!;
            int selected = i & (Width - 1);
            int following = (selected + 1) & (Width - 1);
            cells[selected] = state ^ Salt;
            cells[following] = unchecked(state + (uint)i);
            total = unchecked(total + cells[selected] + cells[following] + (uint)cells.Length);
        }
        uint roots = 0;
        for (int slot = 0; slot < RootCount; ++slot)
        {
            uint[] cells = (uint[])Roots[slot]!;
            roots = unchecked(roots + Sum(cells));
        }
        return Finish(state, total, roots);
    }
    private static uint ReferenceArrayAllocate(int count)
    {
        uint state = Seed, total = 0;
        for (int i = 0; i < count; ++i)
        {
            state = Next(state);
            int slot = i & (RootCount - 1);
            if (i >= RootCount)
            {
                Node[] oldcells = (Node[])Roots[slot]!;
                Node olda = NodeRoots![slot] ?? throw new Exception("missing node root");
                Node oldb = olda.Next ?? throw new Exception("null A.Next");
                total = unchecked(total + Sum(oldcells, olda, oldb));
            }
            Node aFresh = Pair(state);
            Node[] fresh = new Node[Width];
            Array.Fill(fresh, aFresh);
            Roots[slot] = fresh;
            NodeRoots![slot] = aFresh;
            Node[] cells = (Node[])Roots[slot]!;
            Node a = NodeRoots![slot] ?? throw new Exception("missing node root");
            Node b = a.Next ?? throw new Exception("null A.Next");
            a.Value = unchecked(state + (uint)i);
            int selected = i & (Width - 1);
            int following = (selected + 1) & (Width - 1);
            cells[selected] = b;
            cells[following] = a;
            total = unchecked(total + cells[selected].Value + cells[following].Value + (uint)cells.Length);
            Cycle(a, b);
        }
        uint roots = 0;
        for (int slot = 0; slot < RootCount; ++slot)
        {
            Node[] cells = (Node[])Roots[slot]!;
            Node a = NodeRoots![slot] ?? throw new Exception("missing node root");
            Node b = a.Next ?? throw new Exception("null A.Next");
            roots = unchecked(roots + Sum(cells, a, b));
        }
        return Finish(state, total, roots);
    }
    private static uint ReferenceArrayMutate(int count)
    {
        uint state = Seed, total = 0;
        for (int slot = 0; slot < RootCount; ++slot)
        {
            state = Next(state);
            Node aFresh = Pair(state);
            Node[] fresh = new Node[Width];
            Array.Fill(fresh, aFresh);
            Roots[slot] = fresh;
            NodeRoots![slot] = aFresh;
        }
        for (int i = 0; i < count; ++i)
        {
            state = Next(state);
            int slot = i & (RootCount - 1);
            Node[] cells = (Node[])Roots[slot]!;
            Node a = NodeRoots![slot] ?? throw new Exception("missing node root");
            Node b = a.Next ?? throw new Exception("null A.Next");
            a.Value = unchecked(state + (uint)i);
            int selected = i & (Width - 1);
            int following = (selected + 1) & (Width - 1);
            cells[selected] = b;
            cells[following] = a;
            total = unchecked(total + cells[selected].Value + cells[following].Value + (uint)cells.Length);
            Cycle(a, b);
        }
        uint roots = 0;
        for (int slot = 0; slot < RootCount; ++slot)
        {
            Node[] cells = (Node[])Roots[slot]!;
            Node a = NodeRoots![slot] ?? throw new Exception("missing node root");
            Node b = a.Next ?? throw new Exception("null A.Next");
            roots = unchecked(roots + Sum(cells, a, b));
        }
        return Finish(state, total, roots);
    }
    private static uint Run(string family, string phase, int count) => (family, phase) switch
    {
        ("mutable-struct", "allocate") => MutableStructAllocate(count),
        ("mutable-struct", "mutate") => MutableStructMutate(count),
        ("reference-cycle", "allocate") => ReferenceCycleAllocate(count),
        ("reference-cycle", "mutate") => ReferenceCycleMutate(count),
        ("numeric-array", "allocate") => NumericArrayAllocate(count),
        ("numeric-array", "mutate") => NumericArrayMutate(count),
        ("reference-array", "allocate") => ReferenceArrayAllocate(count),
        ("reference-array", "mutate") => ReferenceArrayMutate(count),
        _ => throw new ArgumentException("unknown family/phase")
    };
    private static uint U32(string text) => uint.Parse(text, NumberStyles.None, CultureInfo.InvariantCulture);
    private static void Verify(uint actual, uint step, uint roots, uint state)
    {
        if (stepChecksum != step || rootChecksum != roots || lastState != state || actual != (step ^ roots))
            throw new Exception("independent scalar oracle/checksum mismatch");
    }
    public static void Main(string[] args)
    {
        bool telemetry = args.Length == 12 && args[11] == "--gc-telemetry";
        if (args.Length != 11 && !telemetry) throw new ArgumentException(
            "family phase count warmCount warmRounds step roots state warmStep warmRoots warmState [--gc-telemetry]");
        int n = int.Parse(args[2], CultureInfo.InvariantCulture);
        int warmN = int.Parse(args[3], CultureInfo.InvariantCulture);
        int rounds = int.Parse(args[4], CultureInfo.InvariantCulture);
        if (n < RootCount || n > 2000000 || warmN < RootCount || warmN > 2000000 || rounds < 0 || rounds > 64)
            throw new ArgumentException("count/round budget exceeded");
        if (args[0] == "reference-array") NodeRoots = new Node?[RootCount];
        uint step = U32(args[5]), roots = U32(args[6]), state = U32(args[7]);
        uint ws = U32(args[8]), wr = U32(args[9]), wl = U32(args[10]);
        for (int round = 0; round < rounds; ++round) Verify(Run(args[0], args[1], warmN), ws, wr, wl);
        if (telemetry) { Console.WriteLine("GC_TELEMETRY_START"); Console.Out.Flush(); }
        int cb0 = telemetry ? GC.CollectionCount(0) : 0;
        int cb1 = telemetry ? GC.CollectionCount(1) : 0;
        int cb2 = telemetry ? GC.CollectionCount(2) : 0;
        long pb = telemetry ? GC.GetTotalPauseDuration().Ticks : 0;
        long ab = telemetry ? GC.GetAllocatedBytesForCurrentThread() : 0;
        long tb = telemetry ? GC.GetTotalAllocatedBytes(false) : 0;
        long started = Stopwatch.GetTimestamp();
        uint actual = Run(args[0], args[1], n);
        TimeSpan elapsed = Stopwatch.GetElapsedTime(started);
        int ca0 = telemetry ? GC.CollectionCount(0) : 0;
        int ca1 = telemetry ? GC.CollectionCount(1) : 0;
        int ca2 = telemetry ? GC.CollectionCount(2) : 0;
        long pa = telemetry ? GC.GetTotalPauseDuration().Ticks : 0;
        long aa = telemetry ? GC.GetAllocatedBytesForCurrentThread() : 0;
        long ta = telemetry ? GC.GetTotalAllocatedBytes(false) : 0;
        if (telemetry) { Console.WriteLine("GC_TELEMETRY_END"); Console.Out.Flush(); }
        Verify(actual, step, roots, state);
        int perGroup = args[0] switch { "reference-cycle" => 2, "reference-array" => 3, _ => 1 };
        long planned = (long)perGroup * (args[1] == "allocate" ? n : RootCount);
        long configuredHeapLimit = -1;
        foreach (var setting in GC.GetConfigurationVariables())
            if (setting.Key.Equals("GCHeapHardLimit", StringComparison.OrdinalIgnoreCase))
                configuredHeapLimit = Convert.ToInt64(setting.Value, CultureInfo.InvariantCulture);
        var data = new Dictionary<string, object>
        {
            ["runtime"] = "dotnet", ["family"] = args[0], ["phase"] = args[1],
            ["iterations"] = n, ["warmup_iterations"] = warmN, ["warmup_rounds"] = rounds,
            ["execution_ns"] = elapsed.TotalNanoseconds, ["step_checksum_u32"] = stepChecksum,
            ["root_checksum_u32"] = rootChecksum, ["last_lcg_u32"] = lastState,
            ["return_checksum_u32"] = actual, ["planned_syntax_allocations"] = planned,
            ["root_groups"] = RootCount, ["table_root_slots"] = args[0] == "reference-array" ? 2048 : 1024,
            ["collector_roi"] = false, ["server_gc"] = GCSettings.IsServerGC,
            ["gc_latency_mode"] = GCSettings.LatencyMode.ToString(),
            ["gc_heap_budget_bytes"] = GC.GetGCMemoryInfo().TotalAvailableMemoryBytes,
            ["gc_heap_limit_config_bytes"] = configuredHeapLimit
        };
        if (telemetry)
        {
            data["gc_gen0_count_delta"] = ca0 - cb0; data["gc_gen1_count_delta"] = ca1 - cb1;
            data["gc_gen2_count_delta"] = ca2 - cb2; data["gc_total_pause_ns_delta"] = (pa-pb) * 100L;
            data["main_thread_allocated_bytes_delta"] = aa-ab;
            data["approximate_process_allocated_bytes_delta"] = ta-tb;
        }
        Console.WriteLine(JsonSerializer.Serialize(data));
    }
}
