// Apply recovered GFS loader names and explanatory comments to the open Ghidra program.

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;

public class AnnotateGfs extends GhidraScript {
    @Override
    protected void run() throws Exception {
        // These virtual addresses refer to the Steam build listed in GFS-DECOMPILATION.md.
        annotate("004e8370", "parse_package_header",
            "Name recovered from the embedded re::gfs diagnostic. Returns an MSB reader and writes entry count and first payload offset.");
        annotate("004ae950", "read_msb8_as_u32",
            "Reads eight big-endian bytes. Rejects a nonzero upper word and returns the lower word.");
        annotate("004ae7e0", "read_msb_string",
            "Reads a length through read_msb8_as_u32, then reads the string bytes.");
        annotate("004e51d0", "parse_gfs_entry_table",
            "Calls parse_package_header. Reads name, size and alignment per entry. Rounds the running payload offset up to alignment and passes the entry to a callback.");
        annotate("004e5860", "append_gfs_entry_callback",
            "Null name requests capacity for the entry count. Non-null name appends an entry to the 0x28-byte element array.");
    }

    // Resolve the function at its entry address before updating Ghidra's listing.
    private void annotate(String address, String name, String comment) throws Exception {
        Function function = getFunctionAt(toAddr(address));
        if (function == null) {
            throw new IllegalStateException("Missing function at " + address);
        }
        function.setName(name, SourceType.USER_DEFINED);
        function.setComment(comment);
        println(address + " " + name);
    }
}
