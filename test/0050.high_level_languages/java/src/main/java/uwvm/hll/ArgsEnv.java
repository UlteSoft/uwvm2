package uwvm.hll;

/** Existing argv support plus an explicit probe of TeaVM's getenv stub. */
public final class ArgsEnv {
    public static void main(String[] args) {
        if (args == null) {
            throw new AssertionError("main args must be a non-null array");
        }
        for (String value : args) {
            if (value == null) {
                throw new AssertionError("an argv entry must be a string, including an empty one");
            }
        }
        if (args.length != 0) {
            String[] expected = {"", "\u96ea=\ud83d\ude42", "a=b=c", "="};
            if (args.length != expected.length) {
                throw new AssertionError("unexpected argument count: " + args.length);
            }
            for (int i = 0; i < expected.length; ++i) {
                if (!expected[i].equals(args[i])) {
                    throw new AssertionError("argument mismatch at " + i);
                }
            }
        }
        // The real runtime allocates exact argc pointers next to a byte array.
        // Collect before observing the retained decoded strings as an additional
        // integration check; this does not inspect or fabricate host memory.
        System.gc();
        System.out.println("java args/env: argc=" + args.length);
        for (int i = 0; i < args.length; ++i) {
            describe("arg[" + i + "]", args[i]);
        }
        // TeaVM 0.13.1 implements this overload as unconditional null. Preserve
        // that observable gap rather than replacing it with a custom env import.
        describe("env[UWVM_HLL_EMPTY]", System.getenv("UWVM_HLL_EMPTY"));
        describe("env[UWVM_HLL_UTF8]", System.getenv("UWVM_HLL_UTF8"));
        describe("env[UWVM_HLL_EQUALS]", System.getenv("UWVM_HLL_EQUALS"));
        describe("env[UWVM_HLL_MISSING]", System.getenv("UWVM_HLL_MISSING"));
        System.out.flush();
    }

    private static void describe(String name, String value) {
        StringBuilder line = new StringBuilder(name);
        if (value == null) {
            line.append("=NULL");
        } else {
            line.append("=len:").append(value.length()).append(",utf16:");
            for (int i = 0; i < value.length(); ++i) {
                if (i != 0) {
                    line.append(',');
                }
                line.append((int) value.charAt(i));
            }
        }
        System.out.println(line);
    }
}
