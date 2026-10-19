package uwvm.hll.teavm;

import org.teavm.interop.Import;
import org.teavm.interop.Unmanaged;

/** Guest WASI command exit; never a native Host process or JNI operation. */
public final class WasiSystemExit {
    private WasiSystemExit() { }

    @Import(module = "wasi_snapshot_preview1", name = "proc_exit")
    @Unmanaged
    public static native void procExit(int status);
}
