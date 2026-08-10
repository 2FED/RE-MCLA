// Headless Ghidra script: audit Xenon helper signatures, PDATA, and selected control-flow sites.
// @category MCLA-R

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSpace;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.util.ArrayList;
import java.util.List;

public class ControlFlowAudit extends GhidraScript {
    private static final String[] SITES = {
        "82203F90", "822B88C8", "824AF4D0", "824B0DE8", "8220DA7C", "8220C018",
        "8220DAF4", "8220BF08", "822C9E04", "822C98B8", "823FB7F4", "823F32E8",
        "823FDB24", "823FD718"
    };
    private static final String[] TARGETS = {
        "822B88C8", "824B0DE8", "8220C018", "8220BF08", "822C98B8", "823F32E8", "823FD718"
    };

    private static final class RuntimeFunction {
        long begin;
        long end;
        int prologLength;
        boolean thirtyTwoBit;
        boolean exceptionFlag;
    }

    private long u32(Address address) throws Exception {
        return Integer.toUnsignedLong(currentProgram.getMemory().getInt(address));
    }

    private boolean wordsMatch(Address start, long[] words) {
        try {
            for (int index = 0; index < words.length; index++) {
                if (u32(start.add(index * 4L)) != words[index]) {
                    return false;
                }
            }
            return true;
        } catch (Exception error) {
            return false;
        }
    }

