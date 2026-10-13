import java.lang.management.GarbageCollectorMXBean;
import java.lang.management.ManagementFactory;
import java.util.Arrays;
import java.util.List;

/** Source-equivalent Core 3 GC workloads; never same-bytecode or same physical layout. */
public final class GeneralGc {
    private static final int ROOT_COUNT = 1024;
    private static final int WIDTH = 8;
    private static final int SEED = 123456789;
    private static final int SALT = 0xA5C31F27;
    // Actual global publication keeps every final group observable after Run returns.
    private static final Object[] ROOTS = new Object[ROOT_COUNT];
    private static Node[] NODE_ROOTS;
    private static int stepChecksum, rootChecksum, lastState;
    private static final class Box { int value; Box(int value) { this.value = value; } }
    private static final class Node {
        int value; Node next;
        Node(int value, Node next) { this.value = value; this.next = next; }
    }
    private static int next(int state) { return state * 1664525 + 1013904223; }
    private static Node pair(int state) {
        Node a = new Node(state, null);
        Node b = new Node(state * 3 + 17, a);
        a.next = b;
        return a;
    }
    private static void cycle(Node a, Node b) {
        if (b == null || b.next != a) throw new AssertionError("broken B.next=A");
    }
    private static int sum(int[] cells) {
        if (cells.length != WIDTH) throw new AssertionError("numeric array length");
        int total = 0;
        for (int cell : cells) total += cell;
        return total;
    }
    private static int sum(Node[] cells, Node a, Node b) {
        if (cells.length != WIDTH) throw new AssertionError("reference array length");
        int total = 0;
        for (Node cell : cells) {
            if (cell == null) throw new AssertionError("unexpected null cell");
            total += cell.value;
        }
        cycle(a, b);
        return total;
    }
    private static int finish(int state, int total, int roots) {
        lastState = state; stepChecksum = total; rootChecksum = roots;
        return total ^ roots;
    }
    private static int runMutableStructAllocate(int count)
    {
        int state = SEED, total = 0;
        for (int i = 0; i < count; ++i)
        {
            state = next(state);
            int slot = i & (ROOT_COUNT - 1);
            if (i >= ROOT_COUNT)
            {
                Box olda = (Box)ROOTS[slot];
                total = total + olda.value;
            }
            ROOTS[slot] = new Box(state);
            Box a = (Box)ROOTS[slot];
            a.value = state + i;
            total = total + a.value;
        }
        int roots = 0;
        for (int slot = 0; slot < ROOT_COUNT; ++slot)
        {
            Box a = (Box)ROOTS[slot];
            roots = roots + a.value;
        }
        return finish(state, total, roots);
    }
    private static int runMutableStructMutate(int count)
    {
        int state = SEED, total = 0;
        for (int slot = 0; slot < ROOT_COUNT; ++slot)
        {
            state = next(state);
            ROOTS[slot] = new Box(state);
        }
        for (int i = 0; i < count; ++i)
        {
            state = next(state);
            int slot = i & (ROOT_COUNT - 1);
            Box a = (Box)ROOTS[slot];
            a.value = state + i;
            total = total + a.value;
        }
        int roots = 0;
        for (int slot = 0; slot < ROOT_COUNT; ++slot)
        {
            Box a = (Box)ROOTS[slot];
            roots = roots + a.value;
        }
        return finish(state, total, roots);
    }
    private static int runReferenceCycleAllocate(int count)
    {
        int state = SEED, total = 0;
        for (int i = 0; i < count; ++i)
        {
            state = next(state);
            int slot = i & (ROOT_COUNT - 1);
            if (i >= ROOT_COUNT)
            {
                Node olda = (Node)ROOTS[slot];
                Node oldb = olda.next;
                total = total + olda.value + oldb.value;
                cycle(olda, oldb);
            }
            ROOTS[slot] = pair(state);
            Node a = (Node)ROOTS[slot];
            Node b = a.next;
            a.value = state + i;
            a.next = a;
            int observed = a.value + a.next.value;
            a.next = b;
            observed = observed + a.next.value;
            cycle(a, b);
            total = total + observed;
        }
        int roots = 0;
        for (int slot = 0; slot < ROOT_COUNT; ++slot)
        {
            Node a = (Node)ROOTS[slot];
            Node b = a.next;
            roots = roots + a.value + b.value;
            cycle(a, b);
        }
        return finish(state, total, roots);
    }
    private static int runReferenceCycleMutate(int count)
    {
        int state = SEED, total = 0;
        for (int slot = 0; slot < ROOT_COUNT; ++slot)
        {
            state = next(state);
            ROOTS[slot] = pair(state);
        }
        for (int i = 0; i < count; ++i)
        {
            state = next(state);
            int slot = i & (ROOT_COUNT - 1);
            Node a = (Node)ROOTS[slot];
            Node b = a.next;
            a.value = state + i;
            a.next = a;
            int observed = a.value + a.next.value;
            a.next = b;
            observed = observed + a.next.value;
            cycle(a, b);
            total = total + observed;
        }
        int roots = 0;
        for (int slot = 0; slot < ROOT_COUNT; ++slot)
        {
            Node a = (Node)ROOTS[slot];
            Node b = a.next;
            roots = roots + a.value + b.value;
            cycle(a, b);
        }
        return finish(state, total, roots);
    }
    private static int runNumericArrayAllocate(int count)
    {
        int state = SEED, total = 0;
        for (int i = 0; i < count; ++i)
        {
            state = next(state);
            int slot = i & (ROOT_COUNT - 1);
            if (i >= ROOT_COUNT)
            {
                int[] oldcells = (int[])ROOTS[slot];
                total = total + sum(oldcells);
            }
            int[] fresh = new int[WIDTH];
            Arrays.fill(fresh, state);
            ROOTS[slot] = fresh;
            int[] cells = (int[])ROOTS[slot];
            int selected = i & (WIDTH - 1);
            int following = (selected + 1) & (WIDTH - 1);
            cells[selected] = state ^ SALT;
            cells[following] = state + i;
            total = total + cells[selected] + cells[following] + cells.length;
        }
        int roots = 0;
        for (int slot = 0; slot < ROOT_COUNT; ++slot)
        {
            int[] cells = (int[])ROOTS[slot];
            roots = roots + sum(cells);
        }
        return finish(state, total, roots);
    }
    private static int runNumericArrayMutate(int count)
    {
        int state = SEED, total = 0;
        for (int slot = 0; slot < ROOT_COUNT; ++slot)
        {
            state = next(state);
            int[] fresh = new int[WIDTH];
            Arrays.fill(fresh, state);
            ROOTS[slot] = fresh;
        }
        for (int i = 0; i < count; ++i)
        {
            state = next(state);
            int slot = i & (ROOT_COUNT - 1);
            int[] cells = (int[])ROOTS[slot];
            int selected = i & (WIDTH - 1);
            int following = (selected + 1) & (WIDTH - 1);
            cells[selected] = state ^ SALT;
            cells[following] = state + i;
            total = total + cells[selected] + cells[following] + cells.length;
        }
        int roots = 0;
        for (int slot = 0; slot < ROOT_COUNT; ++slot)
        {
            int[] cells = (int[])ROOTS[slot];
            roots = roots + sum(cells);
        }
        return finish(state, total, roots);
    }
    private static int runReferenceArrayAllocate(int count)
    {
        int state = SEED, total = 0;
        for (int i = 0; i < count; ++i)
        {
            state = next(state);
            int slot = i & (ROOT_COUNT - 1);
            if (i >= ROOT_COUNT)
            {
                Node[] oldcells = (Node[])ROOTS[slot];
                Node olda = NODE_ROOTS[slot];
                Node oldb = olda.next;
                total = total + sum(oldcells, olda, oldb);
            }
            Node aFresh = pair(state);
            Node[] fresh = new Node[WIDTH];
            Arrays.fill(fresh, aFresh);
            ROOTS[slot] = fresh;
            NODE_ROOTS[slot] = aFresh;
            Node[] cells = (Node[])ROOTS[slot];
            Node a = NODE_ROOTS[slot];
            Node b = a.next;
            a.value = state + i;
            int selected = i & (WIDTH - 1);
            int following = (selected + 1) & (WIDTH - 1);
            cells[selected] = b;
            cells[following] = a;
            total = total + cells[selected].value + cells[following].value + cells.length;
            cycle(a, b);
        }
        int roots = 0;
        for (int slot = 0; slot < ROOT_COUNT; ++slot)
        {
            Node[] cells = (Node[])ROOTS[slot];
            Node a = NODE_ROOTS[slot];
            Node b = a.next;
            roots = roots + sum(cells, a, b);
        }
        return finish(state, total, roots);
    }
    private static int runReferenceArrayMutate(int count)
    {
        int state = SEED, total = 0;
        for (int slot = 0; slot < ROOT_COUNT; ++slot)
        {
            state = next(state);
            Node aFresh = pair(state);
            Node[] fresh = new Node[WIDTH];
            Arrays.fill(fresh, aFresh);
            ROOTS[slot] = fresh;
            NODE_ROOTS[slot] = aFresh;
        }
        for (int i = 0; i < count; ++i)
        {
            state = next(state);
            int slot = i & (ROOT_COUNT - 1);
            Node[] cells = (Node[])ROOTS[slot];
            Node a = NODE_ROOTS[slot];
            Node b = a.next;
            a.value = state + i;
            int selected = i & (WIDTH - 1);
            int following = (selected + 1) & (WIDTH - 1);
            cells[selected] = b;
            cells[following] = a;
            total = total + cells[selected].value + cells[following].value + cells.length;
            cycle(a, b);
        }
        int roots = 0;
        for (int slot = 0; slot < ROOT_COUNT; ++slot)
        {
            Node[] cells = (Node[])ROOTS[slot];
            Node a = NODE_ROOTS[slot];
            Node b = a.next;
            roots = roots + sum(cells, a, b);
        }
        return finish(state, total, roots);
    }
    private static int run(String family, String phase, int count) {
        return switch (family) {
            case "mutable-struct" -> phase.equals("allocate") ? runMutableStructAllocate(count) : runMutableStructMutate(count);
            case "reference-cycle" -> phase.equals("allocate") ? runReferenceCycleAllocate(count) : runReferenceCycleMutate(count);
            case "numeric-array" -> phase.equals("allocate") ? runNumericArrayAllocate(count) : runNumericArrayMutate(count);
            case "reference-array" -> phase.equals("allocate") ? runReferenceArrayAllocate(count) : runReferenceArrayMutate(count);
            default -> throw new IllegalArgumentException("unknown family/phase");
        };
    }
    private static int u32(String text) {
        long value = Long.parseUnsignedLong(text);
        if (Long.compareUnsigned(value, 0xffffffffL) > 0) throw new IllegalArgumentException("u32 expected");
        return (int)value;
    }
    private static void verify(int actual, int step, int roots, int state) {
        if (stepChecksum != step || rootChecksum != roots || lastState != state || actual != (step ^ roots))
            throw new AssertionError("independent scalar oracle/checksum mismatch");
    }
    private static long count(List<GarbageCollectorMXBean> beans, boolean time) {
        long total = 0;
        for (int i = 0; i < beans.size(); ++i) {
            GarbageCollectorMXBean bean = beans.get(i);
            long value = time ? bean.getCollectionTime() : bean.getCollectionCount();
            if (value < 0) throw new IllegalStateException("collector counter unavailable");
            total += value;
        }
        return total;
    }
    public static void main(String[] args) {
        boolean telemetry = args.length == 12 && args[11].equals("--gc-telemetry");
        if (args.length != 11 && !telemetry) throw new IllegalArgumentException(
            "family phase count warmCount warmRounds step roots state warmStep warmRoots warmState [--gc-telemetry]");
        if (!args[1].equals("allocate") && !args[1].equals("mutate")) throw new IllegalArgumentException("unknown phase");
        int n = Integer.parseInt(args[2]), warmN = Integer.parseInt(args[3]), rounds = Integer.parseInt(args[4]);
        if (n < ROOT_COUNT || n > 2000000 || warmN < ROOT_COUNT || warmN > 2000000 || rounds < 0 || rounds > 64)
            throw new IllegalArgumentException("count/round budget exceeded");
        if (args[0].equals("reference-array")) NODE_ROOTS = new Node[ROOT_COUNT];
        int step = u32(args[5]), roots = u32(args[6]), state = u32(args[7]);
        int ws = u32(args[8]), wr = u32(args[9]), wl = u32(args[10]);
        List<GarbageCollectorMXBean> beans = telemetry ? ManagementFactory.getGarbageCollectorMXBeans() : List.of();
        com.sun.management.ThreadMXBean allocationBean = null;
        if (telemetry) {
            if (!(ManagementFactory.getThreadMXBean() instanceof com.sun.management.ThreadMXBean candidate) ||
                !candidate.isThreadAllocatedMemorySupported()) throw new IllegalStateException("allocation counter unavailable");
            allocationBean = candidate;
            if (!allocationBean.isThreadAllocatedMemoryEnabled()) allocationBean.setThreadAllocatedMemoryEnabled(true);
        }
        for (int round = 0; round < rounds; ++round) verify(run(args[0], args[1], warmN), ws, wr, wl);
        if (telemetry) { System.out.println("GC_TELEMETRY_START"); System.out.flush(); }
        long cb = telemetry ? count(beans, false) : 0, pb = telemetry ? count(beans, true) : 0;
        long ab = telemetry ? allocationBean.getThreadAllocatedBytes(Thread.currentThread().getId()) : 0;
        long started = System.nanoTime();
        int actual = run(args[0], args[1], n);
        long elapsed = System.nanoTime() - started;
        long ca = telemetry ? count(beans, false) : 0, pa = telemetry ? count(beans, true) : 0;
        long aa = telemetry ? allocationBean.getThreadAllocatedBytes(Thread.currentThread().getId()) : 0;
        if (telemetry) { System.out.println("GC_TELEMETRY_END"); System.out.flush(); }
        verify(actual, step, roots, state);
        int perGroup = switch (args[0]) { case "reference-cycle" -> 2; case "reference-array" -> 3; default -> 1; };
        long planned = (long)perGroup * (args[1].equals("allocate") ? n : ROOT_COUNT);
        String gc = telemetry ? ",\"gc_mxbean_count_delta\":" + (ca-cb) +
            ",\"gc_mxbean_collection_time_ms_delta\":" + (pa-pb) +
            ",\"main_thread_allocated_bytes_delta\":" + (aa-ab) : "";
        System.out.println("{\"runtime\":\"java\",\"family\":\"" + args[0] + "\",\"phase\":\"" + args[1] +
            "\",\"iterations\":" + n + ",\"warmup_iterations\":" + warmN + ",\"warmup_rounds\":" + rounds +
            ",\"execution_ns\":" + elapsed + ",\"step_checksum_u32\":" + Integer.toUnsignedLong(stepChecksum) +
            ",\"root_checksum_u32\":" + Integer.toUnsignedLong(rootChecksum) + ",\"last_lcg_u32\":" +
            Integer.toUnsignedLong(lastState) + ",\"return_checksum_u32\":" + Integer.toUnsignedLong(actual) +
            ",\"planned_syntax_allocations\":" + planned + ",\"root_groups\":1024,\"table_root_slots\":" +
            (args[0].equals("reference-array") ? 2048 : 1024) + ",\"collector_roi\":false" + gc + "}");
    }
}
