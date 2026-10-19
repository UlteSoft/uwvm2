package uwvm.hll.teavm;

import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.List;
import org.teavm.backend.wasm.TeaVMWasmHost;
import org.teavm.backend.wasm.WasmRuntimeType;
import org.teavm.model.ClassHolder;
import org.teavm.model.ClassHolderTransformer;
import org.teavm.model.MethodHolder;
import org.teavm.model.MethodReference;
import org.teavm.model.Program;
import org.teavm.model.ValueType;
import org.teavm.model.instructions.InvocationType;
import org.teavm.model.instructions.InvokeInstruction;
import org.teavm.vm.spi.TeaVMHost;

/** Finite compiler-model controls; the real Kotlin Wasm workflow is separate. */
public final class PluginContract {
    private static int checks;

    private static void require(boolean condition, String message) {
        if (!condition) throw new AssertionError(message);
        checks++;
    }

    private static List<ClassHolderTransformer> install(WasmRuntimeType type, boolean wasmPresent) {
        var registered = new ArrayList<ClassHolderTransformer>();
        TeaVMWasmHost wasm = (TeaVMWasmHost) Proxy.newProxyInstance(
                PluginContract.class.getClassLoader(), new Class<?>[]{TeaVMWasmHost.class},
                (proxy, method, args) -> {
                    if (method.getName().equals("getRuntimeType")) return type;
                    throw new AssertionError("Unexpected Wasm Host operation: " + method);
                });
        TeaVMHost host = (TeaVMHost) Proxy.newProxyInstance(
                PluginContract.class.getClassLoader(), new Class<?>[]{TeaVMHost.class},
                (proxy, method, args) -> {
                    if (method.getName().equals("getExtension")) {
                        require(args[0] == TeaVMWasmHost.class, "exact compiler extension queried");
                        return wasmPresent ? wasm : null;
                    }
                    if (method.getName().equals("add") && args.length == 1
                            && args[0] instanceof ClassHolderTransformer transformer) {
                        registered.add(transformer);
                        return null;
                    }
                    throw new AssertionError("Unexpected compiler Host operation: " + method);
                });
        new WasiProcessExitPlugin().install(host);
        return registered;
    }

    private static void probe(ClassHolderTransformer transformer, MethodReference reference,
                              int argumentCount, boolean instance, boolean receiver,
                              boolean mustRewrite) {
        ClassHolder cls = new ClassHolder("probe.ExistingJarClass");
        MethodHolder method = new MethodHolder("probe", ValueType.VOID);
        Program program = new Program();
        program.createVariable();
        var block = program.createBasicBlock();
        InvokeInstruction call = new InvokeInstruction();
        call.setType(InvocationType.SPECIAL);
        call.setMethod(reference);
        var variables = new org.teavm.model.Variable[argumentCount];
        for (int i = 0; i < variables.length; i++) variables[i] = program.createVariable();
        call.setArguments(variables);
        if (instance) call.setInstance(program.createVariable());
        if (receiver) call.setReceiver(program.createVariable());
        block.add(call);
        method.setProgram(program);
        cls.addMethod(method);
        cls.addMethod(new MethodHolder("noCode", ValueType.VOID));
        transformer.transformClass(cls, null);
        MethodReference expected = mustRewrite
                ? new MethodReference(WasiSystemExit.class, "procExit", int.class, void.class)
                : reference;
        require(call.getMethod().equals(expected), "exact static void(int) mapping or unchanged near miss");
    }

    public static void main(String[] args) {
        require(install(null, false).isEmpty(), "non-Wasm target unchanged");
        require(install(WasmRuntimeType.TEAVM, true).isEmpty(), "non-WASI legacy Wasm unchanged");
        var transforms = install(WasmRuntimeType.WASI, true);
        require(transforms.size() == 1, "exactly one WASI compiler transform");
        var transform = transforms.get(0);
        var exit = new MethodReference(System.class, "exit", int.class, void.class);
        probe(transform, exit, 1, false, false, true);
        probe(transform, new MethodReference("org.teavm.classlib.java.lang.TSystem", exit.getDescriptor()),
                1, false, false, true);
        probe(transform, new MethodReference(System.class, "gc", void.class), 0, false, false, false);
        probe(transform, new MethodReference(Runtime.class, "exit", int.class, void.class), 1, false, false, false);
        probe(transform, new MethodReference(System.class, "exit", long.class, void.class), 1, false, false, false);
        probe(transform, new MethodReference(System.class, "exit", int.class, int.class), 1, false, true, false);
        probe(transform, exit, 0, false, false, false);
        probe(transform, exit, 2, false, false, false);
        probe(transform, exit, 1, true, false, false);
        probe(transform, exit, 1, false, true, false);
        System.out.println("checks=" + checks + " scope=compiler-model-only native-exit-calls=0");
    }
}
