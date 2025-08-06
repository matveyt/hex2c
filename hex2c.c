//
// hex2c
//
// Convert between Intel HEX, Binary and C Include
// Intel HEX: only data record type (00) supported, max size is 64KB
//
// $ make CFLAGS=-O LDFLAGS=-s hex2c
// $ hex2c file.ihx >file.h
//
// https://github.com/matveyt/hex2c
//

#include <ctype.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif // _WIN32

#define PROGRAM_NAME "hex2c"

// Intel HEX file
#define MAX_ADDRESS UINT16_MAX
#define MAX_COUNT   UINT8_MAX
#define EXTRA_BYTES 5       // count(1) + address(2) + type(1) + checksum(1)
#define MIN_LINE    (1 + 2 * EXTRA_BYTES)
#define MAX_LINE    (MIN_LINE + 2 * MAX_COUNT)
// C Include file
#define C_HEADER    "const uint8_t " PROGRAM_NAME "[%zu] = "
#define C_PADDING   4       // extra space on line
// min and max
#ifndef min
#define min(a, b)   (((a) < (b)) ? (a) : (b))
#endif // min
#ifndef max
#define max(a, b)   (((a) > (b)) ? (a) : (b))
#endif // max

/*noreturn*/
static void usage(void)
{
    fputs("\
Usage: " PROGRAM_NAME " [OPTION]... FILE\n\
Convert between Intel HEX, Binary and C Include format.\n\
\n\
-B, --from-binary   FILE has no specific format\n\
-H, --from-hex      FILE has Intel HEX format (default)\n\
-b, --binary        Binary dump output\n\
-c, --c             C Include output (default)\n\
-h, --hex           Intel HEX format output\n\
-o, --output=FILE   set output file name\n\
-w, --wrap=NUMBER   maximum output bytes per line\n\
\n\
If no --output is given then writes to stdout.\n\
Intel HEX format is 8-bit only (64KB max).\n\
", stdout);
    exit(EXIT_SUCCESS);
}

/*noreturn*/
static void die(const char* cause)
{
    fprintf(stderr, "Exiting due to error: '%s'\n", cause);
    exit(EXIT_FAILURE);
}

static FILE* xfopen(const char* fname, const char* mode)
{
    FILE* f = fopen(fname, mode);
    if (f == NULL)
        die("opening file");
    return f;
}

static void* xmalloc(size_t sz)
{
    void* p = malloc(sz);
    if (p == NULL)
        die("memory allocation");
    return memset(p, UINT8_MAX, sz);
}

static inline FILE* as_binary(FILE* f)
{
#if defined(_WIN32)
    _setmode(fileno(f), _O_BINARY);
#endif // _WIN32
    return f;
}

// convert hex number from string
static uint_fast32_t hex_scan(char line[], size_t length)
{
    char t = line[length];
    line[length] = '\0';
    uint_fast32_t number = strtoul(line, NULL, 16);
    line[length] = t;
    return number;
}
static inline uint_fast8_t hex_scan8(char line[])
{
    return hex_scan(line, 2);
}
static inline uint_fast16_t hex_scan16(char line[])
{
    return hex_scan(line, 4);
}

// parsed data chunk
typedef struct {
    uint8_t data[MAX_COUNT];
    size_t address, length;
} CHUNK;

// parse one Intel HEX line
// return record type or -1
static int hex_parse(CHUNK* pc, char line[])
{
    // init chunk
    pc->address = 0;
    pc->length = 0;

    // get line length
    size_t length = strlen(line);

    // empty line not allowed
    if (length == 0)
        return -1;

    // cut trailing newline
    if (line[length - 1] == '\n')
        --length;

    // check formatting
    if (length < MIN_LINE || line[0] != ':' || (length & 1) == 0)
        return -1;
    for (size_t i = 1; i < length; ++i)
        if (!isxdigit(line[i]))
            return -1;

    // get count, address and type
    uint_fast8_t count = hex_scan8(&line[1]);
    uint_fast16_t address = hex_scan16(&line[3]);
    uint_fast8_t type = hex_scan8(&line[7]);
    if ((size_t)MIN_LINE + 2 * count != length
        || (unsigned)address + count > MAX_ADDRESS + 1 || type > 5)
        return -1;

    // get data and checksum
    uint_fast8_t sum = count + (uint_fast8_t)(address >> 8) + (uint_fast8_t)address
        + type + hex_scan8(&line[length - 2]);
    for (size_t cb = 0, i = 9; i < length - 2; i += 2) {
        uint_fast8_t t = hex_scan8(&line[i]);
        pc->data[cb++] = t;
        sum += t;
    }
    // checksum mismatch
    if ((uint8_t)sum != 0)
        return -1;

    pc->address = address;
    pc->length = count;
    return (int)type;
}

// load binary file into memory
static size_t load_binary(uint8_t** bin, FILE* f)
{
    fseek(f, 0, SEEK_END);
    size_t sz = ftell(f);
    rewind(f);
    *bin = xmalloc(sz);
    return fread(*bin, 1, sz, f);
}

