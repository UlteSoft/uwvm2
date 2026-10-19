package demo;
public final class ManifestMain {
    public static void main(String[] args) {
        int value = 0;
        for (int i = 0; i < 7; ++i) value += 6;
        System.out.println("manifest-jar: " + value);
        System.out.flush();
    }
}
