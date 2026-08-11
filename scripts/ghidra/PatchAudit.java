// Headless Ghidra script: export exact loaded-image bytes for reviewed public patches.
// @category MCLA-R

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSpace;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.Listing;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;

public class PatchAudit extends GhidraScript {
    private static final String[][] SITES = {
        { "821bdb08", "4" }, { "82419aa3", "1" }, { "821f7f64", "4" },
        { "8260d0b8", "4" }, { "8260d0d4", "4" }, { "8230c87c", "2" },
        { "822e4b80", "4" }, { "821bd618", "4" }
    };

    private String bytesAt(Address address, int length) throws Exception {
        byte[] bytes = new byte[length];
        int count = currentProgram.getMemory().getBytes(address, bytes);
        if (count != length) {
            throw new IllegalStateException("Short memory read at " + address + ": " + count + "/" + length);
        }
        StringBuilder result = new StringBuilder(length * 2);
        for (byte value : bytes) {
            result.append(String.format("%02X", value & 0xff));
        }
        return result.toString();
    }

    private static String clean(String value) {
        return value == null ? "-" : value.replace('\t', ' ').replace('\r', ' ').replace('\n', ' ');
    }

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) {
            throw new IllegalArgumentException("Usage: PatchAudit.java OUTPUT");
        }
        File output = new File(args[0]).getCanonicalFile();
        File parent = output.getParentFile();
        if (parent == null || !parent.isDirectory()) {
            throw new IllegalArgumentException("Output parent does not exist: " + output);
        }

        AddressSpace space = currentProgram.getAddressFactory().getDefaultAddressSpace();
        Listing listing = currentProgram.getListing();
        try (BufferedWriter writer = new BufferedWriter(new FileWriter(output))) {
            writer.write("program\t" + clean(currentProgram.getName()));
            writer.newLine();
            writer.write("image_base\t" + currentProgram.getImageBase());
            writer.newLine();
            writer.write("address\tlength\tbytes\tword_address\tword_bytes\tinstruction\tfunction_entry\tfunction_name");
            writer.newLine();
            for (String[] site : SITES) {
                Address address = space.getAddress(site[0]);
                int length = Integer.parseInt(site[1]);
                if (address == null || !currentProgram.getMemory().contains(address)) {
                    throw new IllegalStateException("Patch address is not mapped: " + site[0]);
                }
                long alignedOffset = address.getOffset() & ~3L;
                Address wordAddress = space.getAddress(alignedOffset);
                Instruction instruction = listing.getInstructionAt(wordAddress);
                if (instruction == null) {
                    disassemble(wordAddress);
                    instruction = listing.getInstructionAt(wordAddress);
                }
                Function function = currentProgram.getFunctionManager().getFunctionContaining(wordAddress);
                writer.write(address + "\t" + length + "\t" + bytesAt(address, length));
                writer.write("\t" + wordAddress + "\t" + bytesAt(wordAddress, 4));
                writer.write("\t" + clean(instruction == null ? null : instruction.toString()));
                writer.write("\t" + (function == null ? "-" : function.getEntryPoint().toString()));
                writer.write("\t" + clean(function == null ? null : function.getName()));
                writer.newLine();
            }
        }
        println("Patch audit wrote " + SITES.length + " records to " + output);
    }
}
