// @category Keychron
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

public class ExportSelectedDecomp extends GhidraScript {
    @Override
    protected void run() throws Exception {
        DecompInterface decompiler = new DecompInterface();
        decompiler.toggleCCode(true);
        decompiler.toggleSyntaxTree(true);
        if (!decompiler.openProgram(currentProgram)) {
            printerr("Unable to initialize the decompiler");
            return;
        }

        for (String value : getScriptArgs()) {
            Address address = toAddr(value);
            Function function = currentProgram.getFunctionManager().getFunctionAt(address);
            if (function == null) {
                function = currentProgram.getFunctionManager().getFunctionContaining(address);
            }
            if (function == null) {
                println("=== " + value + " NO_FUNCTION ===");
                continue;
            }
            println("=== " + value + " " + function.getName() + " @ " + function.getEntryPoint() + " ===");
            DecompileResults result = decompiler.decompileFunction(function, 120, monitor);
            if (!result.decompileCompleted()) {
                println("DECOMPILE_FAILED: " + result.getErrorMessage());
                continue;
            }
            println(result.getDecompiledFunction().getC());
        }
        decompiler.dispose();
    }
}