// load Intel HEX file as binary image
// return image size or 0
static size_t load_hex(uint8_t** bin, FILE* f)
{
    *bin = xmalloc(MAX_ADDRESS + 1);

    size_t sz = 0, lineno = 0;
    char line[MAX_LINE + 2];    // LF + NUL
    CHUNK chunk;

    while (fgets(line, sizeof(line), f) != NULL) {
        ++lineno;

        int type = hex_parse(&chunk, line);
        if (type < 0) {
            fprintf(stderr, "Warning: line %zu skipped\n", lineno);
            //continue;
        } else if (type == 0) {
            // store chunk into binary image
            // note: hex_parse() guarantees that we never get past MAX_ADDRESS
            memcpy(*bin + chunk.address, chunk.data, chunk.length);
            sz = max(sz, chunk.address + chunk.length);
        } else {
            // nothing
        }
    }

    // shrink memory block
    if (sz > 0)
        *bin = realloc(*bin, sz);
    else
        free(bin);
    return sz;
}

// dump binary data to file
static void dump_binary(uint8_t data[], size_t sz, FILE* f)
{
    fwrite(data, 1, sz, f);
}

// format output as C Include
static void dump_c(uint8_t data[], size_t sz, size_t wrap, FILE* f)
{
    // default wrap
    if (wrap == 0)
        wrap = 8;

    // header
    fprintf(f, C_HEADER "{\n", sz);

    for (size_t i = 0; i < sz; i += wrap) {
        // leading space
        fprintf(f, "%*c", C_PADDING, ' ');

        // data
        size_t cb = min(wrap, sz - i);
        for (size_t j = 0; j < cb; ++j)
            fprintf(f, "0x%02x, ", data[i + j]);

        // trailing space
        fprintf(f, "%*c// %03zx\n", (int)(wrap - cb) * 6 + C_PADDING, ' ', i);
    }

    // footer
    fputs("};\n", f);
}

// format output as Intel HEX file
static void dump_hex(uint8_t data[], size_t sz, size_t wrap, FILE* f)
{
    // 64KB max
    if (sz > MAX_ADDRESS + 1)
        sz = MAX_ADDRESS + 1;
    // default wrap
    if (wrap == 0)
        wrap = 16;

    for (size_t i = 0; i < sz; i += wrap) {
        uint_fast8_t cb = min(wrap, sz - i);
        // : count address type(00)
        fprintf(f, ":%02X%04zX00", cb, i);
        uint_fast8_t sum = cb + (uint_fast8_t)(i >> 8) + (uint_fast8_t)i;
        // data
        for (size_t j = 0; j < cb; ++j) {
            fprintf(f, "%02X", data[i + j]);
            sum += data[i + j];
        }
        // checksum
        fprintf(f, "%02X\n", (uint8_t)(-sum));
    }

    // EOF record
    fputs(":00000001FF\n", f);
}

// main program function
int main(int argc, char* argv[])
{
    static struct option lopts[] = {
        { "from-binary", no_argument, NULL, 'B' },
        { "from-hex", no_argument, NULL, 'H' },
        { "binary", no_argument, NULL, 'b' },
        { "c", no_argument, NULL, 'c' },
        { "hex", no_argument, NULL, 'h' },
        { "output", required_argument, NULL, 'o' },
        { "wrap", required_argument, NULL, 'w' },
        { NULL, 0, NULL, 0 }
    };

    int fmt_in = 'H', fmt_out = 'c';    // i/o format
    char* output = NULL;                // output file name
    size_t wrap = 0;                    // when to wrap output line

    int c;
    while ((c = getopt_long(argc, argv, "BHbcho:w:", lopts, NULL)) != -1) {
        switch (c) {
        case 'B':
        case 'H':
            fmt_in = c;
        break;
        case 'b':
        case 'c':
        case 'h':
            fmt_out = c;
        break;
        case 'o':
            free(output);
            output = strdup(optarg);
        break;
        case 'w':
            wrap = strtoul(optarg, NULL, 0);
            if (wrap > MAX_COUNT)
                wrap = 0;
        break;
        default:
            usage();
        }
    }

    // missing input file name
    if (optind != argc - 1)
        usage();

    // open files
    FILE* fin = xfopen(argv[optind], "r");
    FILE* fout = (output == NULL || strcmp(output, "-") == 0) ?
        stdout : xfopen(output, "w");

    // binary image
    uint8_t* bin = NULL;
    size_t sz = 0;

    // read in
    switch (fmt_in) {
    case 'B':
        sz = load_binary(&bin, as_binary(fin));
    break;
    case 'H':
        sz = load_hex(&bin, fin);
    break;
    }

    // write out
    if (sz > 0) {
        switch (fmt_out) {
        case 'b':
            dump_binary(bin, sz, as_binary(fout));
        break;
        case 'c':
            dump_c(bin, sz, wrap, fout);
        break;
        case 'h':
            dump_hex(bin, sz, wrap, fout);
        break;
        }
    }

    free(bin);
    free(output);
    fclose(fout);
    fclose(fin);
    return 0;
}
