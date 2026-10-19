package uwvm.hll;

/** A finite source-level scalar-value oracle; no expected values are injected. */
public final class TypedDebug {
    private TypedDebug() {}

    public static void main(String[] args) {
        int counter = Integer.parseInt(args[0]);
        while (counter < 25) {
            counter += 7;
            System.out.println("java typed debug: counter=" + counter);
        }
        System.out.println("java typed debug: final=" + counter);
    }
}
