import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

import java.io.File;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;

public class ExportFunctions extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            throw new IllegalArgumentException("usage: output address...");
        }
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        try (PrintWriter writer = new PrintWriter(new File(args[0]), StandardCharsets.UTF_8)) {
            for (int i = 1; i < args.length; i++) {
                Address address = toAddr(Long.decode(args[i]));
                Function function = currentProgram.getFunctionManager().getFunctionContaining(address);
                writer.println("requested=" + address);
                if (function == null) {
                    writer.println("function=NONE");
                    continue;
                }
                writer.println("function=" + function.getName() + "@" + function.getEntryPoint());
                DecompileResults result = decompiler.decompileFunction(function, 60, monitor);
                if (result.decompileCompleted()) {
                    writer.println(result.getDecompiledFunction().getC());
                } else {
                    writer.println("decompile_error=" + result.getErrorMessage());
                }
                writer.println("---");
            }
        } finally {
            decompiler.dispose();
        }
    }
}
