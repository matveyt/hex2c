//
// hex2c
//
// Convert between Intel HEX, Binary and C Include format
// Maximum image size is 64KB
//
// https://github.com/matveyt/hex2c
//

#include "stdz.h"
#include <fcntl.h>
#include <getopt.h>
#if defined(_WIN32)
#include <io.h>
#endif // _WIN32
#include "ihex.h"

const char program_name[] = "hex2c";

// user options
struct {
    int fmt;
    unsigned padding;
    unsigned wrap;
    char* input;
    char* output;
} opt = {0};

/*noreturn*/
void help(void)
{
    fprintf(stdout,
"Usage: %s [OPTION]... FILE\n"
"Convert between Intel HEX, Binary and C Include format.\n"
"\n"
"-b, --binary       Binary dump output\n"
"-c, --c            C Include output\n"
"-x, --hex          Intel HEX format output\n"
"-o, --output=FILE  Set output file name\n"
"-p, --padding=NUM  Extra space on line\n"
"-w, --wrap=NUM     Maximum output bytes per line\n"
"-h, --help         Show this message and exit\n"
"\n"
"If no output is given then writes to stdout.\n"
"Intel HEX format is 8-bit only (64KB max).\n",
        program_name);
    exit(EXIT_SUCCESS);
}

void parse_args(int argc, char* argv[])
{
    static struct option lopts[] = {
        { "binary", no_argument, NULL, 'b' },
        { "c", no_argument, NULL, 'c' },
        { "hex", no_argument, NULL, 'x' },
        { "output", required_argument, NULL, 'o' },
        { "padding", required_argument, NULL, 'p' },
        { "wrap", required_argument, NULL, 'w' },
        { "help", no_argument, NULL, 'h'},
        {0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "bcxo:p:w:h", lopts, NULL)) != -1) {
        switch (c) {
        case 'b':
        case 'c':
        case 'x':
            opt.fmt = c;
        break;
        case 'o':
            free(opt.output);
            opt.output = z_strdup(optarg);
        break;
        case 'p':
            opt.padding = strtoul(optarg, NULL, 0);
            if (opt.padding > UINT8_MAX)
                opt.padding = 0;
        break;
        case 'w':
            opt.wrap = strtoul(optarg, NULL, 0);
            if (opt.wrap > UINT8_MAX)
                opt.wrap = 0;
        break;
        case 'h':
            help();
        break;
        case '?':
            fprintf(stderr, "Try '%s --help' for more information.\n", program_name);
            exit(EXIT_FAILURE);
        break;
        }
    }

    if (optind == argc - 1)
        opt.input = z_strdup(argv[optind]);
    else
        help();
}

// format output as Intel HEX file
void dump_hex(uint8_t* image, size_t sz, size_t base, size_t entry, FILE* f)
{
    // user options
    unsigned wrap = (opt.wrap != 0) ? opt.wrap : 16;

    for (size_t i = 0; i < sz; i += wrap) {
        unsigned cb = min(wrap, sz - i);
        // : count address type(00)
        fprintf(f, ":%02X%04zX00", cb, base + i);
        unsigned sum = cb + (base >> 8) + base + (i >> 8) + i;
        // data
        for (unsigned j = 0; j < cb; ++j) {
            fprintf(f, "%02X", image[i + j]);
            sum += image[i + j];
        }
        // checksum
        fprintf(f, "%02X\n", (uint8_t)(-sum));
    }

    if (entry > 0) {
        unsigned hi = (uint8_t)(entry >> 8);
        unsigned lo = (uint8_t)(entry);
        unsigned sum = 4 + 3 + hi + lo;
        fprintf(f, ":040000030000%02X%02X%02X\n", hi, lo, (uint8_t)(-sum));
    }

    // EOF record
    fputs(":00000001FF\n", f);
}

// format output as C Include
void dump_c(uint8_t* image, size_t sz, size_t base, size_t entry, FILE* f)
{
    // user options
    unsigned wrap = (opt.wrap != 0) ? opt.wrap : 8;
    unsigned padding = (opt.padding != 0) ? opt.padding : 4;

    // header
    fprintf(f, "// made with %s\n", program_name);
    if (base > 0)
        fprintf(f, "// image base 0x%04zx\n", base);
    if (entry > base)
        fprintf(f, "// entry point 0x%04zx\n", entry);
    fprintf(f, "const uint8_t %s_image[%zu] = {\n", program_name, sz);

    for (size_t i = 0; i < sz; i += wrap) {
        // leading space
        fprintf(f, "%*c", padding, ' ');

        // data
        unsigned cb = min(wrap, sz - i);
        for (unsigned j = 0; j < cb; ++j)
            fprintf(f, "0x%02x, ", image[i + j]);

        // trailing space
        fprintf(f, "%*c// %03zx\n", (wrap - cb) * 6 - 1 + padding, ' ', base + i);
    }

    // footer
    fputs("};\n", f);
}

// main program function
int main(int argc, char* argv[])
{
    parse_args(argc, argv);

    // open files
    FILE* fin = z_fopen(opt.input, "rb");
    FILE* fout = (opt.output == NULL || strcmp(opt.output, "-") == 0) ?
        stdout : z_fopen(opt.output, "w");

    // read in
    size_t sz, base, entry;
    uint8_t* image = ihex_load8(&sz, &base, &entry, fin);
    if (image == NULL)
        z_die("ihex");

    // write out
    switch (opt.fmt) {
    case 'b':
#if defined(_O_BINARY)
        _setmode(_fileno(fout), _O_BINARY);
#endif // _O_BINARY
        if (fwrite(image, 1, sz, fout) != sz)
            z_die("fwrite");
    break;
    case 'c':
    case 0:
        dump_c(image, sz, base, entry, fout);
    break;
    case 'x':
        dump_hex(image, sz, base, entry, fout);
    break;
    }

    free(image);
    fclose(fout);
    fclose(fin);
    free(opt.output);
    free(opt.input);
    exit(EXIT_SUCCESS);
}
