package uwvm.hll;

import java.io.IOException;

/** Ordinary-engine profiling only: both blocking stdin gates are outside the timer. */
public final class PerfProfile {
    private static volatile long warmupChecksum;

    public static void main(String[] args) throws IOException {
        int iterations = args.length == 0 ? 2000000000 : Integer.parseInt(args[0]);
        if (iterations < 1 || iterations > 2000000000) {
            throw new IllegalArgumentException("iterations must be in [1, 2000000000]");
        }
        // Initialize stream objects and clock support before the attach boundary.
        if (System.in == null || System.out == null || System.err == null) {
            throw new IllegalStateException("standard streams are unavailable");
        }
        System.out.flush();
        System.err.flush();
        warmupChecksum = System.nanoTime();
        warmupChecksum ^= System.nanoTime();

        int[] state = new int[8192];
        initialize(state);
        warmupChecksum ^= checksum(state, kernel(state, 65536));
        initialize(state);
        warmupChecksum ^= checksum(state, kernel(state, 65536));
        // Reset every input changed by warmup; measured work matches PerfTimed.
        initialize(state);

        gate("UWVM_PROFILE_READY", 'G');
        long start = System.nanoTime();
        int accumulator = kernel(state, iterations);
        long kernelNs = System.nanoTime() - start;
        gate("UWVM_PROFILE_DONE", 'D');

        long result = checksum(state, accumulator);
        System.out.println("java perf timed: iterations=" + iterations);
        System.out.println("Kernel ns: " + kernelNs);
        System.out.println("checksum=" + result);
        System.out.flush();
    }

    private static void gate(String marker, int expected) throws IOException {
        System.err.println(marker);
        System.err.flush();
        int received = System.in.read();
        if (received != expected) {
            throw new IOException("profile gate expected " + expected + ", received " + received);
        }
    }

    private static void initialize(int[] state) {
        for (int i = 0; i < state.length; ++i) { state[i] = i * 0x9e3779b9 + 17; }
    }

    private static int kernel(int[] state, int iterations) {
        int accumulator = 0x12345678;
        for (int i = 0; i < iterations; ++i) {
            int index = (i ^ accumulator) & (state.length - 1);
            int value = state[index];
            value = Integer.rotateLeft(value ^ accumulator, 7) * 1664525 + 1013904223;
            state[index] = value;
            accumulator = Integer.rotateLeft(accumulator + value, 11) ^ i;
        }
        return accumulator;
    }

    private static long checksum(int[] state, int accumulator) {
        long checksum = accumulator;
        for (int value : state) { checksum = checksum * 31 + value; }
        return checksum;
    }
}
