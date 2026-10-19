package demo;
/** Reachable unresolved JNI-style native method: no @Import or generator. */
public final class NativeFailure {
    public static native int unavailableNative(int value);
    public static void main(String[] args) {
        System.out.println(unavailableNative(7));
    }
}
