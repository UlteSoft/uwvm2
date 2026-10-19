package uwvm.hll;

/** Bounded, adjustable guest work for attachment after VM/JIT startup. */
public final class Perf {
    public static void main(String[] args) {
        int iterations = args.length == 0 ? 1000000 : Integer.parseInt(args[0]);
        if (iterations < 1 || iterations > 1000000000) {
            throw new IllegalArgumentException("iterations must be in [1, 1000000000]");
        }
        int[] state = new int[8192];
        for (int i = 0; i < state.length; ++i) { state[i] = i * 0x9e3779b9 + 17; }
        int accumulator = 0x12345678;
        for (int i = 0; i < iterations; ++i) {
            int index = (i ^ accumulator) & (state.length - 1);
            int value = state[index];
            value = Integer.rotateLeft(value ^ accumulator, 7) * 1664525 + 1013904223;
            state[index] = value;
            accumulator = Integer.rotateLeft(accumulator + value, 11) ^ i;
        }
        long checksum = accumulator;
        for (int value : state) { checksum = checksum * 31 + value; }
        System.out.println("java perf: iterations=" + iterations);
        System.out.println("checksum=" + checksum);
    }
}
