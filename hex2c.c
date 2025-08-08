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
#include <stdbool.h>
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
#define MIN_LINE    (1 + 2 * 5) // count(1) + address(2) + type(1) + checksum(1)
#define MAX_LINE    (MIN_LINE + 2 * UINT8_MAX)
// C Include file
#define C_HEADER    "const uint8_t " PROGRAM_NAME "[%zu] = "
// min and max
#ifndef min
#define min(a, b)   (((a) < (b)) ? (a) : (b))
#endif // min
#ifndef max
#define max(a, b)   (((a) > (b)) ? (a) : (b))
#endif // max

// user options
unsigned o_padding = 0;
bool o_silent = false;
unsigned o_wrap = 0;

/*noreturn*/
void usage(void)
{
    fputs("\
Usage: " PROGRAM_NAME " [OPTION]... FILE\n\
Convert between Intel HEX, Binary and C Include format.\n\
\n\
-B, --from-binary   FILE has no specific format\n\
-H, --from-hex      FILE has Intel HEX format [default]\n\
-b, --binary        Binary dump output\n\
-c, --c             C Include output [default]\n\
-h, --hex           Intel HEX format output\n\
-o, --output=FILE   set output file name\n\
-p, --padding=NUM   extra space on line\n\
-s, --silent        suppress messages\n\
-w, --wrap=NUM      maximum output bytes per line\n\
\n\
If no --output is given then writes to stdout.\n\
Intel HEX format is 8-bit only (64KB max).\n\
", stdout);
    exit(EXIT_SUCCESS);
}

void warn(unsigned lineno, const char* msg)
{
    if (!o_silent)
        fprintf(stderr, "Warning (line %u): %s\n", lineno, msg);
}

/*noreturn*/
void die(const char* msg)
{
    if (!o_silent)
        fprintf(stderr, "Exiting due to error: '%s'\n", msg);
    exit(EXIT_FAILURE);
}

FILE* xfopen(const char* fname, const char* mode)
{
    FILE* f = fopen(fname, mode);
    if (f == NULL)
        die("opening file");
    return f;
}

void* xmalloc(size_t sz)
{
    void* p = malloc(sz);
    if (p == NULL)
        die("memory allocation");
    return p;
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
    uint8_t data[UINT8_MAX];
    size_t address, length;
} CHUNK;

