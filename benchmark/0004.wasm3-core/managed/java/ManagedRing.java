import java.lang.management.GarbageCollectorMXBean;
import java.lang.management.ManagementFactory;

public final class ManagedRing {
    private static final int SEED = 123456789;
    private static final int A = 1664525;
    private static final int C = 1013904223;
    private static final Object[] ROOTS = new Object[1024];

    private record Box(int value) {}

    private static int run(int count) {
        int state = SEED;
        for (int remaining = count; remaining != 0; --remaining) {
            state = state * A + C;
            int index = remaining & 1023;
            ROOTS[index] = new Box(state);
            state = ((Box) ROOTS[index]).value;
        }
        return state;
    }

    private static int expected(int count) {
        long multiplier = 1, increment = 0, powerMultiplier = A, powerIncrement = C;
        while (count != 0) {
            if ((count & 1) != 0) {
                multiplier = multiplier * powerMultiplier & 0xffffffffL;
                increment = (increment * powerMultiplier + powerIncrement) & 0xffffffffL;
            }
            powerIncrement = (powerMultiplier + 1) * powerIncrement & 0xffffffffL;
            powerMultiplier = powerMultiplier * powerMultiplier & 0xffffffffL;
            count >>>= 1;
        }
        return (int) ((multiplier * SEED + increment) & 0xffffffffL);
    }

    private static long verifyRoots(int count) {
        if (count < ROOTS.length) throw new IllegalArgumentException("need at least 1024 measured steps");
        int state = expected(count - ROOTS.length);
        int expectedXor = 0;
        for (int i = 0; i < ROOTS.length; ++i) {
            state = state * A + C;
            int index = (ROOTS.length - i) & (ROOTS.length - 1);
            if (!(ROOTS[index] instanceof Box box) || box.value() != state)
                throw new AssertionError("root ring slot mismatch at " + index);
            expectedXor ^= state;
        }
        int observedXor = 0;
        for (Object root : ROOTS) {
            if (!(root instanceof Box box)) throw new AssertionError("missing live root");
            observedXor ^= box.value();
        }
        if (observedXor != expectedXor) throw new AssertionError("root ring mismatch");
        return Integer.toUnsignedLong(observedXor);
    }

    public static void main(String[] args) {
        boolean gcTelemetry = args.length == 2 && args[1].equals("--gc-telemetry");
        if (args.length != 1 && !gcTelemetry)
            throw new IllegalArgumentException("expected one positive iteration count and optional --gc-telemetry");
        int count = Integer.parseInt(args[0]);
        if (count <= 0) throw new IllegalArgumentException("count must be positive");
        for (int i = 0; i < 8; ++i) {
            if (run(250000) != expected(250000)) throw new AssertionError("warmup mismatch");
        }
        long collectionsBefore = gcTelemetry ? collectionCount() : 0;
        long pauseBeforeMs = gcTelemetry ? collectionTimeMs() : 0;
        long allocatedBefore = gcTelemetry ? threadAllocatedBytes() : 0;
        if (gcTelemetry) { System.out.println("GC_TELEMETRY_START"); System.out.flush(); }
        long started = System.nanoTime();
        int actual = run(count);
        long elapsed = System.nanoTime() - started;
        long collectionsAfter = gcTelemetry ? collectionCount() : 0;
        long pauseAfterMs = gcTelemetry ? collectionTimeMs() : 0;
        long allocatedAfter = gcTelemetry ? threadAllocatedBytes() : 0;
        if (gcTelemetry) { System.out.println("GC_TELEMETRY_END"); System.out.flush(); }
        if (actual != expected(count)) throw new AssertionError("result mismatch");
        long rootChecksum = verifyRoots(count);
        String gcFields = gcTelemetry
                ? ",\"gc_collection_count\":" + (collectionsAfter - collectionsBefore)
                  + ",\"gc_total_collection_time_ms\":" + (pauseAfterMs - pauseBeforeMs)
                  + ",\"main_thread_allocated_bytes\":" + (allocatedAfter - allocatedBefore)
                : "";
        System.out.println("{\"runtime\":\"java\",\"iterations\":" + count
                + ",\"guest_ns\":" + elapsed + ",\"checksum\":" + Integer.toUnsignedLong(actual)
                + ",\"root_checksum\":" + rootChecksum + gcFields + "}");
    }

    private static long collectionCount() {
        long count = 0;
        for (GarbageCollectorMXBean collector : ManagementFactory.getGarbageCollectorMXBeans()) {
            long value = collector.getCollectionCount();
            if (value < 0) throw new IllegalStateException("GC collection count unavailable");
            count += value;
        }
        return count;
    }

    private static long collectionTimeMs() {
        long milliseconds = 0;
        for (GarbageCollectorMXBean collector : ManagementFactory.getGarbageCollectorMXBeans()) {
            long value = collector.getCollectionTime();
            if (value < 0) throw new IllegalStateException("GC collection time unavailable");
            milliseconds += value;
        }
        return milliseconds;
    }

    private static long threadAllocatedBytes() {
        var bean = ManagementFactory.getThreadMXBean();
        if (!(bean instanceof com.sun.management.ThreadMXBean hotspot)
                || !hotspot.isThreadAllocatedMemorySupported())
            throw new IllegalStateException("HotSpot main-thread allocation telemetry unavailable");
        if (!hotspot.isThreadAllocatedMemoryEnabled()) hotspot.setThreadAllocatedMemoryEnabled(true);
        return hotspot.getThreadAllocatedBytes(Thread.currentThread().threadId());
    }
}
