package uwvm.hll;

import java.util.Map;

/** Optional producer adapter: real named environment values and a cached read-only map. */
public final class ArgsEnvAdapted {
    public static void main(String[] args) {
        ArgsEnv.main(args);
        Map<String, String> environment = System.getenv();
        if (environment != System.getenv()) {
            throw new AssertionError("environment map must be cached");
        }
        String[] names = {"UWVM_HLL_EMPTY", "UWVM_HLL_UTF8", "UWVM_HLL_EQUALS", "UWVM_HLL_MISSING"};
        for (String name : names) {
            String named = System.getenv(name);
            String mapped = environment.get(name);
            if (named == null ? mapped != null : !named.equals(mapped)) {
                throw new AssertionError("map and named getter disagree: " + name);
            }
            if (environment.containsKey(name) != (named != null)) {
                throw new AssertionError("map key presence mismatch: " + name);
            }
        }
        boolean readonly = false;
        try {
            environment.put("UWVM_HLL_MUTATION", "not allowed");
        } catch (UnsupportedOperationException expected) {
            readonly = true;
        }
        boolean nullName = false;
        try {
            System.getenv((String) null);
        } catch (NullPointerException expected) {
            nullName = true;
        }
        boolean nullKey = false;
        try {
            environment.get(null);
        } catch (NullPointerException expected) {
            nullKey = true;
        }
        boolean nullValue = false;
        try {
            environment.containsValue(null);
        } catch (NullPointerException expected) {
            nullValue = true;
        }
        boolean wrongType = false;
        try {
            environment.get(Integer.valueOf(7));
        } catch (ClassCastException expected) {
            wrongType = true;
        }
        if (!readonly || !nullName || !nullKey || !nullValue || !wrongType) {
            throw new AssertionError("environment API contract mismatch");
        }
        System.gc();
        System.out.println("java env map: cached=true readonly=true named=true null-name=true null-key=true null-value=true wrong-type=true");
        System.out.flush();
    }
}
