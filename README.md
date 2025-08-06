### What is this

Tool to convert between Intel HEX, Binary and C Include format.

### Build

Run `make`.

### Use

```
Usage: hex2c [OPTION]... FILE
Convert between Intel HEX, Binary and C Include format.

-B, --from-binary   FILE has no specific format
-H, --from-hex      FILE has Intel HEX format (default)
-b, --binary        Binary dump output
-c, --c             C Include output (default)
-h, --hex           Intel HEX format output
-o, --output=FILE   set output file name
-w, --wrap=NUMBER   maximum output bytes per line

If no --output is given then writes to stdout.
Intel HEX format is 8-bit only (64KB max).
```