    private List<Address> findWords(long[] words) {
        List<Address> matches = new ArrayList<>();
        for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
            if (!block.isExecute()) {
                continue;
            }
            for (Address cursor = block.getStart(); cursor.compareTo(block.getEnd()) <= 0; cursor = cursor.add(4)) {
                if (cursor.add((words.length - 1L) * 4L).compareTo(block.getEnd()) > 0) {
                    break;
                }
                if (wordsMatch(cursor, words)) {
                    matches.add(cursor);
                }
            }
        }
        return matches;
    }

    private long[] fprSequence(boolean store, int baseRegister) {
        long[] words = new long[18];
        long opcode = store ? 0xD8000000L : 0xC8000000L;
        for (int register = 14; register <= 31; register++) {
            int index = register - 14;
            words[index] = opcode | ((long)register << 21) | ((long)baseRegister << 16) | index * 8L;
        }
        return words;
    }

    private void writeMatches(BufferedWriter writer, String type, List<Address> matches) throws Exception {
        if (matches.isEmpty()) {
            writer.write("signature\t" + type + "\tNONE");
            writer.newLine();
            return;
        }
        for (Address match : matches) {
            writer.write("signature\t" + type + "\t" + match);
            writer.newLine();
        }
    }

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) {
            throw new IllegalArgumentException("Usage: ControlFlowAudit.java OUTPUT");
        }
        File output = new File(args[0]).getCanonicalFile();
        File parent = output.getParentFile();
        if (parent == null || !parent.isDirectory()) {
            throw new IllegalArgumentException("Output parent does not exist: " + output);
        }

        Memory memory = currentProgram.getMemory();
        MemoryBlock pdata = null;
        for (MemoryBlock block : memory.getBlocks()) {
            if (block.getName().equals(".pdata")) {
                pdata = block;
                break;
            }
        }
        if (pdata == null || pdata.getSize() % 8 != 0) {
            throw new IllegalStateException("Expected an 8-byte-record .pdata block");
        }

        long moduleBase = Long.MAX_VALUE;
        for (MemoryBlock block : memory.getBlocks()) {
            moduleBase = Math.min(moduleBase, block.getStart().getUnsignedOffset());
        }
        moduleBase &= 0xFFFF0000L;

        List<RuntimeFunction> runtimeFunctions = new ArrayList<>();
        for (long offset = 0; offset < pdata.getSize(); offset += 8) {
            long begin = u32(pdata.getStart().add(offset));
            if (begin < moduleBase) {
                begin += moduleBase;
            }
            long data = u32(pdata.getStart().add(offset + 4));
            long functionLength = (data >>> 8) & 0x3FFFFFL;
            RuntimeFunction function = new RuntimeFunction();
            function.begin = begin;
            function.end = begin + Math.max(1L, functionLength) * 4L;
            function.prologLength = (int)(data & 0xFFL);
            function.thirtyTwoBit = ((data >>> 30) & 1L) != 0;
            function.exceptionFlag = ((data >>> 31) & 1L) != 0;
            runtimeFunctions.add(function);
        }

        long exceptionCount = runtimeFunctions.stream().filter(function -> function.exceptionFlag).count();
        try (BufferedWriter writer = new BufferedWriter(new FileWriter(output))) {
            writer.write("program\t" + currentProgram.getName());
            writer.newLine();
            writer.write("pdata\t" + pdata.getStart() + "\t" + pdata.getEnd() + "\t" + pdata.getSize() +
                "\t" + runtimeFunctions.size() + "\t" + exceptionCount);
            writer.newLine();

            writeMatches(writer, "savegprlr_14", findWords(new long[]{0xF9C1FF68L}));
            writeMatches(writer, "restgprlr_14", findWords(new long[]{0xE9C1FF68L}));
            writeMatches(writer, "savefpr_14", findWords(new long[]{0xD9CCFF70L}));
            writeMatches(writer, "restfpr_14", findWords(new long[]{0xC9CCFF70L}));
            writeMatches(writer, "savevmx_14", findWords(new long[]{0x3960FEE0L, 0x7DCB61CEL}));
            writeMatches(writer, "restvmx_14", findWords(new long[]{0x3960FEE0L, 0x7DCB60CEL}));
            writeMatches(writer, "savevmx_64", findWords(new long[]{0x3960FC00L, 0x100B61CBL}));
            writeMatches(writer, "restvmx_64", findWords(new long[]{0x3960FC00L, 0x100B60CBL}));
            writeMatches(writer, "setjmp_fpr_store_r3", findWords(fprSequence(true, 3)));
            writeMatches(writer, "longjmp_fpr_load_r3", findWords(fprSequence(false, 3)));
            writeMatches(writer, "longjmp_fpr_load_r7", findWords(fprSequence(false, 7)));

            AddressSpace space = currentProgram.getAddressFactory().getDefaultAddressSpace();
            for (String siteText : SITES) {
                Address siteAddress = space.getAddress(siteText);
                long site = siteAddress.getUnsignedOffset();
                RuntimeFunction containing = null;
                RuntimeFunction predecessor = null;
                RuntimeFunction successor = null;
                for (RuntimeFunction function : runtimeFunctions) {
                    if (site >= function.begin && site < function.end) {
                        containing = function;
                        break;
                    }
                    if (function.begin <= site &&
                        (predecessor == null || function.begin > predecessor.begin)) {
                        predecessor = function;
                    }
                    if (function.begin > site &&
                        (successor == null || function.begin < successor.begin)) {
                        successor = function;
                    }
                }
                if (containing == null) {
                    writer.write(String.format("site\t%s\tGAP\t%08X\t%08X\t%08X", siteAddress,
                        predecessor == null ? 0 : predecessor.begin,
                        predecessor == null ? 0 : predecessor.end,
                        successor == null ? 0 : successor.begin));
                } else {
                    writer.write(String.format("site\t%s\t%08X\t%08X\t%s\t%d\t%s\t%s", siteAddress,
                        containing.begin, containing.end, site == containing.begin, containing.prologLength,
                        containing.thirtyTwoBit, containing.exceptionFlag));
                }
                writer.newLine();
            }

            for (RuntimeFunction function : runtimeFunctions) {
                if (function.exceptionFlag) {
                    writer.write(String.format("exception\t%08X\t%08X\t%d\t%s", function.begin,
                        function.end, function.prologLength, function.thirtyTwoBit));
                    writer.newLine();
                }
            }

            Listing listing = currentProgram.getListing();
            for (String targetText : TARGETS) {
                Address cursor = space.getAddress(targetText);
                Address terminal = null;
                String terminalMnemonic = "LIMIT";
                int instructionCount = 0;
                for (; instructionCount < 256; instructionCount++) {
                    Instruction instruction = listing.getInstructionAt(cursor);
                    if (instruction == null) {
                        disassemble(cursor);
                        instruction = listing.getInstructionAt(cursor);
                    }
                    if (instruction == null) {
                        terminalMnemonic = "UNDECODED";
                        terminal = cursor;
                        break;
                    }
                    if ((instruction.getFlowType().isTerminal() && !instruction.getFlowType().isConditional()) ||
                        (instruction.getFlowType().isJump() && !instruction.getFlowType().isConditional())) {
                        terminal = cursor;
                        terminalMnemonic = instruction.getMnemonicString();
                        instructionCount++;
                        break;
                    }
                    cursor = cursor.add(instruction.getLength());
                }
                writer.write("target_flow\t" + targetText.toLowerCase() + "\t" +
                    (terminal == null ? "NONE" : terminal.toString()) + "\t" + terminalMnemonic + "\t" +
                    instructionCount);
                writer.newLine();
            }
        }

        println("Control-flow audit wrote " + runtimeFunctions.size() + " PDATA records to " + output);
    }
}
