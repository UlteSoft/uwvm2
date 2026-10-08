import java.lang.invoke.VarHandle;

/** Platform-thread creation plus the integer kernel in wasm_thread_performance.cc.
 * This is a cross-language analogue, not an execution of the same Wasm module.
 */
public final class ManagedThreads {
    private static final int WORDS = 1024;
    private static final int PASSES = 32;
    private static final int[] MEMORY = new int[4 * WORDS];

    private static int kernel(int worker) {
        VarHandle.fullFence();
        int base = worker * WORDS;
        int seed = 0x12345 + worker;
        for (int i = 0; i < WORDS; ++i) MEMORY[base + i] = i + seed;
        for (int pass = 0; pass < PASSES; ++pass) {
            for (int i = 0; i < WORDS; ++i) {
                int address = base + i;
                MEMORY[address] = (Integer.rotateLeft(MEMORY[address], 7) ^ (seed + pass))
                        + 0x9e3779b9;
            }
        }
        int checksum = 0;
        for (int i = 0; i < WORDS; ++i)
            checksum = Integer.rotateLeft(checksum, 5) + MEMORY[base + i];
        return checksum;
    }

    private static long measure(boolean threaded, int workers, int rounds, int sample,
                                int[] expected) throws InterruptedException {
        long wall = 0;
        long checksum = 0;
        for (int round = 0; round < rounds; ++round) {
            int[] results = new int[workers];
            Thread[] threads = new Thread[workers];
            long start = System.nanoTime();
            for (int worker = 0; worker < workers; ++worker) {
                final int index = worker;
                if (threaded) {
                    threads[worker] = new Thread(() -> results[index] = kernel(index));
                    threads[worker].start();
                } else {
                    results[worker] = kernel(worker);
                }
            }
            if (threaded)
                for (Thread thread : threads) thread.join();
            wall += System.nanoTime() - start;
            for (int worker = 0; worker < workers; ++worker) {
                if (results[worker] != expected[worker])
                    throw new AssertionError("kernel checksum mismatch for worker " + worker);
                checksum += Integer.toUnsignedLong(results[worker]);
            }
        }
        if (sample >= 0)
            System.out.println("{\"runtime\":\"java-platform-thread\",\"workers\":" + workers
                    + ",\"threaded\":" + threaded + ",\"sample\":" + sample
                    + ",\"rounds\":" + rounds + ",\"wall_ns\":" + (wall / rounds)
                    + ",\"checksum\":" + checksum + ",\"requested_threads\":"
                    + (threaded ? workers * rounds : 0) + "}");
        return checksum;
    }

    public static void main(String[] args) throws InterruptedException {
        if (args.length != 3) throw new IllegalArgumentException("samples rounds order-offset required");
        int samples = Integer.parseInt(args[0]);
        int rounds = Integer.parseInt(args[1]);
        int offset = Integer.parseInt(args[2]);
        if (samples < 1 || samples > 99 || rounds < 1 || rounds > 1024 || offset < 0 || offset > 1)
            throw new IllegalArgumentException("samples/rounds/order-offset out of range");
        int[] expected = new int[4];
        for (int worker = 0; worker < 4; ++worker) expected[worker] = kernel(worker);
        for (int sample = -2; sample < samples; ++sample) {
            for (int workers : new int[]{1, 4}) {
                boolean firstThreaded = ((sample + offset) & 1) != 0;
                measure(firstThreaded, workers, rounds, sample, expected);
                measure(!firstThreaded, workers, rounds, sample, expected);
            }
        }
    }
}