// parse one Intel HEX line
// return record type or -1
int hex_parse(CHUNK* pc, char line[])
{
    // init chunk
    pc->address = 0;
    pc->length = 0;

    // empty line not allowed
    size_t length = strlen(line);
    if (length == 0 || line[0] != ':')
        return -1;

    // cut newline character
    if (line[length - 1] == '\n')
        --length;
    if (line[length - 1] == '\r')
        --length;

    // check number of hex digits
    if (length < MIN_LINE || (length & 1) == 0)
        return -1;
    for (size_t i = 1; i < length; ++i)
        if (!isxdigit(line[i]))
            return -1;

    // get count, address and type
    uint_fast8_t count = hex_scan8(&line[1]);
    uint_fast16_t address = hex_scan16(&line[3]);
    uint_fast8_t type = hex_scan8(&line[7]);
    if ((size_t)MIN_LINE + 2 * count != length
        || (unsigned)address + count > MAX_ADDRESS + 1)
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

// load Intel HEX file as binary image
// return image size or 0
size_t load_hex(uint8_t** bin, FILE* f)
{
    *bin = memset(xmalloc(MAX_ADDRESS + 1), UINT8_MAX, MAX_ADDRESS + 1);
    size_t sz = 0;
    bool found_eof = false;

    for (unsigned lineno = 1; !found_eof; ++lineno) {
        char line[MAX_LINE + 3];    // CR+LF+NUL
        if (fgets(line, sizeof(line), f) == NULL) {
            warn(lineno, "no EOF record");
            break;
        }

        CHUNK chunk;
        switch (hex_parse(&chunk, line)) {
        case 0:
            // hex_parse() ensures that we never get past MAX_ADDRESS
            memcpy(*bin + chunk.address, chunk.data, chunk.length);
            sz = max(sz, chunk.address + chunk.length);
        break;
        case 1:
            found_eof = true;
        break;
        case 2:
        case 3:
        case 4:
        case 5:
            warn(lineno, "extended record");
        break;
        default:
            warn(lineno, "invalid record");
        break;
        }
    }

    // shrink memory block
    if (sz > 0)
        *bin = realloc(*bin, sz);
    else
        free(bin);
    return sz;
}

// format output as Intel HEX file
static void dump_hex(uint8_t data[], size_t sz, FILE* f)
{
    // 64KB max
    sz = min(sz, MAX_ADDRESS + 1);
    // user options
    unsigned wrap = (o_wrap != 0) ? o_wrap : 16;

    for (size_t i = 0; i < sz; i += wrap) {
        unsigned cb = min(wrap, sz - i);
        // : count address type(00)
        fprintf(f, ":%02X%04zX00", cb, i);
        uint_fast8_t sum = cb + (i >> 8) + i;
        // data
        for (unsigned j = 0; j < cb; ++j) {
            fprintf(f, "%02X", data[i + j]);
            sum += data[i + j];
        }
        // checksum
        fprintf(f, "%02X\n", (uint8_t)(-sum));
    }

    // EOF record
    fputs(":00000001FF\n", f);
}

// format output as C Include
static void dump_c(uint8_t data[], size_t sz, FILE* f)
{
    // user options
    unsigned wrap = (o_wrap != 0) ? o_wrap : 8;
    unsigned padding = (o_padding != 0) ? o_padding : 4;

    // header
    fprintf(f, C_HEADER "{\n", sz);

    for (size_t i = 0; i < sz; i += wrap) {
        // leading space
        fprintf(f, "%*c", padding, ' ');

        // data
        unsigned cb = min(wrap, sz - i);
        for (unsigned j = 0; j < cb; ++j)
            fprintf(f, "0x%02x, ", data[i + j]);

        // trailing space
        fprintf(f, "%*c// %03zx\n", (wrap - cb) * 6 - 1 + padding, ' ', i);
    }

    // footer
    fputs("};\n", f);
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
        { "padding", required_argument, NULL, 'p' },
        { "silent", no_argument, NULL, 's' },
        { "wrap", required_argument, NULL, 'w' },
        { NULL, 0, NULL, 0 }
    };

    int fmt_in = 'H', fmt_out = 'c';    // i/o format
    char* output = NULL;                // output file name

    int c;
    while ((c = getopt_long(argc, argv, "BHbcho:p:sw:", lopts, NULL)) != -1) {
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
            //output = strdup(optarg);
            output = strcpy(xmalloc(strlen(optarg) + 1), optarg);
        break;
        case 'p':
            o_padding = strtoul(optarg, NULL, 0);
            if (o_padding > UINT8_MAX)
                o_padding = 0;
        break;
        case 's':
            o_silent = true;
        break;
        case 'w':
            o_wrap = strtoul(optarg, NULL, 0);
            if (o_wrap > UINT8_MAX)
                o_wrap = 0;
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
        as_binary(fin);
        fseek(fin, 0, SEEK_END);
        sz = ftell(fin);
        bin = xmalloc(sz);
        rewind(fin);
        sz = fread(bin, 1, sz, fin);
    break;
    case 'H':
        sz = load_hex(&bin, fin);
    break;
    }

    // write out
    if (sz > 0) {
        switch (fmt_out) {
        case 'b':
            fwrite(bin, 1, sz, as_binary(fout));
        break;
        case 'c':
            dump_c(bin, sz, fout);
        break;
        case 'h':
            dump_hex(bin, sz, fout);
        break;
        }
    }

    free(bin);
    free(output);
    fclose(fout);
    fclose(fin);
    return 0;
}
