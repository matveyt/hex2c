### What is this

Tool to convert between Intel HEX, Binary and C Include format.

### Build

Run `make`.

### Use

```
Usage: hex2c [OPTION]... FILE
Convert between Intel HEX, Binary and C Include format.

-b, --binary        Binary dump output
-c, --c             C Include output
-x, --hex           Intel HEX format output
-o, --output=FILE   Set output file name
-p, --padding=NUM   Extra space on line
-w, --wrap=NUM      Maximum output bytes per line
-h, --help          Show this message and exit

If no output is given then writes to stdout.
Intel HEX format is 8-bit only (64KB max).
```
