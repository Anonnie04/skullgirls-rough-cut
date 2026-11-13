import java.io.BufferedWriter;
import java.nio.file.Files;
import java.nio.file.Path;
import java.io.PrintWriter;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;

public class ExportSkullgirls extends GhidraScript {
    @Override
    protected void run() throws Exception {
        Path output = Path.of(getScriptArgs()[0]);
        DecompInterface decompiler = new DecompInterface();
        DecompileOptions options = new DecompileOptions();
        options.grabFromProgram(currentProgram);
        decompiler.setOptions(options);
        decompiler.toggleSyntaxTree(false);

        try {
            if (!decompiler.openProgram(currentProgram)) {
                throw new IllegalStateException(decompiler.getLastMessage());
            }
            export(output, decompiler);
            exportAssetReferences(output);
        }
        finally {
            decompiler.dispose();
        }
    }

    private void exportAssetReferences(Path output) throws Exception {
        String[] addresses = {"007eff74", "007e61e0"};
        String[] strings = {"Reverge Package File", "characters-foits.gfs"};
        try (BufferedWriter writer = Files.newBufferedWriter(output.resolve("asset-references.tsv"))) {
            writer.write("string\tstring_address\treference_address\tfunction_address\tfunction_name\n");
            for (int i = 0; i < addresses.length; i++) {
                for (var reference : getReferencesTo(toAddr(addresses[i]))) {
                    var from = reference.getFromAddress();
                    Function function = getFunctionContaining(from);
                    String target = function == null ? "\t" :
                        function.getEntryPoint() + "\t" + function.getName(true);
                    writer.write(strings[i] + "\t" + addresses[i] + "\t" + from + "\t" + target + "\n");
                }
            }
        }
    }

    private void export(Path output, DecompInterface decompiler) throws Exception {
        int exported = 0;
        int failed = 0;
        int thunks = 0;
        try (PrintWriter cpp = new PrintWriter(Files.newBufferedWriter(output.resolve("SkullGirls.cpp")));
             BufferedWriter index = Files.newBufferedWriter(output.resolve("functions.tsv"))) {
            cpp.println("// Ghidra-generated pseudocode for " + currentProgram.getName());
            cpp.println("// Executable SHA-256 " + currentProgram.getExecutableSHA256());
            cpp.println("// Image base " + currentProgram.getImageBase() + ". Intended for analysis.");
            index.write("address\tstatus\tname\tdetail\n");

            for (Function function : currentProgram.getFunctionManager().getFunctions(true)) {
                monitor.checkCancelled();
                String address = function.getEntryPoint().toString();
                String name = function.getName(true);
                if (function.isThunk()) {
                    thunks++;
                    index.write(address + "\tthunk\t" + name + "\t\n");
                    continue;
                }
                DecompileResults result = decompiler.decompileFunction(function, 20, monitor);
                if (result.decompileCompleted() && result.getDecompiledFunction() != null) {
                    cpp.printf("%n// %s %s%n", address, name);
                    cpp.println(result.getDecompiledFunction().getC());
                    index.write(address + "\texported\t" + name + "\t\n");
                    exported++;
                }
                else {
                    String detail = result.getErrorMessage().replaceAll("[\\r\\n\\t]+", " ").trim();
                    index.write(address + "\tfailed\t" + name + "\t" + detail + "\n");
                    failed++;
                }
                if ((exported + failed) % 2000 == 0) {
                    println("Processed " + (exported + failed) + " functions");
                }
            }
            if (cpp.checkError()) {
                throw new java.io.IOException("Could not write pseudocode");
            }
        }
        println("Exported " + exported + ", failed " + failed + ", thunks " + thunks);
    }
}
