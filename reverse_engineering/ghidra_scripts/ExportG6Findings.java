import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

import java.io.File;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.util.LinkedHashSet;
import java.util.Set;

public class ExportG6Findings extends GhidraScript {
    private Address findAscii(String text) throws Exception {
        Memory memory = currentProgram.getMemory();
        byte[] needle = text.getBytes(StandardCharsets.US_ASCII);
        return memory.findBytes(memory.getMinAddress(), memory.getMaxAddress(), needle, null, true, monitor);
    }

    @Override
    public void run() throws Exception {
        if (getScriptArgs().length != 1) {
            throw new IllegalArgumentException("output path required");
        }
        File output = new File(getScriptArgs()[0]);
        FunctionIterator functions = currentProgram.getFunctionManager().getFunctions(true);
        int functionCount = 0;
        while (functions.hasNext()) {
            functions.next();
            functionCount++;
        }

        String[] targets = {
            "TX power to enumerator conversion failed, defaulting to 0 dBm",
            "mouse_ble/rpa",
            "ppt_ptx/bond",
            "hall/mem0",
            "hall/mem1",
            "hall/ax0",
            "hall/ax1"
        };

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        try (PrintWriter writer = new PrintWriter(output, StandardCharsets.UTF_8)) {
            writer.println("program=" + currentProgram.getName());
            writer.println("language=" + currentProgram.getLanguageID());
            writer.println("functions=" + functionCount);
            for (String target : targets) {
                Address stringAddress = findAscii(target);
                writer.println();
                writer.println("string=" + target);
                writer.println("address=" + (stringAddress == null ? "NOT_FOUND" : stringAddress));
                if (stringAddress == null) {
                    continue;
                }
                ReferenceIterator references = currentProgram.getReferenceManager().getReferencesTo(stringAddress);
                Set<Function> owners = new LinkedHashSet<>();
                while (references.hasNext()) {
                    Reference reference = references.next();
                    writer.println("xref=" + reference.getFromAddress());
                    Function owner = currentProgram.getFunctionManager().getFunctionContaining(reference.getFromAddress());
                    if (owner != null) {
                        owners.add(owner);
                    }
                }
                if (owners.isEmpty()) {
                    writer.println("owner=NONE");
                }
                for (Function owner : owners) {
                    writer.println("owner=" + owner.getName() + "@" + owner.getEntryPoint());
                    DecompileResults result = decompiler.decompileFunction(owner, 60, monitor);
                    if (result.decompileCompleted()) {
                        writer.println("decompile_begin");
                        writer.println(result.getDecompiledFunction().getC());
                        writer.println("decompile_end");
                    } else {
                        writer.println("decompile_error=" + result.getErrorMessage());
                    }
                }
            }
        } finally {
            decompiler.dispose();
        }
        println("Exported G6 findings to " + output.getAbsolutePath());
    }
}
