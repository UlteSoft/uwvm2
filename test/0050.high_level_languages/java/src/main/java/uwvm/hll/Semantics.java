package uwvm.hll;

/** Real Java bytecode input for TeaVM; no JavaScript interop or custom imports. */
public final class Semantics {
    private interface Value { int value(); }
    private static class Base implements Value {
        final int number;
        Base(int number) { this.number = number; }
        public int value() { return number; }
    }
    private static final class Derived extends Base {
        Derived(int number) { super(number); }
        public int value() { return number * 3 + 1; }
    }
    private static final class Node {
        final int[] values;
        Node next;
        Node(int seed) { values = new int[257]; values[0] = seed; values[256] = seed ^ 0x5a5a; }
    }

    private static void check(boolean condition, String label) {
        if (!condition) { throw new AssertionError(label); }
    }

    private static int exceptions(int input) {
        int result = 0;
        try {
            if (input == 7) { throw new IllegalArgumentException("seven"); }
            // TeaVM's legacy WASI backend does not turn Wasm division traps
            // into Java exceptions; the unguarded case has a separate fixture.
            if (input == 0) { throw new ArithmeticException("zero"); }
            result = 100 / input;
        } catch (IllegalArgumentException error) {
            check("seven".equals(error.getMessage()), "exception message");
            result = 19;
        } catch (ArithmeticException error) {
            result = 23;
        } finally {
            result += 5;
        }
        return result;
    }

    private static int allocations() {
        Node retained = new Node(123);
        retained.next = retained;
        int checksum = 0;
        for (int i = 0; i < 8192; ++i) {
            Node current = new Node(i);
            current.next = current;
            checksum += current.values[0] + current.values[256];
            if ((i & 1023) == 0) { System.gc(); }
        }
        check(retained.next == retained && retained.values[0] == 123, "retained cycle");
        return checksum;
    }

    private static void implicitChecks(Object object, int[] values, int index) {
        int caught = 0;
        try {
            object.hashCode();
        } catch (NullPointerException expected) {
            caught |= 1;
        }
        try {
            int value = values[index];
            // Make a successful read observable so it cannot be eliminated.
            caught += value;
        } catch (ArrayIndexOutOfBoundsException expected) {
            caught |= 2;
        }
        check(caught == 3, "implicit null and bounds exceptions");
    }

    private static int compute() {
        Value[] objects = {new Base(4), new Derived(7), new Derived(-3)};
        int dispatch = 0;
        for (Value item : objects) { dispatch += item.value(); }
        check(dispatch == 18, "interface virtual dispatch");
        check(objects[1] instanceof Derived && ((Base) objects[1]).number == 7, "instanceof cast");
        int[][] array = {{2, 3}, {5, 7, 11}};
        int arraySum = 0;
        for (int[] row : array) { for (int value : row) { arraySum += value; } }
        check(arraySum == 28, "multidimensional arrays");
        int[] copied = new int[4];
        System.arraycopy(array[1], 0, copied, 1, 3);
        check(copied[0] == 0 && copied[3] == 11, "arraycopy zero initialization");
        implicitChecks(null, copied, Integer.parseInt("9"));
        String text = "Java \u4e16\u754c \uD83D\uDE80";
        check(text.length() == 10 && text.codePointAt(8) == 0x1f680, "UTF-16 supplementary code point");
        check(("alpha" + arraySum).equals("alpha28"), "string concatenation");
        // Keep the input dynamic rather than allowing literal-zero folding.
        int runtimeZero = Integer.parseInt("0");
        check(exceptions(7) == 24 && exceptions(runtimeZero) == 28 && exceptions(4) == 30, "catch finally");
        long wide = 0x123456789abcdef0L;
        check(Long.rotateLeft(wide, 8) == 0x3456789abcdef012L, "long rotate");
        check((byte) 255 == -1 && ((-1 >>> 1) == 2147483647), "integer sign and shifts");
        int checksum = allocations();
        for (int i = 0; i < 100000; ++i) { checksum = Integer.rotateLeft(checksum ^ i, 5) + 0x9e3779b9; }
        return checksum;
    }

    public static void main(String[] args) {
        int checksum = compute();
        System.out.println("java semantics: PASS");
        System.out.println("classes arrays exceptions strings utf16 long allocation-cycles gc-request");
        System.out.println("checksum=" + checksum);
    }
}
