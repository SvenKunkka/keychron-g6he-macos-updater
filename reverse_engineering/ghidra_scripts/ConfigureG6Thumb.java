import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.lang.Register;

import java.math.BigInteger;

public class ConfigureG6Thumb extends GhidraScript {
    @Override
    public void run() throws Exception {
        Address start = toAddr(0x20000800L);
        Address end = toAddr(0x200562e8L);
        Register tMode = currentProgram.getRegister("TMode");
        if (tMode == null) {
            throw new IllegalStateException("ARM TMode register is unavailable");
        }
        currentProgram.getProgramContext().setValue(tMode, start, end, BigInteger.ONE);

        long[] entryPoints = {
            0x20021fc8L,
            0x20021fb4L,
            0x20042caeL,
            0x2002212cL,
            0x200220c4L,
            0x200222f4L
        };
        for (long value : entryPoints) {
            Address address = toAddr(value);
            disassemble(address);
            if (getFunctionAt(address) == null) {
                createFunction(address, null);
            }
            currentProgram.getSymbolTable().addExternalEntryPoint(address);
        }
        println("Configured Thumb mode and " + entryPoints.length + " vector targets");
    }
}
