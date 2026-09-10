import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

import java.io.File;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.util.LinkedHashSet;
import java.util.Set;

public class ExportCallers extends GhidraScript {
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
                Address target = toAddr(Long.decode(args[i]));
                writer.println("target=" + target);
                ReferenceIterator references = currentProgram.getReferenceManager().getReferencesTo(target);
                Set<Function> callers = new LinkedHashSet<>();
                while (references.hasNext()) {
                    Reference reference = references.next();
                    writer.println("xref=" + reference.getFromAddress() + " type=" + reference.getReferenceType());
                    Function caller = currentProgram.getFunctionManager().getFunctionContaining(reference.getFromAddress());
                    if (caller != null) {
                        callers.add(caller);
                    }
                }
                for (Function caller : callers) {
                    writer.println("caller=" + caller.getName() + "@" + caller.getEntryPoint());
                    DecompileResults result = decompiler.decompileFunction(caller, 60, monitor);
                    if (result.decompileCompleted()) {
                        writer.println(result.getDecompiledFunction().getC());
                    } else {
                        writer.println("decompile_error=" + result.getErrorMessage());
                    }
                }
                writer.println("---");
            }
        } finally {
            decompiler.dispose();
        }
    }
}
