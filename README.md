### What is this

Tool to convert between Intel HEX, Binary and C Include format.

### Build

If using GCC then simply run `make`. Otherwise, you may need to setup different compile
flags. The source code itself is thought to be C99 portable.

### Use

```
Usage: hex2c [OPTION]... FILE
Convert between Intel HEX, Binary and C Include format.

-b, --binary        Binary dump output
-c, --c             C Include output
-x, --hex           Intel HEX format output
-i, --info          Only show file info
-o, --output=FILE   Set output file name
-z, --filler=XX     Suppress consecutive bytes in output
-p, --padding=NUM   Extra space on line
-w, --wrap=NUM      Maximum output bytes per line
-h, --help          Show this message and exit

If no output is given then writes to stdout.
```
