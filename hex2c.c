//
// hex2c
//
// Convert between Intel HEX, Binary and C Include format
//
// https://github.com/matveyt/hex2c
//

#include "stdz.h"
#include <fcntl.h>
#include <getopt.h>
#if defined(_WIN32)
#include <io.h>
#endif // _WIN32
#include "ihx.h"

static const char program_name[] = "hex2c";

// user options
static struct {
    char* input;
    char* output;
    int fmt_out;
    unsigned filler;
    unsigned padding;
    unsigned wrap;
} opt = {0};

/*noreturn*/
static void help(void)
{
    printf(
"Usage: %s [OPTION]... FILE\n"
"Convert between Intel HEX, Binary and C Include format.\n"
"\n"
"-b, --binary       Binary dump output\n"
"-c, --c            C Include output\n"
"-x, --hex          Intel HEX format output\n"
"-i, --info         Only show file info\n"
"-o, --output=FILE  Set output file name\n"
"-z, --filler=XX    Suppress consecutive bytes in output\n"
"-p, --padding=NUM  Extra space on line\n"
"-w, --wrap=NUM     Maximum output bytes per line\n"
"-h, --help         Show this message and exit\n"
"\n"
"If no output is given then writes to stdout.\n",
        program_name);
    exit(EXIT_SUCCESS);
}

static void parse_args(int argc, char* argv[])
{
    static struct option lopts[] = {
        { "binary", no_argument, NULL, 'b' },
        { "c", no_argument, NULL, 'c' },
        { "hex", no_argument, NULL, 'x' },
        { "info", no_argument, NULL, 'i' },
        { "output", required_argument, NULL, 'o' },
        { "filler", optional_argument, NULL, 'z' },
        { "padding", required_argument, NULL, 'p' },
        { "wrap", required_argument, NULL, 'w' },
        { "help", no_argument, NULL, 'h'},
        {0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "bcxio:z::p:w:h", lopts, NULL)) != -1) {
        switch (c) {
        case 'b':
        case 'c':
        case 'x':
        case 'i':
            opt.fmt_out = c;
        break;
        case 'o':
            free(opt.output);
            opt.output = z_strdup(optarg);
        break;
        case 'z':
            opt.filler = optarg ? strtoul(optarg, NULL, 16) : UINT8_MAX;
        break;
        case 'p':
            opt.padding = strtoul(optarg, NULL, 10);
            if (opt.padding > UINT8_MAX)
                opt.padding = 0;
        break;
        case 'w':
            opt.wrap = strtoul(optarg, NULL, 10);
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

// format output as C Include
static void c_dump(uint8_t* image, size_t sz, size_t base, size_t entry, FILE* f)
{
    // user options
    unsigned wrap = opt.wrap ? opt.wrap : 8;
    unsigned padding = opt.padding ? opt.padding : 4;

    // header
    fprintf(f, "// made with %s\n", program_name);
    if (base > 0)
        fprintf(f, "// image base 0x%04zx\n", base);
    if (entry > 0)
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
    opt.filler = UINT8_MAX + 1; // not used
    parse_args(argc, argv);

    // open files
    FILE* fin = z_fopen(opt.input, "rb");
    FILE* fout = (opt.output == NULL || strcmp(opt.output, "-") == 0) ?
        stdout : z_fopen(opt.output, "w");

    // read in
    uint8_t* image;
    size_t sz, base, entry;
    int fmt_in = ihx_load(&image, &sz, &base, &entry, fin);
    if (fmt_in < 0)
        z_die("ihx_load");

    // write out
    switch (opt.fmt_out) {
    case 'b':
#if defined(_WIN32)
        _setmode(_fileno(fout), _O_BINARY);
#endif
        if (opt.filler <= UINT8_MAX)
            for (size_t i = base; i > 0; --i)
                putc(opt.filler, fout);
        if (fwrite(image, 1, sz, fout) != sz)
            z_die("fwrite");
    break;
    case 'c':
    case 0:
        c_dump(image, sz, base, entry, fout);
    break;
    case 'x':
        ihx_dump(image, sz, base, entry, opt.filler, opt.wrap, fout);
    break;
    case 'i':
        printf("Format: %s\n", (fmt_in == 'x') ? "Intel HEX" : "Binary");
        printf("Size: %zu bytes\n", sz);
        if (fmt_in == 'x' && sz > 0) {
            printf("Address Range: %04zX-%04zX\n", base, base + sz - 1);
            printf("Entry Point: %04zX\n", entry);
        }
    break;
    }

    free(image);
    fclose(fout);
    fclose(fin);
    free(opt.output);
    free(opt.input);
    exit(EXIT_SUCCESS);
}
