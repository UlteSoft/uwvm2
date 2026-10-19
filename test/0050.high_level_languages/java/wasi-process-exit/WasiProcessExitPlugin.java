package uwvm.hll.teavm;

import org.teavm.backend.wasm.TeaVMWasmHost;
import org.teavm.backend.wasm.WasmRuntimeType;
import org.teavm.model.ClassHolderTransformer;
import org.teavm.model.MethodReference;
import org.teavm.model.instructions.InvokeInstruction;
import org.teavm.vm.spi.TeaVMHost;
import org.teavm.vm.spi.TeaVMPlugin;

/** Explicit compiler plugin for the pinned TeaVM legacy WASI target only. */
public final class WasiProcessExitPlugin implements TeaVMPlugin {
    private static final MethodReference SYSTEM_EXIT =
            new MethodReference(System.class, "exit", int.class, void.class);
    private static final MethodReference WASI_EXIT =
            new MethodReference(WasiSystemExit.class, "procExit", int.class, void.class);

    @Override
    public void install(TeaVMHost host) {
        TeaVMWasmHost wasm = host.getExtension(TeaVMWasmHost.class);
        if (wasm == null || wasm.getRuntimeType() != WasmRuntimeType.WASI) return;
        host.add((ClassHolderTransformer) (cls, context) -> {
            for (var method : cls.getMethods()) {
                var program = method.getProgram();
                if (program == null) continue;
                for (int index = 0; index < program.basicBlockCount(); index++) {
                    for (var instruction : program.basicBlockAt(index)) {
                        if (!(instruction instanceof InvokeInstruction invoke)) continue;
                        MethodReference reference = invoke.getMethod();
                        boolean systemClass = reference.getClassName().equals("java.lang.System")
                                || reference.getClassName().equals("org.teavm.classlib.java.lang.TSystem");
                        if (systemClass && reference.getDescriptor().equals(SYSTEM_EXIT.getDescriptor())
                                && invoke.getInstance() == null && invoke.getReceiver() == null
                                && invoke.getArguments().size() == 1) {
                            invoke.setMethod(WASI_EXIT);
                        }
                    }
                }
            }
        });
    }
}
