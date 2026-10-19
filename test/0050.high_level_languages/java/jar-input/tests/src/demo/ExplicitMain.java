package demo;
import dep.Message;
public final class ExplicitMain {
    private static void describe(String name, String value) {
        StringBuilder line = new StringBuilder(name);
        if (value == null) line.append("=NULL");
        else {
            line.append("=len:").append(value.length()).append(",utf16:");
            for (int i = 0; i < value.length(); ++i) {
                if (i != 0) line.append(',');
                line.append((int) value.charAt(i));
            }
        }
        System.out.println(line);
    }
    public static void main(String[] args) {
        System.gc();
        System.out.println(Message.text());
        System.out.println("argc=" + args.length);
        for (int i = 0; i < args.length; ++i) describe("arg[" + i + "]", args[i]);
        for (String key : new String[]{"UWVM_JAR_EMPTY", "UWVM_JAR_UTF8", "UWVM_JAR_EQUALS", "UWVM_JAR_MISSING"})
            describe("env[" + key + "]", System.getenv(key));
        System.out.flush();
    }
}
