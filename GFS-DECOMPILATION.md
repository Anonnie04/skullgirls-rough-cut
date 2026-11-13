# GFS loader in SkullGirls.exe

Build 26588330, image base 0x00400000. Addresses below are virtual addresses.
The Ghidra project contains the labels and comments from
`scripts/AnnotateGfs.java`. The original pseudocode export retains the
original `FUN_` labels.

| Address | Ghidra label | Evidence |
|---|---|---|
| 004e8370 | parse_package_header | Embedded C++ diagnostic gives the name and identifies the return type as `boost::shared_ptr<re::msb_reader>`. The function rejects a zero first-payload offset, checks the exact signature and version, then reads the entry count. |
| 004ae950 | read_msb8_as_u32 | Reads eight bytes, rejects any nonzero byte in the first four, and returns the last four in big-endian order. Its embedded diagnostic names `decode_msb_8<0x4>`. |
| 004ae7e0 | read_msb_string | Uses the eight-byte reader for the string length before reading the payload. |
| 004e51d0 | parse_gfs_entry_table | Calls the header parser, invokes a callback with a null name and the entry count, then invokes it once per entry with name, aligned offset, size, and alignment. |
| 004e5860 | append_gfs_entry_callback | Null name selects the capacity path. A non-null name appends to a vector with 0x28-byte elements. |

The table loop reads the name through 004ae7e0, the size through 004ae950,
and alignment as four big-endian bytes. On this x86 build its offset
arithmetic is 32-bit. It computes `rounded = offset + alignment - 1`,
subtracts `rounded % alignment`, then advances the aligned offset by the
entry size. The portable C++ reader uses wider arithmetic and validates
against the file length, so it differs from the game's reader for some
out-of-range inputs.

Only the header parser name was restored directly from an embedded symbol
string. The other labels describe observed behavior. Ghidra's argument
recovery for the header call and callback is incomplete. The exact class
types and calling conventions still need recovery.
