//
// hex2c
//
// Convert between Intel HEX, Binary and C Include format
// Intel HEX: only data record type (00) supported (64KB max)
//
// https://github.com/matveyt/hex2c
//

#define _DEFAULT_SOURCE
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

// HEX format
#define MAX_SIZE    (UINT16_MAX + 1)
#define MIN_LINE    11  // colon(1) count(2) address(4) type(2) checksum(2)
#define MAX_LINE    (MIN_LINE + 2 * UINT8_MAX)

const char program_name[] = "hex2c";

// user options
unsigned o_padding = 0;
bool o_silent = false;
unsigned o_wrap = 0;

/*noreturn*/
void usage(void)
{
    fprintf(stdout,
"Usage: %s [OPTION]... FILE\n"
"Convert between Intel HEX, Binary and C Include format.\n"
"\n"
"-B, --from-binary   FILE has no specific format\n"
"-H, --from-hex      FILE has Intel HEX format [default]\n"
"-b, --binary        Binary dump output\n"
"-c, --c             C Include output [default]\n"
"-h, --hex           Intel HEX format output\n"
"-o, --output=FILE   set output file name\n"
"-p, --padding=NUM   extra space on line\n"
"-s, --silent        suppress messages\n"
"-w, --wrap=NUM      maximum output bytes per line\n"
"\n"
"If no --output is given then writes to stdout.\n"
"Intel HEX format is 8-bit only (64KB max).\n",
        program_name);
    exit(EXIT_FAILURE);
}

void warn(unsigned lineno, const char msg[])
{
    if (!o_silent)
        fprintf(stderr, "Warning (line %u): %s\n", lineno, msg);
}

/*noreturn*/
void die(const char msg[])
{
    if (!o_silent)
        fprintf(stderr, "Error exit: %s\n", msg);
    exit(EXIT_FAILURE);
}

static inline size_t xmin(size_t x, size_t y)
{
    return (x < y) ? x : y;
}
static inline size_t xmax(size_t x, size_t y)
{
    return (x > y) ? x : y;
}

FILE* xfopen(const char fname[], const char mode[])
{
    FILE* f = fopen(fname, mode);
    if (f == NULL)
        die("open file");
    return f;
}

void* xmalloc(size_t sz)
{
    void* p = malloc(sz);
    if (p == NULL)
        die("memory allocation");
    return p;
}

// read hex number from string
unsigned hex_scan(char line[], unsigned length)
{
    char t = line[length];
    line[length] = '\0';
    unsigned number = (unsigned)strtoul(line, NULL, 16);
    line[length] = t;
    return number;
}
static inline unsigned hex_scan8(char line[])
{ return hex_scan(line, 2); }
static inline unsigned hex_scan16(char line[])
{ return hex_scan(line, 4); }

// parsed data chunk
typedef struct {
    uint8_t data[UINT8_MAX + 1];    // checksum
    unsigned length;
    size_t address;
} CHUNK;

// parse one Intel HEX line
// return record type or -1
int hex_parse(CHUNK* pc, char line[])
{
    // init chunk
    pc->length = 0;
    pc->address = 0;

    // empty line not allowed
    unsigned length = strlen(line);
    if (length == 0 || line[0] != ':')
        return -1;

    // cut newline character
    if (line[length - 1] == '\n')
        --length;
    if (line[length - 1] == '\r')
        --length;

    // check number of hex digits
    if (length < MIN_LINE || length > MAX_LINE || !(length & 1))
        return -1;
    for (unsigned i = 1; i < length; ++i)
        if (!isxdigit(line[i]))
            return -1;

    // get count, address and type
    unsigned count = hex_scan8(&line[1]);
    unsigned address = hex_scan16(&line[3]);
    unsigned type = hex_scan8(&line[7]);
    if (length - 2 * count != MIN_LINE || address + count > MAX_SIZE)
        return -1;

    // get data and checksum
    unsigned sum = count + (address >> 8) + address + type;
    for (unsigned i = 0, j = 9; i <= count; ++i, j += 2) {
        unsigned t = hex_scan8(&line[j]);
        pc->data[i] = t;
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
    *bin = memset(xmalloc(MAX_SIZE), UINT8_MAX, MAX_SIZE);
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
            // hex_parse() ensures that we never get past MAX_SIZE
            memcpy(*bin + chunk.address, chunk.data, chunk.length);
            sz = xmax(sz, chunk.address + chunk.length);
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
        free(*bin);
    return sz;
}

// format output as Intel HEX file
void dump_hex(uint8_t data[], size_t sz, FILE* f)
{
    // 64KB max
    sz = xmin(sz, MAX_SIZE);
    // user options
    unsigned wrap = (o_wrap != 0) ? o_wrap : 16;

    for (size_t i = 0; i < sz; i += wrap) {
        unsigned cb = xmin(wrap, sz - i);
        // : count address type(00)
        fprintf(f, ":%02X%04zX00", cb, i);
        unsigned sum = cb + (i >> 8) + i;
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
void dump_c(uint8_t data[], size_t sz, FILE* f)
{
    // user options
    unsigned wrap = (o_wrap != 0) ? o_wrap : 8;
    unsigned padding = (o_padding != 0) ? o_padding : 4;

    // header
    fprintf(f,
"// made with %s\n"
"const uint8_t %s_data[%zu] = {\n",
        program_name, program_name, sz);

    for (size_t i = 0; i < sz; i += wrap) {
        // leading space
        fprintf(f, "%*c", padding, ' ');

        // data
        unsigned cb = xmin(wrap, sz - i);
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
            output = strdup(optarg);
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
    free(output);

    // binary image
    uint8_t* bin = NULL;
    size_t sz = 0;

    // read in
    switch (fmt_in) {
    case 'B':
#if defined(_WIN32)
        _setmode(fileno(fin), _O_BINARY);
#endif // _WIN32
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
#if defined(_WIN32)
            _setmode(fileno(fout), _O_BINARY);
#endif // _WIN32
            fwrite(bin, 1, sz, fout);
        break;
        case 'c':
            dump_c(bin, sz, fout);
        break;
        case 'h':
            dump_hex(bin, sz, fout);
        break;
        }
        free(bin);
    }

    fclose(fout);
    fclose(fin);
    return 0;
}
