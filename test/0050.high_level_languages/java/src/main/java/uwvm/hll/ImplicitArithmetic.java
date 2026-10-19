package uwvm.hll;

/** Known TeaVM legacy-WASI semantic gap, preserved instead of silently skipped. */
public final class ImplicitArithmetic {
    public static void main(String[] args) {
        // A literal zero also crashes TeaVM 0.13.1's BoundCheckInsertion pass
        // after inlining. Parsing it keeps this a guest-runtime test.
        int divisor = Integer.parseInt(args.length == 0 ? "0" : args[0]);
        try {
            System.out.println("result=" + (100 / divisor));
        } catch (ArithmeticException expected) {
            System.out.println("java implicit arithmetic: PASS");
        }
    }
}
