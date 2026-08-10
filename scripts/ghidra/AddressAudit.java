// Headless Ghidra script: export private instruction/function context for explicit guest addresses.
// @category MCLA-R

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSpace;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.util.Iterator;

public class AddressAudit extends GhidraScript {
    private static String clean(String value) {
        if (value == null) {
            return "-";
        }
        return value.replace('\t', ' ').replace('\r', ' ').replace('\n', ' ');
    }

    private static String instructionText(Instruction instruction) {
        return instruction == null ? "<no-instruction>" : clean(instruction.toString());
    }

    private String bytesAt(Address address) {
        byte[] bytes = new byte[4];
        try {
            int count = currentProgram.getMemory().getBytes(address, bytes);
            if (count != bytes.length) {
                return "<short-read>";
            }
            return String.format("%02X%02X%02X%02X", bytes[0] & 0xff, bytes[1] & 0xff,
                bytes[2] & 0xff, bytes[3] & 0xff);
        }
        catch (Exception error) {
            return "<unreadable>";
        }
    }

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            throw new IllegalArgumentException("Usage: AddressAudit.java OUTPUT ADDRESS [ADDRESS ...]");
        }

        File output = new File(args[0]).getCanonicalFile();
        File parent = output.getParentFile();
        if (parent == null || !parent.isDirectory()) {
            throw new IllegalArgumentException("Output parent does not exist: " + output);
        }

        Listing listing = currentProgram.getListing();
        AddressSpace space = currentProgram.getAddressFactory().getDefaultAddressSpace();
        try (BufferedWriter writer = new BufferedWriter(new FileWriter(output))) {
            writer.write("program\t" + clean(currentProgram.getName()));
            writer.newLine();
            writer.write("image_base\t" + currentProgram.getImageBase());
            writer.newLine();
            writer.write("address\tbytes\tblock\texecute\tfunction_entry\tfunction_name\tinstruction\tprevious\tnext\treferences_from\treferences_to");
            writer.newLine();

            for (int index = 1; index < args.length; index++) {
                Address address = space.getAddress(args[index]);
                if (address == null || !currentProgram.getMemory().contains(address)) {
                    writer.write(clean(args[index]) + "\t-\t<unmapped>\tfalse\t-\t-\t<no-instruction>\t-\t-\t0\t0");
                    writer.newLine();
                    continue;
                }

                Instruction instruction = listing.getInstructionAt(address);
                if (instruction == null) {
                    disassemble(address);
                    instruction = listing.getInstructionAt(address);
                }
                Instruction previous = instruction == null ? listing.getInstructionBefore(address) : instruction.getPrevious();
                Instruction next = instruction == null ? listing.getInstructionAfter(address) : instruction.getNext();
                Function function = currentProgram.getFunctionManager().getFunctionContaining(address);
                MemoryBlock block = currentProgram.getMemory().getBlock(address);

                int referencesFrom = instruction == null ? 0 : instruction.getReferencesFrom().length;
                int referencesTo = 0;
                Iterator<Reference> referenceIterator = currentProgram.getReferenceManager().getReferencesTo(address);
                while (referenceIterator.hasNext()) {
                    referenceIterator.next();
                    referencesTo++;
                }

                writer.write(address.toString());
                writer.write("\t" + bytesAt(address));
                writer.write("\t" + clean(block == null ? null : block.getName()));
                writer.write("\t" + (block != null && block.isExecute()));
                writer.write("\t" + (function == null ? "-" : function.getEntryPoint().toString()));
                writer.write("\t" + clean(function == null ? null : function.getName()));
                writer.write("\t" + instructionText(instruction));
                writer.write("\t" + instructionText(previous));
                writer.write("\t" + instructionText(next));
                writer.write("\t" + referencesFrom);
                writer.write("\t" + referencesTo);
                writer.newLine();
            }
        }

        println("Address audit wrote " + (args.length - 1) + " records to " + output);
    }
}
